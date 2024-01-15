
#include "jump.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/btn.h"
#include "core/indicator.h"
#include "core/log.h"
#include "ledwork.h"
#include "led/work.h"
#include "power.h"

#include "altcalc.h"
#include <Adafruit_BMP280.h>

class _jmpWrk : public Wrk {
    Adafruit_BMP280 bmp = Adafruit_BMP280(PINBMP);
    AltCalc ac;
    AltJmp jmp;
    uint64_t tck;
    int64_t tm = tmill();
    bool ok, iscnp = false;

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

    void mode(AltJmp::mode_t prv) {
        CONSOLE("jmpmode %d -> %d (%d) %0.1f", prv, jmp.mode(), jmp.cnt(), ac.alt());
        switch (jmp.mode()) {
            case AltJmp::TAKEOFF:
                _ind_toff.activate();
                break;
            
            case AltJmp::FREEFALL:
                ledByJump(LED_FF);
                _ind_down.activate();
                iscnp = false;
                break;
            
            case AltJmp::CANOPY:
                ledByJump(LED_CNP);
                _ind_down.activate();
                break;
                
            case AltJmp::GROUND:
                ledByJump(LED_GND);
                _ind_idle.activate();
                iscnp = false;
                break;
        }
    }

public:
    _jmpWrk() {
        if ((ok = bmp.begin())) {
            float p = bmp.readPressure();
            CONSOLE("BMP OK! %0.1fpa", p);
        }
        else
            CONSOLE("Could not find a valid BMP280 sensor, check wiring!");
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
        
        if (tmill()-tm <= 90) return DLY;
        tm = tmill();

        auto interval = utm_diff(tck, tck) / 1000;
        //CONSOLE("interval: %lld", interval);
        ac.tick(bmp.readPressure(), interval);
        //CONSOLE("full: %d", ac.interval());
        auto m = jmp.mode();
        jmp.tick(ac);

        if (m != jmp.mode())
            mode(m);
        
        if (!iscnp && (m == AltJmp::FREEFALL) && (ac.avg().alt() < 1500)) {
            iscnp = true;
            ledByJump(LED_CNP);
        }

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
