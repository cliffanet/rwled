/*
    Led stream: reader
*/

#ifndef _led_read_H
#define _led_read_H

#include <stddef.h>
#include <stdint.h>
#include "fmt.h"

#define LEDREAD_NUMPIXELS 80

namespace LedRead {
    const char *fname();
    bool open();
    bool close();
    size_t fsize();
    uint8_t mynum();

    bool opened();

    class Data {
        // тут нельзя использовать ссылку на буфер:
        // 1. эти данные читаются из буфера, который может меняться другим потоком
        // 2. именно тут данных не так уж и много, чтобы за них так сильно переживать
        uint8_t _d[64];
        size_t _s;
        public:
            const LedFmt::type_t type;

            Data(LedFmt::type_t type, const uint8_t *d, size_t sz);
            uint8_t sz()    const { return _s; }
            
            void data(uint8_t *buf, size_t sz);
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

            static Data fail() {
                return Data(LedFmt::FAIL, NULL, 0);
            }
    };

    typedef struct {
        uint8_t     num;
        bool        chg;
        uint16_t    cnt;
        uint32_t    col[LEDREAD_NUMPIXELS];
    } chan_t;
    
    Data get();
    void reset();

    void start();
    void stop();
    bool isrun();
}

#endif // _led_read_H
