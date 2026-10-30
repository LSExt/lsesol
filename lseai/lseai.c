/*
 * lseai.c - Readline CLI for Gemini API with thread persistence
 * Designed for SunOS 4.1.4 / Solaris 2.5.1 on SPARC
 *
 * Compile:
 *   gcc -std=gnu99 -O2 -o lseai lseai.c -I/usr/local/include -I/usr/local/lse/include -include /usr/local/lse/include/lsecompat.h -L/usr/local/lib -L/usr/local/lse/lib -Wl,-rpath,$prefix/lib -lreadline -lcurses -lcurl -lssl -lcrypto -llsecompat
 */

#include "lseai.h"

/* Set the terminal title when supported */
void set_terminal_title( const char *title){
    if (!title) return;
    printf("\033k%s\033\\", title);
    fflush(stdout);
}

/* Dynamically resolve state file path from environment or default */
const char *get_state_file_path(void) {
    const char *env_path = getenv("GEMINI_STATE_FILE");
    if (env_path && strlen(env_path) > 0) {
        return env_path;
    }
    return DEFAULT_STATE_FILE;
}

/* Standard 7-bit ASCII renderer for SunOS / Solaris TTY buffers */
void print_formatted_response(const char *str) {
    if (!str) return;

    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];

        /* Handle JSON backslash escapes */
        if (c == '\\' && i + 1 < len) {
            char next = str[i + 1];
            switch (next) {
                case 'n': putchar('\n'); i++; continue;
                case 'r': i++; continue;
                case 't': putchar('\t'); i++; continue;
                case '"': putchar('"');  i++; continue;
                case '\\': putchar('\\'); i++; continue;
                case 'u':
                    /* Convert Hex Unicode escapes like \u003e to plain ASCII */
                    if (i + 5 < len) {
                        if (strncasecmp(&str[i + 2], "003e", 4) == 0) { putchar('>'); i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "003c", 4) == 0) { putchar('<'); i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "0026", 4) == 0) { putchar('&'); i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "0022", 4) == 0) { putchar('"'); i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "0027", 4) == 0) { putchar('\''); i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "2022", 4) == 0) { putchar('*'); i += 5; continue; }
                        if (strncasecmp(&str[i + 2], "2013", 4) == 0 ||
                            strncasecmp(&str[i + 2], "2014", 4) == 0) { putchar('-'); i += 5; continue; }
                    }
                    putchar(str[i]);
                    continue;
                default:
                    putchar(str[i]);
                    continue;
            }
        }

        /* Filter high-bit UTF-8 sequences and convert to 7-bit ASCII */
        if (c >= 0x80) {
            if (c == 0xE2 && i + 2 < len && (unsigned char)str[i + 1] == 0x80) {
                unsigned char c3 = (unsigned char)str[i + 2];
                if (c3 == 0x98 || c3 == 0x99) { putchar('\''); i += 2; continue; }
                if (c3 == 0x9C || c3 == 0x9D) { putchar('"');  i += 2; continue; }
                if (c3 == 0x93 || c3 == 0x94) { putchar('-');  i += 2; continue; }
                if (c3 == 0xA2)             { putchar('*');  i += 2; continue; }
                if (c3 == 0xA6)             { printf("..."); i += 2; continue; }
            }
            if ((c & 0xC0) == 0x80) {
                continue;
            }
            continue;
        }

        /* Print only valid printable ASCII, newline, and tab */
        if (c >= 32 || c == '\n' || c == '\t') {
            putchar(c);
        }
    }

    putchar('\n');
    fflush(stdout);
}
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


/* Memory callback for libcurl */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;

    char *ptr = (char *)realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Out of memory in realloc()\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
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

/* Initialize empty thread state if file does not exist */
void init_state_file(void) {
    const char *path = get_state_file_path();
    FILE *f = fopen(path, "r");
    if (!f) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "{\"contents\":[]}");
            fclose(f);
        }
    } else {
        fclose(f);
    }
}


/* Load existing state file into memory buffer */
char *read_state_file(void) {
    const char *path = get_state_file_path();
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc((fsize + 1) * sizeof(char));
    if (buf) {
        size_t bytes_read = fread(buf, 1, fsize, f);
        buf[bytes_read] = '\0';
    }
    fclose(f);
    return buf;
}

/* Append user prompt and model response into thread JSON state */
void update_state_file(const char *user_prompt, const char *raw_resp_json) { char *state = read_state_file();
    if (!state) return;

    /* Locate position right before ending contents array `]}` */
    char *insert_pos = strrchr(state, ']');
    if (!insert_pos) {
        free(state);
        return;
    }

    /* Check if thread already contains previous messages */
    int has_entries = (strstr(state, "\"role\"") != NULL);
    char *escaped_prompt = json_escape(user_prompt);

    /* Extract model response text */
    char *model_text = extract_text_field(raw_resp_json);
    char *escaped_model = model_text ? json_escape(model_text) : strdup("No response text.");

   const char *path = get_state_file_path();
    FILE *f = fopen(path, "w");
    if (f) {
        *insert_pos = '\0';
        fputs(state, f);

        if (has_entries) {
            fputs(",", f);
        }

        /* Write user turn */
        fprintf(f, "{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]},", escaped_prompt);
        /* Write model turn */
        fprintf(f, "{\"role\":\"model\",\"parts\":[{\"text\":\"%s\"}]}", escaped_model);

        fputs("]}", f);
        fclose(f);
    }

    free(state);
    if (escaped_prompt) free(escaped_prompt);
    if (escaped_model) free(escaped_model);
    if (model_text) free(model_text);
}

/* Perform HTTPS POST request using libcurl */
char *send_request(const char *json_payload) {
    const char *api_key = getenv("GEMINI_API_KEY");
    const char *model_name = getenv("GEMINI_MODEL");
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    int retries = 3;
    long http_code = 0;


    if (!api_key) {
        fprintf(stderr, "[Error: GEMINI_API_KEY environment variable is not set]\n");
        return NULL;
    }

    /* Build endpoint URL internally */
    char endpoint[1024];
    snprintf(endpoint, sizeof(endpoint),
             "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
             model_name, api_key);


    headers = curl_slist_append(headers, "Content-Type: application/json");

    while (retries > 0) {
        struct MemoryBuffer chunk = { (char *)malloc(1), 0 };
        if (!chunk.data) return NULL;

        curl = curl_easy_init();
        if (!curl) {
            free(chunk.data);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, endpoint);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res == CURLE_OK && http_code == 200) {
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return chunk.data;
        }

        /* If 503 Service Unavailable, wait 2 seconds and retry */
        if (http_code == 503 && retries > 1) {
            printf("[503 Server Busy - Retrying in 2s... (%d left)]\n", retries - 1);
            sleep(2);
        } else if (res != CURLE_OK) {
            fprintf(stderr, "\ncurl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(chunk.data);
            break;
        } else {
            /* Non-200 HTTP code (e.g., 404, 429) - return raw JSON payload for inspection */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return chunk.data;
        }

        curl_easy_cleanup(curl);
        free(chunk.data);
        retries--;
    }

    curl_slist_free_all(headers);
    return NULL;
}


int main(int argc, char **argv) {
    const char *api_key = getenv("GEMINI_API_KEY");
    const char *model_name = getenv("GEMINI_MODEL");
    const char *state_file = get_state_file_path();

    if (!model_name) {
        model_name = DEFAULT_MODEL;
    }

    set_terminal_title( state_file );

    if (!api_key) {
        fprintf(stderr, "Error: GEMINI_API_KEY environment variable not set.\n");
        fprintf(stderr, "Run: export GEMINI_API_KEY=\"your_key_here\"\n");
        return 1;
    }

    printf("Gemini Terminal Interface (SPARC / libcurl / Readline)\n");
    printf("Active Model: %s\n", model_name);
    printf("Conversation Thread: %s\n", state_file);
    printf("Type 'exit' or 'quit' to end session. \n" );
    printf("Reset thread with '/reset'. Summarize thread with '/summarize'.\n\n");
    init_state_file();


    char *line;
    while ((line = read_multiline_prompt(state_file)) != NULL) {
        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        add_history(line);

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            free(line);
            break;
        }

        if (strcmp(line, "/reset") == 0) {
            const char *path = get_state_file_path();
            unlink(path);
            init_state_file();
            printf("Conversation thread cleared for: %s\n\n", path);
            free(line);
            continue;
        }
        if (strcmp(line, "/compact") == 0|| strcmp(line, "/summarize") == 0 ) {
            const char *path = get_state_file_path();
            compact_state_file();
            printf("Conversation summarized: %s\n\n", path);
            continue;
        }

        /* Load thread state and insert current prompt */
        char *current_state = read_state_file();
        if (!current_state) {
            free(line);
            continue;
        }

        char *insert_pos = strrchr(current_state, ']');
        if (insert_pos) *insert_pos = '\0';

        int has_entries = (strstr(current_state, "\"role\"") != NULL);
        char *escaped_prompt = json_escape(line);

        /* Construct full JSON payload with complete history */
        size_t payload_size = strlen(current_state) + (escaped_prompt ? strlen(escaped_prompt) : 0) + 512;
        char *full_payload = (char *)malloc(payload_size * sizeof(char));
        if (!full_payload) {
            fprintf(stderr, "Memory allocation failed for payload.\n");
            free(current_state);
            if (escaped_prompt) free(escaped_prompt);
            free(line);
            continue;
        }

        sprintf(full_payload, "%s%s{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}]}",
                current_state, (has_entries ? "," : ""), escaped_prompt ? escaped_prompt : "");

        printf("Thinking...\n");
        char *response_json = send_request(full_payload);

        if (response_json) {
            char *text = extract_text_field(response_json);
            if (text) {
                printf("\nGemini:\n");
                print_formatted_response(text);
                printf("\n");
                update_state_file(line, response_json);
                free(text);
            } else {
                /* Fall back to printing raw response if parsing ever fails */
                printf("\nGemini (Raw Output):\n");
                print_formatted_response(response_json);
                printf("\n");
            }
            free(response_json);
        }

        free(current_state);
        if (escaped_prompt) free(escaped_prompt);
        free(full_payload);
        free(line);
    }

    return 0;
}
