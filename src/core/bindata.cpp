
#include "bindata.h"

#include <cstring>

static void ck(uint8_t &a, uint8_t &b, const uint8_t *data, uint8_t sz) {
    a = 0;
    b = 0;
    while (sz > 0) {
        a += *data;
        b += a;
        data ++;
        sz--;
    }
}

void binpk(uint8_t *buf, const uint8_t &mgc, const uint16_t &cmd, const uint8_t *data, const uint8_t &sz) {
    buf[0] = mgc;
    buf[1] = (cmd >> 8) & 0xff;
    buf[2] = cmd & 0xff;
    buf[3] = sz;
    auto b = buf+4;
    memcpy(b, data, sz);
    b += sz;
    ck(b[0], b[1], data, sz);
}


BinRecv::BinRecv(const uint8_t *buf, size_t sz, uint8_t mgc) :
    _b(buf),
    _s(sz)
{
    if (sz < 6) {
        _state = SHORT;
        _cmd = 0;
        _len = 0;
        return;
    }
    if (buf[0] != mgc) {
        _state = PROTO;
        _cmd = 0;
        _len = 0;
        return;
    }

    _cmd = (static_cast<uint16_t>(buf[1]) << 8) | buf[2];
    _len = buf[3];
    if (sz < (_len+6)) {
        _state = SHORT;
        return;
    }

    uint8_t cka, ckb;
    ck(cka, ckb, buf+4, _len);
    _state = (cka == buf[_len+4]) && (ckb == buf[_len+5]) ?
        OK :
        PROTO;
}

void BinRecv::data(uint8_t *buf, size_t sz) {
    if (_state != OK) return;
    if (sz > _len) {
        memcpy(buf, _b+4, _len);
        memset(buf+_len, 0, sz-_len);
    }
    else
        memcpy(buf, _b+4, sz);
}
