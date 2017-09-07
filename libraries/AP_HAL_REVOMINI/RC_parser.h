#pragma once 

#include "Config.h"

class REVOMINI::_parser { // universal parser interface
public:
    _parser() {};
    virtual ~_parser() {};

    virtual void init(uint8_t ch) = 0;
    virtual void late_init(uint8_t b) {}

/*
    virtual uint64_t get_last_signal() const =0;
    virtual uint64_t get_last_change() const =0;
    virtual uint8_t  get_valid_channels() const =0;
    virtual uint16_t get_val(uint8_t ch) const =0;
*/
    virtual uint64_t get_last_signal()    const { noInterrupts(); uint64_t t= _last_signal; interrupts(); return t; }
    virtual uint64_t get_last_change()    const { noInterrupts(); uint64_t t= _last_change; interrupts(); return t; }
    virtual uint8_t  get_valid_channels() const { noInterrupts(); uint8_t  t= _channels;    interrupts(); return t; }
    virtual uint16_t get_val(uint8_t ch)  const { noInterrupts(); uint16_t t= _val[ch];     interrupts(); return t; }

    virtual bool bind(int dsmMode) const  { return true; }

protected:
    volatile uint64_t _last_signal;
    volatile uint16_t _val[REVOMINI_RC_INPUT_NUM_CHANNELS];
    uint64_t          _last_change;
    volatile uint8_t  _channels;

};
