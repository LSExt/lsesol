#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int get_terminal_height(void) {
    struct winsize w;
    if (isatty(fileno(stdout)) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        if (w.ws_row > 0) return w.ws_row;
    }
    return 24; /* Fallback standard VT100 / serial default: 24 lines */
}

/* Wait for a single keypress without waiting for [ENTER] */
char get_single_char(void) {
    struct termios oldt, newt;
    char c;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); /* Raw mode */
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    c = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); /* Restore mode */
    return c;
}

void print_with_pager(const char *formatted_text) {
    int max_lines = get_terminal_height() - 1; /* Reserve 1 line for prompt */
    int current_line = 0;
    const char *p = formatted_text;

    while (*p) {
        putchar(*p);
        if (*p == '\n') {
            current_line++;
            if (isatty(fileno(stdout)) && current_line >= max_lines) {
                printf("--More-- [Space=Next Page, Return=Next Line, q=Quit]");
                fflush(stdout);

                char key = get_single_char();

                /* Clear the prompt line cleanly with carriage return & spaces */
                printf("\r                                                              \r");
                fflush(stdout);

                if (key == 'q' || key == 'Q') {
                    break; /* User stopped paging */
                } else if (key == '\n' || key == '\r') {
                    current_line = max_lines - 1; /* Scroll down just 1 line */
                } else {
                    current_line = 0; /* Spacebar: full page scroll */
                }
            }
        }
        p++;
    }
}
