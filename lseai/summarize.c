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
    if (!filepath || !summary_text) return -1;

    /* Escape summary text for JSON serialization */
    char *escaped_summary = json_escape(summary_text);
    if (!escaped_summary) return -1;

    /*
     * If preserve_last_n > 0, extract the preserved tail turns from original file
     * to append them after the summary.
     */
    char *preserved_tail = NULL;
    if (preserve_last_n > 0) {
        int total_turns = count_conversation_turns(filepath);
        if (total_turns > preserve_last_n) {
            int start_idx = total_turns - preserve_last_n;
            /* Extract JSON chunk for the preserved turns */
            preserved_tail = extract_raw_turns_json(filepath, start_idx, preserve_last_n);
        }
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "[Error: Failed to open %s for writing]\n", filepath);
        free(escaped_summary);
        if (preserved_tail) free(preserved_tail);
        return -1;
    }

    /* Write summary as foundational context */
    fprintf(f, "{\"contents\":["
               "{\"role\":\"user\",\"parts\":[{\"text\":\"SYSTEM CONTEXT / PRIOR DISCUSSION SUMMARY:\\n%s\"}]},"
               "{\"role\":\"model\",\"parts\":[{\"text\":\"Understood. Context saved.\"}]}",
            escaped_summary);

    /* Append preserved recent turns if available */
    if (preserved_tail && strlen(preserved_tail) > 0) {
        fprintf(f, ",%s", preserved_tail);
        free(preserved_tail);
    }

    fprintf(f, "]}");
    fclose(f);
    free(escaped_summary);

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
