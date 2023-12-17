
#include "ledstream.h"
#include "log.h"

#include <stdlib.h>
#include <cstring>
#include <SPIFFS.h>
#include <FS.h>

/* ------------------------------------------------------------------------------------------- *
 *  инициализация
 * ------------------------------------------------------------------------------------------- */
static bool initok = false;
bool lsbegin() {
    if (!SPIFFS.begin(true)) {
        CONSOLE("SPIFFS Mount Failed");
        return false;
    }
    
    initok = true;
    
    return true;
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

bool lsfin() {
    if (!ftmp) {
        CONSOLE("lstmp not opened");
        return false;
    }

    ftmp.close();
    ftmp = File();
    CONSOLE("tmp closed");

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
    ftmp = SPIFFS.open(fname, FILE_WRITE);
    ftmp.seek(orig, SeekSet);

    return pos;
}
