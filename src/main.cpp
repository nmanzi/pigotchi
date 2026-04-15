/*
 * main.cpp
 * TamaLIB on RP2040
 * Nathan Manzi <nathan@nmanzi.com>
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

// wrap TamaLIB
extern "C" {
#include "tamalib.h"
#include "hal.h"
}

