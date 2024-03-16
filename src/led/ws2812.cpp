
#include "ws2812.h"
#include "../core/log.h"

#include <esp_err.h>
#include "driver/rmt.h"

#if defined(ESP_IDF_VERSION)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#define HAS_ESP_IDF_4
#endif
#endif

// This code is adapted from the ESP-IDF v3.4 RMT "led_strip" example, altered
// to work with the Arduino version of the ESP-IDF (3.2)

#define WS2812_T0H_NS (400)
#define WS2812_T0L_NS (850)
#define WS2812_T1H_NS (800)
#define WS2812_T1L_NS (450)

#define WS2811_T0H_NS (500)
#define WS2811_T0L_NS (2000)
#define WS2811_T1H_NS (1200)
#define WS2811_T1L_NS (1300)

static uint32_t t0h_ticks = 0;
static uint32_t t1h_ticks = 0;
static uint32_t t0l_ticks = 0;
static uint32_t t1l_ticks = 0;

static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    const rmt_item32_t bit0 = {{{ t0h_ticks, 1, t0l_ticks, 0 }}}; //Logical 0
    const rmt_item32_t bit1 = {{{ t1h_ticks, 1, t1l_ticks, 0 }}}; //Logical 1
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
    while (size < src_size && num < wanted_num) {
        for (int i = 0; i < 8; i++) {
            // MSB first
            if (*psrc & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }
            num++;
            pdest++;
        }
        size++;
        psrc++;
    }
    *translated_size = size;
    *item_num = num;
}

//#define ESP(func)   ESPDO(func, )
#define ESP(func)   func

void LedDriver::init(uint8_t chan, uint8_t pin) {
    //CONSOLE("chan: %d, pin: %d, max chan: %d", chan, pin, RMT_CHANNEL_MAX);
#if defined(HAS_ESP_IDF_4)
    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(pin), static_cast<rmt_channel_t>(chan));
    cfg.clk_div = 2;
#else
    // Match default TX cfg from ESP-IDF version 3.4
    rmt_config_t cfg = {
        .rmt_mode = RMT_MODE_TX,
        .channel = static_cast<rmt_channel_t>(chan),
        .gpio_num = static_cast<gpio_num_t>(pin),
        .clk_div = 2,
        .mem_block_num = 1,
        .tx_config = {
            .carrier_freq_hz = 38000,
            .carrier_level = RMT_CARRIER_LEVEL_HIGH,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_duty_percent = 33,
            .carrier_en = false,
            .loop_en = false,
            .idle_output_en = true,
        }
    };
#endif
    ESP(rmt_config(&cfg));
    ESP(rmt_driver_install(cfg.channel, 0, 0));

    // Convert NS timings to ticks
    uint32_t counter_clk_hz = 0;

#if defined(HAS_ESP_IDF_4)
    ESP(rmt_get_counter_clock(cfg.channel, &counter_clk_hz));
#else
    // this emulates the rmt_get_counter_clock() function from ESP-IDF 3.4
    if (RMT.conf_ch[cfg.channel].conf1.ref_always_on == RMT_BASECLK_REF) {
        uint32_t div_cnt = RMT.conf_ch[cfg.channel].conf0.div_cnt;
        uint32_t div = div_cnt == 0 ? 256 : div_cnt;
        counter_clk_hz = REF_CLK_FREQ / (div);
    } else {
        uint32_t div_cnt = RMT.conf_ch[cfg.channel].conf0.div_cnt;
        uint32_t div = div_cnt == 0 ? 256 : div_cnt;
        counter_clk_hz = APB_CLK_FREQ / (div);
    }
#endif

    // NS to tick converter
    float ratio = (float)counter_clk_hz / 1e9;

    //if (is800KHz) {
        t0h_ticks = (uint32_t)(ratio * WS2812_T0H_NS);
        t0l_ticks = (uint32_t)(ratio * WS2812_T0L_NS);
        t1h_ticks = (uint32_t)(ratio * WS2812_T1H_NS);
        t1l_ticks = (uint32_t)(ratio * WS2812_T1L_NS);
    //} else {
    //    t0h_ticks = (uint32_t)(ratio * WS2811_T0H_NS);
    //    t0l_ticks = (uint32_t)(ratio * WS2811_T0L_NS);
    //    t1h_ticks = (uint32_t)(ratio * WS2811_T1H_NS);
    //    t1l_ticks = (uint32_t)(ratio * WS2811_T1L_NS);
    //}

    // Initialize automatic timing translator
    ESP(rmt_translator_init(cfg.channel, ws2812_rmt_adapter));
}

void LedDriver::write(uint8_t chan, const uint8_t *data, size_t sz) {
    // Write
    ESP(rmt_write_sample(static_cast<rmt_channel_t>(chan), data, sz, true));
}

void LedDriver::wait(uint8_t chan) {
    // wait to finish
    ESP(rmt_wait_tx_done(static_cast<rmt_channel_t>(chan), pdMS_TO_TICKS(100)));
}

void LedDriver::done(uint8_t chan, uint8_t pin) {
    // Free channel again
    ESP(rmt_driver_uninstall(static_cast<rmt_channel_t>(chan)));

    ESP(gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT));
}

void LedDriver::make(uint8_t pin, const uint8_t *data, size_t sz) {
    init(4, pin);
    write(4, data, sz);
    wait(4);
    done(4, pin);
}
