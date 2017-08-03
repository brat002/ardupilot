
#ifndef __AP_HAL_REVOMINI_CLASS_H__
#define __AP_HAL_REVOMINI_CLASS_H__


#if CONFIG_HAL_BOARD == HAL_BOARD_REVOMINI

#include <AP_HAL_REVOMINI/AP_HAL_REVOMINI.h>
#include "handler.h"

#include <AP_Param/AP_Param.h>

#include <hal.h>
#include "USBDriver.h"
#include <pwm_in.h>
#include <usart.h>
#include <i2c.h>
#include <spi.h>


#if defined(USB_MASSSTORAGE)
#include "massstorage/mass_storage.h"
#endif


class HAL_state{ 
public:
    HAL_state()
     : sd_busy(0)
     , disconnect(0)
    {}

    uint8_t sd_busy;
    uint8_t disconnect;
};

class HAL_REVOMINI : public AP_HAL::HAL {
public:
    HAL_REVOMINI();
    void run(int argc, char * const argv[], Callbacks* callbacks) const override;

    void lateInit();

    static HAL_state state;
    
private:


    void connect_uart(AP_HAL::UARTDriver* uartL,AP_HAL::UARTDriver* uartR, AP_HAL::Proc proc);
    // parameters in hal_param_helper

#if defined(USB_MASSSTORAGE)
    REVOMINI::MassStorage massstorage;
#endif

};


//FUNCTOR_TYPEDEF(PeriodicCbBool, bool);


#endif // __AP_HAL_REVOMINI_CLASS_H__
#endif // __HAL_BOARD_REVOMINI__
