#include "lseai.h"

/*
 * Step 1: Send text to the Gemini API and extract the summary.
 * Returns dynamically allocated string on success (caller must free), NULL on failure.
 */
char *request_gemini_summary(const char *text_to_summarize)
{
    if (!text_to_summarize || strlen(text_to_summarize) == 0) {
        return NULL;
    }

    const char *instructions =
        "Summarize our technical discussion into a concise bulleted list. "
        "Preserve critical environment constraints (Solaris, OS versions, C standards), "
        "code changes, file paths, and key decisions so we can continue seamlessly from this summary.";

    /* Escape text for JSON safety */
    char *escaped_input = json_escape(text_to_summarize);
    if (!escaped_input) return NULL;

    char *escaped_inst = json_escape(instructions);
    if (!escaped_inst) {
        free(escaped_input);
        return NULL;
    }

    /* Build payload */
    size_t payload_sz = strlen(escaped_input) + strlen(escaped_inst) + 512;
    char *payload = (char *)malloc(payload_sz);
    if (!payload) {
        free(escaped_input);
        free(escaped_inst);
        return NULL;
    }

    snprintf(payload, payload_sz,
        "{\"contents\":["
        "{\"role\":\"user\",\"parts\":[{\"text\":\"%s\\n\\n%s\"}]}"
        "]}",
        escaped_inst, escaped_input);

    free(escaped_input);
    free(escaped_inst);

    /* Dispatch to API */
    char *resp_json = send_request(payload);
    free(payload);

    if (!resp_json) {
        fprintf(stderr, "[Error: Summary API request failed]\n");
        return NULL;
    }

    /* Extract response text */
    char *summary_text = extract_text_field(resp_json);
    free(resp_json);

    return summary_text;
}

/*
 * Step 2: Overwrite state file with summary context and preserve recent turns.
 * Returns 0 on success, -1 on failure.
 */
int save_compacted_state_file(const char *filepath, const char *summary_text, int preserve_last_n)
{
    printf("\n=== [DEBUG save_compacted_state_file START] ===\n");
    printf("[DEBUG] Target Filepath:      %s\n", filepath ? filepath : "(NULL)");
    printf("[DEBUG] Preserve Last N:      %d\n", preserve_last_n);
    printf("[DEBUG] Summary Length:       %lu bytes\n", summary_text ? (unsigned long)strlen(summary_text) : 0);

    if (!filepath || !summary_text) {
        fprintf(stderr, "[DEBUG ERROR] Invalid NULL arguments (filepath=%p, summary_text=%p)\n",
                (void*)filepath, (void*)summary_text);
        return -1;
    }

    /* Escape summary text for JSON serialization */
    char *escaped_summary = json_escape(summary_text);
    if (!escaped_summary) {
        fprintf(stderr, "[DEBUG ERROR] json_escape() returned NULL on summary_text!\n");
        return -1;
    }
    printf("[DEBUG] Escaped Summary Len:  %lu bytes\n", (unsigned long)strlen(escaped_summary));

    /*
     * If preserve_last_n > 0, extract the preserved tail turns from original file
     * to append them after the summary.
     */
    char *preserved_tail = NULL;
    if (preserve_last_n > 0) {
        int total_turns = count_conversation_turns(filepath);
        printf("[DEBUG] Pre-compaction Turns: %d\n", total_turns);

        if (total_turns > preserve_last_n) {
            int start_idx = total_turns - preserve_last_n;
            printf("[DEBUG] Calling extract_raw_turns_json(start_idx=%d, count=%d)\n",
                   start_idx, preserve_last_n);

            preserved_tail = extract_raw_turns_json(filepath, start_idx, preserve_last_n);

            if (!preserved_tail) {
                fprintf(stderr, "[DEBUG WARNING] extract_raw_turns_json returned NULL! Recent turns will NOT be preserved.\n");
            } else {
                printf("[DEBUG] Extracted preserved tail: %lu bytes\n", (unsigned long)strlen(preserved_tail));
                /* Print first 80 characters of tail to verify JSON integrity */
                char preview[81];
                strncpy(preview, preserved_tail, 80);
                preview[80] = '\0';
                printf("[DEBUG] Tail preview: [%s...]\n", preview);
            }
        } else {
            printf("[DEBUG] total_turns (%d) <= preserve_last_n (%d), skipping tail extraction\n",
                   total_turns, preserve_last_n);
        }
    }

    printf("[DEBUG] Opening file for overwrite: %s\n", filepath);
    FILE *f = fopen(filepath, "w");
    if (!f) {
        perror("[DEBUG ERROR] Failed to open file for writing");
        free(escaped_summary);
        if (preserved_tail) free(preserved_tail);
        return -1;
    }

    /* Write summary as foundational context */
    int w1 = fprintf(f, "{\"contents\":["
                        "{\"role\":\"user\",\"parts\":[{\"text\":\"SYSTEM CONTEXT / PRIOR DISCUSSION SUMMARY:\\n%s\"}]},"
                        "{\"role\":\"model\",\"parts\":[{\"text\":\"Understood. Context saved.\"}]}",
                     escaped_summary);
    printf("[DEBUG] Wrote baseline context: %d bytes\n", w1);

    /* Append preserved recent turns if available */
    if (preserved_tail && strlen(preserved_tail) > 0) {
        int w2 = fprintf(f, ",%s", preserved_tail);
        printf("[DEBUG] Appended preserved tail: %d bytes\n", w2);
        free(preserved_tail);
    }

    int w3 = fprintf(f, "]}");
    printf("[DEBUG] Wrote closing brackets: %d bytes\n", w3);

    fclose(f);
    free(escaped_summary);

    /* Verification step: reread disk to confirm turn count */
    int new_turns = count_conversation_turns(filepath);
    printf("[DEBUG] Post-compaction Verification: file now has %d turns on disk\n", new_turns);
    printf("=== [DEBUG save_compacted_state_file END (SUCCESS)] ===\n\n");

    return 0;
}
/*
 * Step 3: Top-level manual / CLI compaction routine
 */
int compact_state_file(void)
{
    const char *path = get_state_file_path();
    if (!path) return -1;

    int total_turns = count_conversation_turns(path);
    if (total_turns <= 2) {
        printf("[Compact] Conversation has too few turns to compact (%d).\n\n", total_turns);
        return 0;
    }

    /* Keep preserve_turns from config, or default to 2 */
    int keep = (g_config.preserve_turns > 0) ? g_config.preserve_turns : 2;
    int turns_to_compact = total_turns - keep;
    if (turns_to_compact <= 0) turns_to_compact = total_turns;

    printf("Compacting %d turns of thread context...\n", turns_to_compact);

    /* 1. Extract plain text representation of turns */
    char *extracted_text = extract_turns_for_summary(path, 0, turns_to_compact);
    if (!extracted_text) {
        printf("[Error: Failed to read conversation turns from file]\n\n");
        return -1;
    }

    /* 2. Request summary from model (in-memory) */
    char *summary_text = request_gemini_summary(extracted_text);
    free(extracted_text);

    if (!summary_text) {
        printf("[Error: Compaction API request failed]\n\n");
        return -1;
    }

    /* 3. Output to CLI or Motif text widget */
#ifdef _MOTIF
    /* Send directly to the conversation view window */
    if (output_text_w) {
        char *cleaned = clean_to_ascii_string(summary_text);
        if (cleaned) {
            char banner[512];
            snprintf(banner, sizeof(banner), "\n--- [Context Compacted (%d turns)] ---\n", turns_to_compact);
            XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), banner);
            XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), cleaned);
            XmTextShowPosition(output_text_w, XmTextGetLastPosition(output_text_w));
            free(cleaned);
        }
    }
#else
    printf("\n=== Compacted Thread Summary ===\n");
    print_formatted_response(summary_text);
    printf("================================\n\n");
#endif

    /* 4. Write back to disk */
    if (save_compacted_state_file(path, summary_text, keep) == 0) {
        printf("Thread context successfully compacted -> %s\n\n", path);
    } else {
        printf("[Error: Failed to overwrite state file %s]\n\n", path);
    }

    free(summary_text);
    return 0;
}
