
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
    uint8_t _data[4096];
    uint16_t _pos = 0;  // указатель на чтение из буфера
    uint16_t _chk = 0;  // указатель после всех целых записей (с этого места могут быть ещё данные, но не запись целиком)
    uint16_t _sz = 0;   // указатель на конец прочитанных из файла данных
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
        const uint16_t  avail()     const { return _sz > _pos ? _sz-_pos : 0; }
        // есть ли доступные для чтения записи в буфере
        const bool      reced()     const { return _pos < _chk; }
        // мы закончили читать файл (данные ещё могут оставаться в буфере)
        const bool      eof()       const { return _eof; }
        // пора снова читать файл
        const bool      needfetch() const { return !_eof && (_pos > (_sz / 2)); }

        void clear() {
            _pos = 0;
            _chk = 0;
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
            
            //CONSOLE("beg - _pos: %d, _chk: %d, _sz: %d, free: %d", _pos, _chk, _sz, szf);
            
            // сколько осталось после целиковых записей
            auto szc = _sz > _chk ? _sz - _chk : 0;
            
            // Получим данные в отдельный буфер:
            uint8_t dnew[szc + szf], *dr = dnew+szc, *max = dnew+szc+szf;
            // сначала кусок обрезанной записи
            if (szc > 0)
                memcpy(dnew, _data+_chk, szc);
            // и следом новые данные из буфера
            auto szr = fread(dr, 1, szf, fstr);
            //CONSOLE("sz: %d from %d", szr, szf);
            if (szr <= 0) {
                _eof = true;
                return false;
            }

            // теперь проходим все данные в dnew, процеряем целостность записей
            // но в основном это делается только для поиска LOOP-записей, чтобы
            // сделать вовремя fseek и продолжить чтение из файла с нового места
            uint8_t *dchk = dnew, *fin = dr+szr;
            while (dchk < fin) {
                uint16_t l = fin-dchk;
                LedFmt::head_t h;
                if (l < sizeof(h))
                    break;
                memcpy(&h, dchk, sizeof(h));
                if (h.m != LEDFMT_HDR) {
                    _eof = true;
                    CONSOLE("fmt fail");
                    return false;
                }
                if (l < (sizeof(h) + h.sz))
                    break;
                dchk += sizeof(h) + h.sz;

                if (h.type == LedFmt::LOOP) {
                    // попали на seek
                    LedFmt::loop_t p;
                    memcpy(&p, dchk-h.sz, sizeof(p) <= h.sz ? sizeof(p) : h.sz);
                    CONSOLE("loop: %d, %d, %d", p.fpos, p.len, p.tm);
                    fin = dchk;
                    if (fseek(fstr, p.fpos, SEEK_SET) == 0) {
                        CONSOLE("seek to: %d; fin: 0x%08x, max: 0x%08x", p.fpos, fin, max);
                        // выполняем seek и заного читаем данные
                        // из файла с новой позиции в наш буфер после loop-команды
                        if (max > fin) {
                            auto szs = fread(fin, 1, max-fin, fstr);
                            if (szs <= 0) {
                                szs = 0;
                                _eof = true;
                            }
                            fin += szs;
                        }
                    }
                    else
                        _eof = true;
                }
                // таким образом получаем непрерывный поток данных с уже выполненным seek в нужном месте
                // и дозабитый до упора буфер dnew, где:
                // - dr     - начало новых данных, которые надо скопировать в рабочий буфер
                // - dchk   - концовка всех целостных записей
                // - fin    - окончание данных, прочтённых из файла
            }
            //CONSOLE("checked: %d, dnew: %d + %d = %d", dchk-dnew, dr-dnew, fin-dr, fin-dnew);
            
            // теперь работаем уже с основным буфером
            // блокируем его и переносим в начало актуальные данные
            xSemaphoreTake(_mut, portMAX_DELAY);
            if (_pos > 0) {
                //CONSOLE("shift from: %d / %d (%d)", _pos, _chk, avail());
                // это будет сдвиг на _pos байт влево
                auto *s = _data+_pos, *f = _data+_sz;
                while (s < f) {
                    *(s-_pos) = *s;
                    s++;
                }
                _sz     -= _pos;
                _chk    -= _pos;
                _pos = 0;
            }
            
            // Переносим новые данные
            memcpy(_data+_sz, dr, fin-dr);
            _sz     += fin  - dr;
            _chk    += dchk - dnew;
            //CONSOLE("end - _pos: %d, _chk: %d, _sz: %d", _pos, _chk, _sz);

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
    //CONSOLE("fetched: %d", ok);

    xSemaphoreGive(_premut);
    vTaskDelete( NULL );
}
static bool _preread() {
    char nam[16];
    strncpy_P(nam, PSTR("pread"), sizeof(nam));

    if (xTaskGetHandle(nam) != NULL) {
        // процесс ещё работает
        if (_buf.reced()) // но ещё можем подождать
            return false;

        // ждать уже не можем, поэтому дожидаемся завершения
        CONSOLE("mutex beg");
        xSemaphoreTake(_premut, portMAX_DELAY);
        xSemaphoreGive(_premut);
        CONSOLE("mutex end (avail: %d)",  _buf.avail());

        if (_buf.reced() || _buf.eof()) // дождались новых данных или конца файла
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
    // дожидаемся новых данных или конца файла
    while (!_buf.reced() && !_buf.eof())
        CONSOLE("wait data (avail: %d)", _buf.avail());
    return _buf.get();
}

void LedRead::reset() {
    _buf.clear();
    if (fstr != NULL) {
        xSemaphoreTake(_premut, portMAX_DELAY);
        fseek(fstr, 0, SEEK_SET);
        xSemaphoreGive(_premut);
    }
    _preread();
}
