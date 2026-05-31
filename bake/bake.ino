#include <Arduino.h>
#include "motor.h"
#include "sensor.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  // sensors_setup();
  // wedger_setup();
  // wheels_setup();
  motor_setup();
}

void loop() {
  // sensors_loop();
  // wedger_loop();
  // wheels_loop();
  motor_loop();
}