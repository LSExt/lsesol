#include "lseai.h"
#include "globals.h"

/* Dynamically resolve state file path from environment or default */
const char *get_state_file_path(void) {
    if (g_current_conversation[0] != '\0' && strlen(g_current_conversation) > 0) {
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
               

/* Helper to check if file size or turns exceed g_config limits.
 * Optionally writes back the file size (in bytes)
 * if out_turns or out_size are non-NULL.
 */
int should_auto_summarize(const char *filepath, int turn_count, long *out_size)
{
    long current_size = 0;
    int trigger = 0;

    /* 1. Gather current file size if path is valid */
    if (filepath && filepath[0] != '\0') {
        struct stat st;
        if (stat(filepath, &st) == 0) {
            current_size = (long)st.st_size;
        }
    }

    if (out_size) {
        *out_size = current_size;
    }

    /* Master switch in config */
    if (!g_config.auto_summarize) {
        return 0;
    }

    /* 3. Turn count trigger */
    if (g_config.max_turns > 0 && turn_count >= g_config.max_turns) {
        printf("[Auto-Summarize Trigger] Conversation turns (%d) >= max_turns (%d)\n",
               turn_count, g_config.max_turns);
        trigger = 1;
    }

    /* 4. File size trigger */
    if (g_config.max_size_bytes > 0 && current_size >= g_config.max_size_bytes) {
        printf("[Auto-Summarize Trigger] Conversation size (%ld bytes) >= max_size (%ld bytes)\n",
               current_size, g_config.max_size_bytes);
        trigger = 1;
    }

    return trigger;
}

int perform_auto_summarization(const char *filepath, int total_turns)
{
    int keep_count = (g_config.preserve_turns > 0) ? g_config.preserve_turns : 4;
    int turns_to_compact = total_turns - keep_count;

    printf("[Auto-Summarize] Starting: total=%d, keep=%d, compacting=%d\n",
           total_turns, keep_count, turns_to_compact);

    if (turns_to_compact <= 0) {
        printf("[Auto-Summarize] Aborted: turns_to_compact <= 0\n");
        return 0;
    }

    /* 1. Extract plain text representation of turns */
    char *text_payload = extract_turns_for_summary(filepath, 0, turns_to_compact);
    if (!text_payload) {
        fprintf(stderr, "[Auto-Summarize] FAILED: extract_turns_for_summary returned NULL\n");
        return -1;
    }
    printf("[Auto-Summarize] Extracted %lu bytes of text to summarize\n", (unsigned long)strlen(text_payload));

    /* 2. Call API */
    printf("[Auto-Summarize] Sending summary request to API...\n");
    char *new_summary = request_gemini_summary(text_payload);
    free(text_payload);

    if (!new_summary) {
        fprintf(stderr, "[Auto-Summarize] FAILED: request_gemini_summary returned NULL (check API key / model name!)\n");
        return -1;
    }
    printf("[Auto-Summarize] Received summary from API (%lu bytes)\n", (unsigned long)strlen(new_summary));

    /* 3. Save compacted file */
    if (save_compacted_state_file(filepath, new_summary, keep_count) != 0) {
        fprintf(stderr, "[Auto-Summarize] FAILED: save_compacted_state_file returned error\n");
        free(new_summary);
        return -1;
    }

    free(new_summary);
    printf("[Auto-Summarize] SUCCESS: Conversation compacted successfully!\n");
    return 0;
}
