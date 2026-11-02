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
#define DEFAULT_MODEL "gemini-3.8-flash"
#define BUFFER_SIZE 65536

struct MemoryBuffer {
    char *data;
    size_t size;
};

/* json */
char *json_escape(const char *src);
char *extract_text_field(const char *json_resp);

/* Prototypes */
const char *get_state_file_path(void);
char *extract_text_field(const char *json_resp);
void compact_state_file(void);
char *read_multiline_prompt(const char *title);
/* state file */
void init_state_file(void);
char *read_state_file(void);
const char *get_state_file_path(void);
void update_state_file(const char *user_prompt, const char *raw_resp_json);

/* Request */
char *send_request(const char *json_payload);

/* Response */
char *clean_to_ascii_string(const char *input);
void print_formatted_response(const char *str);

#define PROMPT_HEADER   "\n=== [ You ] ====================================================\n"
#define RESPONSE_HEADER "\n--- [ Gemini ] -------------------------------------------------\n"
#define TURN_DIVIDER    "\n================================================================\n\n"


#endif /* LSEAI_H */
