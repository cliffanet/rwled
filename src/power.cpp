/*
    Power functions
*/

#include "power.h"
#include "worker.h"
#include "btn.h"
#include "indicator.h"
#include "log.h"

#include "esp_sleep.h"

void pwrinit();

static void pwroff() {
    CONSOLE("goto off");
    esp_sleep_enable_ext0_wakeup(PWRBTN, 0); //1 = High, 0 = Low
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
        if (_ti < 3) {
            _c = 0;
            return;
        }

        _ti = -1;
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

        if (_ti >= sizeof(_tm))
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

bool powerStart(bool pwron) {
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
