
#include "indicator.h"
#include "worker.h"
#include "clock.h"
#include "bitstring.h"
#include "log.h"

#include <list>
#include "esp32-hal-gpio.h"

class _indWrk : public Wrk {
    std::list<Indicator *> _hnd;

    static void lght(uint8_t i, uint8_t pin, bool v) {
        static uint8_t p = 0;
        auto vc = bit_test(&p, i);
        if (v && !vc) {
            digitalWrite(pin, HIGH);
            bit_set(&p, i);
        }
        else
        if (!v && vc) {
            digitalWrite(pin, LOW);
            bit_clear(&p, i);
        }
    }

public:
    _indWrk() {
        pinMode(PINRED, OUTPUT);
        pinMode(PINGRN, OUTPUT);
    }
    ~_indWrk() {
        CONSOLE("(0x%08x) destroy", this);
        digitalWrite(PINRED, LOW);
        digitalWrite(PINGRN, LOW);
    }

    bool empty() const {
        return _hnd.empty();
    }

    void add(Indicator *ind) {
        CONSOLE("[%d]: 0x%08x", _hnd.size(), ind);
        _hnd.push_back(ind);
    }
    bool del(Indicator *ind) {
        CONSOLE("[%d]: 0x%08x", _hnd.size(), ind);
        for (auto p = _hnd.begin(); p != _hnd.end(); p++)
            if (*p == ind) {
                CONSOLE("found");
                _hnd.erase(p);
                return true;
            }

        return false;
    }

    state_t run() {
        if (_hnd.empty())
            return END;
        auto &h = _hnd.back();

        uint16_t t = ((tmill() - h->tm) % h->interval) / 100;

        lght(1, PINRED, h->red != NULL ? h->red(t) : false);
        lght(2, PINGRN, h->grn != NULL ? h->grn(t) : false);

        return DLY;
    }
};

static WrkProc<_indWrk> _ind;

Indicator::Indicator(hnd_t red, hnd_t grn, uint16_t interval) :
    red(red),
    grn(grn),
    tm(tmill()),
    interval(interval)
{
    if (!_ind.isrun())
        _ind = wrkRun<_indWrk>();
    _ind->add(this);
}

Indicator::~Indicator() {
    if (!_ind.isrun())
        return;
    _ind->del(this);
    if (_ind->empty())
        _ind.term();
}
