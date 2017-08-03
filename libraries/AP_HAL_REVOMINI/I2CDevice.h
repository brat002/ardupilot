/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
/*
 * Copyright (C) 2015-2016  Intel Corporation. All rights reserved.
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <inttypes.h>
#include "AP_HAL_REVOMINI_Namespace.h"
#include "Scheduler.h"

#include <AP_HAL/HAL.h>
#include <AP_HAL_REVOMINI/HAL_REVOMINI_Class.h>
#include <AP_HAL/I2CDevice.h>
#include <AP_HAL/utility/OwnPtr.h>
#include "Semaphores.h"

#include <i2c.h>
#include "i2c_soft.h"

//#define I2C_DEBUG

using namespace REVOMINI;


#define MAX_I2C_DEVICES 10

class REVOMINI::REVOI2CDevice : public AP_HAL::I2CDevice {
public:

    REVOI2CDevice(uint8_t bus, uint8_t address);
    
    ~REVOI2CDevice();

    static void lateInit();

    /* AP_HAL::I2CDevice implementation */

    /* See AP_HAL::I2CDevice::set_address() */
    inline void set_address(uint8_t address) override { _address = address; }

    /* See AP_HAL::I2CDevice::set_retries() */
    inline void set_retries(uint8_t retries) override { _retries = retries; }


    /* AP_HAL::Device implementation */

    /* See AP_HAL::Device::transfer() */
    bool transfer(const uint8_t *send, uint32_t send_len,
                  uint8_t *recv, uint32_t recv_len) override;


    bool read_registers_multiple(uint8_t first_reg, uint8_t *recv,
                                 uint32_t recv_len, uint8_t times);


    /* See AP_HAL::Device::set_speed() */
    inline bool set_speed(enum AP_HAL::Device::Speed speed) override { return true; };

    /* See AP_HAL::Device::get_semaphore() */
    inline AP_HAL::Semaphore *get_semaphore() override { return &_semaphores[_bus]; } // numbers from 0

    /* See AP_HAL::Device::register_periodic_callback() */
    inline AP_HAL::Device::PeriodicHandle register_periodic_callback(
        uint32_t period_usec, Device::PeriodicCb proc) override
    {   
        return REVOMINIScheduler::register_timer_task(period_usec, proc, &_semaphores[_bus] );
    }

    inline AP_HAL::Device::PeriodicHandle register_periodic_callback(
        uint32_t period_usec, PeriodicCbBool proc)
    {
        return REVOMINIScheduler::register_timer_task(period_usec, proc, &_semaphores[_bus] );
    }


    inline bool adjust_periodic_callback(
        AP_HAL::Device::PeriodicHandle h, uint32_t period_usec) override 
    {
        return REVOMINIScheduler::adjust_timer_task(h, period_usec);
    }
    
    inline bool unregister_callback(PeriodicHandle h) override  { return REVOMINIScheduler::unregister_timer_task(h); }

    inline uint32_t get_error_count() {return _lockup_count; }
    inline uint8_t get_bus() {return _bus; }
    inline uint8_t get_addr() {return _address; }
    
    static inline uint8_t get_dev_count() {return dev_count; }
    static inline REVOMINI::REVOI2CDevice * get_device(uint8_t i) { return devices[i]; }

private:
    void init();

    uint8_t _bus;
    uint16_t _offs;
    uint8_t _address;
    uint8_t _retries;
    uint32_t _lockup_count;
    bool _initialized;

    const i2c_dev *_dev;
    Soft_I2C s_i2c; // per-bus instances

    static REVOMINI::Semaphore _semaphores[3]; // individual for each bus + softI2C
    static REVOMINI::REVOI2CDevice * devices[MAX_I2C_DEVICES]; // links to all created devices
    static uint8_t dev_count;

    static bool lateInitDone;

    bool _slow;
    bool _failed;
    
#ifdef I2C_DEBUG
    static uint8_t last_addr, last_op, last_send_len, last_recv_len, busy, last_status;
#endif
};

class REVOMINI::I2CDeviceManager : public AP_HAL::I2CDeviceManager {
    friend class REVOMINI::REVOI2CDevice;
    
public:
    I2CDeviceManager() { }

    /* AP_HAL::I2CDeviceManager implementation */
    AP_HAL::OwnPtr<AP_HAL::I2CDevice> get_device(uint8_t bus, uint8_t address) {

        // let's first check for existence of such device on same bus
        uint8_t n = REVOI2CDevice::get_dev_count();

        for(uint8_t i=0; i<n; i++){
            REVOI2CDevice * d = REVOI2CDevice::get_device(i);
            if(d){
                if(d->get_bus() == bus && d->get_addr() == address) { // device already exists
                    return nullptr;
                }
            }
        }
    
        return AP_HAL::OwnPtr<AP_HAL::I2CDevice>(
            new REVOI2CDevice(bus, address)
        );
    }
};

