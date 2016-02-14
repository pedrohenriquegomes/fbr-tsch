#include "stdint.h"
#include "string.h"
#include "board.h"
#include "radio.h"
#include "leds.h"
#include "bsp_timer.h"
#include "openrandom.h"

//=========================== defines =========================================

#define CHANNEL              20                           // 11 = 2.405GHz
#define MAX_LEN_PAYLOAD      125                          // max length of the PHY payload, NOT including the CRC
#define MAX_LEN_FRAME        MAX_LEN_PAYLOAD+LENGTH_CRC   // maximum length of the PHY payload, WITH he CRC
#define MIN_INTERFRAME_DELAY 10                           // 32kHz ticks (MUST be >10 per bug in bsp_timer module)
#define MAX_INTERFRAME_DELAY 1000                         // 32kHz ticks

//=========================== variables =======================================

typedef struct {
   uint16_t             num_bsp_timer_compare;
   uint16_t             num_radioTimerCompare;
   uint16_t             num_radioTimerOverflows;
   uint16_t             num_startFrame;
   uint16_t             num_endFrame;
} app_dbg_t;

app_dbg_t app_dbg;

typedef struct {
   uint8_t              txpk_txNow;
   uint8_t              txpk_buf[MAX_LEN_FRAME];
   uint8_t              txpk_len;
   uint8_t              txpk_num;
   uint16_t             shift_reg;
} app_vars_t;

app_vars_t app_vars;

//=========================== prototypes ======================================

uint16_t getrandom(void);
void cb_compare(void);
void cb_radioTimerOverflows(void);
void cb_radioTimerCompare(void);
void cb_startFrame(PORT_TIMER_WIDTH timestamp);
void cb_endFrame(PORT_TIMER_WIDTH timestamp);

//=========================== main ============================================

/**
\brief The program starts executing here.
*/
int mote_main(void) {
   uint8_t  i;
   uint16_t interFrameDelay;
   
   // clear/reset local variables
   memset(&app_vars,0,sizeof(app_vars_t));
   app_vars.shift_reg   = 0xbb5e;
   
   // initialize board
   board_init();
   
   // initialize bsp timer
   bsp_timer_set_callback(cb_compare);
   
   // add (unused) radio callback functions
   radio_setOverflowCb(cb_radioTimerOverflows);
   radio_setCompareCb(cb_radioTimerCompare);
   radio_setStartFrameCb(cb_startFrame);
   radio_setEndFrameCb(cb_endFrame);
   
   // prepare radio
   radio_rfOn();
   radio_setFrequency(CHANNEL); 
   radio_rfOff();
   
   while(1) {
      
      // I'm NOT ready to send now
      app_vars.txpk_txNow = 0;
      
      // arm timer which will set txpk_txNow to 1
      interFrameDelay = MIN_INTERFRAME_DELAY+(getrandom()%(MAX_INTERFRAME_DELAY-MIN_INTERFRAME_DELAY));
      bsp_timer_reset();
      bsp_timer_scheduleIn(interFrameDelay);
      
      // wait for txpk_txNow to be set to 1 (by timer expiring)
      while (app_vars.txpk_txNow==0) {
         __bis_SR_register(GIE+LPM0_bits);
      }
      
      // led
      leds_error_toggle();
      
      // prepare packet
      app_vars.txpk_num++;
      //app_vars.txpk_len           = 1+(getrandom()%(MAX_LEN_PAYLOAD-1));
      app_vars.txpk_len           = MAX_LEN_PAYLOAD;
      app_vars.txpk_buf[0]        = app_vars.txpk_num;
      for (i=1;i<app_vars.txpk_len;i++) {
         app_vars.txpk_buf[i]     = getrandom()%0xff;
      }
      
      // send packet
      radio_loadPacket(app_vars.txpk_buf,app_vars.txpk_len);
      radio_txEnable();
      radio_txNow();
   }
}

//=========================== callbacks =======================================

void cb_compare(void) {
   
   // update debug vals
   app_dbg.num_bsp_timer_compare++;
   
   // ready to send next packet
   app_vars.txpk_txNow = 1;
}

void cb_radioTimerCompare(void) {
   
   // update debug vals
   app_dbg.num_radioTimerCompare++;
}

void cb_radioTimerOverflows(void) {
   
   // update debug vals
   app_dbg.num_radioTimerOverflows++;
}

void cb_startFrame(PORT_TIMER_WIDTH timestamp) {
   
   // update debug vals
   app_dbg.num_startFrame++;
   
   // led
   leds_sync_on();
}

void cb_endFrame(PORT_TIMER_WIDTH timestamp) {
   
   // update debug vals
   app_dbg.num_endFrame++;
   
   // led
   leds_sync_off();
}

// ============= help ================

uint16_t getrandom() {
   uint8_t    i;
   uint16_t   random_value;
   
   random_value = 0;
   for(i=0;i<16;i++) {
      // Galois shift register
      // taps: 16 14 13 11
      // characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1
      random_value          |= (app_vars.shift_reg & 0x01)<<i;
      app_vars.shift_reg     = (app_vars.shift_reg>>1)^(-(int16_t)(app_vars.shift_reg & 1)&0xb400);
   }
   return random_value;
}
