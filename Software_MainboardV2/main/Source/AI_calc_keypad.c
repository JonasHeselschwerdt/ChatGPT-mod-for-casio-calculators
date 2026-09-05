/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

keypad.c: Functions and variables to implement the TCA8418 via I2C

*/

// Includes

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include "AI_calc_main.h"
#include "AI_calc_device.h"
#include "AI_calc_UI.h"
#include "AI_calc_keypad.h"





// Static Keypad variables

static i2c_master_dev_handle_t tca8418_dev = NULL;

static Key_TypeDef Key_LUT[128];     // 128 entries because EVENT_KEY_REG has 7 bit key-event-codes




// Extern variables

// Needed during initialization of cur_pressed_keys in main.c
Key_TypeDef no_key = {
    .special_function = KEY_NOT_DEFINED,
    .normal_meaning = '\0',
    .shift_meaning = '\0',
    .alt_meaning = '\0',
    .calculatormode_meaning = '\0',
    .USB_meaning = '\0',
    .press_timestamp = 0             // ms timestamp
};





// Static Function declarations

static void tca8418_write(uint8_t reg, uint8_t data);
static void tca8418_read(uint8_t reg, uint8_t *data);
static void tca8418_gpio_set_mode(tca_gpio_TypeDef* gpio);
static void initialize_KeyLUT(void);
static void tca8418_get_key_FIFO(uint8_t* keypad_FIFO);
static uint8_t compare_keys(Key_TypeDef *key1, Key_TypeDef *key2);





// TCA8418 related static functions


static void tca8418_write(uint8_t reg, uint8_t data) {

    uint8_t buf[2] = { reg, data };
    ESP_ERROR_CHECK(i2c_master_transmit(tca8418_dev, buf, sizeof(buf), -1));
}

static void tca8418_read(uint8_t reg, uint8_t *data) {

    ESP_ERROR_CHECK(i2c_master_transmit(tca8418_dev, &reg, 1, -1));
    ESP_ERROR_CHECK(i2c_master_receive(tca8418_dev, data, 1, -1));
}

static void tca8418_gpio_set_mode(tca_gpio_TypeDef* gpio){

    if (gpio->gpio_bitmask >> 15){
        // Column Bitmask
        gpio->gpio_bitmask &= 0x7FFF;
        if (gpio->gpio_bitmask > 0x0080){
            gpio->gpio_bitmask >>= 8;
            tca8418_write(TCA_GPIO_DIR3_REG,(gpio->gpio_bitmask & gpio->gpio_mode));
            tca8418_write(TCA_GPIO_PULL3_REG,(gpio->gpio_bitmask & gpio->gpio_pullup_en));
        }
        else{
            tca8418_write(TCA_GPIO_DIR2_REG,(gpio->gpio_bitmask & gpio->gpio_mode));
            tca8418_write(TCA_GPIO_PULL2_REG,(gpio->gpio_bitmask & gpio->gpio_pullup_en));
        }
    }
    else{
        // Row Bitmask
        tca8418_write(TCA_GPIO_DIR1_REG,(gpio->gpio_bitmask & gpio->gpio_mode));
        tca8418_write(TCA_GPIO_PULL1_REG,(gpio->gpio_bitmask & gpio->gpio_pullup_en));
    }
}

static void tca8418_get_key_FIFO(uint8_t* keypad_FIFO){

    // Gets called when an Interrupt occurs
    for(uint8_t i = 0; i < 10; i++){
        keypad_FIFO[i] = 0;
    }
    uint8_t interrupt;
    tca8418_read(TCA_INT_STAT_REG,&interrupt);
    if (interrupt & 0x01){
        // Interrupt source = Keypad
        uint8_t event_counter;
        tca8418_read(TCA_KEY_LCK_EC_REG,&event_counter);
        event_counter &= 0x0F;
        uint8_t key_event;
        for (uint8_t i = 0; i < event_counter && i < 10; i++){
            tca8418_read(TCA_KEY_EVENT_A_REG, &key_event);
            keypad_FIFO[i] = key_event;
        }
        tca8418_write(TCA_INT_STAT_REG,0x01);   // To Clear interrupt
    }
    
}

static void initialize_KeyLUT(void){

    for (uint8_t i = 0; i <= 127; i++){
        Key_LUT[i] = no_key;
    }

    //      Original Key name                       Special function           Normal   Shift     Alt     Calc      USB

    Key_LUT[KEY_SHIFT] =            (Key_TypeDef){  KEY_SHIFT_SPECIAL_FUNC     ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ALPHA] =            (Key_TypeDef){  KEY_ALT_SPECIAL_FUNC       ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ARROW_UP] =         (Key_TypeDef){  KEY_UP_SPECIAL_FUNC        ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_MENU] =             (Key_TypeDef){  KEY_MENU_SPECIAL_FUNC      ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_OPTN] =             (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'a'     ,'A'     ,'@'     ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_CALC] =             (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'b'     ,'B'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ARROW_LEFT] =       (Key_TypeDef){  KEY_LEFT_SPECIAL_FUNC      ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ARROW_DOWN] =       (Key_TypeDef){  KEY_DOWN_SPECIAL_FUNC      ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ARROW_RIGHT] =      (Key_TypeDef){  KEY_RIGHT_SPECIAL_FUNC     ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_SUM] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'c'     ,'C'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_X] =                (Key_TypeDef){  KEY_CAMERA_SPECIAL_FUNC    ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_FRACTURE] =         (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'d'     ,'D'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_SQRT] =             (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'e'     ,'E'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_XSQUARED] =         (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'f'     ,'F'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_XTOTHEPOWEROF] =    (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'g'     ,'G'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_LOG] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'h'     ,'H'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_LN] =               (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'i'     ,'I'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_DOUBLEBRACKETS] =   (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'j'     ,'J'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_DEGREE] =           (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'k'     ,'K'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_XINVERSE] =         (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'l'     ,'L'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_SIN] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'m'     ,'M'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_COS] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'n'     ,'N'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_TAN] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'o'     ,'O'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_STO] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'p'     ,'P'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ENG] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'q'     ,'Q'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_LEFTBRACKET] =      (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'r'     ,'R'     ,'('     ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_RIGHTBRACKET] =     (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'s'     ,'S'     ,')'     ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_SD] =               (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'t'     ,'T'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_MPLUS] =            (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'u'     ,'U'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_7] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'7'     ,'$'     ,'\0'    ,'7'     ,0x00     ,0};
    Key_LUT[KEY_8] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'8'     ,'&'     ,'\0'    ,'8'     ,0x00     ,0};
    Key_LUT[KEY_9] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'9'     ,'%'     ,'\0'    ,'9'     ,0x00     ,0};
    Key_LUT[KEY_DEL] =              (Key_TypeDef){  KEY_BACK_SPECIAL_FUNC      ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_AC] =               (Key_TypeDef){  KEY_ENTER_SPECIAL_FUNC     ,'\0'    ,'\0'    ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_4] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'4'     ,'{'     ,'['     ,'4'     ,0x00     ,0};
    Key_LUT[KEY_5] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'5'     ,'}'     ,']'     ,'5'     ,0x00     ,0};
    Key_LUT[KEY_6] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'6'     ,'#'     ,'\0'    ,'6'     ,0x00     ,0};
    Key_LUT[KEY_MULTIPLY] =         (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'v'     ,'V'     ,'*'     ,'*'     ,0x00     ,0};
    Key_LUT[KEY_DIVIDE] =           (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'w'     ,'W'     ,'/'     ,'/'     ,0x00     ,0};
    Key_LUT[KEY_1] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'1'     ,'^'     ,'\0'    ,'1'     ,0x00     ,0};
    Key_LUT[KEY_2] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'2'     ,'\\'    ,'\0'    ,'2'     ,0x00     ,0};
    Key_LUT[KEY_3] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'3'     ,'\0'    ,'\0'    ,'3'     ,0x00     ,0};
    Key_LUT[KEY_PLUS] =             (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'x'     ,'X'     ,'+'     ,'+'     ,0x00     ,0};
    Key_LUT[KEY_MINUS] =            (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'y'     ,'Y'     ,'-'     ,'-'     ,0x00     ,0};
    Key_LUT[KEY_0] =                (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'0'     ,'\0'    ,'\0'    ,'0'     ,0x00     ,0};
    Key_LUT[KEY_COMMA] =            (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,','     ,';'     ,'\''    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_POWEROFTEN] =       (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,' '     ,'_'     ,'\0'    ,'\0'    ,0x00     ,0};
    Key_LUT[KEY_ANS] =              (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'.'     ,':'     ,'\0'    ,'a'     ,0x00     ,0};
    Key_LUT[KEY_EQUALS] =           (Key_TypeDef){  KEY_NO_SPECIAL_FUNC        ,'z'     ,'Z'     ,'='     ,'='     ,0x00     ,0};

}

static uint8_t compare_keys(Key_TypeDef *key1, Key_TypeDef *key2){

    uint8_t equal = 0;
    if (key1->special_function == key2->special_function){
        if (key1->normal_meaning == key2->normal_meaning){
            if (key1->shift_meaning == key2->shift_meaning){
                if (key1->alt_meaning == key2->alt_meaning){
                    equal = 1;
                }
            }
        }
    }
    return equal;
}






// TCA8418 related exported functions

void tca8418_init_keypad(void){

    uint8_t rows = (0|ROW0|ROW1|ROW2|ROW3|ROW4|ROW5|ROW6);
    uint16_t columns = (0|COL0|COL1|COL2|COL3|COL4|COL5|COL6);

    i2c_device_config_t tca8418_config = {
        .device_address = TCA8418_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus,&tca8418_config,&tca8418_dev));

    gpio_set_level(TCA8418_N_RESET,1);
    vTaskDelay(pdMS_TO_TICKS(10));

    tca8418_write(TCA_CFG_REG,0x01);
    // Keypad Setup
    tca8418_write(TCA_KP_GPIO1_REG,rows);
    tca8418_write(TCA_KP_GPIO2_REG,(uint8_t)columns);
    tca8418_write(TCA_KP_GPIO3_REG,(uint8_t)(columns >> 8));
    // All Pins not configured as Keypad Pins -> Input GPIO
    // Not configured as interrupt source, not part of event FIFO (default behaviour)
    // GPIs read via polling
    tca8418_write(TCA_KEY_LCK_EC_REG,0x00);         // Always unlock Keypad (lock feature not used)

    initialize_KeyLUT();
}

uint8_t tca8418_gpi_get_level(uint16_t gpi_bitmask){

    uint8_t gpi_state;
    if (gpi_bitmask >> 15){
        // Column bitmask
        gpi_bitmask &= 0x7FFF;
        if (gpi_bitmask > 0x0080){
            tca8418_read(TCA_DATA_STAT3_REG,&gpi_state);
            gpi_bitmask >>= 8;
        }
        else{
            tca8418_read(TCA_DATA_STAT2_REG,&gpi_state);
        }
    }
    else{
        // Row Bitmask
        tca8418_read(TCA_DATA_STAT1_REG,&gpi_state);
    }
    return ((gpi_state & gpi_bitmask)!=0);
}

void tca8418_gpo_set_level(uint16_t gpo_bitmask, uint8_t output_level){

    if (gpo_bitmask >> 15){
        // Column bitmask
        gpo_bitmask &= 0x7FFF;
        if (gpo_bitmask > 0x0080){
            tca8418_write(TCA_DATA_OUT3_REG,(gpo_bitmask & output_level));
        }
        else{
            tca8418_write(TCA_DATA_OUT2_REG,(gpo_bitmask & output_level));
        }
    }
    else{
        //Row bitmask
        tca8418_write(TCA_DATA_OUT1_REG,(gpo_bitmask & output_level));
    }
}

void tca8418_init_gpios(void){

    // TCA8418 GPIO Configuration
    tca_gpio_TypeDef bms_alrt_pin = {BMS_ALRT,TCA_GPIO_INPUT,TCA_GPIO_PULLUP_DIS};
    tca_gpio_TypeDef bms_pg_pin = {BMS_PG,TCA_GPIO_INPUT,TCA_GPIO_PULLUP_DIS};
    tca_gpio_TypeDef bms_stat1_pin = {BMS_STAT1,TCA_GPIO_INPUT,TCA_GPIO_PULLUP_DIS};
    tca_gpio_TypeDef bms_stat2_pin = {BMS_STAT2,TCA_GPIO_INPUT,TCA_GPIO_PULLUP_DIS};
    tca8418_gpio_set_mode(&bms_alrt_pin);
    tca8418_gpio_set_mode(&bms_pg_pin);
    tca8418_gpio_set_mode(&bms_stat1_pin);
    tca8418_gpio_set_mode(&bms_stat2_pin);
}

void update_pressed_keys(Key_TypeDef* cur_pressed_keys){

    // Called when TCA8418 Interrupt happens, Updates the global array cur_pressed_keys
    // Adds a timestamp to newly pressed keys, Also sorts cur_pressed_keys 
    // cur_pressed_keys[0] -> oldest pressed key
    uint8_t keypad_FIFO[10] = {0,0,0,0,0,0,0,0,0,0};
    tca8418_get_key_FIFO(keypad_FIFO);
    /*
    ESP_LOGI("Debugging","KeypadFIFO 1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x, 5:0x%x, 6:0x%x, 7:0x%x, 8:0x%x, 9:0x%x, 10:0x%x",
        keypad_FIFO[0],
        keypad_FIFO[1],
        keypad_FIFO[2],
        keypad_FIFO[3],
        keypad_FIFO[4],
        keypad_FIFO[5],
        keypad_FIFO[6],
        keypad_FIFO[7],
        keypad_FIFO[8],
        keypad_FIFO[9]);     // use this debugging message to map the keypad codes
    */
    uint8_t FIFO_pressed_keys[10] = {0,0,0,0,0,0,0,0,0,0};
    uint8_t FIFO_released_keys[10] = {0,0,0,0,0,0,0,0,0,0};
    uint8_t list_pos = 0;
    for(uint8_t i = 0; i<10; i++){
        if (!(keypad_FIFO[i] & 0x80)){
            FIFO_released_keys[list_pos] = keypad_FIFO[i];
            list_pos++;
        }
        else if(keypad_FIFO[i] == 0){
            break;
        }
    }
    list_pos = 0;
    for(uint8_t i = 0; i<10; i++){
        if (keypad_FIFO[i] & 0x80){
            FIFO_pressed_keys[list_pos] = (keypad_FIFO[i] & 0x7F);  // reset Bit 7
            list_pos++;
        }
        else if(keypad_FIFO[i] == 0){
            break;
        }
    }
    // Add pressed keys into cur_pressed_keys, add timestamp
    uint64_t press_time_ms = esp_timer_get_time() / 1000;
    for (uint8_t i=0; i<10;i++){
        if (FIFO_pressed_keys[i] != 0){
            for (int z=0; z<10; z++){
                if (compare_keys(&cur_pressed_keys[z],&no_key)){
                    cur_pressed_keys[z] = Key_LUT[FIFO_pressed_keys[i]];
                    cur_pressed_keys[z].press_timestamp = press_time_ms;
                    break;
                }
            }
        }
        else{
            // End of pressed key list
            break;
        }
    }
    // Delete released keys from cur_pressed_keys, close gap
    for (uint8_t i=0; i<10; i++){
        if (FIFO_released_keys[i] != 0){
            Key_TypeDef released = Key_LUT[FIFO_released_keys[i]];
            for (uint8_t z=0; z<10;z++){
                // Search released key in cur_pressed_keys
                if (compare_keys(&cur_pressed_keys[z],&released)){
                    // Remove from array, shift keys to close the gap
                    for (uint8_t n=0; n < (9-z); n++){
                        cur_pressed_keys[z+n] = cur_pressed_keys[z+n+1];
                    }
                    cur_pressed_keys[9] = no_key;
                    break;
                }
            }
        }
        else{
            // End of released key list
            break;
        }
    }
}

