#include "lseai.h"


/*
 * Filters UTF-8 down to 7-bit ASCII AND unescapes JSON literals (\n, \t, \", \\)
 * so XmText widgets render line breaks properly.
 * NOTE: The caller is responsible for calling free() on the returned string.
 */
char *clean_to_ascii_string(const char *input)
{
    size_t in_len;
    size_t out_cap;
    size_t out_idx = 0;
    size_t i;
    char *out;

    if (!input) {
        return NULL;
    }

    in_len = strlen(input);

    /* Allocate buffer with room for expansion */
    out_cap = (in_len * 2) + 32;
    out = (char *)malloc(out_cap);
    if (!out) {
        return NULL;
    }

    for (i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)input[i];

        /* --- 1. Handle JSON Escape Sequences (\n, \r, \t, \", \\) --- */
        if (c == '\\' && (i + 1 < in_len)) {
            char next = input[i + 1];
            if (next == 'n') {
                out[out_idx++] = '\n'; /* Actual ASCII 10 (Line Feed) */
                i++;
                continue;
            } else if (next == 'r') {
                /* Skip carriage returns for Motif text widgets */
                i++;
                continue;
            } else if (next == 't') {
                out[out_idx++] = '\t';
                i++;
                continue;
            } else if (next == '"') {
                out[out_idx++] = '"';
                i++;
                continue;
            } else if (next == '\\') {
                out[out_idx++] = '\\';
                i++;
                continue;
            }
        }

        /* --- 2. Multi-byte UTF-8 handling --- */
        if (c >= 0x80) {
            /* Check 3-byte sequences (0xE0 - 0xEF) */
            if (c >= 0xE0 && c <= 0xEF && (i + 2 < in_len)) {
                unsigned char c2 = (unsigned char)input[i + 1];
                unsigned char c3 = (unsigned char)input[i + 2];

                /* Ellipsis: UTF-8 0xE2 0x80 0xA6 -> ASCII "..." */
                if (c == 0xE2 && c2 == 0x80 && c3 == 0xA6) {
                    out[out_idx++] = '.';
                    out[out_idx++] = '.';
                    out[out_idx++] = '.';
                    i += 2;
                    continue;
                }
            }

            /* Skip remaining continuation bytes */
            if ((c & 0xC0) == 0x80) {
                continue;
            }
            continue;
        }

        /* --- 3. Keep valid printable ASCII, newline, and tab --- */
        if (c >= 32 || c == '\n' || c == '\t') {
            out[out_idx++] = (char)c;
        }
    }

    out[out_idx] = '\0';
    return out;
}

/* 
 * Formats a raw Gemini response buffer into 7-bit ASCII
 * and prints it directly to stdout with proper flushing.
 */
void print_formatted_response(const char *str)
{    char *cleaned;
    if (!str) {
        return;
    }

    cleaned = clean_to_ascii_string(str);
    if (cleaned) {
        fputs(cleaned, stdout);
        putchar('\n');
        fflush(stdout);
        free(cleaned);
    }
}
