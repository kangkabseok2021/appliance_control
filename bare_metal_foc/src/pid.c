#include "pid.h"
#include "no_alloc.h"

void PID_init(PidController_t *pid)
{
    pid->kp         = PID_KP;
    pid->ki         = PID_KI;
    pid->kd         = PID_KD;
    pid->dt         = PID_DT;
    pid->clamp_min  = PID_CLAMP_MIN;
    pid->clamp_max  = PID_CLAMP_MAX;
    pid->integrator = 0.0f;
    pid->prev_error = 0.0f;
}

float PID_update(PidController_t *pid, float setpoint, float measurement)
{
    float error = setpoint - measurement;

    /* Proportional */
    float p_term = pid->kp * error;

    /* Integral with anti-windup clamp */
    pid->integrator += error * pid->dt;
    float int_limit = (pid->ki > 1e-9f)
        ? pid->clamp_max / pid->ki
        : 1e6f;
    if (pid->integrator >  int_limit) pid->integrator =  int_limit;
    if (pid->integrator < -int_limit) pid->integrator = -int_limit;
    float i_term = pid->ki * pid->integrator;

    /* Derivative (on measurement, not error, to avoid derivative kick) */
    float d_term = pid->kd * (error - pid->prev_error) / pid->dt;
    pid->prev_error = error;

    /* Output clamp */
    float output = p_term + i_term + d_term;
    if (output >  pid->clamp_max) output =  pid->clamp_max;
    if (output <  pid->clamp_min) output =  pid->clamp_min;
    return output;
}

void PID_reset(PidController_t *pid)
{
    pid->integrator = 0.0f;
    pid->prev_error = 0.0f;
}
