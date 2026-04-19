/*
 * config.h - Hardware pin assignments and tunable settings
 *
 * Waveshare RP2040 Zero pinout used here:
 *
 *  SSD1351 OLED (SPI0)
 *  ───────────────────
 *  OLED SCLK  → GPIO 2  (SPI0 SCK)
 *  OLED MOSI  → GPIO 3  (SPI0 TX)
 *  OLED CS    → GPIO 5  (SPI0 CSn)
 *  OLED DC    → GPIO 6
 *  OLED RST   → GPIO 7
 *
 *  Buttons (active LOW, internal pull-up)
 *  ──────────────────────────────────────
 *  Button A (Left)   → GPIO 8
 *  Button B (Middle) → GPIO 9
 *  Button C (Right)  → GPIO 10
 *
 *  Buzzer / speaker (PWM)
 *  ──────────────────────
 *  Buzzer +  → GPIO 11  (PWM5 A)
 *  Buzzer -  → GND
 *
 * Adjust any of the above to match your wiring.
 * 
 * TODO: Add buttons for save/load state and reset
 */

#pragma once

// ── SPI / Display ────────────────────────────────────────────────────────────
#define PIN_OLED_SCLK   2
#define PIN_OLED_MOSI   3
#define PIN_OLED_CS     5
#define PIN_OLED_DC     6
#define PIN_OLED_RST    7

#define OLED_WIDTH      128
#define OLED_HEIGHT     128
#define SPI_FREQ        20000000UL   // 20 MHz — SSD1351 max is ~20 MHz

// ── Buttons ──────────────────────────────────────────────────────────────────
#define PIN_BTN_A       8    // Left  (Tamagotchi button A)
#define PIN_BTN_B       9    // Middle (Tamagotchi button B)
#define PIN_BTN_C       10   // Right  (Tamagotchi button C)
#define BTN_DEBOUNCE_MS 20

// ── Audio ─────────────────────────────────────────────────────────────────────
#define PIN_BUZZER      11
// Buzzer volume: 0–255 (controls PWM duty cycle when playing)
#define BUZZER_VOLUME   128

// ── Display scaling & layout ─────────────────────────────────────────────────
// The Tamagotchi LCD is 32x16 pixels. We scale it up to fill a nice area of
// the 128x128 display.
//
//  SCALE        → each pixel becomes a SCALE×SCALE block
//  LCD_OFFSET_X → left edge of the scaled LCD on screen (pixels)
//  LCD_OFFSET_Y → top edge of the scaled LCD on screen (pixels)
//
#define LCD_SCALE       4
#define LCD_OFFSET_X    0
#define LCD_OFFSET_Y    32

// ── Colours (RGB565) ─────────────────────────────────────────────────────────
#define COLOUR_BG       0x0000   // Black background
#define COLOUR_PIXEL_ON 0x0000   // Black pixels for the LCD