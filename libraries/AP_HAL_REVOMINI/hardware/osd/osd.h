#pragma once 

#include <AP_HAL/AP_HAL.h>

#ifdef BOARD_OSD_CS_PIN

#define OSD_RX_BUF_SIZE 128

#include <AP_HAL_REVOMINI/AP_HAL_REVOMINI.h>

extern const AP_HAL::HAL& hal;

#include "osd_namespace.h"

#include "osd_core/compat.h"
#include "osd_core/Defs.h"


namespace OSDns {// OSD interface emulates UART

    void osd_begin(AP_HAL::OwnPtr<REVOMINI::SPIDevice> spi);
    void osd_loop();

    int16_t osd_available();

    int16_t osd_getc();

    void osd_putc(uint8_t c); 

    void max_do_transfer(const uint8_t *buffer, uint16_t len);
    void update_max_buffer(const uint8_t *buffer, uint16_t len);

}
#endif
