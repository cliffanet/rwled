
#include "read.h"
#include "fmt.h"
#include "../core/file.h"
#include "../core/log.h"

#include <string.h>
#include "vfs_api.h"
#include <sys/unistd.h>


static FILE* fstr = NULL;
static const auto nstr = PSTR("/strm.led");
static uint8_t _mynum = 0;
static uint8_t buf[1100];
static uint16_t bc = 0, bl = 0;

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

const char *LedRead::fname() {
    return nstr;
}

bool LedRead::open()
{
    if (fstr != NULL)
        return false;
    
    fstr = open_P(nstr);
    if (fstr == NULL) return false;
    
    auto r = LedRead::get();
    if (r.type == LedFmt::START) {
        _mynum = r.d<uint8_t>();
        CONSOLE("mynum: %d", _mynum);
    }
    else {
        CONSOLE("file format fail");
        _mynum = 0;
        seek(0);
    }
    
    return fstr != NULL;
}

bool LedRead::close() {
    if (fstr == NULL)
        return false;
    
    fclose(fstr);
    fstr = NULL;
    _mynum = 0;
    bc = 0;
    bl = 0;

    return true;
}

size_t LedRead::fsize()
{
    PFNAME(nstr);

    struct stat st;
    if (stat(fname, &st) != 0)
        return -1;

    return st.st_size;
}

uint8_t LedRead::mynum() {
    return _mynum;
}

bool LedRead::opened() {
    return fstr != NULL;
}


/************************************************************/

LedRead::Data::Data(LedFmt::type_t type, const uint8_t *d, size_t sz) :
    type(type),
    _d(d),
    _s(sz)
{ }

void LedRead::Data::data(uint8_t *buf, size_t sz) {
    if (sz > _s) {
        memcpy(buf, _d, _s);
        memset(buf+_s, 0, sz-_s);
    }
    else
        memcpy(buf, _d, sz);
}

/************************************************************/

static bool _bufread(uint8_t *data, size_t sz) {
    if (sz > bl) {
        if (fstr == NULL) {
            CONSOLE("led stream not opened");
            return false;
        }
        if (bc > 0) {
            for (int i = 0; i < bl; i++)
                buf[i] = buf[bc+i];
            bc = 0;
        }
        size_t l = 1024;
        if (l+bl > sizeof(buf))
            l = sizeof(buf) - bl;
        l = fread(buf+bl, 1, l, fstr);
        //CONSOLE("buf readed: %d / %d", l, ftell(fstr));
        if (l < 1) {
            CONSOLE("[%d]: can\'t file read", ftell(fstr));
            return false;
        }
        bl += l;
    }

    if (sz > bl) {
        CONSOLE("[%d]: no buf[%d] data(%d)", ftell(fstr), sz, bl);
        return false;
    }

    if (data != NULL)
        memcpy(data, buf+bc, sz);
    bc += sz;
    bl -= sz;

    return true;
}

LedRead::Data LedRead::get() {
    LedFmt::head_t h;
    if (!_bufread(reinterpret_cast<uint8_t *>(&h), sizeof(h))) {
        CONSOLE("[%d]: hdr read", ftell(fstr));
        return Data(LedFmt::FAIL, buf+bc, 0);
    }
    if (h.m != LEDFMT_HDR) {
        CONSOLE("[%d]: hdr fail", ftell(fstr));
        return Data(LedFmt::FAIL, buf+bc, 0);
    }

    if (!_bufread(NULL, h.sz)) {
        CONSOLE("[%d]: data read", ftell(fstr));
        return Data(LedFmt::FAIL, buf+bc, 0);
    }

    return Data(static_cast<LedFmt::type_t>(h.type), buf+bc-h.sz, h.sz);
}

bool LedRead::seek(size_t pos) {
    CONSOLE("pos: %d -> %d", ftell(fstr), pos);
    bc = 0;
    bl = 0;
    return fseek(fstr, pos, SEEK_SET) == 0;
}
