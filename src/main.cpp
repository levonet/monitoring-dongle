#include <Arduino.h>
#include <WiFi.h>
#include <PicoSyslog.h>
#include <FastLED.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"


#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

#include "hw_config.h"
#include "info_helper.h"

CRGB leds;

const char *networkSSID = CONFIG_ESP_WIFI_SSID;
const char *networkPswd = CONFIG_ESP_WIFI_PASSWORD;

PicoSyslog::Logger syslog("dongle");

boolean isWiFiConnected = false;

#define USB_HOST_PRIORITY (20)

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg);
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx);

const cdc_acm_host_device_config_t dev_config = {
    .connection_timeout_ms = 5000, // 5 seconds, enough time to plug the device in or experiment with timeout
    .out_buffer_size = 512,
    .in_buffer_size = 512,
    .event_cb = handle_event,
    .data_cb = handle_rx,
    .user_arg = NULL,
};

static cdc_acm_dev_hdl_t cdc_dev = NULL;
static SemaphoreHandle_t device_disconnected_sem;


static bool handle_rx(const uint8_t *data, size_t data_len, void *arg) {
    char output[512];
    strncpy(output, (const char *)data, data_len);
    output[data_len] = '\0';

    syslog.information.printf("%s\n", rtrim(output));

    return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx) {
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            syslog.error.printf("USB: CDC-ACM error has occurred, err_no = %i\n", event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            syslog.information.printf("USB: Device suddenly disconnected\n");
            ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
            xSemaphoreGive(device_disconnected_sem);
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
    // syslog.information.printf("USB:  Parent info:\n");
    // if (dev_info.parent.dev_hdl) {
    //     usb_device_info_t parent_dev_info;
    //     ESP_ERROR_CHECK(usb_host_device_info(dev_info.parent.dev_hdl, &parent_dev_info));
    //     syslog.information.printf("USB:    Bus addr: %d\n", parent_dev_info.dev_addr);
    //     syslog.information.printf("USB:    Port: %d\n", dev_info.parent.port_num);
    // } else {
    //     syslog.information.printf("USB:    Port: ROOT\n");
    // }
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
        // Start handling system events
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            syslog.information.printf("USB: All devices freed\n");
            // Continue handling USB events to allow device reconnection
        }
    }
}

static void run_usb(void) {
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);
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
    // ulTaskNotifyTake(false, 1000); // Wait until the USB host library is installed

    vTaskDelay(100); // Add a short delay to let the tasks run

    syslog.information.printf("USB: Installing CDC-ACM driver\n");
    cdc_acm_host_driver_config_t cdc_config = {
        .driver_task_stack_size = 4096,
        .driver_task_priority = 10,
        .xCoreID = 1,
        .new_dev_cb = cdc_acm_device_event_cb,
    };
    ESP_ERROR_CHECK(cdc_acm_host_install(&cdc_config));

    vTaskDelay(100); // Add a short delay to let the tasks run


    syslog.information.printf("USB: Opening serial\n");
    esp_err_t err = cdc_acm_host_open(CDC_HOST_ANY_VID, CDC_HOST_ANY_PID, 0, &dev_config, &cdc_dev);
    if (ESP_OK != err) {
        syslog.error.println("USB: Failed to open device");
        // ???
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

    syslog.information.printf("USB: GO\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



void led_task(void *param) {
  while (1) {
    static uint8_t hue = 0;
    if (isWiFiConnected) {
      leds = CHSV(hue++, 0XFF, 100);
    } else {
      leds = CRGB::Black;
    }
    FastLED.show();
    delay(50);
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      isWiFiConnected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      isWiFiConnected = false;
      break;
    default: break;
  }
}

void connectToWiFi(const char *ssid, const char *pwd) {
  WiFi.disconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(ssid, pwd);
}

#if !CONFIG_AUTOSTART_ARDUINO
void arduinoTask(void *pvParameter) {
    FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
    xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);

    connectToWiFi(networkSSID, networkPswd);
    syslog.forward_to = nullptr;
    syslog.host = CONFIG_SYSLOG_TAG;
    syslog.server = CONFIG_SYSLOG_SERVER;

    delay(500);

    run_usb();

    while(1) {
        delay(10);
    }
}

extern "C" void app_main() {
    initArduino();

    xTaskCreate(&arduinoTask, "arduino_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}
#else

void setup() {
  FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
  xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);

  connectToWiFi(networkSSID, networkPswd);
  syslog.forward_to = nullptr;
  syslog.host = CONFIG_SYSLOG_TAG;
  syslog.server = CONFIG_SYSLOG_SERVER;
 
  delay(500);
}

void loop() {
    delay(300);

    while (!isWiFiConnected) {
        delay(100);
    }

    run_usb();
}
#endif



