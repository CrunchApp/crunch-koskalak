/**
 * Motion Engine
 *
 * Controls servos via PCA9685 16-channel PWM driver (I2C).
 * Ported from miniKame's oscillator-based gait engine.
 *
 * Supports two DOF configurations (compile-time via KOSKALAK_DOF):
 *   8-DOF (default): miniKame layout, 2 servos per leg (hip + knee)
 *   4-DOF (KT2-style): 1 actuator per leg, dynamic momentum gaits
 *
 * Balance correction: reads offset buffer from the standalone balance
 * controller task (balance_controller.h). The motion engine does NOT
 * compute balance — it only applies the offsets provided. This matches
 * the TurtleOS pattern where balance is an OS-level service invisible
 * to application gait/animation code.
 *
 * Optional servo feedback: when KOSKALAK_SERVO_FEEDBACK is defined,
 * reads actual servo position via ADC for closed-loop joint control.
 * Requires five-wire feedback servos (standard MG90S has no feedback).
 *
 * Expressive animations map device states to motion patterns:
 *   IDLE     → gentle sway (subtle weight shift)
 *   GREETING → excited hop + lean forward
 *   LISTENING → attentive head tilt
 *   THINKING → side-to-side wiggle
 *   SPEAKING → rhythmic bounce (synced to TTS cadence)
 *   SLEEPING → slow breathing motion, lower stance
 */

#pragma once

#include "esp_err.h"
#include "balance_controller.h"
#include <stdbool.h>

#ifndef KOSKALAK_DOF
#define KOSKALAK_DOF 8
#endif

typedef enum {
    GAIT_IDLE_SWAY,
    GAIT_GREETING_HOP,
    GAIT_LISTENING_TILT,
    GAIT_THINKING_WIGGLE,
    GAIT_SPEAKING_BOUNCE,
    GAIT_SLEEP_BREATHE,
    GAIT_WALK_FORWARD,
    GAIT_WALK_BACKWARD,
    GAIT_TURN_LEFT,
    GAIT_TURN_RIGHT,
    GAIT_CELEBRATE,     /* Order confirmed — happy dance */
    GAIT_NONE,          /* All servos hold position */
} gait_id_t;

/**
 * Initialise I2C bus and PCA9685 driver. Moves all servos to neutral position.
 * The I2C bus is shared with the MPU-6050 IMU.
 */
esp_err_t motion_engine_init(void);

/**
 * Start playing a gait pattern. Loops until a different gait is set or GAIT_NONE.
 */
esp_err_t motion_set_gait(gait_id_t gait);

/**
 * Get the currently active gait.
 */
gait_id_t motion_get_gait(void);

/**
 * Set the animation speed multiplier. 1.0 = normal, 0.5 = half speed, 2.0 = double.
 */
esp_err_t motion_set_speed(float multiplier);

/**
 * Move all servos to their neutral (standing) position and stop.
 */
esp_err_t motion_stand_neutral(void);

/**
 * Move all servos to a compact resting position (low power).
 */
esp_err_t motion_crouch(void);

/**
 * Get the number of servos in the current configuration.
 */
int motion_get_servo_count(void);

#ifdef KOSKALAK_SERVO_FEEDBACK
/**
 * Read the actual position of a servo via ADC feedback wire.
 * Only available with five-wire feedback servos.
 *
 * @param servo_index  Servo channel (0 to KOSKALAK_DOF-1)
 * @param angle_out    Output: actual angle in degrees
 */
esp_err_t motion_read_servo_position(int servo_index, float *angle_out);
#endif
