/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * I2CDriver.cpp --- AP_HAL_REVOMINI I2C driver.
 *
 * Copyright (C) 2013, Virtualrobotix.com Roberto Navoni , Emile 
 * All Rights Reserved.R Written by Roberto Navoni  <info@virtualrobotix.com>, 11 January 2013
 */
#pragma GCC optimize ("O2")

#include <AP_HAL/AP_HAL.h>
#include <AP_Param_Helper/AP_Param_Helper.h>

#include "I2CDevice.h"
#include <i2c.h>

using namespace REVOMINI;

extern const AP_HAL::HAL& hal;

REVOMINI::Semaphore REVOI2CDevice::_semaphores[3]; // 2 HW and 1 SW

const timer_dev * REVOI2CDevice::_timers[3] = { // one timer per bus for all devices
    TIMER9,
    TIMER10,
    TIMER4,
};

bool REVOI2CDevice::lateInitDone=false;


REVOI2CDevice * REVOI2CDevice::devices[MAX_I2C_DEVICES]; // links to all created devices
uint8_t REVOI2CDevice::dev_count; // number of devices

#ifdef I2C_DEBUG
 I2C_State REVOI2CDevice::log[I2C_LOG_SIZE] IN_CCM;
 uint8_t   REVOI2CDevice::log_ptr=0;

static uint32_t op_time;
static uint32_t op_sr1;

inline uint32_t i2c_get_operation_time(uint16_t *psr1){
    if(psr1) *psr1 = op_sr1;
    return op_time;
}
#endif



void REVOI2CDevice::lateInit(){
    lateInitDone=true;
}


REVOI2CDevice::REVOI2CDevice(uint8_t bus, uint8_t address)
        : _bus(bus)
        , _address(address)
        , _retries(1)
        , _lockup_count(0)
        , _initialized(false)
        , _slow(false)
        , _failed(false)
        , need_reset(false)
        , _dev(NULL)
{


    // store link to created devices
    if(dev_count<MAX_I2C_DEVICES){
        devices[dev_count++] = this; // links to all created devices
    }                    
}

REVOI2CDevice::~REVOI2CDevice() { 
    for(int i=0;i<dev_count;i++){
        if(devices[i] == this){
            devices[i] = NULL;
        }
    }
}


void REVOI2CDevice::init(){
    if(!lateInitDone) {
        ((HAL_REVOMINI&) hal).lateInit();
    }

    if(need_reset) _do_bus_reset();

    if(_failed) return;

    if(_initialized) return;
    
    const i2c_dev *dev=NULL;

    switch(_bus) {
    case 0:         // this is always internal bus
	    _offs =0;
#if defined(BOARD_I2C_BUS_SLOW) && BOARD_I2C_BUS_SLOW==0
            _slow=true;
#endif

#if defined(BOARD_SOFT_I2C) || defined(BOARD_SOFT_I2C1)
            s_i2c.init_hw( 
                _I2C1->gpio_port, _I2C1->scl_pin,
                _I2C1->gpio_port, _I2C1->sda_pin,
                _timers[_bus]
            );
#else
	    dev = _I2C1;
#endif
	    break;


    case 1:     // flexi port - I2C2
#if !defined(BOARD_HAS_UART3) // in this case I2C on FlexiPort will be bus 2

	    _offs = 2;
 #if defined(BOARD_I2C_BUS_SLOW) && BOARD_I2C_BUS_SLOW==1
            _slow=true;
 #endif

 #if defined(BOARD_SOFT_I2C) || defined(BOARD_SOFT_I2C2)
            s_i2c.init_hw( 
                _I2C2->gpio_port, _I2C2->scl_pin,
                _I2C2->gpio_port, _I2C2->sda_pin,
                _timers[_bus]
            );
 #else
	    dev = _I2C2;
 #endif
	    break;
#else
            return; // not initialized so always returns false

#endif

#if defined(BOARD_SOFT_SCL) && defined(BOARD_SOFT_SDA)
    case 2:         // this bus can use only soft I2C driver
#if defined(BOARD_I2C_BUS_SLOW) && BOARD_I2C_BUS_SLOW==2
            _slow=true;
#endif

#ifdef BOARD_I2C_FLEXI
            if(hal_param_helper->_flexi_i2c){ // move external I2C to flexi port
                s_i2c.init_hw( 
                    _I2C2->gpio_port, _I2C2->scl_pin,
                    _I2C2->gpio_port, _I2C2->sda_pin,
                    _timers[_bus]
                );
            } else 
#endif
            { //                         external I2C on Input port
                s_i2c.init_hw( 
                    PIN_MAP[BOARD_SOFT_SCL].gpio_device,     PIN_MAP[BOARD_SOFT_SCL].gpio_bit,
                    PIN_MAP[BOARD_SOFT_SDA].gpio_device,     PIN_MAP[BOARD_SOFT_SDA].gpio_bit,
                    _timers[_bus]
                );
            }        
            break;
#endif

    default:
            return;
    }
    _dev = dev; // remember

    
    if(_dev) {
//        i2c_init(_dev, _offs, _slow?I2C_125KHz_SPEED:I2C_250KHz_SPEED);
//        i2c_init(_dev, _offs, _slow?I2C_75KHz_SPEED:I2C_250KHz_SPEED);
        i2c_init(_dev, _offs, _slow?I2C_250KHz_SPEED:I2C_400KHz_SPEED);

        // init DMA beforehand    
        dma_init(_dev->dma.stream_rx); 
//        dma_init(_dev->dma.stream_Tx);  not used

    }else {
        s_i2c.init( );

        if(_slow) {
            s_i2c.set_low_speed(true);
        }

    }
    _initialized=true;
}



void REVOI2CDevice::register_completion_callback(Handler h) { 
    if(h && _completion_cb) {// IOC from last call still not called - some error occured so bus reset needed
        _completion_cb=0;
        _do_bus_reset();
    }
    
    _completion_cb=h;

    if(!REVOMINIScheduler::in_interrupt()) { // drivers that calls  register_completion_callback() from interrupt has exclusive bus
        REVOMINIScheduler::set_task_ioc(h!=0);
    }
}
    



bool REVOI2CDevice::transfer(const uint8_t *send, uint32_t send_len, uint8_t *recv, uint32_t recv_len)
{

    uint16_t retries=_retries;
    
again:


    uint32_t ret=0;
    uint8_t last_op=0;

    if(!_initialized) {
        init();
        if(!_initialized) return false;
    }


    if(!_dev){ // no hardware so use soft I2C
            
        if(recv_len) memset(recv, 0, recv_len); // for DEBUG
            
        if(recv_len==0){ // only write
            ret=s_i2c.writeBuffer( _address, send_len, send );            
        }else if(send_len==1){ // only read - send byte is address
            ret=s_i2c.read(_address, *send, recv_len, recv);                
        } else {
            ret=s_i2c.transfer(_address, send_len, send, recv_len, recv);
        }
            
        if(ret == I2C_NO_DEVICE) 
            return false;

        if(ret == I2C_OK) 
            return true;

        if((_retries-retries) > 0) { // don't reset and count for fixed at 2nd try errors
            _lockup_count ++;          
            last_error = ret;  
              
            if(!s_i2c.bus_reset()) return false;    
        }

        if(retries--) goto again;

        return false;
    } // software I2C

// Hardware
#ifdef I2C_DEBUG
    {
     I2C_State &sp = log[log_ptr]; // remember last operation
     sp.st_sr1      = _dev->I2Cx->SR1;
     sp.st_sr2      = _dev->I2Cx->SR2;
     }
#endif

    if(recv_len==0) { // only write
        last_op=1;
        ret = i2c_write(_address, send, send_len);
    } else {
        last_op=0;
        ret = i2c_read( _address, send, send_len, recv, recv_len);
    }


#ifdef I2C_DEBUG
     I2C_State &sp = log[log_ptr]; // remember last operation
     
     sp.start    = i2c_get_operation_time(&sp.op_sr1);
     sp.time     = REVOMINIScheduler::_micros();
     sp.bus      =_bus;
     sp.addr     =_address;
     sp.send_len = send_len;
     sp.recv_len = recv_len;
     sp.ret      = ret;
     sp.sr1      = _dev->I2Cx->SR1;
     sp.sr2      = _dev->I2Cx->SR2;
     if(log_ptr<I2C_LOG_SIZE-1) log_ptr++;
     else                       log_ptr=0;
#endif


    if(ret == I2C_PENDING) return true; // DMA transfer with callback

    if(ret == I2C_OK) return true;

// something went wrong and completion callback never will be called, so release bus semaphore
    if(_completion_cb)  {
        _completion_cb = 0;     // to prevent 2nd bus reset
        register_completion_callback((Handler)0);
    }

    if(ret == I2C_ERR_STOP || ret == I2C_STOP_BERR || ret == I2C_STOP_BUSY) { // bus or another errors on Stop, or bus busy after Stop.
                                                                            //   Data is good but bus reset required
        need_reset = true;
        _initialized=false; // will be reinitialized at next transfer

        _dev->I2Cx->CR1 |= I2C_CR1_SWRST; // set for some time
        
        // we not count such errors as _lockup_count
    
        Revo_handler h = { .mp=FUNCTOR_BIND_MEMBER(&REVOI2CDevice::do_bus_reset, void) }; // schedule reset as io_task
        REVOMINIScheduler::_register_io_process(h.h, IO_ONCE); 
        
        return true; // data is OK
    } 
    
    if(ret != I2C_NO_DEVICE) { // for all errors except NO_DEVICE do bus reset

        if(ret == I2C_BUS_BUSY) {
            _dev->I2Cx->CR1 |= I2C_CR1_SWRST;           // set SoftReset for some time 
            hal_yield(0);
            _dev->I2Cx->CR1 &= (uint16_t)(~I2C_CR1_SWRST); // clear SoftReset flag            
        }

        if((_retries-retries) > 0 || ret==I2C_BUS_ERR){ // not reset bus or log error on 1st try, except ArbitrationLost error
            last_error = ret;   // remember
            if(last_op) last_error+=50; // to distinguish read and write errors
            _lockup_count ++;  
            _initialized=false; // will be reinitialized at next transfer
        
            _do_bus_reset();
        
            if(_failed) return false;
        }
    }

    if(retries--) goto again;

    return false;
}

void REVOI2CDevice::_io_cb(){

}

void REVOI2CDevice::do_bus_reset(){ // public - with semaphores
    if(_semaphores[_bus].take(HAL_SEMAPHORE_BLOCK_FOREVER)){
        _do_bus_reset();
        _semaphores[_bus].give();
    }
}

void REVOI2CDevice::_do_bus_reset(){ // private

    _dev->I2Cx->CR1 &= (uint16_t)(~I2C_CR1_SWRST); // clear soft reset flag

    if(!need_reset) return; // already done
    
    i2c_deinit(_dev); // disable I2C hardware
    if(!i2c_bus_reset(_dev)) {
        _failed = true;         // can't do it in limited time
    }
    need_reset = false; // done
}


bool REVOI2CDevice::read_registers_multiple(uint8_t first_reg, uint8_t *recv,
                                 uint32_t recv_len, uint8_t times){

    while(times--) {
	bool ret = read_registers(first_reg, recv, recv_len);
	if(!ret) return false;
	recv += recv_len;
    }

    return true;
}


enum I2C_state {
    I2C_want_SB,
    I2C_want_ADDR,
    I2C_want_TXE,
    I2C_want_RX_SB,
    I2C_want_RX_ADDR,
    I2C_want_RXNE,
    I2C_done
} ;


/*
    moved from low layer to be properly integrated to multitask

*/
#pragma GCC optimize ("Og")


//#define I2C_DMA_SEND // not works well

/* Send a buffer to the i2c port */
uint32_t REVOI2CDevice::i2c_write(uint8_t addr, const uint8_t *tx_buff, uint8_t len) {

    uint32_t ret = wait_stop_done(true);
    if(ret!=I2C_OK) return ret;

    _addr=addr;
    _tx_buff=tx_buff;
    _tx_len=len;
    _rx_len=0; // only write

            
    i2c_set_isr_handler(_dev, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&REVOI2CDevice::isr_ev, void)));

    // Bus got!  enable Acknowledge for our operation
    _dev->I2Cx->CR1 |= I2C_CR1_ACK; 
    _dev->I2Cx->CR1 &= ~I2C_NACKPosition_Next; 

    _state = I2C_want_SB;
    _error = I2C_ERR_TIMEOUT;

    // Send START condition
    _dev->I2Cx->CR1 |= I2C_CR1_START;

    _dev->I2Cx->CR2 |= I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;    // Enable interrupts


    if(_completion_cb) return I2C_PENDING;
        
    uint32_t timeout = i2c_bit_time * 9 * (len+1) * 3; // time to transfer all data *3


    // need to wait until  transfer complete 
    uint32_t t = hal_micros();
    if(!REVOMINIScheduler::in_interrupt()) { // if function called from task - store it and pause
        _task = REVOMINIScheduler::get_current_task();
        REVOMINIScheduler::task_pause(timeout);
    } else {
        _task=0;
    }
    while (hal_micros() - t < timeout) {
        if(_error!=I2C_ERR_TIMEOUT) break; // error occures
        
        hal_yield(0);
    }

    if(_error==I2C_ERR_TIMEOUT) finish_transfer();                        

    return _error;
        
}

uint32_t REVOI2CDevice::i2c_read(uint8_t addr, const uint8_t *tx_buff, uint8_t txlen, uint8_t *rx_buff, uint8_t rxlen)
{
    uint8_t *dma_rx;
    
    dma_mode = 0; //(rxlen < DMA_BUFSIZE) || ADDRESS_IN_RAM(rx_buff);

    // in case of DMA transfer
    dma_stream rx_stream = _dev->dma.stream_rx;

    uint32_t ret = wait_stop_done(false); // wait for bus release from previous transfer and force it if needed
    if(ret!=I2C_OK) return ret;

    _addr=addr;
    _tx_buff=tx_buff;
    _tx_len=txlen;
    _rx_buff=rx_buff;
    _rx_len=rxlen;

            
    i2c_set_isr_handler(_dev, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&REVOI2CDevice::isr_ev, void)));


    uint32_t t;

    if(dma_mode) {
        //  проверить, не занят ли поток DMA перед использованием
        t = hal_micros();
        while(dma_is_stream_enabled(rx_stream)/* || dma_is_stream_enabled(_dev->dma.stream_tx)*/ ) {
            // wait for transfer termination
            if (hal_micros() - t > I2C_TIMEOUT) {
                dma_disable(rx_stream);  // something went wrong so let it get stopped
                dma_disable(_dev->dma.stream_tx);
                break;                    // DMA stream grabbed
            }
            hal_yield(0); 
        }

        if(ADDRESS_IN_RAM(rx_buff)){
            dma_rx = rx_buff;
            _dev->state->len=0; // clear need to memmove            
        } else {
            dma_rx = _dev->state->buff;
            _dev->state->len=rxlen; // need to memmove
            _dev->state->dst=rx_buff;
        }

        // init DMA beforehand    
        dma_init(rx_stream); 
        dma_clear_isr_bits(rx_stream); 

        DMA_InitTypeDef DMA_InitStructure;
        DMA_StructInit(&DMA_InitStructure);
    
        DMA_InitStructure.DMA_Channel               = _dev->dma.channel;
        DMA_InitStructure.DMA_Memory0BaseAddr       = (uint32_t)dma_rx;
        DMA_InitStructure.DMA_BufferSize            = rxlen;
        DMA_InitStructure.DMA_PeripheralBaseAddr    = (uint32_t)(&(_dev->I2Cx->DR));
        DMA_InitStructure.DMA_PeripheralDataSize    = DMA_PeripheralDataSize_Byte;
        DMA_InitStructure.DMA_MemoryDataSize        = DMA_MemoryDataSize_Byte;
        DMA_InitStructure.DMA_PeripheralInc         = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc             = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_Mode                  = DMA_Mode_Normal;
        DMA_InitStructure.DMA_Priority              = DMA_Priority_High;
        DMA_InitStructure.DMA_FIFOMode              = DMA_FIFOMode_Disable;
        DMA_InitStructure.DMA_FIFOThreshold         = DMA_FIFOThreshold_Full;
        DMA_InitStructure.DMA_MemoryBurst           = DMA_MemoryBurst_Single;
        DMA_InitStructure.DMA_PeripheralBurst       = DMA_PeripheralBurst_Single;
        DMA_InitStructure.DMA_DIR                   = DMA_DIR_PeripheralToMemory;
        
        dma_init_transfer(rx_stream, &DMA_InitStructure);

        dma_attach_interrupt(rx_stream, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&REVOI2CDevice::isr_ioc, void)), DMA_CR_TCIE); 

        dma_enable(rx_stream);        
    } // DMA mode

    _state = I2C_want_SB;
    _error = I2C_ERR_TIMEOUT;

    _dev->I2Cx->CR1 &= ~I2C_NACKPosition_Next; // I2C_NACKPosition_Current
    _dev->I2Cx->CR1 |= I2C_CR1_ACK;      // Bus got!  enable Acknowledge for our operation
    
    _dev->I2Cx->CR1 |= I2C_CR1_START;    // Send START condition

    _dev->I2Cx->CR2 |= I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;    // Enable interrupts

    if(_completion_cb) return I2C_PENDING;
        
    uint32_t timeout = i2c_bit_time * 9 * (txlen+rxlen) * 3; // time to transfer all data *3
        
    t = hal_micros();
    // need to wait until DMA transfer complete */
    if(!REVOMINIScheduler::in_interrupt()) { // if function called from task - store it and pause
        _task = REVOMINIScheduler::get_current_task();
        REVOMINIScheduler::task_pause(timeout);
    } else {
        _task=0;
    }
    while (hal_micros() - t < timeout) {
        if(_error!=I2C_ERR_TIMEOUT) break; // error occures
        
        hal_yield(0);
    }

    if(_error==I2C_ERR_TIMEOUT) finish_transfer();                        

    return _error;
}

void REVOI2CDevice::isr_ev(){
    bool err;

    asm volatile("MOV     %0, r1\n\t"  : "=rm" (err));

    uint32_t sr1itflags = _dev->I2Cx->SR1;
    uint32_t itsources  = _dev->I2Cx->CR2;

    if(err){

        /* I2C Bus error interrupt occurred ----------------------------------------*/
        if(((sr1itflags & I2C_FLAG_BERR) != RESET) && ((itsources & I2C_IT_ERR) != RESET)) {    /* Clear BERR flag */
          _dev->I2Cx->SR1 = (uint16_t)(~I2C_SR1_BERR); // Errata 2.4.6
        }
  
        /* I2C Arbitration Loss error interrupt occurred ---------------------------*/
        if(((sr1itflags & I2C_FLAG_ARLO) != RESET) && ((itsources & I2C_IT_ERR) != RESET)) {
          _error = I2C_BUS_ERR;
    
          /* Clear ARLO flag */
          _dev->I2Cx->SR1 = (uint16_t)(~I2C_SR1_ARLO); // reset them
        }
  
        /* I2C Acknowledge failure error interrupt occurred ------------------------*/
        if(((sr1itflags & I2C_FLAG_AF) != RESET) && ((itsources & I2C_IT_ERR) != RESET))  {
            /* Clear AF flag */
            _dev->I2Cx->SR1 = (uint16_t)(~I2C_SR1_AF); // reset it

            if(_state == I2C_want_ADDR) { // address transfer
                _error = I2C_NO_DEVICE;
            } else if(_state == I2C_want_RX_ADDR) { // restart
                _error = I2C_ERR_REGISTER;
            } else {
                _error = I2C_ERROR;
            }
    
            _dev->I2Cx->CR1 |= I2C_CR1_STOP;          /* Generate Stop */      
        }
  
        /* I2C Over-Run/Under-Run interrupt occurred -------------------------------*/
        if(((sr1itflags & I2C_FLAG_OVR) != RESET) && ((itsources & I2C_IT_ERR) != RESET)) {
           _error = I2C_ERR_OVERRUN;
           _dev->I2Cx->SR1 = (uint16_t)(~I2C_SR1_OVR); // reset it
        }
  
        if(_error) { // смысла ждать больше нет
            finish_transfer();
        }    
    }else{
        uint32_t sr2itflags   = _dev->I2Cx->SR2;

        /* SB Set ----------------------------------------------------------------*/
        if(((sr1itflags & I2C_FLAG_SB & FLAG_MASK) != RESET) && ((itsources & I2C_IT_EVT) != RESET))    {
            // Send address for write
            if(_tx_len){
                I2C_Send7bitAddress(_dev->I2Cx, _addr<<1, I2C_Direction_Transmitter);
                _state = I2C_want_ADDR;
            } else {
                I2C_Send7bitAddress(_dev->I2Cx, _addr<<1, I2C_Direction_Receiver);
                _state = I2C_want_RX_ADDR;
            }

            _dev->I2Cx->CR1 &= (uint16_t)(~I2C_CR1_STOP);    /* clear STOP condition - just to touch CR1*/
        }
        /* ADDR Set --------------------------------------------------------------*/
        else if(((sr1itflags & I2C_FLAG_ADDR & FLAG_MASK) != RESET) && ((itsources & I2C_IT_EVT) != RESET))    {
            /* Clear ADDR register by reading SR1 then SR2 register (SR1 and SR2 has already been read) */
        
            if(_tx_len) { // transmit
                // all flags set before
                _state = I2C_want_TXE;
            }else {      // receive
                if(dma_mode) {
                    if(_rx_len == 1) {                 // Disable Acknowledge 
                        _dev->I2Cx->CR1 &= ~I2C_CR1_ACK;
                        _dev->I2Cx->CR2 &= ~I2C_CR2_LAST; // disable DMA generated NACK
                    } else if(_rx_len == 2) {              // Disable Acknowledge and change NACK position
                        _dev->I2Cx->CR1 |= I2C_NACKPosition_Next; // move NACK to next byte
                        _dev->I2Cx->CR1 &= ~I2C_CR1_ACK;
                        _dev->I2Cx->CR2 &= ~I2C_CR2_LAST; // disable DMA generated NACK
                    } else {                             // Enable ACK and Last DMA bit
                        _dev->I2Cx->CR1 |= I2C_CR1_ACK;
                        _dev->I2Cx->CR2 |= I2C_CR2_LAST;  
                    }
                    _dev->I2Cx->CR2 &= ~I2C_CR2_ITBUFEN;  // disable interrupt by TXE/RXNE
                    _dev->I2Cx->CR2 |=  I2C_CR2_DMAEN;    // Enable I2C RX request - all reads will be in DMA mode
                } else {
                    if(_rx_len == 1) {                 // Disable Acknowledge 
                        _dev->I2Cx->CR1 &= ~I2C_CR1_ACK;
                    } else if(_rx_len == 2) {              // Disable Acknowledge and change NACK position
                        _dev->I2Cx->CR1 |= I2C_NACKPosition_Next; // move NACK to next byte
                        _dev->I2Cx->CR1 &= ~I2C_CR1_ACK;
                    } else {
                        _dev->I2Cx->CR1 |= I2C_CR1_ACK;
                    }
                }
                _state = I2C_want_RXNE;
            }        
        }
    
    
        if((itsources & I2C_IT_BUF) != RESET ){ // data io

            if((sr1itflags & I2C_FLAG_TXE & FLAG_MASK) != RESET) {// TXE set 
                if((sr2itflags & (I2C_FLAG_TRA>>16) & FLAG_MASK) != RESET) {    // I2C in mode Transmitter

                    if(_tx_len) {
                        _dev->I2Cx->DR = *_tx_buff++; // 1 byte
                        _tx_len--;
                    } else { // tx is over and last byte is sent
                        if(_rx_len) {
                            // Send START condition a second time
                            _dev->I2Cx->CR1 |= I2C_CR1_START;
                            _state = I2C_want_RX_SB;
                        } else {   
                            _dev->I2Cx->CR1 |= I2C_CR1_STOP;     /* Send STOP condition */
                            _error = I2C_OK;
                            _state = I2C_done;
                            finish_transfer();
                        }
                    }        
                }
            } 
            if((sr1itflags & I2C_FLAG_BTF & FLAG_MASK) != RESET) {// TXE set 
                if((sr2itflags & (I2C_FLAG_TRA>>16) & FLAG_MASK) != RESET) {    // I2C in mode Transmitter
                    // BTF on transmit
                } else { // BTF on receive
                    // 
                }
            }
            if(((sr1itflags & I2C_FLAG_RXNE & FLAG_MASK) != RESET))   {       // RXNE set
                 if(dma_mode) { // I2C in mode Receiver 
                    (void)_dev->I2Cx->DR;
                 }else{
                    if(_rx_len && !_tx_len) {
                        *_rx_buff++ = (uint8_t)(_dev->I2Cx->DR);
                        _rx_len -= 1; // 1 byte done
	        
                        if(_rx_len == 1) { // last second byte
                            _dev->I2Cx->CR1 &= ~I2C_CR1_ACK;     // Disable Acknowledgement - send NACK for last byte 
                            _dev->I2Cx->CR1 |= I2C_CR1_STOP;     // Send STOP
                        } else if(_rx_len==0) {
                            _error = I2C_OK;
                            _state = I2C_done;

                            finish_transfer();
                        }
                    } else { // fake byte after enable ITBUF
                        (void)_dev->I2Cx->DR;
                    }
                }
            }
        }
    }
}


void REVOI2CDevice::finish_transfer(){

    _dev->I2Cx->CR2 &= ~(I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN | I2C_CR2_ITERREN);    // Disable interrupts
    i2c_clear_isr_handler(_dev);

    if(dma_get_isr_bits(_dev->dma.stream_rx) & DMA_FLAG_TCIF) { // was receive
        dma_disable(_dev->dma.stream_rx);

        _dev->I2Cx->CR2 &= ~(I2C_CR2_LAST | I2C_CR2_DMAEN);        /* Disable I2C DMA request */

        dma_clear_isr_bits(_dev->dma.stream_rx); 

        // transfer done!
        dma_detach_interrupt(_dev->dma.stream_rx);    
        
        if(_dev->state->len){    // we need to memmove
            memmove(_dev->state->dst, _dev->state->buff, _dev->state->len);
        }
    }
    
    Handler h;
    if( (h=_completion_cb) ){    // io completion
        
        _completion_cb=0; // only once and before call because handler can set it itself
        
        revo_call_handler(h, (uint32_t)_dev);
    }
    
    if(_task){ // resume paused task
        REVOMINIScheduler::task_resume(_task);
        _task=NULL;
    }    
}

void REVOI2CDevice::isr_ioc(){
    _dev->I2Cx->CR1 |= I2C_CR1_STOP;     /* Send STOP condition */
    _error = I2C_OK;
    _state = I2C_done;
    _rx_len = 0; // no need to receive
    finish_transfer();
}

uint32_t REVOI2CDevice::wait_stop_done(bool is_write){
    uint32_t sr1;
    uint32_t t;

    uint8_t ret;
    uint8_t i;
    for(i=0; i<10; i++){

        ret=I2C_OK;
       // Wait to make sure that STOP control bit has been cleared - bus released
        t = hal_micros();
        while (_dev->I2Cx->CR1 & I2C_CR1_STOP ){
            if((sr1=_dev->I2Cx->SR1) & I2C_FLAG_BERR & FLAG_MASK) _dev->I2Cx->SR1 = (uint16_t)(~I2C_SR1_BERR); // Errata 2.4.6

            if(sr1 & I2C_FLAG_ARLO & FLAG_MASK) { // arbitration lost or bus error
                _dev->I2Cx->SR1 = (uint16_t)(~I2C_SR1_ARLO); // reset them
                ret= I2C_STOP_BERR; // bus error on STOP
                break;
            }
            if(sr1 & I2C_FLAG_TIMEOUT & FLAG_MASK) { // bus timeout
                _dev->I2Cx->SR1 = (uint16_t)(~I2C_FLAG_TIMEOUT); // reset it
                ret= I2C_ERR_TIMEOUT;                             // STOP generated by hardware
                break;
            }

            if (hal_micros() - t > I2C_SMALL_TIMEOUT) {
                ret=I2C_ERR_STOP;
                break;
            }
        }

        /* wait while the bus is busy */
        t = hal_micros();
        while ((_dev->I2Cx->SR2 & (I2C_FLAG_BUSY>>16) & FLAG_MASK) != 0) {
            if (hal_micros() - t > I2C_SMALL_TIMEOUT) {
                ret=2; // bus busy
                break;
            }
	
            hal_yield(0); 
        }


        if(ret==I2C_OK) return ret;
        
        if(i>0){
            _dev->I2Cx->CR1 |= I2C_CR1_SWRST;           // set SoftReset for some time 
            hal_yield(0);
            _dev->I2Cx->CR1 &= (uint16_t)(~I2C_CR1_SWRST); // clear SoftReset flag                    
        }

        if(i>1){
            last_error = ret;   // remember
            if(is_write) last_error+=50;

            _lockup_count ++;  
            _initialized=false; // will be reinitialized in init()
        
            need_reset=true;
            init();
            
            if(!_initialized) return ret;
        
        }

    }

    return I2C_OK;
}



/*
    errata 2.4.6
Spurious Bus Error detection in Master mode
Description
In Master mode, a bus error can be detected by mistake, so the BERR flag can be wrongly
raised in the status register. This will generate a spurious Bus Error interrupt if the interrupt
is enabled. A bus error detection has no effect on the transfer in Master mode, therefore the
I2C transfer can continue normally.
Workaround
If a bus error interrupt is generated in Master mode, the BERR flag must be cleared by
software. No other action is required and the on-going transfer can be handled normally

*/
