/* 
 * File:   main.c
 * Author: user
 *
 * Created on 2014. március 13., 15:12
 */

#include <pic16f873.h>
#include <stdio.h>
#include <stdlib.h>

#define F_OSC 18430000l   // quartz frequency
#define F_ICLK  (F_OSC / 4) // internal clock

#define F_TMR0  (F_ICLK / 256)  // TMR0 timer frequency


// CONFIG
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON      // Power-up Timer Enable bit (PWRT disabled)
#pragma config CP = OFF         // FLASH Program Memory Code Protection bits (Code protection off)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = ON         // Low Voltage In-Circuit Serial Programming Enable bit (RB3/PGM pin has PGM function; low-voltage programming enabled)
#pragma config CPD = OFF        // Data EE Memory Code Protection (Code Protection off)
#pragma config WRT = ON         // FLASH Program Memory Write Enable (Unprotected program memory may be written to by EECON control)

#define DISPLAY_SHIFT_BEGIN 0b00000001
#ifdef DEBUG
#define TIMER_0_COUNT 0             // most slowly TMR0 to debug 
#else
#define TIMER_0_DIVIDER 45
#define TIMER_0_COUNT (255 - TIMER_0_DIVIDER)    // for suppress display flickering
#endif

#define T_RPM_BASE (F_TMR0 / TIMER_0_DIVIDER)

#define BUTTON PORTBbits.RB0
#define BUTTON_TRIS TRISBbits.TRISB0

#define RPM_INPUT PORTCbits.RC0 // for RPM_INPUT port test.

unsigned char BCD_DISPLAY[4] = {0,0,0,0};

typedef enum E_PRG_STATE {
  meas = 0,
  input_check = 1
};

typedef enum E_MEAS_RANGE {
  s4,
  s2,
  s1
};

int BUTTON_MIRROR, BUTTON_PRESSED;

enum E_PRG_STATE PRG_STATE = meas;
/* In beginning 4 sec measuring time. */
enum E_MEAS_RANGE MEAS_RANGE = s4;

int RPM_INPUT_MIRROR = 3;

unsigned int display_decimal = 0;
#ifdef DEBUG
unsigned int bit_flag;
#endif
unsigned char PC = 0;

/******************************************************************************/
/* General interrupt handler for treats the timer, and counter interrupts     */
/******************************************************************************/

void interrupt isr(void)
{
    /* Determine which flag generated the interrupts */
    if(INTCONbits.RBIF) // If RB port changed ...
    {
//        int_counter++;
        INTCONbits.RBIF = 0; /* Clear RB port changed interrupt flag. */
    }
    else if (PIR1bits.TMR1IF)   // TMR 1 overflow interrupt we must change measuring range
    {
/*        int_counter++;
        Timer1OFF();        // Stop TMR1 timer
        LoadTMR1(TMR1_LOAD_DATA);        //reload tmr1
        Timer1ON();         Then start TMR1 again. */
        PIR1bits.TMR1IF = 0;  // Clear timer1 interrupt flag
    }
    else if (INTCONbits.T0IF) /* T0 interrupt we can take the next display
                               * decimale number */
    {


      /* This is a debug slower cycle.*/
#ifdef DEBUG
      if ((bit_flag++) == 13)
      {
#endif
  /* Out to decimal number. */
      PORTC = BCD_DISPLAY[display_decimal] << 4;
      /* Switch the anode transistors. */
      switch (display_decimal)
      {
        case 0:
        {
          PORTA = 0b00000111;
        } break;
        case 1:
        {
          PORTA = 0b00001011;
        } break;
        case 2:
        {
          PORTA = 0b00001101;
        } break;
        case 3:
        {
          PORTA = 0b00001110;
          if (BUTTON != BUTTON_MIRROR)
          {
            /* depressed button. */
            if (!BUTTON)
            {
              BUTTON_PRESSED = 1;
            }
            
            BUTTON_MIRROR = BUTTON;
          }
        } break;
      }

      display_decimal++;
      if ((display_decimal) == 4)
      {
        display_decimal = 0;
      }

#ifdef DEBUG
      bit_flag = 0;
      }
#endif
      TMR0 = TIMER_0_COUNT; // Reload the timer0 value.
      INTCONbits.T0IF = 0;  // Clear T0 interrupt flag.
    }
}

void wait()
{ int i;
  for (i = 0; i < 10000; i++)
  {

  };
}

/*
 * 
 */
int main(int, char**){

  int i;

  /* ----------------- PORT SETTINGS -----------------------------*/

  TRISC = 0b00001111;       // PORTC 4-7 out, 0-3 in

  BUTTON_TRIS = 1;          // button port input

  PORTA = 0;
  ADCON1bits.PCFG = 0b0110; // All of PORT A pin are digital.
  TRISA = 0x00;             // outputs...

  /* ------------------------------------------------------------- */

  /* ------------ TIMER0 to display treatment ------------------------ */
  TMR0 = TIMER_0_COUNT;

  OPTION_REGbits.T0CS = 0;  // Internal instruction cycle clock (CLKOUT)
  OPTION_REGbits.PSA = 0;   //Prescaler is assigned to the Timer0 module

  OPTION_REGbits.PS = 0b111;  // prescaler = 1 : 256

  INTCONbits.T0IF = 0;
  INTCONbits.T0IE = 1;  // Enable T0 nterrupt

  /* --------------------- TMR1 to RPM counter ------------------------*/

  T1CONbits.TMR1CS = 1; // input from T1OSI port
  T1CONbits.T1OSCEN = 1;  // Enabled T1 timer (input the T1CKI/RC0 port)
//  T1CONbits.TMR1ON = 1; // On the T1 timer
  TMR1H = 0;
  TMR1L = 0;
  T1CONbits.T1CKPS = 0b00;  //

  /* ------------ Global interrupt enabled ----------------------------*/

  INTCONbits.GIE = 1;   // global interrupt enabled

  PORTA = 0xFE;
//  PORTBbits.RB7 = 0;

  int u;

  while (1)
  {
    if (BUTTON_PRESSED)
    {
      /* Incremental the program state. */
      PRG_STATE++;
    };
    switch (PRG_STATE)
      {
        case meas:   // Normal RPM measuring
        {
          T1CONbits.TMR1ON = 1;
          for (u = 0; u < 1000; u++)
          {

          }
          T1CONbits.TMR1ON = 0;
          
        } break;
        case input_check:   // input check
        {
          if (RPM_INPUT != RPM_INPUT_MIRROR)
          {
            if (RPM_INPUT)
            {
              for (i = 1; i < 4; i++)
                BCD_DISPLAY[i] = 12;
            }else
            {
              for (i = 1; i < 4; i++)
                BCD_DISPLAY[i] = 10;
            }
            RPM_INPUT_MIRROR = RPM_INPUT;
          }
        } break;
      }
      BCD_DISPLAY[0] = PRG_STATE;
      BUTTON_PRESSED = 0;
  }



/*  while (1)
  {
    PORTA = ~display_shift;
    wait();
    display_shift = display_shift << 1;
    if (segm_no++ == 5)
    {
      display_shift = DISPLAY_SHIFT_BEGIN;
      segm_no = 0;
    }
  }*/

  return (EXIT_SUCCESS);
}

