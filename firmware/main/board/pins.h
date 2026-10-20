#pragma once

#include <driver/gpio.h>

// Pin map for Artemis Watch hardware revisions 2 and 3 (efuse BLOCK3 rev byte).
// Source: CircuitMess GC_Artemis-Firmware, main/src/Pins.cpp "Revision2".
namespace pins {

constexpr gpio_num_t BtnUp = GPIO_NUM_1;
constexpr gpio_num_t BtnDown = GPIO_NUM_2;
constexpr gpio_num_t BtnSelect = GPIO_NUM_3;
constexpr gpio_num_t BtnAlt = GPIO_NUM_21;

constexpr gpio_num_t Backlight = GPIO_NUM_33;  // active low
constexpr gpio_num_t Buzzer = GPIO_NUM_47;

constexpr gpio_num_t BattRead = GPIO_NUM_5;
constexpr gpio_num_t BattRefSwitch = GPIO_NUM_4;  // high = 2.5 V reference on BattRead
constexpr gpio_num_t UsbDetect = GPIO_NUM_42;     // high = USB power present
constexpr gpio_num_t ChargeIn = GPIO_NUM_7;       // low = charging
constexpr gpio_num_t ChargeOut = GPIO_NUM_6;

// Power latch. Driving it low cuts power to the whole watch.
// Never touch it except in board::powerOff().
constexpr gpio_num_t PowerDown = GPIO_NUM_37;

constexpr gpio_num_t I2cSda = GPIO_NUM_40;
constexpr gpio_num_t I2cScl = GPIO_NUM_41;

constexpr gpio_num_t TftSck = GPIO_NUM_36;
constexpr gpio_num_t TftMosi = GPIO_NUM_35;
constexpr gpio_num_t TftDc = GPIO_NUM_48;
constexpr gpio_num_t TftRst = GPIO_NUM_34;

constexpr gpio_num_t RgbR = GPIO_NUM_14;
constexpr gpio_num_t RgbG = GPIO_NUM_12;
constexpr gpio_num_t RgbB = GPIO_NUM_13;

constexpr gpio_num_t ImuInt1 = GPIO_NUM_38;
constexpr gpio_num_t ImuInt2 = GPIO_NUM_39;

constexpr gpio_num_t Leds[6] = { GPIO_NUM_46, GPIO_NUM_45, GPIO_NUM_44, GPIO_NUM_43, GPIO_NUM_18, GPIO_NUM_17 };

}  // namespace pins
