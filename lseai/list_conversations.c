#include "lseai.h"

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "config.h"


void list_conversations(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open conversation directory '%s': %s\n",
                dir_path, strerror(errno));
        return;
    }

    printf("Available conversations in %s:\n", dir_path);
    printf("--------------------------------------------------\n");

    struct dirent *entry;
    int count = 0;
    const char *suffix = ".gemini.json";
    size_t sfx_len = strlen(suffix);

    while ((entry = readdir(dir)) != NULL) {
        size_t name_len = strlen(entry->d_name);
        
        /* Must be longer than ".gemini.json" and match suffix */
        if (name_len > sfx_len &&
            strcmp(entry->d_name + (name_len - sfx_len), suffix) == 0) {
            
            /* Extract context name without .gemini.json */
            char base_name[MAX_NAME_LEN];
            size_t base_len = name_len - sfx_len;
            if (base_len >= sizeof(base_name)) {
                base_len = sizeof(base_name) - 1;
            }
            strncpy(base_name, entry->d_name, base_len);
            base_name[base_len] = '\0';

            /* Optional: Check file size or modified time via stat */
            char full_path[MAX_PATH_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                printf("  %-24s (%ld bytes)\n", base_name, (long)st.st_size);
            } else {
                printf("  %s\n", base_name);
            }
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        printf("  (No conversations found)\n");
    }
    printf("--------------------------------------------------\n");
    printf("Resume with: lseai --context-name <name>\n");
}
