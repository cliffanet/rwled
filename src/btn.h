/*
    Button worker
*/

#ifndef _btnwork_H
#define _btnwork_H

#define PINBTN  35

typedef enum {
    BTNNONE = 0,
    BTNWIFI,
    BTNNORM
} btn_mode_t;

void btnStart(btn_mode_t mode = BTNWIFI);
bool btnStop();

bool btnMode(btn_mode_t mode);

#endif // _btnwork_H
