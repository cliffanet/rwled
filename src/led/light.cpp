
#include "light.h"
#include "read.h"
#include "ws2812.h"
#include "../core/worker.h"
#include "../core/clock.h"
#include "../core/display.h"
#include "../core/log.h"

#include <string.h>
#include "esp32-hal-gpio.h"

#if HWVER < 2
static uint8_t _pinall[] = { 26, 27, 25, 32 };
#else
static uint8_t _pinall[] = { 26, 27, 16, 17 };
#endif

/************************************************************
 *
 *  Процесс заполнения информации о цветах светодиодов в массивы
 * 
 ************************************************************/

class _fillWrk : public Wrk {
    int64_t beg;
    uint32_t tm = 0;
    bool allowsync = true;

    typedef struct {
        uint8_t     num;
        bool        chg;
        size_t      sz;
        uint8_t     col[LEDLIGHT_NUMPIXELS*3];
    } chan_t;
    chan_t _all[4] = {
        { 1 },
        { 2 },
        { 3 },
        { 4 },
    };
    chan_t *ch = NULL;

    void clear() {
        uint8_t n = 0;
        for (auto &l: _all) {
            bzero(&l, sizeof(chan_t));
            n++;
            l.num = n;
        }
    }

    inline uint8_t acolel(uint32_t acol, uint8_t bit, uint8_t a) {
        return ((((acol >> bit) & 0xff) * a / 0xff) & 0xff) << bit;
    }
    typedef struct {
        uint8_t     r, g, b;
    } col_t;
    col_t acolor(uint32_t acol) {
        uint8_t a = (acol >> 24) & 0xff;
        col_t col;
        if (a == 0xff) {
            col.r = (acol & 0x00ff0000) >> 16;
            col.g = (acol & 0x0000ff00) >> 8;
            col.b = (acol & 0x000000ff);
        }
        else {
            col.r = acolel(acol, 16, a);
            col.g = acolel(acol,  8, a);
            col.b = acolel(acol,  0, a);
        }

        return col;
    }

public:
    _fillWrk(int64_t tm = -1) {
        CONSOLE("(0x%08x) create", this);

        beg = tm > 0 ? tm : tmill();
    }
    ~_fillWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    void sync(int64_t tm) {
        if (allowsync)
            beg = tm;
    }

    void chg(uint8_t n) { _all[n].chg = false; }
    const chan_t& chan(uint8_t n) { return _all[n]; }

    state_t run() {
        if (LedRead::eof())
            return END;
        
        while (tmill() - beg >= tm) {
            // Чтение из файла - наиболее приоритетная задача для стабильности обновления,
            // особенно при большом числе изменений.
            // Поэтому тут while, а не return при ожидании нужного времени.
            // Т.е. даже если придётся подвесить все остальные процессы - лучше так.
            if (!LedRead::recfull()) {
                CONSOLE("need wait to recfull");
                return DLY;
            }

            auto r = LedRead::get();
            
            // парсим
            switch (r.type) {
                case LedFmt::FAIL:
                    CONSOLE("fail");
                    return END;

                case LedFmt::START:
                    CONSOLE("start: %d", r.d<uint8_t>());
                    break;
                
                case LedFmt::TIME:
                    tm = r.d<uint32_t>();
                    //CONSOLE("tm: %d", tm);
                    break;
                
                case LedFmt::CHAN: {
                        auto n = r.d<uint8_t>();
                        ch = (n > 0) && (n <= 4) ?
                            _all + n - 1 : NULL;
                        //CONSOLE("ch: %d", n);
                    }
                    break;
                
                case LedFmt::LED: {
                        auto c = r.d<LedFmt::col_t>();
                        if ((ch != NULL) && (c.num > 0) && (c.num <= LEDLIGHT_NUMPIXELS)) {
                            auto col = acolor(c.color);
                            auto *d = ch->col + (c.num-1)*3;
                            // G - R - B - order
                            d[0] = col.g;
                            d[1] = col.r;
                            d[2] = col.b;
                            ch->chg = true;
                            size_t sz = c.num*3;
                            if (ch->sz < sz)
                                ch->sz = sz;
                        }
                        //CONSOLE("color: %d = 0x%08x", c.num, c.color);
                    }
                    break;
                
                case LedFmt::LOOP: {
                        auto l = r.d<LedFmt::loop_t>();
                        CONSOLE("LOOP: curbeg=%lld, curtm=%lld/%d, tm=%d, len=%d", beg, tmill() - beg, tm, l.tm, l.len);
                        beg += l.len - l.tm;
                        //beg = tmill() +  - l.tm;
                        tm = l.tm;
                        ch = NULL;
                        allowsync = false;
                        CONSOLE("beg: %lld / %d", beg, tm);
                        CONSOLE("tmill: %lld", tmill());
                    }
                    break;
                
                case LedFmt::END:
                    CONSOLE("end");
                    return END;
            }
        }

        // что лучше - DLY или RUN
        // - на worker-interval = 30ms - разница не видна
        // - на worker-interval = 100ms - при DLY очень плохо обновляется (рывками)

        return RUN;
    }

    void end() {
        LedRead::reset();
    }
};

static WrkProc<_fillWrk> _lfwrk;

/************************************************************
 *
 *  Заполнение всей ленты определённым цветом
 * 
 ************************************************************/

static void fullcolor(uint8_t pin, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t led[LEDLIGHT_NUMPIXELS*3];
    for (int n = 0; n < LEDLIGHT_NUMPIXELS; n++) {
        auto *d = led + n*3;
        // G - R - B - order
        d[0] = g;
        d[1] = r;
        d[2] = b;
    }
    LedDriver::make(pin, led, sizeof(led));
}

/************************************************************
 *
 *  Заполнение лент цветами из массивов _fillWrk
 * 
 ************************************************************/

class _showWrk : public Wrk {

public:
    _showWrk() {
        CONSOLE("(0x%08x) create", this);

        for (uint8_t chan=0; chan < 4; chan++)
            LedDriver::init(chan, _pinall[chan]);
    }
    ~_showWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    void done() {
        for (uint8_t chan=0; chan < 4; chan++)
            LedDriver::done(chan, _pinall[chan]);
        CONSOLE("finish");
    }

    state_t run() {
        if (!_lfwrk.isrun()) {
            done();
            return END;
        }

        for (uint8_t chan=0; chan < 4; chan++) {
            auto &c = _lfwrk->chan(chan);

            if (!c.chg || (c.sz <= 0))
                continue;

            uint8_t pin = _pinall[c.num-1];
            LedDriver::write(chan, c.col, c.sz);
            LedDriver::wait(chan);

            _lfwrk->chg(chan);
        }

        return DLY;
    }
};

static WrkProc<_showWrk> _lswrk;

/************************************************************
 *
 *  Внешнее управление лентами
 * 
 ************************************************************/
void LedLight::on() {
    bool reset = digitalRead(LEDLIGHT_PINENABLE) == LOW;

    pinMode(LEDLIGHT_PINENABLE, OUTPUT);
    digitalWrite(LEDLIGHT_PINENABLE, HIGH);

    if (reset)
        for (const auto &pin: _pinall)
            fullcolor(pin, 0, 0, 0);
    
    // после включения hwen - надо переинициировать дисплей
    Display::init();
}

void LedLight::off() {
    for (const auto &pin: _pinall)
        fullcolor(pin, 0, 0, 0);
    digitalWrite(LEDLIGHT_PINENABLE, LOW);
}


void LedLight::start(int64_t tm) {
    if (!_lfwrk.isrun())
        _lfwrk = wrkRun<_fillWrk>(tm);
    else
        _lfwrk->sync(tm);

    if (!_lswrk.isrun())
        _lswrk = wrkRun<_showWrk>();
}

void LedLight::stop() {
    if (_lfwrk.isrun())
        _lfwrk.term();

    if (_lswrk.isrun()) {
        _lswrk.term();
        _lswrk->done();
    }
}

bool LedLight::isrun() {
    return _lfwrk.isrun() && _lswrk.isrun();
}

void LedLight::fixcolor(uint8_t nmask, uint32_t col, bool force) {
    uint8_t nbit = 1;
    static uint32_t colall[4] = { 0, 0, 0, 0 };
    uint32_t *c = colall;

    if (_lfwrk.isrun() || _lswrk.isrun()) {
        stop();
        for (auto &c: colall)
            c = 0;
        for (const auto &pin: _pinall)
            fullcolor(pin, 0, 0, 0);
    }

    uint8_t
        r = (col & 0x00ff0000) >> 16,
        g = (col & 0x0000ff00) >> 8,
        b = (col & 0x000000ff);

    for (const auto &pin: _pinall) {
        if (((nmask & nbit) > 0) && (force || (*c != col))) {
            fullcolor(pin, r, g, b);
            *c = col;
        }
        nbit = nbit << 1;
        c ++;
    }
}
