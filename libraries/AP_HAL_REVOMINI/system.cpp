#pragma GCC optimize ("O2")

#include <stdarg.h>
#include <stdio.h>

#include <AP_HAL/AP_HAL.h>
#include <AP_HAL/system.h>

#include "Scheduler.h"

extern const AP_HAL::HAL& hal;

using namespace REVOMINI;


namespace AP_HAL {

void cond_yield();

void init() // early init on creation of HAL class
{
}

void panic(const char *errormsg, ...)
{
    /* Suspend timer processes. We still want the timer event to go off to run the _failsafe code, however. */
    va_list ap;

    hal.scheduler->suspend_timer_procs();

    if(boardEmergencyHandler) boardEmergencyHandler(); // call emergency handler before

    va_start(ap, errormsg);
    hal.console->vprintf(errormsg, ap);
    va_end(ap);
    hal.console->printf("\n");

    if(is_bare_metal()) { // bare metal build without bootloader should reboot to DFU after any fault
        board_set_rtc_register(DFU_RTC_SIGNATURE, RTC_SIGNATURE_REG);
    }

    error_throb(0);
}

void cond_yield(){
    static uint32_t last_yield=0;
    uint32_t t=REVOMINIScheduler::_millis();
    if(t-last_yield>50) {
        REVOMINIScheduler::yield(0);
        last_yield=t;
    }
}


uint32_t millis()
{
    cond_yield();
    return REVOMINIScheduler::_millis();
}


uint64_t millis64(){
    cond_yield();
    return REVOMINIScheduler::_millis64();
}

uint32_t micros() {
    return REVOMINIScheduler::_micros();
}

uint64_t micros64(){
    return REVOMINIScheduler::_micros64();
}

// revo internals

void delay(uint32_t ms);
void delay_microseconds(uint16_t us);
void yield(uint32_t us);

void delay(uint32_t ms){
    REVOMINIScheduler::_delay(ms); 
}

void delay_microseconds(uint16_t us) { 
    REVOMINIScheduler::_delay_microseconds(us); 
}

void yield(uint32_t us){
    REVOMINIScheduler::yield(us); 
}


} // namespace AP_HAL


// export console IO to low-level functions
// so any printf will go to console, not to /dev/null
extern "C" {
    unsigned char getch(void);
    
    int _read(int fd, char *buf, size_t cnt);

    void putch(unsigned char c);
    
    extern int printf(const char *msg, ...);
}


unsigned char getch(void) {
    if(hal.console->available())
        return hal.console->read();
    return 0;
}
    
int _read(int fd, char *buf, size_t cnt) {
    uint16_t rcv=0;
    
    while(cnt--){
        if(hal.console->available()){
            *buf++ = hal.console->read();
            rcv++;
        } else break;
    }

    return rcv;
}

void putch(unsigned char c) {
    hal.console->write(c);
}

int printf(const char *msg, ...){
    va_list ap;

    va_start(ap, msg);
    hal.console->vprintf(msg, ap);
    va_end(ap);
    return 1;
}

