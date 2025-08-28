#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_err.h>

#include "nvs_flash.h"
#include "PicoSyslog.h"
#include "FastLED.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

#include "hw_config.h"
#include "lgfx_dongle.h"
#include "wifi_station.h"
#include "info_helper.h"
#include "serializer.h"

CRGB leds;
static LGFX_Dongle lcd;
PicoSyslog::Logger syslog("dongle");

boolean isWiFiConnected = false;
boolean isUSBConnected = false;
static int64_t shineTime = 0;
static cdc_acm_dev_hdl_t cdc_dev = NULL;

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg);
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx);

const cdc_acm_host_device_config_t dev_config = {
    .connection_timeout_ms = 5000, // 5 seconds, enough time to plug the device
    .out_buffer_size = 512,
    .in_buffer_size = 1400,        // Safe RSyslog UDP payload size
    .event_cb = handle_event,
    .data_cb = handle_rx,
    .user_arg = NULL,
};

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg) {
    char *output;

    output = (char *)malloc(sizeof(char) * (data_len + 1));
    strncpy(output, (const char *)data, data_len);
    output[data_len] = '\0';

    syslog.information.printf("%s\n", rtrim(output));
    free(output);
    output = NULL;

    leds = CRGB::DarkOrange;
    FastLED.show();
    shineTime = esp_timer_get_time() + 200000; // 200ms for flashing

    return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx) {
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            syslog.error.printf("USB: CDC-ACM error has occurred, err_no = %i\n", event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            syslog.warning.printf("USB: Device suddenly disconnected\n");
            ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
            break;
        case CDC_ACM_HOST_SERIAL_STATE:
            syslog.information.printf("USB: Serial state notif 0x%04X\n", event->data.serial_state.val);
            break;
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        default:
            syslog.warning.printf("USB: Unsupported CDC event: %i\n", event->type);
            break;
    }
}

void cdc_acm_device_event_cb(usb_device_handle_t dev_hdl) {
    syslog.information.printf("USB: Getting device information\n");

    usb_device_info_t dev_info;
    ESP_ERROR_CHECK(usb_host_device_info(dev_hdl, &dev_info));
    syslog.information.printf("USB:  Bus addr: %d\n", dev_info.dev_addr);
    syslog.information.printf("USB:  %s speed\n", (const char *[]) {"Low", "Full", "High"}[dev_info.speed]);
    syslog.information.printf("USB:  bConfigurationValue %d\n", dev_info.bConfigurationValue);
    if (dev_info.str_desc_manufacturer) {
        char manufacturer[256];
        string_descriptor(dev_info.str_desc_manufacturer, (char *)manufacturer);
        syslog.information.printf("USB:  Manufacturer: %s\n", manufacturer);
    }
    if (dev_info.str_desc_product) {
        char product[256];
        string_descriptor(dev_info.str_desc_product, (char *)product);
        syslog.information.printf("USB:  Product: %s\n", product);
    }
    if (dev_info.str_desc_serial_num) {
        char serial_num[256];
        string_descriptor(dev_info.str_desc_serial_num, (char *)serial_num);
        syslog.information.printf("USB:  Serial Number: %s\n", serial_num);
    }
}

static void usb_lib_task(void *arg) {
    uint32_t event_flags;

    while (1) {
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            syslog.information.printf("USB: All devices freed\n");
        }
    }
}

static void run_usb(void) {
    if (isUSBConnected) {
        syslog.information.printf("USB: USB has already been connected\n");
        return;
    }

    syslog.information.printf("USB: Installing USB Host\n");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    TaskHandle_t usb_lib_task_hdl;
    BaseType_t task_created;
    task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), USB_HOST_PRIORITY, &usb_lib_task_hdl);
    assert(task_created == pdTRUE);
    vTaskDelay(pdMS_TO_TICKS(100));

    syslog.information.printf("USB: Installing CDC-ACM driver\n");
    cdc_acm_host_driver_config_t cdc_config = {
        .driver_task_stack_size = 4096,
        .driver_task_priority = 10,
        .xCoreID = 1,
        .new_dev_cb = cdc_acm_device_event_cb,
    };
    ESP_ERROR_CHECK(cdc_acm_host_install(&cdc_config));
    vTaskDelay(pdMS_TO_TICKS(100));

    syslog.information.printf("USB: Opening serial\n");
    esp_err_t err = cdc_acm_host_open(CDC_HOST_ANY_VID, CDC_HOST_ANY_PID, 0, &dev_config, &cdc_dev);
    if (ESP_OK != err) {
        syslog.error.printf("USB: Failed to open device: %s\n", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    syslog.information.printf("USB: Setting up line coding\n");

    cdc_acm_line_coding_t line_coding = {
        .dwDTERate = 115200,
        .bCharFormat = 0, // 0: 1 stopbit, 1: 1.5 stopbits, 2: 2 stopbits
        .bParityType = 0, // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
        .bDataBits = 8,   // 5, 6, 7, 8 or 16
    };

    cdc_acm_line_coding_t *p_line_coding = &line_coding;
    // cdc_acm_host_send_custom_request(cdc_dev,
    //     USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT,
    //     USB_CDC_REQ_SET_LINE_CODING,
    //     0,
    //     0, //cdc_dev->notif.intf_desc->bInterfaceNumber,
    //     sizeof(cdc_acm_line_coding_t),
    //     (uint8_t *)p_line_coding);

    cdc_acm_host_send_custom_request(cdc_dev,
        CP210X_WRITE_REQ,
        CP210X_CMD_SET_BAUDRATE,
        0,
        0,
        sizeof(p_line_coding->dwDTERate),
        (uint8_t *)&p_line_coding->dwDTERate);

    const uint16_t wValue = p_line_coding->bCharFormat | (p_line_coding->bParityType << 4) | (p_line_coding->bDataBits << 8);
    cdc_acm_host_send_custom_request(cdc_dev,
        CP210X_WRITE_REQ,
        CP210X_CMD_SET_LINE_CTL,
        wValue,
        0,
        0,
        NULL);

    isUSBConnected = true;
    syslog.information.printf("USB: ON\n");
}

void led_task(void *param) {
    bool blink = true;
    while (1) {
        blink = !blink;
        if (isWiFiConnected) {
            if (shineTime < esp_timer_get_time()) {
                leds = CRGB::DarkGreen;
            }
        } else {
            leds = blink ? CRGB::DarkRed : CRGB::Black;
        }
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(95));
    }
}

void wifi_connected_cb(void) {
    syslog.information.printf("WiFI: ON\n");
    isWiFiConnected = true;

    syslog.forward_to = nullptr;
    syslog.host = CONFIG_SYSLOG_TAG;
    syslog.server = CONFIG_SYSLOG_SERVER;

    run_usb();
}

void wifi_disconnected_cb(void) {
    isWiFiConnected = false;
}

extern "C" void app_main() {
    //Initialize NVS for WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
    FastLED.setBrightness(64);
    xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);

    if (!lcd.init()) {
        return;
    }
    lcd.setBrightness(96);
    lcd.setTextColor(0x00ff00u);
    lcd.setCursor(2,12);
    lcd.printf("SSID: %s", CONFIG_ESP_WIFI_SSID);
    lcd.setCursor(2,24);
    lcd.printf(" TAG: %s", CONFIG_SYSLOG_TAG);

    wifi_init_sta(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD, wifi_connected_cb, wifi_disconnected_cb);
}
