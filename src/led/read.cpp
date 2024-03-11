
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


/************************************************************/

class Buf {
    uint8_t _data[2048];
    uint16_t _pos = 0, _sz = 0;
    bool _eof = false;
    SemaphoreHandle_t _mut;

    public:
        Buf() {
            _mut = xSemaphoreCreateMutex();
        }
        ~Buf() {
            vSemaphoreDelete(_mut);
        }
        // сколько ещё осталось данных в буфере
        const uint16_t avail() const { return _sz > _pos ? _sz-_pos : 0; }
        const bool eof() const { return _eof; }
        const bool needfetch() const { return !_eof && (avail() < (sizeof(_data) / 2)); }

        /*
        // перемещение данных во внешний буфер
        uint16_t read(uint8_t *dst, uint16_t sz) {
            xSemaphoreTake(_mut, portMAX_DELAY);
            if (sz > avail())
                sz = avail();
            memcpy(dst, _data+_pos, sz);
            _pos += sz;
            xSemaphoreGive(_mut);
            return sz;
        }
        */

        void clear() {
            _pos = 0;
            _sz = 0;
            _eof = false;
        }

        // чтение данных из файла
        bool fetch() {
            // вычисляем, сколько реально свободного места осталось в буфере, это:
            // - место после _sz
            auto szf = sizeof(_data) > _sz ? sizeof(_data)-_sz : 0;
            // - место до текущей _pos
            szf += _pos;
            if (szf <= 0)
                return false;
            
            // Получим данные в отдельный буфер
            uint8_t dr[szf];
            auto szr = fread(dr, 1, szf, fstr);
            //CONSOLE("sz: %d from %d", szr, szf);
            if (szr <= 0) {
                _eof = true;
                return false;
            }
            
            // теперь работаем уже с основным буфером
            // блокируем его и переносим в начало актуальные данные
            xSemaphoreTake(_mut, portMAX_DELAY);
            if (_pos > 0) {
                //CONSOLE("shift from: %d (%d)", _pos, avail());
                auto *d = _data;
                uint16_t sza = 0;
                while (_pos < _sz) {
                    *d = *(_data+_pos);
                    d   ++;
                    _pos++;
                    sza ++;
                }
                _pos = 0;
                _sz = sza;
            }
            
            // Переносим новые данные
            memcpy(_data+_sz, dr, szr);
            _sz += szr;
            //CONSOLE("new sz: %d (%d)", _sz, avail());

            xSemaphoreGive(_mut);
            return true;
        }

        bool hdr(LedFmt::head_t &h) {
            bool ok = false;
            xSemaphoreTake(_mut, portMAX_DELAY);
            if ((_sz > _pos) && ((_sz-_pos) >= sizeof(LedFmt::head_t))) {
                memcpy(&h, _data+_pos, sizeof(LedFmt::head_t));
                ok = (h.m == LEDFMT_HDR) && ((_sz-_pos) >= (sizeof(LedFmt::head_t) + h.sz));
            }
            xSemaphoreGive(_mut);

            return ok;
        }

        bool isrec() { // полна ли очередная запись
            LedFmt::head_t h;
            return hdr(h);
        }

        LedRead::Data get() {
            LedFmt::head_t h;
            if (!hdr(h))
                return LedRead::Data::fail();

            xSemaphoreTake(_mut, portMAX_DELAY);
            //CONSOLE("pos: %d, type: %d, sz: %d", _pos, h.type, h.sz);
            
            _pos += sizeof(h);
            auto d = LedRead::Data(static_cast<LedFmt::type_t>(h.type), _data+_pos, h.sz);
            _pos += h.sz;
            xSemaphoreGive(_mut);

            return d;
        }
};

static Buf _buf;
static auto _premut = xSemaphoreCreateMutex();

static void _preread_f( void * param  ) {
    xSemaphoreTake(_premut, portMAX_DELAY);
    auto buf = reinterpret_cast<Buf *>(param);
    //CONSOLE("running on core %d", xPortGetCoreID());
    bool ok = buf->fetch();
    CONSOLE("fetched: %d", ok);

    xSemaphoreGive(_premut);
    vTaskDelete( NULL );
}
static bool _preread() {
    char nam[16];
    strncpy_P(nam, PSTR("pread"), sizeof(nam));

    if (xTaskGetHandle(nam) != NULL) {
        // процесс ещё работает
        if (_buf.isrec()) // но ещё можем подождать
            return false;

        // ждать уже не можем, поэтому дожидаемся завершения
        CONSOLE("mutex beg");
        xSemaphoreTake(_premut, portMAX_DELAY);
        xSemaphoreGive(_premut);
        CONSOLE("mutex end (avail: %d)",  _buf.avail());

        if (_buf.isrec() || _buf.eof()) // дождались новых данных или конца файла
            return false;
    }


    xTaskCreateUniversal(//PinnedToCore(
        _preread_f,     /* Task function. */
        nam,            /* name of task. */
        10240,          /* Stack size of task */
        &_buf,          /* parameter of the task */
        0,              /* priority of the task */
        NULL,           /* Task handle to keep track of created task */
        1               /* pin task to core 0 */
    );

    // дожидаемся новых данных иликонца файла
    while (!_buf.isrec() && !_buf.eof())
        CONSOLE("wait data (avail: %d)", _buf.avail());

    return true;
}


/************************************************************/

const char *LedRead::fname() {
    return nstr;
}

bool LedRead::open()
{
    if (fstr != NULL)
        return false;
    
    fstr = open_P(nstr);
    if (fstr == NULL) return false;

    _buf.clear();
    _buf.fetch();
    
    auto r = LedRead::get();
    if (r.type == LedFmt::START) {
        _mynum = r.d<uint8_t>();
        CONSOLE("mynum: %d", _mynum);
    }
    else {
        CONSOLE("file format fail");
        _mynum = 0;
        //seek(0);
    }
    
    return true;
}

bool LedRead::close() {
    if (fstr == NULL)
        return false;
    
    fclose(fstr);
    fstr = NULL;
    _mynum = 0;
    _buf.clear();

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
    _s(sz <= sizeof(_d) ? sz : sizeof(_d))
{
    memcpy(_d, d, _s);
}

void LedRead::Data::data(uint8_t *buf, size_t sz) {
    if (_s <= 0)
        return;
    if (sz > _s) {
        memcpy(buf, _d, _s);
        memset(buf+_s, 0, sz-_s);
    }
    else
        memcpy(buf, _d, sz);
}

/************************************************************/

LedRead::Data LedRead::get() {
    //CONSOLE("avail: %d", _buf.avail());
    if (_buf.needfetch()) {
        //CONSOLE("need preread");
        _preread();
    }
    return _buf.get();
}

/*
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
*/
