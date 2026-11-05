#ifndef LSEAI_CONFIG_H
#define LSEAI_CONFIG_H

#define MAX_MODELS 16
#define MAX_PATH_LEN 1024
#define MAX_NAME_LEN 64

typedef struct {
    char state_dir[MAX_PATH_LEN];
    char default_model[MAX_NAME_LEN];
    char api_endpoint_base[MAX_PATH_LEN];

    /* Available models list */
    char models[MAX_MODELS][MAX_NAME_LEN];
    int  model_count;
    
    /* --- Auto-Summarization Config --- */
    int    auto_summarize;      /* 1 = Enabled (default), 0 = Disabled */
    int    max_turns;           /* e.g., 20 turns before compaction */
    long   max_size_bytes;      /* e.g., 32768 (32 KB) before compaction */
    int    preserve_turns;      /* Number of recent turns to leave uncompacted */
} lseai_config_t;

/* Global configuration instance */
extern lseai_config_t g_config;

/* Load config from path (or checks default ~/.lseaiconf) */
int config_load(const char *custom_path);

/* Directory scanner: returns allocated list of available chat names */
char **config_list_conversations(int *out_count);
void config_free_conversation_list(char **list, int count);

#endif /* LSEAI_CONFIG_H */
