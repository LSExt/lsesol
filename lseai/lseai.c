/*
 * lseai.c - Readline CLI for Gemini API with thread persistence
 * Designed for SunOS 4.1.4 / Solaris 2.5.1 on SPARC
 *
 * Compile:
 *   gcc -std=gnu99 -O2 -o lseai lseai.c -I/usr/local/include -I/usr/local/lse/include -include /usr/local/lse/include/lsecompat.h -L/usr/local/lib -L/usr/local/lse/lib -Wl,-rpath,$prefix/lib -lreadline -lcurses -lcurl -lssl -lcrypto -llsecompat
 */

#include "lseai.h"
#include "globals.h"
#include "config.h"

/* Set the terminal title when supported */
void set_terminal_title( const char *title){
    if (!title) return;
    printf("\033k%s\033\\", title);
    fflush(stdout);
}


#define LSEAI_PATH_MAX   1024
#define LSEAI_NAME_MAX   256
#define LSEAI_KEY_MAX    256

/* --- Global CLI / Session Configuration Variables --- */
static char config_file_path[LSEAI_PATH_MAX]    = "${HOME}/.lseaiconf";
static char context_dir[LSEAI_PATH_MAX]         = "${HOME}/.lseai/conversations/";
static char context_file[LSEAI_PATH_MAX]        = "";
static char context_name[LSEAI_NAME_MAX]        = "";
static char attached_file_path[LSEAI_PATH_MAX]  = "";
static char api_key[LSEAI_KEY_MAX]              = "";
static char model_name[LSEAI_NAME_MAX]          = "gemini-3.8-flash"; /* default model */

static int  has_attachment                      = 0;
static int  auto_summarize                      = 1; /* enabled by default */

/* forward declaration */
int parse_args(int argc, char *argv[]);

static void print_help(void)
{
    printf("Usage: lseai [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  --config <path>             Config file (default: ~/.lseaiconf)\n");
    printf("  --context-file-path <path>  Explicit path to a chat JSON file\n");
    printf("  --context-name <name>       Chat session name (<name>.gemini.json)\n");
    printf("  --context-path <dir>        Directory storing conversations\n");
    printf("  --attach-file <file>        Attach a file (image/audio/text) to first request\n");
    printf("  --api-key <key|ENV:var>     API key literal or environment variable reference\n");
    printf("  --model <model>             Model name (overrides config)\n");
    printf("  --no-auto-summarize         Disable automatic session summarization\n");
    printf("  --help, -h                  Display this help message and exit\n\n");
}

/**
 * expand_home_dir:
 * Replaces leading "${HOME}" or "~" with the user's actual HOME directory.
 * Safe bounded buffer writes using snprintf.
 */
static void expand_home_dir(char *path, size_t max_len)
{
    const char *home = getenv("HOME");
    char temp[LSEAI_PATH_MAX];

    if (!path || path[0] == '\0') return;
    if (!home) home = ".";

    /* Case 1: Starts with "${HOME}" */
    if (strncmp(path, "${HOME}", 7) == 0) {
        snprintf(temp, sizeof(temp), "%s%s", home, path + 7);
        snprintf(path, max_len, "%s", temp);
    }
    /* Case 2: Starts with "~/" */
    else if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        snprintf(temp, sizeof(temp), "%s%s", home, path + 1);
        snprintf(path, max_len, "%s", temp);
    }
}



int main(int argc, char **argv) {
/*
    const char *api_key = getenv("GEMINI_API_KEY");
    const char *model_name = getenv("GEMINI_MODEL");
    const char *state_file = get_state_file_path();
    */
    const char *api_key;
    const char *model_name;

    parse_args( argc, argv );

    /* Expand any paths that still have ${HOME} or ~ */
    expand_home_dir(config_file_path, sizeof(config_file_path));
    expand_home_dir(context_dir, sizeof(context_dir));
    if (context_file[0] != '\0') {
        expand_home_dir(context_file, sizeof(context_file));
    }
    if (attached_file_path[0] != '\0') {
        expand_home_dir(attached_file_path, sizeof(attached_file_path));
    }

    if (!model_name) {
        model_name = DEFAULT_MODEL;
    }

    set_terminal_title( g_current_conversation);

    if (!api_key) {
        fprintf(stderr, "Error: GEMINI_API_KEY environment variable not set.\n");
        fprintf(stderr, "Run: export GEMINI_API_KEY=\"your_key_here\"\n");
        return 1;
    }

    printf("Gemini Terminal Interface (SPARC / libcurl / Readline)\n");
    printf("Active Model: %s\n", g_current_model);
    printf("Conversation Thread: %s\n", g_current_conversation);
    printf("Type 'exit' or 'quit' to end session. \n" );
    printf("Reset thread with '/reset'. Summarize thread with '/summarize'.\n\n");
    init_state_file();


    char *line;
    while ((line = read_multiline_prompt(g_current_conversation)) != NULL) {
        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        add_history(line);

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            free(line);
            break;
        }

        if (strcmp(line, "/reset") == 0) {
            const char *path = get_state_file_path();
            unlink(path);
            init_state_file();
            printf("Conversation thread cleared for: %s\n\n", path);
            free(line);
            continue;
        }
        if (strcmp(line, "/compact") == 0|| strcmp(line, "/summarize") == 0 ) {
            const char *path = get_state_file_path();
            compact_state_file();
            printf("Conversation summarized: %s\n\n", path);
            continue;
        }

        /* Load thread state and insert current prompt */
        char *current_state = read_state_file();
        if (!current_state) {
            free(line);
            continue;
        }

        char *insert_pos = strrchr(current_state, ']');
        if (insert_pos) *insert_pos = '\0';

        int has_entries = (strstr(current_state, "\"role\"") != NULL);
        char *escaped_prompt = json_escape(line);

        /* Construct full JSON payload with complete history */
        size_t payload_size = strlen(current_state) + (escaped_prompt ? strlen(escaped_prompt) : 0) + 512;
        char *full_payload = (char *)malloc(payload_size * sizeof(char));
        if (!full_payload) {
            fprintf(stderr, "Memory allocation failed for payload.\n");
            free(current_state);
            if (escaped_prompt) free(escaped_prompt);
            free(line);
            continue;
        }

        sprintf(full_payload, "%s%s{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}]}",
                current_state, (has_entries ? "," : ""), escaped_prompt ? escaped_prompt : "");

        printf("Thinking...\n");
        char *response_json = send_request(full_payload);

        if (response_json) {
            char *text = extract_text_field(response_json);
            if (text) {
                printf(RESPONSE_HEADER);
                print_formatted_response(text);
                printf("\n");
                update_state_file(line, response_json);
                free(text);
            } else {
                /* Fall back to printing raw response if parsing ever fails */
                printf(RESPONSE_HEADER);
                printf("\n(Raw Output):\n");
                print_formatted_response(response_json);
                printf("\n");
            }
            free(response_json);
        }
        printf(TURN_DIVIDER);

        free(current_state);
        if (escaped_prompt) free(escaped_prompt);
        free(full_payload);
        free(line);
    }

    return 0;
}

/* Helper macro for extracting argument values cleanly */
#define REQUIRE_PARAM_ARG(opt_name) do { \
    param_counter++; \
    if (param_counter >= argc) { \
        fprintf(stderr, "ERROR: Option '%s' requires a value.\n", opt_name); \
        exit(1); \
    } \
} while(0)



int parse_args(int argc, char *argv[])
{
    int param_counter = 1;

    while (param_counter < argc) {
        if (strcmp(argv[param_counter], "--config") == 0) {
            REQUIRE_PARAM_ARG("--config");
            snprintf(config_file_path, sizeof(config_file_path), "%s", argv[param_counter]);
        }
        else if (strcmp(argv[param_counter], "--context-file-path") == 0) {
            REQUIRE_PARAM_ARG("--context-file-path");
            snprintf(context_file, sizeof(context_file), "%s", argv[param_counter]);
        }
        else if (strcmp(argv[param_counter], "--context-name") == 0) {
            REQUIRE_PARAM_ARG("--context-name");
            /* Formulate <name>.gemini.json */
            snprintf(context_name, sizeof(context_name), "%s.gemini.json", argv[param_counter]);
        }
        else if (strcmp(argv[param_counter], "--context-path") == 0) {
            REQUIRE_PARAM_ARG("--context-path");
            snprintf(context_dir, sizeof(context_dir), "%s", argv[param_counter]);
        }
        else if (strcmp(argv[param_counter], "--attach-file") == 0) {
            REQUIRE_PARAM_ARG("--attach-file");
            snprintf(attached_file_path, sizeof(attached_file_path), "%s", argv[param_counter]);
            has_attachment = 1;
        }
        else if (strcmp(argv[param_counter], "--api-key") == 0) {
            REQUIRE_PARAM_ARG("--api-key");
            if (strncmp(argv[param_counter], "ENV:", 4) == 0) {
                const char *env_var = argv[param_counter] + 4;
                char *val = getenv(env_var);
                if (!val) {
                    fprintf(stderr, "ERROR: Environment variable '%s' not set.\n", env_var);
                    exit(1);
                }
                snprintf(api_key, sizeof(api_key), "%s", val);
            } else {
                snprintf(api_key, sizeof(api_key), "%s", argv[param_counter]);
            }
        }
        else if (strcmp(argv[param_counter], "--model") == 0) {
            REQUIRE_PARAM_ARG("--model");
            snprintf(model_name, sizeof(model_name), "%s", argv[param_counter]);
        }
        else if (strcmp(argv[param_counter], "--no-auto-summarize") == 0) {
            auto_summarize = 0;
        }
        else if (strcmp(argv[param_counter], "--help") == 0 || strcmp(argv[param_counter], "-h") == 0) {
            print_help();
            exit(0);
        }
        else {
            fprintf(stderr, "ERROR: Unrecognized argument: %s\n", argv[param_counter]);
            print_help();
            exit(1);
        }

        param_counter++;
    }

    return 0;
}
