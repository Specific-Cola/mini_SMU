#include "task.hpp"
#include "measure.hpp"
#include "web.hpp"

TaskHandle_t detect_task;
TaskHandle_t iv_curves_task;
TaskHandle_t output_curves_task;
TaskHandle_t transfer_curves_task;

SemaphoreHandle_t countSemaphore;
String task_status;
extern web_msg_t web_msg;

void task_init() {
  // 创建信号量
  countSemaphore = xSemaphoreCreateCounting(1,0);
  // 创建任务
  xTaskCreate(detect_task_func, "detect_task", 4096, NULL, 1, &detect_task);
  xTaskCreate(iv_task, "iv_task", 4096, NULL, 1, &iv_curves_task);
  xTaskCreate(output_task, "output_task", 4096, NULL, 1, &output_curves_task);
  xTaskCreate(transfer_task, "transfer_task", 4096, NULL, 1, &transfer_curves_task);

}

void detect_task_func(void *pvParameters) {

  while (1) {
    if (web_msg.recv_data.empty() == false) {
      String input = web_msg.recv_data.back();  // 读取一行数据
      web_msg.recv_data.clear();  // 清空接收缓存

    // if (Serial.available() > 0) {
    //   String input = Serial.readStringUntil('\n');  // 读取一行数据

      if (input.equals("iv") || input.equals("output") || input.equals("transfer")) {
        Serial.print("将进入任务: ");
        web_print("将进入任务: ");
        task_status = input;  // 保存任务名称
        Serial.println(input);  // 输出收到的数据
        web_println(input);  // 输出收到的数据
        if (uxSemaphoreGetCount(countSemaphore) == 0) {
          xSemaphoreGive(countSemaphore);  // 释放信号量
        }
      } else if (input.equals("stop")) {
        task_status = "stop";  // 清空任务名称
        Serial.println("停止任务");
        web_println("停止任务");
        if (uxSemaphoreGetCount(countSemaphore) == 1) {
          xSemaphoreTake(countSemaphore, portMAX_DELAY);
        }
      } else {
        Serial.println("未知命令");
        web_println("未知命令");
      }
    }
    vTaskDelay(500);
  }
}

void iv_task(void *pvParameters) {

  while (1) {
    if (xSemaphoreTake(countSemaphore, portMAX_DELAY) == pdTRUE) {
      if (task_status.equals("iv")) {
        // Serial.println("IV task running");
        // web_println("IV task running");
        // vTaskDelay(1000);
        iv_task_code();
      }
      xSemaphoreGive(countSemaphore);
    } else {
      
    }
    vTaskDelay(10);
  }
}

void output_task(void *pvParameters) {
  
  while (1) {
    if (xSemaphoreTake(countSemaphore, portMAX_DELAY) == pdTRUE) {
      if (task_status.equals("output")) {
        // Serial.println("output task running");
        // web_print("output task running");
        // vTaskDelay(1000);
        output_task_code();
      }
      xSemaphoreGive(countSemaphore);
    } else {
      
    }
    vTaskDelay(10);
    
  }
}

void transfer_task(void *pvParameters) {

  while (1) {
    if (xSemaphoreTake(countSemaphore, portMAX_DELAY) == pdTRUE) {
      if (task_status.equals("transfer")) {
        // Serial.println("transfer task running");
        // web_println("transfer task running");
        // vTaskDelay(1000);
        transfer_task_code();
      }
      xSemaphoreGive(countSemaphore);
    } else {
      
    }
    vTaskDelay(10);
  }
}
