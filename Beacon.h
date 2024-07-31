/*
 *  Beacon.h
 *  
 *
 *  Created by Atle Kleven on 06.11.23
 *  Copyright 2023. All rights reserved.
 *  version 2023.01 06-11-2023
 */

#ifndef __Beacon_h__
#define __Beacon_h__

#define FLASH_VERSION 2023
#define FLASH_SUBVERSION 1

#ifndef __debug_mode__
    #define __debug_mode__
/* comment next line to enable debug code */
    #undef __debug_mode__
#endif

/* uncomment next line when running on test setup STK500V2*/
#ifndef __TestSetup_STK500__
    #define __TestSetup_STK500__
/* uncomment next line to  enable compiling for STK500 test setup*/
    #undef __TestSetup_STK500__
#endif

/* output ports*/
#if defined (__TestSetup_STK500__)
    #define	AMBIENT_LIGHT_PORT   PORTB
    #define AMBIENT_LIGHT_DDR   DDRB
    #define AMBIENT_LIGHT	    PB1

    #define CLOCKPORT       PORTB
    #define CLOCK_DDR       DDRB
    #define CLOCK			PB2
#endif

#define	FLASHPORT		PORTB
#define FLASH_DDR       DDRB
#define FLASH		    PB0

/* input ports*/
#define AMB_LIGHT_ADC_PORT      PINB
#define AMB_LIGHT_ADC_PULLUP    PORTB
#define AMB_LIGHT_ADC	        PB3


//#define F_CPU 1000000UL	/* CPU clock in Hertz */
#define F_CPU 125000UL	/* Target CPU clock in Hertz */
/* FUSE bit settings for this setup for avrdude
 * -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
 */

/* prescaler cystem clock / 1024 (CT/1024) */
#define TMR0_PRESC_V _BV(CS02) | _BV(CS00)
#define TMR0_PRESC_0 ~_BV(CS02) & ~_BV(CS01) & ~_BV(CS00)
#define TMR0_PRESCALER 1024
#define TMR0_WAIT 11																		/* in 8 x miliseconds */
#define TMR0_CLK ((((F_CPU * 10) / ((1000UL * TMR0_PRESCALER) / TMR0_WAIT )) + 9) / 10)  	 /* in timer clock cycles */
#define TMR0_START (255 - TMR0_CLK)														/* starting value of Timer */
#define TMR0_PWM_TRESH (TMR0_WAIT)

/* set number of FLASH cycles to wait before actually changing the mode of the Beacon */
#define INITIAL_WAIT 5

#endif

