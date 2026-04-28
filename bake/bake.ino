#include <Arduino.h>
#include "sensor.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  sensors_setup();
  // wedger_setup();
}

void loop() {
  sensors_loop();
  // wedger_loop();
}