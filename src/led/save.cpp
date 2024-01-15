
#include "save.h"
#include "fmt.h"
#include "../core/file.h"
#include "../core/log.h"

#include <string.h>
#include "vfs_api.h"
#include <sys/unistd.h>


static FILE* ftmp = NULL;
static const auto ntmp = PSTR("/.tmp");

static inline FILE* open_P(const char *name, char m = 'r') {
    PFNAME(name);
    char mode[2] = { m };
    FILE* f = fopen(fname, mode);
    if (f)
        CONSOLE("opened[%s] ok: %s", mode, fname);
    else
        CONSOLE("can't open[%s] %s", mode, fname);
    
    return f;
}

static bool tmpadd(LedFmt::type_t type, const uint8_t *data, size_t sz) {
    LedFmt::head_t h;
    h.type  = type;
    h.sz    = sz;

    if (ftmp == NULL) {
        CONSOLE("lstmp not opened");
        return false;
    }

    auto sz1 = fwrite(&h, 1, sizeof(h), ftmp);
    if (sz1 != sizeof(h)) {
        CONSOLE("lstmp hdr-write fail: %d", sz1);
        return false;
    }

    if ((sz > 0) && ((sz1 = fwrite(data, 1, sz, ftmp)) != sz)) {
        CONSOLE("lstmp data-write fail: %d/%d", sz1, sz);
        return false;
    }

    return true;
}

inline bool tmpadd(LedFmt::type_t type) {
    return tmpadd(type, NULL, 0);
}

template <typename T>
static bool tmpadd(LedFmt::type_t type, const T &data) {
    return tmpadd(type, reinterpret_cast<const uint8_t *>(&data), sizeof(T));
}

static int tmpfindtm(uint32_t tm) {
    auto orig = ftmp != NULL ? ftell(ftmp) : 0;
    CONSOLE("ftmp pos: %d", orig);
    if (ftmp != NULL)
        fclose(ftmp);
    
    PFNAME(ntmp);
    ftmp = open_P(fname);
    if (ftmp == NULL) {
        CONSOLE("lstmp not opened for read");
        return -1;
    }

    int pos = -1;
    while (true) {
        auto p = ftell(ftmp);
        LedFmt::head_t h;
        if (fread(&h, 1, sizeof(h), ftmp) != sizeof(h)) {
            CONSOLE("[%d]: hdr read", p);
            break;
        }
        if (h.m != LEDFMT_HDR) {
            CONSOLE("[%d]: hdr fail", p);
            break;
        }
        if (h.type != LedFmt::TIME) {
            if (fseek(ftmp, h.sz, SEEK_CUR) != 0)
                break;
            continue;
        }
        if (h.sz < 4) {
            CONSOLE("[%d]: tm sz: %d", p, h.sz);
            break;
        }
        uint32_t tm1 = 0;
        if (fread(&tm1, 1, sizeof(tm1), ftmp) != sizeof(tm1))
            break;
        CONSOLE("find tm[%d]: %d", p, tm1);
        if (tm1 >= tm) {
            pos = p;
            break;
        }
    }

    fclose(ftmp);
    ftmp = open_P(fname, 'a');
    if (ftmp == NULL)
        CONSOLE("reopen fail: %s", fname);
    CONSOLE("seek aft reopen to orig: %d", orig);
    fseek(ftmp, orig, SEEK_SET);

    return pos;
}

static bool tmpfin() {
    if (ftmp != NULL) {
        fclose(ftmp);
        ftmp = NULL;
        CONSOLE("tmp closed");
    }
    return true;
/*
    lsclose();

    char fsrc[32], fname[32];
    strncpy_P(fsrc, ntmp, sizeof(fsrc));
    strncpy_P(fname, nstr, sizeof(fname));
    
    struct stat st;
    if (stat(fname, &st) == 0)
        unlink(fname);
    if (rename(fsrc, fname) == 0)
        CONSOLE("renamed: %s -> %s", fsrc, fname);
    else {
        CONSOLE("rename fail: %s -> %s", fsrc, fname);
        return false;
    }

    return lsopen();
*/
}


/* --------------------------------------------------------- */

LedSaver::LedSaver() {
    clear();
}

void LedSaver::clear() {
    _state = HEAD;
    if (ftmp != NULL) {
        fclose(ftmp);
        ftmp = NULL;
        CONSOLE("tmp closed");
    }
}

bool LedSaver::open() {
    if (ftmp != NULL) {
        CONSOLE("ftmp already opened");
        return false;
    }
    
    PFNAME(ntmp);

    struct stat st;
    if (stat(fname, &st) == 0)
        unlink(fname);

    ftmp = open_P(fname, 'w');
    if (ftmp == NULL)
        CONSOLE("tmp file open fail");

    return ftmp != NULL;
}

bool LedSaver::addstr(const char *s) {
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
                return tmpadd(LedFmt::START, num);
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
                return tmpadd(LedFmt::TIME, tm);
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
                return tmpadd(LedFmt::CHAN, num);
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
                LedFmt::col_t led = { n, col };
                //CONSOLE("led: %d, %08X", n, led.col);
                return tmpadd(LedFmt::LED, led);
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
                auto pos = tmpfindtm(beg);
                CONSOLE("finded pos: %d", pos);
                if (pos < 0) {
                    CONSOLE("ERR: LOOP fpos");
                    return false;
                }
                LedFmt::loop_t lp = { beg, len, pos };
                return tmpadd(LedFmt::LOOP, lp);
            }
            else
            if (strcmp_P(p, PSTR("END.")) == 0) {
                if (*v != '\0') {
                    CONSOLE("ERR: END format");
                    return false;
                }
                _state = END;
                CONSOLE("END whithout LOOP");
                return tmpadd(LedFmt::END) && tmpfin();
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
                return tmpadd(LedFmt::END) && tmpfin();
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

void LedSaverBuf::clear() {
    LedSaver::clear();
    _tail.clear();
    _ln = 1;
}

bool LedSaverBuf::read(size_t sz, read_t hnd) {
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
