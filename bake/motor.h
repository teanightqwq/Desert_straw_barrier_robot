#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include "warning.h"

struct MotorPins {
  int in1;
  int in2;
  int en; // PWM enable (set to -1 if not used)
};

enum class MotorDir {
  Cw,
  Ccw,
  Stop
};

struct FeederMotorConfig {
  MotorPins motor1;
  MotorPins motor2;
  MotorPins motor3;
  int pwmMax;
  int motor1Pwm;
  float motor2Ratio;
  float motor3Ratio;
  uint32_t startupCheckMs;
};

class FeederMotorController {
 public:
  explicit FeederMotorController(const FeederMotorConfig& cfg);

  void begin();
  void update(WorkStatus status, bool strawDetected);
  void stop_all();

 private:
  void set_motor(const MotorPins& pins, MotorDir dir, int pwm);
  void drive_on_work();
  void reset_startup_state(uint32_t now);
  int motor2_pwm(int motor1Pwm) const;
  int motor3_pwm(int motor1Pwm) const;

  FeederMotorConfig cfg_;
  WorkStatus prevStatus_;
  MotorDir currentDir_;
  uint32_t startMs_;
  bool startupDone_;
  bool sawStraw_;
};

// Optional hooks for integration
void motor_setup();
void motor_loop(WorkStatus status, bool strawDetected);
void motor_stop();

#endif // MOTOR_H
