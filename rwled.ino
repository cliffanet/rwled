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

// Два основных поедателя стэка:
// - wifiserver (около 2к)
// - ledread(_buf)::fetch, который делается при инициализации в основном потоке
// У обоих выделяются под временные буферы большие массивы.
// После загрузки файла сценария wifiserver ещё не успевает выгрузится,
// а LedRead::open() уже пытается сделать fetch из файла, выделяя для этого
// буфер в 4к, т.к. надо сразу прочесть много данных.
// По умолчанию в setup() уже свободно только 6к из 8к.
// А внутри LedRead::open():
// - после загрузки файла сценария: менее 2к
// - в обычном режиме: 4.5-5.5к (т.е. тоже впритык, но хватает)

// Оказывается, есть вот такая простая функция по увеличению размера стэка
SET_LOOP_TASK_STACK_SIZE(16*1024);

void setup() {
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    LedLight::on(); // Это нужно тут, чтобы включить дисплей
    powerStart(true);

    // временно прямое включение для отладки
    //fileInit();
    //LedRead::open();
    //pwrmain();

    // Print unused stack for the task that is running setup()
    CONSOLE("Arduino Stack: %d bytes", getArduinoLoopTaskStackSize());
    CONSOLE("Free Stack: %d", uxTaskGetStackHighWaterMark(NULL));
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
    /*
        Пробовал увеличивать интервал до 100мс,
        особенной пользы от этого не заметно, но
        при этом кнопки реагируют хуже и сложнее
        с антидребезгом контактов.
        Работа сценария светодиодных лент примерно
        одинаковая и на длинном(100) и на коротком(30)
        интервалах.

        На коротком интервале (30ms):
        - больше накладные расходы на переключения между процессами
        - проще удлинить интервал внутри обновления (пропусками)
            (для display и обновления свечения светодиодных лент)
        - кнопки лучше реагируют + доступен антидребезг
    */
    wrkProcess(30);
    u = utm_diff(u);
    if (u < 29000)
        delay((30000-u) / 1000);
}
