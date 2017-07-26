#ifndef _BOARD_STM32V1F4_H_
#define _BOARD_STM32V1F4_H_


void boardInit(void);

/**
 * @brief Configuration of the Cortex-M4 Processor and Core Peripherals
 */
#define __CM4_REV                 0x0001  /*!< Core revision r0p1                            */
#define __MPU_PRESENT             1       /*!< STM32F4XX provides an MPU                     */
#define __NVIC_PRIO_BITS          4       /*!< STM32F4XX uses 4 Bits for the Priority Levels */
#define __Vendor_SysTickConfig    0       /*!< Set to 1 if different SysTick Config is used  */
#define __FPU_PRESENT             1       /*!< FPU present      */

#define CYCLES_PER_MICROSECOND  168
#define SYSTICK_RELOAD_VAL      (CYCLES_PER_MICROSECOND*1000-1)

#undef  STM32_PCLK1
#undef  STM32_PCLK2
#define STM32_PCLK1   (CYCLES_PER_MICROSECOND*1000000/4)
#define STM32_PCLK2   (CYCLES_PER_MICROSECOND*1000000/2)

#define BOARD_BUTTON_PIN     254

//#define BOARD_BUZZER_PIN        5 // PB15, PWM2 - used as PPM2

#define BOARD_NR_USARTS         5
#define BOARD_USART1_TX_PIN     23 
#define BOARD_USART1_RX_PIN     24 
#define BOARD_USART3_TX_PIN     0
#define BOARD_USART3_RX_PIN     100
#define BOARD_USART6_TX_PIN     12
#define BOARD_USART6_RX_PIN     13

//#define BOARD_USART4_RX_PIN     48
//#define BOARD_USART4_TX_PIN     47

//#define BOARD_USART5_RX_PIN     26  // PD2  EXTI_RFM22B / UART5_RX
//#define BOARD_BUTTON_PIN        103 // PA15 CS_RFM22B


#define BOARD_DSM_USART (_USART1)
   
#define BOARD_NR_SPI            3
#define BOARD_SPI1_SCK_PIN      52
#define BOARD_SPI1_MISO_PIN     53
#define BOARD_SPI1_MOSI_PIN     54
#define BOARD_SPI2_SCK_PIN      3 // PB13
#define BOARD_SPI2_MISO_PIN     4 // PB14
#define BOARD_SPI2_MOSI_PIN     5 // PB15
#define BOARD_SPI3_MOSI_PIN     18
#define BOARD_SPI3_MISO_PIN     17
#define BOARD_SPI3_SCK_PIN      16



#define BOARD_MPU6000_CS_PIN	51
#define BOARD_MPU6000_DRDY_PIN	10  // PC4


//#define BOARD_SBUS_INVERTER     6

#define BOARD_USB_SENSE 11      // PC5


// bus 2 (soft) pins
#define BOARD_SOFT_SCL 14
#define BOARD_SOFT_SDA 15

// SoftSerial pins
//#define BOARD_SOFTSERIAL_TX 14
//#define BOARD_SOFTSERIAL_RX 15


# define BOARD_GPIO_A_LED_PIN        36  // BLUE
# define BOARD_GPIO_B_LED_PIN        9      //  frequency select - resistor to VCC or ground
# define BOARD_GPIO_C_LED_PIN        105 // RED

# define BOARD_LED_ON           LOW
# define BOARD_LED_OFF          HIGH


#define BOARD_NR_GPIO_PINS      109


#define BOARD_I2C_BUS_INT 1  // hardware internal I2C
//#define BOARD_I2C_BUS_EXT 1  // external I2C
#define BOARD_I2C_BUS_SLOW 1 // slow down bus with this number


#define BOARD_BARO_DEFAULT HAL_BARO_BMP280_SPI
#define HAL_BARO_BMP280_NAME "bmp280"
#define BOARD_BMP280_CS_PIN 104

#define BOARD_COMPASS_DEFAULT HAL_COMPASS_HMC5843
#define BOARD_COMPASS_HMC5843_I2C_ADDR 0x1E
#define BOARD_COMPASS_HMC5843_ROTATION ROTATION_NONE

#define HAL_COMPASS_HMC5843_I2C_BUS     BOARD_I2C_BUS_INT
#define HAL_COMPASS_HMC5843_I2C_ADDR    BOARD_COMPASS_HMC5843_I2C_ADDR
#define HAL_COMPASS_HMC5843_ROTATION    BOARD_COMPASS_HMC5843_ROTATION


#define BOARD_INS_DEFAULT HAL_INS_MPU60XX_SPI
#define BOARD_INS_ROTATION  ROTATION_YAW_180
#define BOARD_INS_MPU60x0_NAME            "mpu6000"

#define BOARD_STORAGE_SIZE            8192 // 4096 // EEPROM size


#define BOARD_SDCARD_NAME "sdcard"
#define BOARD_SDCARD_CS_PIN   2
#define BOARD_SDCARD_DET_PIN   38 // PB7

#define HAL_BOARD_LOG_DIRECTORY "0:/APM/LOGS"
#define HAL_BOARD_TERRAIN_DIRECTORY "0:/APM/TERRAIN"
//#define HAL_PARAM_DEFAULTS_PATH "0:/APM/defaults.parm"

#define BOARD_OSD_NAME "osd"
#define BOARD_OSD_CS_PIN   103
#define BOARD_OSD_VSYNC_PIN   9 // PC3, Frequency input

#define BOARD_OWN_NAME "AirbotV2"

# define BOARD_PUSHBUTTON_PIN   254
# define BOARD_USB_MUX_PIN      -1
# define BOARD_BATTERY_VOLT_PIN     8   // Battery voltage on A0 (PC2) D8
# define BOARD_BATTERY_CURR_PIN     7   // Battery current on A1 (PC1) D7
# define BOARD_SONAR_SOURCE_ANALOG_PIN 254

#define BOARD_USB_DMINUS 108

#define BOARD_SBUS_UART1 1 // can use UART1 as hardware inverted input

#define USE_SERIAL_4WAY_BLHELI_INTERFACE

// use soft I2C driver instead hardware
//#define BOARD_SOFT_I2C


#define SERVO_PIN_1 46 // PB0 
#define SERVO_PIN_2 45 // PB1
#define SERVO_PIN_3 50 // PA3
#define SERVO_PIN_4 49 // PA2
#define SERVO_PIN_5 48 // PA1
#define SERVO_PIN_6 22 // PA8

/*
     DMA modes:
     
0 - disable
1 - enable on large transfers
2 - enable alaways

*/

   //                                    name            device   bus  mode         cs_pin                 speed_low       speed_high soft   dma
#define BOARD_SPI_DEVICES    { BOARD_INS_MPU60x0_NAME,   _SPI1,   1,  SPI_MODE_3, BOARD_MPU6000_CS_PIN,    SPI_1_125MHZ,   SPI_9MHZ,  true,  0 }, \
                             { HAL_BARO_BMP280_NAME,     _SPI3,   3,  SPI_MODE_3, BOARD_BMP280_CS_PIN,     SPI_1_125MHZ,   SPI_9MHZ,  false, 0 }, \
                             { BOARD_SDCARD_NAME,        _SPI2,   2,  SPI_MODE_0, 255,                     SPI_1_125MHZ,   SPI_18MHZ, false, 2  }, \
                             { BOARD_OSD_NAME,           _SPI3,   3,  SPI_MODE_3, 255,                     SPI_1_125MHZ,   SPI_18MHZ, false, 1  },

/*

    // @Param: MOTOR_LAYOUT
    // @DisplayName: Motor layout scheme
    // @Description: Selects how motors are numbered
    // @Values: 0:ArduCopter, 1: Ardupilot with pins 2&3 for servos 2:OpenPilot,3:CleanFlight
    // @User: Advanced
    AP_GROUPINFO("_MOTOR_LAYOUT", 0,  HAL_REVOMINI, _motor_layout, 0),

    // @Param: USE_SOFTSERIAL
    // @DisplayName: Use SoftwareSerial driver
    // @Description: Use SoftwareSerial driver instead SoftwareI2C on Input Port pins 7 & 8
    // @Values: 0:disabled,1:enabled
    // @User: Advanced
    AP_GROUPINFO("_USE_SOFTSERIAL", 1,  HAL_REVOMINI, _use_softserial, 0),

    // @Param: UART1_SBUS
    // @DisplayName: Use UART1 as SBUS input
    // @Description: Use UART1 as SBUS input, not as MAVlink telemetry
    // @Values: 0:disabled,1:enabled
    // @User: Advanced
    AP_GROUPINFO("UART1_SBUS", 3, AP_Param_Helper, _uart1_sbus, 0), \

    // @Param: SERVO_MASK
    // @DisplayName: Servo Mask of Input port
    // @Description: Enable selected pins of Input port to be used as Servo Out
    // @Values: 0:disabled,1:enable pin3 (PPM_1), 2: enable pin4 (PPM_2), 4: enable pin5 (UART6_TX) , 8: enable pin6 (UART6_RX), 16: enable pin7, 32: enable pin8
    // @User: Advanced
    AP_GROUPINFO("SERVO_MASK", 2, AP_Param_Helper, _servo_mask, 0) 

    // @Param: RC_INPUT
    // @DisplayName: Type of RC input
    // @Description: allows to force specified RC input port
    // @Values: 0:auto, 1:PPM1 (pin3), 2: PPM2 (pin4) etc
    // @User: Advanced
    AP_GROUPINFO("RC_INPUT",     9, AP_Param_Helper, _rc_input, 0)

    // @Param: CONNECT_COM
    // @DisplayName: connect to COM port
    // @Description: Allows to connect USB to arbitrary UART, thus allowing to configure devices on that UARTs. Auto-reset.
    // @Values: 0:disabled, 1:connect to port 1, 2:connect to port 2, etc
    // @User: Advanced
    AP_GROUPINFO("CONNECT_COM", 2, AP_Param_Helper, _connect_com, 0) \

    // @Param: CONNECT_ESC
    // @DisplayName: connect to ESC inputs via 4wayIf
    // @Description: Allows to connect USB to ESC inputs, thus allowing to configure ESC as on 4-wayIf. Auto-reset.
    // @Values: 0:disabled, 1:connect uartA to ESC, 2: connect uartB to ESC, etc
    // @User: Advanced
    AP_GROUPINFO("CONNECT_ESC", 2, AP_Param_Helper, _connect_esc, 0) \

    // @Param: PWM_TYPE
    // @DisplayName: PWM protocol used
    // @Description: Allows to ignore MOT_PWM_TYPE  param and set PWM protocol independently
    // @Values: 0:use MOT_PWM_TYPE, 1:OneShot 2:OneShot125 3:OneShot42 4:PWM125
    // @User: Advanced
    AP_GROUPINFO("PWM_TYPE",     7, AP_Param_Helper, _pwm_type, 0)

    // @Param: USB_STORAGE
    // @DisplayName: allows access to SD card at next reboot
    // @Description: Allows to read/write internal SD card via USB mass-storage protocol. Auto-reset.
    // @Values: 0:normal, 1:work as USB flash drive
    // @User: Advanced
    AP_GROUPINFO("USB_STORAGE",  8, AP_Param_Helper, _usb_storage, 0), \

*/
#define BOARD_HAL_VARINFO \
    AP_GROUPINFO("MOTOR_LAYOUT", 1, AP_Param_Helper, _motor_layout, 0), \
    AP_GROUPINFO("SERVO_MASK",   2, AP_Param_Helper, _servo_mask, 0), \
    AP_GROUPINFO("UART1_SBUS",   3, AP_Param_Helper, _uart1_sbus, 0), \
    AP_GROUPINFO("SOFTSERIAL",   4, AP_Param_Helper, _use_softserial, 0), \
    AP_GROUPINFO("CONNECT_COM",  5, AP_Param_Helper, _connect_com, 0), \
    AP_GROUPINFO("PWM_TYPE",     6, AP_Param_Helper, _pwm_type, 0), \
    AP_GROUPINFO("CONNECT_ESC",  7, AP_Param_Helper, _connect_esc, 0), \
    AP_GROUPINFO("USB_STORAGE",  8, AP_Param_Helper, _usb_storage, 0), \
    AP_GROUPINFO("RC_INPUT",     9, AP_Param_Helper, _rc_input, 0)
    

// parameters
#define BOARD_HAL_PARAMS \
    AP_Int8 _motor_layout; \
    AP_Int8 _use_softserial; \
    AP_Int8 _uart1_sbus; \
    AP_Int8 _servo_mask;   \
    AP_Int8 _connect_com;  \
    AP_Int8 _connect_esc; \
    AP_Int8 _pwm_type; \
    AP_Int8 _rc_input; \
    AP_Int8 _usb_storage; 
    
#endif

#define USB_MASSSTORAGE 

#define HAL_CONSOLE USB_Driver // console on USB
//#define HAL_CONSOLE uart1Driver // console on radio
