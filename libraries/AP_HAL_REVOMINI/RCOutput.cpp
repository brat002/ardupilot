
#pragma GCC optimize ("O2")

#include "AP_HAL_REVOMINI.h"
#include "RCOutput.h"
#include <AP_Param_Helper/AP_Param_Helper.h>
#include <4way/serial_4way.h>

#include "GPIO.h"

using namespace REVOMINI;


// only one!
//#define  DEBUG_PWM 5 // motor 6
//#define DEBUG_INT 5


#define REVOMINI_OUT_CHANNELS 6 // motor's channels enabled by default

// #if FRAME_CONFIG == QUAD_FRAME // this is only QUAD layouts

// ArduCopter
static const uint8_t output_channels_arducopter[]= {  // pin assignment
    SERVO_PIN_1, //Timer3/3  - 1
    SERVO_PIN_2, //Timer3/4  - 2
    SERVO_PIN_3, //Timer2/3  - 3
    SERVO_PIN_4, //Timer2/2  - 4
    SERVO_PIN_5, //Timer2/1
    SERVO_PIN_6, //Timer2/0

    // servo channels on input port
    4, // PB14  CH1_IN - PPM1 Use this channels asd servo only if you use DSM via UART as RC
    5, // PB15  CH2_IN - PPM2 
    12, // PC6  CH3_IN UART6
    13, // PC7  CH4_IN UART6
    14, // PC8  CH5_IN i2c
    15, // PC9  CH6_IN i2c
};


// mimics foreign layouts

// OpenPilot
static const uint8_t output_channels_openpilot[]= {  // pin assignment
    SERVO_PIN_2, //Timer3/4  - 2
    SERVO_PIN_4, //Timer2/2  - 4
    SERVO_PIN_1, //Timer3/3  - 1
    SERVO_PIN_3, //Timer2/3  - 3
    SERVO_PIN_5, //Timer2/1
    SERVO_PIN_6, //Timer2/0

    // servo channels on input port
//    4, // PB14  CH1_IN - PPM1 Use this channels asd servo only if you use DSM via UART as RC
    5, // PB15  CH2_IN - PPM2 
    12, // PC6  CH3_IN UART6
    13, // PC7  CH4_IN UART6
    14, // PC8  CH5_IN i2c
    15, // PC9  CH6_IN i2c
};


// Cleanflight
static const uint8_t output_channels_cleanflight[]= {  // pin assignment
    SERVO_PIN_2, //Timer3/4  - 2
    SERVO_PIN_3, //Timer2/3  - 3
    SERVO_PIN_4, //Timer2/2  - 4
    SERVO_PIN_1, //Timer3/3  - 1
    SERVO_PIN_5, //Timer2/1
    SERVO_PIN_6, //Timer2/0

    // servo channels on input port
//    4, // PB14  CH1_IN - PPM1 Use this channels asd servo only if you use DSM via UART as RC
    5, // PB15  CH2_IN - PPM2 
    12, // PC6  CH3_IN UART6
    13, // PC7  CH4_IN UART6
    14, // PC8  CH5_IN i2c
    15, // PC9  CH6_IN i2c

};

// Arducopter,shifted 2 pins right to use up to 2 servos
static const uint8_t output_channels_servo[]= {  // pin assignment
    SERVO_PIN_3, //Timer2/3  - 1
    SERVO_PIN_4, //Timer2/2  - 2
    SERVO_PIN_5, //Timer2/1  - 3
    SERVO_PIN_6, //Timer2/0  - 4
    SERVO_PIN_1, //Timer3/3    servo1
    SERVO_PIN_2, //Timer3/4    servo2

    // servo channels on input port
//    4, // PB14  CH1_IN - PPM1 Use this channels asd servo only if you use DSM via UART as RC
    5, // PB15  CH2_IN - PPM2 
    12, // PC6  CH3_IN UART6
    13, // PC7  CH4_IN UART6
    14, // PC8  CH5_IN i2c
    15, // PC9  CH6_IN i2c
};



static const uint8_t * const revo_motor_map[]={
    output_channels_arducopter,
    output_channels_servo,
    output_channels_openpilot,
    output_channels_cleanflight,
};


// #endif

static const uint8_t *output_channels = output_channels_openpilot;  // current pin assignment

enum     BOARD_PWM_MODES REVOMINIRCOutput::_mode = BOARD_PWM_NORMAL;
bool     REVOMINIRCOutput::_once_mode = false;

uint16_t REVOMINIRCOutput::_period[REVOMINI_MAX_OUTPUT_CHANNELS] IN_CCM;
uint16_t REVOMINIRCOutput::_freq[REVOMINI_MAX_OUTPUT_CHANNELS] IN_CCM;
uint8_t  REVOMINIRCOutput::_initialized[REVOMINI_MAX_OUTPUT_CHANNELS] IN_CCM;
uint16_t REVOMINIRCOutput::_enabled_channels=0;
bool     REVOMINIRCOutput::_sbus_enabled=0;
bool     REVOMINIRCOutput::_corked=0;
uint8_t  REVOMINIRCOutput::_used_channels=0;

uint8_t REVOMINIRCOutput::_servo_mask=0;

//uint32_t REVOMINIRCOutput::_timer_frequency[REVOMINI_MAX_OUTPUT_CHANNELS] IN_CCM;

uint32_t REVOMINIRCOutput::_timer2_preload;
uint16_t REVOMINIRCOutput::_timer3_preload;

uint8_t  REVOMINIRCOutput::_pwm_type=0;
float    REVOMINIRCOutput::_freq_scale=1;

#define PWM_TIMER_KHZ          2000  // 1000 in cleanflight
#define ONESHOT125_TIMER_KHZ   8000  // 8000 in cleanflight
#define ONESHOT42_TIMER_KHZ   28000  // 24000 in cleanflight
#define PWM_BRUSHED_TIMER_KHZ 16000  // 8000 in cleanflight

#define _BV(bit) (1U << (bit))

void REVOMINIRCOutput::init()
{
    memset(&_period[0], 0, sizeof(_period));
    memset(&_initialized[0], 0, sizeof(_initialized));

    _used_channels=0;
}


void REVOMINIRCOutput::do_4way_if(AP_HAL::UARTDriver* uart) {
    esc4wayInit(output_channels, REVOMINI_OUT_CHANNELS);
    esc4wayProcess(uart);
}

#ifdef DEBUG_INT
extern "C" int printf(const char *msg, ...);

static void isr_handler(){
    uint32_t *sp;

#define FRAME_SHIFT 10 // uses IRQ handler itself

    asm volatile ("mov %0, sp\n\t"  : "=rm" (sp) );
    
    // Stack frame contains:
    // r0, r1, r2, r3, r12, r14, the return address and xPSR
    // - Stacked R0  = sp[0]
    // - Stacked R1  = sp[1]
    // - Stacked R2  = sp[2]
    // - Stacked R3  = sp[3]
    // - Stacked R12 = sp[4]
    // - Stacked LR  = sp[5]
    // - Stacked PC  = sp[6]
    // - Stacked xPSR= sp[7]
    printf("pc=%lx lr=%lx\n", sp[FRAME_SHIFT+6], sp[FRAME_SHIFT+5]);
}
#endif
void REVOMINIRCOutput::lateInit(){ // 2nd stage with loaded parameters

    uint8_t map = hal_param_helper->_motor_layout;
    _servo_mask = hal_param_helper->_servo_mask;
    _pwm_type   = hal_param_helper->_pwm_type;
    
    if(map >= ARRAY_SIZE(revo_motor_map)) return; // don't initialize if parameter is wrong
    output_channels = revo_motor_map[map];
    
    
    InitPWM();
    
#ifdef DEBUG_INT
    uint8_t spin = output_channels[DEBUG_INT]; // motor 6 as button
    REVOMINIGPIO::_pinMode(spin, INPUT_PULLUP);

    Revo_handler h = { .vp = isr_handler  };  
    REVOMINIGPIO::_attach_interrupt(spin, h.h, FALLING, 0);
    

#endif
}

void REVOMINIRCOutput::InitPWM()
{
    for(uint8_t i = 0; i < REVOMINI_MAX_OUTPUT_CHANNELS && i < REVOMINI_OUT_CHANNELS; i++) {
        _freq[i] = 50;
        uint8_t pin = output_channels[i];
        REVOMINIGPIO::_pinMode(pin, PWM);
        _initialized[i] = true;
    }
    _set_output_mode(MODE_PWM_NORMAL); // init timers
}

uint32_t inline REVOMINIRCOutput::_timer_period(uint16_t speed_hz) {
    return (uint32_t)((PWM_TIMER_KHZ*1000UL) / speed_hz);
}


// not from _freq to take channel dependency
uint16_t REVOMINIRCOutput::get_freq(uint8_t ch) {
    if(ch >= REVOMINI_MAX_OUTPUT_CHANNELS) return 0;
    
    const timer_dev *dev = PIN_MAP[output_channels[ch]].timer_device;

    /* transform to period by inverse of _time_period(icr) */
    return (uint16_t)((PWM_TIMER_KHZ*1000UL) / timer_get_reload(dev));
}


void REVOMINIRCOutput::_set_output_mode(enum REVOMINIRCOutput::output_mode mode) {

    
    uint32_t period=0;
    uint32_t freq;
    uint32_t real_freq;

    _once_mode=false;
    

    switch(_pwm_type) {
    case 0:
    default:
        switch(mode){
        case MODE_PWM_NORMAL:
            _mode=BOARD_PWM_NORMAL;
            break;

        case MODE_PWM_BRUSHED:
            _mode=BOARD_PWM_BRUSHED;
            break;    
        
        default:
        case MODE_PWM_ONESHOT:
            _mode=BOARD_PWM_ONESHOT;
            break;
        }  
        break;
          
    case 1:
        _mode=BOARD_PWM_ONESHOT;
        break;
        
    case 2:
        _mode=BOARD_PWM_ONESHOT125;
        break;
    
    case 3:
        _mode=BOARD_PWM_ONESHOT42;
        break;

    case 4:
        _mode=BOARD_PWM_PWM125;
        break;
    }
    

    switch(_mode){

    case BOARD_PWM_NORMAL:
    default:
// output uses timers 2 & 3 so let init them for PWM mode
        period    = ((PWM_TIMER_KHZ*1000UL) / 50); // 50Hz by default - will be corrected late per-channel in init_channel()
        freq      = PWM_TIMER_KHZ;        // 2MHz 0.5us ticks - for 50..490Hz PWM
        break;

    case BOARD_PWM_ONESHOT: // same as PWM but with manual restarting
// output uses timers 2 & 3 so let init them for PWM mode
        period    = (uint32_t)-1; // max possible
        freq      = PWM_TIMER_KHZ;       // 2MHz 0.5us ticks - for 50..490Hz PWM
        _once_mode=true;
        break;

    case BOARD_PWM_ONESHOT125:
        period    = (uint32_t)-1; // max possible
        freq      = ONESHOT125_TIMER_KHZ;       // 16MHz 62.5ns ticks - for 125uS..490Hz OneShot125
//    at 16Mhz, period with 65536 will be 244Hz so even 16-bit timer will never overflows at 500Hz loop
        _once_mode=true;
        break;

   case BOARD_PWM_PWM125: // like Oneshot125 but PWM so not once
        period    = (uint32_t)-1; // max possible
        freq      = ONESHOT125_TIMER_KHZ;       // 16MHz 62.5ns ticks - for 125uS..490Hz OneShot125
//    at 16Mhz, period with 65536 will be 244Hz so even 16-bit timer will never overflows at 500Hz loop
        break;

    case BOARD_PWM_ONESHOT42:
        period    = (uint32_t)-1; // max possible
        freq      = ONESHOT42_TIMER_KHZ;       // 16MHz 62.5ns ticks - for 125uS..490Hz OneShot125
//    at 28Mhz, period with 65536 will be 427Hz so even 16-bit timer should not overflows at 500Hz loop
        _once_mode=true;
        break;

    case BOARD_PWM_BRUSHED: 
                     // dev    period   freq, kHz
        configTimeBase(TIMER2, 1000,    PWM_BRUSHED_TIMER_KHZ);       // 16MHz  - 0..1 in 1000 steps
        configTimeBase(TIMER3, 1000,    PWM_BRUSHED_TIMER_KHZ);       // 16MHz 
        _freq_scale=1; //used ratio, not time
        goto do_init;
    }

                             // dev    period   freq, kHz
    real_freq = configTimeBase(TIMER2, period,  freq);       // 16MHz 62.5ns ticks - for 125uS..490Hz OneShot125
                configTimeBase(TIMER3, period,  freq);       // 16MHz 62.5ns ticks 

    _freq_scale = (float)real_freq / (freq * 1000);

do_init:
    init_channels();
    timer_resume(TIMER2);
    timer_resume(TIMER3);
}



bool REVOMINIRCOutput::is_servo_enabled(uint8_t ch){
    if(ch>=REVOMINI_OUT_CHANNELS){ // servos
        uint8_t sn = ch - REVOMINI_OUT_CHANNELS;
        if(!(_servo_mask & (1<<sn)) ) return false;
    }

    return true;
}

// for Oneshot125 
// [1000;2000] => [125;250]
// so frequency of timers should be 8 times more - 16MHz, but timers on 84MHz can give only 16.8MHz

// channels 1&2, 3&4&5&6 can has a different rates
void REVOMINIRCOutput::set_freq(uint32_t chmask, uint16_t freq_hz) {          
    uint32_t mask=1;
    
    
    for(uint8_t i=0; i< REVOMINI_MAX_OUTPUT_CHANNELS; i++) { // кто последний тот и папа
        if(chmask & mask) {
            if(!(_enabled_channels & mask) ) return;      // not enabled

// for true one-shot        if(_once_mode && freq_hz>50) continue; // no frequency in OneShoot modes
            _freq[i] = freq_hz;

            if(_once_mode && freq_hz>50) freq_hz /=2; // divide frequency by 2 in OneShoot modes
            const uint8_t pin = output_channels[i];
            const timer_dev *dev = PIN_MAP[pin].timer_device;
            timer_set_reload(dev,  _timer_period(freq_hz)); 
        }
        mask <<= 1;
    }
}

void REVOMINIRCOutput::init_channel(uint8_t ch){
    if(ch>=REVOMINI_MAX_OUTPUT_CHANNELS) return;

    uint8_t pin = output_channels[ch];
    if (pin >= BOARD_NR_GPIO_PINS) return;

    const stm32_pin_info &p = PIN_MAP[pin];
    const timer_dev *dev = p.timer_device;

    timer_set_mode(   dev, p.timer_channel, TIMER_PWM);

    uint16_t freq = _freq[ch];
    if(_once_mode && freq>50) freq/=2;
    timer_set_reload(dev,  _timer_period(freq));
    timer_set_compare(dev, p.timer_channel, 0); // to prevent outputs in case of timer overflow
}


void REVOMINIRCOutput::init_channels(){
    for (uint16_t ch = 0; ch < REVOMINI_OUT_CHANNELS; ch++) {
        init_channel(ch);
    }
}

/* constrain pwm to be between min and max pulsewidth */
static inline uint16_t constrain_period(uint16_t p) {
    if (p > RC_INPUT_MAX_PULSEWIDTH) return RC_INPUT_MAX_PULSEWIDTH;
    if (p < RC_INPUT_MIN_PULSEWIDTH) return RC_INPUT_MIN_PULSEWIDTH;
    return p;
}

void REVOMINIRCOutput::set_pwm(uint8_t ch, uint16_t pwm){
    if(ch>=REVOMINI_MAX_OUTPUT_CHANNELS) return;

    if (!(_enabled_channels & _BV(ch))) return;      // not enabled

    if(!is_servo_enabled(ch)) return; // disabled servo

    uint8_t pin = output_channels[ch];
    if (pin >= BOARD_NR_GPIO_PINS) return;


    switch(_mode){
    case BOARD_PWM_BRUSHED:
        pwm -= 1000; // move from 1000..2000 to 0..1000
        break;

    case BOARD_PWM_ONESHOT42: // works at single freq
    case BOARD_PWM_ONESHOT125:
        break;

    default:
        pwm <<= 1; // frequency of timers 2MHz
        break;
    }

    pwm *= _freq_scale; // учесть неточность установки частоты таймера для малых прескалеров

    const stm32_pin_info &p = PIN_MAP[pin];
    const timer_dev *dev = p.timer_device;
    timer_set_compare(dev, p.timer_channel, pwm); 
}

void REVOMINIRCOutput::write(uint8_t ch, uint16_t period_us)
{
    if(ch>=REVOMINI_MAX_OUTPUT_CHANNELS) return;


    if(_used_channels<ch) _used_channels=ch+1;
    
    uint16_t pwm = constrain_period(period_us);

    if(_period[ch]==pwm) return; // already so

    _period[ch]=pwm;
    
    if(_corked) return;

    set_pwm(ch, pwm);
}



void REVOMINIRCOutput::write(uint8_t ch, uint16_t* period_us, uint8_t len)
{
    if(ch>=REVOMINI_MAX_OUTPUT_CHANNELS) return;

    for (int i = 0; i < len; i++) {
        write(i + ch, period_us[i]);
    }
}

uint16_t REVOMINIRCOutput::read(uint8_t ch)
{
    if(ch>=REVOMINI_MAX_OUTPUT_CHANNELS) return RC_INPUT_MIN_PULSEWIDTH;

    return _period[ch];
}

void REVOMINIRCOutput::read(uint16_t* period_us, uint8_t len)
{
// here we don't need to limit channel count - all unsupported will be read as RC_INPUT_MIN_PULSEWIDTH
    for (int i = 0; i < len; i++) {
        period_us[i] = read(i);
    }
}


void REVOMINIRCOutput::enable_ch(uint8_t ch)
{
    if (ch >= REVOMINI_MAX_OUTPUT_CHANNELS) 
        return;

#if 0
    if (ch >= 8 && !(_enabled_channels & (1U<<ch))) {
        // this is the first enable of an auxiliary channel - setup
        // aux channels now. This delayed setup makes it possible to
        // use BRD_PWM_COUNT to setup the number of PWM channels.
        _init_alt_channels();
    }
#endif

    if(_enabled_channels & (1U<<ch) ) return; // already OK

    if(!is_servo_enabled(ch)) return; // disabled servo
            
    _enabled_channels |= (1U<<ch);
    if (_period[ch] == PWM_IGNORE_THIS_CHANNEL) {
        _period[ch] = 0;
    }
    
    uint8_t pin = output_channels[ch];
    
    if(!_initialized[ch]) {
    
        const timer_dev *dev = PIN_MAP[pin].timer_device;
    
        uint32_t period    = ((PWM_TIMER_KHZ*1000UL) / 50); // 50Hz by default
        configTimeBase(dev, period,  PWM_TIMER_KHZ);       // 2MHz 0.5us ticks - for 50..490Hz PWM
        timer_resume(dev);

        REVOMINIGPIO::_pinMode(pin, PWM);
    
        _initialized[ch]=true;
    }

#ifdef DEBUG_INT
    uint8_t spin = output_channels[DEBUG_INT]; // motor 6
    REVOMINIGPIO::_pinMode(spin, INPUT_PULLUP);
#endif
    
}

void REVOMINIRCOutput::disable_ch(uint8_t ch)
{
    if (ch >= REVOMINI_MAX_OUTPUT_CHANNELS) {
        return;
    }
    
    if(!is_servo_enabled(ch)) return; // disabled servo

    _enabled_channels &= ~(1U<<ch);
    _period[ch] = PWM_IGNORE_THIS_CHANNEL;
    

    uint8_t pin = output_channels[ch];
    if (pin >= BOARD_NR_GPIO_PINS) return;

    REVOMINIGPIO::_pinMode(pin, OUTPUT);
    REVOMINIGPIO::_write(pin, 0);
}


void REVOMINIRCOutput::push()
{
    _corked = false;

#ifdef DEBUG_PWM
    uint8_t spin = output_channels[DEBUG_PWM]; // motor 6 as strobe
    REVOMINIGPIO::_pinMode(spin, OUTPUT);
    REVOMINIGPIO::_write(spin, 1);
#endif
    
    for (uint16_t ch = 0; ch < _used_channels; ch++) {
        set_pwm(ch, _period[ch]);
    }

    if(_once_mode){
    // generate timer's update on ALL used pins, but only once per timer

        for (uint16_t ch = 0; ch < REVOMINI_OUT_CHANNELS; ch++) {
            uint8_t pin = output_channels[ch];
    
            const timer_dev *tim = PIN_MAP[pin].timer_device;
            tim->state->updated=false;  // reset flag first
        }

        for (uint16_t ch = 0; ch < REVOMINI_OUT_CHANNELS; ch++) {
            if (!(_enabled_channels & _BV(ch))) continue;      // not enabled
            uint8_t pin = output_channels[ch];
            const stm32_pin_info &p = PIN_MAP[pin];
            const timer_dev *tim = p.timer_device;
    
            if(!tim->state->updated) { // update each timer once
                tim->state->updated=true;
                timer_generate_update(tim);
            }
            timer_set_compare(tim, p.timer_channel, 0); // to prevent outputs  in case of timer overflows
        }
    
//        timer_generate_update(TIMER2);
//        timer_generate_update(TIMER3);
    }
        
#ifdef DEBUG_PWM
    REVOMINIGPIO::_write(spin, 0);
#endif
}

