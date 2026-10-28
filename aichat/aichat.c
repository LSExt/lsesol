/*
 * aichat.c - Readline CLI for Gemini API with thread persistence
 * Designed for SunOS 4.1.4 / Solaris 2.5.1 on SPARC
 *
 * Compile:
 *   gcc -std=gnu99 -O2 -o aichat aichat.c -I/usr/local/include -I/usr/local/lse/include -include /usr/local/lse/include/lsecompat.h -L/usr/local/lib -L/usr/local/lse/lib -Wl,-rpath,$prefix/lib -lreadline -lcurses -lcurl -lssl -lcrypto -llsecompat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define STATE_FILE "/tmp/gemini_thread.json"
#define DEFAULT_MODEL "gemini-2.5-flash"
#define BUFFER_SIZE 65536

struct MemoryBuffer {
    char *data;
    size_t size;
};

/* Unescape JSON response strings for SPARC terminal rendering */
void print_formatted_response(const char *str) {
    if (!str) return;

    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\\' && i + 1 < len) {
            switch (str[i + 1]) {
                case 'n': 
                    putchar('\n'); 
                    i++; 
                    break;
                case 'r': 
                    i++; 
                    break;
                case 't': 
                    putchar('\t'); 
                    i++; 
                    break;
                case '"': 
                    putchar('"');  
                    i++; 
                    break;
                case '\\': 
                    putchar('\\'); 
                    i++; 
                    break;
                case 'u':
                    /* Unescape \u003e ('>') and \u003c ('<') */
                    if (i + 5 < len) {
                        if (strncasecmp(&str[i + 2], "003e", 4) == 0) {
                            putchar('>');
                            i += 5;
                            break;
                        } else if (strncasecmp(&str[i + 2], "003c", 4) == 0) {
                            putchar('<');
                            i += 5;
                            break;
                        }
                    }
                    putchar(str[i]);
                    break;
                default: 
                    putchar(str[i]); 
                    break;
            }
        } else {
            putchar(str[i]);
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

/* Collects multi-line buffer until user hits Ctrl+D on a new line */
char *read_multiline_prompt(void) {
    char *full_buffer = NULL;
    size_t total_len = 0;

    printf("sparc-gemini> (Paste text, press Ctrl+D on a new line to submit)\n");

    while (1) {
        char *line = readline("... ");

        /* EOF (Ctrl+D pressed) */
        if (!line) {
            printf("\n");
            break;
        }

        /* Check for single-line exit/reset commands before submitting */
        if (total_len == 0) {
            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "/reset") == 0) {
                return line;
            }
        }

        size_t line_len = strlen(line);
        size_t new_len = total_len + line_len + 2;

        char *new_buf = (char *)realloc(full_buffer, new_len);
        if (!new_buf) {
            free(line);
            if (full_buffer) free(full_buffer);
            return NULL;
        }

        full_buffer = new_buf;
        if (total_len == 0) {
            strcpy(full_buffer, line);
        } else {
            strcat(full_buffer, "\n");
            strcat(full_buffer, line);
        }

        total_len = strlen(full_buffer);
        free(line);
    }

    if (full_buffer && strlen(full_buffer) > 0) {
        add_history(full_buffer);
    }

    return full_buffer;
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
    if (access(STATE_FILE, F_OK) != 0) {
        FILE *f = fopen(STATE_FILE, "w");
        if (f) {
            chmod(STATE_FILE, 0600);
            fputs("{\"contents\":[]}", f);
            fclose(f);
        }
    }
}

/* Load existing state file into memory buffer */
char *read_state_file(void) {
    FILE *f = fopen(STATE_FILE, "r");
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
void update_state_file(const char *user_prompt, const char *raw_resp_json) {
    char *state = read_state_file();
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

    FILE *f = fopen(STATE_FILE, "w");
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
char *send_request(const char *endpoint, const char *json_payload) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    int retries = 3;
    long http_code = 0;

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

    if (!model_name) {
        model_name = DEFAULT_MODEL;
    }

    if (!api_key) {
        fprintf(stderr, "Error: GEMINI_API_KEY environment variable not set.\n");
        fprintf(stderr, "Run: export GEMINI_API_KEY=\"your_key_here\"\n");
        return 1;
    }

    char endpoint[1024];
    snprintf(endpoint, sizeof(endpoint),
             "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
             model_name, api_key);

    printf("Gemini Terminal Interface (SPARC / libcurl / Readline)\n");
    printf("Active Model: %s\n", model_name);
    printf("Type 'exit' or 'quit' to end session. Reset thread with '/reset'.\n\n");
    init_state_file();


    char *line;
    /*while ((line = readline("sparc-gemini> ")) != NULL) {*/
    while ((line = read_multiline_prompt()) != NULL) {
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
            unlink(STATE_FILE);
            init_state_file();
            printf("Conversation history cleared.\n\n");
            free(line);
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
        char *response_json = send_request(endpoint, full_payload);

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
