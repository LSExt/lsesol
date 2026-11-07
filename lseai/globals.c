/* globals.c - Single allocation of shared globals */
#include "lseai.h"
#include "config.h"
#include "globals.h"

int g_turn_count = 0;
char g_current_model[MAX_NAME_LEN] = "gemini-3.8-flash";
int conv_count = 0;
char g_current_conversation[1024] = "";
int g_verbose = 0;


char **g_files_to_attach = NULL;
int    g_num_attachments = 0;

void add_attachment(const char *filepath) {
    if (!filepath) return;
    g_files_to_attach = (char **)realloc(g_files_to_attach, sizeof(char *) * (g_num_attachments + 1));
    g_files_to_attach[g_num_attachments] = strdup(filepath);
    g_num_attachments++;
}

void clear_attachments(void) {
    if (!g_files_to_attach) return;
    for (int i = 0; i < g_num_attachments; i++) {
        if (g_files_to_attach[i]) {
            free(g_files_to_attach[i]);
        }
    }
    free(g_files_to_attach);
    g_files_to_attach = NULL;
    g_num_attachments = 0;
}
