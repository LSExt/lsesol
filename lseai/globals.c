/* globals.c - Single allocation of shared globals */
#include "config.h"
#include "globals.h"

int g_turn_count = 0;
char g_current_model[MAX_NAME_LEN] = "gemini-3.8-flash";
int conv_count = 0;
char g_current_conversation[1024] = "";
