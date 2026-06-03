#include "can_frame.h"
#include "no_alloc.h"

/* Q8.8 int16 range: ±32767/256 ≈ ±127.99 Nm — spec clamps at ±127 */
#define TORQUE_MAX_NM  127.0f
#define TORQUE_MIN_NM -127.0f
#define Q8_SCALE       256.0f   /* Q8.8: 2^8 */

int CanFrame_pack(float torque_Nm, uint8_t soc_pct, uint16_t rpm,
                  CanFrame_t *out)
{
    if (!out) return -1;

    int ret = 0;
    /* Clamp torque — return -2 to signal overflow but still pack clamped value */
    if (torque_Nm > TORQUE_MAX_NM) { torque_Nm = TORQUE_MAX_NM; ret = -2; }
    if (torque_Nm < TORQUE_MIN_NM) { torque_Nm = TORQUE_MIN_NM; ret = -2; }

    /* Q8.8 encode: int16 big-endian in data[0:1] */
    int16_t raw_torque = (int16_t)(torque_Nm * Q8_SCALE);
    out->id     = CAN_ID_TPDO1;
    out->dlc    = 8;
    out->data[0] = (uint8_t)((raw_torque >> 8) & 0xFF);
    out->data[1] = (uint8_t)( raw_torque       & 0xFF);

    /* SoC in data[2] */
    out->data[2] = soc_pct;

    /* RPM big-endian in data[3:4] */
    out->data[3] = (uint8_t)((rpm >> 8) & 0xFF);
    out->data[4] = (uint8_t)( rpm       & 0xFF);

    /* Reserved */
    out->data[5] = out->data[6] = out->data[7] = 0x00;
    return ret;
}

int CanFrame_unpack(const CanFrame_t *in, float *torque_out,
                    uint8_t *soc_out, uint16_t *rpm_out)
{
    if (!in || !torque_out || !soc_out || !rpm_out) return -1;

    /* Sign-extend via explicit int16 cast — endianness-safe byte assembly */
    int16_t raw = (int16_t)((uint16_t)((uint16_t)in->data[0] << 8) |
                              (uint16_t)in->data[1]);
    *torque_out = (float)raw / Q8_SCALE;

    *soc_out = in->data[2];

    *rpm_out = (uint16_t)(((uint16_t)in->data[3] << 8) |
                            (uint16_t)in->data[4]);
    return 0;
}
