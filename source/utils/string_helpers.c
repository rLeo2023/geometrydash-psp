#include <ctype.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// WHY DOESN'T C STANDARD CONTAIN STRCASESTR
bool contains(const char *first, const char *second) {
    if (*second == '\0')
        return true;

    for (; *first; first++) {
        const char *f = first;
        const char *s = second;

        while (*f && *s &&
               tolower((unsigned char)*f) == tolower((unsigned char)*s)) {
            f++;
            s++;
        }

        if (*s == '\0')
            return true;
    }

    return false;
}

bool parse_bool(const char *str) {
    return (str[0] == '1' && str[1] == '\0');
}

// Returns number but only using 3 significant digits, make sure to free this
char *truncate_number(int number) {
    char *buffer = malloc(20);

    if (!buffer) return NULL;

    int positive = abs(number);

    if (positive >= 1000000000) {
        float value = number / 1000000000.f;
        snprintf(buffer, 20, "%.3gB", value);
    } else if (positive >= 1000000) {
        float value = number / 1000000.f;
        snprintf(buffer, 20, "%.3gM", value);
    } else if (positive >= 1000) {
        float value = number / 1000.f;
        snprintf(buffer, 20, "%.3gK", value);
    } else {
        snprintf(buffer, 20, "%d", number);
    }

    return buffer;
}

char *truncate_speed(size_t bytes) {
    char *buffer = malloc(20);

    if (!buffer) return NULL;

    if (bytes >= 1024 * 1024) {
        float value = bytes / (float) (1024 * 1024);
        snprintf(buffer, 20, "%.3g MB/s", value);
    } else if (bytes >= 1024) {
        float value = bytes / 1024.f;
        snprintf(buffer, 20, "%.3g KB/s", value);
    } else {
        snprintf(buffer, 20, "%d B/s", bytes);
    }

    return buffer;
}

// Taken from: https://github.com/abejfehr/URLDecode/blob/master/urldecode.c
char *url_decode(const char *str) {
    int d = 0;
    char *dStr = malloc(strlen(str) + 1);
    char eStr[] = "00";

    strcpy(dStr, str);

    while(!d) {
        d = 1;
        int i;

        for(i = 0; i < strlen(dStr); ++i) {
            if(dStr[i] == '%') {
                if(dStr[i+1] == 0) return dStr;

                if (isxdigit((unsigned char) dStr[i+1]) && isxdigit((unsigned char) dStr[i+2])) {
                    eStr[0] = dStr[i+1];
                    eStr[1] = dStr[i+2];

                    long int x = strtol(eStr, NULL, 16);

                    if (x == 0x20) continue;

                    d = 0;

                    memmove(&dStr[i+1], &dStr[i+3], strlen(&dStr[i+3])+1);

                    dStr[i] = x;
                }
            }
        }
    }

    return dStr;
}

void remove_char(char *str, char character) {
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != character) dst++;
    }
    *dst = '\0';
}