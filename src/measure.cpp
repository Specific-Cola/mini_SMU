#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include "web.hpp"
#include "measure.hpp"

Adafruit_ADS1115 ads;

const int SYNC_PIN = 5;
const int LDAC_PIN = 4;
const int RELAY1_PIN = 32; // 切换栅压正负引脚
const int RELAY2_PIN = 33; // 切换反馈电阻引脚

const double OffsetCalibration[] = {0.055, 0.050, 0.070, -0.020, 0.050, -0.050, 0.030, -0.060};

// 定义起始电压、结束电压、步长和持续时间数组
double START_VOLTAGE[] = {0, 0, 0, -6, 0, 0, 6, 0};
double END_VOLTAGE[] = {0, 0, 0, -6, 0, 0, 6, 0};
double VsdSTEP = 0.0; // IV 模式的步长
double VgSTEP = 0.0;  // Transfer 模式的步长
int sign = -1;
double DURATION = 2000;
double RL = 0.0;
double Rf1 = 4700.0;
double Rf2 = 470000.0;
double Rf = Rf1;
double Sampletime = 5000;
const double nA = 1000000000.0;
float startTime = 0;         // 记录It模式开始的时间戳
bool isItModeActive = false; // 用于跟踪是否已经进入It模式

enum Mode
{
  Idle,
  IV,
  Transfer,
  Output,
  It
};                       // 增加Idle模式
Mode currentMode = Idle; // 初始化当前模式为Idle

double targetVoltage[8] = {0, 0, 0, -6, 0, 6, 0, 0};
int count = 0;
double last_current;

void setMode(String command);
void GetCurrent(double targetVoltage, double &voltage, double &current, double &Vds);
void updateTargetVoltages();
void updateIVVoltages();
void updateTransferVoltages();
void updateOutputVoltages();
void updateIdleVoltages();
uint16_t mapVoltageToDAC(double voltage, int channel);
double calculateVoltage(int16_t adcValue);
void setGainControlMode();
void writeDAC(int channel, uint16_t value);
void updateDACValues();
void readAndAssignIVParameters();
void readAndAssignTransferParameters();
void readAndAssignOutputParameters();
void measure_task(void *pvParameters);
void updateItVoltages();
void readAndAssignItParameters();


TaskHandle_t measure_task_handle;

void measure_init()
{
  ads.begin();

  pinMode(SYNC_PIN, OUTPUT);
  digitalWrite(SYNC_PIN, HIGH);
  pinMode(LDAC_PIN, OUTPUT);
  digitalWrite(LDAC_PIN, HIGH);

  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);

  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY2_PIN, LOW);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.setDataMode(SPI_MODE2);
  SPI.setBitOrder(MSBFIRST);

  digitalWrite(LDAC_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(LDAC_PIN, HIGH);

  xTaskCreate(measure_task, "measure_task", 8192, NULL, 1, &measure_task_handle);
}

void measure_task(void *pvParameters)
{
  Serial.println("measure_task start");
  while (true)
  {
    //有线模式任务设置
    if (Serial.available() > 0)
    {
      String command = Serial.readStringUntil('\n');
      setMode(command); // 设置模式
    }

    //无线模式任务设置
    if (measurement_params.msg_coming)
    {
      web_setMode();
      measurement_params.msg_coming = false;
    }

    if (count == 2)
    {
      measurement_params.currentType = measurement_params_t::None;
      currentMode = Idle;
    }
    double voltage, current, Vds;
    setGainControlMode();
    updateTargetVoltages();
    if (targetVoltage[1] < 0)
    {
      sign = -1;
      digitalWrite(RELAY1_PIN, HIGH);
      targetVoltage[1] = -targetVoltage[1];
    }
    else
    {
      sign = 1;
      digitalWrite(RELAY1_PIN, LOW);
    }
    updateDACValues();
    targetVoltage[1] = targetVoltage[1] * sign;
    GetCurrent(targetVoltage[0], voltage, current, Vds);
    
    // 滞回比较：切换阈值设为800nA，返回阈值设为900nA
    static bool isHighGain = false;  // 增加状态记忆
    
    if (!isHighGain && (abs(current * nA) < 8000.0)) 
    {
      Rf = Rf2;
      digitalWrite(RELAY2_PIN, HIGH);
      vTaskDelay(DURATION/2);  // 延长稳定时间
      isHighGain = true;  // 锁定状态
      GetCurrent(targetVoltage[0], voltage, current, Vds);  // 重新测量
    }
    else if (isHighGain && (abs(current * nA) > 9000.0)) 
    {
      Rf = Rf1;
      digitalWrite(RELAY2_PIN, LOW);
      vTaskDelay(DURATION/2);  // 延长稳定时间
      isHighGain = false;  // 解锁状态
      GetCurrent(targetVoltage[0], voltage, current, Vds);  // 重新测量
    }
    if (currentMode == IV)
    {
      Serial.print(targetVoltage[0], 3);
    }
    if (currentMode == It)
    {
      if (!isItModeActive)
      {
        startTime = millis();
        isItModeActive = true;
      }
      // 计算从进入It模式开始的时间，单位为秒
      float elapsedTime = (millis() - startTime) / 1000;
      Serial.print(elapsedTime, 2);
    }
    else
    {
      isItModeActive = false;
    }
    if (currentMode == Transfer)
    {
      Serial.print(targetVoltage[1] * 10, 2);
    }
    if (currentMode == Output)
    {
      Serial.print(targetVoltage[1] * 10, 1);
      Serial.print(", ");
      Serial.print(targetVoltage[0], 3);
    }
    Serial.print(", ");
    Serial.println(current * nA, 3);

    web_update(targetVoltage[0], targetVoltage[1] * 10, current * nA);

    vTaskDelay(DURATION/2);
  }
}

void setMode(String command)
{
  count = 0;
  vTaskDelay(1000);
  if (command == "IV")
  {
    readAndAssignIVParameters(); // 获取输入参数并赋值
    currentMode = IV;
  }
  else if (command == "Transfer")
  {
    readAndAssignTransferParameters();
    currentMode = Transfer;
  }
  else if (command == "Output")
  {
    readAndAssignOutputParameters();
    currentMode = Output;
  }
  else if (command == "IT")
  {
    readAndAssignItParameters();
    currentMode = It;
  }
  else if (command == "Idle")
  {
    currentMode = Idle;
  }
  else
  {
    Serial.println("Unknown command");
  }
}

void GetCurrent(double targetVoltage, double &voltage, double &current, double &Vds)
{
  int16_t adcValue = ads.readADC_SingleEnded(0);
  voltage = calculateVoltage(adcValue) - 0.02;
  if (targetVoltage < 0)
  {
    current = -voltage / Rf;
  }
  else
  {
    current = voltage / Rf;
  }
  if (targetVoltage == 0)
  {
    Vds = 0;
  }
  else
  {
    Vds = targetVoltage * (abs(targetVoltage) * Rf - RL * voltage) / (abs(targetVoltage) * Rf);
  }
}

void updateTargetVoltages()
{
  switch (currentMode)
  {
  case IV:
    updateIVVoltages();
    break;
  case Transfer:
    updateTransferVoltages();
    break;
  case Output:
    updateOutputVoltages();
    break;
  case It:
    updateItVoltages();
    break;
  case Idle:
    updateIdleVoltages();
    break;
  }
}

// 各模式的电压更新逻辑
void updateIVVoltages()
{
  static int Vsd_count = 0;
  static int direction = -1;
  static double pre = 0;  // 保存上一个值
  pre = targetVoltage[0]; // 更新 pre 为当前值
  targetVoltage[0] += VsdSTEP * direction;
  if (targetVoltage[0] > (END_VOLTAGE[0] + VsdSTEP / 2) || targetVoltage[0] < (START_VOLTAGE[0] - VsdSTEP / 2))
  {
    direction = -direction;
    targetVoltage[0] += VsdSTEP * direction;
  }
  if (pre > 0 && targetVoltage[0] <= 0 || pre < 0 && targetVoltage[0] >= 0)
  {
    Vsd_count++;
  }
  if (Vsd_count == 2)
  {
    Vsd_count = 0;
    count = 2;
    pre = 0;
  }
}
void updateTransferVoltages()
{
  static int phase = 0;  // 当前阶段：0-到达START，1-从START到END，2-从END回到START，3-从START回到0
  static double pre = 0; // 上一个电压值

  pre = targetVoltage[1]; // 保存当前值作为上次值

  switch (phase)
  {
  case 0: // 从 0 达到 START_VOLTAGE
    targetVoltage[1] -= 0.5;
    if (targetVoltage[1] <= START_VOLTAGE[1])
    {
      targetVoltage[1] = START_VOLTAGE[1];
      phase = 1; // 切换到下一阶段
    }
    break;

  case 1: // 从 START_VOLTAGE 到 END_VOLTAGE
    targetVoltage[1] += VgSTEP;
    if (targetVoltage[1] >= END_VOLTAGE[1])
    {
      targetVoltage[1] = END_VOLTAGE[1];
      phase = 2; // 切换到下一阶段
    }
    break;

  case 2: // 从 END_VOLTAGE 回到 START_VOLTAGE
    targetVoltage[1] -= VgSTEP;
    if (targetVoltage[1] <= START_VOLTAGE[1])
    {
      targetVoltage[1] = START_VOLTAGE[1];
      phase = 3; // 切换到下一阶段
    }
    break;

  case 3: // 从 START_VOLTAGE 回到 0
    targetVoltage[1] += 0.5;
    if (targetVoltage[1] >= 0)
    {
      targetVoltage[1] = 0;
      phase = 4; // 完成所有阶段
    }
    break;

  default:
    break;
  }

  // 如果跨越零点，记录
  static int Vg_count = 0;
  if ((pre > 0 && targetVoltage[1] <= 0) || (pre < 0 && targetVoltage[1] >= 0))
  {
    Vg_count++;
  }

  if (phase == 4)
  {
    // 逻辑完成后的处理，例如重置、信号发送等
    count = 2; // 假设需要发送完成信号
    Vg_count = 0;
    phase = 0; // 重置阶段
  }
}

void updateOutputVoltages()
{
  static int Vsd_count = 0;
  static int Vg_count = 0;
  static int Vsd_direction = -1;
  static int Vg_direction = -1;
  static double Vsd_pre = 0; // 保存上一个值
  static double Vg_pre = 0;  // 保存上一个值
  static int iv_start = -1;
  Vsd_pre = targetVoltage[0]; // 更新 pre 为当前值
  Vg_pre = targetVoltage[1];  // 更新 pre 为当前值
  if (iv_start == 1)
  {
    targetVoltage[0] += VsdSTEP * Vsd_direction;
  }
  else if (iv_start == -1)
  {
    Vsd_count = 2;
  }
  if (targetVoltage[0] > (END_VOLTAGE[0] + VsdSTEP / 2) || targetVoltage[0] < (START_VOLTAGE[0] - VsdSTEP / 2))
  {
    Vsd_direction = -Vsd_direction;
    targetVoltage[0] += VsdSTEP * Vsd_direction;
  }
  if (Vsd_pre > 0 && targetVoltage[0] <= 0 || Vsd_pre < 0 && targetVoltage[0] >= 0)
  {
    Vsd_count++;
  }
  if (Vsd_count == 2)
  {
    Vsd_count = 0;
    targetVoltage[1] += VgSTEP * Vg_direction;
    Vsd_pre = 0;
  }
  if (targetVoltage[1] > (END_VOLTAGE[1] + VgSTEP / 2) || targetVoltage[1] < (START_VOLTAGE[1] - VgSTEP / 2))
  {
    iv_start = -iv_start;
    Vg_direction = -Vg_direction;
    targetVoltage[1] += VgSTEP * Vg_direction;
  }
  if (Vg_pre > 0 && targetVoltage[1] <= 0 || Vg_pre < 0 && targetVoltage[1] >= 0)
  {
    Vg_count++;
  }
  if (Vg_count == 2)
  {
    Vg_count = 0;
    count = 2;
    Vg_pre = 0;
    iv_start = -1;
  }
}

void updateItVoltages()
{
  static int internalCount = 0; // 内部计数器，初始化为 0
  targetVoltage[0] = START_VOLTAGE[0];

  // 每次调用函数时内部计数器加 1
  internalCount++;

  // 检查内部计数器是否达到采样次数
  if (internalCount >= Sampletime)
  {
    count = 2; // 达到目标次数时将内部计数器置为 2
    internalCount = 0;
  }
}

void updateIdleVoltages()
{
  for (int channel = 0; channel < 1; channel++)
  {
    targetVoltage[channel] = 0;
  }
  VgSTEP = 0;
  VsdSTEP = 0;
  DURATION = 1000;
}

void readAndAssignIVParameters()
{
  while (!Serial.available())
  {
    // 等待串口数据输入
  }

  // 从串口读取输入数据
  String parameters = Serial.readStringUntil('\n');
  parameters.trim(); // 去掉首尾空格或换行符

  // 解析输入的三个参数
  int firstComma = parameters.indexOf(',');
  int secondComma = parameters.indexOf(',', firstComma + 1);

  if (firstComma != -1 && secondComma != -1)
  {
    // 提取并赋值到全局变量
    START_VOLTAGE[0] = parameters.substring(0, firstComma).toDouble();
    END_VOLTAGE[0] = parameters.substring(firstComma + 1, secondComma).toDouble();
    VsdSTEP = parameters.substring(secondComma + 1).toDouble();
  }
  else
  {
    // 如果格式错误，提供默认值以防程序中断
    START_VOLTAGE[0] = 0; // 默认值
    END_VOLTAGE[0] = 0;   // 默认值
    VsdSTEP = 0;          // 默认值
  }
}

void readAndAssignTransferParameters()
{
  while (!Serial.available())
  {
    // 等待串口数据输入
  }
  String parameters = Serial.readStringUntil('\n');
  parameters.trim();
  int firstComma = parameters.indexOf(',');
  int secondComma = parameters.indexOf(',', firstComma + 1);
  int thirdComma = parameters.indexOf(',', secondComma + 1);

  if (firstComma != -1 && secondComma != -1 && thirdComma != -1)
  {
    double firstParam = parameters.substring(0, firstComma).toDouble();
    targetVoltage[0] = firstParam;
    START_VOLTAGE[1] = parameters.substring(firstComma + 1, secondComma).toDouble() / 10.0;
    END_VOLTAGE[1] = parameters.substring(secondComma + 1, thirdComma).toDouble() / 10.0;
    VgSTEP = parameters.substring(thirdComma + 1).toDouble() / 10.0;
  }
  else
  {
    START_VOLTAGE[0] = 0; // 默认值
    END_VOLTAGE[0] = 0;   // 默认值
    START_VOLTAGE[1] = 0; // 默认值
    END_VOLTAGE[1] = 0;   // 默认值
    VgSTEP = 0;           // 默认值
  }
}

void readAndAssignOutputParameters()
{ // Vsd start,end,step,Vg start, end, step
  while (!Serial.available())
  {
  }
  String parameters = Serial.readStringUntil('\n');
  parameters.trim();
  int firstComma = parameters.indexOf(',');
  int secondComma = parameters.indexOf(',', firstComma + 1);
  int thirdComma = parameters.indexOf(',', secondComma + 1);
  int fourthComma = parameters.indexOf(',', thirdComma + 1);
  int fifthComma = parameters.indexOf(',', fourthComma + 1);

  if (firstComma != -1 && secondComma != -1 && thirdComma != -1 && fourthComma != -1 && fifthComma != -1)
  {
    START_VOLTAGE[0] = parameters.substring(0, firstComma).toDouble();                      // Vsd start
    END_VOLTAGE[0] = parameters.substring(firstComma + 1, secondComma).toDouble();          // Vsd end
    VsdSTEP = parameters.substring(secondComma + 1, thirdComma).toDouble();                 // Vsd step
    START_VOLTAGE[1] = parameters.substring(thirdComma + 1, fourthComma).toDouble() / 10.0; // Vg start
    END_VOLTAGE[1] = parameters.substring(fourthComma + 1, fifthComma).toDouble() / 10.0;   // Vg end
    VgSTEP = parameters.substring(fifthComma + 1).toDouble() / 10.0;                        // Vg step
  }
  else
  {
    START_VOLTAGE[0] = 0; // 默认值
    END_VOLTAGE[0] = 0;   // 默认值
    VsdSTEP = 0;          // 默认值
    START_VOLTAGE[1] = 0; // 默认值
    END_VOLTAGE[1] = 0;   // 默认值
    VgSTEP = 0;           // 默认值
  }
}
void readAndAssignItParameters()
{
  while (!Serial.available())
  {
    // 等待串口数据输入
  }

  // 从串口读取输入数据
  String parameters = Serial.readStringUntil('\n');
  parameters.trim(); // 去掉首尾空格或换行符

  // 解析输入的三个参数
  int firstComma = parameters.indexOf(',');
  // int secondComma = parameters.indexOf(',', firstComma + 1);

  if (firstComma != -1 )
  {
    // 提取并赋值到全局变量
    START_VOLTAGE[0] = parameters.substring(0, firstComma).toDouble();
    Sampletime = parameters.substring(firstComma + 1).toDouble();
  }
  else
  {
    START_VOLTAGE[0] = 0; // 默认值
  }
}

// 映射到0到4095的范围，-10V到10V，并应用通道偏移校准。不改动
uint16_t mapVoltageToDAC(double voltage, int channel)
{
  double calibratedVoltage;
  calibratedVoltage = voltage + OffsetCalibration[channel];
  uint16_t dacValue = (uint16_t)(((calibratedVoltage + 10.0) / 20.0) * 4095);
  return dacValue;
}

// 计算电压，不改动
double calculateVoltage(int16_t adcValue)
{
  // 最大量程6.144V，187.5uV/bit
  double voltage = adcValue * 0.1875 / 1000.0;
  return voltage;
}

// 设置增益，不改动
void setGainControlMode()
{
  // 设置增益控制模式，根据您的描述是最高位为1，次两位为00，5-4为11，3-0为0000
  uint16_t gainControlMode = (1 << 15) | (0 << 14) | (3 << 4) | 0;

  // 开始传输
  digitalWrite(SYNC_PIN, LOW);
  SPI.transfer16(gainControlMode);
  digitalWrite(SYNC_PIN, HIGH);
}

// 写入DAC，不改动
void writeDAC(int channel, uint16_t value)
{
  // 根据DAC数值确定命令字节
  uint16_t command;
  command = (0 << 15) | (channel << 12) | value;

  // 开始传输
  digitalWrite(SYNC_PIN, LOW);
  SPI.transfer16(command);
  digitalWrite(SYNC_PIN, HIGH);
}
void updateDACValues()
{
  for (int channel = 0; channel < 8; channel++)
  {
    uint16_t dacValue = mapVoltageToDAC(targetVoltage[channel], channel);
    writeDAC(channel, dacValue);
  }
  // 拉低LDAC引脚，更新所有通道的DAC输出
  digitalWrite(LDAC_PIN, LOW);
  delayMicroseconds(10); // 确保LDAC引脚低电平保持时间足够
  digitalWrite(LDAC_PIN, HIGH);
}

void web_setMode()
{
  count = 0;
  if (measurement_params.currentType == measurement_params_t::IV)
  {
    START_VOLTAGE[0] = measurement_params.iv.startVoltage;
    END_VOLTAGE[0] = measurement_params.iv.endVoltage;
    VsdSTEP = measurement_params.iv.step;
    currentMode = IV;

  }
  else if (measurement_params.currentType == measurement_params_t::Transfer)
  {
    START_VOLTAGE[1] = measurement_params.transfer.vgStart;
    END_VOLTAGE[1] = measurement_params.transfer.vgEnd;
    VgSTEP = measurement_params.transfer.vgStep;
    targetVoltage[0] = measurement_params.transfer.vd;
    currentMode = Transfer;
  }
  else if (measurement_params.currentType == measurement_params_t::Output)
  {
    START_VOLTAGE[0] = measurement_params.output.vdStart;
    END_VOLTAGE[0] = measurement_params.output.vdEnd;
    VsdSTEP = measurement_params.output.vdStep;
    START_VOLTAGE[1] = measurement_params.output.vgStart;
    END_VOLTAGE[1] = measurement_params.output.vgEnd;
    VgSTEP = measurement_params.output.vgStep;
    currentMode = Output;
  }
  else
  {
    currentMode = Idle;
  }
}

void web_update(double Vds, double Vg, double current)
{
  if (currentMode != Idle){
    uploadMeasureData(Vds, Vg, current);
  }

}