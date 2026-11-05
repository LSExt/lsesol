/* globals.h */
#ifndef GLOBALS_H
#define GLOBALS_H

/* Declaration only - NO memory is allocated here */
extern int g_turn_count;
extern char g_current_model[64];
extern char g_current_conversation[1024];
extern lseai_config_t g_config;

extern char **g_files_to_attach;
extern int    g_num_attachments;

void add_attachment(const char *filepath);
void clear_attachments(void);

#endif
