#ifndef PID_H
#define PID_H

#include "pid_gains.h"

/* Discrete PID controller with anti-windup integrator clamp.
   All state is kept in PidController_t — zero heap. */
typedef struct {
    float kp, ki, kd, dt;
    float clamp_min, clamp_max;
    float integrator;
    float prev_error;
} PidController_t;

/* Initialise from pid_gains.h constants. */
void PID_init(PidController_t *pid);

/* Compute output: u = Kp*e + Ki*∫e·dt + Kd*(Δe/dt).
   Anti-windup: integrator clamped to [clamp_min/ki, clamp_max/ki]. */
float PID_update(PidController_t *pid, float setpoint, float measurement);

/* Reset integrator and previous error to zero. */
void PID_reset(PidController_t *pid);

#endif /* PID_H */
