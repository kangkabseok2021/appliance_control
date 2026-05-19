#include <gtest/gtest.h>
#include "MqttClient.h"

// These tests verify MqttClient behaviour without a real broker.
// Integration tests with a live Mosquitto broker are in hil/tests/.

TEST(MqttClientTest, DefaultNotConnected) {
    MqttClient client("localhost", 1883, "test-id");
    EXPECT_FALSE(client.isConnected());
}

TEST(MqttClientTest, ConnectToNonexistentBrokerReturnsFalse) {
    // Port 19999 should be unreachable
    MqttClient client("localhost", 19999, "test-id");
    EXPECT_FALSE(client.connect());
    EXPECT_FALSE(client.isConnected());
}

TEST(MqttClientTest, PublishWhenDisconnectedReturnsFalse) {
    MqttClient client("localhost", 19999, "test-id");
    EXPECT_FALSE(client.publish("test/topic", "hello"));
}

TEST(MqttClientTest, DisconnectIdempotent) {
    MqttClient client("localhost", 19999, "test-id");
    EXPECT_NO_THROW(client.disconnect());
    EXPECT_NO_THROW(client.disconnect());
}
