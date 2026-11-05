#include "lseai.h"


/*
 * Filters UTF-8 down to 7-bit ASCII AND unescapes JSON literals (\n, \t, \", \\)
 * so XmText widgets render line breaks properly.
 * NOTE: The caller is responsible for calling free() on the returned string.
 */
char *clean_to_ascii_string(const char *str)
{
    size_t in_len;
    size_t out_cap;
    size_t out_idx = 0;
    char *out;

    if (!str) {
        return NULL;
    }

    in_len = strlen(str);

    /* Allocate buffer with room for expansion */
    out_cap = (in_len * 2) + 32;
    out = (char *)malloc(out_cap);
    if (!out) {
        return NULL;
    }

    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];

        /* Handle JSON backslash escapes */
        if (c == '\\' && i + 1 < len) {
            char next = str[i + 1];
            switch (next) {
                case 'n': out[out_idx++] = '\n'; i++; continue;
                case 'r': out[out_idx++] = '\r'; i++; continue;
                case 't': out[out_idx++] = '\t'; i++; continue;
                case '"': out[out_idx++] = '"';  i++; continue;
                case '\\': out[out_idx++] = '\\'; i++; continue;
                case '/':  out[out_idx++] = '/';  i++; continue;
                case 'u':
                    /* Convert Hex Unicode escapes like \u003e to plain ASCII */
                    if (i + 5 < len) {
                        if (strncasecmp(&str[i + 2], "003e", 4) == 0) { out[out_idx++] ='>'; i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "003c", 4) == 0) { out[out_idx++] ='<'; i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "0026", 4) == 0) { out[out_idx++] ='&'; i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "0022", 4) == 0) { out[out_idx++] ='"'; i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "0027", 4) == 0) { out[out_idx++] ='\''; i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "2022", 4) == 0) { out[out_idx++] ='*'; i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "2013", 4) == 0 ||
                            strncasecmp(&str[i + 2], "2014", 4) == 0) { out[out_idx++] ='-'; i += 5; continue; }
                    }
                    out[out_idx++] = str[i];
                    continue;
                default:
                    out[out_idx++] = str[i];
                    continue;
            }
        }


        /* Filter high-bit UTF-8 sequences and convert to 7-bit ASCII */
        if (c >= 0x80) {
            if (c == 0xE2 && i + 2 < len && (unsigned char)str[i + 1] == 0x80) {
                unsigned char c3 = (unsigned char)str[i + 2];
                if (c3 == 0x98 || c3 == 0x99) { out[out_idx++] ='\''; i += 2; continue; }
                if (c3 == 0x9C || c3 == 0x9D) { out[out_idx++] ='"';  i += 2; continue; }
                if (c3 == 0x93 || c3 == 0x94) { out[out_idx++] ='-';  i += 2; continue; }
                if (c3 == 0xA2)             { out[out_idx++] = '*';  i += 2; continue; }
                if (c3 == 0xA6)             { out[out_idx++] = '.'; out[out_idx++] = '.'; out[out_idx++] = '.'; i += 2; continue; }
            }
            if ((c & 0xC0) == 0x80) {
                continue;
            }
            continue;
        }
        /* 
         * Keep printable ASCII, newline, carriage return, tab, 
         * and ESC (0x1B / 27) for ANSI color sequences 
         */
        if (c >= 32 || c == '\n' || c == '\r' || c == '\t' || c == 0x1B) {
            out[out_idx++] = c;
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
        /* Format Markdown headers, bullets, and code boxes */
        char *formatted = format_markdown_to_ascii(cleaned); 
        fputs(formatted, stdout);
        putchar('\n');
        fflush(stdout);
        free(cleaned);
    }
}
