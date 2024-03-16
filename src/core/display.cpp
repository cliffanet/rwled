
#include "display.h"
#include "worker.h"
#include "log.h"

#include <list>

/* ------------------------------------------------------------------------------------------- *
 *  Дисплей
 * ------------------------------------------------------------------------------------------- */

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

class _dsplWrk : public Wrk {
    std::list<Display*> _hnd;

public:
    _dsplWrk() {
        init();
    }
    ~_dsplWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    void init() {
        CONSOLE("(0x%08x) init", this);
        u8g2.begin();
    }

    bool empty() const {
        return _hnd.empty();
    }

    void add(Display *d) {
        CONSOLE("[%d]: 0x%08x", _hnd.size(), d);
        _hnd.push_back(d);
    }
    bool del(Display *d) {
        CONSOLE("[%d]: 0x%08x", _hnd.size(), d);
        for (auto p = _hnd.begin(); p != _hnd.end(); p++)
            if (*p == d) {
                CONSOLE("found");
                _hnd.erase(p);
                return true;
            }

        return false;
    }

    state_t run() {
        u8g2.firstPage();
        do {
            for (auto d = _hnd.rbegin(); d != _hnd.rend(); ++d) {
                (*d)->hnd(u8g2);
                if ((*d)->clr)
                    break;
            }
        }  while( u8g2.nextPage() );

        return DLY;
    }
};

static WrkProc<_dsplWrk> _dspl;

void displayInit() {
    if (_dspl.isrun())
        _dspl->init();
}

Display::Display(hnd_t hnd, bool clr, bool visible) :
    hnd(hnd),
    clr(clr)
{
    if (!visible)
        return;
    if (!_dspl.isrun())
        _dspl = wrkRun<_dsplWrk>();
    _dspl->add(this);
}

Display::~Display() {
    hide();
}

void Display::show() {
    if (!_dspl.isrun())
        _dspl = wrkRun<_dsplWrk>();
    _dspl->del(this);
    _dspl->add(this);
}

void Display::hide() {
    if (!_dspl.isrun())
        return;
    _dspl->del(this);
    if (_dspl->empty())
        _dspl.term();
}
