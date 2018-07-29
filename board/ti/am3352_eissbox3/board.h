/*
 * board.h
 *
 * Board functions for TI AM3352 EISSBox3 board
 *
 * Copyright (C) 2018, IPKeys Technologies, LLC - http://www.ipkeys.com/
 *
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#define EMIF_OCP_CONFIG_EISSBOX3	0x00141414

/*
 * Three pin mux functions that must exist.  
 * 1. We must be able to enable uart0, for initial output
 * 2. i2c0 to setup PMIC.
 * 3. We then have a main pinmux function that can be overridden
 *    to enable all other pinmux that is required on the board.
 */
void enable_uart0_pin_mux(void);
void enable_i2c0_pin_mux(void);
void enable_board_pin_mux(void);

#endif
