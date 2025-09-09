#include <cstdint>
#include <ctype.h>
#include <string.h>

unsigned char *rtrim(unsigned char *str) {
    unsigned char *back = str + strlen((const char*)str);
    while (isspace(*--back));
    *(back + 1) = '\0';
    return str;
}

bool is_empty(const unsigned char *str) {
    if (str == NULL) {
        return true;
    }

    while (*str != '\0') {
        if (!isspace(*str)) {
            return false;
        }
        str++;
    }

    return true;
}

bool is_ordinary_char(const unsigned char exclusion[], size_t exclusion_size, unsigned char c) {
    for (uint8_t it = 0; it < exclusion_size; it++) {
        if (exclusion[it] == c) {
            return false;
        }
    }

    return true;
}

/*
 * Return pointer to next line, or NULL if end
 */
unsigned char *nextln(unsigned char *data, size_t data_len) {
    const unsigned char exclusion[] = {'\0', '\n', '\r'};
    size_t exclusion_size = sizeof(exclusion) / sizeof(exclusion[0]);

    unsigned char *forward = data;
    int32_t cur = data_len;
    while (--cur > 0 && is_ordinary_char(exclusion, exclusion_size, *++forward));
    *(forward) = '\0';

    while (--cur > 0 && !is_ordinary_char(exclusion, exclusion_size, *++forward));
    if (cur <= 0) {
        return NULL;
    }

    return forward;
}
