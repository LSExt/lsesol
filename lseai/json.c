#include "lseai.h"

/* Extract the LAST model text payload from raw Gemini API JSON */
char *extract_text_field(const char *json_resp) {
    if (!json_resp) return NULL;

    const char *target = "\"text\"";
    const char *last_match = NULL;
    const char *p = json_resp;

    /* Find the last occurrence of key "text" in the response string */
    while ((p = strstr(p, target)) != NULL) {
        last_match = p;
        p += strlen(target);
    }

    if (!last_match) return NULL;

    /* Move pointer past "text" and find the starting quote of the value string */
    p = last_match + strlen(target);
    while (*p && *p != ':') p++;
    if (*p == ':') p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (*p != '"') return NULL; /* Missing opening quote */
    p++; /* Move past starting quote */

    const char *start = p;

    /* Scan to closing quote, ignoring escaped quotes (\") */
    while (*p) {
        if (*p == '\\') {
            if (*(p + 1) != '\0') {
                p += 2;
                continue;
            }
        }
        if (*p == '"') {
            break;
        }
        p++;
    }

    if (*p != '"') return NULL;

    size_t len = p - start;
    char *text = (char *)malloc((len + 1) * sizeof(char));
    if (!text) return NULL;

    strncpy(text, start, len);
    text[len] = '\0';

    return text;
}

/* Escape string characters for raw JSON injection */
char *json_escape(const char *src) {
    size_t len = strlen(src);
    char *dest = (char *)malloc(len * 2 + 1);
    if (!dest) return NULL;
    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        switch (src[i]) {
            case '"':  dest[j++] = '\\'; dest[j++] = '"';  break;
            case '\\': dest[j++] = '\\'; dest[j++] = '\\'; break;
            case '\n': dest[j++] = '\\'; dest[j++] = 'n';  break;
            case '\r': dest[j++] = '\\'; dest[j++] = 'r';  break;
            case '\t': dest[j++] = '\\'; dest[j++] = 't';  break;
            default:   dest[j++] = src[i]; break;
        }
    }
    dest[j] = '\0';
    return dest;
}
    
