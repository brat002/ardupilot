#ifndef BOARD_STM32V1F4
#define BOARD_STM32V1F4

#include <boards.h>
#include "../../SPIDevice.h"
#include <AP_Common/AP_Common.h>

using namespace REVOMINI;

extern const stm32_pin_info PIN_MAP[BOARD_NR_GPIO_PINS] __FLASH__ = {

    /* Top header */
/*
    const gpio_dev  * const gpio_device;      < Maple pin's GPIO device 
    const timer_dev * const timer_device;     < Pin's timer device, if any. 
    const adc_dev   * const adc_device;       < ADC device, if any. 
    uint8_t gpio_bit;             < Pin's GPIO port bit. 
    uint8_t timer_channel;        < Timer channel, or 0 if none. 
    uint8_t adc_channel;          < Pin ADC channel, or ADCx if none. 
*/

    {&gpiob,   NULL, NULL, 10, NO_CH,     ADCx}, /* D0/PB10  0 USART3_TX/I2C2-SCL */
    {&gpiob,   NULL, NULL,  2, NO_CH,     ADCx}, /* D1/PB2   1*/
    {&gpiob,   NULL, NULL, 12, NO_CH,     ADCx}, /* D2/PB12  2 SDCARD CS pin */
    {&gpiob,   NULL, NULL, 13, NO_CH,     ADCx}, /* D3/PB13  3 SPI2_SCK */
    {&gpiob,&timer12,NULL, 14, TIMER_CH1, ADCx}, /* D4/PB14  4 SPI2_MOSI */
    {&gpiob,&timer12,NULL, 15, TIMER_CH2, ADCx}, /* D5/PB15  5 SPI2_MISO */
    {&gpioc,   NULL,&_adc1, 0, NO_CH,       10}, /* D6/PC0   6 NC */
    {&gpioc,   NULL,&_adc1, 1, NO_CH,       11}, /* D7/PC1   7 AMP */
    {&gpioc,   NULL,&_adc1, 2, NO_CH,       12}, /* D8/PC2   8 VOLT */
    {&gpioc,   NULL,&_adc1, 3, NO_CH,       13}, /* D9/PC3   9 freq sense - resistor to VCC, used asd MAX7456 VSYNC */
    {&gpioc,   NULL,&_adc1, 4, NO_CH,       14}, /* D10/PC4  10 EXTI_MPU6000 */
    {&gpioc,   NULL,&_adc1, 5, NO_CH,       15}, /* D11/PC5  1 Vbat */
    {&gpioc, &timer8,NULL,  6, TIMER_CH1, ADCx}, /* D12/PC6  2 CH3_IN / UART6_TX */
    {&gpioc, &timer8,NULL,  7, TIMER_CH2, ADCx}, /* D13/PC7  3 CH4_IN / UART6_RX */
    {&gpioc, &timer8,NULL,  8, TIMER_CH3, ADCx}, /* D14/PC8  4 CH5_IN / S_scl */
    {&gpioc, &timer8,NULL,  9, TIMER_CH4, ADCx}, /* D15/PC9  5 CH6_IN / S_sda  */
    {&gpioc,   NULL, NULL, 10, NO_CH,     ADCx}, /* D16/PC10 6  */
    {&gpioc,   NULL, NULL, 11, NO_CH,     ADCx}, /* D17/PC11 7  */
    {&gpioc,   NULL, NULL, 12, NO_CH,     ADCx}, /* D18/PC12 8  */
    {&gpioc,   NULL, NULL, 13, NO_CH,     ADCx}, /* D19/PC13 9  */
    {&gpioc,   NULL, NULL, 14, NO_CH,     ADCx}, /* D20/PC14 20  */
    {&gpioc,   NULL, NULL, 15, NO_CH,     ADCx}, /* D21/PC15 1   */
    {&gpioa, &timer1,NULL,  8, TIMER_CH1, ADCx}, /* D22/PA8  2 SERVO6 */
    {&gpioa, &timer1,NULL,  9, TIMER_CH2, ADCx}, /* D23/PA9  3 USART1_TX */
    {&gpioa, &timer1,NULL, 10, TIMER_CH3, ADCx}, /* D24/PA10 4 USART1_RX */
    {&gpiob, &timer4,NULL,  9, TIMER_CH4, ADCx}, /* D25/PB9  5 LED */
    {&gpiod,   NULL, NULL,  2, NO_CH,     ADCx}, /* D26/PD2  6 EXTI_RFM22B / UART5_RX */
    {&gpiod,   NULL, NULL,  3, NO_CH,     ADCx}, /* D27/PD3  7*/
    {&gpiod,   NULL, NULL,  6, NO_CH,     ADCx}, /* D28/PD6  8*/
    {&gpiog,   NULL, NULL, 11, NO_CH,     ADCx}, /* D29/PG11 9*/
    {&gpiog,   NULL, NULL, 12, NO_CH,     ADCx}, /* D30/PG12 30*/
    {&gpiog,   NULL, NULL, 13, NO_CH,     ADCx}, /* D31/PG13 1*/
    {&gpiog,   NULL, NULL, 14, NO_CH,     ADCx}, /* D32/PG14 2*/
    {&gpiog,   NULL, NULL,  8, NO_CH,     ADCx}, /* D33/PG8  3*/
    {&gpiog,   NULL, NULL,  7, NO_CH,     ADCx}, /* D34/PG7  4*/
    {&gpiog,   NULL, NULL,  6, NO_CH,     ADCx}, /* D35/PG6  5*/
    {&gpiob, &timer3,NULL,  5, TIMER_CH2, ADCx}, /* D36/PB5  6 */
    {&gpiob, &timer4,NULL,  6, TIMER_CH1, ADCx}, /* D37/PB6  7 RCD_CS on SPI_3*/
    {&gpiob, &timer4,NULL,  7, TIMER_CH2, ADCx}, /* D38/PB7  8 SD_DET */
    {&gpiof,   NULL,&_adc3, 6, NO_CH,        4}, /* D39/PF6  9*/
    {&gpiof,   NULL,&_adc3, 7, NO_CH,        5}, /* D40/PF7  40*/
    {&gpiof,   NULL,&_adc3, 8, NO_CH,        6}, /* D41/PF8  1*/
    {&gpiof,   NULL,&_adc3, 9, NO_CH,        7}, /* D42/PF9  2*/
    {&gpiof,   NULL,&_adc3,10, NO_CH,        8}, /* D43/PF10 3*/
    {&gpiof,   NULL, NULL, 11, NO_CH,     ADCx}, /* D44/PF11 4*/
    {&gpiob, &timer3,&_adc1,1, TIMER_CH4,    9}, /* D45/PB1  5  SERVO2 */
    {&gpiob, &timer3,&_adc1,0, TIMER_CH3,    8}, /* D46/PB0  6  SERVO1 */
    {&gpioa, &timer2,&_adc1,0, TIMER_CH1,    0}, /* D47/PA0  7  RSSI input */
    {&gpioa, &timer2,&_adc1,1, TIMER_CH2,    1}, /* D48/PA1  8  SERVO5 / UART4_RX */
    {&gpioa, &timer2,&_adc1,2, TIMER_CH3,    2}, /* D49/PA2  9  SERVO4 */
    {&gpioa, &timer2,&_adc1,3, TIMER_CH4,    3}, /* D50/PA3  50 SERVO3 */
    {&gpioa,   NULL, &_adc1,4, NO_CH,        4}, /* D51/PA4  1 CS_MPU6000 */
    {&gpioa,   NULL, &_adc1,5, NO_CH,        5}, /* D52/PA5  2 SPI1_CLK */
    {&gpioa, &timer3,&_adc1,6, TIMER_CH1,    6}, /* D53/PA6  3 SPI1_MISO */
    {&gpioa, &timer3,&_adc1,7, TIMER_CH2,    7}, /* D54/PA7  4 SPI1_MOSI */
    {&gpiof,   NULL, NULL,  0, NO_CH,     ADCx}, /* D55/PF0  5*/
    {&gpiod,   NULL, NULL, 11, NO_CH,     ADCx}, /* D56/PD11 6*/
    {&gpiod, &timer4,NULL, 14, TIMER_CH3, ADCx}, /* D57/PD14 7*/
    {&gpiof,   NULL, NULL,  1, NO_CH,     ADCx}, /* D58/PF1  8*/
    {&gpiod, &timer4,NULL, 12, TIMER_CH1, ADCx}, /* D59/PD12 9*/
    {&gpiod, &timer4,NULL, 15, TIMER_CH4, ADCx}, /* D60/PD15 60*/
    {&gpiof,   NULL, NULL,  2, NO_CH,     ADCx}, /* D61/PF2  1*/
    {&gpiod, &timer4,NULL, 13, TIMER_CH2, ADCx}, /* D62/PD13 2*/
    {&gpiod,   NULL, NULL,  0, NO_CH,     ADCx}, /* D63/PD0  3*/
    {&gpiof,   NULL, NULL,  3, NO_CH,     ADCx}, /* D64/PF3  4*/
    {&gpioe,   NULL, NULL,  3, NO_CH,     ADCx}, /* D65/PE3  5*/
    {&gpiod,   NULL, NULL,  1, NO_CH,     ADCx}, /* D66/PD1  6*/
    {&gpiof,   NULL, NULL,  4, NO_CH,     ADCx}, /* D67/PF4  7*/
    {&gpioe,   NULL, NULL,  4, NO_CH,     ADCx}, /* D68/PE4  8*/
    {&gpioe,   NULL, NULL,  7, NO_CH,     ADCx}, /* D69/PE7  9*/
    {&gpiof,   NULL, NULL,  5, NO_CH,     ADCx}, /* D70/PF5  70*/
    {&gpioe,   NULL, NULL,  5, NO_CH,     ADCx}, /* D71/PE5  1*/
    {&gpioe,   NULL, NULL,  8, NO_CH,     ADCx}, /* D72/PE8  2*/
    {&gpiof,   NULL, NULL, 12, NO_CH,     ADCx}, /* D73/PF12 3*/
    {&gpioe,   NULL, NULL,  6, NO_CH,     ADCx}, /* D74/PE6  4*/
    {&gpioe, &timer1,NULL,  9, TIMER_CH1, ADCx}, /* D75/PE9  */
    {&gpiof,   NULL, NULL, 13, NO_CH,     ADCx}, /* D76/PF13 6*/
    {&gpioe,   NULL, NULL, 10, NO_CH,     ADCx}, /* D77/PE10 7*/
    {&gpiof,   NULL, NULL, 14, NO_CH,     ADCx}, /* D78/PF14 8*/
    {&gpiog,   NULL, NULL,  9, NO_CH,     ADCx}, /* D79/PG9  9*/
    {&gpioe, &timer1,NULL, 11, TIMER_CH2, ADCx}, /* D80/PE11 */
    {&gpiof,   NULL, NULL, 15, NO_CH,     ADCx}, /* D81/PF15 1*/
    {&gpiog,   NULL, NULL, 10, NO_CH,     ADCx}, /* D82/PG10 2*/
    {&gpioe,   NULL, NULL, 12, NO_CH,     ADCx}, /* D83/PE12 3*/
    {&gpiog,   NULL, NULL,  0, NO_CH,     ADCx}, /* D84/PG0  4*/
    {&gpiod,   NULL, NULL,  5, NO_CH,     ADCx}, /* D85/PD5  5*/
    {&gpioe, &timer1,NULL, 13, TIMER_CH3, ADCx}, /* D86/PE13 */
    {&gpiog,   NULL, NULL,  1, NO_CH,     ADCx}, /* D87/PG1  7*/
    {&gpiod,   NULL, NULL,  4, NO_CH,     ADCx}, /* D88/PD4  8*/
    {&gpioe, &timer1,NULL, 14, TIMER_CH4, ADCx}, /* D89/PE14 */
    {&gpiog,   NULL, NULL,  2, NO_CH,     ADCx}, /* D90/PG2  90*/
    {&gpioe,   NULL, NULL,  1, NO_CH,     ADCx}, /* D91/PE1  1*/
    {&gpioe,   NULL, NULL, 15, NO_CH,     ADCx}, /* D92/PE15 2*/
    {&gpiog,   NULL, NULL,  3, NO_CH,     ADCx}, /* D93/PG3  3*/
    {&gpioe,   NULL, NULL,  0, NO_CH,     ADCx}, /* D94/PE0  4*/
    {&gpiod,   NULL, NULL,  8, NO_CH,     ADCx}, /* D95/PD8  5*/
    {&gpiog,   NULL, NULL,  4, NO_CH,     ADCx}, /* D96/PG4  6*/
    {&gpiod,   NULL, NULL,  9, NO_CH,     ADCx}, /* D97/PD9  7*/
    {&gpiog,   NULL, NULL,  5, NO_CH,     ADCx}, /* D98/PG5  8*/
    {&gpiod,   NULL, NULL, 10, NO_CH,     ADCx}, /* D99/PD10 9*/
    {&gpiob,   NULL, NULL, 11, NO_CH,     ADCx}, /* D100/PB11 100 USART3_RX/I2C2-SDA */
    {&gpiob, &timer4,NULL,  8, TIMER_CH3, ADCx}, /* D101/PB8  I2C1_SCL  PPM_IN */
    {&gpioe,   NULL, NULL,  2, NO_CH,     ADCx}, /* D102/PE2 */
    {&gpioa,   NULL, NULL, 15, NO_CH,     ADCx}, /* D103/PA15  */
    {&gpiob,   NULL, NULL,  3, NO_CH,     ADCx}, /* D104/PB3   */
    {&gpiob,   NULL, NULL,  4, NO_CH,     ADCx}, /* D105/PB4   */
    {&gpioa,   NULL, NULL, 13, NO_CH,     ADCx}, /* D106/PA13 SWDIO */
    {&gpioa,   NULL, NULL, 14, NO_CH,     ADCx}, /* D107/PA14 SWCLK */
    {&gpioa,   NULL, NULL, 11, NO_CH,     ADCx}, /* D108/PA11  - USB D- */
    
};


extern const struct TIM_Channel PWM_Channels[] __FLASH__ =   {
//CH1 and CH2 also for PPMSUM / SBUS / DSM
    { // 0 RC_IN1
	.pin         = 50,
    },
    { // 1 RC_IN2
	.pin         = 37,
    },
    { // 2 RC_IN3
	.pin         = -1,
    },
    { // 3 RC_IN4
	.pin         = -1,
    },
    { // 4 RC_IN5
	.pin         = -1,
    },
    { // 5 RC_IN6
	.pin         = -1,
    },
};


/*
     DMA modes:
     
0 - disable
1 - enable on large transfers
2 - enable alaways

*/
extern const SPIDesc spi_device_table[] = {    // different SPI tables per board subtype
//               name            device   bus  mode         cs_pin                 speed_low       speed_high   dma               priority
     { BOARD_INS_MPU60x0_NAME,   _SPI1,   1,  SPI_MODE_0, BOARD_MPU6000_CS_PIN,    SPI_1_125MHZ,   SPI_9MHZ,   SPI_TRANSFER_DMA, DMA_Priority_VeryHigh,   1,          5 }, 
     { BOARD_SDCARD_NAME,        _SPI3,   2,  SPI_MODE_0, 255,                     SPI_1_125MHZ,   SPI_18MHZ,  SPI_TRANSFER_DMA, DMA_Priority_Medium,     0,          0 }, 
     { HAL_BARO_BMP280_NAME,     _SPI3,   3,  SPI_MODE_3, BOARD_BMP280_CS_PIN,     SPI_1_125MHZ,   SPI_9MHZ,   SPI_TRANSFER_DMA, DMA_Priority_High,       1,          1 }, 
     { BOARD_OSD_NAME,           _SPI2,   3,  SPI_MODE_0, BOARD_OSD_CS_PIN,        SPI_1_125MHZ,   SPI_4_5MHZ, SPI_TRANSFER_DMA, DMA_Priority_Low,        2,          2 },
};

extern const uint8_t REVOMINI_SPI_DEVICE_NUM_DEVICES = ARRAY_SIZE(spi_device_table);

void boardInit(void) {


#ifdef BOARD_HMC5883_DRDY_PIN
    // Init HMC5883 DRDY EXT_INT pin - but it not used by driver
    gpio_set_mode(PIN_MAP[BOARD_HMC5883_DRDY_PIN].gpio_device, PIN_MAP[BOARD_HMC5883_DRDY_PIN].gpio_bit, GPIO_INPUT_PU);
#endif

#ifdef BOARD_MPU6000_DRDY_PIN
    // Init MPU6000 DRDY pin - but it not used by driver
    gpio_set_mode(PIN_MAP[BOARD_MPU6000_DRDY_PIN].gpio_device, PIN_MAP[BOARD_MPU6000_DRDY_PIN].gpio_bit, GPIO_INPUT_PU);
#endif

#ifdef BOARD_SBUS_INVERTER
// it is not necessary because of 10K resistor to ground
    gpio_set_mode( PIN_MAP[BOARD_SBUS_INVERTER].gpio_device, PIN_MAP[BOARD_SBUS_INVERTER].gpio_bit, GPIO_OUTPUT_PP);
    gpio_write_bit(PIN_MAP[BOARD_SBUS_INVERTER].gpio_device, PIN_MAP[BOARD_SBUS_INVERTER].gpio_bit, 0); // not inverted
#endif

}


#endif