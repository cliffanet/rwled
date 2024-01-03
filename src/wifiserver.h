/*
    WiFi-server functions
*/

#ifndef _wifi_server_H
#define _wifi_server_H

#include "../def.h"
#include <stdint.h>
#include <stddef.h>

bool wifiNetIfInit();
bool wifiSrvStart();
bool wifiSrvStop();

#endif // _wifi_server_H
