/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

UI.h: UI-Settings, Variable Types, extern Variables and Functions

*/

#ifndef UI_H
#define UI_H

// Includes

#include "AI_calc_maindisplay.h"

#include <stdint.h>




// Page length defines

#define MAX_SCRIBBLE_PAGE_SECTORS 6
#define SCRIBBLE_PAGE_LENGTH ((MAX_SCRIBBLE_PAGE_SECTORS * MAIN_DISPLAY_COLUMNS * MAIN_DISPLAY_ROWS) - MAIN_DISPLAY_COLUMNS)

#define MAX_ANSWER_PAGE_SECTORS 60
#define ANSWER_PAGE_LENGTH (MAX_ANSWER_PAGE_SECTORS * MAIN_DISPLAY_COLUMNS * MAIN_DISPLAY_ROWS)





// UI-Task Defines

#define UI_LOOP_DELAYTIME 20        // in ms
#define UI_LOOPS_PER_MIN ((1000 / UI_LOOP_DELAYTIME) * 60)






// UI-Unlock Code in calculator mode

#define UI_UNLOCK_CODE "02-04-2004"





// Menu-Defines

#define NO_MENU_SELECTED 0



// Type-Definitions

// UI Modes
typedef enum{
    UI_MODE_SCRIBBLE,       // for typing in prompts
    UI_MODE_TEXTINPUT,      // for typing in passwords, API keys, etc. (from menu)
    UI_MODE_CHATVIEW,       // for viewing previous conversations  
    UI_MODE_MENU,           // for changing settings, etc.
    UI_MODE_FILEVIEW        // viewing text files that have been saved to the device
}UI_mode_TypeDef;

typedef struct{
    uint8_t autooff_tresh_mins;
    UI_mode_TypeDef UI_mode;
}UI_TypeDef;



typedef enum{
    CHATVIEW_PROMPT,        // currently looking at prompt text
    CHATVIEW_ANSWER         // currently looking at answer text
} chatview_text_TypeDef;

typedef struct menu_entry_TypeDef menu_entry_TypeDef;
struct menu_entry_TypeDef{
    char entry_text[MAIN_DISPLAY_COLUMNS];      // text
    menu_entry_TypeDef *menu_child;             // Pointer at menu-entry-array that gets opened through enter
    menu_entry_TypeDef *menu_parent;            // Pointer at menu-entry-array that gets opened through back
    void (*entry_callback)(void);               // Pointer at function that gets executed when the entry is opened with enter
};

/*

Example for a menu

menu_entry_TypeDef main_menu[] = {
    {" Manage Wifis       ",wifi_menu,NULL,NULL},       // no callback, open other menu
    {" Manage LLMs        ",LLM_menu,NULL,NULL},
    {" Open File System   ",NULL,NULL,do_something}     // callback, execute callback function
};

void do_something(void){
    ...
}

*/

// Different UI-Modes

typedef struct{
    char scribble_page[SCRIBBLE_PAGE_LENGTH + 1];           // +1 for String-terminator
    char scribble_page_backup[SCRIBBLE_PAGE_LENGTH + 1];    // +1 for String-terminator
    uint8_t current_scribble_page_sector;
    uint16_t scribble_cursor_pos;
} scribble_mode_typeDef;

typedef struct{
    char command_prompt[MAIN_DISPLAY_COLUMNS+1];
    uint8_t sensitive_information;                          // boolean
} text_input_mode_typeDef;

typedef struct{
    char* prompt_texts[SCRIBBLE_PAGE_LENGTH + 1];           // +1 for String-terminator
    char* anwer_texts[ANSWER_PAGE_LENGTH + 1];              // +1 for String-terminator
    uint8_t current_chat_page_sector;
    uint16_t current_chat;                                  // even = showing prompt, uneven = showing answer
} chatview_mode_typeDef;

typedef struct{
    menu_entry_TypeDef* current_menu;       // Points at the array of menu_entry_TypeDef that is currently opened
    uint8_t menu_cursor_pos;
} menu_mode_typeDef;

typedef struct{

} fileview_mode_typeDef;








// Extern global Variables

extern UI_TypeDef UI;




// Exported functions

void UI_init(void);




#endif

