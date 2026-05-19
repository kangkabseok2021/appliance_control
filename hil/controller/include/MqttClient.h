#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Lightweight MQTT 3.1.1 client over plain TCP.
// Implements CONNECT, PUBLISH (QoS 0/1), SUBSCRIBE, PINGREQ.
// No TLS — suitable for internal Mosquitto broker over localhost/Docker.
//
// Design: one background thread reads incoming packets and dispatches
// to registered callbacks. publish() is thread-safe.

class MqttClient {
public:
    using MessageCallback = std::function<void(const std::string& topic,
                                               const std::string& payload)>;

    MqttClient(const std::string& host, uint16_t port, const std::string& client_id);
    ~MqttClient();

    // Connect, subscribe, start reader thread. Retries with backoff on failure.
    bool connect();
    void disconnect();

    // Thread-safe. QoS 0 = fire-and-forget; QoS 1 = wait for PUBACK.
    bool publish(const std::string& topic, const std::string& payload, uint8_t qos = 0);

    void subscribe(const std::string& topic_filter, MessageCallback cb);

    bool isConnected() const { return connected_.load(); }

private:
    void readerLoop();
    bool sendConnect();
    bool sendSubscribe(const std::string& filter, uint8_t qos = 1);
    bool sendPing();
    void processPacket(uint8_t type, const std::vector<uint8_t>& data);
    bool writeAll(const std::vector<uint8_t>& buf);
    bool readByte(uint8_t& b);
    bool readExact(std::vector<uint8_t>& buf, size_t n);
    uint32_t readRemainingLength();

    std::string  host_;
    uint16_t     port_;
    std::string  clientId_;
    int          sock_{-1};

    std::atomic<bool>   connected_{false};
    std::atomic<bool>   running_{false};
    std::thread         reader_;
    std::mutex          writeMu_;

    std::mutex          subMu_;
    std::unordered_map<std::string, MessageCallback> subs_;

    uint16_t            pktId_{1};
};
