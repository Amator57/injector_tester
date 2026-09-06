#include "web_manager.h"

#include <ArduinoJson.h>
#include <ESP.h>

WebManager Web;

//======================================================
// Допоміжні функції
//======================================================

static String modeName(uint8_t m) {
  switch (m) {
    case MODE_SINGLE:     return "single";
    case MODE_BURST:      return "burst";
    case MODE_CONTINUOUS: return "continuous";
    case MODE_HOLD:       return "hold";
  }

  return "burst";
}

//------------------------------------------------------

static uint8_t modeFromName(const String &s) {
  if (s == "single")     return MODE_SINGLE;
  if (s == "continuous") return MODE_CONTINUOUS;
  if (s == "hold")       return MODE_HOLD;

  return MODE_BURST;
}

//------------------------------------------------------

static String stateName(uint8_t s) {
  switch (s) {
    case ST_IDLE:       return "idle";
    case ST_PULSE:      return "pulse";
    case ST_GAP:        return "gap";
    case ST_BURST_WAIT: return "burst_wait";
    case ST_HOLD:       return "hold";
    case ST_DONE:       return "done";
  }

  return "idle";
}

//------------------------------------------------------

static uint32_t clampU32(float v, float lo, float hi) {
  if (v < lo) v = lo;
  if (v > hi) v = hi;

  return (uint32_t)(v + 0.5f);
}

//------------------------------------------------------------

bool WebManager::begin() {
  Serial.println();
  Serial.println("==================================");
  Serial.println("WiFi Manager");
  Serial.println("==================================");

  if (!mountSPIFFS()) {
    Serial.println("SPIFFS mount failed");
  }

  Config.load();

  if (connectSTA()) {
    Serial.println("STA mode");
    initMDNS();
  } else {
    Serial.println("AP mode");
    startAP();
  }

  startWebServer();

  return true;
}

//------------------------------------------------------------

void WebManager::loop() {
  server.handleClient();

  checkWiFi();
}

//------------------------------------------------------------

void WebManager::beginSTA() {
  WiFi.mode(WIFI_STA);

  //--------------------------------------------------------
  // DHCP / Static IP
  //--------------------------------------------------------

  if (!Config.network.dhcp) {
    IPAddress ip;
    IPAddress gw;
    IPAddress mask;
    IPAddress dns1;
    IPAddress dns2;

    ip.fromString(Config.network.ip);
    gw.fromString(Config.network.gateway);
    mask.fromString(Config.network.subnet);
    dns1.fromString(Config.network.dns1);
    dns2.fromString(Config.network.dns2);

    WiFi.config(ip, gw, mask, dns1, dns2);
  }

  //--------------------------------------------------------

  WiFi.begin(
    Config.network.ssid.c_str(),
    Config.network.password.c_str());
}

//------------------------------------------------------------

bool WebManager::connectSTA(uint32_t timeout) {
  if (Config.network.ssid.isEmpty()) {
    Serial.println("SSID not configured");
    return false;
  }

  beginSTA();

  Serial.print("Connecting");

  uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);

    Serial.print(".");

    if (millis() - start > timeout) {
      Serial.println();
      Serial.println("Connection timeout");

      WiFi.disconnect(true);

      return false;
    }
  }

  Serial.println();
  Serial.println("Connected");

  Serial.print("IP : ");

  Serial.println(WiFi.localIP());

  apMode = false;

  return true;
}

//------------------------------------------------------------

void WebManager::checkWiFi() {
  if (apMode)
    return;

  uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (reconnecting) {
      reconnecting = false;

      Serial.println();
      Serial.println("WiFi reconnected");

      Serial.print("IP : ");

      Serial.println(WiFi.localIP());
    }

    return;
  }

  if (Config.network.ssid.isEmpty())
    return;

  if (!reconnecting) {
    reconnecting = true;

    reconnectTs = now;

    reconnectAttempts = 0;

    Serial.println();
    Serial.println("WiFi lost, reconnecting...");
  }

  if (now - reconnectTs >= STA_RECONNECT_TIMEOUT_MS) {
    Serial.println("STA reconnect failed");

    reconnecting = false;

    startAP();

    return;
  }

  if (now - reconnectTs >= reconnectAttempts * STA_RECONNECT_RETRY_MS) {
    reconnectAttempts++;

    Serial.printf("Reconnect attempt %u\n", reconnectAttempts);

    WiFi.disconnect();

    beginSTA();
  }
}

//------------------------------------------------------------

void WebManager::startAP() {
  apMode = true;

  apName = Config.device.deviceName;

  if (apName.isEmpty())
    apName = "InjectorTester";

  apName += "_Setup";

  WiFi.mode(WIFI_AP);

  WiFi.softAP(apName.c_str());

  Serial.println();

  Serial.println("Access Point started");

  Serial.print("SSID : ");

  Serial.println(apName);

  Serial.print("IP   : ");

  Serial.println(WiFi.softAPIP());

  Serial.println("Web  : http://192.168.4.1");

  initMDNS();
}

//------------------------------------------------------------

void WebManager::stopAP() {
  WiFi.softAPdisconnect(true);

  apMode = false;
}

//------------------------------------------------------------

bool WebManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

//------------------------------------------------------------

IPAddress WebManager::localIP() const {
  if (apMode)
    return WiFi.softAPIP();

   return WiFi.localIP();
}

//------------------------------------------------------------

String WebManager::apSSID() const { return apName; }

//------------------------------------------------------------

void WebManager::initMDNS() {
  String h = Config.device.deviceName;

  h.toLowerCase();

  String clean;

  for (unsigned int i = 0; i < h.length(); ++i) {
    char c = h[i];

    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      clean += c;
  }

  if (clean.isEmpty())
    clean = "injectortester";

  hostName = clean;

  if (MDNS.begin(hostName.c_str())) {
    MDNS.addService("http", "tcp", 80);

    Serial.print("mDNS : http://");

    Serial.print(hostName);

    Serial.println(".local");
  } else {
    Serial.println("mDNS start failed");
  }
}

//------------------------------------------------------------

bool WebManager::mountSPIFFS() {
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS mounted");
    return true;
  }

  Serial.println("SPIFFS mount FAILED");

  return false;
}

//------------------------------------------------------------

String WebManager::loadFile(const char *path) {
  File file = SPIFFS.open(path, "r");

  if (!file) {
    Serial.printf("Cannot open %s\n", path);
    return "";
  }

  String html = file.readString();

  file.close();

  return html;
}

//------------------------------------------------------------

void WebManager::startWebServer() {
  Serial.println();
  Serial.println("=================================");
  Serial.println("Web Server started");
  Serial.println("Firmware version: 1.0.0");
  Serial.println("=================================");

  server.on("/",
  HTTP_GET,
  [this]()
  {
    handleRoot();
  });

  server.on("/style.css",
  HTTP_GET,
  [this]()
  {
    handleStyle();
  });

  server.on("/script.js",
  HTTP_GET,
  [this]()
  {
    handleScript();
  });

  //--------------------------------------------------
  // REST API
  //--------------------------------------------------

  server.on("/api/config",
  HTTP_GET,
  [this]()
  {
    handleGetConfig();
  });

  server.on("/api/config",
  HTTP_POST,
  [this]()
  {
    handlePostConfig();
  });

  server.on("/api/status",
  HTTP_GET,
  [this]()
  {
    handleStatus();
  });

  server.on("/api/start",
  HTTP_POST,
  [this]()
  {
    handleStart();
  });

  server.on("/api/stop",
  HTTP_POST,
  [this]()
  {
    handleStop();
  });

  //--------------------------------------------------
  // Restart
  //--------------------------------------------------

  Serial.println("Register /api/restart");
  server.on(
    "/api/restart",
    HTTP_POST,
    [this]()
    {
      Injector.stop();

      server.send(
        200,
        "text/plain",
        "Restarting");

      delay(300);

      ESP.restart();
    });

  //--------------------------------------------------
  // Factory Reset
  //--------------------------------------------------

  server.on(
    "/api/factory",
    HTTP_POST,
    [this]()
    {
      Injector.stop();

      Config.loadDefaults();

      Config.save();

      server.send(
        200,
        "text/plain",
        "Factory reset");

      delay(300);

      ESP.restart();
    });

  server.onNotFound(
    [this]()
    {
      handleNotFound();
    });

  server.begin();
}

//------------------------------------------------------------

void WebManager::handleRoot() {
    File file =
        SPIFFS.open("/index.html","r");

    if(!file)
    {
        server.send(
            404,
            "text/plain",
            "index.html not found");

        return;
    }

    server.streamFile(
        file,
        "text/html");

    file.close();
}

//------------------------------------------------------------

void WebManager::handleStyle() {
  File file = SPIFFS.open("/style.css", "r");

  if (!file) {
    server.send(404, "text/plain", "style.css not found");
    return;
  }

  server.streamFile(file, "text/css");

  file.close();
}

//------------------------------------------------------------

void WebManager::handleScript() {
  File file = SPIFFS.open("/script.js", "r");

  if (!file) {
    server.send(404, "text/plain", "script.js not found");
    return;
  }

  server.streamFile(file, "application/javascript");

  file.close();
}

//------------------------------------------------------------

void WebManager::handleGetConfig() {
    JsonDocument doc;

    //--------------------------------------------------
    // Device
    //--------------------------------------------------

    doc["device"]["deviceName"] =
        Config.device.deviceName;

    //--------------------------------------------------
    // Network
    //--------------------------------------------------

    doc["network"]["ssid"] =
        Config.network.ssid;

    doc["network"]["password"] =
        Config.network.password;

    doc["network"]["dhcp"] =
        Config.network.dhcp;

    doc["network"]["ip"] =
        Config.network.ip;

    doc["network"]["gateway"] =
        Config.network.gateway;

    doc["network"]["subnet"] =
        Config.network.subnet;

    doc["network"]["dns1"] =
        Config.network.dns1;

    doc["network"]["dns2"] =
        Config.network.dns2;

    //--------------------------------------------------
    // Injector
    //--------------------------------------------------

    doc["injector"]["mode"] =
        modeName(Config.injector.mode);

    doc["injector"]["pulseMs"] =
        Config.injector.pulseWidthUs / 1000.0;

    doc["injector"]["periodMs"] =
        Config.injector.periodUs / 1000.0;

    doc["injector"]["pulses"] =
        Config.injector.pulsesPerBurst;

    doc["injector"]["burstPeriodMs"] =
        Config.injector.burstPeriodUs / 1000.0;

    doc["injector"]["holdTimeoutSec"] =
        Config.injector.holdTimeoutSec;

    doc["injector"]["autoStopSec"] =
        Config.injector.autoStopSec;

    doc["injector"]["invertOutput"] =
        Config.injector.invertOutput;

    //--------------------------------------------------
    // Serialize JSON
    //--------------------------------------------------

    String json;

    serializeJson(doc, json);

    server.send(
        200,
        "application/json",
        json);
}

//------------------------------------------------------------

void WebManager::handlePostConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No JSON");
        return;
    }

    JsonDocument doc;

    DeserializationError err =
        deserializeJson(doc, server.arg("plain"));

    if (err) {
        server.send(400, "text/plain", "JSON parse error");
        return;
    }

    bool restartNeeded = false;

    //--------------------------------------------------
    // Device
    //--------------------------------------------------

    if (doc["device"].is<JsonObject>())
    {
        JsonObject d = doc["device"];

        if (d["deviceName"].is<const char*>())
            Config.device.deviceName = d["deviceName"].as<String>();

        restartNeeded = true;
    }

    //--------------------------------------------------
    // Network
    //--------------------------------------------------

    if (doc["network"].is<JsonObject>())
    {
        JsonObject n = doc["network"];

        if (n["ssid"].is<const char*>())
            Config.network.ssid = n["ssid"].as<String>();

        if (n["password"].is<const char*>())
            Config.network.password = n["password"].as<String>();

        if (n["dhcp"].is<bool>())
            Config.network.dhcp = n["dhcp"];

        if (n["ip"].is<const char*>())
            Config.network.ip = n["ip"].as<String>();

        if (n["gateway"].is<const char*>())
            Config.network.gateway = n["gateway"].as<String>();

        if (n["subnet"].is<const char*>())
            Config.network.subnet = n["subnet"].as<String>();

        if (n["dns1"].is<const char*>())
            Config.network.dns1 = n["dns1"].as<String>();

        if (n["dns2"].is<const char*>())
            Config.network.dns2 = n["dns2"].as<String>();

        restartNeeded = true;
    }

    //--------------------------------------------------
    // Injector (типові параметри)
    //--------------------------------------------------

    if (doc["injector"].is<JsonObject>())
    {
        JsonObject i = doc["injector"];

        if (i["mode"].is<const char*>())
            Config.injector.mode =
                modeFromName(i["mode"].as<String>());

        if (i["pulseMs"].is<float>())
            Config.injector.pulseWidthUs =
                clampU32(i["pulseMs"], 0.1f, 1000.0f) * 1000UL;

        if (i["periodMs"].is<float>())
            Config.injector.periodUs =
                clampU32(i["periodMs"], 0.2f, 60000.0f) * 1000UL;

        if (i["pulses"].is<uint32_t>())
            Config.injector.pulsesPerBurst =
                min((uint32_t)i["pulses"], (uint32_t)65535);

        if (i["burstPeriodMs"].is<float>())
            Config.injector.burstPeriodUs =
                clampU32(i["burstPeriodMs"], 0.0f, 3600000.0f) * 1000UL;

        if (i["holdTimeoutSec"].is<uint32_t>())
            Config.injector.holdTimeoutSec =
                min((uint32_t)i["holdTimeoutSec"], (uint32_t)3600);

        if (i["autoStopSec"].is<uint32_t>())
            Config.injector.autoStopSec =
                min((uint32_t)i["autoStopSec"], (uint32_t)86400);

        if (i["invertOutput"].is<bool>())
            Config.injector.invertOutput = i["invertOutput"];
    }

    //--------------------------------------------------
    // Зберігаємо (безпечна зупинка перед записом NVS)
    //--------------------------------------------------

    Injector.stop();

    Config.save();

    Serial.println("Configuration saved");

    if (doc["network"].is<JsonObject>())
        Serial.printf("Network: SSID='%s' DHCP=%d IP=%s\n",
                      Config.network.ssid.c_str(),
                      Config.network.dhcp,
                      Config.network.ip.c_str());

    if (restartNeeded)
    {
        server.send(
            200,
            "application/json",
            "{\"restart\":true}");

        delay(300);

        ESP.restart();
    }
    else
    {
        server.send(
            200,
            "application/json",
            "{\"restart\":false}");
    }
}

//------------------------------------------------------------

void WebManager::handleStatus() {
    JsonDocument doc;

    bool running = Injector.isRunning();

    doc["running"] = running;
    doc["state"] = stateName(Injector.state());
    doc["mode"] = modeName(Injector.mode());
    doc["pulses"] = Injector.pulsesTotal();
    doc["bursts"] = Injector.burstsTotal();
    doc["elapsedMs"] = Injector.elapsedMs();

    //--------------------------------------------------
    // Активні параметри
    //--------------------------------------------------

    InjectorRuntime p = Injector.params();

    JsonObject pr = doc["params"].to<JsonObject>();

    pr["mode"] = modeName(p.mode);
    pr["pulseMs"] = p.pulseWidthUs / 1000.0;
    pr["periodMs"] = p.periodUs / 1000.0;
    pr["pulses"] = p.pulsesPerBurst;
    pr["burstPeriodMs"] = p.burstPeriodUs / 1000.0;

    //--------------------------------------------------
    // WiFi
    //--------------------------------------------------

    JsonObject w = doc["wifi"].to<JsonObject>();

    w["mode"] = apMode ? "AP" : "STA";
    w["connected"] = isConnected();
    w["ip"] = localIP().toString();
    w["ssid"] = apMode ? apName : Config.network.ssid;
    w["rssi"] = apMode ? 0 : WiFi.RSSI();

    //--------------------------------------------------

    String json;

    serializeJson(doc, json);

    server.send(
        200,
        "application/json",
        json);
}

//------------------------------------------------------------

void WebManager::handleStart() {
    if (!server.hasArg("plain")) {
        server.send(
            400,
            "application/json",
            "{\"error\":\"No JSON\"}");
        return;
    }

    JsonDocument doc;

    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(
            400,
            "application/json",
            "{\"error\":\"JSON parse error\"}");
        return;
    }

    //--------------------------------------------------
    // Режим
    //--------------------------------------------------

    uint8_t mode = MODE_BURST;

    if (doc["mode"].is<const char*>())
        mode = modeFromName(doc["mode"].as<String>());
    else if (doc["mode"].is<uint8_t>())
        mode = doc["mode"];

    //--------------------------------------------------
    // Параметри (за замовчуванням - з конфігурації)
    //--------------------------------------------------

    float pulseMs = Config.injector.pulseWidthUs / 1000.0;
    float periodMs = Config.injector.periodUs / 1000.0;
    float burstPeriodMs = Config.injector.burstPeriodUs / 1000.0;
    uint32_t pulses = Config.injector.pulsesPerBurst;

    if (doc["pulseMs"].is<float>())
        pulseMs = doc["pulseMs"];

    if (doc["periodMs"].is<float>())
        periodMs = doc["periodMs"];

    if (doc["pulses"].is<uint32_t>())
        pulses = doc["pulses"];

    if (doc["burstPeriodMs"].is<float>())
        burstPeriodMs = doc["burstPeriodMs"];

    //--------------------------------------------------
    // Валідація / клампінг
    //--------------------------------------------------

    if (pulseMs < 0.1f) pulseMs = 0.1f;
    if (pulseMs > 1000.0f) pulseMs = 1000.0f;

    if (mode == MODE_BURST || mode == MODE_CONTINUOUS) {
        if (periodMs < pulseMs + 0.05f)
            periodMs = pulseMs + 0.05f;

        if (periodMs > 60000.0f)
            periodMs = 60000.0f;
    } else {
        // для SINGLE / HOLD період не використовується
        periodMs = pulseMs + 1.0f;
    }

    if (mode == MODE_BURST) {
        if (pulses > 65535UL)
            pulses = 65535;

        if (burstPeriodMs > 3600000.0f)
            burstPeriodMs = 3600000.0f;

        // якщо період пакета коротший за сам пакет - пауза не потрібна
        double burstMs = (double)pulses * periodMs;

        if (pulses > 0 &&
            burstPeriodMs > 0 &&
            burstPeriodMs < burstMs)
            burstPeriodMs = 0;
    } else {
        burstPeriodMs = 0;
    }

    //--------------------------------------------------
    // Запуск
    //--------------------------------------------------

    InjectorRuntime p;

    p.mode = mode;
    p.pulseWidthUs = clampU32(pulseMs, 0.1f, 1000.0f) * 1000UL;
    p.periodUs = (uint32_t)(periodMs * 1000.0f + 0.5f);
    p.pulsesPerBurst = pulses;
    p.burstPeriodUs = clampU32(burstPeriodMs, 0.0f, 3600000.0f) * 1000UL;

    if (!Injector.start(p)) {
        server.send(
            400,
            "application/json",
            "{\"error\":\"Invalid parameters\"}");
        return;
    }

    JsonDocument r;

    r["started"] = true;
    r["mode"] = modeName(mode);
    r["pulseMs"] = p.pulseWidthUs / 1000.0;
    r["periodMs"] = p.periodUs / 1000.0;
    r["pulses"] = p.pulsesPerBurst;
    r["burstPeriodMs"] = p.burstPeriodUs / 1000.0;

    String json;

    serializeJson(r, json);

    server.send(
        200,
        "application/json",
        json);
}

//------------------------------------------------------------

void WebManager::handleStop() {
    Injector.stop();

    Serial.println("Stopped by web");

    server.send(
        200,
        "application/json",
        "{\"stopped\":true}");
}

//------------------------------------------------------------

void WebManager::handleRestart() {
  server.send(200, "text/plain", "Restarting");

  delay(500);

  ESP.restart();
}

//------------------------------------------------------------

void WebManager::handleFactory() {

  Config.reset();

  server.send(200, "text/plain", "Factory Reset");

  delay(500);

  ESP.restart();
}

//------------------------------------------------------------

void WebManager::handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}
