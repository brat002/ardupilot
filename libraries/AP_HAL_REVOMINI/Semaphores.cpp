/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#pragma GCC optimize ("O2")

#include <AP_HAL_REVOMINI/AP_HAL_REVOMINI.h>
#include "Semaphores.h"
#include "Scheduler.h"

using namespace REVOMINI;

extern const AP_HAL::HAL& hal;

bool Semaphore::_error = false;

#ifdef SEM_PROF 
uint64_t Semaphore::sem_time=0;    
#endif

// Constructor
Semaphore::Semaphore()
    : _taken(false)
    , _task(NULL)
    , _weak(false)
{}


bool Semaphore::give() {
    noInterrupts();
    if (_taken) {
        _taken = false;
        _task = NULL;
        interrupts();
        REVOMINIScheduler::yield();        // switch context to waiting task if any
        return true;
    }
    interrupts();
    return false;
}

bool Semaphore::take_nonblocking() {       
    bool ret = _take_nonblocking(); 
    if(!ret) { //Increase the priority to one tick of the semaphore's owner up to the priority of the current task, without task blocking
        REVOMINIScheduler::task_want_semaphore(_task, NULL); 
    }
    return ret; 
}


bool Semaphore::take(uint32_t timeout_ms) {
    if (REVOMINIScheduler::in_interrupt()) {
        if(_take_nonblocking()) {
            return true; // all OK if we got
        }
        
/*
!!!    we got breaking changes from upstream - all drivers now calls ->take(WAIT_FOREVER) so I can't allow to kill UAV

        if(timeout_ms) {       // no panic in air! just return
             AP_HAL::panic("PANIC: Semaphore::take used from inside timer process");
*/

// let set global flag and check it in scheduler so reschedule such tasks in next tick        
            _error=true; // remember that it was
//                      or to add queue for such tasks and use setjmp/longjmp to emulate waiting in semaphore
            
            return false; 
//        } 
    }
    return _take_from_mainloop(timeout_ms);
}

bool Semaphore::_take_from_mainloop(uint32_t timeout_ms) {
    
    /* Try to take immediately */
    if (_take_nonblocking()) {
        return true;
    } 

    hal_yield(0); // to sync with context switch timer

    bool ret=false;

    uint32_t t  = REVOMINIScheduler::_micros(); 
    uint32_t dt = timeout_ms*1000; // timeout time

    do {
        if(timeout_ms == HAL_SEMAPHORE_BLOCK_FOREVER){
            //Increase the priority of the semaphore's owner up to the priority of the current task and block task
            REVOMINIScheduler::task_want_semaphore(_task, this); 
        } else  {
            //Increase the priority of the semaphore's owner up to the priority of the current task for one tick
            REVOMINIScheduler::task_want_semaphore(_task, NULL); 
        }

        hal_yield(0); // no max task time - this is more useful, task will be excluded from scheduling until semaphore released  // REVOMINIScheduler::_delay_microseconds(10);
        if (_take_nonblocking()) {
            ret= true;
            break;
        }
    } while(timeout_ms == HAL_SEMAPHORE_BLOCK_FOREVER ||  (REVOMINIScheduler::_micros() - t) < dt);

#ifdef SEM_PROF 
    sem_time += REVOMINIScheduler::_micros()-t; // calculate semaphore wait time
#endif

    return ret;
}

bool Semaphore::_take_nonblocking() {
    noInterrupts();
    void *me = REVOMINIScheduler::get_current_task();
    if (!_taken) {
        _taken = true;
        _task = me;// remember task which owns semaphore 
        if(!_weak)  REVOMINIScheduler::task_has_semaphore(true); 
        interrupts();
        return true;
    }
    if(_task == me){     // the current task already owns this semaphore
        interrupts();
        return true; 
    }
    interrupts();
    return false;
}

