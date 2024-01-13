/*
    Power functions
*/

#include "power.h"
#include "core/worker.h"
#include "core/btn.h"
#include "core/indicator.h"
#include "core/log.h"

#include "esp_sleep.h"
#include "esp32-hal.h"
#include "esp32-hal-gpio.h"

void pwrinit();

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
    const Indicator _ind = Indicator(
        [this](uint16_t){ return _l = (_ti >= 0) && (_ti < sizeof(_tm)) && (_c > _tm[_ti]) && (_c < _tm[_ti]+40); },
        [this](uint16_t){ return (_ti >= 0) || ((_ti % 8) > -4); }
    );
    const Btn _b = Btn([this](){ click(); });

    void click() {
        CONSOLE("click: i=%d, c=%d, l=%d", _ti, _c, _l);
        if (_ti < 0) return;
        if (!_l) {
            _ti = -100;
            return;
        }

        _ti++;
        _c = 0;
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
        if (_ti < 0) {
            // отрицательные значения _ti
            // означают ожидание завершения процесса
            _ti --;
            return _ti < -60 ? END : DLY;
        }

        if (_ti >= sizeof(_tm)/sizeof(_tm[0]))
            // положительные значения _ti
            // означают индекс для массива _tm[]
            return END;
        
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
        else
            pwroff();
    }
};
static WrkProc<_powerWrk> _pwr;

/* ------------------------------------------------------------------------------------------- *
 *  Запуск / остановка
 * ------------------------------------------------------------------------------------------- */

void powerOff() {
    powerStart(false);
}

bool powerStart(bool pwron)
{
    if (_pwr.isrun())
        return false;
    
    _pwr = wrkRun<_powerWrk>(pwron);
    
    return true;
}

bool powerStop() {
    if (!_pwr.isrun())
        return false;

    _pwr.term();

    return true;
}

static void _reset() {
    _pwr.reset();
}
