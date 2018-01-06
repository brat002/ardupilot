/*
 * UARTDriver.cpp --- AP_HAL_REVOMINI UART driver.
 
 Based on:
 
 * UART driver
 * Copyright (C) 2013, Virtualrobotix.com Roberto Navoni , Emile 
 * All Rights Reserved.
 *
 * This software is released under the "BSD3" license.  Read the file
 * "LICENSE" for more information.
 */

#pragma GCC optimize ("O2")

#include <AP_HAL/AP_HAL.h>

#if CONFIG_HAL_BOARD == HAL_BOARD_REVOMINI
#include "UARTDriver.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <usb.h>
#include <usart.h>
#include <gpio_hal.h>
#include <AP_Param_Helper/AP_Param_Helper.h>

using namespace REVOMINI;

REVOMINIUARTDriver::REVOMINIUARTDriver(const struct usart_dev *usart):
    _usart_device(usart),
    _initialized(false),
    _blocking(true)
{
}

//uint8_t mode = (UART_Parity_No <<16) | UART_Stop_Bits_1
void REVOMINIUARTDriver::begin(uint32_t baud, uint32_t bmode) {

    if(!_usart_device) return;

#ifdef BOARD_SBUS_UART
    if(_initialized &&  hal_param_helper->_uart_sbus && _usart_device==UARTS[hal_param_helper->_uart_sbus]) return; //already used as SBUS
//    if(usart_is_used(_usart_device)) return; it is NORMAL to Ardupilot to open one USART many times
#endif

    uint32_t mode=0;

    if(_usart_device->tx_pin < BOARD_NR_GPIO_PINS){
        const stm32_pin_info *txi = &PIN_MAP[_usart_device->tx_pin];
        gpio_set_af_mode(txi->gpio_device, txi->gpio_bit, _usart_device->gpio_af);
        gpio_set_mode(txi->gpio_device, txi->gpio_bit, GPIO_AF_OUTPUT_PP);
        mode |= USART_Mode_Tx;
    } 
	
    if(_usart_device->rx_pin < BOARD_NR_GPIO_PINS){
	const stm32_pin_info *rxi = &PIN_MAP[_usart_device->rx_pin];
        gpio_set_af_mode(rxi->gpio_device, rxi->gpio_bit, _usart_device->gpio_af);
        gpio_set_mode(rxi->gpio_device, rxi->gpio_bit, GPIO_AF_OUTPUT_OD_PU); 
        mode |= USART_Mode_Rx;
    }

    if(!mode) return;

    usart_disable(_usart_device);
        
    usart_init(_usart_device);
    usart_setup(_usart_device, (uint32_t)baud, 
                USART_WordLength_8b, bmode & 0xffff /*USART_StopBits_1*/ , (bmode>>16) & 0xffff /* USART_Parity_No*/, mode, USART_HardwareFlowControl_None);
    usart_enable(_usart_device);

    _initialized = true;
}


void REVOMINIUARTDriver::flush() {
    if (!_initialized) {
        return;
    }
    usart_reset_rx(_usart_device);
    usart_reset_tx(_usart_device);
}


uint32_t REVOMINIUARTDriver::available() {
    if (!_initialized) {
        return 0;
    }

    uint16_t v=usart_data_available(_usart_device); 
    return v;
}

int16_t REVOMINIUARTDriver::read() {
    if (available() == 0) {
        return -1;
    }
    return usart_getc(_usart_device);
}

size_t REVOMINIUARTDriver::write(uint8_t c) {

    if (!_initialized) { 
        return 0;
    }

    uint16_t n;
    uint16_t tr=3; // попытки
    while(tr) {
        n = usart_putc(_usart_device, c);
        if(n==0) { // no place for character
            hal_yield(0);
            if(!_blocking) tr--; // при неблокированном выводе уменьшим счетчик попыток
        } else break; // успешно отправили
    } 
    return n;
}

size_t REVOMINIUARTDriver::write(const uint8_t *buffer, size_t size)
{
    uint16_t tr=3; // попыток
    uint16_t n;
    uint16_t sent=0;
    while(tr && size) {

        n = usart_tx(_usart_device, buffer, size);
        if(n<size) { // no place for character
            hal_yield(0);
            if(!_blocking) tr--; // при неблокированном выводе уменьшим счетчик попыток
        } else break; // успешно отправили
        buffer+=n;
        sent+=n;
        size-=n;
    }
    return sent;
}

#endif // CONFIG_HAL_BOARD
