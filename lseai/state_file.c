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
               

/* Helper to check if file size or turns exceed g_config limits */
int should_auto_summarize(const char *filepath, int turn_count)
{
    /* Master switch in config */
    if (!g_config.auto_summarize) {
        return 0;
    }

    /* 1. Turn count trigger */
    if (g_config.max_turns > 0 && turn_count >= g_config.max_turns) {
        printf("[Auto-Summarize Trigger] Conversation turns (%d) >= max_turns (%d)\n",
               turn_count, g_config.max_turns);
        return 1;
    }

    /* 2. File size trigger */
    if (g_config.max_size_bytes > 0 && filepath && strlen(filepath) > 0) {
        struct stat st;
        if (stat(filepath, &st) == 0 && st.st_size >= g_config.max_size_bytes) {
            printf("[Auto-Summarize Trigger] Conversation size (%ld bytes) >= max_size (%ld bytes)\n",
                   (long)st.st_size, g_config.max_size_bytes);
            return 1;
        }
    }

    return 0;
}

int perform_auto_summarization(const char *filepath, int total_turns)
{
    /* Determine how many historical turns need compression */
    int keep_count = (g_config.preserve_turns > 0) ? g_config.preserve_turns : 2;
    int turns_to_compact = total_turns - keep_count;

    if (turns_to_compact <= 0) {
        return 0; /* Not enough history to compact while respecting preserve_turns */
    }

    printf("[Auto-Summarize] Compacting %d of %d turns (preserving last %d)...\n",
           turns_to_compact, total_turns, keep_count);

    /* 1. Format the older turns to be digested by the model */
    char *text_payload = extract_turns_for_summary(filepath, 0, turns_to_compact);
    if (!text_payload) {
        fprintf(stderr, "[Auto-Summarize] Error extracting turns\n");
        return -1;
    }

    /* 2. Execute summary request using your existing summarize.o engine */
    char *new_summary = request_gemini_summary(text_payload);
    free(text_payload);

    if (!new_summary) {
        fprintf(stderr, "[Auto-Summarize] Model API summary failed\n");
        return -1;
    }

    /* 3. Re-serialize state file: update summary record and prune old turns */
    if (compact_state_file() != 0) {
        fprintf(stderr, "[Auto-Summarize] Failed to write updated state file\n");
        free(new_summary);
        return -1;
    }

    free(new_summary);
    return 0;
}
