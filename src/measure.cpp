#include "measure.hpp"
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include "task.hpp"
#include "web.hpp"

measure_t measure_data;
web_msg_t web_msg;

Adafruit_ADS1115 ads;  

// 定义AD5328的SYNC和LDAC引脚
const int SYNC_PIN = 5;//     Arduino 10  ESP8266 15
const int LDAC_PIN = 17;//      Arduino 9   ESP8266 2
const int RELAY1_PIN = 0; // ESP32的继电器引脚
 
const float OffsetCalibration[] = {0.050, 0.035, 0.070, -0.020, 0.050, -0.050, 0.030, -0.020};
// 定义起始电压、结束电压、步长和持续时间数组
float START_VOLTAGE[] = {-1, -4, 0, -6, 0, 0, 6, 0};
float END_VOLTAGE[] =   {1, 4, 0, -6, 0, 0, 6, 0};
float VsdSTEP = 0.0;   // IV 模式的步长
float VgSTEP = 0.1;    // Transfer 模式的步长
int sign = -1;   //标志位，用于判断是否需要反转Vg，因为 rem 无法接受负电压
unsigned long DURATION = 1000;
float Rf1 = 12000;
const float nA = 1000000000;
const float uA = 1000000;
const float mA = 1000;
float targetVoltage[8] ={0, 0, 0, -6, 0, 0, 6, 0};


void measure_init() {

  ads.begin();

  pinMode(SYNC_PIN, OUTPUT);
  digitalWrite(SYNC_PIN, HIGH);
  pinMode(LDAC_PIN, OUTPUT);
  digitalWrite(LDAC_PIN, HIGH);  

  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);

  // 初始化SPI通信
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2); // 设置SPI时钟
  SPI.setDataMode(SPI_MODE2);          // 设置SPI模式
  SPI.setBitOrder(MSBFIRST);           // 设置数据顺序为高位先行

  // 初始化LDAC引脚状态
  digitalWrite(LDAC_PIN, LOW);
  vTaskDelay(10);
  digitalWrite(LDAC_PIN, HIGH);
  
  setGainControlMode();  // 设置增益控制模式

  updateDACValues();  // 初始化DAC输出

}

void iv_task_code() {

  // 读取电压和电流
  float voltage, current;
  GetCurrent(targetVoltage[0], voltage, current);

  // 打印结果
  String ret = "Vsd: " + String(targetVoltage[0], 3) + ", Current: " + String(current, 12);
  web_println(ret);
  Serial.println(ret);
  
  // 更新目标电压
  updateIVVoltages();
  updateDACValues();
  vTaskDelay(DURATION);
}

void output_task_code() {

  float voltage, current;
  GetCurrent(targetVoltage[0], voltage, current);
  // 打印结果
  String ret = "Vg: " + String(targetVoltage[1], 3) + ", Vsd: " + String(targetVoltage[0], 3) + ", Current: " + String(current, 12);
  web_println(ret);
  Serial.println(ret);
  
  // 更新目标电压
  updateOutputVoltages();
  updateDACValues();
  vTaskDelay(DURATION);

}

void transfer_task_code() {

  // 读取电压和电流
  float voltage, current;
  GetCurrent(targetVoltage[1], voltage, current);

  // 打印结果
  String ret = "Vg: " + String(targetVoltage[1], 3) + ", Current: " + String(current, 12);
  web_println(ret);
  Serial.println(ret);
  // 更新目标电压
  updateTransferVoltages();
  updateDACValues();
  vTaskDelay(DURATION);

}


void GetCurrent(float targetVoltage, float &voltage, float &current) {
  int16_t adcValue = ads.readADC_SingleEnded(0);
  voltage = calculateVoltage(adcValue);
  if (targetVoltage <= 0) {
    voltage += 0.007;
    current = -voltage / Rf1;
  } else {
    current = voltage / Rf1;
  }
}

// 各模式的电压更新逻辑
void updateIVVoltages() {
  static int count = 0;
  static int direction = 1;
  
  targetVoltage[1] = START_VOLTAGE[1];
  if (count < (END_VOLTAGE[0] - START_VOLTAGE[0]) / VsdSTEP + 1) {
    targetVoltage[0] = START_VOLTAGE[0] + count * VsdSTEP;
    count += direction;
  } 

  if ((count == (END_VOLTAGE[0] - START_VOLTAGE[0]) / VsdSTEP + 1) || count == -1) {
    count -= direction;
    direction = -direction;
  }

}
void updateTransferVoltages(){
  static int count = 0;
  static int direction = 1;

  targetVoltage[0] = START_VOLTAGE[0];
  if (count < (END_VOLTAGE[1] - START_VOLTAGE[1]) / VgSTEP + 1) {
    targetVoltage[1] = START_VOLTAGE[1] + count * VgSTEP;
    count += direction;
    if (targetVoltage[1] < 0) {
      sign = -1;
      targetVoltage[1] = -targetVoltage[1];
    } else {
      sign = 1;
    }
  }
  if ((count == (END_VOLTAGE[1] - START_VOLTAGE[1]) / VgSTEP + 1) || count == -1) {
    count -= direction;
    direction = -direction;
  }

  
}
void updateOutputVoltages(){
    static int count1 = 0;
    static int count2 = 0;
    static int direction = 1;
    static int iv_count = 0;
    if (count2 == (END_VOLTAGE[1] - START_VOLTAGE[1]) / VgSTEP + 1) {
      count2 = 0;
    }
    if (count1 < (END_VOLTAGE[0] - START_VOLTAGE[0]) / VsdSTEP + 1) {
        targetVoltage[0] = START_VOLTAGE[0] + count1 * VsdSTEP;
        targetVoltage[1] = START_VOLTAGE[1] + count2 * VgSTEP;
        if (targetVoltage[1] < 0) {
            sign = -1;
            targetVoltage[1] = -targetVoltage[1];
        } else {
            sign = 1;
        }
        count1 += direction;
    } 
    if ((count1 == (END_VOLTAGE[0] - START_VOLTAGE[0]) / VsdSTEP + 1) || count1 == -1){
        count1 -= direction;
        direction = -direction;
        iv_count++;
    }
    if (iv_count == 2) {
      count2++;
      iv_count = 0;
    }
    
}

void updateIdleVoltages() {
  for (int channel = 0; channel < 1; channel++) {
    targetVoltage[channel] = 0;
  }
}


// 映射到0到4095的范围，-10V到10V，并应用通道偏移校准。不改动
uint16_t mapVoltageToDAC(float voltage, int channel) {
  float calibratedVoltage;
  calibratedVoltage = voltage + OffsetCalibration[channel];
  uint16_t dacValue = (uint16_t)(((calibratedVoltage + 10.0) / 20.0) * 4095);
  return dacValue;
} 

//计算电压，不改动
float calculateVoltage(int16_t adcValue) {
  // 最大量程6.144V，187.5uV/bit
  float voltage = adcValue * 0.1875 / 1000.0 ;
  return voltage;
}

//设置增益，不改动
void setGainControlMode() {
  // 设置增益控制模式，根据您的描述是最高位为1，次两位为00，5-4为11，3-0为0000
  uint16_t gainControlMode = (1 << 15) | (0 << 14) | (3 << 4) | 0;

  // 开始传输
  digitalWrite(SYNC_PIN, LOW);
  SPI.transfer16(gainControlMode);
  digitalWrite(SYNC_PIN, HIGH);
}

//写入DAC，不改动
void writeDAC(int channel, uint16_t value) {
  // 根据DAC数值确定命令字节
  uint16_t command;
  command = (0 << 15) | (channel << 12) | value;

  // 开始传输
  digitalWrite(SYNC_PIN, LOW);
  SPI.transfer16(command);
  digitalWrite(SYNC_PIN, HIGH);
}
void updateDACValues() {
  // 继电器决定正负Vg继电器
  if (sign == -1) {
    digitalWrite(RELAY1_PIN, HIGH);
  } else {
    digitalWrite(RELAY1_PIN, LOW);
  }
  for (int channel = 0; channel < 8; channel++) {
    uint16_t dacValue = mapVoltageToDAC(targetVoltage[channel], channel);
    writeDAC(channel, dacValue);
  }
  // 拉低LDAC引脚，更新所有通道的DAC输出
  digitalWrite(LDAC_PIN, LOW);
  delayMicroseconds(10);  // 确保LDAC引脚低电平保持时间足够
  digitalWrite(LDAC_PIN, HIGH);
}
