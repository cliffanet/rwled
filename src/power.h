/*
    Power functions
*/

#ifndef _power_H
#define _power_H

#include "../def.h"

#if HWVER >= 2
#define PINHWEN         25
#define PINBATTAB       39
#define PINBATTCD       36
#endif

namespace pwr {
    void hwon();
    void off();
    bool start(bool pwron);
    bool stop();
    void batt();
}

#endif // _power_H
