#include "MqttClient.h"
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace std::chrono_literals;

// ── Encode helpers ─────────────────────────────────────────────────────────

static void appendU16(std::vector<uint8_t>& v, uint16_t n) {
    v.push_back(static_cast<uint8_t>(n >> 8));
    v.push_back(static_cast<uint8_t>(n & 0xFF));
}

static void appendStr(std::vector<uint8_t>& v, const std::string& s) {
    appendU16(v, static_cast<uint16_t>(s.size()));
    v.insert(v.end(), s.begin(), s.end());
}

static void appendRemainingLen(std::vector<uint8_t>& v, uint32_t len) {
    do {
        uint8_t b = len & 0x7F;
        len >>= 7;
        if (len > 0) b |= 0x80;
        v.push_back(b);
    } while (len > 0);
}

// ── Constructor / Destructor ───────────────────────────────────────────────

MqttClient::MqttClient(const std::string& host, uint16_t port, const std::string& cid)
    : host_(host), port_(port), clientId_(cid) {}

MqttClient::~MqttClient() { disconnect(); }

// ── Connect ───────────────────────────────────────────────────────────────

bool MqttClient::connect() {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &res) != 0)
        return false;

    sock_ = ::socket(res->ai_family, res->ai_socktype, 0);
    if (sock_ < 0) { ::freeaddrinfo(res); return false; }

    if (::connect(sock_, res->ai_addr, res->ai_addrlen) != 0) {
        ::close(sock_); sock_ = -1; ::freeaddrinfo(res); return false;
    }
    ::freeaddrinfo(res);

    if (!sendConnect()) { ::close(sock_); sock_ = -1; return false; }

    // Wait for CONNACK
    uint8_t type_byte = 0;
    if (!readByte(type_byte) || (type_byte >> 4) != 2) {
        ::close(sock_); sock_ = -1; return false;
    }
    readRemainingLength();
    uint8_t b1 = 0, b2 = 0;
    readByte(b1); readByte(b2);
    if (b2 != 0) { ::close(sock_); sock_ = -1; return false; }  // non-zero = refused

    // Re-subscribe registered filters
    {
        std::lock_guard<std::mutex> lk(subMu_);
        for (auto& [filter, _] : subs_) sendSubscribe(filter);
    }

    connected_ = true;
    running_   = true;
    reader_ = std::thread([this]{ readerLoop(); });
    return true;
}

void MqttClient::disconnect() {
    running_   = false;
    connected_ = false;
    if (sock_ >= 0) { ::shutdown(sock_, SHUT_RDWR); ::close(sock_); sock_ = -1; }
    if (reader_.joinable()) reader_.join();
}

// ── CONNECT packet ────────────────────────────────────────────────────────

bool MqttClient::sendConnect() {
    std::vector<uint8_t> payload;
    appendStr(payload, "MQTT");                // protocol name
    payload.push_back(4);                      // protocol level 3.1.1
    payload.push_back(0x02);                   // connect flags: clean session
    appendU16(payload, 60);                    // keepalive 60s
    appendStr(payload, clientId_);

    std::vector<uint8_t> pkt;
    pkt.push_back(0x10);                       // CONNECT
    appendRemainingLen(pkt, static_cast<uint32_t>(payload.size()));
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return writeAll(pkt);
}

// ── PUBLISH ───────────────────────────────────────────────────────────────

bool MqttClient::publish(const std::string& topic, const std::string& payload, uint8_t qos) {
    std::lock_guard<std::mutex> lk(writeMu_);
    std::vector<uint8_t> body;
    appendStr(body, topic);
    if (qos > 0) appendU16(body, pktId_++);
    body.insert(body.end(), payload.begin(), payload.end());

    std::vector<uint8_t> pkt;
    uint8_t first = static_cast<uint8_t>(0x30 | (qos << 1));
    pkt.push_back(first);
    appendRemainingLen(pkt, static_cast<uint32_t>(body.size()));
    pkt.insert(pkt.end(), body.begin(), body.end());
    return connected_ && writeAll(pkt);
}

// ── SUBSCRIBE ─────────────────────────────────────────────────────────────

void MqttClient::subscribe(const std::string& filter, MessageCallback cb) {
    std::lock_guard<std::mutex> lk(subMu_);
    subs_[filter] = std::move(cb);
    if (connected_) sendSubscribe(filter);
}

bool MqttClient::sendSubscribe(const std::string& filter, uint8_t qos) {
    std::vector<uint8_t> body;
    appendU16(body, pktId_++);
    appendStr(body, filter);
    body.push_back(qos);

    std::vector<uint8_t> pkt;
    pkt.push_back(0x82);   // SUBSCRIBE
    appendRemainingLen(pkt, static_cast<uint32_t>(body.size()));
    pkt.insert(pkt.end(), body.begin(), body.end());
    return writeAll(pkt);
}

// ── Reader loop ───────────────────────────────────────────────────────────

void MqttClient::readerLoop() {
    while (running_) {
        uint8_t first = 0;
        if (!readByte(first)) break;
        uint8_t  type = first >> 4;
        uint32_t rem  = readRemainingLength();
        std::vector<uint8_t> data;
        if (rem > 0 && !readExact(data, rem)) break;
        processPacket(type, data);
    }
    connected_ = false;
}

void MqttClient::processPacket(uint8_t type, const std::vector<uint8_t>& data) {
    if (type == 3) {  // PUBLISH
        size_t i = 0;
        uint16_t tlen = (static_cast<uint16_t>(data[i]) << 8) | data[i+1]; i += 2;
        std::string topic(data.begin() + i, data.begin() + i + tlen); i += tlen;
        std::string payload(data.begin() + i, data.end());

        std::lock_guard<std::mutex> lk(subMu_);
        for (auto& [filter, cb] : subs_) {
            // Simple wildcard: # matches anything, + matches one level
            bool match = (filter == topic) ||
                         (filter.back() == '#' &&
                          topic.substr(0, filter.size() - 1) == filter.substr(0, filter.size() - 1));
            if (match) cb(topic, payload);
        }
    }
    // PINGRESP (type 13): no action needed
}

// ── Low-level I/O ─────────────────────────────────────────────────────────

bool MqttClient::writeAll(const std::vector<uint8_t>& buf) {
    size_t sent = 0;
    while (sent < buf.size()) {
        ssize_t n = ::write(sock_, buf.data() + sent, buf.size() - sent);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool MqttClient::readByte(uint8_t& b) {
    return ::read(sock_, &b, 1) == 1;
}

bool MqttClient::readExact(std::vector<uint8_t>& buf, size_t n) {
    buf.resize(n);
    size_t got = 0;
    while (got < n) {
        ssize_t r = ::read(sock_, buf.data() + got, n - got);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

uint32_t MqttClient::readRemainingLength() {
    uint32_t val = 0, mul = 1;
    uint8_t b = 0;
    do {
        if (!readByte(b)) return 0;
        val += (b & 0x7F) * mul;
        mul <<= 7;
    } while (b & 0x80);
    return val;
}
