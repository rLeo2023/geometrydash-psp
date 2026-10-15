#include <ctype.h>
#include <stddef.h>
#include <stdbool.h>

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