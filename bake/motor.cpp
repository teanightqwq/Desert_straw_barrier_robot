#include "motor.h"

static bool is_valid_pin(int pin) {
  return pin >= 0;
}

static void setup_motor_pins(const MotorPins& pins) {
  if (!is_valid_pin(pins.in1) || !is_valid_pin(pins.in2)) {
    return;
  }
  pinMode(pins.in1, OUTPUT);
  pinMode(pins.in2, OUTPUT);
  if (is_valid_pin(pins.en)) {
    pinMode(pins.en, OUTPUT);
    analogWrite(pins.en, 0);
  }
  digitalWrite(pins.in1, LOW);
  digitalWrite(pins.in2, LOW);
}

FeederMotorController::FeederMotorController(const FeederMotorConfig& cfg)
    : cfg_(cfg),
      prevStatus_(STATUS_NOT_START),
      currentDir_(MotorDir::Cw),
      startMs_(0),
      startupDone_(false),
      sawStraw_(false) {}

void FeederMotorController::begin() {
  setup_motor_pins(cfg_.motor1);
  setup_motor_pins(cfg_.motor2);
  setup_motor_pins(cfg_.motor3);
}

void FeederMotorController::update(WorkStatus status, bool strawDetected) {
  uint32_t now = millis();

  if (status != STATUS_ON_WORK) {
    stop_all();
    prevStatus_ = status;
    startMs_ = 0;
    startupDone_ = false;
    sawStraw_ = false;
    currentDir_ = MotorDir::Cw;
    return;
  }

  if (prevStatus_ != STATUS_ON_WORK) {
    reset_startup_state(now);
  }

  if (strawDetected) {
    sawStraw_ = true;
  }

  if (!startupDone_ && (now - startMs_ >= cfg_.startupCheckMs)) {
    if (!sawStraw_) {
      currentDir_ = (currentDir_ == MotorDir::Cw) ? MotorDir::Ccw : MotorDir::Cw;
    }
    startupDone_ = true;
  }

  drive_on_work();
  prevStatus_ = status;
}

void FeederMotorController::stop_all() {
  set_motor(cfg_.motor1, MotorDir::Stop, 0);
  set_motor(cfg_.motor2, MotorDir::Stop, 0);
  set_motor(cfg_.motor3, MotorDir::Stop, 0);
}

void FeederMotorController::set_motor(const MotorPins& pins, MotorDir dir, int pwm) {
  if (!is_valid_pin(pins.in1) || !is_valid_pin(pins.in2)) {
    return;
  }

  switch (dir) {
    case MotorDir::Cw:
      digitalWrite(pins.in1, HIGH);
      digitalWrite(pins.in2, LOW);
      break;
    case MotorDir::Ccw:
      digitalWrite(pins.in1, LOW);
      digitalWrite(pins.in2, HIGH);
      break;
    case MotorDir::Stop:
    default:
      digitalWrite(pins.in1, LOW);
      digitalWrite(pins.in2, LOW);
      break;
  }

  if (is_valid_pin(pins.en)) {
    analogWrite(pins.en, pwm);
  }
}

void FeederMotorController::drive_on_work() {
  int motor1_pwm = constrain(cfg_.motor1Pwm, 0, cfg_.pwmMax);
  int motor2_pwm = motor2_pwm(motor1_pwm);
  int motor3_pwm = motor3_pwm(motor1_pwm);

  set_motor(cfg_.motor1, currentDir_, motor1_pwm);
  set_motor(cfg_.motor2, currentDir_, motor2_pwm);
  set_motor(cfg_.motor3, MotorDir::Cw, motor3_pwm);
}

void FeederMotorController::reset_startup_state(uint32_t now) {
  startMs_ = now;
  startupDone_ = false;
  sawStraw_ = false;
  currentDir_ = MotorDir::Cw;
}

int FeederMotorController::motor2_pwm(int motor1Pwm) const {
  int pwm = static_cast<int>(motor1Pwm * cfg_.motor2Ratio);
  return constrain(pwm, 0, cfg_.pwmMax);
}

int FeederMotorController::motor3_pwm(int motor1Pwm) const {
  int pwm = static_cast<int>(motor1Pwm * cfg_.motor3Ratio);
  return constrain(pwm, 0, cfg_.pwmMax);
}

// ---- Example wiring (TBD) ----
static const FeederMotorConfig kFeederMotorConfig = {
    // motor1 (AT8236 or TB6612) pins
    {-1, -1, -1},
    // motor2 (AT8236 or TB6612) pins
    {-1, -1, -1},
    // motor3 (AT8236 or TB6612) pins
    {-1, -1, -1},
    255,
    200,
    0.9f,
    0.7f,
    3000,
};

static FeederMotorController feederMotors(kFeederMotorConfig);

void motor_setup() {
  feederMotors.begin();
}

void motor_loop(WorkStatus status, bool strawDetected) {
  feederMotors.update(status, strawDetected);
}

void motor_stop() {
  feederMotors.stop_all();
}
