#include "lseai.h"
#include "globals.h"

/* Dynamically resolve state file path from environment or default */
const char *get_state_file_path(void) {
/*    const char *env_path = getenv("GEMINI_STATE_FILE");*/
    /* const char *env_path =   g_current_conversation; */
/*    if (env_path && strlen(env_path) > 0) {
*/
    if (g_current_conversation && strlen(g_current_conversation) > 0) {
        fprintf( stderr, "current conversation %s\n", g_current_conversation);
        return g_current_conversation;
    }
    return DEFAULT_STATE_FILE;
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
               
