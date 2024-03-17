//#include <Arduino.h>

#include "src/core/log.h"
#include "src/core/clock.h"
#include "src/core/worker.h"
#include "src/core/file.h"
#include "src/console.h"
#include "src/wifiserver.h"
#include "src/power.h"

#include "src/led/read.h"
#include "src/led/light.h"
#include "src/led/work.h"
#include "src/jump.h"

void setup() {
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    LedLight::on(); // Это нужно тут, чтобы включить дисплей
    powerStart(true);

    // временно прямое включение для отладки
    //fileInit();
    //LedRead::open();
    //pwrmain();
}

//  это запускается после успешного "включения"
void pwrinit() {
    CONSOLE("Firmware " FWVER_FILENAME "; Build Date: " __DATE__);
    fileInit();
    LedRead::open();
    //initConsoleReader(Serial);
    wifiSrvStart();
    
    CONSOLE("init finish");
}

// Это запускается после завершения wifiSrv
void pwrmain() {
    ledWork();
    jumpStart();
}

// при выключении питания
void pwrterm() {
    LedLight::off();
}

void loop() {
    uint64_t u = utick();
    wrkProcess(30);
    u = utm_diff(u);
    if (u < 29000)
        delay((30000-u) / 1000);
}
