//
//  Beacon.c
//
//  Created by Atle Kleven on 31-07-2024.
//  Copyright Atle Kleven 031-07-2024. All rights reserved.
//



#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "OA_Beacon.h"



/* Part 1: Macro definitions */

	/* See also headder file */

/* Part 2: Variable definitions */


/*
 * Bits that are set inside interrupt routines, and watched outside in
 * the program's main loop.
 */
volatile struct
{
  uint8_t tmr0_int: 1;
  uint8_t tmr1_int: 1;
  uint8_t anacomp_int: 1;
  int8_t pinchange_int: 1;
}
intflags;




/* Part 3: Interrupt service routines */
	
	
ISR(TIM0_OVF_vect){
	intflags.tmr0_int = 1;		/* Mark the occurence of timer0 overflow interrupt */
	}

ISR(ANA_COMP_vect){
    intflags.anacomp_int = 1; /* Mark the occurence of an analog comparator interrupt*/
}

ISR(PCINT0_vect){
    intflags.pinchange_int = 1; /* Mark the occurence of a pin change interrupt*/
}

/* Part 4: Auxiliary functions */
/* function to start the tomert0 conbnting to TOP and generatinmg an interrupt */
// static void
//   start_timer_to_TOP(void)
//   {

//   }


/*
 * Do all the startup-time peripheral initializations.
 */
static void
	ioinit(void)
{
        /* if useing a 8 MHz clock with 8 prescaler (1000000MHz) */
        /* Set MCU master clock prescaler to 64 for a CPU_FREQUENCY of 8MHz/64=125000Hz */
        /* signal a clockchage */
        CLKPR = _BV(7);
        /* set prescaler to 64 */
        CLKPR = _BV(CLKPS2) | _BV(CLKPS1);
/* FUSE bit settings for this setup for avrdude
 * -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
 */
        
 /*
   * Set up the 8-bit timer 0.
   *
   * Timer 0 will be set up prescaled with system clock / 1024
   * producing a timer wrap around period of apporx 2 seconds
   * up-counting until overflow where an overflow intererupt will be asserted (TOEI0 in TIMSK)
   * The timer is set in fast pwm mode (WGM1 and WGM0 in TCCR0A)
   * NO: Output compare interrupt at value of OCR0A (OCIE0A in TIMSK)
   * Toggle pin OC0A (PB0) at counter compare match with OCR0A
   * The timer0 will be started by setting the prescaler (CS02..0 in TCCR0B)
   */
   
        /* set the timer to fast PWM mode */
        TCCR0A |= _BV(WGM01) | _BV(WGM00);
        /* Set to output compare match to set pin OC0A*/
        TCCR0A |= _BV(COM0A1);
        /* set the timer compare value to desired FLASH ON counter cycles*/
        OCR0A = TMR0_PWM_TRESH;
        /* Set the FLASH (OC0A) pin as output */
        FLASH_DDR |= _BV(FLASH);
        /* Enabel timer overflow interrupt*/
        TIMSK |= _BV(TOIE0);
        /* Start timing sequence by setting the prescaler */
        TCCR0B |= TMR0_PRESC_V;
 
 /*
  * Setup the analog comparator
  * Reference voltage 1,1V from bandgap voltage
  */
        /* Make sure the ADC is turned off */
        ADCSRA &= ~_BV(ADEN);
        /* enable the ADC multiplexer on analog comparator input to choose the right pin input */
        ADCSRB |= _BV(ACME);
        /* Set the adc multiplexer to pin PB3 */
        ADMUX |= _BV(MUX1) | _BV(MUX0);
        /* Enable the adc bandgap selector for the analog comparator */
        ACSR |= _BV(ACBG);
        /* Set the bandgap to 1.1V without external bypass capacitor, disconnected form PB0(AREF)*/
        ADMUX |= _BV(REFS1);
        /* Set the interrupt mode to toggle on any EDGE */ /* from night to day to night */
        ACSR &= ~_BV(ACIS1) & ~_BV(ACIS0);
        /* Disable the digital input buffer on AIN1 (the negative pin on the analog comparator)*/
        DIDR0 |= _BV(ADC3D);
        /* enable the analog comparator */
        ACSR &= ~_BV(ACD);
        /* Enable analog comparator interrupt */
        ACSR |= _BV(ACIE);
        
        
  
  /* Enable pull-ups for binary inputs *
   * Setting the PORTxn bits for pins configured as inputs,
   * DDRxn = 0, enables pullup resistors.
   */
        
        PORTB = _BV(ENABlE_FLASH_PIN);

  /*
   * Enable Port outputs:
   * setting the PORTxn bit enables the pin as output
   */
  
   #if defined (__TestSetup_STK500__)
      AMBIENT_LIGHT_DDR |= _BV(AMBIENT_LIGHT);
   #endif

	/* 
	 * Enable external interrupts on the input pins
	 */

	PCMSK |= _BV(PCINT4);
	/* Enable PIN Change interrupt */
	GIMSK |= _BV(PCIE); 




  /* Start with idle sleep mode */
    set_sleep_mode(SLEEP_MODE_IDLE);
        
	sei();			/* enable interrupts */

	
} /* END ioinit */



/* Part 5: main() */

int
main(void)
{
  /*
   * Our modus of operation.
   * MODE_IDLE means we watch out for a change from the analog comparator
   * MODE_DEEP_SLEEP means we are waiting to start the beacon
   */
 enum
  {
    MODE_IDLE,
    MODE_WAIT,
	  MODE_CHANGE,
    MODE_DEEP_SLEEP
  } __attribute__((packed)) mode = MODE_IDLE;
  
     

    uint8_t wait_state = INITIAL_WAIT; // holds the number of blink sequences to wait before really changing the state
    

	/* initialize the hardware */
  ioinit();

/* set the intfalgs.anacomp to initialize the state of the beacon */
intflags.anacomp_int = 1;

  for (;;)
    {
		
		/* For all interrupts check the system status */   
		  switch (mode)
			{
        case MODE_IDLE:
                    /*
                    * The Beacon is in DAY or NIGHT MODE 
                    * A voltage passing between the 1.1V threshold on the AMBIENT_LIGHT_ADC pin should change the mode of the beacon
                    */

                  /* If we get a pinchange interrupt in mode = MODE_IDLE we ingnore it*/
                   if (intflags.pinchange_int)
                    {
                      intflags.pinchange_int = 0;
                    }

                   /* Just check the staus of the ENABLE_FLASH_PIN */
                    if (bit_is_clear(ENABlE_FLASH_PORT,ENABlE_FLASH_PIN))
                      {
                        /* shut down the beacon and set in deepsleep */
                        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
                        mode = MODE_DEEP_SLEEP;

                        /* do some cleanup*/
                         /* disable the pwm and timer0 */
                            /* disable the timer0 interrupt */
                            TIMSK &= ~_BV(TOIE0);
                            /* Stop timing sequence by setting the prescaler to 0 */
                            TCCR0B = TMR0_PRESC_0;
                            /* disconnect compare match from the output pin FLASH (OCOA) */
                            TCCR0A &= ~_BV(COM0A1);
                            /* reset the timer0 counter register */
                            TCNT0 = 0;
                            /* power down the timer0 */
                            PRR |= _BV(PRTIM0);

                            /* disable the analog comparator interrupt*/
                            ACSR &= ~_BV(ACIE);
                            
                           /* enable FLASH as output after disconnection from timer */
                            FLASH_DDR |= _BV(FLASH);
                             /* TURN OFF the FLASH pin */
                            FLASHPORT &= ~_BV(FLASH);

                        //break;
                      }
                        
                        
                      
                      
                    /* analog comparator interrupt flag set on toggle state*/
                      if (intflags.anacomp_int)
                      {
                        /* an analog comparator interrupt has occured */
                        /* reset the analog comparator interrupt flag */
                        intflags.anacomp_int = 0;
                        /* disable the analog comparator interrupt*/
                        ACSR &= ~_BV(ACIE);

                        /* start the timer here to enable the wait state */
                        /* power up the timer0 */
                        PRR &= ~_BV(PRTIM0);
                        /* enable the timer0 top interrupt */
                        TIMSK |= _BV(TOIE0);

                        /* check the status of the analog comparator and setup the current mode */
                        if bit_is_set(ACSR, ACO){
                            /* The analog comparator is positive (NIGHT) */

#if defined (__TestSetup_STK500__)                    
  AMBIENT_LIGHT_PORT |= _BV(AMBIENT_LIGHT); // reflect this on the AMBIENT LIGHT PIN
#endif
                            /* disconnect compare match from the output pin FLASH */
                            TCCR0A &= ~_BV(COM0A1);

                              /* enable the FLASH pin as output */
                              FLASH_DDR |= _BV(FLASH);
                              FLASHPORT &= ~_BV(FLASH); // make sure the FLASH is OFF
                            /* start timing sequence by setting the prescaler */
                            TCCR0B = TMR0_PRESC_V;                
                        } 
                            
#if defined (__TestSetup_STK500__)
                        else
                            {
                            /* The analog comparator is negative (DAY) */                    
       AMBIENT_LIGHT_PORT &= ~_BV(AMBIENT_LIGHT);// reflect this on the AMBIENT LIGHT PIN
                            } /* END else */
#endif
                            
                            /* start timing sequence by setting the prescaler */
                            TCCR0B = TMR0_PRESC_V;
                        /* Change mode */
                        mode = MODE_WAIT;

                      }       
				 break;
			
      case MODE_WAIT:
                /* in this mode we wait some seconds before entering the new night or day mode */
                if (intflags.tmr0_int)
                {
                  intflags.tmr0_int = 0;
                  if (--wait_state == 0)
                  { 
                    wait_state = INITIAL_WAIT;
                    mode = MODE_CHANGE; 
                  }
                }
                
          break;
			case MODE_CHANGE:
                        /* check state of comparator after the wait to set the right mode */
                        /* the state is detected even if the interrupt is disabled */
                       if bit_is_set(ACSR, ACO){
                            // FROM_DAY;
                            /* last_mode == FROM_DAY */
                            /* Enable the OCOA pin as output, start blinking */
                            TCCR0A |= _BV(COM0A1);
                        }
                       else {
                            // FROM_NIGHT;
                            /* disable the pwm and timer0 */
                            /* disable the timer0 interrupt */
                            TIMSK &= ~_BV(TOIE0);
                            /* Stop timing sequence by setting the prescaler to 0 */
                            TCCR0B = TMR0_PRESC_0;
                            /* disconnect compare match from the output pin FLASH (OCOA) */
                            TCCR0A &= ~_BV(COM0A1);
                            /* reset the timer0 counter register */
                            TCNT0 = 0;
                            /* power down the timer0 */
                            PRR |= _BV(PRTIM0);
                            
                           /* enable FLASH as output after disconnection from timer */
                            FLASH_DDR |= _BV(FLASH);
                             /* TURN OFF the FLASH pin */
                            FLASHPORT &= ~_BV(FLASH);
                            }
                         
                        
                        /* change mode */
                        mode = MODE_IDLE;
                        /* Enable analog comparator interrupt */
                        ACSR |= _BV(ACIE);                          
       
					break;
      case MODE_DEEP_SLEEP:
                  /* check the status of the ENABLE_FLASH_PIN. Turn ON the Beacon? */
                  if (intflags.pinchange_int)
                  {
                    intflags.pinchange_int = 0;
                    if (bit_is_set(ENABlE_FLASH_PORT,ENABlE_FLASH_PIN))
                    {
                     /* waking up the beacon*/
                      /* sleep mode idle needed for the timer clocks to run */
                      set_sleep_mode(SLEEP_MODE_IDLE);
                      /* next mode MODE_IDLE*/
                      mode = MODE_IDLE;
                      /* start the timer to generate an interrupt to change mode*/
                      /* power up the timer0 */
                      PRR &= ~_BV(PRTIM0);
                      /* enable the timer0 top interrupt */
                      TIMSK |= _BV(TOIE0);
                      /* disconnect compare match from the output pin FLASH */
                      TCCR0A &= ~_BV(COM0A1);
                      /* reset the timer0 counter register */
                      TCNT0 = 0;
                      /* start timing sequence by setting the prescaler */
                      TCCR0B = TMR0_PRESC_V;
                      
                      /* Enable analog comparator interrupt */
                        ACSR |= _BV(ACIE); 
                      /* set the intfalgs.anacomp to initialize the state of the beacon */
                      intflags.anacomp_int = 1;

                    }
                  }
            break;
                  
                  
			}
			/*end case */
        
        /* The analog comparator is only enabled in the idle sleep mode (SLEEP_MODE_IDLE) */
        /* Put the MCU in sleep and wait for next interrupt */
        sleep_mode();
	  
    } /*main loop*/
}
