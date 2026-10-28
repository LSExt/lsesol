#include "aichat.h"

/* Option A: Summarize and compact thread history into a baseline context block */
void compact_state_file(void) {
    const char *path = get_state_file_path();

    /* Read current thread history from state file */
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("[Compact] No state file found at %s\n\n", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 20) {
        fclose(f);
        printf("[Compact] State file is empty.\n\n");
        return;
    }

    char *history_json = (char *)malloc(sz + 1);
    if (!history_json) {
        fclose(f);
        return;
    }

    fread(history_json, 1, sz, f);
    history_json[sz] = '\0';
    fclose(f);

    printf("Compacting thread context...\n");

    const char *compact_prompt = 
        "Summarize our technical discussion so far into a concise bulleted list. "
        "Include critical environment constraints, code changes, file paths, "
        "and key decisions so we can continue seamlessly from this summary.";

    /* Inject compaction prompt into history payload */
    size_t payload_sz = sz + strlen(compact_prompt) + 256;
    char *payload = (char *)malloc(payload_sz);
    if (!payload) {
        free(history_json);
        return;
    }

    char *close_bracket = strrchr(history_json, ']');
    if (close_bracket) {
        *close_bracket = '\0';
        snprintf(payload, payload_sz, 
            "%s,{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}]}", 
            history_json, compact_prompt);
    } else {
        snprintf(payload, payload_sz, 
            "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}]}", 
            compact_prompt);
    }

    free(history_json);

    /* Clean 1-argument call */
    char *resp_json = send_request(payload);
    free(payload);

    if (!resp_json) {
        printf("[Error: Compaction API request failed]\n\n");
        return;
    }

    char *summary_text = extract_text_field(resp_json);
    free(resp_json);

    if (!summary_text) {
        printf("[Error: Failed to parse summary from API response]\n\n");
        return;
    }

    /* Render summary and save to disk */
    printf("\n=== Compacted Thread Summary ===\n");
    print_formatted_response(summary_text);
    printf("================================\n\n");

    f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"contents\":["
                   "{\"role\":\"user\",\"parts\":[{\"text\":\"SYSTEM CONTEXT / PRIOR DISCUSSION SUMMARY:\\n%s\"}]},"
                   "{\"role\":\"model\",\"parts\":[{\"text\":\"Understood. Context saved.\"}]}"
                   "]}", summary_text);
        fclose(f);
        printf("Thread context successfully compacted -> %s\n\n", path);
    } else {
        printf("[Error: Failed to overwrite state file %s]\n\n", path);
    }

    free(summary_text);
}
