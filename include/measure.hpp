#ifndef MEASURE_HPP
#define MEASURE_HPP
#include <Arduino.h>

typedef struct {
    float voltage;
    float current;
    
} measure_t;

void measure_init();

void iv_task_code();
void output_task_code();
void transfer_task_code();

void GetCurrent(float targetVoltage, float &voltage, float &current);
void updateIVVoltages();
void updateTransferVoltages();
void updateOutputVoltages();
void updateIdleVoltages();
uint16_t mapVoltageToDAC(float voltage, int channel);
float calculateVoltage(int16_t adcValue);
void setGainControlMode();
void writeDAC(int channel, uint16_t value);
void updateDACValues();

#endif