#include "lseai.h"

/* Collects multi-line buffer until user hits Ctrl+D on a new line */
char *read_multiline_prompt(const char *title) {
    char *full_buffer = NULL;
    size_t total_len = 0;

    printf("conversation: %s\n", title );
    printf("lseai> (Paste text, press Ctrl+D on a new line to submit)\n");

    while (1) {
        char *line = readline("... ");

        /* EOF (Ctrl+D pressed) */
        if (!line) {
            printf("\n");
            break;
        }

        /* Check for single-line exit/reset commands before submitting */
        if (total_len == 0) {
            if (strcmp(line, "exit") == 0 ||
                strcmp(line, "quit") == 0 ||
                strcmp(line, "/reset") == 0 ||
                strcmp(line, "/compact") == 0 ||
                strcmp(line, "/summarize") == 0) {
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
