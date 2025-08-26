#pragma once

#define BTN_PIN     0

#define LED_DI_PIN     40
#define LED_CI_PIN     39

#define TFT_CS_PIN     4
#define TFT_SDA_PIN    3
#define TFT_SCL_PIN    5
#define TFT_DC_PIN     2
#define TFT_RES_PIN    1
#define TFT_LEDA_PIN   38

#define SD_MMC_D0_PIN  14
#define SD_MMC_D1_PIN  17
#define SD_MMC_D2_PIN  21
#define SD_MMC_D3_PIN  18
#define SD_MMC_CLK_PIN 12
#define SD_MMC_CMD_PIN 16

#define USB_HOST_PRIORITY (20)

/*
 * Source https://github.com/espressif/esp-usb/blob/master/host/class/cdc/usb_host_cp210x_vcp/usb_host_cp210x_vcp.c
 */
#define CP210X_WRITE_REQ (USB_BM_REQUEST_TYPE_TYPE_VENDOR | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT)
#define CP210X_CMD_SET_LINE_CTL (0x03) // Set the line control
#define CP210X_CMD_SET_BAUDRATE (0x1E) // Set the baud rate
