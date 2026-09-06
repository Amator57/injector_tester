#pragma once

#include <Arduino.h>

#include "config.h"

//======================================================
// Піни
//======================================================

#define INJECTOR_PIN_OUT 5 // вихід на опторозв'язку (GPIO5)
#define INJECTOR_PIN_LED 8 // вбудований LED (активний LOW)

//======================================================
// Стан генератора
//======================================================

enum InjectorState : uint8_t
{
    ST_IDLE = 0,      // зупинено
    ST_PULSE = 1,     // імпульс (форсунка відкрита)
    ST_GAP = 2,       // пауза між імпульсами
    ST_BURST_WAIT = 3,// пауза між пакетами
    ST_HOLD = 4,      // постійно відкрито
    ST_DONE = 5       // тест завершено
};

//======================================================
// Параметри запуску
//======================================================

struct InjectorRuntime
{
    uint8_t mode;

    uint32_t pulseWidthUs;
    uint32_t periodUs;
    uint32_t pulsesPerBurst;
    uint32_t burstPeriodUs;
};

//======================================================
// InjectorManager
//======================================================

class InjectorManager
{
public:

    void begin();

    bool start(const InjectorRuntime &params);

    void stop();

    void loop();

    bool isRunning() const;

    uint8_t state() const;
    uint8_t mode() const;

    uint32_t pulsesTotal() const;
    uint32_t burstsTotal() const;

    uint32_t elapsedMs() const;

    InjectorRuntime params() const;

private:

    void applyIdle();
};

//======================================================

extern InjectorManager Injector;
