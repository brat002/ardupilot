#pragma once 

//#pragma GCC optimize ("O2")

#include <AP_HAL/HAL.h>

#include "RC_parser.h"
#include "RCInput.h"


class REVOMINI::SBUS_parser : public REVOMINI::_parser {
public:
    SBUS_parser() {}

    void init(uint8_t ch);
    void late_init(uint8_t ch);
    
private:

    static REVOMINIUARTDriver *uartSDriver; 

    void add_uart_input(); // add some input bytes, for SBUS over a serial port
    void _io_completion();
    
    uint8_t _ioc;
    
// state of add_sbus_input
    struct SBUS {
        uint8_t frame[26];
        uint8_t partial_frame_count;
        uint32_t last_input_uS;
    } sbus;
};

