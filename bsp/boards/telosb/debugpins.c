/**
\brief TelosB-specific definition of the "debugpins" bsp module.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, February 2012.
*/
#include <stdint.h>               // needed for uin8_t, uint16_t
#include "msp430f1611.h"
#include "debugpins.h"

//=========================== defines =========================================

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

void debugpins_init() {
   P6DIR |=  0x40;      // frame [P6.6]
   P6DIR |=  0x80;      // slot  [P6.7]
   P6DIR |=  0x04;      // fsm   [P6.2]
   P2DIR |=  0x40;      // task  [P2.6]
   P6DIR |=  0x01;      // isr   [P6.0]
   P6DIR |=  0x02;      // radio [P6.1] 
   P2DIR |=  0x08;      // light [P2.3] 
   P6DIR |=  0x08;      // user  [P6.3] 
}

// P6.6
void debugpins_frame_toggle() {
#ifdef ENABLE_DEBUGPINS
   P6OUT ^=  0x40;
#endif
}
void debugpins_frame_clr() {
#ifdef ENABLE_DEBUGPINS
   P6OUT &= ~0x40;
#endif
}
void debugpins_frame_set() {
#ifdef ENABLE_DEBUGPINS
   P6OUT |=  0x40;
#endif
}

// P6.7
void debugpins_slot_toggle() {
#ifdef ENABLE_DEBUGPINS
   P6OUT ^=  0x80;
#endif
}
void debugpins_slot_clr() {
#ifdef ENABLE_DEBUGPINS
   P6OUT &= ~0x80;
#endif
}
void debugpins_slot_set() {
#ifdef ENABLE_DEBUGPINS
   P6OUT |=  0x80;
#endif
}

// P6.2
void debugpins_fsm_toggle() {
#ifdef ENABLE_DEBUGPINS
  P6OUT ^=  0x04;
#endif
}
void debugpins_fsm_clr() {
#ifdef ENABLE_DEBUGPINS
  P6OUT &= ~0x04;
#endif
}
void debugpins_fsm_set() {
#ifdef ENABLE_DEBUGPINS
  P6OUT |=  0x04;
#endif
}

// P2.6
void debugpins_task_toggle() {
#ifdef ENABLE_DEBUGPINS
  P2OUT ^=  0x40;
#endif
}
void debugpins_task_clr() {
#ifdef ENABLE_DEBUGPINS
   P2OUT &= ~0x40;
#endif
}
void debugpins_task_set() {
#ifdef ENABLE_DEBUGPINS
   P2OUT |=  0x40;
#endif
}

// P6.0
void debugpins_isr_toggle() {
#ifdef ENABLE_DEBUGPINS
  P6OUT ^=  0x01;
#endif
}
void debugpins_isr_clr() {
#ifdef ENABLE_DEBUGPINS
  P6OUT &= ~0x01;
#endif
}
void debugpins_isr_set() {
#ifdef ENABLE_DEBUGPINS
   P6OUT |=  0x01;
#endif
}

// P6.1
void debugpins_radio_toggle() {
#ifdef ENABLE_DEBUGPINS
   P6OUT ^=  0x02;
#endif
}
void debugpins_radio_clr() {
#ifdef ENABLE_DEBUGPINS
  P6OUT &= ~0x02;
#endif
}
void debugpins_radio_set() {
#ifdef ENABLE_DEBUGPINS
  P6OUT |=  0x02;
#endif
}

// P2.3
void debugpins_light_toggle() {
#ifdef ENABLE_DEBUGPINS
  P2OUT ^=  0x08;
#endif
}
void debugpins_light_clr() {
#ifdef ENABLE_DEBUGPINS
  P2OUT &= ~0x08;
#endif
}
void debugpins_light_set() {
#ifdef ENABLE_DEBUGPINS
  P2OUT |=  0x08;
#endif
}

// P6.3
void debugpins_user_toggle() {
#ifdef ENABLE_DEBUGPINS
  P6OUT ^=  0x08;
#endif
}
void debugpins_user_clr() {
#ifdef ENABLE_DEBUGPINS
  P6OUT &= ~0x08;
#endif
}
void debugpins_user_set() {
#ifdef ENABLE_DEBUGPINS
  P6OUT |=  0x08;
#endif
}

void    leds_toggle_2x(void){
#ifdef ENABLE_DEBUGPINS
  uint16_t i;
  debugpins_task_toggle();
  for (i=0;i<0xFFFF;i++);
  for (i=0;i<0xFFFF;i++);
  debugpins_task_toggle();
#endif
}  
void    leds_toggle_4x(void){
#ifdef ENABLE_DEBUGPINS
  uint16_t i;
  leds_toggle_2x();
  for (i=0;i<0xFFFF;i++);
  for (i=0;i<0xFFFF;i++);
  leds_toggle_2x();
#endif
}

//=========================== private =========================================
