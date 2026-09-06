#include <Arduino.h>

#include "config.h"
#include "injector.h"
#include "web_manager.h"

//======================================================
// Індикація режиму AP (LED блимає, коли генератор
// зупинено і пристрій у режимі точки доступу)
//======================================================

static uint32_t ledTs = 0;
static bool ledBlink = false;

static void indicatorLoop() {
  if (!Injector.isRunning()) {
    if (Web.isAP()) {
      if (millis() - ledTs >= 600) {
        ledTs = millis();
        ledBlink = !ledBlink;
        digitalWrite(INJECTOR_PIN_LED, ledBlink ? LOW : HIGH);
      }
    } else if (ledBlink) {
      ledBlink = false;
      digitalWrite(INJECTOR_PIN_LED, HIGH);
    }
  }
}

//======================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==================================");
  Serial.println("Injector Tester / Cleaner  v1.0");
  Serial.println("==================================");

  //-------------------------------
  // Configuration
  //-------------------------------

  Config.begin();
  Config.load();

  //-------------------------------
  // Pulse engine (hardware timer)
  //-------------------------------

  Injector.begin();

  //-------------------------------
  // WiFi + Web
  //-------------------------------

  Web.begin();

  Serial.println();
  Serial.println("System started");
}

void loop() {
  Web.loop();

  Injector.loop();

  indicatorLoop();
}
