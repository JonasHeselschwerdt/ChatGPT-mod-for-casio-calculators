/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

LLMs.c: Handles communication with the APIs of AI-Assistants

*/

// Implemented: OpenAI Responses API
// Still missing: Gemini, Claude



// Includes

#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "string.h"
#include <stdio.h>
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "AI_calc_LLMS.h"
#include "AI_calc_UI.h"
#include "AI_calc_camera.h"






// Static variables (generic)

static nvs_handle_t api_keys_handle;

static const char base64_LUT[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};





// Static variables (openAI)

static EventGroupHandle_t openai_events;
static nvs_handle_t responseID_handle;

// Instructions that get added to every prompt
static const char openai_instructions[] =   "Use only basic ASCII characters"
                                            "Do not use Unicode or Emojis"
                                            "Client device can only display text and latex formated strings"
                                            "Do not send back images or diagrams/tables";

static const char* openai_supported_models[] = {
    "gpt-5.5",
    "gpt-5.6",
    "gpt-5.6-terra",
    "gpt-5.6-luna"
};







// Static function declarations

static void apikey_nvs_init(void);
static void base64_encode(uint8_t* raw_data, size_t raw_data_lenght, char* dest);

// OpenAI chat save logic
static esp_err_t openAI_chat_clear_chatdata(void);
static esp_err_t openAI_chatdata_append_chat(const char* prompt, char* answer);
static esp_err_t openAI_chatdata_get_item (char* string_dest, uint16_t chat_pos, uint8_t show_response);

// OpenAI nvs
static void openAI_nvs_init(void);

// OpenAI main task
static void openAI_get_answer_task(void* parameters);

// OpenAI responses API implementation
static esp_err_t json_append(char** buffer,size_t* buffer_size,size_t* position,const char* string);
static esp_err_t openAI_create_request_json(const char* previous_response_id,char** request_json, const ans_task_params* parameters);
static esp_err_t openAI_http_post(const char* request_json,char** response_json);
static esp_err_t openAI_process_response_json(const char* response_json,char* response_id, ans_task_params* parameters);







// Static functions (openAI related)

static void openAI_nvs_init(void){

    // For openAI related settings and the last responseID
    ESP_ERROR_CHECK(nvs_open("OpenAIAPI", NVS_READWRITE, &responseID_handle));
}

static esp_err_t openAI_chat_clear_chatdata(void){

    // Delete latest response ID and chatdata
    if (nvs_set_str(responseID_handle,"response_ID","") != ESP_OK){
        return ESP_FAIL;
    }
    if (nvs_commit(responseID_handle != ESP_OK)){
        return ESP_FAIL;
    }
    // Clear the entire chat/openai directory
    DIR* dir = opendir("/littlefs/chat/openai");
    if (!dir) {
        return ESP_FAIL;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL){
        // Ignore subdirectories
        if (entry->d_type == DT_DIR){
            continue;
        }
        char filepath[300];
        snprintf(filepath,sizeof(filepath),"/littlefs/chat/openai/%s",entry->d_name);
        if (unlink(filepath) != 0) {
            closedir(dir);
            return ESP_FAIL;
        }
    }
    closedir(dir);
    return ESP_OK;
}

static esp_err_t openAI_chatdata_append_chat(const char* prompt, char* answer){

    // Adding an interaction with ChatGPT to the chat directory
    if (strlen(answer) > ANSWER_PAGE_LENGTH){
        answer[ANSWER_PAGE_LENGTH] = '\0';
    }
    // First read the chat_pos of the newest chat
    DIR* dir = opendir("/littlefs/chat/openai");
    if (!dir){
        return ESP_FAIL;
    }
    struct dirent* entry;
    int highest_chat_pos = -1;
    while ((entry = readdir(dir)) != NULL){
        if (entry->d_type == DT_DIR)
            continue;
        // Convert ending of file name to int
        int selected_file_pos = atoi(entry->d_name + 7);    // this works because answer_ and prompt_ are the same length
        highest_chat_pos = (selected_file_pos > highest_chat_pos)?selected_file_pos:highest_chat_pos;
    }
    closedir(dir);
    if (highest_chat_pos >= (MAX_CONVERSATION_LENGTH-1)){
        // This situation should be avoided externally in UI.h (no more space for new chat)
        return ESP_FAIL;
    }
    // Save data
    // Assuming the highest_chat_pos can be up to 65536 (is an uint16_t):
    char filename_answer[64];
    char filename_prompt[64];
    snprintf(filename_answer,sizeof(filename_answer),"/littlefs/chat/openai/answer_%d",(highest_chat_pos+1));
    snprintf(filename_prompt,sizeof(filename_prompt),"/littlefs/chat/openai/prompt_%d",(highest_chat_pos+1));
    FILE* prompt_file = fopen(filename_prompt,"wb");
    if (!prompt_file){
        return ESP_FAIL;
    }
    fwrite(prompt,1,(strlen(prompt)+1),prompt_file);
    fclose(prompt_file);
    FILE* answer_file = fopen(filename_answer,"wb");
    if (!answer_file){
        return ESP_FAIL;
    }
    fwrite(answer,1,(strlen(answer)+1),answer_file);
    fclose(answer_file);
    return ESP_OK;
}

static esp_err_t openAI_chatdata_get_item(char* string_dest, uint16_t chat_pos, uint8_t show_response){

    // Loads either a prompt or a response_id from the chat directory
    // This function assumes string_dest has size of at least ANSWER_PAGE_LENGTH+1
    char filepath[64];
    if (show_response){
        snprintf(filepath, sizeof(filepath),"/littlefs/chat/openai/answer_%u", chat_pos);
    }
    else{
        snprintf(filepath, sizeof(filepath),"/littlefs/chat/openai/prompt_%u", chat_pos);
    }
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return ESP_FAIL;
    }
    size_t bytes_read = fread(string_dest,1,(ANSWER_PAGE_LENGTH+1),file);
    fclose(file);
    return ESP_OK;
}


static esp_err_t json_append(char** buffer,size_t* buffer_size,size_t* position,const char* string){

    // Appends to unformated Json string, allocates more memory if needed
    if (buffer == NULL || *buffer == NULL || buffer_size == NULL || position == NULL || string == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    size_t string_length = strlen(string);
    // +1 for terminating '\0'
    size_t required_size = *position + string_length + 1;
    if (required_size > *buffer_size){
        // Grow exponentially to avoid frequent reallocations
        size_t new_size = *buffer_size;
        while (new_size < required_size){
            new_size *= 2;
            if (new_size >= REQUEST_JSON_MAX_SIZE){
                new_size = REQUEST_JSON_MAX_SIZE; 
                break;
            }
        }
        if (new_size < required_size) {
            return ESP_ERR_NO_MEM;
        }
        char* new_buffer = heap_caps_realloc(*buffer,new_size,MALLOC_CAP_SPIRAM);
        if (new_buffer == NULL){
            return ESP_ERR_NO_MEM;
        }
        *buffer = new_buffer;
        *buffer_size = new_size;
    }
    memcpy(&(*buffer)[*position],string,string_length);
    *position += string_length;
    (*buffer)[*position] = '\0';
    return ESP_OK;
}

static esp_err_t openAI_create_request_json(const char* previous_response_id,char** request_json,const ans_task_params* parameters){

    //Creating an unformated JSON string, not using cJson to save RAM when sending Image files:
    if (request_json == NULL || parameters == NULL || parameters->prompt == NULL || parameters->model_version == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    if (parameters->model_version[0] == '\0'){
        return ESP_ERR_INVALID_ARG;
    }
    *request_json = NULL;
    // Small initial buffer
    size_t buffer_size = REQUEST_JSON_INITIAL_SIZE;
    char* json = heap_caps_malloc(buffer_size,MALLOC_CAP_SPIRAM);
    if (json == NULL){
        return ESP_ERR_NO_MEM;
    }
    size_t position = 0;
    esp_err_t ret;
    json[0] = '\0';
    // {
    ret = json_append(&json,&buffer_size,&position,"{");
    if (ret != ESP_OK){
        goto cleanup;
    }
    // "model":"..."
    ret = json_append(&json,&buffer_size,&position,"\"model\":\"");
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = json_append(&json,&buffer_size,&position,parameters->model_version);
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = json_append(&json,&buffer_size,&position,"\"");
    if (ret != ESP_OK){
        goto cleanup;
    }
    // ,"instructions":"..."
    ret = json_append(&json,&buffer_size,&position,",\"instructions\":\"");
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = json_append(&json,&buffer_size,&position,openai_instructions);
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = json_append(&json,&buffer_size,&position,"\"");
    if (ret != ESP_OK){
        goto cleanup;
    }
    // ,"previous_response_id":"..."
    if (previous_response_id != NULL && previous_response_id[0] != '\0'){
        ret = json_append(&json,&buffer_size,&position,",\"previous_response_id\":\"");
        if (ret != ESP_OK){
            goto cleanup;
        }
        ret = json_append(&json,&buffer_size,&position,previous_response_id);
        if (ret != ESP_OK){
            goto cleanup;
        }
        ret = json_append(&json,&buffer_size,&position,"\"");
        if (ret != ESP_OK){
            goto cleanup;
        }
    }
    // ,"input":[
    ret = json_append(&json,&buffer_size,&position,",\"input\":[");
    if (ret != ESP_OK){
        goto cleanup;
    }
    // {"role":"user","content":[
    ret = json_append(&json,&buffer_size,&position,"{\"role\":\"user\",\"content\":[");
    if (ret != ESP_OK){
        goto cleanup;
    }
    // {"type":"input_text","text":"..."
    ret = json_append(&json,&buffer_size,&position,"{\"type\":\"input_text\",\"text\":\"");
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = json_append(&json,&buffer_size,&position,parameters->prompt);
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = json_append(&json,&buffer_size,&position,"\"}");
    if (ret != ESP_OK){
        goto cleanup;
    }
    // Adiing Image data to JSON
    for (uint8_t i = 0; i < MAX_SAVED_PICTURES; i++){
        if (parameters->picture_paths[i][0] == '\0'){
            continue;
        }
        FILE* file = fopen(parameters->picture_paths[i], "rb");
        if (file == NULL){
            ret = ESP_FAIL;
            goto cleanup;
        }
        ret = json_append(&json, &buffer_size, &position, ",{\"type\":\"input_image\",\"image_url\":\"data:image/jpeg;base64,");
        if (ret != ESP_OK) {
            fclose(file);
            goto cleanup;
        }
        uint8_t raw_buffer[JPEG_READ_BUFFER_SIZE];
        char base64_buffer[BASE64_BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(raw_buffer, 1, sizeof(raw_buffer), file)) > 0) {
            base64_buffer[0] = '\0';
            base64_encode(raw_buffer, bytes_read, base64_buffer);
            ret = json_append(&json, &buffer_size, &position, base64_buffer);
            if (ret != ESP_OK) {
                fclose(file);
                goto cleanup;
            }
        }
        fclose(file);
        ret = json_append(&json, &buffer_size, &position, "\"}");
        if (ret != ESP_OK) {
            goto cleanup;
        }
    }
    // Close content, user message, input and root object
    ret = json_append(&json,&buffer_size,&position,"]}]}");
    if (ret != ESP_OK){
        goto cleanup;
    }
    *request_json = json;
    return ESP_OK;

cleanup:

    free(json);
    return ret;
}

static esp_err_t openAI_http_event_handler(esp_http_client_event_t* event){

    // For loading OpenAI response Json
    if (event->event_id == HTTP_EVENT_ON_DATA){
        openAI_response_buffer_t* response = (openAI_response_buffer_t*)event->user_data;
        size_t required_size = response->length + event->data_len + 1;
        if (required_size > response->capacity){
            size_t new_capacity = response->capacity * 2;
            while (new_capacity < required_size){
                new_capacity *= 2;
            }
            char* new_data = realloc(response->data,new_capacity);
            if (new_data == NULL) {
                return ESP_ERR_NO_MEM;
            }
            response->data = new_data;
            response->capacity = new_capacity;
        }
        memcpy(response->data + response->length,event->data,event->data_len);
        response->length += event->data_len;
        response->data[response->length] = '\0';
    }
    return ESP_OK;
}

static esp_err_t openAI_http_post(const char* request_json,char** response_json){

    // For sending the Request JSON to OpenAI
    if (request_json == NULL || response_json == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    *response_json = NULL;
    openAI_response_buffer_t response = {
        .data = malloc(4096),
        .length = 0,
        .capacity = 4096
    };
    if (response.data == NULL){
        return ESP_ERR_NO_MEM;
    }
    response.data[0] = '\0';
    esp_err_t ret = ESP_OK;
    // HTTP client configuration
    esp_http_client_config_t config = {
        .url = "https://api.openai.com/v1/responses",
        .method = HTTP_METHOD_POST,
        .timeout_ms = (OPENAI_API_RESPONSE_TIMEOUT * 1000),
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = openAI_http_event_handler,
        .user_data = &response
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL){
        free(response.data);
        return ESP_FAIL;
    }
    // HTTP headers
    // Get API key from nvs
    char API_key[200] = {0};
    size_t length = sizeof(API_key);
    ret = nvs_get_str(api_keys_handle,"openai",API_key,&length);
    if (ret != ESP_OK){
        goto cleanup;
    }
    ret = esp_http_client_set_header(client,"Content-Type","application/json");
    if (ret != ESP_OK){
        goto cleanup;
    }
    char auth_header[256];
    snprintf(auth_header,sizeof(auth_header),"Bearer %s",API_key);
    ret = esp_http_client_set_header(client,"Authorization",auth_header);
    if (ret != ESP_OK){
        goto cleanup;
    }
    // Request body
    ret = esp_http_client_set_post_field(client,request_json,strlen(request_json));
    if (ret != ESP_OK){
        goto cleanup;
    }
    // Perform request
    ret = esp_http_client_perform(client);
    if (ret != ESP_OK){
        ESP_LOGE("OpenAI","HTTP POST failed: %s",esp_err_to_name(ret));
        goto cleanup;
    }
    // Response
    if (response.length == 0){
        ESP_LOGE("OpenAI", "Response body is empty");
        ret = ESP_FAIL;
        goto cleanup;
    }
    *response_json = response.data;
    response.data = NULL;
    ESP_LOGI("OpenAI","Received %u bytes",(unsigned)response.length);
    // HTTP status
    int status_code = esp_http_client_get_status_code(client);
    if (status_code < 200 || status_code >= 300){
        ESP_LOGE("OpenAI","Received Status code: %d",status_code);
        ret = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    free(response.data);
    esp_http_client_cleanup(client);
    return ret;
}

static esp_err_t openAI_process_response_json(const char* response_json, char* response_id, ans_task_params* parameters){

    // Parses received JSON string (using cJson)
    if (response_json == NULL || response_id == NULL || parameters == NULL || parameters->answer == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    cJSON* root = cJSON_Parse(response_json);
    if (root == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    esp_err_t ret = ESP_FAIL;
    // Response ID
    cJSON* id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL) {
        goto cleanup;
    }
    strcpy(response_id, id->valuestring);
    // output
    cJSON* output = cJSON_GetObjectItemCaseSensitive(root, "output");
    if (!cJSON_IsArray(output)){
        goto cleanup;
    }
    // Search output items
    cJSON* output_item = NULL;
    cJSON_ArrayForEach(output_item, output){
        cJSON* type = cJSON_GetObjectItemCaseSensitive(output_item, "type");
        if (!cJSON_IsString(type) || strcmp(type->valuestring, "message") != 0) {
            continue;
        }
        cJSON* content = cJSON_GetObjectItemCaseSensitive(output_item, "content");
        if (!cJSON_IsArray(content)) {
            continue;
        }
        cJSON* content_item = NULL;
        cJSON_ArrayForEach(content_item, content) {
            cJSON* content_type = cJSON_GetObjectItemCaseSensitive(content_item, "type");
            if (!cJSON_IsString(content_type) || strcmp(content_type->valuestring, "output_text") != 0) {
                continue;
            }
            cJSON* text =
                cJSON_GetObjectItemCaseSensitive(content_item, "text");
            if (!cJSON_IsString(text) || text->valuestring == NULL) {
                continue;
            }
            strncpy(parameters->answer,text->valuestring,ANSWER_PAGE_LENGTH);
            // Always terminate at ANSWER_PAGE_LENGTH
            parameters->answer[ANSWER_PAGE_LENGTH] = '\0';
            ret = ESP_OK;
            goto cleanup;
        }
    }

cleanup:

    cJSON_Delete(root);
    return ret;
    return ESP_OK;
}

static void openAI_get_answer_task(void* parameters){

    ans_task_params* params = (ans_task_params*) parameters;
    char* request_json = NULL;
    if (params->start_new_chat){
        // Delete chatdata
        openAI_chat_clear_chatdata();
        // Create request Json
        if (openAI_create_request_json("",&request_json,params) != ESP_OK){
            strcpy(params->answer,"JSON CREATE ERROR");
            openAI_chatdata_append_chat(params->prompt,params->answer);
            nvs_set_str(responseID_handle,"response_ID","");
            nvs_commit(responseID_handle);
            xEventGroupSetBits(openai_events, OPENAI_DONE_BIT);
            vTaskDelete(NULL);
        }
        //ESP_LOGI("JSON Request","\n %s",request_json);
    }
    else{
        // Get Last Response ID
        char previous_response_id[128] = {0};
        size_t length = sizeof(previous_response_id);
        if (nvs_get_str(responseID_handle,"response_ID",previous_response_id,&length) != ESP_OK){
            ESP_LOGI("OpenAI","No previous response ID available, starting new chat");
            strcpy(previous_response_id,"");
        }
        // Create request Json
        if (openAI_create_request_json(previous_response_id,&request_json,parameters) != ESP_OK){
            strcpy(params->answer,"JSON CREATE ERROR");
            openAI_chatdata_append_chat(params->prompt,params->answer);
            nvs_set_str(responseID_handle,"response_ID","");
            nvs_commit(responseID_handle);
            xEventGroupSetBits(openai_events, OPENAI_DONE_BIT);
            vTaskDelete(NULL);
        }
        //ESP_LOGI("JSON Request","\n %s",request_json);
    }
    // Http POST Json
    char* response_json = NULL;
    if (openAI_http_post(request_json,&response_json) != ESP_OK){
        strcpy(params->answer,"HTTP POST ERROR");
        openAI_chatdata_append_chat(params->prompt,params->answer);
        nvs_set_str(responseID_handle,"response_ID","");
        nvs_commit(responseID_handle);
        free(request_json);
        xEventGroupSetBits(openai_events, OPENAI_DONE_BIT);
        vTaskDelete(NULL);
    }
    free(request_json);
    //ESP_LOGI("JSON RESPONSE", "\n %s", response_json);
    // Process response Json
    char new_latest_response_id[128];
    openAI_process_response_json(response_json,new_latest_response_id,params);
    free(response_json);
    // Append text exchange to chatdata
    openAI_chatdata_append_chat(params->prompt,params->answer);
    // Save latest Response ID
    nvs_set_str(responseID_handle,"response_ID",new_latest_response_id);
    nvs_commit(responseID_handle);

    // End Task
    xEventGroupSetBits(openai_events, OPENAI_DONE_BIT);
    vTaskDelete(NULL);
}





// Generic static functions

static void apikey_nvs_init(void){

    ESP_ERROR_CHECK(nvs_open("api_keys", NVS_READWRITE, &api_keys_handle));
}

static void base64_encode(uint8_t* raw_data, size_t raw_data_length, char* dest){

    /*
    To base64 encode big JPGs (e.g. 0.8MiB): Read small buffers (e.g. raw_data_length = 1200 Bytes) and convert them
    into separate base64 strings, buffersize (in bytes) has to be divisible by 3.
    The last buffer (and only this one) of the JPG data can be not divisible by 3
    Images sent to OpenAI API must be Base64 encoded
    */
    uint8_t padding_leftover = (uint8_t)(raw_data_length % 3);
    size_t encoding_steps = (raw_data_length / 3);
    if (padding_leftover){
        encoding_steps++;
    }
    for (size_t i=0; i< encoding_steps; i++){
        if ((!padding_leftover) || (i!=encoding_steps-1)){
            // Do not need padding
            uint32_t encoding_buf = 0;
            char string_buf[5];
            encoding_buf += (uint32_t)((raw_data[i*3])<<16);
            encoding_buf += (uint32_t)((raw_data[(i*3)+1])<<8);
            encoding_buf += raw_data[(i*3)+2];
            string_buf[0] = base64_LUT[(uint8_t)((encoding_buf>>18)&0b00111111)];
            string_buf[1] = base64_LUT[(uint8_t)((encoding_buf>>12)&0b00111111)];
            string_buf[2] = base64_LUT[(uint8_t)((encoding_buf>>6)&0b00111111)];
            string_buf[3] = base64_LUT[(uint8_t)((encoding_buf)&0b00111111)];
            string_buf[4] = '\0';
            strcat(dest,string_buf);
        }
        else if ((padding_leftover == 1) && (i==(encoding_steps-1))){
            // Padding needed
            uint16_t encoding_buf = 0;
            char string_buf[5];
            encoding_buf += (uint16_t)(raw_data[i*3] << 4);
            string_buf[0] = base64_LUT[(uint8_t)((encoding_buf>>6) & 0b00111111)];
            string_buf[1] = base64_LUT[(uint8_t)(encoding_buf & 0b00111111)];
            string_buf[2] = '=';
            string_buf[3] = '=';
            string_buf[4] = '\0';
            strcat(dest,string_buf);
        }
        else{
            // Padding needed
            uint32_t encoding_buf = 0;
            char string_buf[5];
            encoding_buf += (uint32_t)(raw_data[i*3] << 10);
            encoding_buf += (uint32_t)(raw_data[(i*3)+1]);
            string_buf[0] = base64_LUT[(uint8_t)((encoding_buf>>12) & 0b00111111)];
            string_buf[1] = base64_LUT[(uint8_t)((encoding_buf>>6) & 0b00111111)];
            string_buf[2] = base64_LUT[(uint8_t)(encoding_buf & 0b00111111)];
            string_buf[3] = '=';
            string_buf[4] = '\0';
            strcat(dest,string_buf);
        }
    }
}




// Exported functions (generic)

void llms_init(void){

    apikey_nvs_init();
    openAI_nvs_init();
    // Each model gets their own directory for chat data
    mkdir("/littlefs/chat/openai",0755);
    mkdir("/littlefs/chat/claude",0755);
    mkdir("/littlefs/chat/gemini",0755);
}

esp_err_t save_API_Key(ai_model_TypeDef ai_model, char* api_key){

    if (ai_model == AI_MODEL_OPENAI){
        if(nvs_set_str(api_keys_handle,"openai",api_key)!=ESP_OK){
            return ESP_FAIL;
        }
    }
    else if (ai_model == AI_MODEL_GEMINI){
        if(nvs_set_str(api_keys_handle,"gemini",api_key)!=ESP_OK){
            return ESP_FAIL;
        }
    }
    else if (ai_model == AI_MODEL_CLAUDE){
        if(nvs_set_str(api_keys_handle,"claude",api_key)!=ESP_OK){
            return ESP_FAIL;
        }
    }
    if(nvs_commit(api_keys_handle)!=ESP_OK){
        return ESP_FAIL;
    }
    return ESP_OK;
}






// Exported functions (OpenAI)

void start_openai_conversation(char* new_prompt, char* ans_dest, size_t ans_dest_size, char (*pic_paths)[64], const char* model_ver){

    /*
    Call when starting a new chat
    Caution: This function is RAM hungry, needs REQUEST_JSON_MAX_SIZE bytes available in PSRAM (in worst case),
    if pictures are appended as littlefs paths
    */
    if (ans_dest_size < (size_t)(ANSWER_PAGE_LENGTH+1)){
        ESP_LOGE("OpenAI","Ans buffer size has to be at least ANSWER_PAGE_LENGTH");
        return;
    }
    uint8_t model_ver_valid = 0;
    for (size_t i=0; i<sizeof(openai_supported_models); i++){
        if (strcmp(model_ver,openai_supported_models[i]) == 0){
            model_ver_valid = 1;
            break;
        }
    }
    if (!model_ver_valid){
        ESP_LOGE("OpenAI","Invalid model name");
        return;
    }
    if (!openai_events){
        openai_events = xEventGroupCreate();
    }
    xEventGroupClearBits(openai_events,OPENAI_DONE_BIT);
    ans_task_params parameters = {
        .prompt = new_prompt,
        .answer = ans_dest,
        .answer_length = ans_dest_size,
        .start_new_chat = 1,
        .picture_paths = pic_paths,
        .model_version = model_ver
    };
    // It is okay to give the address of parameters to xtaskcreate since we wait for the task to finish
    // with xeventgroupwaitbits (parameters only exists within stack)
    xTaskCreate(openAI_get_answer_task, "openAI",16384,&parameters,3,NULL);
    xEventGroupWaitBits(openai_events,OPENAI_DONE_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
}

void get_openai_response(char* new_prompt, char* ans_dest, size_t ans_dest_size, char (*pic_paths)[64], const char* model_ver){

    /*
    Call when trying to get a response in the previous chat
    Caution: This function is RAM hungry, needs REQUEST_JSON_MAX_SIZE bytes in PSRAM (in worst case),
    if pictures are appended as littlefs paths
    */
    if (ans_dest_size < (size_t)(ANSWER_PAGE_LENGTH+1)){
        ESP_LOGE("OpenAI","Ans buffer size has to be at least ANSWER_PAGE_LENGTH");
        return;
    }
    uint8_t model_ver_valid = 0;
    for (size_t i=0; i<sizeof(openai_supported_models); i++){
        if (strcmp(model_ver,openai_supported_models[i]) == 0){
            model_ver_valid = 1;
            break;
        }
    }
    if (!model_ver_valid){
        ESP_LOGE("OpenAI","Invalid model name");
        return;
    }
    if (!openai_events){
        openai_events = xEventGroupCreate();
    }
    xEventGroupClearBits(openai_events,OPENAI_DONE_BIT);
    ans_task_params parameters = {
        .prompt = new_prompt,
        .answer = ans_dest,
        .answer_length = ans_dest_size,
        .start_new_chat = 0,
        .picture_paths = pic_paths,
        .model_version = model_ver
    };
    // It is okay to give the address of parameters to xtaskcreate since we wait for the task to finish
    // with xeventgroupwaitbits (parameters only exists within stack)
    xTaskCreate(openAI_get_answer_task, "openAI",16384,&parameters,3,NULL);
    xEventGroupWaitBits(openai_events,OPENAI_DONE_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
}

void load_prev_openai_response(uint16_t chat_position, char* ans_dest, size_t ans_dest_size){

    // Load the response ID from littlefs
    if (ans_dest_size < (size_t)(ANSWER_PAGE_LENGTH+1)){
        ESP_LOGE("OpenAI Chat","Ans buffer size has to be at least ANSWER_PAGE_LENGTH");
        return;
    }
    if (chat_position >= MAX_CONVERSATION_LENGTH){
        ESP_LOGE("OpenAI Chat","Invalid Chat pos");
    }
    openAI_chatdata_get_item(ans_dest,chat_position,1);
}

void load_prev_openai_prompt(uint16_t chat_position, char* prompt_dest, size_t prompt_dest_size){

    // Load prompt from littlefs
    if (prompt_dest_size < (size_t)(SCRIBBLE_PAGE_LENGTH+1)){
        ESP_LOGE("OpenAI Chat","Prompt buffer size has to be at least SCRIBBLE_PAGE_LENGTH");
        return;
    }
    if (chat_position >= MAX_CONVERSATION_LENGTH){
        ESP_LOGE("OpenAI Chat","Invalid Chat pos");
    }
    openAI_chatdata_get_item(prompt_dest,chat_position,0);
}