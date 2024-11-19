#include <Arduino.h>
#include "measure.hpp"
#include "web.hpp"
#include "task.hpp"

void setup() {
  Serial.begin(115200);

  measure_init();
  task_init();
  web_init();
}

void loop() {
  
}