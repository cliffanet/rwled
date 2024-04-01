/*
    Display worker
*/

#ifndef _display_H
#define _display_H

#include "../../def.h"
#include <U8g2lib.h>
#include <functional>

#define SPRN(str, ...)      snprintf_P(s, sizeof(s), PSTR(str), ##__VA_ARGS__)
#define SCPY(str)           do { strncpy_P(s, PSTR(str), sizeof(s)); s[sizeof(s)-1] = '\0'; } while (0)
#define SWIDTH              u8g2.getStrWidth(s)
#define SRGHT               (u8g2.getDisplayWidth()-SWIDTH)
#define SCENT               (SRGHT/2)

class Display {
    public:
        static void init();
        static void redraw();

        typedef std::function<void (U8G2 &u8g2)> hnd_t;
        const hnd_t hnd;
        const bool clr;

        Display(hnd_t hnd, bool clr = false, bool visible = true);
        ~Display();
        void show();
        void hide();
};

#endif // _display_H
