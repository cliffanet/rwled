/*
    Bin pack/unpack
*/

#ifndef _bin_data_H
#define _bin_data_H

#include <stdint.h>
#include <cstddef>

void binpk(uint8_t *buf, const uint8_t &mgc, const uint16_t &cmd, const uint8_t *data, const uint8_t &sz);

template <int S>
class Bin {
    uint8_t _b[S + 6];

    public:
        Bin(uint16_t cmd, const uint8_t *data, uint8_t mgc = '#') {
            binpk(_b, mgc, cmd, data, S);
        }

        size_t sz()     const { return sizeof(_b); }
        size_t len()    const { return S; }
        const uint8_t *buf() const { return _b; }
};

template <typename T>
Bin<sizeof(T)> BinData(uint16_t cmd, const T &data, uint8_t mgc = '#') {
    return Bin<sizeof(T)>(cmd, reinterpret_cast<const uint8_t*>(&data), mgc);
}

inline
Bin<0> BinCmd(uint16_t cmd, uint8_t mgc = '#') {
    return Bin<0>(cmd, NULL, mgc);
}

class BinRecv {
    const uint8_t *_b;
    size_t _s;
    uint16_t _cmd;
    uint8_t  _len;
    typedef enum {
        OK = 0,
        SHORT,
        PROTO
    } state_t;
    state_t _state;
    public:
        BinRecv(const uint8_t *buf, size_t sz, uint8_t mgc = '#');
        bool valid()    const { return _state == OK; }
        operator bool() const { return _state == OK; }
        state_t state() const { return _state; }
        uint16_t cmd()  const { return _cmd; }
        uint8_t sz()    const { return _len+6; }
        uint8_t len()   const { return _len; }
        
        void data(uint8_t *buf, size_t sz);
        template <typename T>
        T data() {
            // Можно было бы вернуть просто: const T&
            // сделав простой возврат: *reinterpret_cast<uint8_t*>(_b+4)
            // Но тут есть большой риск, что придут данные, короче, чем T,
            // и тогда будем иметь неприятные вылеты. Поэтому лучше лишний
            // раз скопировать данные и обнулить хвост при необходимости.
            T d;
            data(reinterpret_cast<uint8_t*>(&d), sizeof(T));
            return d;
        }
};

#endif // _bin_data_H
