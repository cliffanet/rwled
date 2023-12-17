
#include "ledstream.h"
#include "log.h"

#include <stdlib.h>
#include <cstring>
#include <SPIFFS.h>
#include <FS.h>

static File fstr;
static const auto nstr = PSTR("/strm.led");

/* ------------------------------------------------------------------------------------------- *
 *  инициализация
 * ------------------------------------------------------------------------------------------- */
static bool initok = false;
bool lsbegin() {
    if (!SPIFFS.begin(true)) {
        CONSOLE("SPIFFS Mount Failed");
        return false;
    }

    char fname[32];
    strncpy_P(fname, nstr, sizeof(fname));
    fstr = SPIFFS.open(fname, FILE_READ);
    if (fstr)
        CONSOLE("opened ok: %s, %db", fstr.name(), fstr.size());
    else
        CONSOLE("can't open %s", fname);
    
    initok = true;
    
    return true;
}

/* ------------------------------------------------------------------------------------------- *
 *  основной файл потока
 * ------------------------------------------------------------------------------------------- */

bool lsopened() {
    return fstr;
}

static uint8_t buf[1100];
static uint16_t bc = 0, bl = 0;
bool _bufread(uint8_t *data, size_t sz) {
    if (sz > bl) {
        if (!fstr) {
            CONSOLE("led stream not opened");
            return LSFAIL;
        }
        if (bc > 0) {
            for (int i = 0; i < bl; i++)
                buf[i] = buf[bc+i];
            bc = 0;
        }
        size_t l = 1024;
        if (l+bl > sizeof(buf))
            l = sizeof(buf) - bl;
        l = fstr.read(buf+bl, l);
        CONSOLE("buf readed: %d / %d", l, fstr.position());
        if (l < 1) {
            CONSOLE("[%d]: can\'t file read", fstr.position());
            return false;
        }
        bl += l;
    }

    if (sz > bl) {
        CONSOLE("[%d]: no buf[%d] data(%d)", fstr.position(), sz, bl);
        return false;
    }

    if (data != NULL)
        memcpy(data, buf+bc, sz);
    bc += sz;
    bl -= sz;

    return true;
}

ls_type_t lsget(uint8_t *data, size_t sz) {

    ls_rhead_t h;
    if (!_bufread(reinterpret_cast<uint8_t *>(&h), sizeof(h))) {
        CONSOLE("[%d]: hdr read", fstr.position());
        return LSFAIL;
    }
    if (h.m != LSHDR) {
        CONSOLE("[%d]: hdr fail", fstr.position());
        return LSFAIL;
    }

    size_t l = h.sz < sz ? h.sz : sz;
    if (l < 0) l = 0;
    if ((l > 0) && !_bufread(data, l)) {
        CONSOLE("[%d]: data read", fstr.position());
        return LSFAIL;
    }
    if ((h.sz > l) && !_bufread(NULL, h.sz-l)) {
        CONSOLE("[%d]: skip empty", fstr.position());
        return LSFAIL;
    }
    
    return static_cast<ls_type_t>(h.type);
}


/* ------------------------------------------------------------------------------------------- *
 *  временный файл
 * ------------------------------------------------------------------------------------------- */
static File ftmp;
static const auto ntmp = PSTR("/.tmp");

bool lstmp() {
    if (!initok || ftmp)
        return false;
    
    char fname[32];
    strncpy_P(fname, ntmp, sizeof(fname));

    SPIFFS.remove(fname);
    ftmp = SPIFFS.open(fname, FILE_WRITE);
    if (!ftmp)
        CONSOLE("tmp file open fail");

    return ftmp;
}

bool lsadd(ls_type_t type, const uint8_t *data, size_t sz) {
    ls_rhead_t h;
    h.type  = type;
    h.sz    = sz;

    if (!ftmp) {
        CONSOLE("lstmp not opened");
        return false;
    }

    if (ftmp.write(reinterpret_cast<const uint8_t *>(&h), sizeof(h)) != sizeof(h)) {
        CONSOLE("lstmp hdr-write fail");
        return false;
    }

    if ((sz > 0) && (ftmp.write(data, sz) != sz)) {
        CONSOLE("lstmp data-write fail");
        return false;
    }

    return true;
}

int lsfindtm(uint32_t tm) {
    auto orig = ftmp.position();
    if (ftmp)
        ftmp.close();
    
    char fname[32];
    strncpy_P(fname, ntmp, sizeof(fname));
    ftmp = SPIFFS.open(fname, FILE_READ);
    if (!ftmp) {
        CONSOLE("lstmp not opened");
        return -1;
    }

    CONSOLE("ftmp: %s, %d", ftmp.name(), ftmp.size());
    ftmp.seek(0, SeekSet);

    int pos = -1;
    while (true) {
        auto p = ftmp.position();
        ls_rhead_t h;
        if (ftmp.read(reinterpret_cast<uint8_t *>(&h), sizeof(h)) != sizeof(h)) {
            CONSOLE("[%d]: hdr read", p);
            break;
        }
        if (h.m != LSHDR) {
            CONSOLE("[%d]: hdr fail", p);
            break;
        }
        if (h.type != LSTIME) {
            ftmp.seek(h.sz, SeekCur);
            continue;
        }
        if (h.sz < 4) {
            CONSOLE("[%d]: tm sz: %d", p, h.sz);
            break;
        }
        uint32_t tm1 = 0;
        if (ftmp.read(reinterpret_cast<uint8_t *>(&tm1), sizeof(tm1)) != sizeof(tm1))
            break;
        CONSOLE("find tm[%d]: %d", p, tm1);
        if (tm1 >= tm) {
            pos = p;
            break;
        }
    }

    ftmp.close();
    ftmp = SPIFFS.open(fname, FILE_APPEND);
    if (ftmp)
        CONSOLE("reopened: %s, %d", ftmp.name(), ftmp.size());
    else
        CONSOLE("reopen fail: %s", fname);
    CONSOLE("seek to: %d", orig);
    ftmp.seek(orig, SeekSet);
    CONSOLE("pos: %d / %d", ftmp.position(), ftmp.size());

    return pos;
}

bool lsfin() {
    if (!ftmp) {
        CONSOLE("lstmp not opened");
        return false;
    }
    CONSOLE("fsize: %s, %d", ftmp.name(), ftmp.size());

    if (fstr)
        fstr.close();

    ftmp.close();
    ftmp = File();
    CONSOLE("tmp closed");

    char fsrc[32], fname[32];
    strncpy_P(fsrc, ntmp, sizeof(fsrc));
    strncpy_P(fname, nstr, sizeof(fname));
    SPIFFS.remove(fname);
    if (SPIFFS.rename(fsrc, fname))
        CONSOLE("renamed: %s -> %s", fsrc, fname);
    else {
        CONSOLE("rename fail: %s -> %s", fsrc, fname);
        return false;
    }

    return true;
}