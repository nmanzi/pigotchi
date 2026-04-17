/*
 * main.cpp
 * TamaLIB on RP2040
 * Nathan Manzi <nathan@nmanzi.com>
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <EEPROM.h>

// wrap TamaLIB
extern "C" {
#include "tamalib.h"
#include "hal.h"
#include "cpu.h"
}

#include "config.h"
#include "rom.h"

// include background and icon bitmaps as arrays of RGB565 pixels
#include "img/background.h"
#include "img/icon_feed.h"
#include "img/icon_bathroom.h"
#include "img/icon_medicine.h"
#include "img/icon_discipline.h"
#include "img/icon_attention.h"
#include "img/icon_game.h"
#include "img/icon_light.h"
#include "img/icon_meter.h"
 
// ssd1351 over SPI
Adafruit_SSD1351 oled(
    OLED_WIDTH, OLED_HEIGHT,
    &SPI,
    PIN_OLED_CS,
    PIN_OLED_DC,
    PIN_OLED_RST
);
 
// emulated lcd matrix and framebuffer
static bool lcd_matrix[32][16];
static bool lcd_icons[8];
static bool screen_dirty = false;
static uint16_t framebuf[OLED_HEIGHT][OLED_WIDTH];
 
// button state tracking for debouncing
struct ButtonState {
    uint8_t  pin;
    bool     last_raw;
    bool     debounced;
    uint32_t last_change_ms;
};
 
static ButtonState buttons[3] = {
    { PIN_BTN_A, true, true, 0 },
    { PIN_BTN_B, true, true, 0 },
    { PIN_BTN_C, true, true, 0 },
};

// icon mapping
static const uint16_t* icons[8] = {
    icon_feed, icon_light, icon_game, icon_medicine,
    icon_bathroom, icon_meter, icon_discipline, icon_attention
};

static const uint16_t icon_src_w[8] = {
    icon_feed_width, icon_light_width, icon_game_width, icon_medicine_width,
    icon_bathroom_width, icon_meter_width, icon_discipline_width, icon_attention_width
};
static const uint16_t icon_src_h[8] = {
    icon_feed_height, icon_light_height, icon_game_height, icon_medicine_height,
    icon_bathroom_height, icon_meter_height, icon_discipline_height, icon_attention_height
};

// audio state tracking
static uint32_t current_freq_dhz = 0;   // frequency in dHz (tenths of Hz)
static bool     audio_playing    = false;

// save data tracking

#define SAVE_MAGIC 0xABCD
typedef struct {
    uint16_t magic;          // should be 0xABCD to indicate valid save data
    uint16_t pc, x, y;
    uint8_t  a, b, np, sp, flags;
    uint32_t tick_counter;
    uint32_t clk_2hz, clk_4hz, clk_8hz, clk_16hz;
    uint32_t clk_32hz, clk_64hz, clk_128hz, clk_256hz;
    uint32_t prog_timer_ts;
    uint8_t  prog_timer_enabled, prog_timer_data, prog_timer_rld;
    uint32_t call_depth;
    struct { uint8_t factor_flag, mask, triggered, vector; } interrupts[INT_SLOT_NUM];
    uint8_t  cpu_halted;
    uint8_t  memory[MEM_BUFFER_SIZE];
} save_state_t;

// ─────────────────────────────────────────────────────────────────────────────
// Save data management (stored in EEPROM)
// ─────────────────────────────────────────────────────────────────────────────

static void state_save(void)
{
    state_t *s = tamalib_get_state();
    save_state_t slot = {};

    slot.magic             = SAVE_MAGIC;
    slot.pc                = *s->pc;
    slot.x                 = *s->x;
    slot.y                 = *s->y;
    slot.a                 = *s->a;
    slot.b                 = *s->b;
    slot.np                = *s->np;
    slot.sp                = *s->sp;
    slot.flags             = *s->flags;
    slot.tick_counter      = *s->tick_counter;
    slot.clk_2hz           = *s->clk_timer_2hz_timestamp;
    slot.clk_4hz           = *s->clk_timer_4hz_timestamp;
    slot.clk_8hz           = *s->clk_timer_8hz_timestamp;
    slot.clk_16hz          = *s->clk_timer_16hz_timestamp;
    slot.clk_32hz          = *s->clk_timer_32hz_timestamp;
    slot.clk_64hz          = *s->clk_timer_64hz_timestamp;
    slot.clk_128hz         = *s->clk_timer_128hz_timestamp;
    slot.clk_256hz         = *s->clk_timer_256hz_timestamp;
    slot.prog_timer_ts     = *s->prog_timer_timestamp;
    slot.prog_timer_enabled= *s->prog_timer_enabled;
    slot.prog_timer_data   = *s->prog_timer_data;
    slot.prog_timer_rld    = *s->prog_timer_rld;
    slot.call_depth        = *s->call_depth;
    slot.cpu_halted        = *s->cpu_halted;
    for (int i = 0; i < INT_SLOT_NUM; i++) {
        slot.interrupts[i].factor_flag = s->interrupts[i].factor_flag_reg;
        slot.interrupts[i].mask        = s->interrupts[i].mask_reg;
        slot.interrupts[i].triggered   = s->interrupts[i].triggered;
        slot.interrupts[i].vector      = s->interrupts[i].vector;
    }
    memcpy(slot.memory, s->memory, MEM_BUFFER_SIZE);

    EEPROM.put(0, slot);
    EEPROM.commit();
    Serial.println("[tamalib] State saved.");
}

static void state_load(void)
{
    save_state_t slot;
    EEPROM.get(0, slot);
    if (slot.magic != SAVE_MAGIC) {
        Serial.println("[tamalib] No valid save found.");
        return;
    }

    state_t *s = tamalib_get_state();

    *s->pc                       = slot.pc;
    *s->x                        = slot.x;
    *s->y                        = slot.y;
    *s->a                        = slot.a;
    *s->b                        = slot.b;
    *s->np                       = slot.np;
    *s->sp                       = slot.sp;
    *s->flags                    = slot.flags;
    *s->tick_counter             = slot.tick_counter;
    *s->clk_timer_2hz_timestamp  = slot.clk_2hz;
    *s->clk_timer_4hz_timestamp  = slot.clk_4hz;
    *s->clk_timer_8hz_timestamp  = slot.clk_8hz;
    *s->clk_timer_16hz_timestamp = slot.clk_16hz;
    *s->clk_timer_32hz_timestamp = slot.clk_32hz;
    *s->clk_timer_64hz_timestamp = slot.clk_64hz;
    *s->clk_timer_128hz_timestamp= slot.clk_128hz;
    *s->clk_timer_256hz_timestamp= slot.clk_256hz;
    *s->prog_timer_timestamp     = slot.prog_timer_ts;
    *s->prog_timer_enabled       = slot.prog_timer_enabled;
    *s->prog_timer_data          = slot.prog_timer_data;
    *s->prog_timer_rld           = slot.prog_timer_rld;
    *s->call_depth               = slot.call_depth;
    *s->cpu_halted               = slot.cpu_halted;
    for (int i = 0; i < INT_SLOT_NUM; i++) {
        s->interrupts[i].factor_flag_reg = slot.interrupts[i].factor_flag;
        s->interrupts[i].mask_reg        = slot.interrupts[i].mask;
        s->interrupts[i].triggered       = slot.interrupts[i].triggered;
        // vector is a fixed hardware value; restoring it is harmless but optional
    }
    memcpy(s->memory, slot.memory, MEM_BUFFER_SIZE);

    tamalib_refresh_hw();  // sync LCD/buzzer output with restored state
    Serial.println("[tamalib] State loaded.");
}

// ─────────────────────────────────────────────────────────────────────────────
// HAL - Memory
// ─────────────────────────────────────────────────────────────────────────────
 
static void* hal_malloc(u32_t size)
{
    return malloc(size);
}
 
static void hal_free(void *ptr)
{
    free(ptr);
}
 
// ─────────────────────────────────────────────────────────────────────────────
// HAL - Halt
// ─────────────────────────────────────────────────────────────────────────────
 
static void hal_halt(void)
{
    // CPU halted, spin forever (or add a reset / error screen here)
    Serial.println("[tamalib] CPU HALTED");
    while (true) {
        tight_loop_contents();
    }
}
 
// ─────────────────────────────────────────────────────────────────────────────
// HAL - Logging (routes to Serial)
// ─────────────────────────────────────────────────────────────────────────────
 
static bool_t hal_is_log_enabled(log_level_t level)
{
    // Enable ERROR and INFO; disable MEMORY/CPU/INT to keep serial quiet.
    return (level == LOG_ERROR || level == LOG_INFO) ? 1 : 0;
}
 
static void hal_log(log_level_t level, char *fmt, ...)
{
    if (!hal_is_log_enabled(level)) return;
 
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
 
    Serial.print("[tamalib] ");
    Serial.println(buf);
}
 
// ─────────────────────────────────────────────────────────────────────────────
// HAL - Timing
// ─────────────────────────────────────────────────────────────────────────────
 
static void hal_sleep_until(timestamp_t ts)
{
    while ((int32_t)(ts - (timestamp_t)micros()) > 0) {
        tight_loop_contents();
    }
}
 
static timestamp_t hal_get_timestamp(void)
{
    return (timestamp_t)micros();
}
 
// ─────────────────────────────────────────────────────────────────────────────
// HAL - Screen
// ─────────────────────────────────────────────────────────────────────────────
 
static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
    if (x < 32 && y < 16) {
        lcd_matrix[x][y] = (val != 0);
        screen_dirty = true;
    }
}
 
static void hal_set_lcd_icon(u8_t icon, bool_t val)
{
    if (icon < 8) {
        lcd_icons[icon] = (val != 0);
        screen_dirty = true;
    }
}
 
static void hal_update_screen(void)
{
    if (!screen_dirty) return;
    screen_dirty = false;

    // Render LCD pixels
    for (uint8_t x = 0; x < 32; x++) {
        for (uint8_t y = 0; y < 16; y++) {
            // Map each logical pixel to the block of screen pixels it covers
            int16_t x0 = LCD_OFFSET_X + (int16_t)(x * LCD_SCALE);
            int16_t x1 = LCD_OFFSET_X + (int16_t)((x + 1) * LCD_SCALE);
            int16_t y0 = LCD_OFFSET_Y + (int16_t)(y * LCD_SCALE);
            int16_t y1 = LCD_OFFSET_Y + (int16_t)((y + 1) * LCD_SCALE);
            for (int16_t py = y0; py < y1; py++) {
                for (int16_t px = x0; px < x1; px++) {
                    framebuf[py][px] = lcd_matrix[x][y]
                        ? COLOUR_PIXEL_ON
                        : background_img[py * OLED_WIDTH + px];
                }
            }
        }
    }

    // Render Icons
    for (uint8_t i = 0; i < 8; i++) {
        if (!lcd_icons[i]) {
            // clear icon area to background if icon is off
            uint8_t col = (uint8_t)(i % 4);
            uint8_t row = (uint8_t)(i / 4);
            int16_t cx = (int16_t)(col * 32 + 16);
            int16_t cy = (row == 0) ? 16 : (OLED_HEIGHT - 16);
            int16_t x0 = cx - 16;
            int16_t y0 = cy - 16;
            for (int16_t py = y0; py < y0 + 32; py++) {
                for (int16_t px = x0; px < x0 + 32; px++) {
                    if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
                        framebuf[py][px] = background_img[py * OLED_WIDTH + px];
                    }
                }
            }
        } else {
            // draw icon from source bitmap
            uint16_t sw = icon_src_w[i];
            uint16_t sh = icon_src_h[i];

            uint8_t col = i % 4;
            uint8_t row = i / 4;

            int16_t cx = (int16_t)(col * 32 + 16); // icon center x
            int16_t cy = (row == 0) ? 16 : (OLED_HEIGHT - 16); // icon center y (16 for top row, 112 for bottom row)

            int16_t x0 = cx - (int16_t)(sw / 2);
            int16_t y0 = cy - (int16_t)(sh / 2);

            for (int16_t dy = 0; dy < (int16_t)sh; dy++) {
                int16_t py = y0 + dy;
                if (py < 0 || py >= OLED_HEIGHT) continue;
                uint16_t sy = (uint16_t)dy * sh / sh;
                for (int16_t dx = 0; dx < (int16_t)sw; dx++) {
                    int16_t px = x0 + dx;
                    if (px < 0 || px >= OLED_WIDTH) continue;
                    uint16_t sx = (uint16_t)dx * sw / sw;
                    uint16_t colour = icons[i][sy * sw + sx];
                    framebuf[py][px] = colour;
                }
            }
        }
    }

    // Write the framebuffer to the display
    oled.drawRGBBitmap(0, 0, (uint16_t*)framebuf, OLED_WIDTH, OLED_HEIGHT);
}
 
// ─────────────────────────────────────────────────────────────────────────────
// HAL - Audio
// ─────────────────────────────────────────────────────────────────────────────
//
// TamaLIB passes frequency in dHz (tenths of Hz), e.g. 4400 = 440.0 Hz.
// tone() takes Hz as a uint32_t.
 
static void hal_set_frequency(u32_t freq_dhz)
{
    current_freq_dhz = freq_dhz;
    if (audio_playing && freq_dhz > 0) {
        tone(PIN_BUZZER, freq_dhz / 10);
    }
}
 
static void hal_play_frequency(bool_t en)
{
    audio_playing = (en != 0);
    if (audio_playing && current_freq_dhz > 0) {
        tone(PIN_BUZZER, current_freq_dhz / 10);
    } else {
        noTone(PIN_BUZZER);
    }
}
 
// ─────────────────────────────────────────────────────────────────────────────
// HAL - Event handler  (buttons + optional save/load)
// ─────────────────────────────────────────────────────────────────────────────
 
static void debounce_buttons(void)
{
    uint32_t now = millis();
    for (int i = 0; i < 3; i++) {
        bool raw = !digitalRead(buttons[i].pin);  // active LOW → invert
        if (raw != buttons[i].last_raw) {
            buttons[i].last_raw     = raw;
            buttons[i].last_change_ms = now;
        }
        if ((now - buttons[i].last_change_ms) >= BTN_DEBOUNCE_MS) {
            buttons[i].debounced = buttons[i].last_raw;
        }
    }
}
 
static int hal_handler(void)
{
    debounce_buttons();
 
    // Map our three buttons to TamaLIB's BTN_LEFT, BTN_MIDDLE, BTN_RIGHT.
    // tamalib_set_button() takes: button id, pressed state (bool_t)
    tamalib_set_button(BTN_LEFT,   buttons[0].debounced ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    tamalib_set_button(BTN_MIDDLE, buttons[1].debounced ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);
    tamalib_set_button(BTN_RIGHT,  buttons[2].debounced ? BTN_STATE_PRESSED : BTN_STATE_RELEASED);

    // Auto-save whenever BTN_LEFT is released
    if (buttons[0].debounced == BTN_STATE_RELEASED && buttons[0].last_raw == BTN_STATE_PRESSED) {
        state_save();
    }
 
    // Return 0 to keep running, non-zero to stop the main loop.
    return 0;
}
 
// ─────────────────────────────────────────────────────────────────────────────
// wire all the function pointers up
// ─────────────────────────────────────────────────────────────────────────────
 
static hal_t rp2040_hal = {
    .malloc           = hal_malloc,
    .free             = hal_free,
    .halt             = hal_halt,
    .is_log_enabled   = hal_is_log_enabled,
    .log              = hal_log,
    .sleep_until      = hal_sleep_until,
    .get_timestamp    = hal_get_timestamp,
    .update_screen    = hal_update_screen,
    .set_lcd_matrix   = hal_set_lcd_matrix,
    .set_lcd_icon     = hal_set_lcd_icon,
    .set_frequency    = hal_set_frequency,
    .play_frequency   = hal_play_frequency,
    .handler          = hal_handler,
};
 
// ─────────────────────────────────────────────────────────────────────────────
// setup and loop
// ─────────────────────────────────────────────────────────────────────────────
 
void setup()
{
    Serial.begin(115200);
    // Give serial a moment to come up (useful when debugging over USB CDC)
    delay(500);
    Serial.println("TamaLIB RP2040 starting...");

    // ── EEPROM (save data) ─────────────────────────────────────────────────
    EEPROM.begin(sizeof(save_state_t));
    Serial.println("EEPROM initialised.");
 
    // ── Display ───────────────────────────────────────────────────────────
    SPI.begin();
    SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
 
    oled.begin();
    memcpy(framebuf, background_img, sizeof(framebuf));
    oled.drawRGBBitmap(0, 0, (uint16_t*)framebuf, OLED_WIDTH, OLED_HEIGHT);
 
    Serial.println("Display initialised.");
 
    // ── Buttons ───────────────────────────────────────────────────────────
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);
 
    // ── Audio ─────────────────────────────────────────────────────────────
    pinMode(PIN_BUZZER, OUTPUT);
    noTone(PIN_BUZZER);
 
    // ── TamaLIB ───────────────────────────────────────────────────────────
    tamalib_register_hal(&rp2040_hal);
 
    // Initialise with the ROM array from rom.h.
    // Third arg = timestamp resolution in units per second (1000000 = microseconds)
    tamalib_init(tamagotchi_rom, NULL, 1000000);

    // Load saved state from EEPROM
    state_load();
 
    Serial.println("TamaLIB initialised. Starting main loop...");
}
 
void loop()
{
    tamalib_mainloop();
}