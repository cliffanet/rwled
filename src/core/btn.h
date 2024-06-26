/*
    Button worker
*/

#ifndef _btn_H
#define _btn_H

#include "../../def.h"
#include <functional>

#define PINBTN  35
#define PWRBTN  GPIO_NUM_35

class Btn {
    public:
        typedef std::function<void (void)> hnd_t;
        const hnd_t sng, lng;

        Btn(hnd_t sng, hnd_t lng = NULL);
        ~Btn();
        bool activate();
        bool hide();
};

#endif // _btn_H
