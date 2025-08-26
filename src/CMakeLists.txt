list(APPEND srcs "main.cpp"
                 "info_helper.cpp"
                 "wifi_station.cpp"
                 )

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES usb esp_wifi nvs_flash
                    )
