
#pragma once

#include "usb/usb_types_ch9.h"

char *rtrim(char *s);

void string_descriptor(const usb_str_desc_t *str_desc, char *str);
