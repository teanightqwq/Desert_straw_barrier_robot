#include "motor.h"
#include "logger.h"

static int g_a_dir = -1;

static int clamp_speed(int speed) {
  if (speed < 0) return 0;
  if (speed > MOTOR_PWM_MAX) return MOTOR_PWM_MAX;
  return speed;
}

static void write_motor(int in1, int in2, int pwm_pin, int dir, int speed) {
  int pwm = clamp_speed(speed);

  if (dir == 0 || pwm == 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(pwm_pin, 0);
    return;
  }

  if (dir > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }

  analogWrite(pwm_pin, pwm);
}

static void apply_targets(int c_speed, int a_dir) {
  int c_pwm = clamp_speed(c_speed);
  int a_direction = (a_dir > 0) ? 1 : (a_dir < 0 ? -1 : 0);
  int c_direction = (c_pwm == 0) ? 0 : MOTOR_C_DIR;

  int a_pwm = clamp_speed(static_cast<int>(c_pwm * A_SPEED_SCALE + 0.5f));
  int b_pwm = clamp_speed(static_cast<int>(c_pwm * B_SPEED_SCALE + 0.5f));
  int b_direction = (a_direction == 0) ? 0 : a_direction;

  write_motor(MOTOR_AIN1, MOTOR_AIN2, MOTOR_APWM, a_direction, a_pwm);
  write_motor(MOTOR_BIN1, MOTOR_BIN2, MOTOR_BPWM, b_direction, b_pwm);
  write_motor(MOTOR_CIN1, MOTOR_CIN2, MOTOR_CPWM, c_direction, c_pwm);
}

void motor_setup() {
  pinMode(MOTOR_AIN1, OUTPUT);
  pinMode(MOTOR_AIN2, OUTPUT);
  pinMode(MOTOR_BIN1, OUTPUT);
  pinMode(MOTOR_BIN2, OUTPUT);
  pinMode(MOTOR_CIN1, OUTPUT);
  pinMode(MOTOR_CIN2, OUTPUT);
  pinMode(MOTOR_APWM, OUTPUT);
  pinMode(MOTOR_BPWM, OUTPUT);
  pinMode(MOTOR_CPWM, OUTPUT);

  analogWriteResolution(MOTOR_APWM, MOTOR_PWM_RES_BITS);
  analogWriteResolution(MOTOR_BPWM, MOTOR_PWM_RES_BITS);
  analogWriteResolution(MOTOR_CPWM, MOTOR_PWM_RES_BITS);

  apply_targets(DEFAULT_C_SPEED, g_a_dir);

  char msg[180] = {0};
  snprintf(msg,
           sizeof(msg),
           "A(%d,%d,%d) B(%d,%d,%d) C(%d,%d,%d) defC=%d aScale=%.2f bScale=%.2f",
           MOTOR_AIN1,
           MOTOR_AIN2,
           MOTOR_APWM,
           MOTOR_BIN1,
           MOTOR_BIN2,
           MOTOR_BPWM,
           MOTOR_CIN1,
           MOTOR_CIN2,
           MOTOR_CPWM,
           DEFAULT_C_SPEED,
           static_cast<double>(A_SPEED_SCALE),
           static_cast<double>(B_SPEED_SCALE));
  logger_event("motor_boot", msg);
}

void motor_loop() {
  apply_targets(DEFAULT_C_SPEED, g_a_dir);
}

void motor_set_a_dir(int a_dir) {
  int next = (a_dir > 0) ? 1 : (a_dir < 0 ? -1 : 0);
  if (g_a_dir == next) {
    return;
  }
  g_a_dir = next;
  char msg[64] = {0};
  snprintf(msg, sizeof(msg), "dirA=%d", g_a_dir);
  logger_event("motor_dir", msg);
}
