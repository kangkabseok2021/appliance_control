#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdint.h>
#include <assert.h>

/* CAN frame: CANopen TPDO1 class (ID 0x181).
   data[0:1]  int16_t Q8.8  torque_Nm  big-endian
   data[2]    uint8_t       SoC %
   data[3:4]  uint16_t      RPM        big-endian
   data[5:7]  reserved (0x00)
   static_assert guards padding to keep sizeof == 13. */
typedef struct __attribute__((packed)) {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} CanFrame_t;

static_assert(sizeof(CanFrame_t) == 13, "CanFrame_t padding changed");

#define CAN_ID_TPDO1  0x181U

/* Pack motor telemetry into a CAN frame.
   torque_Nm clamped to [−200, +200] before Q8.8 encoding.
   Returns 0 on success, −1 on null pointer, −2 on overflow (clamped). */
int CanFrame_pack(float torque_Nm, uint8_t soc_pct, uint16_t rpm,
                  CanFrame_t *out);

/* Unpack a CAN frame into motor telemetry.
   Returns 0 on success, −1 on null pointer. */
int CanFrame_unpack(const CanFrame_t *in, float *torque_out,
                    uint8_t *soc_out, uint16_t *rpm_out);

#endif /* CAN_FRAME_H */
