/*
    Power functions
*/

#include "power.h"
#include "core/worker.h"
#include "core/btn.h"
#include "core/display.h"
#include "core/indicator.h"
#include "core/log.h"

#include "esp_sleep.h"
#include "esp32-hal.h"
#include "esp32-hal-gpio.h"

void pwrinit();
void pwrterm();

static void pwroff() {
    CONSOLE("goto off");
    // ждём, пока будет отпущена кнопка
    while (digitalRead(PWRBTN) == LOW) delay(100);
    esp_sleep_enable_ext0_wakeup(PWRBTN, LOW); //1 = High, 0 = Low
    CONSOLE("Going to deep sleep now");
    esp_deep_sleep_start();
    CONSOLE("This will never be printed");
}

/* ------------------------------------------------------------------------------------------- *
 *  процессинг
 * ------------------------------------------------------------------------------------------- */
static void _reset();
class _powerWrk : public Wrk {
    const uint8_t _tm[3] = { 50, 30, 60 };
    int8_t _ti = 0;
    uint16_t _c = 0;
    bool _l = false;

#if HWVER < 2
    Indicator _ind_fin = Indicator(
        [this](uint16_t){ return ison ? false : abs(_ti) % 10 < 5; },
        [this](uint16_t){ return ison ? abs(_ti) % 10 < 5 : false; }
    );
    const Indicator _ind = Indicator(
        [this](uint16_t){ return _l; },
        [this](uint16_t){ return true; }
    );
#endif

    const Btn _b = Btn([this](){ click(); });

    Display _dspl = Display(
        [this](U8G2 &u8g2) {
            char s[30];

            u8g2.setFont(u8g2_font_ImpactBits_tr);
            if (ison)
                SCPY("pwr: ON");
            else
                SCPY("pwr: OFF");
            u8g2.drawStr(SCENT, 13, s);

            if (_l) {
                u8g2.setFont(u8g2_font_bubble_tr);
                SCPY("PUSH");
                u8g2.drawStr(SCENT, 50, s);
            }
        }, true
    );
    Display _dspl_fin = Display(
        [this](U8G2 &u8g2) {
            char s[30];

            u8g2.setFont(u8g2_font_bubble_tr);
            if (ison)
                SCPY("HELLO");
            else
                SCPY("GOODBY");
            u8g2.drawStr(SCENT, 40, s);
        }, true, false
    );

    void click() {
        CONSOLE("click: i=%d, c=%d, l=%d", _ti, _c, _l);
        if (_ti < 0) return;
        if (!_l) {
            _ti = -100;
            return;
        }

        _ti++;
        _c = 0;

        if (_ti >= sizeof(_tm)/sizeof(_tm[0])) {
            // _ti >= 0: индекс для массива _tm[]
            // _ti < 0: ожидание завершения процесса
            _ti = -1;
            _dspl_fin.show();
#if HWVER < 2
            _ind_fin.activate();
#endif
        }
    }


public:
    const bool ison;
    _powerWrk(bool pwron) :
        ison(pwron)
    {
    }
    ~_powerWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    state_t run() {
        _l = (_ti >= 0) && (_ti < sizeof(_tm)) && (_c > _tm[_ti]) && (_c < _tm[_ti]+40);
        if (_ti < 0) {
            // отрицательные значения _ti
            // означают ожидание завершения процесса
            _ti --;
            return _ti < -45 ? END : DLY;
        }
        
        _c ++;

        if ((_c > (_tm[_ti] + 1)) && !_l)
            _ti = -100;

        return DLY;
    }

    void end() {
        _reset();
        CONSOLE("ison=%d", ison);

        if (_ti <= -100) {
            CONSOLE("fail");
            if (ison) pwroff();
            return;
        }

        if (ison)
            pwrinit();
        else {
            pwrterm();
            pwroff();
        }
    }
};
static WrkProc<_powerWrk> _pwr;

/* ------------------------------------------------------------------------------------------- *
 *  индикатор заряда батарей
 * ------------------------------------------------------------------------------------------- */

#if HWVER >= 2
class _battWrk : public Wrk {
    static double _fmap(uint32_t in, uint32_t in_min, uint32_t in_max, double out_min, double out_max) {
        return  (out_max - out_min) * (in - in_min) / (in_max - in_min) + out_min;
    }
    static uint8_t _sym(uint8_t pin) {
        //return 3.35 * pwrBattRaw() / 4095 * 3 / 2;
        uint16_t raw = analogRead(pin);
        //auto v = _fmap(raw, 0x0000, 0x0fff, 0.16, 3.35) * 4 / 3;
        //CONSOLE("pin: %d, v: %0.2f", pin, v);

        return
            raw > 3880 ? 5 :
            raw > 3670 ? 4 :
            raw > 3550 ? 3 :
            raw > 3430 ? 2 :
            raw > 3310 ? 1 :
            0;
    }

    Display _dspl = Display(
        [](U8G2 &u8g2) {
            u8g2.setFont(u8g2_font_battery19_tn);
            u8g2.setFontDirection(1);
            u8g2.drawGlyph(35,  0, '0' + _sym(PINBATTCD));
            u8g2.drawGlyph(35,  7, '0' + _sym(PINBATTAB));
            u8g2.setFontDirection(0);
        }
    );
public:
    _battWrk() {
        pinMode(PINBATTAB, INPUT);
        pinMode(PINBATTCD, INPUT);
    }
    state_t run() { return DLY; }
};
static WrkProc<_battWrk> _batt;
#endif

/* ------------------------------------------------------------------------------------------- *
 *  Запуск / остановка
 * ------------------------------------------------------------------------------------------- */


void pwr::hwon() {
#if HWVER >= 2
    pinMode(PINHWEN, OUTPUT);
    digitalWrite(PINHWEN, LOW);
#endif
}

void pwr::off() {
    start(false);
}

bool pwr::start(bool pwron) {
    if (_pwr.isrun())
        return false;
    
    _pwr = wrkRun<_powerWrk>(pwron);
    
    return true;
}

bool pwr::stop() {
    if (!_pwr.isrun())
        return false;

    _pwr.term();

    return true;
}

static void _reset() {
    _pwr.reset();
}

void pwr::batt() {
#if HWVER >= 2
    if (!_pwr.isrun())
        _batt = wrkRun<_battWrk>();
#endif
}
