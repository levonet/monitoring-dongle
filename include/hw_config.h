#pragma once

#include "driver/gpio.h"

// Button hardware configuration:
#define BTN_PIN         GPIO_NUM_0

// RGB led hardware configuration:
#define LED_DI_PIN      GPIO_NUM_40
#define LED_CI_PIN      GPIO_NUM_39

// Display (ST7735s) hardware configuration:
#define DISPLAY_RST     GPIO_NUM_1
#define DISPLAY_DC      GPIO_NUM_2
#define DISPLAY_MOSI    GPIO_NUM_3
#define DISPLAY_CS      GPIO_NUM_4
#define DISPLAY_SCLK    GPIO_NUM_5
#define DISPLAY_LEDA    GPIO_NUM_38
#define DISPLAY_MISO    GPIO_NUM_NC
#define DISPLAY_BUSY    GPIO_NUM_NC
#define DISPLAY_WIDTH   160
#define DISPLAY_HEIGHT  80

// USB configuration:
#define USB_HOST_PRIORITY (20)

/*
 * Source https://github.com/espressif/esp-usb/blob/master/host/class/cdc/usb_host_cp210x_vcp/usb_host_cp210x_vcp.c
 */
#define CP210X_WRITE_REQ (USB_BM_REQUEST_TYPE_TYPE_VENDOR | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT)
#define CP210X_CMD_SET_LINE_CTL (0x03) // Set the line control
#define CP210X_CMD_SET_BAUDRATE (0x1E) // Set the baud rate
