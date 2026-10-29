/*
 * gemini_motif.c - Standalone Motif / CDE Frontend for Gemini Client
 * Target: Solaris 2.5.1 / GCC 4.3.6 / Motif 1.2 or 2.x
 *
 * gcc -O2 gemini_motif.c -o gemini_motif  -I/usr/dt/include -I/usr/openwin/include -L/usr/dt/lib -R/usr/dt/lib -L/usr/openwin/lib -R/usr/openwin/lib -lXm -lXt -lX11 -lsocket -lnsl 
 */

#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/Label.h>
#include <Xm/SelectioB.h>

/* Global widget handles needed for backend hooks */
static Widget output_text_w;
static Widget input_text_w;
static Widget conv_menu_w;
static Widget conv_pane_w;      /* <-- ADD THIS */
static Widget model_menu_w;
static Widget toplevel_w;
/* Forward declaration so new_chat_ok_cb can reference it */
static void conv_select_cb(Widget w, XtPointer client_data, XtPointer call_data);


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
            /* 1. Create a persistent heap copy for client_data callback */
            char *saved_name = strdup(chat_name);

            /* 2. Dynamically add new push button to the dropdown menu pane */
            n = 0;
            new_item = XmCreatePushButton(conv_pane_w, saved_name, args, n);
            XtManageChild(new_item);
            
            /* Hook existing selection callback */
            XtAddCallback(new_item, XmNactivateCallback, conv_select_cb, (XtPointer)saved_name);

            /* 3. Update OptionMenu to display the newly created chat */
            n = 0;
            XtSetArg(args[n], XmNmenuHistory, new_item); n++;
            XtSetValues(conv_menu_w, args, n);

            /* 4. Trigger backend logic (optional: clear output buffer for fresh chat) */
            printf("Created and switched to conversation: %s\n", chat_name);
            XmTextSetString(output_text_w, "--- New Conversation Started ---\n");
        }
        XtFree(chat_name);
    }

    /* Destroy the prompt dialog */
    XtDestroyWidget(w);
}

/* Callback: When user clicks "Cancel" in the New Chat Dialog */
static void new_chat_cancel_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(w);
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
        XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), "\n\nUser: ");
        XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), prompt);
        XmTextInsert(output_text_w, XmTextGetLastPosition(output_text_w), "\nGemini: [Waiting for API integration...]");
        
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

/* Callback: Model Selected */
static void model_select_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *model_name = (char *)client_data;
    printf("Selected Model: %s\n", model_name);
}

/* Callback: Thread Selected */
static void conv_select_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    char *conv_name = (char *)client_data;
    printf("Selected Context: %s\n", conv_name);
}

int main(int argc, char *argv[])
{
    XtAppContext app_context;
    Widget toplevel, main_window, form;
    Widget top_bar, conv_pane, model_pane, new_btn;
    Widget btn_sample_chat, btn_model;
    Widget send_btn;
    Arg args[20];
    int n;

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

    /* Main Window Shell */
    n = 0;
    XtSetArg(args[n], XmNtitle, "Gemini AI Client - Solaris CDE"); n++;
    XtSetArg(args[n], XmNwidth, 700); n++;
    XtSetArg(args[n], XmNheight, 550); n++;
    XtSetValues(toplevel, args, n);

    main_window = XmCreateMainWindow(toplevel, "mainWindow", NULL, 0);
    XtManageChild(main_window);

    /* Top-level Form layout */
    form = XmCreateForm(main_window, "mainForm", NULL, 0);
    XtManageChild(form);

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
    conv_pane = XmCreatePulldownMenu(top_bar, "convPane", NULL, 0);
    conv_pane_w = conv_pane;
    btn_sample_chat = XmCreatePushButton(conv_pane, "sample chat", NULL, 0);
    XtManageChild(btn_sample_chat);
    XtAddCallback(btn_sample_chat, XmNactivateCallback, conv_select_cb, (XtPointer)"sample chat");

    n = 0;
    XtSetArg(args[n], XmNlabelString, XmStringCreateLocalized("Chat / Context:")); n++;
    XtSetArg(args[n], XmNsubMenuId, conv_pane); n++;
    conv_menu_w = XmCreateOptionMenu(top_bar, "convOptionMenu", args, n);
    XtManageChild(conv_menu_w);

    /* New Chat Button */
    n = 0;
    new_btn = XmCreatePushButton(top_bar, "New Chat", args, n);
    XtManageChild(new_btn);
    XtAddCallback(new_btn, XmNactivateCallback, new_conv_cb, NULL);

    /* Model Dropdown (OptionMenu) */
    model_pane = XmCreatePulldownMenu(top_bar, "modelPane", NULL, 0);
    btn_model = XmCreatePushButton(model_pane, "gemini-flash-3.8", NULL, 0);
    XtManageChild(btn_model);
    XtAddCallback(btn_model, XmNactivateCallback, model_select_cb, (XtPointer)"gemini-flash-3.8");

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
    input_text_w = XmCreateScrolledText(form, "inputText", args, n);
    XtManageChild(input_text_w);

    /* -------------------------------------------------------------
     * 3. OUTPUT CONVERSATION PANE (With Copy/Paste Scrolling)
     * ------------------------------------------------------------- */
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, top_bar); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNbottomWidget, input_text_w); n++;
    XtSetArg(args[n], XmNeditable, False); n++;               /* Read only */
    XtSetArg(args[n], XmNcursorPositionVisible, False); n++;
    XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
    XtSetArg(args[n], XmNwordWrap, True); n++;
    output_text_w = XmCreateScrolledText(form, "outputText", args, n);
    XtManageChild(output_text_w);

    /* Set default placeholder text */
    XmTextSetString(output_text_w, "response goes here\n");

    /* Show window and enter event loop */
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_context);

    return 0;
}

