#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include "config.h"
#include "injector.h"

class WebManager {
public:
  bool begin();

  void loop();

  bool connectSTA(uint32_t timeout = 20000);

  void startAP();

  void stopAP();

  bool isConnected() const;

  bool isAP() const { return apMode; }

  IPAddress localIP() const;

  String apSSID() const;


private:

    WebServer server{80};

    bool apMode=false;

    String apName;

    String hostName;

    //--------------------------------------------------

    static const uint32_t STA_RECONNECT_TIMEOUT_MS = 30000;

    static const uint32_t STA_RECONNECT_RETRY_MS = 10000;

    bool reconnecting=false;

    uint32_t reconnectTs=0;

    uint32_t reconnectAttempts=0;

    //--------------------------------------------------

    void beginSTA();

    void checkWiFi();

    void initMDNS();

    bool mountSPIFFS();

    void startWebServer();

    String loadFile(const char* path);

    //--------------------------------------------------
    // WEB pages
    //--------------------------------------------------

    void handleRoot();

    void handleStyle();

    void handleScript();

    //--------------------------------------------------
    // REST API
    //--------------------------------------------------

    void handleGetConfig();

    void handlePostConfig();

    void handleStatus();

    void handleStart();

    void handleStop();

    //--------------------------------------------------

    void handleRestart();

    void handleFactory();

    void handleNotFound();
};

extern WebManager Web;
