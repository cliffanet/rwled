
#include "jump.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/btn.h"
#include "core/indicator.h"
#include "core/log.h"
#include "ledwork.h"
#include "ledtest.h"
#include "power.h"

#include "altcalc.h"
#include <Adafruit_BMP280.h>

class _jmpWrk : public Wrk {
    Adafruit_BMP280 bmp = Adafruit_BMP280(PINBMP);
    AltCalc ac;
    AltJmp jmp;
    uint64_t tck;
    int64_t tm = tmill();
    bool ok;

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
                ledByJump();
                _ind_down.activate();
                break;
            
            case AltJmp::CANOPY:
                ledByJump();
                _ind_down.activate();
                break;
                
            case AltJmp::GROUND:
                _ind_idle.activate();
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

        return DLY;
    }

    bool iscnp() {
        return
            (jmp.mode() == AltJmp::CANOPY) && (
                (ac.alt() < 1000) ||
                (
                    (ac.avg().speed() < -2) &&
                    (ac.avg().speed() > -35)
                )
            );
    }

    bool isalt() {
        return jmp.mode() > AltJmp::GROUND;
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

bool jumpIsCnp() {
    return _jmp.isrun() && _jmp->iscnp();
}

bool jumpIsAlt() {
    return _jmp.isrun() && _jmp->isalt();
}
