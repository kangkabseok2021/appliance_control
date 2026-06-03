#ifndef CAN_SIM_H
#define CAN_SIM_H

#include "can_frame.h"

/* In-memory CAN bus simulation.
   64-slot SPSC ring buffer; lock-free via index masking.
   Compiled only when CAN_SIMULATION is defined; on real hardware a
   HAL CAN driver is substituted behind the same function signatures. */
#ifdef CAN_SIMULATION

#define CAN_BUS_DEPTH 64U

typedef struct {
    CanFrame_t frames[CAN_BUS_DEPTH];
    uint8_t    head;
    uint8_t    tail;
} CanBus_t;

void CanBus_init(CanBus_t *bus);
/* Returns 0 on success, −1 if full. */
int  CanBus_send(CanBus_t *bus, const CanFrame_t *frame);
/* Returns 0 on success, −1 if empty. */
int  CanBus_recv(CanBus_t *bus, CanFrame_t *out);

#endif /* CAN_SIMULATION */
#endif /* CAN_SIM_H */
