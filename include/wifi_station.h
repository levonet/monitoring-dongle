#pragma once

typedef void (*wifi_cb_t)();

void wifi_init_sta(const char *ssid, const char *password, wifi_cb_t callback_connected = NULL, wifi_cb_t callback_fail = NULL);
