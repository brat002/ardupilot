/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#pragma GCC push_options

#pragma GCC optimize ("O2")

#include <AP_HAL/AP_HAL.h>


#include <spi.h>
#include "Semaphores.h"

#include <inttypes.h>
#include <vector>
#include <AP_HAL/HAL.h>
#include <AP_HAL/SPIDevice.h>

#include "Scheduler.h"
#include <spi.h>
#include <boards.h>

#include "SPIDevice.h"
#include "GPIO.h"


using namespace REVOMINI;


static const SPIDesc spi_device_table[] = {    // different SPI tables per board subtype
    BOARD_SPI_DEVICES
};

#define REVOMINI_SPI_DEVICE_NUM_DEVICES (sizeof(spi_device_table)/sizeof(SPIDesc))

static const spi_pins board_spi_pins[] = {
    { // 0
        BOARD_SPI1_SCK_PIN,
        BOARD_SPI1_MISO_PIN,
        BOARD_SPI1_MOSI_PIN
    },
    { // 1
        BOARD_SPI2_SCK_PIN,
        BOARD_SPI2_MISO_PIN,
        BOARD_SPI2_MOSI_PIN
    },
    { //2
        BOARD_SPI3_SCK_PIN,
        BOARD_SPI3_MISO_PIN,
        BOARD_SPI3_MOSI_PIN
    }
};


REVOMINI::Semaphore SPIDevice::_semaphores[MAX_BUS_NUM] IN_CCM; // per bus data
void *              SPIDevice::owner[MAX_BUS_NUM] IN_CCM;
uint8_t             SPIDevice::buffer[MAX_BUS_NUM][SPI_BUFFER_SIZE]; // DMA buffer in RAM

#ifdef DEBUG_SPI 
struct spi_trans SPIDevice::spi_trans_array[SPI_LOG_SIZE]  IN_CCM;
uint8_t SPIDevice::spi_trans_ptr=0;
#endif


#ifdef BOARD_SOFTWARE_SPI

#define SCK_H       {sck_port->BSRRL = sck_pin; }
#define SCK_L       {sck_port->BSRRH = sck_pin; }

#define MOSI_H      {mosi_port->BSRRL = mosi_pin; }
#define MOSI_L      {mosi_port->BSRRH = mosi_pin; }

#define MISO_read   ((miso_port->IDR & miso_pin)!=0)


static void dly_spi() {
    delay_ns100(dly_time);
};


uint8_t SPIDevice::_transfer_s(uint8_t bt) {

    for(int ii = 0; ii < 8; ++ii) {
        if (bt & 0x80) {
            MOSI_H;
        } else {
            MOSI_L;
        }
        SCK_L;

        dly_spi();
        SCK_H;
         
        bt <<= 1;
        if (MISO_read) {
            bt |= 1;
        }
        dly_spi();
    }

    return bt;
}
#endif

void SPIDevice::register_completion_callback(Handler h) { 
    if(_completion_cb){ // IOC from last call still not called
        // TODO: ???
    }
    _completion_cb = h; 
}


uint8_t SPIDevice::transfer(uint8_t out){
#ifdef BOARD_SOFTWARE_SPI
    if(_desc.soft) {
        return _transfer_s(out);
    } else 
#endif
    {
        return _transfer(out);
    }
    
}


uint8_t SPIDevice::_transfer(uint8_t data) {

    //wait for TXE before send
    while (!spi_is_tx_empty(_desc.dev)) {    // надо дожидаться окончания передачи.
        if(!spi_is_busy(_desc.dev) ) break;
    }

    //write 1byte
    spi_tx_reg(_desc.dev, data); //    _desc.dev->SPIx->DR = data;

    //read one byte
    while (!spi_is_rx_nonempty(_desc.dev)) {    // надо дожидаться окончания передачи.
        if(!spi_is_busy(_desc.dev) ) break;
    }
    
    return (uint8_t)spi_rx_reg(_desc.dev);
}

void SPIDevice::send(uint8_t out) {
    //wait for TXE before send
    while (!spi_is_tx_empty(_desc.dev)) {    // надо дожидаться окончания передачи.
        if(!spi_is_busy(_desc.dev) ) break;
    }
    //write 1byte
    spi_tx_reg(_desc.dev, out); //    _desc.dev->SPIx->DR = data;
}


bool SPIDevice::transfer(const uint8_t *out, uint32_t send_len, uint8_t *recv, uint32_t recv_len){
    
    uint8_t ret=1;
    bool was_dma=false;

// differrent devices on bus requires different modes
    if(owner[_desc.bus-1] != this) { // bus was in use by another driver so need reinit
        _initialized=false;
    }
    
    if(!_initialized){
        init();
        if(!_initialized) goto done;
        owner[_desc.bus-1] = this; // Got it!
    }

#ifdef BOARD_SOFTWARE_SPI
    if(_desc.soft) {
        uint16_t rate;

        switch(_speed) {
        case SPI_36MHZ:
        case SPI_18MHZ:
        case SPI_9MHZ:
        case SPI_4_5MHZ:
            rate = 1;
            break;
        case SPI_2_25MHZ:
            rate = 2;
	    break;
        case SPI_1_125MHZ:
	    rate = 4; // 400 ns delay
	    break;
        case SPI_562_500KHZ:
	    rate = 8;
	    break;
        case SPI_281_250KHZ:
	    rate = 16;
	    break;
        case SPI_140_625KHZ:
        default:
	    rate = 32;
	    break;
        }
        _cs_assert();

        dly_time = rate; 

        if (out != NULL && send_len) {
            for (uint16_t i = 0; i < send_len; i++) {
                _transfer_s(out[i]);
            }    
        } 
    
        if(recv !=NULL && recv_len) {
            for (uint16_t i = 0; i < recv_len; i++) {
                recv[i] = _transfer_s(0);
            }
        }
    } else 
#endif
    {
    
        uint32_t t = hal_micros();
        while(_desc.dev->state->busy){ //       wait for previous transfer finished
            if(hal_micros() - t > 2000) break; // SPI transfer can't be so long so let grab the bus
            hal_yield(0);
        }
        
        spi_set_speed(_desc.dev, determine_baud_rate(_speed));
        _desc.dev->state->busy = true; // we got bus for bus

#define MIN_DMA_BYTES 3 // write to 2-byte register not uses DMA

        _cs_assert();

        switch(_desc.mode){
        case 3: // interrupts
            break;
            
        case 1: // DMA
        case 2: // only DMA
            if(send_len){
                if((send_len >= MIN_DMA_BYTES || _desc.mode==2)){ // long enough 
                    _desc.dev->state->len=0;

                    if(ADDRESS_IN_RAM(out)){   // not in CCM
                        ret=dma_transfer(out, NULL, send_len);
                        was_dma=true;
                    } else {
                        if(send_len<=SPI_BUFFER_SIZE){
                            uint8_t *buf = &buffer[_desc.bus-1][0];
                            memmove(buf,out,send_len);
                            ret=dma_transfer(buf, NULL, send_len);
                            was_dma=true;
                        }
                    }
                } 
                
                if(!was_dma) {
                    ret=spimaster_transfer(_desc.dev, out, send_len, NULL, 0);
                }
            }
            if(recv_len) {
                if((recv_len>=MIN_DMA_BYTES || _desc.mode==2) ) { // long enough or always DMA
                    if(ADDRESS_IN_RAM(recv)){   //not in CCM
                        _desc.dev->state->len=0;
                        ret=dma_transfer(NULL, recv, recv_len);
                        was_dma=true;
                    }else {
                        if(send_len<=SPI_BUFFER_SIZE){
                            uint8_t *buf = &buffer[_desc.bus-1][0];
                            
                            _desc.dev->state->len=recv_len;
                            _desc.dev->state->dst=recv;
                            
                            ret=dma_transfer(NULL, buf, recv_len); 
                            was_dma=true;
                        }
                    }
                } 
                if(!was_dma) {
                    ret=spimaster_transfer(_desc.dev, NULL, 0, recv, recv_len);
                }
            }
            break;
        case 0: // polling
            ret = spimaster_transfer(_desc.dev, out, send_len, recv, recv_len);
            break;
        }
    }

#ifdef DEBUG_SPI 
    struct spi_trans &p = spi_trans_array[spi_trans_ptr];

    p.dev      = _desc.dev;
    p.send_len = send_len;
    if(send_len)
      p.sent = out[0];
    else p.sent = 0;
    if(send_len>1)
      p.sent1 = out[1];
    else p.sent1 = 0;
    p.recv_len = recv_len;
    if(recv_len)
      p.recv0 = recv[0];
    else p.recv0 = 0;
    if(recv_len>1)
      p.recv1 = recv[1];
    else p.recv1 = 0;
    
    spi_trans_ptr++;
    if(spi_trans_ptr>=SPI_LOG_SIZE) spi_trans_ptr=0;
#endif

done:
    if(was_dma){    
        // nothing to do - all in ISR
    } else {
        _cs_release();
        _desc.dev->state->busy=false;
        if(_completion_cb) {
            revo_call_handler(_completion_cb, (uint32_t)&_desc);
            _completion_cb=0;
        }
    }
    return ret==0;

}

    



bool SPIDevice::transfer_fullduplex(const uint8_t *out, uint8_t *recv, uint32_t len) {

    if(owner[_desc.bus-1] != this) { // bus was in use by another driver so need reinit
        _initialized=false;
        init();
        if(!_initialized) return false;
        owner[_desc.bus-1] = this; // Got it!
    }

    _cs_assert();

#ifdef BOARD_SOFTWARE_SPI
    if(_desc.soft) {
        if (out != NULL && recv !=NULL && len) {
            for (uint16_t i = 0; i < len; i++) {
                recv[i] = _transfer_s(out[i]);
            }    
        } 
    } else 
#endif
    {
        spi_set_speed(_desc.dev, determine_baud_rate(_speed)); //- on cs_assert()
        
        if(_desc.mode && (out==NULL || ADDRESS_IN_RAM(out)) && (recv==NULL || ADDRESS_IN_RAM(recv)) ) {
            dma_transfer(out, recv, len);
        } else {
            if (out != NULL && recv !=NULL && len) {
                for (uint16_t i = 0; i < len; i++) {
                    recv[i] = _transfer(out[i]);
                }    
            }
        } 
    }
    _cs_release();
    return true;
}


AP_HAL::OwnPtr<REVOMINI::SPIDevice>
SPIDeviceManager::_get_device(const char *name)
{
    const SPIDesc *desc = nullptr;
    
    /* Find the bus description in the table */
    for (uint8_t i = 0; i < REVOMINI_SPI_DEVICE_NUM_DEVICES; i++) {
        if (!strcmp(spi_device_table[i].name, name)) {
            desc = &spi_device_table[i];
            break;
        }
    }
 
    if (!desc) {
        AP_HAL::panic("SPI: invalid device name");
    }

    return AP_HAL::OwnPtr<REVOMINI::SPIDevice>(new SPIDevice(*desc));
}


SPIDevice::SPIDevice(const SPIDesc &device_desc)
    : _desc(device_desc)
    , _initialized(false)
    , _completion_cb(0)

{
    if(_desc.cs_pin < BOARD_NR_GPIO_PINS) {
        _cs = REVOMINIGPIO::get_channel(_desc.cs_pin);
        if (!_cs) {
            AP_HAL::panic("Unable to instantiate cs pin");
        }
    } else {
        _cs = NULL;
    }        
}        

const spi_pins* SPIDevice::dev_to_spi_pins(const spi_dev *dev) {
    if (     dev->SPIx == SPI1)
        return &board_spi_pins[0];
    else if (dev->SPIx == SPI2)
        return &board_spi_pins[1];
    else if (dev->SPIx == SPI3)
        return &board_spi_pins[2];
    else {
        assert_param(0);
        return NULL;
    }
}

spi_baud_rate SPIDevice::determine_baud_rate(SPIFrequency freq)
{

	spi_baud_rate rate;

	switch(freq) {
	case SPI_36MHZ:
		rate = SPI_BAUD_PCLK_DIV_2;
		byte_time = 1; // time in 0.25uS units
		break;
	case SPI_18MHZ:
		rate = SPI_BAUD_PCLK_DIV_4;
		byte_time = 2;
		break;
	case SPI_9MHZ:
		rate = SPI_BAUD_PCLK_DIV_8;
		byte_time = 4;
		break;
	case SPI_4_5MHZ:
		rate = SPI_BAUD_PCLK_DIV_16;
		byte_time = 8;
		break;
	case SPI_2_25MHZ:
		rate = SPI_BAUD_PCLK_DIV_32;
		byte_time = 16;
		break;
	case SPI_1_125MHZ:
		rate = SPI_BAUD_PCLK_DIV_64;
		byte_time = 32;
		break;
	case SPI_562_500KHZ:
		rate = SPI_BAUD_PCLK_DIV_128;
		byte_time = 64;
		break;
	case SPI_281_250KHZ:
		rate = SPI_BAUD_PCLK_DIV_256;
		byte_time = 128;
		break;
	case SPI_140_625KHZ:
		rate = SPI_BAUD_PCLK_DIV_256;
		byte_time = 255;
		break;
	default:
		rate = SPI_BAUD_PCLK_DIV_32;
		byte_time = 16;
		break;
	}
	return rate;
}



// DMA
static uint32_t rw_workbyte[] = { 0xffff }; // not in stack!


uint8_t  SPIDevice::dma_transfer(const uint8_t *out, const uint8_t *recv, uint32_t btr)
{
    DMA_InitTypeDef DMA_InitStructure;
    DMA_StructInit(&DMA_InitStructure);

    const Spi_DMA &dp = _desc.dev->dma;


//  проверить, не занят ли поток DMA перед использованием
    uint32_t t = hal_micros();
    while(dma_is_stream_enabled(dp.stream_rx) || dma_is_stream_enabled(dp.stream_tx) ) {   // wait for previous transfer termination
        if(hal_micros() - t > 1000) break; // DMA transfer can't be more than 1ms
        hal_yield(0);
    } 

    spi_disable_irq(_desc.dev, SPI_RXNE_TXE_INTERRUPTS); // just for case

    dma_init(dp.stream_rx); dma_init(dp.stream_tx);

    dma_clear_isr_bits(dp.stream_rx); dma_clear_isr_bits(dp.stream_tx);

    /* shared DMA configuration values */
    DMA_InitStructure.DMA_Channel               = dp.channel;
    DMA_InitStructure.DMA_PeripheralBaseAddr    = (uint32_t)(&(_desc.dev->SPIx->DR));
    DMA_InitStructure.DMA_PeripheralDataSize    = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize        = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_PeripheralInc         = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_BufferSize            = btr;
    DMA_InitStructure.DMA_Mode                  = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority              = _desc.prio; // DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode              = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold         = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst           = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst       = DMA_PeripheralBurst_Single;
    //DMA_InitStructure.DMA_M2M                   = DMA_M2M_Disable;

 
  // receive stream
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    if(recv) {
      DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)recv;
      DMA_InitStructure.DMA_MemoryInc       = DMA_MemoryInc_Enable;
    } else {
      DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)rw_workbyte;
      DMA_InitStructure.DMA_MemoryInc       = DMA_MemoryInc_Disable;
    }
    dma_init_transfer(dp.stream_rx, &DMA_InitStructure);

  // transmit stream
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    if(out) {
      DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)out;
      DMA_InitStructure.DMA_MemoryInc       = DMA_MemoryInc_Enable;
    } else {
      DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)rw_workbyte;
      DMA_InitStructure.DMA_MemoryInc       = DMA_MemoryInc_Disable;
    }
    dma_init_transfer(dp.stream_tx, &DMA_InitStructure);

    dma_enable(dp.stream_rx); dma_enable(dp.stream_tx);

    // we should call it after completion via interrupt
    dma_attach_interrupt(dp.stream_rx, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&SPIDevice::dma_isr, void)), DMA_CR_TCIE);


    if(_completion_cb) {// we should call it after completion via interrupt    
        //SPI_I2S_DMACmd(_desc.dev->SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE); /* Enable SPI TX/RX request */
        spi_enable_dma_req(_desc.dev, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx);
        return 0;
    }

    // no callback - need to wait
   
#define MAX_SPI_TIME (8 * btr)// in uS
#if 0 
    uint8_t ret=0;


    t=hal_micros();
    if(!REVOMINIScheduler::in_interrupt()) { // if function called from task - store it and pause
        _task = REVOMINIScheduler::get_current_task();
        REVOMINIScheduler::task_pause(MAX_SPI_TIME);
    } else _task=NULL;
    while ( (dma_get_isr_bits(dp.stream_rx) & DMA_FLAG_TCIF) == 0) { 
        hal_yield(0); // пока ждем пусть другие работают. 
        if(hal_micros()-t > MAX_SPI_TIME) { ret=1; break; } // timeout
    }

    dma_isr();  //  disable DMA 
#else
    uint8_t ret=1;
    
    t=hal_micros();
    _task = REVOMINIScheduler::get_current_task();

    noInterrupts(); // we are in multitask so if task switch occures between enable and pause then all transfer will occures before task will be paused
                    //  so task will be paused after transfer and never be resumed - so will cause timeout
    /* Enable SPI TX/RX request */
//    SPI_I2S_DMACmd(_desc.dev->SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE);
    spi_enable_dma_req(_desc.dev, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx);

    if(_task) REVOMINIScheduler::task_pause(MAX_SPI_TIME);
    interrupts();
            
    while (hal_micros()-t < MAX_SPI_TIME && _desc.dev->state->busy) {
        hal_yield(0); // пока ждем пусть другие работают.
    }

    _task = NULL; // already resumed
    if(_desc.dev->state->busy) { // timeout, so there was no ISR, so
        disable_dma();          //   disable DMA
        isr_transfer_finish();  //   disable interrupts
    } else
       ret=0;   // OK
#endif
    return ret; 
}

void SPIDevice::disable_dma(){
    const Spi_DMA &dp = _desc.dev->dma;

    dma_disable(dp.stream_rx); dma_disable(dp.stream_tx);
    dma_detach_interrupt(dp.stream_rx); // we attach interrupt each request

    // Disable SPI RX/TX request 
//    SPI_I2S_DMACmd(_desc.dev->SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, DISABLE);
    spi_disable_dma_req(_desc.dev, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx);

    dma_clear_isr_bits(dp.stream_rx); dma_clear_isr_bits(dp.stream_tx);
}

void SPIDevice::dma_isr(){
    disable_dma();
    
    if(_desc.dev->state->len) {
        memmove(_desc.dev->state->dst, &buffer[_desc.bus-1][0], _desc.dev->state->len);
        _desc.dev->state->len=0; // once
    }

/* Datasheet:

During discontinuous communications, there is a 2 APB clock period delay between the
write operation to SPI_DR and the BSY bit setting. As a consequence, it is mandatory to
wait first until TXE=1 and then until BSY=0 after writing the last data.

See Figure 259

*/// now 2 bytes still should be sent so enable interrupt by TXE to not wait in ISR

    
    if(spi_is_busy(_desc.dev)) { // transfer could finish in time of memmove
        _isr_mode=SPI_ISR_FINISH; // should release SPI bus after last RXNE is set
        _send_len = 0;  // just for case
        _recv_len = 0;
        spi_attach_interrupt(_desc.dev, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&SPIDevice::spi_isr, void)) );    
    
        spi_irq_enable(_desc.dev, SPI_I2S_IT_TXE);  // priority of SPI interrupt is higher than DMA so interrupt can will be immediately because
                                                    // transfer can be finished when memmove() took place
    } else { // spi already free!
        isr_transfer_finish();                // releases SPI bus 
    }
}


void SPIDevice::init(){
    if(_cs) {
        _cs->mode(OUTPUT);
        _cs_release();    // do not hold the SPI bus initially
    }

    _completion_cb=0;
    
    const spi_pins *pins = dev_to_spi_pins(_desc.dev);

    if (!pins || pins->sck > BOARD_NR_GPIO_PINS || pins->mosi > BOARD_NR_GPIO_PINS || pins->miso > BOARD_NR_GPIO_PINS) {
        return;
    }

#ifdef BOARD_SOFTWARE_SPI
    if(_desc.soft) { //software
    
        { // isolate p
            const stm32_pin_info &p = PIN_MAP[pins->sck];
        
            const gpio_dev *sck_dev  = p.gpio_device;
                  uint8_t   sck_bit  = p.gpio_bit;

            gpio_set_mode(sck_dev,  sck_bit, GPIO_OUTPUT_PP);
            gpio_set_speed(sck_dev, sck_bit, GPIO_Speed_100MHz);
            gpio_write_bit(sck_dev, sck_bit, 1); // passive SCK high
            

            sck_port = sck_dev->GPIOx;
            sck_pin  = 1<<sck_bit;
        }

        { // isolate p
            const stm32_pin_info &p = PIN_MAP[pins->mosi];

            const gpio_dev *mosi_dev = p.gpio_device;
                  uint8_t   mosi_bit = p.gpio_bit;
            gpio_set_mode(mosi_dev,  mosi_bit, GPIO_OUTPUT_PP);
            gpio_set_speed(mosi_dev, mosi_bit, GPIO_Speed_100MHz);
            gpio_write_bit(mosi_dev, mosi_bit, 1); // passive MOSI high

            mosi_port = mosi_dev->GPIOx;
            mosi_pin  = 1<<mosi_bit;
        }

        { // isolate p
            const stm32_pin_info &p = PIN_MAP[pins->miso];

            const gpio_dev *miso_dev = p.gpio_device; 
                  uint8_t   miso_bit = p.gpio_bit;
            gpio_set_mode(miso_dev,  miso_bit, GPIO_INPUT_PU);
            gpio_set_speed(miso_dev, miso_bit, GPIO_Speed_100MHz);

            miso_port = miso_dev->GPIOx;
            miso_pin  = 1<<miso_bit;
        }

    } else 
#endif
    { /// hardware
        spi_init(_desc.dev); // disable device

        const stm32_pin_info &miso = PIN_MAP[pins->miso];
        spi_gpio_master_cfg(_desc.dev,
                        miso.gpio_device, PIN_MAP[pins->sck].gpio_bit,
                        miso.gpio_bit,    PIN_MAP[pins->mosi].gpio_bit);


        spi_master_enable(_desc.dev, determine_baud_rate(_desc.lowspeed), _desc.sm, MSBFIRST);          
    }
    _initialized=true;

}


bool SPIDevice::set_speed(AP_HAL::Device::Speed speed)
{

    if(owner[_desc.bus-1] != this) { // bus was in use by another driver so need reinit
        _initialized=false;
        init();
        if(!_initialized) return false;
        owner[_desc.bus-1] = this; // Got it!
    }

    switch (speed) {
    case AP_HAL::Device::SPEED_HIGH:
        _speed = _desc.highspeed;
        break;
    case AP_HAL::Device::SPEED_LOW:
    default:
        _speed = _desc.lowspeed;
        break;
    }

    return true;
}


uint16_t  SPIDevice::send_strobe(const uint8_t *buffer, uint16_t len){ // send in ISR and strobe each byte by CS
    while(_desc.dev->state->busy) hal_yield(0); // wait for previous transfer finished
    
    _send_address = buffer;
    _send_len = len;
    _recv_len = 0;
    _isr_mode = SPI_ISR_STROBE;

    spi_attach_interrupt(_desc.dev, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&SPIDevice::spi_isr, void)) );    

    _cs->_write(0);

    uint32_t timeout = len * 15; // time to transfer all data - 3uS per byte

    uint32_t t = hal_micros();
    _desc.dev->state->busy=true;

   (void)_desc.dev->SPIx->DR; // read fake data out
//[            
    _send_len--;       //        write 1st byte to start transfer
    uint8_t b = *_send_address++;
    _desc.dev->SPIx->DR = b;
//]


    // need to wait until  transfer complete 
    _task = REVOMINIScheduler::get_current_task();

    noInterrupts(); 
    spi_irq_enable(_desc.dev, SPI_I2S_IT_RXNE); // enable - will be interrupt after end of byte transfer

    if(_task) REVOMINIScheduler::task_pause(timeout); // if function called from task - store it and pause
    interrupts();

    while (hal_micros() - t < timeout && _send_len) {
        hal_yield(0);
    }

    _task = NULL; // already resumed
    if(_desc.dev->state->busy) isr_transfer_finish(); // timeout
    
    return _send_len;
}

uint8_t SPIDevice::wait_for(uint8_t out, spi_WaitFunc cb, uint16_t dly){ // wait for needed byte in ISR
    _send_len = out;
    _isr_mode = SPI_ISR_COMPARE;
    _recv_len = 0;
    _compare_cb = cb;    

    spi_attach_interrupt(_desc.dev, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&SPIDevice::spi_isr, void)) );    

    uint32_t t = hal_micros();
    _desc.dev->state->busy=true;

    (void)_desc.dev->SPIx->DR; // read fake data out

    // need to wait until transfer complete 
    _task = REVOMINIScheduler::get_current_task();

    _desc.dev->SPIx->DR = out; // start transfer and clear flag

    noInterrupts();
    spi_irq_enable(_desc.dev, SPI_I2S_IT_RXNE); 
    spi_irq_enable(_desc.dev, SPI_I2S_IT_TXE); // enable - will be interrupt on next line

    if(_task)  REVOMINIScheduler::task_pause(dly); // if function called from task - store it and pause
    interrupts();

    while (hal_micros() - t < dly) {
        hal_yield(0);
        if(_send_len == 0) break;
    }
    _task = NULL; // already resumed
    if(_desc.dev->state->busy) isr_transfer_finish(); // timeout
    return  _recv_data;
}

// releases SPI bus after last TXE is set
void SPIDevice::isr_transfer_finish(){
//    spi_irq_disable(_desc.dev, SPI_I2S_IT_TXE); // can't be combined
//    spi_irq_disable(_desc.dev, SPI_I2S_IT_RXNE);
    spi_disable_irq(_desc.dev, SPI_RXNE_TXE_INTERRUPTS); // just for case

    spi_detach_interrupt(_desc.dev);

    while (spi_is_rx_nonempty(_desc.dev)) {    // read out fake data for TX only transfers
        (void)spi_rx_reg(_desc.dev);
    }

    _desc.dev->state->busy=false; // reset 

    if(_task){ // resume paused task
        REVOMINIScheduler::task_resume(_task); // task will be resumed having very high priority
        _task=NULL;
        REVOMINIScheduler::context_switch_isr(); // force context switch just after return from ISR so task will get a tick
    }

    spi_wait_busy(_desc.dev); // SPI is double-buffered so we should wait to not spoil sent byte, but there no way to do it in interrupt
    if(_isr_mode != SPI_ISR_COMPARE) {
        _cs_release(); // free bus
    }

    Handler h;
    if((h=_completion_cb)) {
        _completion_cb=0; // only once and BEFORE call itself because IOC can do new transfer

        revo_call_handler(h, (uint32_t)&_desc);
    }
}

void SPIDevice::spi_isr(){
    if(spi_is_tx_empty(_desc.dev)  && spi_is_irq_enabled(_desc.dev, SPI_TXE_INTERRUPT)) {
//    if(SPI_I2S_GetITStatus()){
        if(_send_len) {    
            switch(_isr_mode) {
/*
            case SPI_ISR_STROBE:
                (void)_desc.dev->SPIx->DR; // read fake data out

                spi_wait_busy(_desc.dev); // SPI is double-buffered so we should wait to not spoil sent byte
                _cs->write(1);
                delay_ns100(1);
                _send_len--;
                _cs->write(0);
                delay_ns100(1);
                _desc.dev->SPIx->DR = *_send_address++;
                break;
*/
            case SPI_ISR_NONE:
                _send_len--;
                _desc.dev->SPIx->DR = *_send_address++;
                break;
            
            case SPI_ISR_COMPARE:
                _desc.dev->SPIx->DR = _send_len; // data to send in len
                break;
                
            default:
                break;
            }
        } else { // all sent
//            isr_transfer_finish();            // releases SPI bus after last TXE is set
            // now we in 1 byte till bus release
            (void)_desc.dev->SPIx->DR;          // reset RXNE  
            
            spi_disable_irq(_desc.dev, SPI_TXE_INTERRUPT);  // disable TXE interrupt
            spi_enable_irq(_desc.dev,  SPI_RXNE_INTERRUPT); // enable - will be interrupt after byte finished
            if(_recv_len) {
                // we need to switch to receive mode
            } else {
                _isr_mode=SPI_ISR_FINISH; // should  releases SPI bus after last RXNE is set
            }
        }
    }

//    if(SPI_I2S_GetITStatus()){
    if(spi_is_rx_nonempty(_desc.dev) /* && spi_is_irq_enabled(_desc.dev, SPI_RXNE_INTERRUPT) */) {
        switch(_isr_mode) {
        case SPI_ISR_FINISH:
            isr_transfer_finish();                // releases SPI bus after last TXE is set
            break;

        case SPI_ISR_STROBE: {
            (void)_desc.dev->SPIx->DR; // read fake data out
            if(_send_len){
                spi_wait_busy(_desc.dev); 
                _cs->_write(1);
                delay_ns100(1);
                _send_len--;
                uint8_t b = *_send_address++;
                _cs->_write(0);
                delay_ns100(1);
                _desc.dev->SPIx->DR = b;            
            } else {
                isr_transfer_finish();                // releases SPI bus after transfer complete
            }

            } break;

        case SPI_ISR_NONE:
            if(_recv_len){
                _recv_len--;
                *_recv_address++ = _desc.dev->SPIx->DR;
            }
            if(_recv_len==0) {      // last byte received
                isr_transfer_finish();                // releases SPI bus after last RXNE is set
            }
            break;

        case SPI_ISR_COMPARE:
            _recv_data = _desc.dev->SPIx->DR;
            if(_compare_cb(_recv_data) ) { // ok
                if(_recv_len){
                    _isr_mode = SPI_ISR_NONE;
                } else {
                    _send_len=0;             // mark transfer finished
                    isr_transfer_finish();
                }
            }
        default:
            isr_transfer_finish();                // releases SPI bus
            break;
        }
    }

}

