#pragma once

#include <Arduino.h>
#include <Preferences.h>

//======================================================
// Device
//======================================================

struct DeviceConfig
{
    String deviceName;
};

//======================================================
// Network
//======================================================

struct NetworkConfig
{
    String ssid;
    String password;

    bool dhcp;

    String ip;
    String gateway;
    String subnet;

    String dns1;
    String dns2;
};

//======================================================
// Injector
//======================================================

enum InjectorMode : uint8_t
{
    MODE_SINGLE = 0,     // одиничний імпульс
    MODE_BURST = 1,      // пакет імпульсів з паузою між пакетами
    MODE_CONTINUOUS = 2, // безперервна послідовність до зупинки
    MODE_HOLD = 3        // постійно відкрита форсунка (промивка)
};

struct InjectorConfig
{
    uint8_t mode;

    uint32_t pulseWidthUs;  // довжина імпульсу, мкс
    uint32_t periodUs;      // період імпульсів, мкс

    uint32_t pulsesPerBurst; // імпульсів у пакеті (0 = без обмежень)
    uint32_t burstPeriodUs;  // період пакета, мкс (0 = без паузи)

    uint32_t holdTimeoutSec; // аварійне відключення режиму HOLD (0 = без обмежень)
    uint32_t autoStopSec;    // глобальний автостоп тесту (0 = вимкнено)

    bool invertOutput;       // інверсія вихідного сигналу (опторозв'язка з інверсією)
};

//======================================================
// ConfigManager
//======================================================

class ConfigManager
{
public:

    DeviceConfig device;
    NetworkConfig network;
    InjectorConfig injector;

    bool begin();

    bool load();

    bool save();

    void reset();

    void loadDefaults();

private:

    Preferences prefs;
};

//======================================================

extern ConfigManager Config;
