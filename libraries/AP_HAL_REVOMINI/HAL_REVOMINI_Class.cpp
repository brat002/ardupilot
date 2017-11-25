#pragma GCC optimize ("O2")

#include <AP_HAL/AP_HAL.h>

#if CONFIG_HAL_BOARD == HAL_BOARD_REVOMINI
#include <assert.h>
#include <AP_HAL_REVOMINI/AP_HAL_REVOMINI.h>
#include "AP_HAL_REVOMINI_Namespace.h"
#include "AP_HAL_REVOMINI_Private.h"
#include "HAL_REVOMINI_Class.h"
#include "RCInput.h"
#include "Util.h"
//#include <AP_HAL_Empty/AP_HAL_Empty.h>
//#include <AP_HAL_Empty/AP_HAL_Empty_Private.h>

#include <AP_Param_Helper/AP_Param_Helper.h>

#include "UART_PPM.h"

#if defined(USE_SOFTSERIAL)
#include "UART_SoftDriver.h"
#endif


#if USE_WAYBACK == ENABLED
#include "AP_WayBack/AP_WayBack.h"
#endif



#if defined(BOARD_SDCARD_NAME) || defined(BOARD_DATAFLASH_FATFS)
#include "sd/SD.h"
#endif

#if defined(BOARD_OSD_CS_PIN)
#include "UART_OSD.h"
#endif

#if defined(USB_MASSSTORAGE) 
#include "massstorage/mass_storage.h"
#endif


using namespace AP_HAL;
using namespace REVOMINI;


static REVOMINI::I2CDeviceManager i2c_mgr_instance;

// XXX make sure these are assigned correctly
static USBDriver USB_Driver(1);                 // ACM
static REVOMINIUARTDriver uart1Driver(_USART1); // main port
static REVOMINIUARTDriver uart6Driver(_USART6); // pin 7&8(REVO)/5&6(RevoMini) of input port

#ifdef BOARD_HAS_UART3
 static REVOMINIUARTDriver uart3Driver(_USART3); // flexi port
#elif defined(BOARD_SOFT_UART3)
 static SerialDriver uart3Driver(false);              // soft UART on pins 7&8 of input port
#elif defined(USE_SOFTSERIAL)
 static SerialDriver softDriver(false) IN_CCM;  // pin 7&8 of input port
#endif


#ifdef BOARD_OSD_CS_PIN
 static UART_OSD uartOSDdriver;
#endif

#if FRAME_CONFIG == QUAD_FRAME && defined(BOARD_USART4_RX_PIN) && defined( BOARD_USART4_TX_PIN)
 static REVOMINIUARTDriver uart4Driver(_UART4);  // pin 5&6 of servo port
#endif

static UART_PPM uartPPM2(1); // PPM2 input IN_CCM
//static UART_PPM uartPPM1(0); // PPM1 input 



// only for DSM satellite, served in rc_input
//static REVOMINIUARTDriver uart5Driver(_UART5,0);  // D26/PD2  6 EXTI_RFM22B / UART5_RX  input-only UART for DSM satellite

/*
        input port pinout
        1    2    3    4    5    6    7   8
pin              b14  b15  c6   c7   c8   c9
       gnd  vcc  PPM  buzz 6_tx 6_rx Sscl Ssda
USE_SOFTSERIAL ->                    S_tx S_rx
servos ->        srv7 srv8 srv9 srv10 srv11 srv12 
*/


/* OPLINK AIR port pinout
1       2       3       4       5       6       7
                        
gnd    +5      26      103                     
               rx      pow
*/

static REVOMINI::SPIDeviceManager spiDeviceManager;
static REVOMINIAnalogIn  analogIn;
static REVOMINIStorage   storageDriver;
static REVOMINIGPIO      gpioDriver;
static REVOMINIRCInput   rcinDriver;
static REVOMINIRCOutput  rcoutDriver;
static REVOMINIScheduler schedulerInstance;
static REVOMINIUtil      utilInstance;

HAL_state HAL_REVOMINI::state;

//const AP_HAL::UARTDriver** HAL_REVOMINI::uarts[] = { &uartA, &uartB, &uartC, &uartD, &uartE, &uartF };

/*
        AP_HAL::UARTDriver* _uartA, // console
        AP_HAL::UARTDriver* _uartB, // 1st GPS
        AP_HAL::UARTDriver* _uartC, // telem1
        AP_HAL::UARTDriver* _uartD, // telem2
        AP_HAL::UARTDriver* _uartE, // 2nd GPS - 
        AP_HAL::UARTDriver* _uartF, // extra1
*/
HAL_REVOMINI::HAL_REVOMINI() :
    AP_HAL::HAL(
        &USB_Driver,            /* uartA - USB console */
        &uart6Driver,           /* uartB - pin 7&8(REVO)/5&6(RevoMini) of input port - for GPS */
        &uart1Driver,           /* uartC - main port  - for telemetry */
#ifdef BOARD_HAS_UART3
        &uart3Driver,           /* uartD - flexi port */
#elif defined(BOARD_SOFT_UART3)
        &uart3Driver,           /* uartD - sotf UART on pins 7&8 */
#else
        NULL,                   /* no uartD */
#endif

#if FRAME_CONFIG == QUAD_FRAME && defined(BOARD_USART4_RX_PIN)
        &uart4Driver,           /* uartE  - PWM pins 5&6 */
#elif defined(BOARD_OSD_NAME) && defined(USE_SOFTSERIAL) && defined(BOARD_SOFTSERIAL_TX) && defined(BOARD_SOFTSERIAL_RX)
        &softDriver,            /* uartE softSerial on boards with OSD*/
#else
//        NULL,                   /* no uartE */
        &uartPPM2,              /* uartE - input data from PPM2 pin */
#endif

#if defined(BOARD_OSD_NAME)
        &uartOSDdriver,         /* uartF  - OSD emulated UART */
#elif defined(USE_SOFTSERIAL) && defined(BOARD_SOFTSERIAL_TX) && defined(BOARD_SOFTSERIAL_RX)
        &softDriver,   /* uartF softSerial */
#else
        NULL,                   /* no uartF */
#endif
        &i2c_mgr_instance,
        &spiDeviceManager,  /* spi */
        &analogIn,          /* analogin */
        &storageDriver,     /* storage */
        &HAL_CONSOLE,       /* console via radio or USB on per-board basis */
        &gpioDriver,        /* gpio */
        &rcinDriver,        /* rcinput */
        &rcoutDriver,       /* rcoutput */
        &schedulerInstance, /* scheduler */
        &utilInstance,	    /* util */
        nullptr,            /* no optical flow */
        nullptr             /* no CAN */
    )
    
           //  0     1       2       3        4       5
           // USB    Main    Flexi   UART6    Uart4   Soft/Osd
    , uarts{ &uartA, &uartC, &uartD, &uartB,  &uartE, &uartF }

{

    uint32_t sig = board_get_rtc_register(RTC_CONSOLE_REG);
    if( (sig & ~CONSOLE_PORT_MASK) == CONSOLE_PORT_SIGNATURE) {
        AP_HAL::UARTDriver** up = uarts[sig & CONSOLE_PORT_MASK];
        if(up){        
            console =  *up;
        }
    }
}

extern const AP_HAL::HAL& hal;


void HAL_REVOMINI::run(int argc,char* const argv[], Callbacks* callbacks) const
{
    assert(callbacks);

    /* initialize all drivers and private members here.
     * up to the programmer to do this in the correct order.
     * Scheduler should likely come first. */

    scheduler->init();

    gpio->init();

    rcout->init(); 
    
    usart_disable_all();

    {
#if defined(USB_MASSSTORAGE)
        uint32_t sig = board_get_rtc_register(RTC_MASS_STORAGE_REG);
        if( sig == MASS_STORAGE_SIGNATURE) {
            board_set_rtc_register(0, RTC_MASS_STORAGE_REG);

#if defined(BOARD_SDCARD_NAME) && defined(BOARD_SDCARD_CS_PIN)
            SD.begin(REVOMINI::SPIDeviceManager::_get_device(BOARD_SDCARD_NAME));
#elif defined(BOARD_DATAFLASH_FATFS)
            SD.begin(REVOMINI::SPIDeviceManager::_get_device(HAL_DATAFLASH_NAME));
#endif

            state.sd_busy=true;
            massstorage.setup();        //      init USB as mass-storage
        } 
        else 
#endif
        {
            extern void usb_init();     //      init as VCP
            usb_init(); // moved from boards.cpp
            printf("\nUSB init done at %ldms\n", millis());            

            uartA->begin(115200); // uartA is the USB serial port used for the console, so lets make sure it is initialized at boot 

        }
    }
    

    if(console != uartA) {
        console->begin(57600);  // init telemetry port as console
    }

    rcin->init();

    storage->init(); // Uses EEPROM.*, flash_stm* reworked
    analogin->init();

    if(!state.sd_busy) {

#if defined(BOARD_SDCARD_NAME) && defined(BOARD_SDCARD_CS_PIN)
        SD.begin(REVOMINI::SPIDeviceManager::_get_device(BOARD_SDCARD_NAME));
#elif defined(BOARD_DATAFLASH_FATFS)
        SD.begin(REVOMINI::SPIDeviceManager::_get_device(HAL_DATAFLASH_NAME));
#endif

#if defined(BOARD_OSD_NAME)
        uartF->begin(57600); // init OSD after SD but before call to lateInit(), but only if not in USB_STORAGE
#endif

    }


    printf("\nHAL init done at %ldms\n", millis());            

    callbacks->setup();

#if 0 //[ here is too late :( so we need a small hack and call lateInit from REVOMINIScheduler::register_delay_callback 
//         which called when parameters already initialized
    
    lateInit();
#endif //]

    
    scheduler->system_initialized(); // clear bootloader flag

    REVOMINIScheduler::start_stats_task(); 

    printf("\nLoop started at %ldms\n", millis());            


// main application loop hosted here!
    for (;;) {
        callbacks->loop();
//        ((REVOMINI::REVOMINIScheduler *)scheduler)->loop(); // to execute stats in main loop
//        ((REVOMINI::REVOMINIRCInput *)rcin)->loop(); // to execute debug in main loop
    }
}


#if USE_WAYBACK == ENABLED && defined(WAYBACK_DEBUG)

#define SERIAL_BUFSIZE 128

static AP_HAL::UARTDriver* uart;

static void getSerialLine(char *cp ){      // получение строки
    uint8_t cnt=0; // строка не длиннее 256 байт

    while(true){
        if(!uart->available()){
            continue;
        }

        char c=uart->read();

        if(c==0x0d || (cnt && c==0x0a)){
            cp[cnt]=0;
            return;
        }
        if(c==0x0a) continue; // skip unneeded LF

        cp[cnt]=c;
        if(cnt<SERIAL_BUFSIZE) cnt++;

    }

}
#endif

static bool lateInitDone=false;



void HAL_REVOMINI::lateInit() {
    
    if(lateInitDone) return;
    
    lateInitDone=true;

    {
        uint32_t sig = board_get_rtc_register(RTC_CONSOLE_REG);
        uint8_t port = hal_param_helper->_console_uart;

        if(port < sizeof(uarts)/sizeof(AP_HAL::UARTDriver**) ){

            if( (sig & ~CONSOLE_PORT_MASK) == CONSOLE_PORT_SIGNATURE) {
                if(port != (sig & CONSOLE_PORT_MASK)) { // wrong console - reboot needed
                    board_set_rtc_register(CONSOLE_PORT_SIGNATURE | (port & CONSOLE_PORT_MASK), RTC_CONSOLE_REG);
                    REVOMINIScheduler::_reboot(false);
                }
            } else { // no signature - set and check console
                board_set_rtc_register(CONSOLE_PORT_SIGNATURE | (port & CONSOLE_PORT_MASK), RTC_CONSOLE_REG);

                AP_HAL::UARTDriver** up = uarts[port];
                if(up && *up != console) {
    
                    REVOMINIScheduler::_reboot(false);
                }
            }
        }
    }
    {
        uint32_t g   = board_get_rtc_register(RTC_OV_GUARD_REG);
        uint32_t sig = board_get_rtc_register(RTC_OVERCLOCK_REG);
        uint8_t  oc  = hal_param_helper->_overclock;
        
        if(g==OV_GUARD_FAIL_SIGNATURE) {
            if(oc==0) {
                board_set_rtc_register(OVERCLOCK_SIGNATURE | oc, RTC_OVERCLOCK_REG); // set default clock
                board_set_rtc_register(0, RTC_OV_GUARD_REG);                    // and reset failure
            } else printf("\noverclocking failed!\n");            
        } else {

            if((sig & ~OVERCLOCK_SIG_MASK ) == OVERCLOCK_SIGNATURE ) { // if correct signature
                board_set_rtc_register(OVERCLOCK_SIGNATURE | oc, RTC_OVERCLOCK_REG); // set required clock in any case
                if((sig & OVERCLOCK_SIG_MASK) != oc) {                 // if wrong clock
                    REVOMINIScheduler::_reboot(false);                 //  then reboot required
                } 
            } else { // no signature, write only if needed
                if(oc) board_set_rtc_register(OVERCLOCK_SIGNATURE | oc, RTC_OVERCLOCK_REG); // set required clock
            }
        }
    }
    
    { // one-time connection to COM-port
        uint8_t conn = hal_param_helper->_connect_com;
        if(conn) {
            hal_param_helper->_connect_com = 0;
            hal_param_helper->_connect_com.save();
            
            if(conn < sizeof(uarts)/sizeof(AP_HAL::UARTDriver**) ){
                AP_HAL::UARTDriver** up = uarts[conn];
                if(up && *up){
                    connect_uart(uartA,*up, NULL);
                }
            }
    
        }
    }

#if defined(USB_MASSSTORAGE)
    if(!state.sd_busy){    
        uint8_t conn=hal_param_helper->_usb_storage;
        
        if(conn){
            hal_param_helper->_usb_storage=0;
            hal_param_helper->_usb_storage.save();

            board_set_rtc_register(MASS_STORAGE_SIGNATURE, RTC_MASS_STORAGE_REG);
            REVOMINIScheduler::_reboot(false);
        }
    }
#endif

#ifdef USE_SERIAL_4WAY_BLHELI_INTERFACE
    { // one-time connection to ESC
        uint8_t conn = hal_param_helper->_connect_esc;

        if(conn){
            hal_param_helper->_connect_esc = 0;
            hal_param_helper->_connect_esc.save();

            conn-=1;

            if(conn < sizeof(uarts)/sizeof(AP_HAL::UARTDriver**) ){
                AP_HAL::UARTDriver** up = uarts[conn];
                if(up && *up){
                    REVOMINIRCOutput::do_4way_if(*up);
                }
            }
        }
    }
#endif

#if USE_WAYBACK == ENABLED && defined(WAYBACK_DEBUG)
    {
        uint8_t dbg = hal_param_helper->_dbg_wayback;
        if(dbg){
                    
            dbg -=1;

            if(dbg < sizeof(uarts)/sizeof(AP_HAL::UARTDriver**) ){
                AP_HAL::UARTDriver** up = uarts[dbg];
                if(up && *up){        
                    uart = *up;
            
                    AP_WayBack track;
                    REVOMINIScheduler::_delay(5000);
            
                    track.set_debug_mode(true);
                    track.init();
                    track.start();
                    
                    uart->begin(115200);
    
                    uart->println("send pairs 'lat,lon'");
                    uart->println("send G to get point");

                    uart->println("send S to show track point");            
            
                    char buffer[SERIAL_BUFSIZE];
                    float x,y;
                    char *bp=buffer;
                    uint16_t i=0;
            
                    while(1){
                        getSerialLine(buffer);

                        if(buffer[1]==0) {
                            switch(buffer[0]){
                            case 'G': // return by track
                                // get point
    
                                track.stop();

                                while(track.get_point(x,y)){
                                    uart->print(x);
                                    uart->print(",");
                                    uart->println(y);
                                }
                                uart->println(".");
                                break;
                                
                            case 'c':
                            case 'C':
                                hal_param_helper->_dbg_wayback = 0;
                                hal_param_helper->_dbg_wayback.save();
                                goto done;
                                
                                
                            case 'R': // Reset
                                track.stop();
                                track.end();
                                track.init();
                                track.start();
                                break;
                            
                            case 'S': // show current state
                                i=0;
                                while(true){
                                    uint16_t k=i;
                                    if(!track.show_track(i, x, y )) break;
                                    uart->print(k);
                                    uart->print(",");
                                    uart->print(x);
                                    uart->print(",");
                                    uart->println(y);
                                }
                                uart->println(".");
                                break;
                            case 'h':
                            case 'H':
                                uart->println("send pairs 'lat,lon'");
                                uart->println("send G to get point");

                                uart->println("send S to show track point");            
                                uart->println("send C to exit this mode");            
                                break;
                        
                            }
                        } else {
                            // given a point - "x,y"
                            bp=buffer;

                            while(*bp) {
                                if(*bp++ == ',') break;
                            }
                            x=atof(buffer);
                            y=atof(bp);
        
                            uint32_t t=AP_HAL::micros();

                            track.add_point(x,y);
                            t=AP_HAL::micros() - t;

                            uart->print("# time=");
                            uart->println(t);
                        
                        }
                    }
                }
            }

        }
        
    }
done:
#endif

    REVOMINIRCOutput::lateInit(); // 2nd stage - now with loaded parameters

    // all another parameter-dependent inits

#ifdef BOARD_I2C_FLEXI
    if(hal_param_helper->_flexi_i2c) {
        uart3Driver.disable(); // uses Flexi port occupied by I2C 
    }
#endif
    REVOI2CDevice::lateInit();

    REVOMINIStorage::late_init(hal_param_helper->_eeprom_deferred); 
    
    uint8_t flags=0;

    if(hal_param_helper->_rc_fs)      flags |= BOARD_RC_FAILSAFE;

    REVOMINIRCInput::late_init(flags);
}

// 57600 gives ~6500 chars per second or 6 chars per ms
void HAL_REVOMINI::connect_uart(AP_HAL::UARTDriver* uartL,AP_HAL::UARTDriver* uartR, AP_HAL::Proc proc){
    while(1){
        bool got=false;
        if(uartL->available()) { uartR->write(uartL->read()); got=true; }
        if(uartR->available()) { uartL->write(uartR->read()); got=true; }
        if(proc) proc();
        if(!got) REVOMINIScheduler::yield(300); // give a chance to other threads
        if(state.disconnect) break;
    }
}

static const HAL_REVOMINI hal_revo;

const AP_HAL::HAL& AP_HAL::get_HAL() {
//    static const HAL_REVOMINI hal_revo;
    return hal_revo;
}


extern "C" void usb_mass_mal_USBdisconnect();

void usb_mass_mal_USBdisconnect(){ 
    HAL_REVOMINI::state.sd_busy=false;

}

#endif
