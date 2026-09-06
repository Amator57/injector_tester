#include "config.h"

ConfigManager Config;

//------------------------------------------------------
// Ініціалізація Preferences
//------------------------------------------------------

bool ConfigManager::begin() { return prefs.begin("injtest", false); }

//------------------------------------------------------
// Значення за замовчуванням
//------------------------------------------------------

void ConfigManager::loadDefaults() {
  //--------------------------------------------------
  // Device
  //--------------------------------------------------

  device.deviceName = "InjectorTester";

  //--------------------------------------------------
  // Network
  //--------------------------------------------------

  network.ssid = "";
  network.password = "";

  network.dhcp = true;

  network.ip = "192.168.1.201";
  network.gateway = "192.168.1.1";
  network.subnet = "255.255.255.0";

  network.dns1 = "8.8.8.8";
  network.dns2 = "1.1.1.1";

  //--------------------------------------------------
  // Injector
  //--------------------------------------------------

  injector.mode = MODE_BURST;

  injector.pulseWidthUs = 3000;     // 3 мс
  injector.periodUs = 15000;        // 15 мс (~66.7 Гц)

  injector.pulsesPerBurst = 50;     // 50 імпульсів у пакеті
  injector.burstPeriodUs = 1000000; // 1 с

  injector.holdTimeoutSec = 60;
  injector.autoStopSec = 0;

  injector.invertOutput = false;
}

//------------------------------------------------------
// Завантаження конфігурації
//------------------------------------------------------

bool ConfigManager::load() {
  loadDefaults();

  //--------------------------------------------------
  // Device
  //--------------------------------------------------

  device.deviceName = prefs.getString("devname", device.deviceName);

  //--------------------------------------------------
  // Network
  //--------------------------------------------------

  network.ssid = prefs.getString("ssid", network.ssid);

  network.password = prefs.getString("password", network.password);

  network.dhcp = prefs.getBool("dhcp", network.dhcp);

  network.ip = prefs.getString("ip", network.ip);
  network.gateway = prefs.getString("gateway", network.gateway);
  network.subnet = prefs.getString("subnet", network.subnet);

  network.dns1 = prefs.getString("dns1", network.dns1);
  network.dns2 = prefs.getString("dns2", network.dns2);

  //--------------------------------------------------
  // Injector
  //--------------------------------------------------

  injector.mode = prefs.getUChar("mode", injector.mode);

  injector.pulseWidthUs = prefs.getUInt("pulseUs", injector.pulseWidthUs);
  injector.periodUs = prefs.getUInt("periodUs", injector.periodUs);

  injector.pulsesPerBurst = prefs.getUInt("pulses", injector.pulsesPerBurst);
  injector.burstPeriodUs = prefs.getUInt("burstUs", injector.burstPeriodUs);

  injector.holdTimeoutSec = prefs.getUInt("holdTo", injector.holdTimeoutSec);
  injector.autoStopSec = prefs.getUInt("autoStop", injector.autoStopSec);

  injector.invertOutput = prefs.getBool("invert", injector.invertOutput);

  return true;
}

//------------------------------------------------------
// Збереження конфігурації
//------------------------------------------------------

bool ConfigManager::save() {
  //--------------------------------------------------
  // Device
  //--------------------------------------------------

  prefs.putString("devname", device.deviceName);

  //--------------------------------------------------
  // Network
  //--------------------------------------------------

  prefs.putString("ssid", network.ssid);
  prefs.putString("password", network.password);

  prefs.putBool("dhcp", network.dhcp);

  prefs.putString("ip", network.ip);
  prefs.putString("gateway", network.gateway);
  prefs.putString("subnet", network.subnet);

  prefs.putString("dns1", network.dns1);
  prefs.putString("dns2", network.dns2);

  //--------------------------------------------------
  // Injector
  //--------------------------------------------------

  prefs.putUChar("mode", injector.mode);

  prefs.putUInt("pulseUs", injector.pulseWidthUs);
  prefs.putUInt("periodUs", injector.periodUs);

  prefs.putUInt("pulses", injector.pulsesPerBurst);
  prefs.putUInt("burstUs", injector.burstPeriodUs);

  prefs.putUInt("holdTo", injector.holdTimeoutSec);
  prefs.putUInt("autoStop", injector.autoStopSec);

  prefs.putBool("invert", injector.invertOutput);

  return true;
}

//------------------------------------------------------
// Скидання до заводських налаштувань
//------------------------------------------------------

void ConfigManager::reset() {
  prefs.clear();

  loadDefaults();

  save();

  Serial.println("Factory defaults restored");
}
