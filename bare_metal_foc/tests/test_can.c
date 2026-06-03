/* test_can.c — CAN frame packing, unpacking, endianness, and bus tests */
#define CAN_SIMULATION
/* setUp/tearDown defined in test_runner.c */
#include "unity.h"
#include "can_frame.h"
#include "can_sim.h"
#include <math.h>


/* 1. REQ-003/004: round-trip 100 Nm within 1 Q8.8 LSB (1/256 ≈ 0.004 Nm)
   Note: Q8.8 int16 saturates at ±127.99 Nm; 100 Nm is safely in range. */
void test_can_pack_unpack_round_trip_200Nm(void)
{
    CanFrame_t f;
    float torque_out; uint8_t soc; uint16_t rpm;
    CanFrame_pack(100.0f, 80, 3000, &f);
    CanFrame_unpack(&f, &torque_out, &soc, &rpm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f/256.0f, 100.0f, torque_out);
    TEST_ASSERT_EQUAL_UINT8(80, soc);
    TEST_ASSERT_EQUAL_UINT16(3000, rpm);
}

/* 2. Big-endian encoding: torque=0.5 Nm → Q8.8 = 128 = 0x0080 → data[0]=0, data[1]=128 */
void test_can_endianness_big_endian(void)
{
    CanFrame_t f;
    CanFrame_pack(0.5f, 0, 0, &f);
    TEST_ASSERT_EQUAL_UINT8(0x00, f.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80, f.data[1]);
}

/* 3. Overflow clamp: torque=500 Nm clamped to +127, returns -2 */
void test_can_overflow_clamp(void)
{
    CanFrame_t f;
    float torque_out; uint8_t soc; uint16_t rpm;
    int ret = CanFrame_pack(500.0f, 0, 0, &f);
    TEST_ASSERT_EQUAL_INT(-2, ret);
    CanFrame_unpack(&f, &torque_out, &soc, &rpm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f/256.0f, 127.0f, torque_out);
}

/* 4. Negative torque: -50 Nm round-trip */
void test_can_negative_torque(void)
{
    CanFrame_t f;
    float torque_out; uint8_t soc; uint16_t rpm;
    CanFrame_pack(-50.0f, 50, 1500, &f);
    CanFrame_unpack(&f, &torque_out, &soc, &rpm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f/256.0f, -50.0f, torque_out);
}

/* 5. Null pointer returns -1 */
void test_can_null_pointer(void)
{
    TEST_ASSERT_EQUAL_INT(-1, CanFrame_pack(0.0f, 0, 0, NULL));
    CanFrame_t f = {0};
    TEST_ASSERT_EQUAL_INT(-1, CanFrame_unpack(&f, NULL, NULL, NULL));
}

/* 6. In-memory bus: send 64 frames, receive all — no frame loss */
void test_can_bus_no_frame_loss(void)
{
    CanBus_t bus;
    CanBus_init(&bus);
    CanFrame_t tx = {0}, rx = {0};
    tx.id = CAN_ID_TPDO1; tx.dlc = 8;

    for (int i = 0; i < 63; i++) {
        tx.data[0] = (uint8_t)i;
        TEST_ASSERT_EQUAL_INT(0, CanBus_send(&bus, &tx));
    }
    for (int i = 0; i < 63; i++) {
        TEST_ASSERT_EQUAL_INT(0, CanBus_recv(&bus, &rx));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, rx.data[0]);
    }
    /* Buffer should now be empty */
    TEST_ASSERT_EQUAL_INT(-1, CanBus_recv(&bus, &rx));
}

int run_can_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_can_pack_unpack_round_trip_200Nm);
    RUN_TEST(test_can_endianness_big_endian);
    RUN_TEST(test_can_overflow_clamp);
    RUN_TEST(test_can_negative_torque);
    RUN_TEST(test_can_null_pointer);
    RUN_TEST(test_can_bus_no_frame_loss);
    return UNITY_END();
}
