#ifndef LSEAI_H
#define LSEAI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "config.h"
#define DEFAULT_STATE_FILE "/tmp/gemini_thread.json"
#define DEFAULT_MODEL "gemini-3.8-flash"
#define BUFFER_SIZE 65536

#define true 1
#define false 0

struct MemoryBuffer {
    char *data;
    size_t size;
};

/* json */
char *json_escape(const char *src);
char *extract_text_field(const char *json_resp);
char *extract_turns_for_summary(const char *filepath, int start_turn, int count);
char *extract_raw_turns_json(const char *filepath, int start_turn, int count);


/* Prototypes */
const char *get_state_file_path(void);
char *extract_text_field(const char *json_resp);
int compact_state_file(void);
char *read_multiline_prompt(const char *title);
char *format_markdown_to_ascii(const char *src);
/* state file */
void init_state_file(void);
char *read_state_file(void);
const char *get_state_file_path(void);
void update_state_file(const char *user_prompt, const char *raw_resp_json);
int should_auto_summarize(const char *filepath, int turn_count, long *out_size);
int perform_auto_summarization(const char *filepath, int total_turns);
int count_conversation_turns(const char *filepath);
int save_compacted_state_file(const char *filepath, const char *summary_text, int preserve_last_n);

/* summary */
char *request_gemini_summary(const char *text_to_summarize);

/* Request */
char *send_request(const char *json_payload);

/* Response */
char *clean_to_ascii_string(const char *input);
void print_formatted_response(const char *str);


#define PROMPT_HEADER "\n################################################################\n"  \
"###  YOU                                                      ###\n" \
"################################################################\n\n"

#define RESPONSE_HEADER "\n+--------------------------------------------------------------+\n" \
"|  GEMINI                                                       |\n" \
"+--------------------------------------------------------------+\n\n"

#define TURN_DIVIDER "\n================================================================\n" \
"----------------------------------------------------------------\n\n"

#endif /* LSEAI_H */
