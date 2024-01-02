
#include "jump.h"
#include "worker.h"
#include "clock.h"
#include "btn.h"
#include "indicator.h"
#include "log.h"
#include "ledwork.h"
#include "power.h"

#include "altcalc.h"
#include <Adafruit_BMP280.h>

class _jmpWrk : public Wrk {
    Adafruit_BMP280 bmp = Adafruit_BMP280(PINBMP);
    AltCalc ac;
    ac_jmpmode_t _mode = ac.jmpmode();
    bool ok = false;

    Btn _b = Btn(
        [](){ powerStart(false); },
        [this]() { clicklng(); }
    );
    Indicator _ind_toff = Indicator(
        [this](uint16_t t) { return (t < 3) || ((t > 7) && (t < 10)); },
        [](uint16_t) { return true; },
        5000
    );
    Indicator _ind_down = Indicator(
        [this](uint16_t t) { return true; },
        [](uint16_t) { return true; }
    );
    Indicator _ind_idle = Indicator(
        [this](uint16_t t) { return t < 3; },
        NULL,
        5000
    );

    void clicklng() {
        ledTgl();
    }

    void mode(ac_jmpmode_t prv, ac_jmpmode_t cur) {
        CONSOLE("jmpmode %d -> %d (%d) %d", prv, cur, ac.jmpcnt(), ac.alt());
        switch (cur) {
            case ACJMP_TAKEOFF:
                _ind_toff.activate();
                break;
            
            case ACJMP_FREEFALL:
                _ind_down.activate();
                break;
            
            case ACJMP_CANOPY:
                _ind_down.activate();
                break;
                
            case ACJMP_NONE:
                _ind_idle.activate();
                break;
        }
    }

public:
    _jmpWrk() {
        if (!(ok = bmp.begin())) {   
            CONSOLE("Could not find a valid BMP280 sensor, check wiring!");
        }
    }
#ifdef FWVER_DEBUG
    ~_jmpWrk() {
        CONSOLE("(0x%08x) destroy", this);
        bmp.setSampling(Adafruit_BMP280::MODE_SLEEP);
    }
#endif

    state_t run() {
        if (!ok)
            return END;
        
        static uint32_t tck;
        
        uint32_t interval = utm_diff32(tck, tck) / 1000;
        ac.tick(bmp.readPressure(), interval);

        if (_mode != ac.jmpmode())
            mode(_mode, ac.jmpmode());
        _mode = ac.jmpmode();

        return DLY;
    }
};

static WrkProc<_jmpWrk> _jmp;

bool jumpStart() {
    if (_jmp.isrun())
        return false;
    
    _jmp = wrkRun<_jmpWrk>();

    return false;
}

bool jumpStop() {
    if (!_jmp.isrun())
        return false;
    
    _jmp.term();

    return false;
}
