/*
    Button worker
*/

#ifndef _btn_H
#define _btn_H

#include <functional>

#define PINBTN  35
#define PWRBTN  GPIO_NUM_35

class Btn {
    public:
        typedef std::function<void (void)> hnd_t;
        const hnd_t sng, lng;

        Btn(hnd_t sng, hnd_t lng = NULL);
        ~Btn();
};

#endif // _btn_H
