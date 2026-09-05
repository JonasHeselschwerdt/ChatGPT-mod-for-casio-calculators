/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

keypad.h: Settings for TCA8418 implementation

*/

#ifndef KEYPAD_H
#define KEYPAD_H

// Includes






// I2C Address:

#define TCA8418_I2C_ADDR 0x34





// Keypad Rows and Columns (Bitmasks for TCA8418 registers)

#define ROW0 0x1
#define ROW1 0x2
#define ROW2 0x4
#define ROW3 0x8
#define ROW4 0x10
#define ROW5 0x20
#define ROW6 0x40
#define ROW7 0x80

#define COL0 0x1
#define COL1 0x2
#define COL2 0x4
#define COL3 0x8
#define COL4 0x10
#define COL5 0x20
#define COL6 0x40
#define COL7 0x80
#define COL8 0x100
#define COL9 0x200






// TCA8418 Registers

#define TCA_CFG_REG 0x01
#define TCA_KP_GPIO1_REG 0x1D
#define TCA_KP_GPIO2_REG 0x1E
#define TCA_KP_GPIO3_REG 0x1F
#define TCA_KEY_LCK_EC_REG 0x03
#define TCA_INT_STAT_REG 0x02
#define TCA_KEY_EVENT_A_REG 0x04
#define TCA_DATA_STAT1_REG 0x14
#define TCA_DATA_STAT2_REG 0x15
#define TCA_DATA_STAT3_REG 0x16
#define TCA_DATA_OUT1_REG 0x17
#define TCA_DATA_OUT2_REG 0x18
#define TCA_DATA_OUT3_REG 0x19
#define TCA_GPIO_DIR1_REG 0x23
#define TCA_GPIO_DIR2_REG 0x24
#define TCA_GPIO_DIR3_REG 0x25
#define TCA_GPIO_PULL1_REG 0x2C
#define TCA_GPIO_PULL2_REG 0x2D
#define TCA_GPIO_PULL3_REG 0x2E






// TCA8418 GPIOs

// Bit 15 of gpio_bitmask = 0 -> Bits 0:7 = Row 0:7 bitmask
// Bit 15 of gpio_bitmask = 1 -> Bits 0:9 = Column 0:9 bitmask

#define BMS_ALRT 0x8080     // or FREEGPIO_3
#define BMS_PG 0x8100       // or FREEGPIO_2
#define BMS_STAT1 0x8200    // or FREEGPIO_1
#define BMS_STAT2 0x0080    // or FREEGPIO_0





// Tca8418 GPIO Typedef

typedef struct{
    uint16_t gpio_bitmask;
    uint8_t gpio_mode;
    uint8_t gpio_pullup_en;
} tca_gpio_TypeDef;

#define TCA_GPIO_OUTPUT 1
#define TCA_GPIO_INPUT 0
#define TCA_GPIO_PULLUP_DIS 1
#define TCA_GPIO_PULLUP_EN 0





// Original Keypad Layout: Original Key Name to TCA8418 KEY_EVENT_REG code
// Bit 7 always set to 0 (not part of KEY_EVENT_REG code) 

#define KEY_SHIFT 0x15
#define KEY_ALPHA 0x01
#define KEY_ARROW_UP 0x1F
#define KEY_MENU 0x3D
#define KEY_OPTN 0x16
#define KEY_CALC 0x02
#define KEY_ARROW_LEFT 0x0B
#define KEY_ARROW_DOWN 0x29
#define KEY_ARROW_RIGHT 0x33
#define KEY_SUM 0x2A
#define KEY_X 0x34
#define KEY_FRACTURE 0x17
#define KEY_SQRT 0x03
#define KEY_XSQUARED 0x0C
#define KEY_XTOTHEPOWEROF 0x20
#define KEY_LOG 0x2B
#define KEY_LN 0x3E
#define KEY_DOUBLEBRACKETS 0x18
#define KEY_DEGREE 0x04
#define KEY_XINVERSE 0x0D
#define KEY_SIN 0x21
#define KEY_COS 0x35
#define KEY_TAN 0x3F
#define KEY_STO 0x1A
#define KEY_ENG 0x0E
#define KEY_LEFTBRACKET 0x22
#define KEY_RIGHTBRACKET 0x2C
#define KEY_SD 0x36
#define KEY_MPLUS 0x40
#define KEY_7 0x06
#define KEY_8 0x10
#define KEY_9 0x24
#define KEY_DEL 0x2E
#define KEY_AC 0x38
#define KEY_4 0x19
#define KEY_5 0x0F
#define KEY_6 0x23
#define KEY_MULTIPLY 0x37
#define KEY_DIVIDE 0x42
#define KEY_1 0x1B
#define KEY_2 0x05
#define KEY_3 0x25
#define KEY_PLUS 0x2D
#define KEY_MINUS 0x41
#define KEY_0 0x07
#define KEY_COMMA 0x11
#define KEY_POWEROFTEN 0x2F
#define KEY_ANS 0x39
#define KEY_EQUALS 0x43






// Original Calculator Key to altered Keypad Layout translation

typedef enum{
    // All possible special functions on the altered keypad 
    KEY_NOT_DEFINED,
    KEY_NO_SPECIAL_FUNC,
    KEY_SHIFT_SPECIAL_FUNC,
    KEY_ALT_SPECIAL_FUNC,
    KEY_UP_SPECIAL_FUNC,
    KEY_DOWN_SPECIAL_FUNC,
    KEY_RIGHT_SPECIAL_FUNC,
    KEY_LEFT_SPECIAL_FUNC,
    KEY_MENU_SPECIAL_FUNC,
    KEY_BACK_SPECIAL_FUNC,
    KEY_ENTER_SPECIAL_FUNC,
    KEY_CAMERA_SPECIAL_FUNC
} KeySpecialFunc_TypeDef;

typedef struct{
    // Properties of an altered key
    KeySpecialFunc_TypeDef special_function;
    char normal_meaning;
    char shift_meaning;
    char alt_meaning;
    char calculatormode_meaning;
    uint8_t USB_meaning;
    uint64_t press_timestamp;
} Key_TypeDef;





// Extern variables

extern Key_TypeDef no_key;





// Exported functions

// Init fucntions
void tca8418_init_keypad(void);
void tca8418_init_gpios(void);
// GPIO control
uint8_t tca8418_gpi_get_level(uint16_t gpi_bitmask);
void tca8418_gpo_set_level(uint16_t gpo_bitmask, uint8_t output_level);
// Keypad control
void update_pressed_keys(Key_TypeDef* cur_pressed_keys);




#endif