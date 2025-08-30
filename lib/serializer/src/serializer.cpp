#include <cstdint>
#include <ctype.h>
#include <string.h>

char *rtrim(char *s) {
    char *back = s + strlen(s);
    while (isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

bool is_ordinary_char(const char exclusion[], size_t exclusion_size, char c) {
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
char *nextln(char *data, size_t data_len) {
    const char exclusion[] = {'\0', '\n', '\r'};
    size_t exclusion_size = sizeof(exclusion) / sizeof(exclusion[0]);

    char *forward = data;
    int32_t cur = data_len;
    while (--cur > 0 && is_ordinary_char(exclusion, exclusion_size, *++forward));
    *(forward) = '\0';

    while (--cur > 0 && !is_ordinary_char(exclusion, exclusion_size, *++forward));
    if (cur <= 0) {
        return NULL;
    } 

    return forward;
}
