#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "config.h"

lseai_config_t g_config;

/* Helper: Trim leading and trailing whitespace in-place */
static char *trim_whitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return str;
}

/* Helper: Expand leading ~ to user's HOME directory */
static void expand_home_path(const char *src, char *dst, size_t dst_size)
{
    if (src[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(dst, dst_size, "%s%s", home, src + 1);
            return;
        }
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/* Set built-in default values */
static void set_default_config(void)
{
    expand_home_path("~/.lseai/conversations", g_config.state_dir, sizeof(g_config.state_dir));
    strncpy(g_config.default_model, "gemini-2.5-flash", sizeof(g_config.default_model) - 1);
    strncpy(g_config.api_endpoint_base,
            "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent",
            sizeof(g_config.api_endpoint_base) - 1);

    g_config.model_count = 0;
    strncpy(g_config.models[g_config.model_count++], "gemini-3.8-flash", MAX_NAME_LEN - 1);
    strncpy(g_config.models[g_config.model_count++], "gemini-1.5-pro", MAX_NAME_LEN - 1);

    /* Auto-summarize defaults */
    g_config.auto_summarize  = 1;          /* Enabled by default */
    g_config.max_turns       = 20;         /* Compact after 20 turns */
    g_config.max_size_bytes  = 32 * 1024;  /* 32 KB threshold */
    g_config.preserve_turns  = 4;          /* Always retain the last 4 exchanges */
}

/* Parse comma-separated list of models */
static void parse_models_list(char *val)
{
    char *token = strtok(val, ",");
    g_config.model_count = 0;

    while (token && g_config.model_count < MAX_MODELS) {
        char *m = trim_whitespace(token);
        if (strlen(m) > 0) {
            strncpy(g_config.models[g_config.model_count++], m, MAX_NAME_LEN - 1);
        }
        token = strtok(NULL, ",");
    }
}

static int check_config_security(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        /* Check if group or others have read/write/execute permissions (0077) */
        if (st.st_mode & (S_IRWXG | S_IRWXO)) {
            fprintf(stderr, "WARNING: %s has insecure permissions (mode 0%o)!\n", 
                    path, (unsigned int)(st.st_mode & 0777));
            fprintf(stderr, "Enforcing private access (chmod 0600 %s)...\n", path);
            chmod(path, 0600);
        }
    }
    return 0;
}

int config_load(const char *custom_path)
{
    char conf_path[MAX_PATH_LEN];
    FILE *fp;
    char line[1024];

    set_default_config();

    if (custom_path) {
        strncpy(conf_path, custom_path, sizeof(conf_path) - 1);
    } else {
        expand_home_path("~/.lseaiconf", conf_path, sizeof(conf_path));
    }

    check_config_security( conf_path );

    fp = fopen(conf_path, "r");
    if (!fp) {
        /* Not an error: defaults will be used */
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *p = trim_whitespace(line);

        /* Skip comments and empty lines */
        if (*p == '\0' || *p == '#' || *p == ';') {
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_whitespace(p);
        char *val = trim_whitespace(eq + 1);

        if (strcasecmp(key, "state_dir") == 0) {
            expand_home_path(val, g_config.state_dir, sizeof(g_config.state_dir));
        } else if (strcasecmp(key, "default_model") == 0) {
            strncpy(g_config.default_model, val, sizeof(g_config.default_model) - 1);
        } else if (strcasecmp(key, "api_endpoint_base") == 0) {
            strncpy(g_config.api_endpoint_base, val, sizeof(g_config.api_endpoint_base) - 1);
        } else if (strcasecmp(key, "models") == 0) {
            parse_models_list(val);
        } else  if (strcmp(key, "auto_summarize") == 0) {
            g_config.auto_summarize = (strcmp(val, "true") == 0 || atoi(val) == 1) ? 1 : 0;
        } 
        else if (strcmp(key, "max_turns") == 0) {
            g_config.max_turns = atoi(val);
        } 
        else if (strcmp(key, "max_size_bytes") == 0) {
            g_config.max_size_bytes = atol(val);
        } 
        else if (strcmp(key, "preserve_turns") == 0) {
            g_config.preserve_turns = atoi(val);
        }
    }

    fclose(fp);

    /* Ensure directory exists (mkdir -p mode 0700) */
    mkdir(g_config.state_dir, 0700);

    return 1;
}

/*
 * Scans state_dir for *.gemini.json files and returns an array of chat names.
 * Example: "work-notes.gemini.json" -> returns "work-notes"
 */
char **config_list_conversations(int *out_count)
{
    DIR *dir;
    struct dirent *entry;
    char **names = NULL;
    int count = 0;
    const char *suffix = ".gemini.json";
    size_t s_len = strlen(suffix);

    *out_count = 0;
    dir = opendir(g_config.state_dir);
    if (!dir) return NULL;

    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > s_len && strcmp(entry->d_name + (len - s_len), suffix) == 0) {
            names = (char **)realloc(names, sizeof(char *) * (count + 1));
            size_t name_len = len - s_len;
            names[count] = (char *)malloc(name_len + 1);
            strncpy(names[count], entry->d_name, name_len);
            names[count][name_len] = '\0';
            count++;
        }
    }
    closedir(dir);

    *out_count = count;
    return names;
}

void config_free_conversation_list(char **list, int count)
{
    if (!list) return;
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}
