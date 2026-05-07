#ifndef WHEELS_H
#define WHEELS_H

#include <Arduino.h>

// TODO: Replace with actual GPIO and H-bridge wiring.
// Motor driver pins are placeholders for now.
struct MotorPins {
  int in1;
  int in2;
  int en; // PWM enable (set to -1 if not used)
};

enum class WheelDir {
  Forward,
  Backward,
  Stop
};

// Two motors per group (front vs rear or left vs right) driven by H-bridge.
struct MotorGroup {
  MotorPins left;
  MotorPins right;
};

struct WheelConfig {
  MotorGroup active;  // speed priority group
  MotorGroup passive; // only needs to spin
  int pwmMax;          // e.g., 255
};

class WheelController {
 public:
  explicit WheelController(const WheelConfig& cfg);

  void begin();
  void forward(int speed);
  void backward(int speed);
  void stop();

 private:
  void set_dir_and_speed(WheelDir dir, int speed);
  int passive_spin_pwm() const;

  WheelConfig cfg_;
};

// Optional demo hooks
void wheels_setup();
void wheels_forward(int speed);
void wheels_backward(int speed);
void wheels_stop();

#endif // WHEELS_H
