#ifndef TASK_HPP
#define TASK_HPP
#include <Arduino.h>
#include "web.hpp"


void task_init();
void detect_task_func(void *pvParameters);
void iv_task(void *pvParameters);
void output_task(void *pvParameters);
void transfer_task(void *pvParameters);

#endif