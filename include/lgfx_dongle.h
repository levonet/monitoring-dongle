#pragma once

#include "LovyanGFX.hpp"

class LGFX_Dongle : public lgfx::LGFX_Device {
    lgfx::Panel_ST7735S _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX_Dongle(void);

};
