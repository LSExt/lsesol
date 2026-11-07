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
    

/* Helper: Read an entire file into memory */
static char *load_file_to_memory(const char *filepath)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t r = fread(buf, 1, len, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* Helper: Extract a quoted string value after a specific JSON key,
 * and clean it via clean_to_ascii_string().
 */
static const char *scan_json_field(const char *p, const char *key, char **out_val)
{
    const char *k = strstr(p, key);
    if (!k) return NULL;

    /* Move past key and find ':' */
    p = k + strlen(key);
    while (*p && *p != ':') p++;
    if (*p == ':') p++;

    /* Find opening quote */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != '"') return NULL;
    p++; /* skip opening quote */

    const char *start = p;

    /* Scan to closing quote, respecting backslashes */
    while (*p) {
        if (*p == '\\') {
            if (*(p + 1) != '\0') {
                p += 2;
                continue;
            }
        }
        if (*p == '"') break;
        p++;
    }

    if (*p != '"') return NULL;

    /* Extract raw slice and pass directly to your clean_to_ascii_string */
    size_t len = p - start;
    char *raw_slice = (char *)malloc(len + 1);
    if (raw_slice) {
        strncpy(raw_slice, start, len);
        raw_slice[len] = '\0';

        *out_val = clean_to_ascii_string(raw_slice);
        free(raw_slice);
    } else {
        *out_val = NULL;
    }

    return p + 1; /* Return pointer right after the closing quote */
}

/* -------------------------------------------------------------
 * 1. Count conversation turns
 * -----------------------------------------------------------*/
int count_conversation_turns(const char *filepath)
{
    char *raw = load_file_to_memory(filepath);
    if (!raw) return 0;

    int count = 0;
    const char *p = raw;
    const char *target = "\"role\"";

    while ((p = strstr(p, target)) != NULL) {
        count++;
        p += strlen(target);
    }

    free(raw);
    return count;
}

/**
 * Parses a JSON slice of turns returned by extract_raw_turns_json()
 * and prints them sequentially into a dynamically allocated buffer.
 * Caller must free() the returned pointer.
 */
char *format_turns_for_display(const char *raw_turns_json)
{
    if (!raw_turns_json || *raw_turns_json == '\0') {
        return NULL;
    }

    size_t cap = 8192;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    const char *p = raw_turns_json;

    while (*p) {
        char *role = NULL;
        char *text = NULL;

        /* Locate next "role" field */
        p = scan_json_field(p, "role", &role);
        if (!p || !role) {
            if (role) free(role);
            break;
        }

        /* Locate matching "text" field */
        p = scan_json_field(p, "text", &text);
        if (!p || !text) {
            free(role);
            if (text) free(text);
            break;
        }

        /* Determine header tag based on role */
        const char *tag = (strcmp(role, "user") == 0) ? PROMPT_HEADER : RESPONSE_HEADER;

        /* don't add the summary to back-scroll reload,
         * is not user generated
         */
        if( strcmp(role, "summary") == 0){
            continue;
        }


        /* Format: "\n[Tag]:\n<text>\n\n" */
        size_t block_len = strlen(tag) + strlen(text) + 64;
        while (len + block_len >= cap) {
            cap *= 2;
            char *new_buf = realloc(buf, cap);
            if (!new_buf) {
                free(buf);
                free(role);
                free(text);
                return NULL;
            }
            buf = new_buf;
        }

                char *ascii_text = clean_to_ascii_string(text);
                if (ascii_text) {
                    /* Format Markdown headers, bullets, code boxes and tables */
                    char *formatted = format_markdown_to_ascii(ascii_text);
                    if(formatted){
        				len += snprintf(buf + len, cap - len,
        				                "%s%s\n\n",
        				                tag, formatted);
                        free(formatted);
                    }
                    free(ascii_text);
                }

        free(role);
        free(text);
    }

    return buf;
}


/* -------------------------------------------------------------
 * 2. Extract turns for summarization
 * -----------------------------------------------------------*/
char *extract_turns_for_summary(const char *filepath, int start_turn, int count)
{
    char *raw = load_file_to_memory(filepath);
    if (!raw) return NULL;

    size_t cap = 8192;
    size_t len = 0;
    char *out = (char *)malloc(cap);
    if (!out) {
        free(raw);
        return NULL;
    }
    out[0] = '\0';

    /* 1. Extract existing summary if present */
    char *prev_summary = NULL;
    scan_json_field(raw, "\"summary\"", &prev_summary);
    if (prev_summary && strlen(prev_summary) > 0) {
        snprintf(out, cap, "=== PREVIOUS SUMMARY ===\n%s\n\n=== NEW DIALOGUE TO SUMMARIZE ===\n", prev_summary);
        len = strlen(out);
        free(prev_summary);
    } else {
        strcpy(out, "=== DIALOGUE TO SUMMARIZE ===\n");
        len = strlen(out);
    }

    /* 2. Walk through the turns */
    const char *cursor = raw;
    int current_turn = 0;
    int extracted = 0;

    while (cursor && extracted < count) {
        char *role = NULL;
        char *text = NULL;

        /* Locate "role" */
        cursor = scan_json_field(cursor, "\"role\"", &role);
        if (!cursor) break;

        /* Locate "text" */
        cursor = scan_json_field(cursor, "\"text\"", &text);
        if (!cursor) {
            if (role) free(role);
            break;
        }

        /* If within window [start_turn, start_turn + count) */
        if (current_turn >= start_turn) {
            const char *display_role = (strcmp(role, "user") == 0) ? "User" : "Assistant";
            size_t entry_len = strlen(display_role) + strlen(text) + 8;

            while (len + entry_len >= cap) {
                cap *= 2;
                out = (char *)realloc(out, cap);
            }

            len += sprintf(out + len, "%s: %s\n\n", display_role, text);
            extracted++;
        }

        if (role) free(role);
        if (text) free(text);

        current_turn++;
    }

    free(raw);
    return out;
}

/**
 * Extracts a verbatim JSON slice of turns from `start_turn` for `count` turns.
 * Example returned string:
 *   {"role":"user","parts":[{"text":"Hi"}]},{"role":"model","parts":[{"text":"Hello"}]}
 *
 * Returns an allocated string (caller must free()), or NULL on error.
 */
char *extract_raw_turns_json(const char *filepath, int start_turn, int count)
{
    if (!filepath || count <= 0 || start_turn < 0) {
        return NULL;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, sz, f);
    buf[n] = '\0';
    fclose(f);

    const char *target = "\"role\"";
    size_t target_len = strlen(target);
    const char *p = buf;

    int current_turn = 0;
    const char *slice_start = NULL;
    const char *slice_end = NULL;

    while ((p = strstr(p, target)) != NULL) {
        /*
         * Each turn is an object: find the opening '{' for this turn
         * by walking backwards from the "role" key.
         */
        const char *turn_obj_start = p;
        while (turn_obj_start > buf && *turn_obj_start != '{') {
            turn_obj_start--;
        }

        /* 1. Mark the beginning of our desired turn window */
        if (current_turn == start_turn) {
            slice_start = turn_obj_start;
        }

        /* 2. Walk forward through this turn object to find its matching closing '}' */
        const char *scan = p + target_len;
        int brace_depth = 1; /* We started at '{' */
        int in_string = 0;

        while (*scan && brace_depth > 0) {
            if (*scan == '\\' && in_string) {
                if (*(scan + 1) != '\0') {
                    scan += 2; /* Skip escaped character */
                    continue;
                }
            }
            if (*scan == '"') {
                in_string = !in_string;
            } else if (!in_string) {
                if (*scan == '{') brace_depth++;
                else if (*scan == '}') brace_depth--;
            }
            if (brace_depth == 0) {
                break;
            }
            scan++;
        }

        /* If this was the last turn in the requested count, mark the end */
        if (current_turn == (start_turn + count - 1)) {
            slice_end = scan + 1; /* Include the closing '}' */
            break;
        }

        /* Advance to next turn */
        current_turn++;
        p = scan;
    }

    /* Validate that we found the desired slice */
    if (!slice_start || !slice_end || slice_end <= slice_start) {
        free(buf);
        return NULL;
    }

    /* Allocate and copy the raw JSON slice */
    size_t slice_len = slice_end - slice_start;
    char *result = (char *)malloc(slice_len + 1);
    if (result) {
        memcpy(result, slice_start, slice_len);
        result[slice_len] = '\0';
    }

    free(buf);
    return result;
}
