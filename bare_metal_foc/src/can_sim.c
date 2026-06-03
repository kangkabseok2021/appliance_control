#define CAN_SIMULATION
#include "can_sim.h"
#include "no_alloc.h"
#include <string.h>

#ifdef CAN_SIMULATION

void CanBus_init(CanBus_t *bus)
{
    memset(bus, 0, sizeof(CanBus_t));
}

int CanBus_send(CanBus_t *bus, const CanFrame_t *frame)
{
    uint8_t next = (bus->head + 1U) & (CAN_BUS_DEPTH - 1U);
    if (next == bus->tail) return -1;  /* full */
    bus->frames[bus->head] = *frame;
    bus->head = next;
    return 0;
}

int CanBus_recv(CanBus_t *bus, CanFrame_t *out)
{
    if (bus->head == bus->tail) return -1;  /* empty */
    *out = bus->frames[bus->tail];
    bus->tail = (bus->tail + 1U) & (CAN_BUS_DEPTH - 1U);
    return 0;
}

#endif /* CAN_SIMULATION */
