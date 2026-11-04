#include "lseai.h"
/*
 * Transforms raw Gemini markdown into structured, human-readable
 * ASCII text suitable for XmText widgets and terminals.
 *
 * NOTE: The caller is responsible for calling free() on the returned string.
 */
char *format_markdown_to_ascii(const char *src)
{
    size_t in_len;
    size_t out_cap;
    size_t out_idx = 0;
    char *out;
    const char *p;
    int in_code_block = 0;

    if (!src) return NULL;

    in_len = strlen(src);
    /* Allocate buffer with plenty of expansion space for borders and underlines */
    out_cap = (in_len * 2) + 2048;
    out = (char *)malloc(out_cap);
    if (!out) return NULL;

    p = src;
    while (*p != '\0') {
        const char *line_start = p;
        const char *line_end;
        size_t line_len;

        /* Find end of current line */
        line_end = strchr(p, '\n');
        if (!line_end) {
            line_len = strlen(line_start);
            p = line_start + line_len;
        } else {
            line_len = (size_t)(line_end - line_start);
            p = line_end + 1;
        }

        /* -------------------------------------------------------------
         * 1. Code Fence Handling (```lang ... ```)
         * ------------------------------------------------------------- */
        if (line_len >= 3 && strncmp(line_start, "```", 3) == 0) {
            if (!in_code_block) {
                /* Opening code fence: extract language identifier */
                char lang[32];
                size_t lang_len = 0;
                size_t k;

                for (k = 3; k < line_len && lang_len < 31; k++) {
                    if (!isspace((unsigned char)line_start[k])) {
                        lang[lang_len++] = line_start[k];
                    }
                }
                lang[lang_len] = '\0';

                /* Draw top boundary rule */
                out_idx += sprintf(&out[out_idx], "\n+--- [ %s%s ] --------------------------------------------------\n",
                                   lang_len > 0 ? lang : "Code",
                                   lang_len > 0 ? " Code" : "");
                in_code_block = 1;
            } else {
                /* Closing code fence: draw bottom boundary rule */
                out_idx += sprintf(&out[out_idx], "+----------------------------------------------------------------\n\n");
                in_code_block = 0;
            }
            continue;
        }

        /* Inside a code block: keep lines verbatim (no prefix so copy-paste stays valid) */
        if (in_code_block) {
            memcpy(&out[out_idx], line_start, line_len);
            out_idx += line_len;
            out[out_idx++] = '\n';
            continue;
        }

        /* -------------------------------------------------------------
         * 2. Headers: # H1, ## H2, ### H3
         * ------------------------------------------------------------- */
        if (line_len > 2 && line_start[0] == '#') {
            int level = 0;
            size_t idx = 0;

            while (idx < line_len && line_start[idx] == '#') {
                level++;
                idx++;
            }

            if (idx < line_len && line_start[idx] == ' ') {
                idx++; /* skip space */
                size_t title_len = line_len - idx;
                size_t t;

                out[out_idx++] = '\n';

                /* Print header title in uppercase */
                for (t = 0; t < title_len; t++) {
                    out[out_idx++] = (char)toupper((unsigned char)line_start[idx + t]);
                }
                out[out_idx++] = '\n';

                /* Draw underline (=== for H1, --- for H2/H3) */
                char under_char = (level == 1) ? '=' : '-';
                size_t rule_len = (title_len < 64) ? title_len : 64;
                for (t = 0; t < rule_len; t++) {
                    out[out_idx++] = under_char;
                }
                out[out_idx++] = '\n';
                continue;
            }
        }

        /* -------------------------------------------------------------
         * 3. Bullet Points (* item or - item)
         * ------------------------------------------------------------- */
        if (line_len >= 2 && (line_start[0] == '*' || line_start[0] == '-') && line_start[1] == ' ') {
            out[out_idx++] = ' ';
            out[out_idx++] = ' ';
            out[out_idx++] = 'o';
            out[out_idx++] = ' ';
            memcpy(&out[out_idx], line_start + 2, line_len - 2);
            out_idx += (line_len - 2);
            out[out_idx++] = '\n';
            continue;
        }

        /* -------------------------------------------------------------
         * 4. Regular Text Paragraph Line
         * ------------------------------------------------------------- */
        memcpy(&out[out_idx], line_start, line_len);
        out_idx += line_len;
        out[out_idx++] = '\n';
    }

    out[out_idx] = '\0';
    return out;
}
