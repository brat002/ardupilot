#include "sd.h"
#include "../diskio.h"
#include "../ff.h"
#include <stdlib.h>
#include <stdint.h>


#define CS_HIGH()	spi_chipSelectHigh()
#define CS_LOW()	spi_chipSelectLow(1)
#define	MMC_CD		spi_detect() /* Card detect (yes:true, no:false, default:true) */
#define	MMC_WP		0 /* Write protected (yes:true, no:false, default:false) */



/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/


/* MMC/SD command */
#define CMD0	(0)		/* GO_IDLE_STATE */
#define CMD1	(1)		/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)		/* SEND_IF_COND */
#define CMD9	(9)		/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define CMD13	(13)	        /* Get_STATUS */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */


// utility function 
extern uint8_t spi_spiSend(uint8_t b);
extern uint8_t spi_spiRec(void);
extern void spi_spiTransfer(const uint8_t *send, uint32_t send_len,  uint8_t *recv, uint32_t recv_len);
extern void spi_chipSelectHigh(void);
extern bool spi_chipSelectLow(bool take_sem);
extern void spi_yield();
extern uint8_t spi_detect();
extern uint32_t get_fattime ();


static volatile DSTATUS Stat = STA_NOINIT;	/* Physical drive status */

static volatile UINT Timer1, Timer2;	/* 1kHz decrement timer stopped at zero (disk_timerproc()) */
static DWORD sd_max_sectors=0;



extern int printf(const char *msg, ...);

#if defined(BOARD_SDCARD_CS_PIN)

static BYTE CardType;			/* Card type flags */
static BYTE was_write=0;
static BYTE csd[16]; // for DMA reads

static int8_t xmit_datablock (const BYTE *buff,	BYTE token);


uint8_t sd_get_type() {
    return CardType;
}

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Initialize MMC interface */


/* Exchange a byte */
static
BYTE xchg_spi (
	BYTE dat	/* Data to send */
)
{
    return spi_spiSend(dat);
}

static inline BYTE rcvr_spi(){
    return xchg_spi(0xFF);
}

/* Receive multiple byte */
static
void rcvr_spi_multi (
	BYTE *buff,		/* Pointer to data buffer */
	UINT btr		/* Number of bytes to receive (even number) */
){

    spi_spiTransfer(NULL,0, buff, btr);
}


/* Send multiple byte */
static void xmit_spi_multi (
	const BYTE *buff,	/* Pointer to the data */
	UINT btx			/* Number of bytes to send (even number) */
)
{

    spi_spiTransfer(buff, btx,NULL, 0);
}

/*
void power_on(void){ // Power on the SD-Card Socket if hardware allows
}

void power_off(void){
}
*/



/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
int wait_ready (	/* 1:Ready, 0:Timeout */
	UINT wt			/* Timeout [ms] */
)
{
	BYTE d;


	Timer2 = wt;
	do {
		d = xchg_spi(0xFF);		
		spi_yield(); /* This loop takes a time. Insert rot_rdq() here for multitask envilonment. */
	} while (d != 0xFF && Timer2);	/* Wait for card goes ready or timeout */

	if(d == 0xFF) {
	    // endless recursion! sd_get_state(); // reset errors
//	    if(was_write){ // no endless recursion because we reset flag first
//	        was_write=0;
//	        sd_get_state(); // reset errors
//	    }
	    return 1;
	}
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Deselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/

static
void deselect (void)
{
	CS_HIGH();		/* Set CS# high */
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */

}



/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/

static
int select (void)	/* 1:OK, 0:Timeout */
{
	if(!CS_LOW()) { /* take semaphore and Set CS# low */
	    printf("semaphore failed!\n");
	    return 0;		
	}
	xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */
	if (wait_ready(500)) return 1;	/* Wait for card ready */

	deselect();
	return 0;	/* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/

static
int8_t rcvr_datablock (	/* 1:OK, 0:Error */
	BYTE *buff,			/* Data buffer */
	UINT btr			/* Data block length (byte) */
)
{
	BYTE token;
        BYTE ret=0;

	Timer1 = 200;
	do {					/* Wait for DataStart token in timeout of 200ms */
    	    token = xchg_spi(0xFF);
	    spi_yield();	/* This loop will take a time. Insert rot_rdq() here for multitask environment. */
	} while ((token == 0xFF) && Timer1);
//	} while ((token != 0xFE) && Timer1);

	if(token != 0xFE) {

/*            if(token == 0xC0 || token == 0x80) {
                BYTE r = wait_ready(500);
        	rcvr_spi_multi(buff, btr);		// read the rest
                printf("rcvr_datablock failed token=%x ret=%d\n", token, r);
                goto done;
            }
*/
            if(token == 0xFF) {
                printf("rcvr_datablock timeout\n");
            } else {
                printf("rcvr_datablock failed token=%x\n", token);
            }
	    goto done;		/* Function fails if invalid DataStart token or timeout */
	}

	rcvr_spi_multi(buff, btr);		/* Store trailing data to the buffer */
	xchg_spi(0xFF); xchg_spi(0xFF);		/* Discard CRC */

        ret=1; 				/* Function succeeded */
done:
	return ret;
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

static int8_t xmit_datablock (	/* 1:OK, 0:Failed */
	const BYTE *buff,	/* Ponter to 512 byte data to be sent */
	BYTE token			/* Token */
)
{
	BYTE resp;


	if (!wait_ready(500)) return 0;		/* Wait for card ready */

	xchg_spi(token);				/* Send token */
	if (token != 0xFD) {				/* Send data if token is other than StopTran */
		xmit_spi_multi(buff, 512);		/* Data */
		xchg_spi(0xFF); xchg_spi(0xFF);	/* Dummy CRC */

		resp = xchg_spi(0xFF);			/* Receive data resp */
		if ((resp & 0x1F) != 0x05) {
		    printf("xmit_datablock failed resp=%x\n", resp);
		     return 0;	/* Function fails if the data packet was not accepted */
		}
	}
	return 1;
}


/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (	        	/* Return value: R1 resp (bit7==1:Failed to send) */
	BYTE cmd,		/* Command index */
	DWORD arg		/* Argument */
)
{
	BYTE n, res;


	if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) {
		    printf("cmd55 failed\n");
		    return res;
		}
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		spi_yield();
		if (!select()) {
		    printf("can't select SDn");
		    return 0xFF;
		}
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);			/* Start + command index */
	xchg_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xchg_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xchg_spi((BYTE)(arg >> 8));		/* Argument[15..8] */
	xchg_spi((BYTE)arg);			/* Argument[7..0] */
	n = 0x01;				/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;		/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;		/* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command resp */
	if (cmd == CMD12) xchg_spi(0xFF);	/* Diacard following one byte when CMD12 */
	n = 32;					/* Wait for response (10 bytes max) */
	do {
	    res = xchg_spi(0xFF);
	    spi_yield();	                /* This loop will take a time. Insert rot_rdq() here for multitask environment. */
	} while ((res & 0x80) && --n);

        if(res>1){
            printf("cmd%d response=%x\n",cmd, res);

        }
	return res;				/* Return received response */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS sd_initialize () {
	BYTE n, cmd, ty, ocr[4] ;
	
	if (Stat & STA_NODISK) return Stat;	/* Is card existing in the soket? */
	if(!(Stat & STA_NOINIT) )  return Stat; // already done


	ty = 0;
	n = send_cmd(CMD0, 0);
	if (n == 1 || n==0) {			/* Put the card SPI/Idle state */
		Timer1 = 1000;						/* Initialization timeout = 1 sec */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);	/* Get 32 bit return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {				/* Is the card supports vcc of 2.7-3.6V? */
				while (Timer1 && send_cmd(ACMD41, 1UL << 30)) ;	/* Wait for end of initialization with ACMD41(HCS) */
				if (Timer1 && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* Card id SDv2 */
				}
			}
		} else {	/* Not SDv2 card */
			if (send_cmd(ACMD41, 0) <= 1) 	{	/* SDv1 or MMC? */
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 (ACMD41(0)) */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 (CMD1(0)) */
			}
			while (Timer1 && send_cmd(cmd, 0)) ;		/* Wait for end of initialization */
			if (!Timer1 || send_cmd(CMD16, 512) != 0)	/* Set block length: 512 */
				ty = 0;
		}
	} else {
	    ty=0;
	}
	CardType = ty;	/* Card type */
	deselect();

	if (ty) {			/* OK */
	    Stat &= ~STA_NOINIT;	/* Clear STA_NOINIT flag */
            sd_getSectorCount(&sd_max_sectors);
        } else {			/* Failed */
	    Stat = STA_NOINIT;
	}


	return Stat;
}

BYTE sd_getSectorCount(DWORD *ptr){
    DWORD csize;
    
    if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
        if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
	    csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
	    *ptr = csize << 10;
	} else {					/* SDC ver 1.XX or MMC ver 3 */
	    BYTE n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
	    csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
	    *ptr = csize << (n - 9);
	}
	return RES_OK;
    }
    return RES_ERROR;

}

/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS sd_status (){

	return Stat;	/* Return disk status */
}

uint8_t sd_get_state(){

    if(send_cmd(CMD13, 0)){
        BYTE ret = xchg_spi(0xFF);
        if(ret) printf("Card error: %x\n",ret);
        return ret;
    }
    
    return 0xff;

/*
7 - out of range / CSD overwrite
6 • Erase param: An invalid selection for erase, sectors or groups.
5 • Write protect violation: The command tried to write a write-protected block.
4 • Card ECC failed: Card internal ECC was applied but failed to correct the data.
3 • CC error: Internal card controller error.
2 • Error: A general or an unknown error occurred during the operation.
1 • Write protect erase skip | lock/unlock command failed: This status bit has two functions over-
loaded. It is set when the host attempts to erase a write-protected sector or makes a sequence or
password errors during card lock/unlock operation.
0 • Card is locked: Set when the card is locked by the user. Reset when it is unlocked.

*/

}


/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/

static bool single_sector_card=false;


DRESULT sd_read (
	BYTE *buff,		/* Pointer to the data buffer to store read data */
	DWORD sector,	        /* Start sector number (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
        BYTE ret;
        uint16_t sectorInc=1;
        
	if (!count) return RES_PARERR;		        /* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */
	
	if(sector > sd_max_sectors) return RES_PARERR;		        /* Check parameter */

	if (!(CardType & CT_BLOCK)) {
	    sectorInc = 512;
	    sector *= sectorInc;	/* LBA to BA conversion (byte addressing cards) */
	}


	if (count == 1) {	                /* Single sector read */
		if (( (ret=send_cmd(CMD17, sector)) == 0)	/* READ_SINGLE_BLOCK */
		    && rcvr_datablock(buff, 512)) {
		        count = 0;
		} else {
		    printf("SD: failed to send CMD17 ret=%x\n", ret);
		}
		    
	} else {				/* Multiple sector read */
	    if(single_sector_card) {
		do {
		    if ((ret=send_cmd(CMD17, sector)) == 0){	/* READ_SINGLE_BLOCK */
			if(rcvr_datablock(buff, 512)) {
			    buff   += 512;
			    sector += sectorInc;
			} else {
			    printf("SD: failed to receive data count=%d\n", count);
			    break;
			}
		    } else {
		        printf("SD: failed to send CMD17 count=%d ret=%x\n", count, ret);
		        break;
		    }
                    deselect();
		    spi_yield();
		} while(--count);

	    } else {
//	        send_cmd(CMD23, count);
		if (CardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */

    	        uint16_t got=0;
            	if ((ret=send_cmd(CMD18, sector)) == 0) {	/* READ_MULTIPLE_BLOCK */
    		    do {
		        if (!rcvr_datablock(buff, 512)) {
		            printf("SD: failed to receive datablock count=%d\n", count);
		            break;
		        }
		        got++;
		        buff += 512;
		    } while (--count);
		    send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
		    if(got && count) {     // readed only 1 block
		        sector+=got*sectorInc;
		        single_sector_card=true;
		        return sd_read(buff, sector/sectorInc, count); // read remaining in single sector mode
		    }
		} else {
		    printf("SD: failed to send CMD18 count=%d ret=%x\n", count, ret);

		}
	    }
	}
	deselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT sd_write (
	const BYTE *buff,	/* Ponter to the data to write */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	if (!count) return RES_PARERR;		/* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check drive status */
	if (Stat & STA_PROTECT) return RES_WRPRT;	/* Check write protect */
	if(sector > sd_max_sectors) return RES_PARERR;		        /* Check parameter */

        uint16_t sectorInc = 1;


	if (!(CardType & CT_BLOCK)){
	    sectorInc = 512;
	    sector *= sectorInc;	/* LBA ==> BA conversion (byte addressing cards) */
	}

	if (count == 1) {	/* Single sector write */
		if (send_cmd(CMD24, sector) == 0) {	/* WRITE_BLOCK */
		    if( xmit_datablock(buff, 0xFE)) {
			count = 0;
		    }		    
		}
	}
	else {	
	    if(single_sector_card) {
	        do {
    		    if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE)) {
			sector+=sectorInc;
			buff += 512;
		    } else break; // error
	        } while(--count);
	    } else {
				/* Multiple sector write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */

		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
		    do {
			if (!xmit_datablock(buff, 0xFC)) break;
			buff += 512;
		    } while (--count);
		    if (!xmit_datablock(0, 0xFD)) count = 1;	/* STOP_TRAN token */
		}
	    }
	}
	deselect();

//        was_write = 1;

	if(count) return RES_ERROR;

        uint8_t ret = sd_get_state(); // reset errors
	
	if(ret) return RES_ERROR;

	return RES_OK;	/* Return result */
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/

DRESULT sd_ioctl (
	BYTE cmd,		/* Control command code */
	void *buff		/* Pointer to the conrtol data */
)
{
    DRESULT res;
    BYTE n;
    DWORD *dp, st, ed;

    BYTE *ptr = (BYTE *)buff;

    res = RES_ERROR;

    if (cmd== CTRL_POWER){

        switch (*ptr) {
        case 0: // Sub control code == 0 (POWER_OFF) 
//             if (SD_PRESENT())
//                 power_off(); // Power off 
             res = RES_OK;
             break;
         case 1: // Sub control code == 1 (POWER_ON) 
//             power_on(); // Power on 
             res = RES_OK;
             break;
         case 2: // Sub control code == 2 (POWER_GET) 
             *(ptr + 1) = (BYTE) MMC_CD;
             break;
         default:
             res = RES_PARERR;
             break;
        }

    } else {
    
        if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */

	switch (cmd) {
	case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
		if (select()) res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (DWORD) */
	        res=sd_getSectorCount((DWORD*)buff);
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
		if (CardType & CT_SD2) {	/* SDC ver 2.00 */
		    if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
			xchg_spi(0xFF);
			if (rcvr_datablock(csd, 16)) {				/* Read partial block */
				for (n = 64 - 16; n; n--) xchg_spi(0xFF);	/* Purge trailing data */
				*(DWORD*)buff = 16UL << (csd[10] >> 4);
				res = RES_OK;
			}
		    }
		} else {					/* SDC ver 1.XX or MMC */
		    if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
			if (CardType & CT_SD1) {	/* SDC ver 1.XX */
				*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
			} else {					/* MMC */
				*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
			}
			res = RES_OK;
		    }
		}
		break;

	case CTRL_TRIM :	                                /* Erase a block of sectors (used when _USE_ERASE == 1) */
		if (!(CardType & CT_SDC)) break;		/* Check if the card is SDC */
		if (sd_ioctl(MMC_GET_CSD, csd)) break;        	/* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
		dp = buff; st = dp[0]; ed = dp[1];		/* Load sector block */
		if (!(CardType & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000)) {	/* Erase sector block */
			res = RES_OK;	                        /* FatFs does not check result of this command */
		}
		break;
/*
        case MMC_GET_TYPE: // Get card type flags (1 byte) 
             *ptr = g_card_type;
             res = RES_OK;
             break;
*/
         case MMC_GET_CSD: /* Receive CSD as a data block (16 bytes) */
             if (send_cmd(CMD9, 0) == 0 /* READ_CSD */
             && rcvr_datablock(ptr, 16))
                 res = RES_OK;
             break;

         case MMC_GET_CID: /* Receive CID as a data block (16 bytes) */
             if (send_cmd(CMD10, 0) == 0 /* READ_CID */
             && rcvr_datablock(ptr, 16))
                 res = RES_OK;
             break;

         case MMC_GET_OCR: /* Receive OCR as an R3 resp (4 bytes) */
             if (send_cmd(CMD58, 0) == 0) { /* READ_OCR */
                 for (n = 4; n; n--)
                     *ptr++ = rcvr_spi();
                 res = RES_OK;
             }
             break;

         case MMC_GET_SDSTAT: /* Receive SD status as a data block (64 bytes) */
             if (send_cmd(ACMD13, 0) == 0) { /* SD_STATUS */
                 rcvr_spi();
                 if (rcvr_datablock(ptr, 64))
                     res = RES_OK;
             }
             break;

	default:
		res = RES_PARERR;
	}

	deselect();
    }
    return res;
}


/*-----------------------------------------------------------------------*/
/* Device timer function                                                 */
/*-----------------------------------------------------------------------*/
/* This function must be called from timer interrupt routine in period
/  of 1 ms to generate card control timing.
*/

void sd_timerproc (void)
{
	WORD n;
	BYTE s;


	n = Timer1;						/* 1kHz decrement timer stopped at 0 */
	if (n) Timer1 = --n;
	n = Timer2;
	if (n) Timer2 = --n;

	s = Stat;
	if (MMC_WP) {	/* Write protected */
		s |= STA_PROTECT;
	} else {		/* Write enabled */
		s &= ~STA_PROTECT;
	}
	if (MMC_CD) {	/* Card is in socket */
		s &= ~STA_NODISK;
	} else {		/* Socket empty */
		s |= (STA_NODISK | STA_NOINIT);
	}
	Stat = s;
}


#elif defined(BOARD_DATAFLASH_FATFS)

// emulate SD card on board's dataflash

// flash size
#define DF_NUM_PAGES 0x1f00
#define DF_PAGE_SIZE 256L

#define DF_RESET BOARD_DATAFLASH_CS_PIN // RESET (PB3)

//Winbond M25P16 Serial Flash Embedded Memory 16 Mb, 3V
#define JEDEC_WRITE_ENABLE           0x06
#define JEDEC_WRITE_DISABLE          0x04
#define JEDEC_READ_STATUS            0x05
#define JEDEC_WRITE_STATUS           0x01
#define JEDEC_READ_DATA              0x03
#define JEDEC_FAST_READ              0x0b
#define JEDEC_DEVICE_ID              0x9F
#define JEDEC_PAGE_WRITE             0x02

#define JEDEC_BULK_ERASE             0xC7 // full chip erase
#define JEDEC_SECTOR_ERASE           0x20 // 4k erase
#define JEDEC_PAGE_ERASE             0xD8 // 64K erase

#define JEDEC_STATUS_BUSY            0x01
#define JEDEC_STATUS_WRITEPROTECT    0x02
#define JEDEC_STATUS_BP0             0x04
#define JEDEC_STATUS_BP1             0x08
#define JEDEC_STATUS_BP2             0x10
#define JEDEC_STATUS_TP              0x20
#define JEDEC_STATUS_SEC             0x40
#define JEDEC_STATUS_SRP0            0x80

//#define expect_memorytype            0x20
//#define expect_capacity              0x15

#define MAX_ERASE_SIZE 16384

static bool flash_died=false;
static uint8_t df_manufacturer;
static uint16_t df_device;
static uint32_t erase_size = BOARD_DATAFLASH_ERASE_SIZE;
#if BOARD_DATAFLASH_ERASE_SIZE  >= 65536
static uint8_t erase_cmd=JEDEC_PAGE_ERASE;
#else
static uint8_t erase_cmd=JEDEC_SECTOR_ERASE;
#endif

uint8_t sd_get_type() {
    return 0;
}

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Initialize MMC interface */


/* Exchange a byte */
static inline uint8_t spi_write(
	BYTE dat	/* Data to send */
)
{
    return spi_spiSend(dat);
}

static inline uint8_t spi_read(){
    return spi_spiSend(0xFF);
}

/* Receive multiple byte */
static inline void read_spi_multi (
	BYTE *buff,		/* Pointer to data buffer */
	UINT btr		/* Number of bytes to receive (even number) */
){

    spi_spiTransfer(NULL,0, buff, btr);
}


/* Send multiple byte */
static inline void write_spi_multi (
	const BYTE *buff,	/* Pointer to the data */
	UINT btx	        /* Number of bytes to send (even number) */
)
{

    spi_spiTransfer(buff, btx,NULL, 0);
}


static inline
void cs_release (void)
{
	CS_HIGH();		/* Set CS# high */
}




static
bool cs_assert(void)	/* 1:OK, 0:Timeout */
{
	if(!CS_LOW()) { /* take semaphore and Set CS# low */
	    printf("semaphore failed!\n");
	    return 0;		
	}
	return 1;	
}



// This function is mainly to test the device
static void ReadManufacturerID()
{
    // activate dataflash command decoder
    if (!cs_assert()) return;

    // Read manufacturer and ID command...
    spi_write(JEDEC_DEVICE_ID); //

    df_manufacturer = spi_read();
    df_device = spi_read(); //memorytype
    df_device = (df_device << 8) | spi_read(); //capacity
    spi_read(); // dummy byte

    // release SPI bus for use by other sensors
    cs_release();
}
            
            
// Read the status register
static uint8_t ReadStatusReg()
{
    uint8_t tmp;

    // activate dataflash command decoder
    if (!cs_assert()) return JEDEC_STATUS_BUSY;

    // Read status command
    spi_write(JEDEC_READ_STATUS);
    tmp = spi_read(); // We only want to extract the READY/BUSY bit

    // release SPI bus for use by other sensors
    cs_release();

    return tmp;
}

static uint8_t ReadStatus()
{
  // We only want to extract the READY/BUSY bit
    int32_t status = ReadStatusReg();
    if (status < 0)
            return -1;
    return status & JEDEC_STATUS_BUSY;
}

/*-----------------------------------------------------------------------*/
// Wait for dataflash
/*-----------------------------------------------------------------------*/

static int wait_ready (	/* 1:Ready, 0:Timeout */
	UINT wt			/* Timeout [ms] */
)
{
    if(flash_died) return 0;

    Timer2 = wt;
    do {
        if(ReadStatus()==0) return 1; //ready

	spi_yield();    /* This loop takes a time. Insert rot_rdq() here for multitask envilonment. */
    } while(Timer2);	/* Wait for card goes ready or timeout */

    flash_died = true;
    
    return 0;
}






static void Flash_Jedec_WriteEnable(void){ 
    if (!wait_ready(500)) return;  /* Wait for card ready */

    if (!cs_assert()) return;
    spi_write(JEDEC_WRITE_ENABLE);
    cs_release();
}

static uint8_t sector_buf[DF_PAGE_SIZE]; // we ALWAYS can use DMA!

static bool read_page( BYTE *buf, DWORD pageNum){
    uint32_t PageAdr = pageNum * DF_PAGE_SIZE;

    if (!wait_ready(500)) return 0;  /* Wait for card ready */

    if (!cs_assert()) return 0;

    uint8_t cmd[4];
    cmd[0] = JEDEC_READ_DATA;
    cmd[1] = (PageAdr >> 16) & 0xff;
    cmd[2] = (PageAdr >>  8) & 0xff;
    cmd[3] = (PageAdr >>  0) & 0xff;

    
    write_spi_multi(cmd, sizeof(cmd));

    read_spi_multi(sector_buf, DF_PAGE_SIZE);

    cs_release();
    uint16_t i;
    for(i=0; i<DF_PAGE_SIZE;i++){
        buf[i] = ~sector_buf[i];       // let filesystem will be inverted, this allows extend files without having to Read-Modify-Write on FAT
                                    // original: 0xFF is clear and 0 can be programmed any time
                                    // inverted: 0 is clear and 1 can be programmed any time
                                    // to mark cluster as used it should be set 1 in the FAT. Also new entries in dirs can be created without RMW
    }

    return 1;
}


static bool write_page(const BYTE *buf, DWORD pageNum){
    uint32_t PageAdr = pageNum * DF_PAGE_SIZE;

    uint16_t i;
    for(i=0; i<DF_PAGE_SIZE;i++){
        sector_buf[i] = ~buf[i];       // let filesystem will be inverted, this allows extend files without having to Read-Modify-Write on FAT
    }

    Flash_Jedec_WriteEnable();

    if (!cs_assert()) return 0;

    uint8_t cmd[4];
    cmd[0] = JEDEC_PAGE_WRITE;
    cmd[1] = (PageAdr >> 16) & 0xff;
    cmd[2] = (PageAdr >>  8) & 0xff;
    cmd[3] = (PageAdr >>  0) & 0xff;

    write_spi_multi(cmd, sizeof(cmd));

    write_spi_multi(sector_buf, DF_PAGE_SIZE);
    cs_release();
    return 1;
}

static bool erase_page(uint16_t pageNum)
{

    Flash_Jedec_WriteEnable();

    uint32_t PageAdr = pageNum * DF_PAGE_SIZE;

    uint8_t cmd[4];
    cmd[0] = erase_cmd;
    cmd[1] = (PageAdr >> 16) & 0xff;
    cmd[2] = (PageAdr >>  8) & 0xff;
    cmd[3] = (PageAdr >>  0) & 0xff;

    if (!cs_assert()) return 0;
    write_spi_multi(cmd, sizeof(cmd));
    cs_release();
    return 1;
}

BYTE sd_getSectorCount(DWORD *ptr){

    uint8_t capacity = df_device & 0xFF;
    uint8_t memtype =  (df_device>>8) & 0xFF;
    uint32_t size=0;
    
    printf("SPI Flash codes: mfg=%x type=%x cap=%x\n ",df_manufacturer, memtype, capacity);

    const char * mfg=NULL;
    
    switch(df_manufacturer){
    case 0xEF: //  Winbond Serial Flash 
        if (memtype == 0x40) {
            mfg="Winbond";
            size = (1 << ((capacity & 0x0f) + 8)) *8;
/*
 const uint8_t _capID[11]      = {0x10,  0x11,   0x12,   0x13,   0x14, 0x15, 0x16, 0x17, 0x18,  0x19,  0x43};
  const uint32_t _memSize[11]  = {64L*K, 128L*K, 256L*K, 512L*K, 1L*M, 2L*M, 4L*M, 8L*M, 16L*M, 32L*M, 8L*M};
*/
            erase_size=4096;
            erase_cmd=JEDEC_SECTOR_ERASE;
        }
        break;
    case 0xbf: // SST
        if (memtype == 0x25) {
            mfg="Microchip";
            size = (1 << ((capacity & 0x07) + 12))*8 ;
        }
        break;
        
    case 0x20: // micron
        if (memtype == 0xba){// JEDEC_ID_MICRON_N25Q128        0x20ba18
            mfg="Micron";
            size = (1 << ((capacity & 0x0f) + 8))*8 ;
            erase_size=4096;
            erase_cmd=JEDEC_SECTOR_ERASE;
        } else if(memtype==0x20) {  // JEDEC_ID_MICRON_M25P16         0x202015
            mfg="Micron";
            size = (1 << ((capacity & 0x0f) + 8))*8 ;
        }
        break;
        
    case 0xC2:     //JEDEC_ID_MACRONIX_MX25L6406E   0xC22017
        if (memtype == 0x20) { 
            mfg="MACRONIX";
            size = (1 << ((capacity & 0x0f) + 8))*8 ;
            erase_size=4096;
            erase_cmd=JEDEC_SECTOR_ERASE;
        }
        break;
    
    case 0x9D: // ISSI
        if (memtype == 0x40 || memtype == 0x30) {
            mfg = "ISSI";
            size      = (1 << ((capacity & 0x0f) + 8))*8;
        }
        break;

    default:
        break;
    }

    if(mfg && size) printf("%s SPI Flash found sectors=%ld\n", mfg, size);
    else  {
        printf("unknown Flash!\n");
        size = BOARD_DATAFLASH_PAGES; // as defined 
    } 

    if(erase_size > MAX_ERASE_SIZE)   size -= (erase_size/DF_PAGE_SIZE); // reserve for RMW ops
        
    *ptr = size / (FAT_SECTOR_SIZE/DF_PAGE_SIZE);    // in 512b blocks
        
    return RES_OK;

}

/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS sd_initialize () {
    static bool initialized=0;

    if(initialized) return Stat;

    ReadManufacturerID();

    sd_getSectorCount(&sd_max_sectors);

    
    initialized=1;

    Stat=0;
    return Stat;
}


/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS sd_status (){

	return Stat;	/* Return disk status */
}


/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT sd_read (
	BYTE *buff,		/* Pointer to the data buffer to store read data */
	DWORD sector,	        /* Start sector number (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
        
	if (!count) return RES_PARERR;		        /* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */
	if(sector > sd_max_sectors) return RES_PARERR;		        /* Check parameter */

        count  *= FAT_SECTOR_SIZE/DF_PAGE_SIZE; // 256bytes page from 512byte sector
        sector *= FAT_SECTOR_SIZE/DF_PAGE_SIZE;
    

	do {
	    if(!read_page(buff, sector)) break;
	    buff   += DF_PAGE_SIZE;
	    sector += 1;
	} while(--count);

	return count ? RES_ERROR : RES_OK;	/* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/


bool sd_move_block(uint32_t sec_from, uint32_t sec_to, const uint8_t *buff, uint32_t sec, uint16_t count);

/*
    в системе нет памяти, а выполнять Read-Modify-Write для флешки как-то надо. придется использовать свободный блок 
    флеши для буфферизации. Ресурс это конечно уменьшит, зато быстрее перепаяют на нормальную :) 
    по-уму надо читать FAT и выбирать случайный свободный кластер, но использование для этого функций самой FatFs 
    приводит к смещению окна в неожиданное время, например когда запись вызывается для сброса измененных структур FAT
    
*/
bool sd_move_block(uint32_t sec_from, uint32_t sec_to, const uint8_t *buff, uint32_t sec, uint16_t count){

    if(!erase_page(sec_to)) return RES_ERROR;

    uint8_t *sbuffer = malloc(DF_PAGE_SIZE); // read just the needed sector, not in stack for not in CCM
    if(!sbuffer) return false;

    uint16_t cnt;
    
    for(cnt=erase_size/DF_PAGE_SIZE; cnt>0; cnt--){
        if(buff && sec_from >= sec && sec_from < (sec + count) ) { // if replacement data fit to sector
            memcpy(sbuffer, buff, DF_PAGE_SIZE);       // will use it
            buff+=DF_PAGE_SIZE;
        } else {                                // read from source
            if(!read_page(sbuffer, sec_from)) break;
        }
        if(!write_page(sbuffer, sec_to)) break;
        sec_from++;
        sec_to++;    
    }
    
    free(sbuffer);
    
    return cnt==0;
}


DRESULT sd_write (
	const BYTE *buff,	/* Ponter to the data to write */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	if (!count) return RES_PARERR;		/* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check drive status */
	if(sector > sd_max_sectors) return RES_PARERR;		        /* Check parameter */


        uint16_t pos = sector % erase_size/FAT_SECTOR_SIZE; // number of sector in erase block
        if( pos == 0 && count >= erase_size/FAT_SECTOR_SIZE){ // begin of erase block - write full cluster
            count  *= FAT_SECTOR_SIZE/DF_PAGE_SIZE; // 256bytes page from 512byte sector
            sector *= FAT_SECTOR_SIZE/DF_PAGE_SIZE;
            if(!erase_page(sector)) return RES_ERROR;
    	    do {
    	        if(!write_page(buff, sector)) break;
	        buff   += DF_PAGE_SIZE;
	        sector += 1;
	    } while(--count);
	    
        } else { // read-modify-write
            uint8_t *ptr;
            bool need_erase = false; // check for free space for write
            
            {
                uint8_t *sbuffer = malloc(FAT_SECTOR_SIZE*count); // read just the needed sectors
                if(!sbuffer) return RES_ERROR; // no memory at all
                
                ptr = sbuffer;

                uint32_t r_sector = sector;
                uint16_t r_count  = count;

                r_sector *= FAT_SECTOR_SIZE/DF_PAGE_SIZE;
                r_count  *= FAT_SECTOR_SIZE/DF_PAGE_SIZE;      // read data will be rewritten

	        do {
	            if(!read_page(ptr, r_sector)) break;
	            ptr   += DF_PAGE_SIZE;
	            r_sector += 1;
                } while(--r_count);
                
                
                const uint8_t *pp;
                for(ptr = sbuffer, pp=buff; ptr < sbuffer+FAT_SECTOR_SIZE*count;ptr++,pp++){
//                    if(~*ptr & *pp){ // у нас не должно быть нулей там где нужна 1
                // filesystem is inverted, so - у нас не должно быть 1 там где нужен 0
                // пример: чистая FF - инверсия 00, можно писАть любой байт
                //         считали F0 - инверсия 0F - можно писАть *0
                    if(*ptr & *pp){ 
                        need_erase=true;
                        break;
                    }
                }
                free(sbuffer);// don't need more
            }
            
            if(need_erase){// write do dirty block

                uint8_t *cluster = malloc(erase_size);
                if(!cluster) { // we can try to allocate up to 64K so absense of memory should not be error!
                    if(erase_size <= MAX_ERASE_SIZE) return RES_ERROR; // we have no reserved page
                    
                    // we can use any free sector of flash as buffer, too                    
                    uint32_t fr_sec = sd_max_sectors * (FAT_SECTOR_SIZE/DF_PAGE_SIZE); // the last page

                    // read data from beginning of erase block up to part that will be rewritten
                    uint32_t r_sector = (sector * (FAT_SECTOR_SIZE/DF_PAGE_SIZE)) & ~(erase_size/DF_PAGE_SIZE - 1);

                    // move data to sectors of free block
                    if(!sd_move_block(r_sector, fr_sec, NULL, 0, 0)) return RES_ERROR; 
                    // move back with inserting of data to write
                    if(!sd_move_block(fr_sec, r_sector, buff, sector * (FAT_SECTOR_SIZE/DF_PAGE_SIZE), count * (FAT_SECTOR_SIZE/DF_PAGE_SIZE))) return RES_ERROR; // move back
                    count=0; // all OK

                    
                } else { // we have enough memory for cluster buffer
            
                    ptr = cluster;

                    // read data from beginning of erase block up to part that will be rewritten
                    uint32_t r_sector = (sector * (FAT_SECTOR_SIZE/DF_PAGE_SIZE)) & ~(erase_size/DF_PAGE_SIZE - 1);
                    uint32_t w_sector = r_sector;
                    uint16_t r_count = erase_size/DF_PAGE_SIZE;      // read full page
        	    do {
	                if(r_sector >= sector * (FAT_SECTOR_SIZE/DF_PAGE_SIZE)) break;
	                if(!read_page(ptr, r_sector)) break;
	                ptr   += DF_PAGE_SIZE;
	                r_sector += 1;
                    } while(--r_count);

                    // Yes I know that we could to go out from the buffer end - but this used only for FAT_FS which writes FAT per one sector
//                  memcpy(cluster+FAT_SECTOR_SIZE*pos, (BYTE *)buff, FAT_SECTOR_SIZE*count); // insert sectors to write to right place
                    memcpy(ptr, (BYTE *)buff, FAT_SECTOR_SIZE*count); // insert sectors to write to right place

                    // skip part that will be rewritten
                    r_count  -= (FAT_SECTOR_SIZE/DF_PAGE_SIZE) * count;
                    r_sector += (FAT_SECTOR_SIZE/DF_PAGE_SIZE) * count;
                    ptr      +=  FAT_SECTOR_SIZE * count;

                    // read the tail
        	    while(r_count) {
	                if(!read_page(ptr, r_sector)) break;
	                ptr   += DF_PAGE_SIZE;
	                r_sector += 1;
	                --r_count;
                    };
                    
                    if(r_count) return RES_ERROR;    
        
                    // erase page
                    if(!erase_page(w_sector)) return RES_ERROR;
            
                    ptr = cluster;
                    uint16_t w_count = erase_size/DF_PAGE_SIZE;
    	            do {
    	                if(!write_page(ptr, w_sector)) break;
	                ptr   += DF_PAGE_SIZE;
    	                w_sector += 1;
        	    } while(--w_count);
            
                    count = w_count;
                    
                    free(cluster);
                }
                
            } else { // block is ready to write - just write needed part
                count  *= FAT_SECTOR_SIZE/DF_PAGE_SIZE; // 256bytes page from 512byte sector
                sector *= FAT_SECTOR_SIZE/DF_PAGE_SIZE;
    	        do {
        	    if(!write_page(buff, sector)) break;
    	            buff   += DF_PAGE_SIZE;
    	            sector += 1;
    	        } while(--count);                
            }
        }


	return count ? RES_ERROR : RES_OK;	/* Return result */
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/

DRESULT sd_ioctl (
	BYTE cmd,		/* Control command code */
	void *buff		/* Pointer to the conrtol data */
)
{
    DRESULT res;

    BYTE *ptr = (BYTE *)buff;

    res = RES_ERROR;

    if (cmd== CTRL_POWER){

        switch (*ptr) {
        case 0: // Sub control code == 0 (POWER_OFF) 
             res = RES_OK;
             break;
         case 1: // Sub control code == 1 (POWER_ON) 
             res = RES_OK;
             break;
         case 2: // Sub control code == 2 (POWER_GET) 
             *(ptr + 1) = (BYTE) MMC_CD;
             break;
         default:
             res = RES_PARERR;
             break;
        }

    } else {
    
        if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */

	switch (cmd) {
	case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
	    res = RES_OK;
	    break;

	case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (uint32_t ) */    
            *(uint32_t *)buff = sd_max_sectors;
	    res = RES_OK;	        
	    break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of FAT sector (uint32_t ) */
	    *(uint32_t *)buff = erase_size/FAT_SECTOR_SIZE;
	    res = RES_OK;	        
	    break;

        case GET_SECTOR_SIZE:
	    *(uint32_t *)buff = FAT_SECTOR_SIZE; // always
	    res = RES_OK;	        
	    break;

	case CTRL_TRIM : {	                                /* Erase a block of sectors (used when _USE_TRIM == 1) */
	    uint32_t  start_sector = ((uint32_t  *)buff)[0];
	    uint32_t  end_sector   = ((uint32_t  *)buff)[1];
	    uint32_t  block, last_block=-1;
	    
	    if(start_sector>=sd_max_sectors || end_sector>=sd_max_sectors) return RES_PARERR;

            uint32_t  sector;
	    for(sector=start_sector; sector <= end_sector;sector++){
                uint32_t  df_sect = sector * (FAT_SECTOR_SIZE/DF_PAGE_SIZE);    // sector in DataFlash
                block = df_sect / (erase_size/DF_PAGE_SIZE);    // number of EraseBlock
                if(last_block!=block){
                    last_block=block;
                    if(!erase_page(df_sect)) return RES_ERROR;                    
                }
            }
	    res = RES_OK;
	    
            } break;

	default:
		res = RES_PARERR;
	}

    }
    return res;
}


/*-----------------------------------------------------------------------*/
/* Device timer function                                                 */
/*-----------------------------------------------------------------------*/
/* This function must be called from timer interrupt routine in period
/  of 1 ms to generate card control timing.
*/

void sd_timerproc (void)
{
	WORD n;

	n = Timer1;						/* 1kHz decrement timer stopped at 0 */
	if (n) Timer1 = --n;
	n = Timer2;
	if (n) Timer2 = --n;

}



#endif //defined(BOARD_DATAFLASH_FATFS)
