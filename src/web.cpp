#include "web.hpp"
#include "measure.hpp"
#include <WebSocketsServer.h>


const char* ssid = "ESP32_SMU";
const char* password = "12345678";
WebSocketsServer webSocket = WebSocketsServer(81);
extern measure_t measure_data;
extern web_msg_t web_msg;

WebServer server(80);
TaskHandle_t web_task_handle;

const String htmlContent = "<!DOCTYPE HTML><html><head><title>ESP32 Serial Monitor</title><style>#output {width: 100%; height: 300px;}</style></head><body><h1>ESP32 Serial Monitor</h1><textarea id=\"output\" readonly></textarea><br><input type=\"text\" id=\"input\" /><br><button onclick=\"sendMessage()\">Send</button><script>var connection = new WebSocket('ws://192.168.1.100:81');let messages = [];connection.onopen = function() {console.log('Connected to ESP32');};connection.onerror = function(error) {console.log('WebSocket Error:', error);};connection.onmessage = function(event) {console.log('Received data from ESP32:', event.data);messages.push(event.data);updateOutput();};function sendMessage() {var input = document.getElementById(\"input\").value;if (input.trim() !== \"\") {connection.send(input);messages.push(input);document.getElementById(\"input\").value = \"\";updateOutput();}}function updateOutput() {var output = document.getElementById(\"output\");output.value = messages.join(\"\");output.scrollTop = output.scrollHeight;}</script></body></html>";

void handleRoot() {
    server.send(200, "text/html", htmlContent);
}

// void handleSendMessage() {
//   if (server.arg("msg") != "") {
//     String msg = server.arg("msg");
//     web_msg.recv_data.push_back(msg); // 将接收到的消息保存到全局变量中
//     Serial.println(msg); // 将接收到的消息打印到串口
//   }
//   server.send(200, "text/plain", "OK");
// }

// void handleGetMessages() {
//   server.send(200, "text/plain", web_msg.send_data.back()); // 向网页返回最后一条发送的消息
// }


void web_task(void *arg) {
    while (true) {
        server.handleClient();
        webSocket.loop();
    }
}

void web_init() {

    IPAddress local_IP(192, 168, 1, 100);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);

    while (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        delay(500);
        Serial.println("Waiting for WiFi AP configuration");
    }
    while (!WiFi.softAP(ssid, password)) {
        delay(500);
        Serial.println("Creating WiFi AP");
    }
    Serial.println("WiFi AP created");

    server.on("/", handleRoot);
    // server.on("/send", handleSendMessage);
    // server.on("/receive", HTTP_GET, handleGetMessages);
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);


    Serial.println("Web server started");

    xTaskCreate(web_task, "web_task", 4096, NULL, 1, &web_task_handle);

}

void web_println(String msg) {
    // web_msg.send_data.push_back(msg + "\n");
    webSocket.broadcastTXT(msg + "\n");
}

void web_print(String msg) {
    // web_msg.send_data.push_back(msg);
    webSocket.broadcastTXT(msg);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
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
                Serial.printf("%s\n", payload);
                web_msg.recv_data.push_back((char*)payload); // 将接收到的消息保存到全局变量中
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

