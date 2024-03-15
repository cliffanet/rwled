
#include "light.h"
#include "read.h"
#include "../core/worker.h"
#include "../core/clock.h"
#include "../core/log.h"

#include <string.h>

/************************************************************
 *
 *  Процесс заполнения информации о цветах светодиодов в массивы
 * 
 ************************************************************/

class _fillWrk : public Wrk {
    int64_t beg;
    uint32_t tm = 0;

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
    _fillWrk() {
        CONSOLE("(0x%08x) create", this);

        beg = tmill();
    }
    ~_fillWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    state_t run() {
        if (LedRead::eof())
            return END;
        
        while (tmill() - beg >= tm) {
            if (!LedRead::recfull()) {
                CONSOLE("need wait to recfull");
                return DLY;
            }

            auto r = LedRead::get();
            
            // парсим
            switch (r.type) {
                case LedFmt::FAIL:
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
                        CONSOLE("beg: %lld / %d", beg, tm);
                        CONSOLE("tmill: %lld", tmill());
                    }
                    break;
                
                case LedFmt::END:
                    CONSOLE("end");
                    return END;
            }
        }

        return DLY;
    }
};

static WrkProc<_fillWrk> _lfwrk;

void LedLight::start() {
    if (!_lfwrk.isrun())
        _lfwrk = wrkRun<_fillWrk>();
}

void LedLight::stop() {
    if (_lfwrk.isrun())
        _lfwrk.term();
}

bool LedLight::isrun() {
    return _lfwrk.isrun();
}
