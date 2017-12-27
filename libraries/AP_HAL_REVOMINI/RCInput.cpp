#include <AP_HAL/HAL.h>
#include <AP_Param_Helper/AP_Param_Helper.h>

#include <exti.h>
#include <timer.h>
#include "RCInput.h"
#include <pwm_in.h>
#include <AP_HAL/utility/dsm.h>
#include "sbus.h"
#include "GPIO.h"
#include "ring_buffer_pulse.h"

#include "RC_PPM_parser.h"

#ifdef BOARD_SPEKTRUM_RX_PIN
 #include "RC_DSM_parser.h"
#endif
#ifdef BOARD_NRF_CS_PIN
 #include "RC_NRF_parser.h"
#endif
#ifdef BOARD_SBUS_UART
 #include "RC_SBUS_parser.h"
#endif


// Constructors ////////////////////////////////////////////////////////////////
using namespace AP_HAL;
using namespace REVOMINI;


/*
    DSM satellite connection
        1   2   3   4
pins    *   *   *   *   *   *   *
use    gnd vcc 26  103 xxx xxx xxx
DSM    GND     rx  en

*/


extern const AP_HAL::HAL& hal;
/*
input_channels:
    4,  // PB14 T12/1 - PPM
    5,  // PB15 T12/2 - PPM2
    12, // PC6  T8/1  - 6_tx 
    13, // PC7  T8/2  - 6_rx 
    14, // PC8  T8/3  - Soft_scl / soft_TX
    15, // PC9  T8/4  - Soft_sda / soft_RX
*/

_parser * REVOMINIRCInput::parsers[MAX_RC_PARSERS] IN_CCM;  // individual parsers on each PPM pin and DSM/SBUS USART
uint8_t   REVOMINIRCInput::num_parsers IN_CCM;


uint8_t   REVOMINIRCInput::_valid_channels IN_CCM; //  = 0;
uint64_t  REVOMINIRCInput::_last_read IN_CCM; // = 0;


uint16_t REVOMINIRCInput::_override[REVOMINI_RC_INPUT_NUM_CHANNELS] IN_CCM;
bool REVOMINIRCInput::_override_valid;

bool REVOMINIRCInput::is_PPM IN_CCM;

uint8_t REVOMINIRCInput::_last_read_from IN_CCM;

uint16_t REVOMINIRCInput::max_num_pulses IN_CCM;


bool REVOMINIRCInput::rc_failsafe_enabled IN_CCM;

/* constrain captured pulse to be between min and max pulsewidth. */
static inline uint16_t constrain_pulse(uint16_t p) {
    if (p > RC_INPUT_MAX_PULSEWIDTH) return RC_INPUT_MAX_PULSEWIDTH;
    if (p < RC_INPUT_MIN_PULSEWIDTH) return RC_INPUT_MIN_PULSEWIDTH;
    return p;
}




REVOMINIRCInput::REVOMINIRCInput()
{   }

void REVOMINIRCInput::init() {
    caddr_t ptr;

    memset((void *)&_override[0],   0, sizeof(_override));

/* OPLINK AIR port pinout
1       2       3       4       5       6       7
               PD2     PA15                
gnd    +5      26      103                     
used as
for DSM:       rx      pow
for RFM        int     cs

*/

    is_PPM=true;
    _last_read_from=0;
    max_num_pulses=0;

    clear_overrides();

    pwmInit(is_PPM); // PPM sum mode

    uint8_t pp=0;

    ptr = sbrk_ccm(sizeof(PPM_parser)); // allocate memory in CCM
    parsers[pp++] = new(ptr) PPM_parser;
    
    ptr = sbrk_ccm(sizeof(PPM_parser)); // allocate memory in CCM
    parsers[pp++] = new(ptr) PPM_parser;

#ifdef BOARD_SPEKTRUM_RX_PIN
    ptr = sbrk_ccm(sizeof(DSM_parser)); // allocate memory in CCM
    parsers[pp++] =new(ptr) DSM_parser; 
#endif
#ifdef BOARD_NRF_CS_PIN
    ptr = sbrk_ccm(sizeof(NRF_parser)); // allocate memory in CCM
    parsers[pp++] =new(ptr) NRF_parser;
#endif
#ifdef BOARD_SBUS_UART
    ptr = sbrk_ccm(sizeof(SBUS_parser)); // allocate memory in CCM
    parsers[pp++] =new(ptr) SBUS_parser;
#endif

    num_parsers = pp; // counter

    for(uint8_t i=0; i<num_parsers;i++) {
        parsers[i]->init(i);
    }

}


void REVOMINIRCInput::late_init(uint8_t b) {

    for(uint8_t i=0; i<num_parsers;i++) {
        parsers[i]->late_init(b);
    }
    
    if(b & BOARD_RC_FAILSAFE) rc_failsafe_enabled=true;
}

// we can have 4 individual sources of data - internal DSM from UART5, SBUS from UART1 and 2 PPM parsers
bool REVOMINIRCInput::new_input()
{
    if(_override_valid) return true;

    uint8_t inp=hal_param_helper->_rc_input;
    if(inp &&  inp < num_parsers+1){
        inp-=1;
        
        return parsers[inp]->get_last_signal() >_last_read;
    }

    for(uint8_t i=0; i<num_parsers;i++) {
        if(parsers[i]->get_last_signal() >_last_read) return true;
    }
    
    return false;
}


uint8_t REVOMINIRCInput::num_channels()
{
    return _valid_channels;
}


uint16_t REVOMINIRCInput::last_4=0;

//#define LOST_TIME 50 // this is wrong! Any packet lost and viola... 
#define LOST_TIME 500
#define FRAME_TIME 50 // time between packets

uint16_t REVOMINIRCInput::_read_ppm(uint8_t ch,uint8_t n){
    const _parser *p = parsers[n];
    _valid_channels = p->get_valid_channels();
    return            p->get_val(ch);
}



uint16_t REVOMINIRCInput::read(uint8_t ch)
{
    uint16_t data=0;
    uint64_t pulse=0;
    uint64_t last=0;

    if(ch>=REVOMINI_RC_INPUT_NUM_CHANNELS) return 0;

    uint64_t now=systick_uptime();
    uint8_t got=0;


    uint8_t inp=hal_param_helper->_rc_input;
    if(inp &&  inp < num_parsers+1 ){
        inp-=1;

        const _parser *p = parsers[inp];
        pulse           = p->get_last_signal();
        last            = p->get_last_change();
        data            = p->get_val(ch);
        _valid_channels = p->get_valid_channels();
        got = inp+1;

    } else if(now - _last_read > FRAME_TIME) { // seems that we loose 1 frame on current RC receiver so should select new one
        uint32_t best_t=(uint32_t) -1;
        
        for(uint8_t i=0; i<num_parsers;i++) {
            
            const _parser *p = parsers[i];
            pulse = p->get_last_signal();
            last  = p->get_last_change();
            
            uint32_t dt = now-pulse; // time from signal
            
            if( pulse >_last_read &&  // data is newer than last
                dt<best_t &&          // and most recent
                ((now - last ) < RC_DEAD_TIME || !rc_failsafe_enabled)) // and time from last change less than RC_DEAD_TIME
            {
                best_t = dt;
                data            = p->get_val(ch);
                _valid_channels = p->get_valid_channels();                
                got = i+1;
            }
        }
        // now we have a most recent data
    }
    
    if(got)  {
        _last_read_from = got;
        _last_read      = pulse;
    } else {
        if(_last_read_from) {   //      read from the last channel
            uint8_t n = _last_read_from-1;
            const _parser *p = parsers[n];
            pulse           = p->get_last_signal();
            last            = p->get_last_change();
            data            = p->get_val(ch);
            _valid_channels = p->get_valid_channels();
            _last_read = pulse;
        } else { // no data at all

            if( ch == 2) data = 900;
            else         data = 1000;
        }
    }

    /* Check for override */
    uint16_t over = _override[ch];
    if(over) return over;

    if( ch == 4) {
        last_4 = data;
    }

    if( ch == 2 && rc_failsafe_enabled) { // throttle
        if( (now-pulse) > LOST_TIME ||   // last pulse is very old
            (now-last)  > RC_DEAD_TIME ) // last change is very old
        {
            data = 900;
        }

        if(hal_param_helper->_aibao_fs) {
/*
 Receiver-DEVO-RX719-for-Walkera-Aibao
 failsafe: mode below 1000 and throttle at 1500
*/            
            if(last_4 < 1000 && data >1300)
                data = 900;
        }

    }
    return data;
}

uint8_t REVOMINIRCInput::read(uint16_t* periods, uint8_t len)
{

    for (uint8_t i = 0; i < len; i++){
        periods[i] = read(i);
    }

    return _valid_channels;
}


bool REVOMINIRCInput::set_overrides(int16_t *overrides, uint8_t len)
{
    bool res = false;
    for (int i = 0; i < len; i++) {
        res |= set_override(i, overrides[i]);
    }
    return res;
}

bool REVOMINIRCInput::set_override(uint8_t channel, int16_t override)
{
    if (override < 0) return false; /* -1: no change. */
    if (channel < REVOMINI_RC_INPUT_NUM_CHANNELS) {
        _override[channel] = override;
        if (override != 0) {
    	    _override_valid = true;
            return true;
        }
    }
    return false;
}

void REVOMINIRCInput::clear_overrides()
{
    for (int i = 0; i < REVOMINI_RC_INPUT_NUM_CHANNELS; i++) {
	set_override(i, 0);
    }
}

bool REVOMINIRCInput::rc_bind(int dsmMode){
#ifdef BOARD_SPEKTRUM_RX_PIN
    return parsers[2]->bind(dsmMode); // only DSM
#else
    return false;
#endif

}

