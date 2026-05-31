#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

// Motor GPIO mapping (ESP32-S3)
constexpr int MOTOR_AIN1 = 4;
constexpr int MOTOR_AIN2 = 5;
constexpr int MOTOR_APWM = 6;

constexpr int MOTOR_BIN1 = 9;
constexpr int MOTOR_BIN2 = 10;
constexpr int MOTOR_BPWM = 11;

constexpr int MOTOR_CIN1 = 15;
constexpr int MOTOR_CIN2 = 16;
constexpr int MOTOR_CPWM = 17;

// PWM config
constexpr int MOTOR_PWM_FREQ_HZ = 20000;
constexpr int MOTOR_PWM_RES_BITS = 12;
constexpr int MOTOR_PWM_MAX = 4095;

constexpr int MOTOR_PWM_CH_A = 0;
constexpr int MOTOR_PWM_CH_B = 1;
constexpr int MOTOR_PWM_CH_C = 2;

// Speed config (0..MOTOR_PWM_MAX)
constexpr int DEFAULT_A_SPEED = MOTOR_PWM_MAX;
constexpr float B_SPEED_SCALE = 1.0f;
constexpr float C_SPEED_SCALE = 1.0f;

void motor_setup();
void motor_loop();
void motor_set_a_dir(int a_dir);


#endif // MOTOR_H
