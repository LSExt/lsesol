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
#include "config.h"
#include "globals.h"


/* Global widget handles needed for backend hooks */
static Widget output_text_w;
static Widget input_text_w;
static Widget conv_menu_w;
static Widget conv_pane_w;
static Widget model_menu_w;
static Widget model_pane_w;
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

/* Callback: Thread Selected */
static void conv_select_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *conv_name = (char *)client_data;
    printf("Selected Context: %s\n", conv_name);
    char *chat_name = (char *)client_data;
    if (!chat_name) return;

    /* Update global path */
    snprintf(g_current_conversation, sizeof(g_current_conversation),
             "%s/%s.gemini.json", g_config.state_dir, chat_name);

    printf("Switched conversation to: %s (%s)\n", chat_name, g_current_conversation);

    /* Clear or reload output text area */
    XmTextSetString(output_text_w, "--- Switched Conversation ---\n");

    /* TODO: Call backend helper to reload previous messages from this file into output_text_w */
}

Widget create_main_menubar(Widget parent)
{
    Widget menu_bar, conv_menu_pane, conv_cascade, compress_item;
    XmString label_str;
    Widget menu_edit, cascade_edit;
    XmString s_edit;
    int n;
    Arg args[20];

    /* -------------------------------------------------------------
     * TOP MENU BAR (XmMenuBar)
     * ------------------------------------------------------------- */
    menu_bar = XmCreateMenuBar(parent, "menuBar", NULL, 0);
    XtManageChild(menu_bar);

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
    conv_menu_pane = XmCreatePulldownMenu(menu_bar, "convMenuPane", NULL, 0);

    /* Menu Item: Compress */
    compress_item = XmCreatePushButton(conv_menu_pane, "Compress (Summarize History)", NULL, 0);
    XtManageChild(compress_item);
    XtAddCallback(compress_item, XmNactivateCallback, compress_cb, NULL);

    /* Cascade button on the menu bar pointing to the pane */
    label_str = XmStringCreateLocalized("Conversation");
    n = 0;
    XtSetArg(args[n], XmNsubMenuId, conv_menu_pane); n++;
    XtSetArg(args[n], XmNlabelString, label_str); n++;
    conv_cascade = XmCreateCascadeButton(menu_bar, "convCascade", args, n);
    XtManageChild(conv_cascade);
    XmStringFree(label_str); /* Free your copy to prevent a memory leak */

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
    XtManageChild(top_bar);

    /* Thread Dropdown (OptionMenu) */
    int chat_count = 0;
    char **chats = config_list_conversations(&chat_count);
    Widget initial_btn = NULL;
    char *initial_chat_name = NULL;

    conv_pane = XmCreatePulldownMenu(top_bar, "convPane", NULL, 0);
    conv_pane_w = conv_pane;

    if (chat_count == 0) {
        /* No saved files found: initialize "default" */
        initial_chat_name = strdup("default");
        initial_btn = XmCreatePushButton(conv_pane, initial_chat_name, NULL, 0);
        XtManageChild(initial_btn);
        XtAddCallback(initial_btn, XmNactivateCallback, conv_select_cb, (XtPointer)initial_chat_name);
    } else {
        int i;
        for (i = 0; i < chat_count; i++) {
            /* Keep persistent heap string for each callback client_data */
            char *saved_name = strdup(chats[i]);
            Widget btn = XmCreatePushButton(conv_pane, saved_name, NULL, 0);
            XtManageChild(btn);
            XtAddCallback(btn, XmNactivateCallback, conv_select_cb, (XtPointer)saved_name);

            if (i == 0) {
                initial_btn = btn;
                initial_chat_name = saved_name;
            }
        }
    }

    n = 0;
    XtSetArg(args[n], XmNlabelString, XmStringCreateLocalized("Chat / Context:")); n++;
    XtSetArg(args[n], XmNsubMenuId, conv_pane); n++;
    conv_menu_w = XmCreateOptionMenu(top_bar, "convOptionMenu", args, n);
    XtManageChild(conv_menu_w);

    /* --- INITIALIZE GLOBAL VARIABLE ON APPLICATION STARTUP --- */
    if (initial_chat_name) {
        snprintf(g_current_conversation, sizeof(g_current_conversation),
                 "%s/%s.gemini.json", g_config.state_dir, initial_chat_name);
        init_state_file();
        printf("Initial conversation set to: %s\n", g_current_conversation);
    }

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

