#include <ctype.h>
#include <string.h>

#include "info_helper.h"

char *rtrim(char *s) {
    char* back = s + strlen(s);
    while (isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

void string_descriptor(const usb_str_desc_t *str_desc, char *str) {
    int cursor = 0;

    if (str_desc == NULL) {
        str[cursor] = '\0';
        return;
    }

    for (int i = 0; i < str_desc->bLength / 2; i++) {
        /*
        USB String descriptors of UTF-16.
        Right now We just skip any character larger than 0xFF to stay in BMP Basic Latin and Latin-1 Supplement range.
        */
        if (str_desc->wData[i] > 0xFF) {
            continue;
        }
        str[cursor++] = (char)str_desc->wData[i];
    }
    str[cursor] = '\0';
}
