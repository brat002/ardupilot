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

#pragma GCC optimize ("O0")
#include "SPIDevice.h"
#include "GPIO.h"
//#pragma GCC pop_options



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


REVOMINI::Semaphore SPIDevice::_semaphores[4] IN_CCM; // per bus+1


#ifdef DEBUG_SPI 
struct spi_trans SPIDevice::spi_trans_array[SPI_LOG_SIZE]  IN_CCM;
uint8_t SPIDevice::spi_trans_ptr=0;
#endif




#define SCK_H       {sck_port->BSRRL = sck_pin; }
#define SCK_L       {sck_port->BSRRH = sck_pin; }

#define MOSI_H      {mosi_port->BSRRL = mosi_pin; }
#define MOSI_L      {mosi_port->BSRRH = mosi_pin; }

#define MISO_read   ((miso_port->IDR & miso_pin)!=0)

static volatile GPIO_TypeDef *sck_port;
static          uint16_t      sck_pin;

static volatile GPIO_TypeDef *mosi_port;
static          uint16_t      mosi_pin;

static volatile GPIO_TypeDef *miso_port;
static          uint16_t      miso_pin;

static uint16_t dly_time;


static void dly_spi() {
    delay_ns100(dly_time);
};


#pragma GCC optimize ("O0")


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



uint8_t SPIDevice::transfer(uint8_t out){

    if(_desc.soft) {
        return _transfer_s(out);
    } else {
        return _transfer(out);
    }
    
}


uint8_t SPIDevice::_transfer(uint8_t data) {
    uint8_t buf[1];

    //write 1byte
    //spi_tx(_desc.dev, &data, 1);
    spi_tx_reg(_desc.dev, data); //    _desc.dev->SPIx->DR = data;

    //read one byte
    while (!spi_is_rx_nonempty(_desc.dev)) {    // надо дожидаться окончания передачи.
        if(!spi_is_busy(_desc.dev) ) break;
    }
    
//    while (!spi_is_tx_empty(_desc.dev));
//    while (!spi_is_busy(_desc.dev))  ;
    
    buf[0] = (uint8_t)spi_rx_reg(_desc.dev);
    return buf[0];
}




bool SPIDevice::transfer(const uint8_t *send, uint32_t send_len, uint8_t *recv, uint32_t recv_len){
    
    uint8_t ret=0;
    bool was_dma=false;


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

        if (send != NULL && send_len) {
            for (uint16_t i = 0; i < send_len; i++) {
                _transfer_s(send[i]);
            }    
        } 
    
        if(recv !=NULL && recv_len) {
            for (uint16_t i = 0; i < recv_len; i++) {
                recv[i] = _transfer_s(0);
            }
        }
    } else {
        spi_set_speed(_desc.dev, determine_baud_rate(_speed));

#define MIN_DMA_BYTES 16
        _cs_assert();

        if(_desc.dma){
            if(send_len){
                if((send_len > MIN_DMA_BYTES || _desc.dma>1)){ // long enough 
                    if(ADDRESS_IN_RAM(send)){   // not in CCM
                        dma_transfer(send, NULL, send_len);
                        was_dma=true;
                    } else {
                        uint8_t *buf = (uint8_t*) malloc(send_len);
                        if(buf) {
                            memmove(buf,send,send_len);
                            dma_transfer(buf, NULL, send_len);
                            was_dma=true;
                            free(buf);
                        }
                    }
                } 
                
                if(!was_dma) {
                    spimaster_transfer(_desc.dev, send, send_len, NULL, 0);
                }
            }
            if(recv_len) {
                if((recv_len>MIN_DMA_BYTES || _desc.dma>1) ) { // long enough 
                    if(ADDRESS_IN_RAM(recv)){   //not in CCM
                        dma_transfer(NULL, recv, recv_len);
                        was_dma=true;
                    }else {
                        uint8_t *buf = (uint8_t*)malloc(recv_len);
                        if(buf) {
                            dma_transfer(NULL, buf, recv_len);
                            memmove(recv,buf,recv_len);
                            was_dma=true;
                            free(buf);
                        }
                    }
                } 
                if(!was_dma) {
                    spimaster_transfer(_desc.dev, NULL, 0, recv, recv_len);
                }
            }
        } else {
            ret = spimaster_transfer(_desc.dev, send, send_len, recv, recv_len);
        }
    }

#ifdef DEBUG_SPI 
    struct spi_trans &p = spi_trans_array[spi_trans_ptr];

    p.dev      = _desc.dev;
    p.send_len = send_len;
    if(send_len)
      p.sent = send[0];
    else p.sent = 0;
    if(send_len>1)
      p.sent1 = send[1];
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

    _cs_release();

    if(!was_dma){    
        if(_completion_cb) revo_call_handler(_completion_cb, (uint32_t)&_desc);
    }
    
    return ret==0;

}

    



bool SPIDevice::transfer_fullduplex(const uint8_t *send, uint8_t *recv, uint32_t len) {

    _cs_assert();

    if(_desc.soft) {
        if (send != NULL && recv !=NULL && len) {
            for (uint16_t i = 0; i < len; i++) {
                recv[i] = _transfer_s(send[i]);
            }    
        } 
    } else {
        spi_set_speed(_desc.dev, determine_baud_rate(_speed)); //- on cs_assert()
        
        if(_desc.dma && (send==NULL || ADDRESS_IN_RAM(send)) && (recv==NULL || ADDRESS_IN_RAM(recv)) ) {
            dma_transfer(send, recv, len);
        } else {
            if (send != NULL && recv !=NULL && len) {
                for (uint16_t i = 0; i < len; i++) {
                    recv[i] = _transfer(send[i]);
                }    
            }
        } 
    }
    _cs_release();
    return true;
}


#pragma GCC optimize ("O2")

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

//    return AP_HAL::OwnPtr<AP_HAL::SPIDevice>(new SPIDevice(*desc));
    return AP_HAL::OwnPtr<REVOMINI::SPIDevice>(new SPIDevice(*desc));
}


SPIDevice::SPIDevice(const SPIDesc &device_desc)
    : _desc(device_desc)
    , _initialized(false)

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
		break;
	case SPI_18MHZ:
		rate = SPI_BAUD_PCLK_DIV_4;
		break;
	case SPI_9MHZ:
		rate = SPI_BAUD_PCLK_DIV_8;
		break;
	case SPI_4_5MHZ:
		rate = SPI_BAUD_PCLK_DIV_16;
		break;
	case SPI_2_25MHZ:
		rate = SPI_BAUD_PCLK_DIV_32;
		break;
	case SPI_1_125MHZ:
		rate = SPI_BAUD_PCLK_DIV_64;
		break;
	case SPI_562_500KHZ:
		rate = SPI_BAUD_PCLK_DIV_128;
		break;
	case SPI_281_250KHZ:
		rate = SPI_BAUD_PCLK_DIV_256;
		break;
	case SPI_140_625KHZ:
		rate = SPI_BAUD_PCLK_DIV_256;
		break;
	default:
		rate = SPI_BAUD_PCLK_DIV_32;
		break;
	}
	return rate;
}



// DMA
static uint32_t rw_workbyte[] = { 0xffff }; // not in stack!


void  SPIDevice::dma_transfer(const uint8_t *send, const uint8_t *recv, uint32_t btr)
{
    DMA_InitTypeDef DMA_InitStructure;
    DMA_StructInit(&DMA_InitStructure);

    const Spi_DMA &dp = _desc.dev->dma;

    dma_init(dp.stream_rx); dma_init(dp.stream_tx);

//  проверить, не занят ли поток DMA перед использованием
    while(dma_is_stream_enabled(dp.stream_rx) || dma_is_stream_enabled(dp.stream_tx) ) {
        // wait for transfer termination
        hal_yield(0);
    } 

    dma_clear_isr_bits(dp.stream_rx); dma_clear_isr_bits(dp.stream_tx);

    /* shared DMA configuration values */
    DMA_InitStructure.DMA_Channel               = dp.channel;
    DMA_InitStructure.DMA_PeripheralBaseAddr    = (uint32_t)(&(_desc.dev->SPIx->DR));
    DMA_InitStructure.DMA_PeripheralDataSize    = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize        = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_PeripheralInc         = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_BufferSize            = btr;
    DMA_InitStructure.DMA_Mode                  = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority              = DMA_Priority_High;
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
    if(send) {
      DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)send;
      DMA_InitStructure.DMA_MemoryInc       = DMA_MemoryInc_Enable;
    } else {
      DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)rw_workbyte;
      DMA_InitStructure.DMA_MemoryInc       = DMA_MemoryInc_Disable;
    }
    dma_init_transfer(dp.stream_tx, &DMA_InitStructure);

    dma_enable(dp.stream_rx); dma_enable(dp.stream_tx);

    if(_completion_cb) {// we should call it after completion via interrupt
        dma_attach_interrupt(dp.stream_rx, REVOMINIScheduler::get_handler(FUNCTOR_BIND_MEMBER(&SPIDevice::isr, void)), DMA_CR_TCIE);
    }

    /* Enable SPI TX/RX request */
    SPI_I2S_DMACmd(_desc.dev->SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, ENABLE);

    if(_completion_cb) {// we should call it after completion via interrupt
        return;
    }

    // no callback - need to wait
    uint16_t dly = btr >>2; // 20 MHZ -> ~0.5 uS per byte
   /* Wait until Receive Complete */
    while ( (dma_get_isr_bits(dp.stream_rx) & DMA_FLAG_TCIF) == 0) { 
        if(dly!=0)
            hal_yield(dly); // пока ждем пусть другие работают. на меньших скоростях SPI вызовется несколько раз
    }

    isr();  //  disable DMA and release CS    
}

void SPIDevice::isr(){
    const Spi_DMA &dp = _desc.dev->dma;

    dma_disable(dp.stream_rx); dma_disable(dp.stream_tx);
    dma_detach_interrupt(dp.stream_rx);

    // Disable SPI RX/TX request 
    SPI_I2S_DMACmd(_desc.dev->SPIx, SPI_I2S_DMAReq_Rx | SPI_I2S_DMAReq_Tx, DISABLE);
    
    dma_clear_isr_bits(dp.stream_rx); dma_clear_isr_bits(dp.stream_tx);

    
    if(_completion_cb) {
        _cs_release();
        revo_call_handler(_completion_cb, (uint32_t)&_desc);
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

    if(_desc.soft) { //software
    
        { // isolate p
            const stm32_pin_info &p = PIN_MAP[pins->sck];
        
            const gpio_dev *sck_dev  = p.gpio_device;
                  uint8_t   sck_bit  = p.gpio_bit;

            gpio_set_mode(sck_dev,  sck_bit,  GPIO_OUTPUT_PP);
            gpio_write_bit(sck_dev,  sck_bit, 1); // passive SCK high

            sck_port = sck_dev->GPIOx;
            sck_pin  = 1<<sck_bit;
        }

        { // isolate p
            const stm32_pin_info &p = PIN_MAP[pins->mosi];

            const gpio_dev *mosi_dev = p.gpio_device;
                  uint8_t   mosi_bit = p.gpio_bit;
            gpio_set_mode(mosi_dev, mosi_bit, GPIO_OUTPUT_PP);

            mosi_port = mosi_dev->GPIOx;
            mosi_pin  = 1<<mosi_bit;
        }

        { // isolate p
            const stm32_pin_info &p = PIN_MAP[pins->miso];

            const gpio_dev *miso_dev = p.gpio_device; 
                  uint8_t   miso_bit = p.gpio_bit;
            gpio_set_mode(miso_dev, miso_bit, GPIO_INPUT_PU);

            miso_port = miso_dev->GPIOx;
            miso_pin  = 1<<miso_bit;
        }

    } else { /// hardware
        spi_init(_desc.dev); // disable device

        const stm32_pin_info &miso = PIN_MAP[pins->miso];
        spi_gpio_master_cfg(_desc.dev,
                    miso.gpio_device, PIN_MAP[pins->sck].gpio_bit,
                    miso.gpio_bit,    PIN_MAP[pins->mosi].gpio_bit);


        spi_master_enable(_desc.dev, determine_baud_rate(_desc.lowspeed), _desc.mode, MSBFIRST);         
    }
    _initialized=true;

}


bool SPIDevice::set_speed(AP_HAL::Device::Speed speed)
{

    if(!_initialized) {
        init();
        if(!_initialized) return false;
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

