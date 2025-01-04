#include <Arduino.h>
#include "web.hpp"
#include "measure.hpp"

extern measurement_params_t measurement_params;

void setup() {
  Serial.begin(115200);

  web_init();
  measure_init();

}

void loop() {
  Serial.println(measurement_params.currentType);
  web_println("currentType: " + String(measurement_params.currentType));
  web_println(measurement_params.msg_coming ? "true" : "false");
  // uploadMeasureData(0.001, 0.002, -0.003);
  delay(1000);
}