#include "lseai.h"
/*
 * Transforms raw Gemini markdown into structured, human-readable
 * ASCII text suitable for XmText widgets and terminals.
 *
 * NOTE: The caller is responsible for calling free() on the returned string.
 */

#define MAX_TABLE_COLS 16

/* Trims leading and trailing spaces from a cell */
static void trim_cell(const char *src, size_t len, char *dest, size_t dest_cap)
{
    size_t start = 0;
    while (start < len && isspace((unsigned char)src[start])) start++;
    
    size_t end = len;
    while (end > start && isspace((unsigned char)src[end - 1])) end--;

    size_t copy_len = (end - start < dest_cap - 1) ? (end - start) : (dest_cap - 1);
    if (copy_len > 0) {
        memcpy(dest, src + start, copy_len);
    }
    dest[copy_len] = '\0';
}

/* Helper to check if a row is the separator row: |:---:|---| */
static int is_separator_row(const char *line, size_t len)
{
    size_t i;
    int saw_dash = 0;
    for (i = 0; i < len; i++) {
        char c = line[i];
        if (c == '-') saw_dash = 1;
        else if (c != '|' && c != ':' && !isspace((unsigned char)c)) {
            return 0;
        }
    }
    return saw_dash;
}

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
    /* Expansion space for table borders, underlines, and padding */
    out_cap = (in_len * 3) + 4096;
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
                char lang[32];
                size_t lang_len = 0;
                size_t k;

                for (k = 3; k < line_len && lang_len < 31; k++) {
                    if (!isspace((unsigned char)line_start[k])) {
                        lang[lang_len++] = line_start[k];
                    }
                }
                lang[lang_len] = '\0';

                out_idx += sprintf(&out[out_idx], "+--- [ %s%s ] --------------------------------------------------",
                                   lang_len > 0 ? lang : "Code",
                                   lang_len > 0 ? " Code" : "");
                in_code_block = 1;
            } else {
                out_idx += sprintf(&out[out_idx], "+----------------------------------------------------------------");
                in_code_block = 0;
            }
            continue;
        }

        /* Inside a code block: keep verbatim */
        if (in_code_block) {
            memcpy(&out[out_idx], line_start, line_len);
            out_idx += line_len;
            out[out_idx++] = '\n';
            continue;
        }

        /* -------------------------------------------------------------
         * 2. Light Table Formatter (| col1 | col2 |)
         * ------------------------------------------------------------- */
        if (line_len >= 3 && line_start[0] == '|' && line_start[line_len - 1] == '|') {
            /* Look ahead to measure columns for the entire table block */
            const char *tbl_ptr = line_start;
            int col_widths[MAX_TABLE_COLS] = {0};
            int num_cols = 0;

            while (*tbl_ptr != '\0') {
                const char *row_end = strchr(tbl_ptr, '\n');
                size_t row_len = row_end ? (size_t)(row_end - tbl_ptr) : strlen(tbl_ptr);

                if (row_len < 3 || tbl_ptr[0] != '|' || tbl_ptr[row_len - 1] != '|') {
                    break; /* End of table reached */
                }

                if (!is_separator_row(tbl_ptr, row_len)) {
                    int c_idx = 0;
                    const char *cell_start = tbl_ptr + 1;
                    const char *pipe;

                    while ((pipe = memchr(cell_start, '|', (tbl_ptr + row_len) - cell_start)) != NULL && c_idx < MAX_TABLE_COLS) {
                        char buf[128];
                        trim_cell(cell_start, pipe - cell_start, buf, sizeof(buf));
                        int w = (int)strlen(buf);
                        if (w > col_widths[c_idx]) {
                            col_widths[c_idx] = w;
                        }
                        c_idx++;
                        cell_start = pipe + 1;
                    }
                    if (c_idx > num_cols) num_cols = c_idx;
                }

                tbl_ptr = row_end ? (row_end + 1) : (tbl_ptr + row_len);
            }

            /* Render table lines with normalized column widths */
            if (num_cols > 0) {
                out[out_idx++] = '\n';

                while (*p != '\0') {
                    const char *row_start = line_start;
                    size_t r_len = line_len;

                    int sep = is_separator_row(row_start, r_len);

                    if (sep) {
                        /* Render divider line: +------+------+ */
                        int c;
                        out[out_idx++] = '+';
                        for (c = 0; c < num_cols; c++) {
                            int w;
                            for (w = 0; w < col_widths[c] + 2; w++) out[out_idx++] = '-';
                            out[out_idx++] = '+';
                        }
                        out[out_idx++] = '\n';
                    } else {
                        /* Render content row: | item   | 123    | */
                        int c_idx = 0;
                        const char *cell_start = row_start + 1;
                        const char *pipe;

                        out[out_idx++] = '|';
                        while ((pipe = memchr(cell_start, '|', (row_start + r_len) - cell_start)) != NULL && c_idx < num_cols) {
                            char buf[128];
                            trim_cell(cell_start, pipe - cell_start, buf, sizeof(buf));
                            
                            int pad = col_widths[c_idx] - (int)strlen(buf);
                            out_idx += sprintf(&out[out_idx], " %s", buf);
                            while (pad-- > 0) out[out_idx++] = ' ';
                            out[out_idx++] = ' ';
                            out[out_idx++] = '|';

                            c_idx++;
                            cell_start = pipe + 1;
                        }
                        out[out_idx++] = '\n';
                    }

                    /* Advance outer loop past this table line */
                    if (*p == '\0') break;
                    line_start = p;
                    line_end = strchr(p, '\n');
                    line_len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);
                    p = line_end ? (line_end + 1) : (line_start + line_len);

                    /* If the next line isn't part of the table, stop eating lines */
                    if (line_len < 3 || line_start[0] != '|' || line_start[line_len - 1] != '|') {
                        p = line_start; /* Rewind so standard parser picks up this next line */
                        break;
                    }
                }
                out[out_idx++] = '\n';
                continue;
            }
        }

        /* -------------------------------------------------------------
         * 3. Headers: # H1, ## H2, ### H3
         * ------------------------------------------------------------- */
        if (line_len > 2 && line_start[0] == '#') {
            int level = 0;
            size_t idx = 0;

            while (idx < line_len && line_start[idx] == '#') {
                level++;
                idx++;
            }

            if (idx < line_len && line_start[idx] == ' ') {
                idx++;
                size_t title_len = line_len - idx;
                size_t t;

                out[out_idx++] = '\n';
                for (t = 0; t < title_len; t++) {
                    out[out_idx++] = (char)toupper((unsigned char)line_start[idx + t]);
                }
                out[out_idx++] = '\n';

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
         * 4. Bullet Points (* item or - item)
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
         * 5. Regular Text Paragraph Line
         * ------------------------------------------------------------- */
        memcpy(&out[out_idx], line_start, line_len);
        out_idx += line_len;
        out[out_idx++] = '\n';
    }

    out[out_idx] = '\0';
    return out;
}
