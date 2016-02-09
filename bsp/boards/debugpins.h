#ifndef __DEBUGPINS_H
#define __DEBUGPINS_H

/**
\addtogroup BSP
\{
\addtogroup debugpins
\{

\brief Cross-platform declaration "leds" bsp module.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, February 2012.
*/

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

//=========================== prototypes ======================================

void debugpins_init(void);

void debugpins_frame_toggle(void);
void debugpins_frame_clr(void);
void debugpins_frame_set(void);

void debugpins_slot_toggle(void);
void debugpins_slot_clr(void);
void debugpins_slot_set(void);

void debugpins_fsm_toggle(void);
void debugpins_fsm_clr(void);
void debugpins_fsm_set(void);

void debugpins_task_toggle(void);
void debugpins_task_clr(void);
void debugpins_task_set(void);

void debugpins_isr_toggle(void);
void debugpins_isr_clr(void);
void debugpins_isr_set(void);

void debugpins_radio_toggle(void);
void debugpins_radio_clr(void);
void debugpins_radio_set(void);

void debugpins_rxlight_toggle(void);
void debugpins_rxlight_clr(void);
void debugpins_rxlight_set(void);

void debugpins_txlight_toggle(void);
void debugpins_txlight_clr(void);
void debugpins_txlight_set(void);

/**
\}
\}
*/

#endif
