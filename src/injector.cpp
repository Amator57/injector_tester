#include "injector.h"

//======================================================
// Апаратний таймер (1 МГц, 64 біт)
//======================================================

static hw_timer_t *timer = nullptr;
static portMUX_TYPE injMux = portMUX_INITIALIZER_UNLOCKED;

//======================================================
// Змінні, доступні з ISR
//======================================================

static volatile uint8_t v_state = ST_IDLE;
static volatile bool v_running = false;
static volatile bool v_invert = false;

static volatile uint8_t v_mode = MODE_BURST;
static volatile uint32_t v_pulseUs = 3000;
static volatile uint32_t v_periodUs = 15000;
static volatile uint32_t v_pulsesPerBurst = 50;
static volatile uint32_t v_burstPeriodUs = 1000000;

static volatile uint32_t v_pulseIndex = 0;
static volatile uint32_t v_pulsesTotal = 0;
static volatile uint32_t v_burstsTotal = 0;

static volatile uint32_t v_startMs = 0;
static volatile uint32_t v_stopMs = 0;

//======================================================
// Швидке керування пінами (прямі регістри, для ISR)
//======================================================

static inline void IRAM_ATTR rawHigh(uint8_t pin) { digitalWrite(pin, HIGH); }

static inline void IRAM_ATTR rawLow(uint8_t pin) { digitalWrite(pin, LOW); }

//------------------------------------------------------

static inline void IRAM_ATTR outWrite(bool on) {
  //--------------------------------------------------
  // Вихід на опторозв'язку
  //--------------------------------------------------

  if (v_invert) {
    if (on)
      rawLow(INJECTOR_PIN_OUT);
    else
      rawHigh(INJECTOR_PIN_OUT);
  } else {
    if (on)
      rawHigh(INJECTOR_PIN_OUT);
    else
      rawLow(INJECTOR_PIN_OUT);
  }

  //--------------------------------------------------
  // LED (активний LOW)
  //--------------------------------------------------

  if (on)
    rawLow(INJECTOR_PIN_LED);
  else
    rawHigh(INJECTOR_PIN_LED);
}

//------------------------------------------------------

static inline void IRAM_ATTR setAlarm(uint64_t us) {
  timerWrite(timer, 0);
  timerStart(timer);
  timerAlarm(timer, us, true, 0);
}

static inline void IRAM_ATTR stopAlarm() {
  timerStop(timer);
  timerWrite(timer, 0);
}

//======================================================
// ISR генератора імпульсів
//======================================================

static void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&injMux);

  switch (v_state) {

    case ST_PULSE: {
      outWrite(false);

      v_pulsesTotal++;
      v_pulseIndex++;

      if (v_mode == MODE_SINGLE) {
        v_running = false;
        v_stopMs = millis();
        v_state = ST_DONE;
        stopAlarm();
      } else if (v_mode == MODE_BURST &&
                 v_pulsesPerBurst > 0 &&
                 v_pulseIndex >= v_pulsesPerBurst) {
        //--------------------------------------------------
        // Пакет завершено
        //--------------------------------------------------

        v_pulseIndex = 0;
        v_burstsTotal++;

        uint64_t burstUs = (uint64_t)v_pulsesPerBurst * v_periodUs;
        uint64_t waitUs = 50;

        if ((uint64_t)v_burstPeriodUs > burstUs)
          waitUs = (uint64_t)v_burstPeriodUs - burstUs;

        if (waitUs < 50)
          waitUs = 50;

        v_state = ST_BURST_WAIT;
        setAlarm((uint32_t)waitUs);
      } else {
        //--------------------------------------------------
        // Пауза між імпульсами
        //--------------------------------------------------

        uint32_t offUs = 50;

        if (v_periodUs > v_pulseUs)
          offUs = v_periodUs - v_pulseUs;

        if (offUs < 50)
          offUs = 50;

        v_state = ST_GAP;
        setAlarm(offUs);
      }
      break;
    }

    case ST_GAP:
    case ST_BURST_WAIT:
      v_state = ST_PULSE;
      outWrite(true);
      setAlarm(v_pulseUs);
      break;

    case ST_HOLD:
      //--------------------------------------------------
      // Тайм-аут безпечного утримання
      //--------------------------------------------------

      outWrite(false);
      v_running = false;
      v_stopMs = millis();
      v_state = ST_DONE;
      stopAlarm();
      break;

    default:
      stopAlarm();
      break;
  }

  portEXIT_CRITICAL_ISR(&injMux);
}

//======================================================
// InjectorManager
//======================================================

InjectorManager Injector;

//------------------------------------------------------

void InjectorManager::begin() {
  v_invert = Config.injector.invertOutput;

  pinMode(INJECTOR_PIN_OUT, OUTPUT);
  pinMode(INJECTOR_PIN_LED, OUTPUT);

  applyIdle();

  timer = timerBegin(1000000); // 1 МГц, 1 тік = 1 мкс
  timerAttachInterrupt(timer, &onTimer);
}

//------------------------------------------------------

void InjectorManager::applyIdle() {
  outWrite(false);
}

//------------------------------------------------------

bool InjectorManager::start(const InjectorRuntime &p) {
  //--------------------------------------------------
  // Валідація
  //--------------------------------------------------

  if (p.pulseWidthUs < 100 || p.pulseWidthUs > 1000000UL)
    return false;

  if (p.mode == MODE_BURST || p.mode == MODE_CONTINUOUS) {
    if (p.periodUs < p.pulseWidthUs + 50)
      return false;

    if (p.periodUs > 60000000UL)
      return false;
  }

  if (p.mode == MODE_BURST && p.pulsesPerBurst > 65535UL)
    return false;

  stop();

  portENTER_CRITICAL(&injMux);

  v_invert = Config.injector.invertOutput;

  v_mode = p.mode;
  v_pulseUs = p.pulseWidthUs;
  v_periodUs = p.periodUs;
  v_pulsesPerBurst = p.pulsesPerBurst;
  v_burstPeriodUs = p.burstPeriodUs;

  v_pulseIndex = 0;
  v_pulsesTotal = 0;
  v_burstsTotal = 0;

  v_startMs = millis();
  v_stopMs = v_startMs;
  v_running = true;

  if (p.mode == MODE_HOLD) {
    v_state = ST_HOLD;
    outWrite(true);

    uint32_t holdUs = Config.injector.holdTimeoutSec * 1000000UL;

    if (holdUs > 0)
      setAlarm(holdUs);
    else
      stopAlarm();
  } else {
    v_state = ST_PULSE;
    outWrite(true);
    setAlarm(v_pulseUs);
  }

  portEXIT_CRITICAL(&injMux);

  Serial.println("Run:");

  Serial.print("  mode       : ");
  Serial.println(v_mode);

  Serial.print("  pulse      : ");
  Serial.print(v_pulseUs);
  Serial.println(" us");

  Serial.print("  period     : ");
  Serial.print(v_periodUs);
  Serial.println(" us");

  Serial.print("  pulses     : ");
  Serial.println(v_pulsesPerBurst);

  Serial.print("  burst per. : ");
  Serial.print(v_burstPeriodUs);
  Serial.println(" us");

  return true;
}

//------------------------------------------------------

void InjectorManager::stop() {
  portENTER_CRITICAL(&injMux);

  if (v_state != ST_IDLE) {
    outWrite(false);
    stopAlarm();

    v_running = false;
    v_stopMs = millis();
    v_state = ST_IDLE;
  }

  portEXIT_CRITICAL(&injMux);
}

//------------------------------------------------------

void InjectorManager::loop() {
  //--------------------------------------------------
  // Глобальний автостоп
  //--------------------------------------------------

  if (v_running &&
      Config.injector.autoStopSec > 0 &&
      (millis() - v_startMs) >= Config.injector.autoStopSec * 1000UL) {
    Serial.println("Auto stop");
    stop();
  }

  //--------------------------------------------------
  // Очищення стану "завершено" через 3 с
  //--------------------------------------------------

  if (!v_running &&
      v_state == ST_DONE &&
      (millis() - v_stopMs) > 3000) {
    portENTER_CRITICAL(&injMux);

    if (v_state == ST_DONE)
      v_state = ST_IDLE;

    portEXIT_CRITICAL(&injMux);
  }
}

//------------------------------------------------------

bool InjectorManager::isRunning() const { return v_running; }

uint8_t InjectorManager::state() const { return v_state; }

uint8_t InjectorManager::mode() const { return v_mode; }

uint32_t InjectorManager::pulsesTotal() const { return v_pulsesTotal; }

uint32_t InjectorManager::burstsTotal() const { return v_burstsTotal; }

uint32_t InjectorManager::elapsedMs() const {
  if (v_running)
    return millis() - v_startMs;

  return v_stopMs - v_startMs;
}

//------------------------------------------------------

InjectorRuntime InjectorManager::params() const {
  InjectorRuntime p;

  p.mode = v_mode;
  p.pulseWidthUs = v_pulseUs;
  p.periodUs = v_periodUs;
  p.pulsesPerBurst = v_pulsesPerBurst;
  p.burstPeriodUs = v_burstPeriodUs;

  return p;
}
