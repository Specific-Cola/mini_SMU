#include "web.hpp"
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include <FS.h>

const char *ssid = "ESP32_SMU";
const char *password = "12345678";
WebSocketsServer webSocket = WebSocketsServer(81);
measurement_params_t measurement_params;

WebServer server(80);
TaskHandle_t web_task_handle;

void handleWebPage()
{
    // 使用绝对路径访问文件
    String filePath = "/web.html";
    if (SPIFFS.exists(filePath))
    {
        File file = SPIFFS.open(filePath, "r");
        server.streamFile(file, "text/html");
        file.close();
    }
    else
    {
        Serial.println("找不到文件:" + filePath);
        server.send(404, "text/plain", "找不到文件:" + filePath);
    }
}

void web_task(void *arg)
{
    while (true)
    {
        server.handleClient();
        webSocket.loop();
        delay(1); // 防止看门狗复位
    }
}

void web_init()
{
    // 初始化SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // 配置WiFi
    IPAddress local_IP(192, 168, 1, 100);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);

    while (!WiFi.softAPConfig(local_IP, gateway, subnet))
    {
        delay(500);
        Serial.println("Waiting for WiFi AP configuration");
    }
    while (!WiFi.softAP(ssid, password))
    {
        delay(500);
        Serial.println("Creating WiFi AP");
    }
    Serial.println("WiFi AP created");

    // 设置Web服务器路由
    server.on("/", handleWebPage);
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.println("Web server started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    xTaskCreate(web_task, "web_task", 8192, NULL, 1, &web_task_handle);
}

void web_println(String msg)
{
    String jsonMsg;
    StaticJsonDocument<200> doc;
    doc["type"] = "log";
    doc["message"] = msg;
    serializeJson(doc, jsonMsg);
    webSocket.broadcastTXT(jsonMsg);
}

void web_print(String msg)
{
    String jsonMsg;
    StaticJsonDocument<200> doc;
    doc["type"] = "log";
    doc["message"] = msg;
    serializeJson(doc, jsonMsg);
    webSocket.broadcastTXT(jsonMsg);
}

bool parseMeasurementParams(const char *json)
{
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error)
    {
        Serial.print("JSON解析失败: ");
        Serial.println(error.c_str());
        return false;
    }

    const char *type = doc["type"];

    if (strcmp(type, "iv") == 0)
    {
        measurement_params.currentType = measurement_params_t::IV;
        measurement_params.iv.startVoltage = doc["startVoltage"].as<double>();
        measurement_params.iv.endVoltage = doc["endVoltage"].as<double>();
        measurement_params.iv.step = doc["step"].as<double>();
    }
    else if (strcmp(type, "output") == 0)
    {
        measurement_params.currentType = measurement_params_t::Output;
        measurement_params.output.vgStart = doc["vgStart"].as<double>();
        measurement_params.output.vgEnd = doc["vgEnd"].as<double>();
        measurement_params.output.vgStep = doc["vgStep"].as<double>();
        measurement_params.output.vdStart = doc["vdStart"].as<double>();
        measurement_params.output.vdEnd = doc["vdEnd"].as<double>();
        measurement_params.output.vdStep = doc["vdStep"].as<double>();
    }
    else if (strcmp(type, "transfer") == 0)
    {
        measurement_params.currentType = measurement_params_t::Transfer;
        measurement_params.transfer.vgStart = doc["vgStart"].as<double>();
        measurement_params.transfer.vgEnd = doc["vgEnd"].as<double>();
        measurement_params.transfer.vgStep = doc["vgStep"].as<double>();
        measurement_params.transfer.vd = doc["vd"].as<double>();
    }
    else if (strcmp(type, "none") == 0)
    {
        measurement_params.currentType = measurement_params_t::None;
        return true;
    }
    else
    {
        return false;
    }

    return true;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected!\n", num);
        break;

    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    }
    break;

    case WStype_TEXT:
    {
        // 解析测量参数
        measurement_params.msg_coming = parseMeasurementParams((char *)payload);
        if (measurement_params.msg_coming)
        {
            String response;
            StaticJsonDocument<200> doc;
            doc["type"] = "response";
            doc["message"] = "参数设置成功";
            serializeJson(doc, response);
            webSocket.sendTXT(num, response);
        }
        else
        {
            String response;
            StaticJsonDocument<200> doc;
            doc["type"] = "error";
            doc["message"] = "参数设置失败";
            serializeJson(doc, response);
            webSocket.sendTXT(num, response);
        }
    }
    break;

    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
        break;
    }
}

void uploadMeasureData(double Vds, double Vg, double current)
{
    measurement_params.measure_data.Vds = Vds;
    measurement_params.measure_data.Vg = Vg;
    measurement_params.measure_data.current = current;
    String jsonMsg;
    StaticJsonDocument<200> doc;
    doc["type"] = "data";
    doc["cur_type"] = measurement_params.currentType;
    doc["Vds"] = measurement_params.measure_data.Vds;
    doc["Vg"] = measurement_params.measure_data.Vg;
    doc["current"] = measurement_params.measure_data.current;
    serializeJson(doc, jsonMsg);
    webSocket.broadcastTXT(jsonMsg);
}