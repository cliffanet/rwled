
#include "dataread.h"
#include "worker.h"
#include "ledstream.h"
#include "log.h"

#include "txt.h"

//#include <Arduino.h> // ESP.getFreeHeap()
//#include <pgmspace.h>   // PSTR()

class iParser {
    enum {
        HEAD,
        DATA,
        FIN
    } _state;

public:
    iParser() { clear(); }

    void clear() {
        _state = HEAD;
    }

    bool addstr(const char *s) {
        char p[32];
        size_t n = txtparam(p, sizeof(p), s);
        if (n < 2)
            return false;
        const char *v = s+n;
        //CONSOLE("param: {%s} - |%s|", p, v);

        switch (_state) {
            case HEAD:
                if (strcmp_P(p, PSTR("LEDS")) == 0) {
                    const char *c = v;
                    auto n = strtol(v, (char **)&c, 10);
                    if ((n < 1) || (n > 16) || (c <= v) || (*c != '\0')) {
                        CONSOLE("ERR: HEAD number");
                        return false;
                    }
                    //CONSOLE("head: %d", n);
                    _state = DATA;
                }
                else {
                    CONSOLE("ERR: param on HEAD: %s", p);
                    return false;
                }
                break;
            
            case DATA:
                if (strcmp_P(p, PSTR("tm")) == 0) {
                    const char *c = v;
                    auto n = strtol(v, (char **)&c, 10);
                    if ((n < 0) || (n > 3600000) || (c <= v) || (*c != '\0')) {
                        CONSOLE("ERR: TM value");
                        return false;
                    }
                    //CONSOLE("tm: %d", n);
                }
                else
                if (strcmp_P(p, PSTR("ch")) == 0) {
                    const char *c = v;
                    auto n = strtol(v, (char **)&c, 10);
                    if ((n < 1) || (n > 4) || (c <= v) || (*c != '\0')) {
                        CONSOLE("ERR: CHAN number");
                        return false;
                    }
                    //CONSOLE("chan: %d", n);
                }
                else
                if (strcmp_P(p, PSTR("led")) == 0) {
                    const char *c = v;
                    auto n = strtol(v, (char **)&c, 10);
                    if ((n < 1) || (n > 255) || (c <= v) || (*c != ',')) {
                        CONSOLE("ERR: LED number");
                        return false;
                    }
                    v = c+1;
                    auto col1 = strtoll(v, (char **)&c, 16);
                    if ((col1 < 0) || (col1 > 0xffffffff) || (c <= v) || (*c != '\0')) {
                        CONSOLE("ERR: LED color");
                        return false;
                    }
                    uint32_t col = col1;
                    //CONSOLE("led: %d, %08X", n, col);
                }
                else {
                    CONSOLE("ERR: param on DATA: %s", p);
                    return false;
                }
        }

        return true;
    }
};

class _consoleReader : public Wrk {
    Stream &_fh;
    BufTail _tail;
    iParser _prs;
    uint32_t _ln = 1;
    
    void clear() {
        _tail.clear();
        _prs.clear();
        _ln = 1;
    }

public:
    _consoleReader(Stream &fh) :
        _fh(fh)
    { }
#ifdef FWVER_DEBUG
    ~_consoleReader() {
        CONSOLE("_consoleReader(0x%08x) destroy", this);
    }
#endif

    state_t run() {
        auto len = _fh.available();
        if (len <= 0) return DLY;

        char buf[_tail.len()+len+1], *b = buf;

        char *br = buf + _tail.restore(buf);
        _fh.readBytes(br, len);
        br[len] = '\0';
        len += _tail.len();
        //CONSOLE("read(%d) free:%d", len, ESP.getFreeHeap());

        while (len > 0) {
            char s[256];
            int l = txtline(s, sizeof(s), b);

            if ((l <= 0) || !ISLNEND(b[l-1])) {
                //CONSOLE("tail: [%d] %s", len, b);
                // сохраняем хвост буфера
                _tail.save(b, len);
                return RUN;
            }

            //CONSOLE("line: l=%d |%s|", l, s);
            if (!_prs.addstr(s)) {
                CONSOLE("PARSE[%d] ERROR: %s", _ln, s);
                clear();
                return RUN;
            }

            for (int i = 0; i<l; i++)
                if (b[i] == '\n')
                    _ln ++;

            b += l;
            len -= l;
        }

        return _fh.available() > 0 ? RUN : DLY;
    }
};

static WrkProc<_consoleReader> _reader;
void initConsoleReader(Stream &fh) {
    if (!_reader.isrun())
        _reader = wrkRun<_consoleReader>(fh);
}
