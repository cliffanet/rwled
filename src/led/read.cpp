
#include "read.h"
#include "fmt.h"
#include "ws2812.h"
#include "../core/file.h"
#include "../core/clock.h"
#include "../core/log.h"

#include <string.h>
#include "vfs_api.h"
#include <sys/unistd.h>

static uint8_t _pinall[] = { 26, 27, 25, 32 };

#define mutTake(m)      xSemaphoreTake(m, portMAX_DELAY)
#define mutGive(m)      xSemaphoreGive(m)

/************************************************************
 *
 *  Блок включения/выключения питания лент
 * 
 ************************************************************/
static void fullcolor(uint8_t pin, uint8_t r, uint8_t g, uint8_t b);

void LedRead::on() {
    bool reset = digitalRead(LEDLIGHT_PINENABLE) == LOW;

    pinMode(LEDLIGHT_PINENABLE, OUTPUT);
    digitalWrite(LEDLIGHT_PINENABLE, HIGH);

    if (reset)
        for (const auto &pin: _pinall)
            fullcolor(pin, 0, 0, 0);
}

void LedRead::off() {
    for (const auto &pin: _pinall)
        fullcolor(pin, 0, 0, 0);
    digitalWrite(LEDLIGHT_PINENABLE, LOW);
}


/************************************************************
 *
 *  Класс записи сценария
 * 
 ************************************************************/

class LedRec {
    // тут нельзя использовать ссылку на буфер:
    // 1. эти данные читаются из буфера, который может меняться другим потоком
    // 2. именно тут данных не так уж и много, чтобы за них так сильно переживать
    uint8_t _d[64];
    size_t _s;
    public:
        const LedFmt::type_t type;
    
        LedRec(LedFmt::type_t type, const uint8_t *d, size_t sz) :
            type(type),
            _s(sz <= sizeof(_d) ? sz : sizeof(_d))
        {
            memcpy(_d, d, _s);
        }

        uint8_t sz()    const { return _s; }

        void data(uint8_t *buf, size_t sz) {
            if (_s <= 0)
                return;
            if (sz > _s) {
                memcpy(buf, _d, _s);
                memset(buf+_s, 0, sz-_s);
            }
            else
                memcpy(buf, _d, sz);
        }
        template <typename T>
        T d() {
            // Можно было бы вернуть просто: const T&
            // сделав простой возврат: *reinterpret_cast<uint8_t*>(_b+4)
            // Но тут есть большой риск, что придут данные, короче, чем T,
            // и тогда будем иметь неприятные вылеты. Поэтому лучше лишний
            // раз скопировать данные и обнулить хвост при необходимости.
            T d;
            data(reinterpret_cast<uint8_t*>(&d), sizeof(T));
            return d;
        }

        static LedRec fail() {
            return LedRec(LedFmt::FAIL, NULL, 0);
        }
};

/************************************************************
 *
 *  Блок функций работы с файлом данных
 * 
 ************************************************************/

static FILE* fstr = NULL;
static const auto nstr = PSTR("/strm.led");
static uint8_t _mynum = 0;

static LedRec _bufopen();

const char *LedRead::fname() {
    return nstr;
}

bool LedRead::open()
{
    if (fstr != NULL)
        return false;
    
    PFNAME(nstr);
    char mode[2] = { 'r' };
    fstr = fopen(fname, mode);
    if (fstr)
        CONSOLE("opened[%s] ok: %s", mode, fname);
    else {
        CONSOLE("can't open[%s] %s", mode, fname);
        return false;
    }
    
    auto r = _bufopen();
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


/************************************************************
 *
 *  Блок упреждающего чтения данных из файла в буфер
 * 
 ************************************************************/

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
            mutTake(_mut);
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

            mutGive(_mut);
            return true;
        }

        bool hdr(LedFmt::head_t &h) {
            bool ok = false;
            mutTake(_mut);
            if ((_sz > _pos) && ((_sz-_pos) >= sizeof(LedFmt::head_t))) {
                memcpy(&h, _data+_pos, sizeof(LedFmt::head_t));
                ok = (h.m == LEDFMT_HDR) && ((_sz-_pos) >= (sizeof(LedFmt::head_t) + h.sz));
            }
            mutGive(_mut);

            return ok;
        }

        LedRec get() {
            LedFmt::head_t h;
            if (!hdr(h))
                return LedRec::fail();

            mutTake(_mut);
            //CONSOLE("pos: %d, type: %d, sz: %d", _pos, h.type, h.sz);
            
            _pos += sizeof(h);
            auto d = LedRec(static_cast<LedFmt::type_t>(h.type), _data+_pos, h.sz);
            _pos += h.sz;
            mutGive(_mut);

            return d;
        }
};

static Buf _buf;
static auto _premut = xSemaphoreCreateMutex();

static LedRec _bufopen() {
    _buf.clear();
    _buf.fetch();
    return _buf.get();
}

static void _preread_f( void * param  ) {
    mutTake(_premut);
    //CONSOLE("running on core %d", xPortGetCoreID());
    bool ok = _buf.fetch();
    //CONSOLE("fetched: %d, fpos", ok, ftell(fstr));

    mutGive(_premut);
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
        mutTake(_premut);
        mutGive(_premut);
        CONSOLE("mutex end (avail: %d)",  _buf.avail());

        if (_buf.reced() || _buf.eof()) // дождались новых данных или конца файла
            return false;
    }

    xTaskCreateUniversal(//PinnedToCore(
        _preread_f,     /* Task function. */
        nam,            /* name of task. */
        10240,          /* Stack size of task */
        NULL,           /* parameter of the task */
        5,              /* priority of the task */
        NULL,           /* Task handle to keep track of created task */
        0               /* pin task to core 0 */
    );

    return true;
}


/************************************************************
 *
 *  Блок заполнения информации о цветах светодиодов в массивы
 * 
 ************************************************************/

static auto _ledmut = xSemaphoreCreateMutex();

typedef struct {
    uint8_t     num;
    bool        chg;
    uint16_t    cnt;
    uint8_t     col[LEDREAD_NUMPIXELS*3];
} ledchan_t;
static ledchan_t _ledall[] = {
    { 1 },
    { 2 },
    { 3 },
    { 4 },
};

static void _led_clear() {
    mutTake(_ledmut);
    uint8_t n = 0;
    for (auto &l: _ledall) {
        bzero(&l, sizeof(ledchan_t));
        n++;
        l.num = n;
    }
    mutGive(_ledmut);
}

static inline uint8_t _acolel(uint32_t acol, uint8_t bit, uint8_t a) {
    return ((((acol >> bit) & 0xff) * a / 0xff) & 0xff) << bit;
}
typedef struct {
    uint8_t     r, g, b;
} ledcol_t;
static ledcol_t _acolor(uint32_t acol) {
    uint8_t a = (acol >> 24) & 0xff;
    ledcol_t col;
    if (a == 0xff) {
        col.r = (acol & 0x00ff0000) >> 16;
        col.g = (acol & 0x0000ff00) >> 8;
        col.b = (acol & 0x000000ff);
    }
    else {
        col.r = _acolel(acol, 16, a);
        col.g = _acolel(acol,  8, a);
        col.b = _acolel(acol,  0, a);
    }

    return col;
}

static auto _fillmut = xSemaphoreCreateMutex();
static bool _fillrun = false;
void _fill_f( void * param  ) {
    mutTake(_fillmut);
    auto beg = tmill();
    uint32_t tm = 0;
    CONSOLE("running on core %d, beg: %lld", xPortGetCoreID(), beg);

    _led_clear();
    ledchan_t *led = NULL;

    while (_fillrun && (fstr != NULL)) {
        // Ожидаем очередное время tm
        while (_fillrun && (tmill() - beg < tm)) ;

        // дожидаемся новых данных или конца файла
        while (_fillrun && !_buf.reced() && !_buf.eof())
            CONSOLE("wait data (avail: %d)", _buf.avail());
        
        // новая запись
        auto r = _buf.get();

        // инициируем догрузку пре-буфера, если требуется
        if (_buf.needfetch())
            _preread();
        
        // парсим
        switch (r.type) {
            case LedFmt::FAIL:
                goto theEnd;
            
            case LedFmt::START:
                CONSOLE("start: %d", r.d<uint8_t>());
                break;
            
            case LedFmt::TIME:
                tm = r.d<uint32_t>();
                //CONSOLE("tm: %d", tm);
                break;
            
            case LedFmt::CHAN: {
                    auto n = r.d<uint8_t>();
                    led = (n > 0) && (n <= 4) ?
                        _ledall + n - 1 : NULL;
                    //CONSOLE("ch: %d", n);
                }
                break;
            
            case LedFmt::LED: {
                    auto c = r.d<LedFmt::col_t>();
                    if ((led != NULL) && (c.num > 0) && (c.num <= LEDREAD_NUMPIXELS)) {
                        auto col = _acolor(c.color);
                        mutTake(_ledmut);
                        auto *d = led->col + (c.num-1)*3;
                        // G - R - B - order
                        d[0] = col.g;
                        d[1] = col.r;
                        d[2] = col.b;
                        led->chg = true;
                        if (led->cnt < c.num)
                            led->cnt = c.num;
                        mutGive(_ledmut);
                    }
                    //CONSOLE("color: %d = 0x%08x", c.num, c.color);
                }
                break;
            
            case LedFmt::LOOP: {
                    auto l = r.d<LedFmt::loop_t>();
                    CONSOLE("LOOP: curbeg=%lld, curtm=%lld/%d, tm=%d, len=%d", beg, tmill() - beg, tm, l.tm, l.len);
                    beg += l.len - l.tm;
                    //beg = tmill() +  - l.tm;
                    tm = l.tm;
                    led = NULL;
                    CONSOLE("beg: %lld / %d", beg, tm);
                    CONSOLE("tmill: %lld", tmill());
                }
                break;
            
            case LedFmt::END:
                CONSOLE("end");
                goto theEnd;
        }
    }

    theEnd:
    CONSOLE("finish");
    
    _led_clear();

    // сбросим буфер в начало
    _buf.clear();
    if (fstr != NULL) {
        mutTake(_premut);
        fseek(fstr, 0, SEEK_SET);
        mutGive(_premut);
    }
    _preread();

    mutGive(_fillmut);
    vTaskDelete( NULL );
}

static TaskHandle_t _fill(bool run) {
    char nam[16];
    strncpy_P(nam, PSTR("ledfill"), sizeof(nam));

    TaskHandle_t hnd = xTaskGetHandle(nam);

    if (run && (hnd == NULL)) {
        _fillrun = true;
        xTaskCreateUniversal(//PinnedToCore(
            _fill_f,        /* Task function. */
            nam,            /* name of task. */
            10240,          /* Stack size of task */
            NULL,           /* parameter of the task */
            0,              /* priority of the task */
            NULL,           /* Task handle to keep track of created task */
            1               /* pin task to core 0 */
        );
    }

    return hnd;
}

/************************************************************
 *
 *  Блок отрисовки цветов на светодиоды
 * 
 ************************************************************/
static auto _drvmut = xSemaphoreCreateMutex();

static auto _showmut = xSemaphoreCreateMutex();
static bool _showrun = false;
void _show_f( void * param  ) {
    mutTake(_showmut);
    CONSOLE("running on core %d", xPortGetCoreID());
    typedef uint8_t col_t[LEDREAD_NUMPIXELS*3];
    col_t colall[4];
    bool chg[4];
    size_t sz[4];
    //uint8_t col[LEDREAD_NUMPIXELS*3];


    mutTake(_drvmut);
    for (uint8_t chan=0; chan < 4; chan++)
        LedDriver::init(chan, _pinall[chan]);
    mutGive(_drvmut);

    while (_showrun) {
        uint8_t chan = 0;
        //CONSOLE("----");
        for (auto &l: _ledall) {
            auto col=colall[chan];
            mutTake(_ledmut);
            chg[chan] = l.chg && (l.cnt > 0);
            mutGive(_ledmut);

            if (!chg[chan]) {
                chan ++;
                continue;
            }

            CONSOLE("draw: %d", l.num);

            mutTake(_ledmut);
            sz[chan] = l.cnt * 3;
            //CONSOLE("size: %d / %d", sz, sizeof(col));
            memcpy(col, l.col, sz[chan]);
            l.chg = false;
            mutGive(_ledmut);

            uint8_t pin = _pinall[l.num-1];

            mutTake(_drvmut);
            LedDriver::write(chan, col, sz[chan]);
            LedDriver::wait(chan);
            mutGive(_drvmut);
            //vTaskDelay(pdMS_TO_TICKS(100));
            
            chan ++;
        }

        /*
        for (uint8_t i=0; i < 2; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
            chan = 0;
            mutTake(_drvmut);
            for (auto &l: _ledall) {
                if (chg[chan])
                    LedDriver::write(chan, colall[chan], sz[chan]);
                chan ++;
            }
            mutGive(_drvmut);

            mutTake(_drvmut);
            chan = 0;
            for (auto &l: _ledall) {
                if (chg[chan])
                    LedDriver::wait(chan);
                chan ++;
            }
            mutGive(_drvmut);
        }
        */

        //CONSOLE("+++");
        vTaskDelay(pdMS_TO_TICKS(100));
        //CONSOLE("////");
    }

    mutTake(_drvmut);
    for (uint8_t chan=0; chan < 4; chan++)
        LedDriver::done(chan, _pinall[chan]);
    mutGive(_drvmut);

    CONSOLE("finish");
    mutGive(_showmut);
    vTaskDelete( NULL );
}

static TaskHandle_t _show(bool run) {
    char nam[16];
    strncpy_P(nam, PSTR("ledshow"), sizeof(nam));

    TaskHandle_t hnd = xTaskGetHandle(nam);

    if (run && (hnd == NULL)) {
        _showrun = true;
        xTaskCreateUniversal(//PinnedToCore(
            _show_f,        /* Task function. */
            nam,            /* name of task. */
            10240,          /* Stack size of task */
            NULL,           /* parameter of the task */
            0,              /* priority of the task */
            NULL,           /* Task handle to keep track of created task */
            1               /* pin task to core 0 */
        );
    }

    return hnd;
}



/************************************************************/
void LedRead::start() {
    _fill(true);
    _show(true);
}

void LedRead::stop() {
    CONSOLE("_fillrun: %d, _showrun: %d", _fillrun, _showrun);
    _showrun = false;
    _fillrun = false;

    mutTake(_showmut);
    mutGive(_showmut);
    mutTake(_fillmut);
    mutGive(_fillmut);
    CONSOLE("fin");
}

bool LedRead::isrun() {
    return (_fill(false) != NULL) || (_show(false) != NULL);
}

/* ------------------------------------------------ */

static void fullcolor(uint8_t pin, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t led[LEDREAD_NUMPIXELS*3];
    for (int n = 0; n < LEDREAD_NUMPIXELS; n++) {
        auto *d = led + n*3;
        // G - R - B - order
        d[0] = g;
        d[1] = r;
        d[2] = b;
    }
    CONSOLE("make: %d", pin);
    mutTake(_drvmut);
    LedDriver::make(pin, led, sizeof(led));
    mutGive(_drvmut);
}

void LedRead::fixcolor(uint8_t nmask, uint32_t col, bool force) {
    return;
    uint8_t nbit = 1;
    static uint32_t colall[4] = { 0, 0, 0, 0 };
    uint32_t *c = colall;

    if (_fillrun || _showrun) {
        stop();
        for (auto &c: colall)
            c = 0;
        for (const auto &pin: _pinall)
            fullcolor(pin, 0, 0, 0);
    }

    uint8_t
        r = (col & 0x00ff0000) >> 16,
        g = (col & 0x0000ff00) >> 8,
        b = (col & 0x000000ff);

    for (const auto &pin: _pinall) {
        if (((nmask & nbit) > 0) && (force || (*c != col))) {
            fullcolor(pin, r, g, b);
            *c = col;
        }
        nbit = nbit << 1;
        c ++;
    }
}
