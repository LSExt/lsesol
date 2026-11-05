#include "lseai.h"
/*
 * Transforms raw Gemini markdown into structured, human-readable
 * ASCII text suitable for XmText widgets and terminals.
 *
 * NOTE: The caller is responsible for calling free() on the returned string.
 */

#define MAX_TABLE_COLS 16

/* =========================================================================
 * LaTeX & Unicode Arrow -> Clean ASCII Replacements for Motif/XmText
 * ========================================================================= */

typedef struct {
    const char *token;
    size_t      token_len;
    const char *ascii_repr;
    size_t      repr_len;
} ArrowRule;

/* Ordered from longest / most specific to shortest */
static const ArrowRule ARROW_RULES[] = {
    /* LaTeX Math with delimiters */
    { "$\\leftrightarrow$", 18, "<->", 3 },
    { "$\\Leftrightarrow$", 18, "<=>", 3 },
    { "$\\rightarrow$",     14, "->",  2 },
    { "$\\leftarrow$",      13, "<-",  2 },
    { "$\\Rightarrow$",     14, "=>",  2 },
    { "$\\Leftarrow$",      13, "<=",  2 },
    { "$\\to$",              6, "->",  2 },

    /* Bare LaTeX Macros */
    { "\\leftrightarrow",   15, "<->", 3 },
    { "\\Leftrightarrow",   15, "<=>", 3 },
    { "\\rightarrow",       11, "->",  2 },
    { "\\leftarrow",        10, "<-",  2 },
    { "\\Rightarrow",       11, "=>",  2 },
    { "\\Leftarrow",        10, "<=",  2 },
    { "\\to",                3, "->",  2 },

    /* UTF-8 Byte Sequences (for when Gemini sends raw Unicode) */
    { "\xE2\x86\x94",        3, "<->", 3 }, /*  */
    { "\xE2\x87\x94",        3, "<=>", 3 }, /*  */
    { "\xE2\x86\x92",        3, "->",  2 }, /*  */
    { "\xE2\x86\x90",        3, "<-",  2 }, /*  */
    { "\xE2\x87\x92",        3, "=>",  2 }, /*  */
    { "\xE2\x87\x90",        3, "<=",  2 }, /*  */

    { NULL, 0, NULL, 0 }
};

/**
 * Check if the current position matches an arrow pattern.
 * If matched, outputs the ASCII arrow, advances input pointer `src`, and returns 1.
 * Otherwise returns 0.
 */
static int match_and_replace_arrow(const char **src, char **dst, size_t *dst_rem) {
    const ArrowRule *rule;

    for (rule = ARROW_RULES; rule->token != NULL; rule++) {
        if (strncmp(*src, rule->token, rule->token_len) == 0) {
            /* Ensure destination buffer has space */
            if (*dst_rem > rule->repr_len) {
                memcpy(*dst, rule->ascii_repr, rule->repr_len);
                *dst += rule->repr_len;
                *dst_rem -= rule->repr_len;
            }
            /* Advance input buffer past the token */
            *src += rule->token_len;
            return 1;
        }
    }
    return 0;
}


static size_t format_prose_line(const char *src, size_t len, char *dst, size_t max_len)
{
    size_t i = 0;
    size_t out_idx = 0;

    while (i < len && out_idx + 4 < max_len) {
        /* Check if we are at '$' or '\' */
        if (src[i] == '$' || src[i] == '\\') {
            size_t start = i;
            int has_leading_dollar = (src[i] == '$');
            if (has_leading_dollar) i++;

            if (i < len && src[i] == '\\') {
                const char *cmd = &src[i];
                size_t rem = len - i;

                /* \rightarrow or \to */
                if ((rem >= 11 && strncmp(cmd, "\\rightarrow", 11) == 0) ||
                    (rem >= 3  && strncmp(cmd, "\\to", 3) == 0)) {
                    size_t cmd_len = (cmd[1] == 'r') ? 11 : 3;
                    i += cmd_len;
                    if (has_leading_dollar && i < len && src[i] == '$') i++;
                    dst[out_idx++] = '-';
                    dst[out_idx++] = '>';
                    continue;
                }
                /* \Rightarrow */
                if (rem >= 11 && strncmp(cmd, "\\Rightarrow", 11) == 0) {
                    i += 11;
                    if (has_leading_dollar && i < len && src[i] == '$') i++;
                    dst[out_idx++] = '=';
                    dst[out_idx++] = '>';
                    continue;
                }
                /* \leftarrow */
                if (rem >= 10 && strncmp(cmd, "\\leftarrow", 10) == 0) {
                    i += 10;
                    if (has_leading_dollar && i < len && src[i] == '$') i++;
                    dst[out_idx++] = '<';
                    dst[out_idx++] = '-';
                    continue;
                }
                /* \Leftarrow */
                if (rem >= 10 && strncmp(cmd, "\\Leftarrow", 10) == 0) {
                    i += 10;
                    if (has_leading_dollar && i < len && src[i] == '$') i++;
                    dst[out_idx++] = '<';
                    dst[out_idx++] = '=';
                    continue;
                }
                /* \leftrightarrow */
                if (rem >= 15 && strncmp(cmd, "\\leftrightarrow", 15) == 0) {
                    i += 15;
                    if (has_leading_dollar && i < len && src[i] == '$') i++;
                    dst[out_idx++] = '<';
                    dst[out_idx++] = '-';
                    dst[out_idx++] = '>';
                    continue;
                }
                /* \Leftrightarrow */
                if (rem >= 15 && strncmp(cmd, "\\Leftrightarrow", 15) == 0) {
                    i += 15;
                    if (has_leading_dollar && i < len && src[i] == '$') i++;
                    dst[out_idx++] = '<';
                    dst[out_idx++] = '=';
                    dst[out_idx++] = '>';
                    continue;
                }
            }
            /* Not a recognized arrow; rewind to where we started */
            i = start;
        }

        /* Normal ASCII copy */
        dst[out_idx++] = src[i++];
    }

    return out_idx;
}
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

        /* Strip trailing \r (CRLF sanitize) */
        while (line_len > 0 && line_start[line_len - 1] == '\r') {
            line_len--;
        }

        /* -------------------------------------------------------------
         * 1. Code Fence Handling (```lang ... ```)
         * ------------------------------------------------------------- */
        /* Skip leading whitespace to tolerate indented fences */
        size_t lead_ws = 0;
        while (lead_ws < line_len && (line_start[lead_ws] == ' ' || line_start[lead_ws] == '\t')) {
            lead_ws++;
        }

        if ((line_len - lead_ws) >= 3 && strncmp(line_start + lead_ws, "```", 3) == 0) {
            if (!in_code_block) {
                char lang[64];
                size_t lang_len = 0;
                size_t k = lead_ws + 3;

                /* Skip whitespace after ``` */
                while (k < line_len && isspace((unsigned char)line_start[k])) {
                    k++;
                }

                /* Capture only the language token (stop at space, newline, or punctuation) */
                while (k < line_len && lang_len < (sizeof(lang) - 1)) {
                    unsigned char c = (unsigned char)line_start[k];
                    if (isspace(c) || c == '\\' || c == '/' || c == '*' || c == ';') {
                        break;
                    }
                    lang[lang_len++] = line_start[k++];
                }
                /* Trim trailing spaces from lang */
                while (lang_len > 0 && isspace((unsigned char)lang[lang_len - 1])) {
                    lang_len--;
                }
                lang[lang_len] = '\0';

                if (lang_len > 0) {
                    out_idx += snprintf(&out[out_idx], 125, "+--- [ %s ] --------------------------------------------------\n", lang);
                } else {
                    out_idx += sprintf(&out[out_idx], "+--- [ Code ] --------------------------------------------------\n");
                }
                in_code_block = 1;
            } else {
                out_idx += sprintf(&out[out_idx], "+----------------------------------------------------------------\n");
                in_code_block = 0;
            }
            continue;
        }

        /* Inside a code block: keep verbatim without altering code */
        if (in_code_block) {
            memcpy(&out[out_idx], line_start, line_len);
            out_idx += line_len;
            out[out_idx++] = '\n';
            continue;
        }

       /* -------------------------------------------------------------
         * 2. Light Table Formatter (| col1 | col2 |)
         * ------------------------------------------------------------- */
        /* Trim trailing spaces from line_len to ensure trailing '|' matches */
        size_t trimmed_len = line_len;
        while (trimmed_len > 0 && isspace((unsigned char)line_start[trimmed_len - 1])) {
            trimmed_len--;
        }

        if (trimmed_len >= 3 && line_start[0] == '|' && line_start[trimmed_len - 1] == '|') {
            const char *tbl_ptr = line_start;
            int col_widths[MAX_TABLE_COLS] = {0};
            int num_cols = 0;

            /* PASS 1: Compute column widths */
            while (*tbl_ptr != '\0') {
                const char *row_end = strchr(tbl_ptr, '\n');
                size_t r_len = row_end ? (size_t)(row_end - tbl_ptr) : strlen(tbl_ptr);
                while (r_len > 0 && isspace((unsigned char)tbl_ptr[r_len - 1])) r_len--;

                if (r_len < 3 || tbl_ptr[0] != '|' || tbl_ptr[r_len - 1] != '|') {
                    break; /* Not a table row */
                }

                if (!is_separator_row(tbl_ptr, r_len)) {
                    int c_idx = 0;
                    const char *cell_start = tbl_ptr + 1;
                    const char *pipe;

                    while ((pipe = memchr(cell_start, '|', (tbl_ptr + r_len) - cell_start)) != NULL && c_idx < MAX_TABLE_COLS) {
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

                tbl_ptr = row_end ? (row_end + 1) : (tbl_ptr + r_len);
            }

            /* PASS 2: Render table with aligned borders */
            if (num_cols > 0) {
                out[out_idx++] = '\n';
                tbl_ptr = line_start;

                while (*tbl_ptr != '\0') {
                    const char *row_end = strchr(tbl_ptr, '\n');
                    size_t r_len = row_end ? (size_t)(row_end - tbl_ptr) : strlen(tbl_ptr);
                    const char *next_row = row_end ? (row_end + 1) : (tbl_ptr + r_len);
                    while (r_len > 0 && isspace((unsigned char)tbl_ptr[r_len - 1])) r_len--;

                    if (r_len < 3 || tbl_ptr[0] != '|' || tbl_ptr[r_len - 1] != '|') {
                        break;
                    }

                    if (is_separator_row(tbl_ptr, r_len)) {
                        out[out_idx++] = '+';
                        for (int c = 0; c < num_cols; c++) {
                            for (int w = 0; w < col_widths[c] + 2; w++) out[out_idx++] = '-';
                            out[out_idx++] = '+';
                        }
                        out[out_idx++] = '\n';
                    } else {
                        int c_idx = 0;
                        const char *cell_start = tbl_ptr + 1;
                        const char *pipe;

                        out[out_idx++] = '|';
                        while ((pipe = memchr(cell_start, '|', (tbl_ptr + r_len) - cell_start)) != NULL && c_idx < num_cols) {
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

                    tbl_ptr = next_row;
                }

                /* Synchronize outer loop pointer 'p' to end of table */
                p = tbl_ptr;
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
         * 4. Bullet Points (* item, - item, or indented bullets)
         * ------------------------------------------------------------- */
        size_t b_indent = 0;
        while (b_indent < line_len && (line_start[b_indent] == ' ' || line_start[b_indent] == '\t')) {
            b_indent++;
        }

        if ((line_len - b_indent) >= 2 && 
            (line_start[b_indent] == '*' || line_start[b_indent] == '-') && 
            line_start[b_indent + 1] == ' ') 
        {
            /* Preserve indentation + add clean bullet symbol */
            for (size_t sp = 0; sp < b_indent; sp++) out[out_idx++] = ' ';
            out[out_idx++] = ' ';
            out[out_idx++] = 'o';
            out[out_idx++] = ' ';

            const char *item_text = line_start + b_indent + 2;
            size_t item_len = line_len - (b_indent + 2);

            /* Format the bullet text through prose formatter so arrows/macros resolve */
            out_idx += format_prose_line(item_text, item_len, &out[out_idx], out_cap - out_idx);
            out[out_idx++] = '\n';
            continue;
        }

        /* -------------------------------------------------------------
         * 5. Regular Prose Line (Arrows & Inline Formatting)
         * ------------------------------------------------------------- */
        out_idx += format_prose_line(line_start, line_len, &out[out_idx], out_cap - out_idx);
        out[out_idx++] = '\n';
    }

    out[out_idx] = '\0';
    return out;
}
