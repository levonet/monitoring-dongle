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

#include "pin_config.h"

CRGB leds;

const char *networkName = CONFIG_ESP_WIFI_SSID;
const char *networkPswd = CONFIG_ESP_WIFI_PASSWORD;

PicoSyslog::Logger syslog("fluidnc");

boolean isUSBEnabled = false;
boolean isWiFiConnected = false;

#define EXAMPLE_USB_HOST_PRIORITY   (20)
#define EXAMPLE_USB_DEVICE_VID      (0x303A)
#define EXAMPLE_USB_DEVICE_PID      (0x4001) // 0x303A:0x4001 (TinyUSB CDC device)
#define EXAMPLE_USB_DEVICE_DUAL_PID (0x4002) // 0x303A:0x4002 (TinyUSB Dual CDC device)
#define EXAMPLE_TX_STRING           ("CDC test string!")
#define EXAMPLE_TX_TIMEOUT_MS       (1000)

static const char *TAG = "USB-CDC";
static SemaphoreHandle_t device_disconnected_sem;

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg) {
    syslog.information.printf("Data received\n");
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_INFO);
    return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx) {
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        syslog.error.printf("CDC-ACM error has occurred, err_no = %i\n", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        syslog.information.printf("Device suddenly disconnected\n");
        ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        syslog.information.printf("Serial state notif 0x%04X\n", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default:
        syslog.warning.printf("Unsupported CDC event: %i\n", event->type);
        break;
    }
}

static void usb_lib_task(void *arg) {
    while (1) {
        // Start handling system events
        uint32_t event_flags;
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
    syslog.information.printf("Installing USB Host\n");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), EXAMPLE_USB_HOST_PRIORITY, NULL);
    assert(task_created == pdTRUE);

    syslog.information.printf("Installing CDC-ACM driver\n");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = handle_event,
        .data_cb = handle_rx,
        .user_arg = NULL,
    };

    while (1) {
        cdc_acm_dev_hdl_t cdc_dev = NULL;

        // Open USB device from tusb_serial_device example example. Either single or dual port configuration.
        syslog.information.printf("Opening CDC ACM device 0x%04X:0x%04X...\n", EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_PID);
        // ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_PID);
        esp_err_t err = cdc_acm_host_open(EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_PID, 0, &dev_config, &cdc_dev);
        if (ESP_OK != err) {
            syslog.information.printf("Opening CDC ACM device dual 0x%04X:0x%04X...\n", EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_DUAL_PID);
            // ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_DUAL_PID);
            err = cdc_acm_host_open(EXAMPLE_USB_DEVICE_VID, EXAMPLE_USB_DEVICE_DUAL_PID, 0, &dev_config, &cdc_dev);
            if (ESP_OK != err) {
                syslog.error.println("Failed to open device");
                // ESP_LOGI(TAG, "Failed to open device");
                continue;
            }
        }
        cdc_acm_host_desc_print(cdc_dev);
        vTaskDelay(pdMS_TO_TICKS(100));

        // Test sending and receiving: responses are handled in handle_rx callback
        ESP_ERROR_CHECK(cdc_acm_host_data_tx_blocking(cdc_dev, (const uint8_t *)EXAMPLE_TX_STRING, strlen(EXAMPLE_TX_STRING), EXAMPLE_TX_TIMEOUT_MS));
        vTaskDelay(pdMS_TO_TICKS(100));

        // Test Line Coding commands: Get current line coding, change it 9600 7N1 and read again
        syslog.information.printf("Setting up line coding\n");
        // ESP_LOGI(TAG, "Setting up line coding");

        cdc_acm_line_coding_t line_coding;
        ESP_ERROR_CHECK(cdc_acm_host_line_coding_get(cdc_dev, &line_coding));
        syslog.information.printf("[1] Line Get: Rate: %u, Databits: %u\n", line_coding.dwDTERate, line_coding.bDataBits);
        syslog.information.printf("[1]        Stop bits: %u, Parity: %u\n", line_coding.bCharFormat, line_coding.bParityType);
        // ESP_LOGI(TAG, "Line Get: Rate: %"PRIu32", Stop bits: %"PRIu8", Parity: %"PRIu8", Databits: %"PRIu8"",
        //          line_coding.dwDTERate, line_coding.bCharFormat, line_coding.bParityType, line_coding.bDataBits);

        line_coding.dwDTERate = 115200;
        line_coding.bDataBits = 8;   // 5, 6, 7, 8 or 16
        line_coding.bParityType = 0; // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
        line_coding.bCharFormat = 0; // 0: 1 stopbit, 1: 1.5 stopbits, 2: 2 stopbits
        ESP_ERROR_CHECK(cdc_acm_host_line_coding_set(cdc_dev, &line_coding));
        syslog.information.printf("[2] Line Set: Rate: %u, Databits: %u\n", line_coding.dwDTERate, line_coding.bDataBits);
        syslog.information.printf("[2]        Stop bits: %u, Parity: %u\n", line_coding.bCharFormat, line_coding.bParityType);
        // ESP_LOGI(TAG, "Line Set: Rate: %"PRIu32", Stop bits: %"PRIu8", Parity: %"PRIu8", Databits: %"PRIu8"",
        //          line_coding.dwDTERate, line_coding.bCharFormat, line_coding.bParityType, line_coding.bDataBits);

        ESP_ERROR_CHECK(cdc_acm_host_line_coding_get(cdc_dev, &line_coding));
        syslog.information.printf("[3] Line Get: Rate: %u, Databits: %u\n", line_coding.dwDTERate, line_coding.bDataBits);
        syslog.information.printf("[3]        Stop bits: %u, Parity: %u\n", line_coding.bCharFormat, line_coding.bParityType);
        // ESP_LOGI(TAG, "Line Get: Rate: %"PRIu32", Stop bits: %"PRIu8", Parity: %"PRIu8", Databits: %"PRIu8"",
        //          line_coding.dwDTERate, line_coding.bCharFormat, line_coding.bParityType, line_coding.bDataBits);

        ESP_ERROR_CHECK(cdc_acm_host_set_control_line_state(cdc_dev, true, false));

        // We are done. Wait for device disconnection and start over
        syslog.information.printf("Example finished successfully! You can reconnect the device to run again.\n");
        // ESP_LOGI(TAG, "Example finished successfully! You can reconnect the device to run again.");
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
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
      syslog.information.println("Hello");
      isWiFiConnected = true;
      if (!isUSBEnabled) {


        isUSBEnabled = true;
      }

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

    connectToWiFi(networkName, networkPswd);
    syslog.forward_to = nullptr;
    syslog.host = "fluidnc001-3";
    syslog.server = "143.244.207.191";

    delay(500);

    // run_usb();

    while(1) {
        delay(100);
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

  connectToWiFi(networkName, networkPswd);
  syslog.forward_to = nullptr;
  syslog.host = "fluidnc001-3";
  syslog.server = "143.244.207.191";
 
  delay(500);
}

void loop() {
    delay(300);

    while (!isUSBEnabled) {
        delay(100);
    }

    run_usb();
}
#endif



