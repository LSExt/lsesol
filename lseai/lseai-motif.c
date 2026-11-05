/*
 * gemini_motif.c - Standalone Motif / CDE Frontend for Gemini Client
 * Target: Solaris 2.5.1 / GCC 4.3.6 / Motif 1.2 or 2.x
 *
 * gcc -O2 gemini_motif.c -o gemini_motif  -I/usr/dt/include -I/usr/openwin/include -L/usr/dt/lib -R/usr/dt/lib -L/usr/openwin/lib -R/usr/openwin/lib -lXm -lXt -lX11 -lsocket -lnsl 
 */

#include "lseai.h"
#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/Label.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/FileSB.h>      /* XmCreateFileSelectionDialog */
#include <Xm/MessageB.h>    /* XmCreateQuestionDialog, XmCreateWarningDialog, etc. */
#include <Xm/SelectioB.h>   /* Include this if you also use XmCreatePromptDialog */
#include "config.h"
#include "globals.h"


/* Global widget handles needed for backend hooks */
static Widget output_text_w;
static Widget input_text_w;
static Widget conv_menu_w;
static Widget conv_pane_w;
static Widget model_menu_w;
static Widget model_pane_w;
static Widget top_bar_w;
static Widget toplevel_w;


static void append_conversation_text( Widget w, char *msg)
{
	 XmTextInsert(w, XmTextGetLastPosition(output_text_w), msg);
	 XmTextShowPosition(w, XmTextGetLastPosition(output_text_w));
}

/* Forward declaration so new_chat_ok_cb can reference it */
static void conv_select_cb(Widget w, XtPointer client_data, XtPointer call_data);
static void model_select_cb(Widget w, XtPointer client_data, XtPointer call_data);


/* Callback: When user clicks "OK" in the New Chat Dialog */
static void new_chat_ok_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmSelectionBoxCallbackStruct *cbs = (XmSelectionBoxCallbackStruct *)call_data;
    char *chat_name = NULL;
    Widget new_item;
    Arg args[5];
    int n;

    /* Extract C string from Motif XmString */
    if (XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &chat_name)) {
        if (chat_name && strlen(chat_name) > 0) {
            /* Create a persistent heap copy for client_data callback */
            char *saved_name = strdup(chat_name);

            /* Dynamically add new push button to the dropdown menu pane */
            n = 0;
            new_item = XmCreatePushButton(conv_pane_w, saved_name, args, n);
            XtManageChild(new_item);
            
            /* Hook existing selection callback */
            XtAddCallback(new_item, XmNactivateCallback, conv_select_cb, (XtPointer)saved_name);

            /* Update OptionMenu to display the newly created chat */
            n = 0;
            XtSetArg(args[n], XmNmenuHistory, new_item); n++;
            XtSetValues(conv_menu_w, args, n);

            /* Trigger backend logic (optional: clear output buffer for fresh chat) */
            printf("Created and switched to conversation: %s\n", chat_name);
            XmTextSetString(output_text_w, "--- New Conversation Started ---\n");

            /* Format full path into g_current_conversation */
            snprintf(g_current_conversation, sizeof(g_current_conversation),
                     "%s/%s.gemini.json", g_config.state_dir, chat_name);

            /* Initialize empty conversation state file on disk */
            init_state_file();

            /* Reset turn counter for fresh conversation */
            g_turn_count = 0;
            
        }
        XtFree(chat_name);
    }

    /* Destroy the prompt dialog */
    XtDestroyWidget(w);
}
static Widget add_menu_item( Widget parent, const char *label,
                            const char *accel, const char *accel_text,
                             XtCallbackProc cb, XtPointer client_data)
{
    Widget btn;
    XmString x_label = XmStringCreateLocalized((char *)label);
    XmString x_accel = accel_text ? XmStringCreateLocalized((char *)accel_text) : NULL;
    int n = 0;
    Arg args[8];

    XtSetArg(args[n], XmNlabelString, x_label); n++;
    if (accel && x_accel) {
        XtSetArg(args[n], XmNaccelerator, accel); n++;
        XtSetArg(args[n], XmNacceleratorText, x_accel); n++;
    }

    btn = XmCreatePushButton(parent, (char *)label, args, n);
    XtManageChild(btn);

    if (cb) {
        XtAddCallback(btn, XmNactivateCallback, cb, client_data);
    }

    XmStringFree(x_label);
    if (x_accel) XmStringFree(x_accel);

    return btn;
}

static void add_menu_separator(Widget parent)
{
    Widget sep = XmCreateSeparator(parent, "separator", NULL, 0);
    XtManageChild(sep);
}

static void cb_menu_edit_cut(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget focus = XmGetFocusWidget(toplevel_w);
    if (XmIsText(focus)) {
        XmTextCut(focus, CurrentTime);
    }
}

static void cb_menu_edit_copy(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget focus = XmGetFocusWidget(toplevel_w);
    if (XmIsText(focus)) {
        XmTextCopy(focus, CurrentTime);
    }
}

static void cb_menu_edit_paste(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget focus = XmGetFocusWidget(toplevel_w);
    if (XmIsText(focus)) {
        XmTextPaste(focus);
    }
}

static void cb_menu_edit_select_all(Widget w, XtPointer client_data, XtPointer call_data)
{
    /* If input widget is active, select prompt; otherwise select entire terminal */
    Widget focus = XmGetFocusWidget(toplevel_w);
    Widget target = (focus == input_text_w) ? input_text_w : output_text_w;
    XmTextSetSelection(target, 0, XmTextGetLastPosition(target), CurrentTime);
}

static void cb_menu_edit_clear_screen(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmTextSetString(output_text_w, "");
}

static void cb_menu_edit_clear_prompt(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmTextSetString(input_text_w, "");
}


/* Callback: Manual Conversation Compaction */
static void compress_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    int keep_n = (g_config.keep_last_turns > 0) ? g_config.keep_last_turns : 5;
    char msg[256];

    append_conversation_text(output_text_w, "\n>>> [ Compacting conversation history... ] <<<\n");
    XFlush(XtDisplay(output_text_w));

    if (compact_state_file() == 0) {
        snprintf(msg, sizeof(msg), 
                 ">>> [ Compaction successful. Keeping last %d turns + summary. ] <<<\n\n", keep_n);
        append_conversation_text(output_text_w, msg);
    } else {
        append_conversation_text(output_text_w, 
                 ">>> [ Compaction skipped or failed (not enough turns or API error). ] <<<\n\n");
    }

    /* Scroll to bottom */
    XmTextShowPosition(output_text_w, XmTextGetLastPosition(output_text_w));
}

/* Helper to rebuild the Model OptionMenu pane dynamically */
void refresh_model_dropdown(void)
{
    WidgetList children;
    Cardinal num_children = 0;
    int i;
    Widget initial_btn = NULL;

    /* 1. Remove all old model buttons from the pulldown pane */
    XtVaGetValues(model_pane_w,
                  XmNchildren, &children,
                  XmNnumChildren, &num_children,
                  NULL);

    for (i = (int)num_children - 1; i >= 0; i--) {
        XtDestroyWidget(children[i]);
    }

    /* 2. Recreate buttons from updated g_config.models */
    for (i = 0; i < g_config.model_count; i++) {
        char *saved_model = strdup(g_config.models[i]);
        Widget btn = XmCreatePushButton(model_pane_w, saved_model, NULL, 0);
        XtManageChild(btn);
        XtAddCallback(btn, XmNactivateCallback, model_select_cb, (XtPointer)saved_model);

        if (strcmp(saved_model, g_current_model) == 0 || i == 0) {
            initial_btn = btn;
        }
    }

    /* 3. Re-set visible selection */
    if (initial_btn) {
        XtVaSetValues(model_menu_w, XmNmenuHistory, initial_btn, NULL);
    }
}

/* Callback: User clicks Refresh Models */
static void refresh_models_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    const char *key = getenv("GEMINI_API_KEY"); /* Or load from config */

    append_conversation_text(output_text_w, "\nQuerying Google API for supported models...\n");

    if (fetch_remote_models(key) == 0) {
        refresh_model_dropdown();
        append_conversation_text(output_text_w, "Model list updated successfully.\n");
    } else {
        append_conversation_text(output_text_w, "Failed to retrieve models from Google.\n");
    }
}

/* Callback: When user clicks "Cancel" in the New Chat Dialog */
static void new_chat_cancel_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(w);
}


/* Helper to allocate an X11 color from name or hex string */
static Pixel get_pixel_color(Widget w, const char *color_name)
{
    XrmValue from, to;
    from.addr = (char *)color_name;
    from.size = strlen(color_name) + 1;
    to.addr = NULL;
    to.size = 0;

    if (XtConvertAndStore(w, XtRString, &from, XtRPixel, &to)) {
        return *(Pixel *)to.addr;
    }
    return BlackPixelOfScreen(XtScreen(w));
}


/* Callback: Send Button Pressed */
static void send_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *prompt = XmTextGetString(input_text_w);
    
    if (prompt && strlen(prompt) > 0) {
        /* 
         * TODO: Plug in your Gemini API call here.
         * For now, append what the user typed to the output area.
         */
        XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), PROMPT_HEADER);
        XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), prompt);

         
        /* Load thread state and insert current prompt */
        char *current_state = read_state_file();
        if (!current_state) {
           if (prompt) {
               XtFree(prompt);
           }
            return;
        }

        char *insert_pos = strrchr(current_state, ']');
        if (insert_pos) *insert_pos = '\0';

        int has_entries = (strstr(current_state, "\"role\"") != NULL);
        char *escaped_prompt = json_escape(prompt);

        /* Construct full JSON payload with complete history */
        size_t payload_size = strlen(current_state) + (escaped_prompt ? strlen(escaped_prompt) : 0) + 512;
        char *full_payload = (char *)malloc(payload_size * sizeof(char));
        if (!full_payload) {
            fprintf(stderr, "Memory allocation failed for payload.\n");
            free(current_state);
            if (escaped_prompt) free(escaped_prompt);
            if (prompt) {
                XtFree(prompt);
            }
            return;
        }

        sprintf(full_payload, "%s%s{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}]}",
                current_state, (has_entries ? "," : ""), escaped_prompt ? escaped_prompt : "");



        printf("Thinking...\n");
        char *response_json = send_request(full_payload);


        if (response_json) {
            char *text = extract_text_field(response_json);
            if (text) {

                char *ascii_text = clean_to_ascii_string(text);
                if (ascii_text) {
                    /* Format Markdown headers, bullets, and code boxes */
                    char *formatted = format_markdown_to_ascii(ascii_text);

                    /* Insert into output scrolled text widget */

                    XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), RESPONSE_HEADER);
                    XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), formatted );
                    XmTextShowPosition(output_text_w, XmTextGetLastPosition(output_text_w));
                    
                    /* Clean up heap */
                    free(ascii_text);
                }
                
                update_state_file(prompt, response_json);
                free(text);
            } else {
                /* Fall back to printing raw response if parsing ever fails */
                printf("\nGemini (Raw Output):\n");
                /*print_formatted_response(response_json);*/
                printf(response_json);
                printf("\n");
            }
            free(response_json);
        }

        free(current_state);
        if (escaped_prompt) free(escaped_prompt);
        free(full_payload);

        
        XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), TURN_DIVIDER );


        /* Scroll to bottom */
        XmTextShowPosition(output_text_w, XmTextGetLastPosition(output_text_w));
        
        /* Clear input buffer */
        XmTextSetString(input_text_w, "");
    }
    
    if (prompt) {
        XtFree(prompt);
    }
}

/* Callback: New Conversation Button (Opens Modal Prompt Dialog) */
static void new_conv_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget dialog;
    Arg args[10];
    int n = 0;
    XmString title_str = XmStringCreateLocalized("New Conversation");
    XmString prompt_str = XmStringCreateLocalized("Enter Conversation Name:");

    XtSetArg(args[n], XmNdialogTitle, title_str); n++;
    XtSetArg(args[n], XmNselectionLabelString, prompt_str); n++;
    XtSetArg(args[n], XmNautoUnmanage, False); n++; /* We handle destruction manually */
    
    dialog = XmCreatePromptDialog(toplevel_w, "newChatDialog", args, n);

    XmStringFree(title_str);
    XmStringFree(prompt_str);

    /* Unmanage the "Help" button common to Motif dialogs */
    XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));

    /* Hook OK and Cancel actions */
    XtAddCallback(dialog, XmNokCallback, new_chat_ok_cb, NULL);
    XtAddCallback(dialog, XmNcancelCallback, new_chat_cancel_cb, NULL);

    /* Show Dialog */
    XtManageChild(dialog);
}

/* Callback: Model Selected from Dropdown */
static void model_select_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *model_name = (char *)client_data;
    if (model_name && strlen(model_name) > 0) {
        strncpy(g_current_model, model_name, sizeof(g_current_model) - 1);
        g_current_model[sizeof(g_current_model) - 1] = '\0';
        printf("Switched Model to: %s\n", g_current_model);
    }
}



void select_first_conversation(void)
{
    config_load(NULL);
    strncpy(g_current_model, g_config.default_model, sizeof(g_current_model) - 1);
    g_current_model[sizeof(g_current_model) - 1] = '\0';

    int conv_count = 0;
    char **conversation_list = config_list_conversations(&conv_count);

    if (conversation_list && conv_count > 0) {
        /* Construct full path: ~/.lseai/conversations/<chat_name>.gemini.json */
        snprintf(g_current_conversation, sizeof(g_current_conversation),
                 "%s/%s.gemini.json", g_config.state_dir, conversation_list[0]);
    } else {
        /* Default fallback path */
        snprintf(g_current_conversation, sizeof(g_current_conversation),
                 "%s/default.gemini.json", g_config.state_dir);
    }
    g_current_conversation[sizeof(g_current_conversation) - 1] = '\0';

    /* Make sure the state file exists with empty JSON {"contents":[]} */
    init_state_file();
    /* Now it is safe to free the temporary directory list */
    if (conversation_list) {
        config_free_conversation_list(conversation_list, conv_count);
    }
}

/* Callback to free the heap-allocated chat name when the button is destroyed */
static void conv_btn_destroy_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    (void)w;
    (void)call_data;
    if (client_data != NULL) {
        free((char *)client_data);
    }
}

void refresh_thread_dropdown(void)
{
    Arg args[10];
    int n;
    int chat_count = 0;
    char **chats = NULL;
    Widget initial_btn = NULL;
    char *initial_chat_name = NULL;

    /* ------------------------------------------------------------------ */
    /* 1. Tear down existing entries in the Pulldown Menu Pane            */
    /* ------------------------------------------------------------------ */
    if (conv_pane_w != NULL) {
        WidgetList children = NULL;
        Cardinal num_children = 0;

        n = 0;
        XtSetArg(args[n], XmNchildren, &children); n++;
        XtSetArg(args[n], XmNnumChildren, &num_children); n++;
        XtGetValues(conv_pane_w, args, n);

        if (num_children > 0 && children != NULL) {
            /* Unmanage all children in a single X server batch */
            XtUnmanageChildren(children, num_children);

            /*
             * Destroy in reverse to avoid index churn. 
             * XtDestroyWidget automatically triggers conv_btn_destroy_cb,
             * which frees each strdup'd string safely.
             */
            while (num_children > 0) {
                num_children--;
                XtDestroyWidget(children[num_children]);
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* 2. Retrieve conversation list from configuration subsystem         */
    /* ------------------------------------------------------------------ */
    chats = config_list_conversations(&chat_count);

    if (chat_count == 0) {
        /* No saved files found: initialize "default" */
        initial_chat_name = strdup("default");
        initial_btn = XmCreatePushButton(conv_pane_w, initial_chat_name, NULL, 0);

        /* Bind free() to widget destruction lifecycle */
        XtAddCallback(initial_btn, XmNdestroyCallback, 
                      conv_btn_destroy_cb, (XtPointer)initial_chat_name);
        XtAddCallback(initial_btn, XmNactivateCallback, 
                      conv_select_cb, (XtPointer)initial_chat_name);
        XtManageChild(initial_btn);
    } else {
        int i;
        for (i = 0; i < chat_count; i++) {
            char *saved_name = strdup(chats[i]);
            Widget btn = XmCreatePushButton(conv_pane_w, saved_name, NULL, 0);

            /* Bind free() to widget destruction lifecycle */
            XtAddCallback(btn, XmNdestroyCallback, 
                          conv_btn_destroy_cb, (XtPointer)saved_name);
            XtAddCallback(btn, XmNactivateCallback, 
                          conv_select_cb, (XtPointer)saved_name);
            XtManageChild(btn);

            if (i == 0) {
                initial_btn = btn;
                initial_chat_name = saved_name;
            }
        }
        
        /* 
         * If config_list_conversations dynamically allocates its string table,
         * free the container array or items here according to its contract.
         */
    }
    config_free_conversation_list( chats, chat_count);

    /* ------------------------------------------------------------------ */
    /* 3. Synchronize OptionMenu UI label and Active State File           */
    /* ------------------------------------------------------------------ */
    if (initial_btn != NULL && conv_menu_w!= NULL) {
        /*
         * Force the parent XmOptionMenu to visually display initial_btn
         * as the currently active selection.
         */
        n = 0;
        XtSetArg(args[n], XmNmenuHistory, initial_btn); n++;
        XtSetValues(conv_menu_w, args, n);
    }

    if (initial_chat_name != NULL) {
        snprintf(g_current_conversation, sizeof(g_current_conversation),
                 "%s/%s.gemini.json", g_config.state_dir, initial_chat_name);
        init_state_file();
        printf("Initial conversation set to: %s\n", g_current_conversation);
    }
}

/************************
** FILE callbacks
*************************/

/* File selection callback for raw/formatted export */
static void execute_export(Widget dialog, XtPointer client_data, XtPointer call_data)
{
    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
    char *filepath = NULL;
    int is_json = (int)(long)client_data;

    if (!XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &filepath)) {
        return;
    }

    if (filepath && strlen(filepath) > 0) {
        if (is_json) {
            /* Copy current state file to destination */
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", g_current_conversation, filepath);


            system(cmd);
        } else {
            /* Dump clean text from output_text_w to target file */
            char *text = XmTextGetString(output_text_w);
            if (text) {
                FILE *fp = fopen(filepath, "w");
                if (fp) {
                    fputs(text, fp);
                    fclose(fp);
                }
                XtFree(text);
            }
        }
    }
    XtFree(filepath);
    XtUnmanageChild(dialog);
}

static void cancel_dialog_cb(Widget dialog, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild(dialog);
}

static void show_file_exporter(int is_json)
{
    Widget fs_dialog;
    XmString title = XmStringCreateLocalized(is_json ? "Export Raw State (JSON)" : "Export Readable Notes");

    fs_dialog = XmCreateFileSelectionDialog(toplevel_w, "exportDialog", NULL, 0);
    XtVaSetValues(fs_dialog, XmNdialogTitle, title, NULL);
    XmStringFree(title);

    XtAddCallback(fs_dialog, XmNokCallback, execute_export, (XtPointer)(long)is_json);
    XtAddCallback(fs_dialog, XmNcancelCallback, cancel_dialog_cb, NULL);
    XtManageChild(fs_dialog);
}

static void cb_menu_file_export_txt(Widget w, XtPointer client_data, XtPointer call_data) {
    show_file_exporter(0);
}

static void cb_menu_file_export_json(Widget w, XtPointer client_data, XtPointer call_data) {
    show_file_exporter(1);
}

/* Helper function to copy a file byte-for-byte */
static int copy_file(const char *src_path, const char *dst_path)
{
    FILE *src = fopen(src_path, "rb");
    if (!src) return -1;

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return -1;
    }

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return -1;
        }
    }

    fclose(src);
    fclose(dst);
    return 0;
}

/* Callback executed when the user clicks 'OK' in the File Selection Dialog */
static void import_file_ok_cb(Widget dialog, XtPointer client_data, XtPointer call_data)
{
    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
    char *src_path = NULL;

    /* Extract C-string path from Motif Compound String */
    if (!XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &src_path)) {
        XtUnmanageChild(dialog);
        return;
    }

    /* Verify source file is readable and is a regular file */
    struct stat st;
    if (stat(src_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: Selected path is not a valid regular file: %s\n", src_path);
        XtFree(src_path);
        XtUnmanageChild(dialog);
        return;
    }

    /* 
     * Derive clean conversation name from filename:
     * e.g., "/path/to/my_chat.gemini.json" -> "my_chat"
     * e.g., "/path/to/backup.json"         -> "backup"
     */
    char path_copy[1024];
    strncpy(path_copy, src_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *base = basename(path_copy);
    char conv_name[256];
    strncpy(conv_name, base, sizeof(conv_name) - 1);
    conv_name[sizeof(conv_name) - 1] = '\0';

    /* Strip known extensions to guess the pure conversation ID */
    char *ext = strstr(conv_name, ".gemini.json");
    if (ext) {
        *ext = '\0';
    } else {
        ext = strrchr(conv_name, '.');
        if (ext) *ext = '\0';
    }

    /* Fallback if the file was just ".json" */
    if (strlen(conv_name) == 0) {
        strncpy(conv_name, "imported_conversation", sizeof(conv_name));
    }

    /* Build destination matching: %s/%s.gemini.json */
    char dest_path[1024];
    snprintf(dest_path, sizeof(dest_path), "%s/%s.gemini.json",
             g_config.state_dir, conv_name);

    /* Avoid overwriting if source and destination are identical */
    if (strcmp(src_path, dest_path) != 0) {
        if (copy_file(src_path, dest_path) == 0) {
            printf("Imported: '%s' -> '%s'\n", src_path, dest_path);
        } else {
            perror("Failed to copy conversation file");
            XtFree(src_path);
            XtUnmanageChild(dialog);
            return;
        }
    }

    /* Point active conversation to newly imported target */
    strncpy(g_current_conversation, dest_path, sizeof(g_current_conversation) - 1);
    g_current_conversation[sizeof(g_current_conversation) - 1] = '\0';

    XtFree(src_path);
    XtDestroyWidget(dialog); /* Destroy rather than leak the dialog */

    /* Refresh UI components */
    refresh_thread_dropdown();

    /* Load imported conversation into viewer if your app has this helper */
    /* load_conversation_file(g_current_conversation); */
}

/* File -> Open / Import Menu Callback */
static void cb_menu_file_open(Widget w, XtPointer client_data, XtPointer call_data)
{
    Arg args[10];
    int n = 0;
    XmString title = XmStringCreateLocalized("Import Conversation File");
    XmString mask  = XmStringCreateLocalized("*.json");

    XtSetArg(args[n], XmNdialogTitle, title); n++;
    XtSetArg(args[n], XmNdirMask, mask); n++;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); n++;
    XtSetArg(args[n], XmNautoUnmanage, False); n++;

    Widget fs_dialog = XmCreateFileSelectionDialog(toplevel_w, "openDialog", args, n);

    XmStringFree(title);
    XmStringFree(mask);

    /* Hide the default Help button */
    XtUnmanageChild(XmFileSelectionBoxGetChild(fs_dialog, XmDIALOG_HELP_BUTTON));

    /* Hook callbacks */
    XtAddCallback(fs_dialog, XmNokCallback, import_file_ok_cb, NULL);
    XtAddCallback(fs_dialog, XmNcancelCallback, cancel_dialog_cb, NULL);

    XtManageChild(fs_dialog);
}

static void delete_cancel_cb(Widget dialog, XtPointer client_data, XtPointer call_data)
{
    char *path = (char *)client_data;
    if (path) free(path);
    XtDestroyWidget(dialog);
}

static void delete_confirmed_cb(Widget dialog, XtPointer client_data, XtPointer call_data)
{
    char *target_file = (char *)client_data;

    if (target_file && strlen(target_file) > 0) {
        if (unlink(target_file) == 0) {
            printf("deleted file %s\n", target_file);
        } else {
            perror("unlink failed");
        }

        /* Clear current conversation if it matches the one deleted */
        if (strcmp(g_current_conversation, target_file) == 0) {
            g_current_conversation[0] = '\0'; /* or your app's clear mechanism */
        }

        XmTextSetString(output_text_w, "");
        refresh_thread_dropdown();
    }

    if (target_file) free(target_file);
    XtDestroyWidget(dialog);
}

static void cb_menu_file_delete(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget q_dialog;
    char msg[1024];
    XmString x_msg;
    char *target_copy;

    if (strlen(g_current_conversation) == 0) {
        return; /* Nothing active to delete */
    }

    snprintf(msg, sizeof(msg), "Permanently delete active conversation file?\n%s", g_current_conversation);
    x_msg = XmStringCreateLocalized(msg);

    q_dialog = XmCreateQuestionDialog(toplevel_w, "confirmDelete", NULL, 0);
    XtVaSetValues(q_dialog, 
        XmNmessageString, x_msg,
        XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL, /* Prevent interaction elsewhere */
        NULL);
    XmStringFree(x_msg);

    /* Duplicate the string so it cannot be mutated or freed while waiting for user confirmation */
    target_copy = strdup(g_current_conversation);

    XtAddCallback(q_dialog, XmNokCallback, delete_confirmed_cb, (XtPointer)target_copy);
    XtAddCallback(q_dialog, XmNcancelCallback, delete_cancel_cb, (XtPointer)target_copy);
    
    XtManageChild(q_dialog);
}
static void cb_menu_file_exit(Widget w, XtPointer client_data, XtPointer call_data)
{
    exit(0);
}
/*********** FILE callback helpers end **********/

/* Callback: Thread Selected */
static void conv_select_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *conv_name = (char *)client_data;
    char msg[2024];
    printf("Selected Context: %s\n", conv_name);
    char *chat_name = (char *)client_data;
    if (!chat_name) return;

    /* Update global path */
    snprintf(g_current_conversation, sizeof(g_current_conversation),
             "%s/%s.gemini.json", g_config.state_dir, chat_name);

    printf("Switched conversation to: %s (%s)\n", chat_name, g_current_conversation);

    /* Clear or reload output text area */
    snprintf( msg, sizeof( msg )-1, "-- Conversation to: %s, file: %s --\n", chat_name, g_current_conversation );
    append_conversation_text(output_text_w, msg );

   /* 
    append_conversation_text(output_text_w, "--- Conversation to: ");
    append_conversation_text(output_text_w, chat_name);
    append_conversation_text(output_text_w, "in file :");
    append_conversation_text(output_text_w, g_current_conversation);
    append_conversation_text(output_text_w, "---\n");
*/

    /* TODO: Call backend helper to reload previous messages from this file into output_text_w */
}

Widget create_main_menubar(Widget parent)
{
    Widget menu_bar,  menu_context, compress_item;
    XmString s_file, s_edit, s_context;
    Widget menu_file, menu_edit, cascade_file, cascade_edit, cascade_context;

    int n;
    Arg args[20];

    /* -------------------------------------------------------------
     * TOP MENU BAR (XmMenuBar)
     * ------------------------------------------------------------- */
    menu_bar = XmCreateMenuBar(parent, "menuBar", NULL, 0);
    XtManageChild(menu_bar);

    /* -------------------------------------------------------------
     * 1. FILE MENU
     * ------------------------------------------------------------- */
    menu_file = XmCreatePulldownMenu(menu_bar, "menuFile", NULL, 0);

    add_menu_item(menu_file, "New Conversation...", "Ctrl<Key>N", "Ctrl+N", new_conv_cb, NULL);
    add_menu_separator(menu_file);
    add_menu_item(menu_file, "Export as Plain Text...", NULL, NULL, cb_menu_file_export_txt, NULL);
    add_menu_item(menu_file, "Export as Raw JSON...", NULL, NULL, cb_menu_file_export_json, NULL);
    add_menu_item(menu_file, "Import / Conversation...", NULL, NULL, cb_menu_file_open, NULL);
    add_menu_separator(menu_file);
    add_menu_item(menu_file, "Delete Current Conversation", NULL, NULL, cb_menu_file_delete, NULL);
    add_menu_separator(menu_file);
    add_menu_item(menu_file, "Exit", "Ctrl<Key>Q", "Ctrl+Q", cb_menu_file_exit, NULL);

    s_file = XmStringCreateLocalized("File");
    cascade_file = XtVaCreateManagedWidget("cascadeFile",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNlabelString, s_file,
        XmNmnemonic, 'F',
        XmNsubMenuId, menu_file,
        NULL);
    XmStringFree(s_file);
    

    /* -------------------------------------------------------------
     * 2. EDIT MENU
     * ------------------------------------------------------------- */
    menu_edit = XmCreatePulldownMenu(menu_bar, "menuEdit", NULL, 0);

    add_menu_item(menu_edit, "Cut", NULL, NULL, cb_menu_edit_cut, NULL);
    add_menu_item(menu_edit, "Copy", NULL, NULL, cb_menu_edit_copy, NULL);
    add_menu_item(menu_edit, "Paste", NULL, NULL, cb_menu_edit_paste, NULL);
    add_menu_separator(menu_edit);
    add_menu_item(menu_edit, "Select All", "Ctrl<Key>A", "Ctrl+A", cb_menu_edit_select_all, NULL);
    add_menu_separator(menu_edit);
    add_menu_item(menu_edit, "Clear Screen", NULL, NULL, cb_menu_edit_clear_screen, NULL);
    add_menu_item(menu_edit, "Clear Prompt Box", NULL, NULL, cb_menu_edit_clear_prompt, NULL);

    s_edit = XmStringCreateLocalized("Edit");
    cascade_edit = XtVaCreateManagedWidget("cascadeEdit",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNlabelString, s_edit,
        XmNmnemonic, 'E',
        XmNsubMenuId, menu_edit,
        NULL);
    XmStringFree(s_edit);


    /* "Conversation" Pulldown */
    menu_context = XmCreatePulldownMenu(menu_bar, "menuContext", NULL, 0);

    /* Menu Item: Compress */
    add_menu_item(menu_context, "Summarize and compress history", NULL, NULL, compress_cb, NULL);

    s_context = XmStringCreateLocalized("Conversation");
    cascade_context = XtVaCreateManagedWidget("cascadeContext",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNlabelString, s_context,
        XmNmnemonic, 'C',
        XmNsubMenuId, menu_context,
        NULL);
    XmStringFree(s_context);

    return menu_bar;
}



int main(int argc, char *argv[])
{
    XtAppContext app_context;
    Widget toplevel, main_window, form;
    Widget top_bar, conv_pane, model_pane, new_btn;
    Widget btn_sample_chat, btn_model;
    Widget send_btn;
    Widget menu_bar;

    Arg args[20];
    int n;

    select_first_conversation(); 


       /* Initialize active model: match default_model from config, or pick first available */
    g_current_model[0] = '\0';

    if (g_config.model_count > 0) {
        int i;
        /* find configured default_model in available models */
        for (i = 0; i < g_config.model_count; i++) {
            if (strncmp(g_config.models[i], g_config.default_model, sizeof(g_config.default_model) - 1) == 0) {
                strncpy(g_current_model, g_config.models[i], sizeof(g_current_model) - 1);
                g_current_model[sizeof(g_current_model) - 1] = '\0';
                break;
            }
        }

        /* If default_model was not matched, take the first model in the list */
        if (g_current_model[0] == '\0') {
            strncpy(g_current_model, g_config.models[0], sizeof(g_current_model) - 1);
            g_current_model[sizeof(g_current_model) - 1] = '\0';
        }
    }

    /* Ultimate hardcoded fallback if config list was empty */
    if (g_current_model[0] == '\0') {
        strncpy(g_current_model, "gemini-2.5-flash", sizeof(g_current_model) - 1);
        g_current_model[sizeof(g_current_model) - 1] = '\0';
    }

    printf("Active model set to: %s\n", g_current_model);

    /* Initialize Xt Intrinsics */
    toplevel = XtVaAppInitialize(
        &app_context, 
        "GeminiCDE", 
        NULL, 0, 
        &argc, argv, 
        NULL, 
        NULL
    );
    toplevel_w = toplevel;

   /* Main Window Shell Dimensions */
    n = 0;
    XtSetArg(args[n], XmNtitle, "Gemini AI Client - Solaris CDE"); n++;
    XtSetArg(args[n], XmNwidth, 780); n++;
    XtSetArg(args[n], XmNheight, 600); n++;
    XtSetValues(toplevel, args, n);

    main_window = XmCreateMainWindow(toplevel, "mainWindow", NULL, 0);
    XtManageChild(main_window);

    menu_bar = create_main_menubar( main_window);


    /* -------------------------------------------------------------
     * MAIN FORM CONTAINER (Below MenuBar)
     * ------------------------------------------------------------- */
    form = XmCreateForm(main_window, "mainForm", NULL, 0);
    XtManageChild(form);

    /* Tell XmMainWindow about the MenuBar and Form WorkArea */
    XmMainWindowSetAreas(main_window, menu_bar, NULL, NULL, NULL, form);


    /* -------------------------------------------------------------
     * 1. TOP CONTROL BAR (RowColumn with Menus & Buttons)
     * ------------------------------------------------------------- */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
    XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
    top_bar = XmCreateRowColumn(form, "topBar", args, n);
    top_bar_w = top_bar;
    XtManageChild(top_bar);

    conv_pane = XmCreatePulldownMenu(top_bar, "convPane", NULL, 0);
    conv_pane_w = conv_pane;

    n = 0;
    XtSetArg(args[n], XmNlabelString, XmStringCreateLocalized("Chat / Context:")); n++;
    XtSetArg(args[n], XmNsubMenuId, conv_pane); n++;
    conv_menu_w = XmCreateOptionMenu(top_bar, "convOptionMenu", args, n);
    XtManageChild(conv_menu_w);

    refresh_thread_dropdown();

    /* New Chat Button */
    n = 0;
    new_btn = XmCreatePushButton(top_bar, "New Chat", args, n);
    XtManageChild(new_btn);
    XtAddCallback(new_btn, XmNactivateCallback, new_conv_cb, NULL);

    Widget initial_model_btn = NULL;
    /* Model Dropdown (OptionMenu) */
    model_pane = XmCreatePulldownMenu(top_bar, "modelPane", NULL, 0);
    model_pane_w = model_pane;

    if (g_config.model_count > 0) {
        int i;
        for (i = 0; i < g_config.model_count; i++) {
            /* Create persistent string for client_data callback */
            char *saved_model = strdup(g_config.models[i]);
            Widget btn = XmCreatePushButton(model_pane, saved_model, NULL, 0);
            XtManageChild(btn);
            XtAddCallback(btn, XmNactivateCallback, model_select_cb, (XtPointer)saved_model);

            /* Check if this button matches the active model to select it */
            if (strcmp(saved_model, g_current_model) == 0) {
                initial_model_btn = btn;
            } else if (i == 0 && initial_model_btn == NULL) {
                /* Fallback to first button if match not found yet */
                initial_model_btn = btn;
            }
        }
    } else {
        /* Fallback if config has no models defined */
        char *fallback_model = strdup("gemini-2.5-flash");
        initial_model_btn = XmCreatePushButton(model_pane, fallback_model, NULL, 0);
        XtManageChild(initial_model_btn);
        XtAddCallback(initial_model_btn, XmNactivateCallback, model_select_cb, (XtPointer)fallback_model);
    }

    n = 0;
    XtSetArg(args[n], XmNlabelString, XmStringCreateLocalized("Model:")); n++;
    XtSetArg(args[n], XmNsubMenuId, model_pane); n++;
    model_menu_w = XmCreateOptionMenu(top_bar, "modelOptionMenu", args, n);
    XtManageChild(model_menu_w);

    /* -------------------------------------------------------------
     * 2. BOTTOM PROMPT & SEND BUTTON
     * ------------------------------------------------------------- */
    /* Send Button */
    n = 0;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNmarginWidth, 12); n++;
    XtSetArg(args[n], XmNmarginHeight, 8); n++;
    send_btn = XmCreatePushButton(form, "Send", args, n);
    XtManageChild(send_btn);
    XtAddCallback(send_btn, XmNactivateCallback, send_cb, NULL);

    /* Prompt Input Area (Scrolled multi-line text at bottom) */
    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNrightWidget, send_btn); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrows, 4); n++;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNwordWrap, True); n++;
    XtSetArg(args[n], XmNscrollHorizontal, False); n++;
    input_text_w = XmCreateScrolledText(form, "inputText", args, n);
    XtManageChild(input_text_w);

    /* Enforce word-wrap directly on the text widget */
    XtVaSetValues(input_text_w,
        XmNwordWrap, True,
        XmNscrollHorizontal, False,
        NULL);

    /* -------------------------------------------------------------
     * 3. OUTPUT CONVERSATION PANE (With Copy/Paste Scrolling)
     * ------------------------------------------------------------- */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, top_bar); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, XtParent(input_text_w)); n++;
    XtSetArg(args[n], XmNeditable, False); n++;               /* Read only */
    XtSetArg(args[n], XmNcursorPositionVisible, False); n++;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNwordWrap, True); n++;

    XtSetArg(args[n], XmNscrollHorizontal, False); n++;   

    output_text_w = XmCreateScrolledText(form, "outputText", args, n);
    XtManageChild(output_text_w);

    /* Set default placeholder text */
    /*XmTextSetString(output_text_w, "response goes here\n");*/

    /* Show window and enter event loop */
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_context);

    return 0;
}

