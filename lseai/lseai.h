#ifndef LSEAI_H
#define LSEAI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define DEFAULT_STATE_FILE "/tmp/gemini_thread.json"
#define DEFAULT_MODEL "gemini-2.5-flash"
#define BUFFER_SIZE 65536

struct MemoryBuffer {
    char *data;
    size_t size;
};


/* Prototypes */
const char *get_state_file_path(void);
char *send_request(const char *json_payload);
char *extract_text_field(const char *json_resp);
void print_formatted_response(const char *str);
void compact_state_file(void);
char *read_multiline_prompt(const char *title);


#endif /* LSEAI_H */
