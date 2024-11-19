#ifndef WEB_HPP
#define WEB_HPP
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <WebSocketsServer.h>

typedef struct {
    std::vector<String> send_data;
    std::vector<String> recv_data;

}web_msg_t;

void web_init();
void handleSendData();
void web_println(String msg);
void web_print(String msg);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

#endif