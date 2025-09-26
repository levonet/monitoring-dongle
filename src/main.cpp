#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <regex.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_err.h>

#include "nvs_flash.h"
#include "PicoSyslog.h"
#include "FastLED.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "button.h"

#include "hw_config.h"
#include "lgfx_dongle.h"
#include "wifi_station.h"
#include "info_helper.h"
#include "serializer.h"
#include "ring_buffer.h"

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define RX_RING_BUFFER 4096

CRGB leds;
static LGFX_Dongle lcd;
PicoSyslog::Logger syslog("dongle");
static button_t btn;
TimerHandle_t getNameTimer;
TimerHandle_t rxTimer;
SemaphoreHandle_t rxSemaphore;
ring_buffer_t rxbuf;
regex_t regex;
int reti;
nvs_handle_t hdlNvs;

boolean isWiFiConnected = false;
boolean isUSBConnected = false;
static int64_t shineTime = 0;
static cdc_acm_dev_hdl_t cdc_dev = NULL;
static char tag_name_rx[64];
static char tag_name_nvs[64];

static bool handle_cdc_rx(const uint8_t *data, size_t data_len, void *arg);
static void handle_cdc_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx);

const cdc_acm_host_device_config_t dev_config = {
    .connection_timeout_ms = 5000, // 5 seconds, enough time to plug the device
    .out_buffer_size = 512,
    .in_buffer_size = 1024,        // The maximum packet size for SuperSpeed USB
    .event_cb = handle_cdc_event,
    .data_cb = handle_cdc_rx,
    .user_arg = NULL,
};

const char *nvsGetKey(const char *key, char *value, const char *defaultVal) {
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(hdlNvs, key, NULL, &required_size);
    if (err != ESP_OK) {
        return defaultVal;
    }

    err = nvs_get_str(hdlNvs, key, value, &required_size);
    if (err == ESP_OK) {
        return value;
    }

    return defaultVal;
}

static void draw_info(const char* ssid, const char* server, const char* tag) {
    lcd.clear();
    lcd.setCursor(2, 12);
    lcd.printf("  SSID: %s", ssid);
    lcd.setCursor(2, 24);
    lcd.printf("SYSLOG: %s", server);
    lcd.setCursor(2, 36);
    lcd.printf("   TAG: %s", tag);
}

void line_transmit(unsigned char* line) {
    if (is_empty(line)) {
        return;
    }

    syslog.information.printf("%s\n", rtrim(line));

    if (xTimerIsTimerActive(getNameTimer) == pdFALSE) {
        return;
    }

    regmatch_t pmatch[2];
    if (!regexec(&regex, (const char*)line, ARRAY_SIZE(pmatch), pmatch, 0)) {
        xTimerStop(getNameTimer, 0);

        sprintf(tag_name_rx, "%.*s", (int)(pmatch[1].rm_eo - pmatch[1].rm_so), line + pmatch[1].rm_so);
        syslog.host = tag_name_rx;
        nvs_set_str(hdlNvs, "dongle_tag_name", tag_name_rx);
        draw_info(CONFIG_ESP_WIFI_SSID, CONFIG_SYSLOG_SERVER, tag_name_rx);
    }
}

static bool handle_cdc_rx(const uint8_t *data, size_t data_len, void *arg) {
    uint16_t i = 0;

    if (xSemaphoreTake(rxSemaphore, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false; // Received data was NOT processed
    }

    if (xTimerIsTimerActive(rxTimer) == pdTRUE) {
        xTimerStop(rxTimer, 0);
    }

    do {
        ring_buffer_append(&rxbuf, (unsigned char)data[i]);
    } while (++i < data_len);

    xSemaphoreGive(rxSemaphore);
    xTimerStart(rxTimer, 0);

    return true;
}

void handle_time_rx_logger(TimerHandle_t xTimer) {
    unsigned char *output;
    uint16_t i = 0;

    uint16_t data_len = ring_buffer_available(&rxbuf);
    if (!data_len) {
        return;
    }

    if (xSemaphoreTake(rxSemaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    output = (unsigned char *)malloc(sizeof(char) * (data_len + 1));
    do {
        output[i] = ring_buffer_read(&rxbuf);
    } while (++i < data_len);
    output[data_len] = '\0';

    xSemaphoreGive(rxSemaphore);

    unsigned char *line = output;
    unsigned char *line_next;
    size_t line_len = data_len + 1;
    do {
        line_next = nextln(line, line_len);

        line_transmit(line);

        if (line_next != NULL) {
            line_len -= line_next - line;
            /* TODO: Замінити PicoSyslog на пряму відправку по udp.
             * Причина: ребутається MCU при послідовному виклику syslog.*.printf().
             */
            vTaskDelay(pdMS_TO_TICKS(148)); // Don't ask
        }
        line = line_next;
    } while (line_next != NULL);

    free(output);
    output = NULL;

    leds = CRGB::DarkOrange;
    FastLED.show();
    shineTime = esp_timer_get_time() + 200000; // 200ms of flashing
}

static void handle_cdc_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx) {
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
    syslog.host = nvsGetKey("dongle_tag_name", tag_name_nvs, CONFIG_SYSLOG_TAG);
    syslog.server = CONFIG_SYSLOG_SERVER;
#ifdef CONFIG_SYSLOG_PORT
    syslog.port = CONFIG_SYSLOG_PORT;
#endif

    run_usb();
}

void wifi_disconnected_cb(void) {
    isWiFiConnected = false;
}

static void handle_button(button_t *btn, button_state_t state) {
    switch (state) {
        case BUTTON_PRESSED: {
                leds = CRGB::White;
                FastLED.show();
                shineTime = esp_timer_get_time() + 1000000; // up to 1s of flashing
                break;
            }
        case BUTTON_RELEASED:
            FastLED.clear();
            shineTime = esp_timer_get_time() + 200000; // 200ms of darkness
            break;
        case BUTTON_CLICKED: {
                if (!isUSBConnected) {
                    break;
                }
                if (xTimerIsTimerActive(getNameTimer) == pdTRUE) {
                    break;
                }

                xTimerStart(getNameTimer, 0);

                const uint8_t msg_name[] = "$Config/Filename\n";
                cdc_acm_host_data_tx_blocking(cdc_dev, msg_name, sizeof(msg_name), 500);
                break;
            }
        case BUTTON_PRESSED_LONG: {
                const uint8_t msg_unlock[] = "$Alarm/Disable\n";
                cdc_acm_host_data_tx_blocking(cdc_dev, msg_unlock, sizeof(msg_unlock), 500);
                break;
            }
        default:
            break;
    }
}

static void handle_timer_get_name(TimerHandle_t xTimer) {
    return;
}

extern "C" void app_main() {
    //Initialize NVS for WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open("storage", NVS_READWRITE, &hdlNvs);
    if (ret != ESP_OK) {
        return;
    }

    getNameTimer = xTimerCreate("nameTimer", pdMS_TO_TICKS(1000), pdFALSE, (void *)0, handle_timer_get_name);
    if (getNameTimer == NULL) {
        return;
    }
    rxTimer = xTimerCreate("rxTimer", pdMS_TO_TICKS(250), pdFALSE, (void *)0, handle_time_rx_logger);
    if (rxTimer == NULL) {
        return;
    }

    FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
    FastLED.setBrightness(64);
    xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);

    if (!lcd.init()) {
        return;
    }
    lcd.setBrightness(96);
    lcd.setTextColor(0x00ff00u);
    draw_info(CONFIG_ESP_WIFI_SSID, CONFIG_SYSLOG_SERVER, nvsGetKey("dongle_tag_name", tag_name_nvs, CONFIG_SYSLOG_TAG));

    btn.gpio = BTN_PIN;
    btn.pressed_level = 0;
    btn.internal_pull = true;
    btn.autorepeat = false;
    btn.callback = handle_button;
    ESP_ERROR_CHECK(button_init(&btn));

    rxSemaphore = xSemaphoreCreateMutex();
    if (rxSemaphore == NULL) {
        return;
    }

    ring_buffer_init(&rxbuf, RX_RING_BUFFER);

    reti = regcomp(&regex, "\\$Config/Filename=(.*)\\.ya?ml$", REG_EXTENDED);
    if (reti) {
        return;
    }

    wifi_init_sta(CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD, wifi_connected_cb, wifi_disconnected_cb);
}
