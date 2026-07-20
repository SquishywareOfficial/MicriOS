#pragma once

#define USER_SETUP_LOADED

// ESP32-2432S028R panels use TFT_eSPI's alternate ILI9341 init sequence.
// The standard ILI9341 initializer can report 320x240 in software while the
// controller remains in a portrait address window.
#define ILI9341_2_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Backlight GPIO21 is owned by CydHardware so it can remain attached to LEDC
// PWM. Defining TFT_BL here makes TFT_eSPI reclaim the pin during init().

// This ESP32-2432S028R panel uses inverted controller polarity. The exact
// working board profile (including NerdMiner's CYD target) requires INVON for
// requested TFT_BLACK pixels to appear black on the physical display.
#define TFT_INVERSION_ON

// The CYD display is wired to HSPI. Touch has its own VSPI bus.
#define USE_HSPI_PORT

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  16000000
