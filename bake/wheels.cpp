
#include "wheels.h"

static void setup_motor(const MotorPins& pins) {
	pinMode(pins.in1, OUTPUT);
	pinMode(pins.in2, OUTPUT);
	if (pins.en >= 0) {
		pinMode(pins.en, OUTPUT);
		analogWrite(pins.en, 0);
	}
	digitalWrite(pins.in1, LOW);
	digitalWrite(pins.in2, LOW);
}

static void drive_motor(const MotorPins& pins, WheelDir dir, int pwm) {
	switch (dir) {
		case WheelDir::Forward:
			digitalWrite(pins.in1, HIGH);
			digitalWrite(pins.in2, LOW);
			break;
		case WheelDir::Backward:
			digitalWrite(pins.in1, LOW);
			digitalWrite(pins.in2, HIGH);
			break;
		case WheelDir::Stop:
		default:
			digitalWrite(pins.in1, LOW);
			digitalWrite(pins.in2, LOW);
			break;
	}

	if (pins.en >= 0) {
		analogWrite(pins.en, pwm);
	}
}

static void drive_group(const MotorGroup& group, WheelDir dir, int pwm) {
	drive_motor(group.left, dir, pwm);
	drive_motor(group.right, dir, pwm);
}

WheelController::WheelController(const WheelConfig& cfg) : cfg_(cfg) {}

void WheelController::begin() {
	setup_motor(cfg_.active.left);
	setup_motor(cfg_.active.right);
	setup_motor(cfg_.passive.left);
	setup_motor(cfg_.passive.right);
}

void WheelController::forward(int speed) {
	set_dir_and_speed(WheelDir::Forward, speed);
}

void WheelController::backward(int speed) {
	set_dir_and_speed(WheelDir::Backward, speed);
}

void WheelController::stop() {
	set_dir_and_speed(WheelDir::Stop, 0);
}

void WheelController::set_dir_and_speed(WheelDir dir, int speed) {
	int active_pwm = constrain(speed, 0, cfg_.pwmMax);
	int passive_pwm = (dir == WheelDir::Stop) ? 0 : passive_spin_pwm();

	drive_group(cfg_.active, dir, active_pwm);
	drive_group(cfg_.passive, dir, passive_pwm);
}

int WheelController::passive_spin_pwm() const {
	// Minimal PWM to ensure the passive group spins; tune later.
	return cfg_.pwmMax / 3;
}

// ---- Example wiring (TBD) ----
// Adjust pins to actual H-bridge driver mapping.
static const WheelConfig kWheelConfig = {
		// active group (speed priority)
		{{-1, -1, -1}, {-1, -1, -1}},
		// passive group
		{{-1, -1, -1}, {-1, -1, -1}},
		255,
};

static WheelController wheels(kWheelConfig);

// Optional demo hooks
void wheels_setup() {
	wheels.begin();
}

void wheels_forward(int speed) {
	wheels.forward(speed);
}

void wheels_backward(int speed) {
	wheels.backward(speed);
}

void wheels_stop() {
	wheels.stop();
}
