
#include "jump.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/btn.h"
#include "core/display.h"
#include "core/indicator.h"
#include "core/log.h"
#include "led/work.h"
#include "power.h"

#include "altcalc.h"
#include <Adafruit_BMP280.h>

class _jmpWrk : public Wrk {
    Adafruit_BMP280 bmp = Adafruit_BMP280(PINBMP);
    AltCalc ac;
    AltJmp jmp;
    uint64_t tck;
    bool ok, iscnp = false;

    Display _dspl = Display(
        [this](U8G2 &u8g2) {
            char s[30];
            u8g2.setFont(u8g2_font_ImpactBits_tr);
            SPRN("%.0f", abs(ac.avg().alt()));
            u8g2.drawStr(SRGHT, 63, s);

            switch (jmp.mode()) {
                case AltJmp::INIT:      SCPY("INIT"); break;
                case AltJmp::GROUND:    SCPY("gnd"); break;
                case AltJmp::TAKEOFF:   SCPY("toff"); break;
                case AltJmp::FREEFALL:  SCPY("ff"); break;
                case AltJmp::CANOPY:    SCPY("cnp"); break;
                default: s[0] = '\0';
            }
            u8g2.drawStr(SRGHT, 48, s);

            SPRN("%.1f", ac.avg().speed());
            u8g2.drawStr(SRGHT, 33, s);

            if (jmp.newtm() > 0) {
                SPRN("%d", jmp.newtm()/1000);
                u8g2.drawStr(SRGHT, 14, s);
            }
        }
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

        auto interval = utm_diff(tck, tck) / 1000;
        //CONSOLE("interval: %lld", interval);
        ac.tick(bmp.readPressure(), interval);
        //CONSOLE("full: %d", ac.interval());
        auto m = jmp.mode();
        jmp.tick(ac);

        if (m != jmp.mode())
            mode(m);
        
        if (!iscnp && ((m == AltJmp::FREEFALL) || (m == AltJmp::CANOPY)) && (ac.avg().alt() < 1500)) {
            iscnp = true;
            ledByJump(LED_FFEND);
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
