// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// Released under the GPLv3 license to match the rest of the
// Adafruit NeoPixel library

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

// Which pin on the Arduino is connected to the NeoPixels?
#define PIN        27 // On Trinket or Gemma, suggest changing this to 1

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 20 // Popular NeoPixel ring size

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels[] = {
    Adafruit_NeoPixel(NUMPIXELS, 27, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 25, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 26, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 32, NEO_GRB + NEO_KHZ800),
};

#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels

void setup() {
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  // END of Trinket-specific code.

    for (auto &p: pixels) p.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
}
struct {
    int16_t v;
    int16_t p;
} mall[] = {
    {  0, 10 },
    {  150, 10 },
    {  200, -10 },
};

void loop() {
    static int n=0;

    //for (auto &m: mall) {
    //    m.v += m.p;
    //    if ((m.v >= 254) || (m.v <= 0)) m.p *= -1;
    //}

    //Serial.printf("v: %d %d %d\n", mall[0].v, mall[1].v, mall[2].v);
    //pixels.setPixelColor(n, pixels.Color(mall[0].v, mall[1].v, mall[2].v));
    //for (n = 1; n<=30; n++)
    //    pixels.setPixelColor(n, pixels.Color(mall[0].v, mall[1].v, mall[2].v));

    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } led[] = {
        { 0xff, 0x00, 0x00 },
        { 0x00, 0xff, 0x00 },
        { 0x00, 0x00, 0xff },
        { 0xff, 0xff, 0x00 },
        { 0xff, 0x00, 0xff },
        { 0x00, 0xff, 0xff },
        { 0xff, 0xff, 0xff },
        { 0xff, 0xff, 0xff },
        { 0xff, 0xff, 0xff },
        { 0xff, 0xff, 0xff },
        { 0x00, 0x00, 0x00 },
    };

    for (const auto &l: led) {
        for (n = 0; n<=20; n++)
            for (auto &p: pixels)
                p.setPixelColor(n, p.Color(l.r, l.g, l.b));
        
        for (auto &p: pixels)
            p.show();
        delay(2000);
    }

    //pixels.show();

    //n++;
    //if (n >= 30) n=0;

    //delay(200);
}
