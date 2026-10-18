// Porkchop - Hosyond/LCDWiki E32R40T 4.0-inch CYD
// Board settings reused from mjwgeek/SquachWatch-CYD-4.0 (known-good 4.0 panel).
// LCD ST7796S 320x480 native, application uses 480x320 landscape.
#define USER_SETUP_INFO "Porkchop Hosyond CYD40 ST7796S"
#define ST7796_DRIVER
#define TFT_WIDTH 320
#define TFT_HEIGHT 480
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 27
#define TFT_BACKLIGHT_ON HIGH
#define TFT_RGB_ORDER TFT_BGR
// Test polarity for the owner's 4-inch panel: the previous ON setting
// rendered black as white and green as magenta.  Keep this in the
// experimental branch until the owner verifies red/green/blue at boot.
#define TFT_INVERSION_OFF
// XPT2046 is separately owned by the sketch. Do not define TOUCH_CS here.
#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
