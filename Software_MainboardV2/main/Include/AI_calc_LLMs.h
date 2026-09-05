/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

LLMs.h: Settings for AI models etc.

*/

#ifndef LLMS_H
#define LLMS_H


// Inlcudes

#include "AI_calc_camera.h"





// AI Models (only ChatGPT implemented atm)

typedef enum ai_model{
    AI_MODEL_OPENAI,
    AI_MODEL_GEMINI,
    AI_MODEL_CLAUDE
} ai_model_TypeDef;






// OpenAI

#define OPENAI_DONE_BIT BIT0

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} openAI_response_buffer_t;

// OpenAI settings

#define OPENAI_API_RESPONSE_TIMEOUT 50      // in s

// JSON Request settings
#define REQUEST_JSON_INITIAL_SIZE 4096
#define REQUEST_JSON_MAX_SIZE (5*1024*1024)     // 5 MiB (!) Needs 8MB PSRAM ESP32S3 Model

#if REQUEST_JSON_MAX_SIZE < ((PICTURES_MAX_TOTAL_SIZE * 4/3) + 10000)
#error "The Request JSON max size is not big enough for JPG data + the rest of the JSON string"
#endif

/*
For Prompt instructions see static const char openai_instructions[] in LLMs.c 
For supported models see const char* openai_supported_models[] in LLMs.c
Also see MAX_CONVERSATION_LENGTH in UI.h for maximum amount of text exchanges in a chat
Also see ANSWER_PAGE_LENGTH in UI.h for maximum anser length
*/




// Base64 conversion of JPEGS

#define JPEG_READ_BUFFER_SIZE 1200
#define BASE64_BUFFER_SIZE ((JPEG_READ_BUFFER_SIZE / 3) * 4 + 1)        // +1 for string terminator

#if JPEG_READ_BUFFER_SIZE % 3 != 0
#error "JPEG_READ_BUFFER_SIZE needs to be a multiple of 3 Bytes!"
#endif






// General Get Answer Task

typedef struct{
    char* prompt;
    char* answer;
    size_t answer_length;
    uint8_t start_new_chat;
    char (*picture_paths)[64];
    char* model_version;
} ans_task_params;








// Exported functions

// Generic exported functions
void llms_init(void);
esp_err_t save_API_Key(ai_model_TypeDef ai_model, char* api_key);

// OpenAI exported functions
void start_openai_conversation(char* new_prompt, char* ans_dest, size_t ans_dest_size, char (*pic_urls)[64], const char* model_ver);
void get_openai_response(char* new_prompt, char* ans_dest, size_t ans_dest_size, char (*pic_urls)[64], const char* model_ver);
void load_prev_openai_response(uint16_t chat_position, char* ans_dest, size_t ans_dest_size);
void load_prev_openai_prompt(uint16_t chat_position, char* prompt_dest, size_t prompt_dest_size);

#endif