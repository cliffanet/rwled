
#include "dataparser.h"
#include "log.h"
#include "ledstream.h"

#include <string.h>

DataParser::DataParser() {
    clear();
    lstmp();
}

void DataParser::clear() {
    _state = HEAD;
}

bool DataParser::addstr(const char *s) {
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
                uint8_t num = n;
                return lsadd(LSSTART, num);
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
                if ((n < 0) || (n > DATALENMAX) || (c <= v) || (*c != '\0')) {
                    CONSOLE("ERR: TM value");
                    return false;
                }
                //CONSOLE("tm: %d", n);
                uint32_t tm = n;
                return lsadd(LSTIME, tm);
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
                uint8_t num = n;
                return lsadd(LSCHAN, num);
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
                auto col = strtoll(v, (char **)&c, 16);
                if ((col < 0) || (col > 0xffffffff) || (c <= v) || (*c != '\0')) {
                    CONSOLE("ERR: LED color");
                    return false;
                }
                ls_led_t led = { n, col };
                //CONSOLE("led: %d, %08X", n, led.col);
                return lsadd(LSLED, led);
            }
            else
            if (strcmp_P(p, PSTR("LOOP")) == 0) {
                const char *c = v;
                auto beg = strtol(v, (char **)&c, 10);
                if ((beg < 0) || (beg > DATALENMAX) || (c <= v) || (*c != ',')) {
                    CONSOLE("ERR: LOOP beg");
                    return false;
                }
                v = c+1;
                auto len = strtol(v, (char **)&c, 10);
                if ((len < 0) || (len > DATALENMAX) || (c <= v) || (*c != '\0')) {
                    CONSOLE("ERR: LOOP len");
                    return false;
                }
                CONSOLE("LOOP: %d, %d", beg, len);
                _state = FIN;
                auto pos = lsfindtm(beg);
                CONSOLE("finded pos: %d", pos);
                if (pos < 0) {
                    CONSOLE("ERR: LOOP fpos");
                    return false;
                }
                ls_loop_t lp = { beg, len, pos };
                return lsadd(LSLOOP, lp);
            }
            else
            if (strcmp_P(p, PSTR("END.")) == 0) {
                if (*v != '\0') {
                    CONSOLE("ERR: END format");
                    return false;
                }
                _state = END;
                CONSOLE("END whithout LOOP");
                return lsadd(LSEND) && lsfin();
            }
            else {
                CONSOLE("ERR: param on DATA: %s", p);
                return false;
            }
            break;
        
        case FIN:
            if (strcmp_P(p, PSTR("END.")) == 0) {
                if (*v != '\0') {
                    CONSOLE("ERR: END format");
                    return false;
                }
                _state = END;
                CONSOLE("END");
                return lsadd(LSEND) && lsfin();
            }
            else {
                CONSOLE("ERR: param on FIN: %s", p);
                return false;
            }
            break;
    }

    return true;
}


////

void DataBufParser::clear() {
    DataParser::clear();
    _tail.clear();
    _ln = 1;
}

bool DataBufParser::read(size_t sz, read_t hnd) {
    char buf[_tail.len()+sz+1], *b = buf, *bhnd = buf + _tail.len();

    _tail.restore(buf);
    size_t len = hnd(bhnd, sz);
    if ((len <= 0) || (len > sz))
        return false;
    
    bhnd[len] = '\0';
    //CONSOLE("read[%d/%d] |%s|", _tail.len(), len, buf);
    len += _tail.len();
    _tail.clear();

    while (len > 0) {
        char s[256];
        int l = txtline(s, sizeof(s), b);

        if ((l <= 0) || !ISLNEND(b[l-1])) {
            //CONSOLE("tail: [%d] %s", len, b);
            // больше не нашли окончания строки
            // сохраняем хвост буфера
            _tail.save(b, len);
            return true;
        }

        //CONSOLE("line: l=%d/%d |%s|", strlen(s), l, s);
        if (!addstr(s)) {
            CONSOLE("PARSE[%d] ERROR: %s", _ln, s);
            clear();
            return false;
        }

        for (int i = 0; i<l; i++)
            if (b[i] == '\n')
                _ln ++;

        b += l;
        len -= l;
    }
    
    return true;
}
