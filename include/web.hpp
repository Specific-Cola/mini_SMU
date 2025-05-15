#ifndef WEB_HPP
#define WEB_HPP
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// 测量参数结构体
typedef struct {
    // IV测量参数
    struct {
        double startVoltage;
        double endVoltage;
        double step;
    } iv;

    // Output测量参数
    struct {
        double vgStart;
        double vgEnd;
        double vgStep;
        double vdStart;
        double vdEnd;
        double vdStep;
    } output;

    // Transfer测量参数
    struct {
        double vgStart;
        double vgEnd;
        double vgStep;
        double vd;
    } transfer;

    // 当前测量类型
    enum MeasureType {
        None,
        IV,
        Output,
        Transfer
    } currentType;

    //测量数据上传
    struct {
        double Vds;
        double Vg;
        double current;
        double last_current;
    } measure_data;

    bool msg_coming;

} measurement_params_t;


void web_init();
void web_println(String msg);
void web_print(String msg);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void handleWebPage();
bool parseMeasurementParams(const char* json);
void uploadMeasureData(double Vds, double Vg, double current);

extern measurement_params_t measurement_params;

#endif