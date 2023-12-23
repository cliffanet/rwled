
#include "btn.h"
#include "worker.h"
#include "clock.h"
#include "wifi.h"
#include "log.h"

#include "esp32-hal-gpio.h"

class _btnWrk : public Wrk {
    btn_mode_t _mode;
    int8_t _pushed = 0;
    bool _click = false;

    void clksngl() {
        CONSOLE("mode: %d", _mode);
        switch (_mode) {
            case BTNWIFI:
                wifiStop();
                break;
        }
    }

    void clklong() {
        CONSOLE("mode: %d", _mode);
    }

public:
    _btnWrk(btn_mode_t m) : _mode(m) {
        pinMode(PINBTN, INPUT);
    }
#ifdef FWVER_DEBUG
    ~_btnWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }
#endif

    void mode(btn_mode_t m) {
        _mode = m;
    }
    btn_mode_t mode() const {
        return _mode;
    }

    state_t run() {
        if (_mode == BTNNONE)
            return END;
        bool pushed = digitalRead(PINBTN) == LOW;
        if (pushed && (_pushed < 0))
            _pushed = 0;

        if (!_click) {
            if (_mode == BTNNORM) {
                // need wait long-click
                if (pushed) {
                    _pushed++;
                    if (_pushed > 50) {
                        _click = true;
                        clklong();
                    }
                }
                else
                if (_pushed > 2) {
                    _click = true;
                    clksngl();
                }
            }
            else
            if (pushed) {
                _pushed++;
                if (_pushed > 2) {
                    _click = true;
                    clksngl();
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

    void end() {
        _mode = BTNNONE;
    }
};

static WrkProc<_btnWrk> _btnwrk;
void btnStart(btn_mode_t mode) {
    if (_btnwrk.isrun())
        _btnwrk->mode(mode);
    else
        _btnwrk = wrkRun<_btnWrk>(mode);
    
}

bool btnStop() {
    if (!_btnwrk.isrun())
        return false;
    _btnwrk.term();
    return true;
}

bool btnMode(btn_mode_t mode) {
    if (mode == BTNNONE)
        return btnStop();
    else
        btnStart(mode);
    return true;
}
