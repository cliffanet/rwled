
#include "btn.h"
#include "worker.h"
#include "clock.h"
#include "wifi.h"
#include "log.h"

#include <list>
#include "esp32-hal-gpio.h"

class _btnWrk : public Wrk {
    int8_t _pushed = 0;
    bool _click = false;

    std::list<Btn*> _hnd;

public:
    _btnWrk() {
        pinMode(PINBTN, INPUT);
    }
#ifdef FWVER_DEBUG
    ~_btnWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }
#endif

    bool empty() const {
        return _hnd.empty();
    }

    void add(Btn *b) {
        CONSOLE("[%d]: 0x%08x", _hnd.size(), b);
        _hnd.push_back(b);
    }
    bool del(Btn *b) {
        CONSOLE("[%d]: 0x%08x", _hnd.size(), b);
        for (auto p = _hnd.begin(); p != _hnd.end(); p++)
            if (*p == b) {
                CONSOLE("found");
                _hnd.erase(p);
                return true;
            }

        return false;
    }

    state_t run() {
        if (_hnd.empty())
            return END;
        bool pushed = digitalRead(PINBTN) == LOW;
        if (pushed && (_pushed < 0))
            _pushed = 0;

        if (!_click) {
            auto h = _hnd.back();
            if (h->lng != NULL) {
                // need wait long-click
                if (pushed) {
                    _pushed++;
                    if (_pushed > 50) {
                        _click = true;
                        h->lng();
                    }
                }
                else
                if (_pushed > 2) {
                    _click = true;
                    if (h->sng != NULL)
                        h->sng();
                }
            }
            else
            if (pushed) {
                _pushed++;
                if (_pushed > 2) {
                    _click = true;
                    if (h->sng != NULL)
                        h->sng();
                }
            }
        }

        if (!pushed) {
            if (_pushed > 0)
                _pushed = 0;
            else
            if (_pushed < -5)
                _pushed --;
            else
                _click = false;
        }

        return DLY;
    }
};

static WrkProc<_btnWrk> _btn;

Btn::Btn(hnd_t sng, hnd_t lng) :
    sng(sng),
    lng(lng)
{
    if (!_btn.isrun())
        _btn = wrkRun<_btnWrk>();
    _btn->add(this);
}

Btn::~Btn() {
    if (!_btn.isrun())
        return;
    _btn->del(this);
    if (_btn->empty())
        _btn.term();
}
