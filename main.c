/* 
 * File:   main.c
 * Author: user
 *
 * Created on 2014. március 13., 15:12
 */

#include <pic16f873.h>
#include <stdio.h>
#include <stdlib.h>
//#include <math.h>

#define F_OSC 18430000l   // quartz frequency
#define F_ICLK  (F_OSC / 4) // internal clock

#define F_TMR0  (F_ICLK / 256)  // TMR0 timer frequency 17998,047


// CONFIG
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config CP = OFF         // FLASH Program Memory Code Protection bits (Code protection off)
#pragma config BOREN = OFF      // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = ON         // Low Voltage In-Circuit Serial Programming Enable bit (RB3/PGM pin has PGM function; low-voltage programming enabled)
#pragma config CPD = OFF        // Data EE Memory Code Protection (Code Protection off)
#pragma config WRT = ON         // FLASH Program Memory Write Enable (Unprotected program memory may be written to by EECON control)

/* The measuring opto disc slots number (1 round = DSIC_SLOTS impulse.)*/
#define DISC_SLOTS  60

#define DISPLAY_SHIFT_BEGIN 0b00000001
#ifdef DEBUG
#define TIMER_0_COUNT 0             // most slowly TMR0 to debug 
#else
#define TIMER_0_DIVIDER 45
#define TIMER_0_COUNT (255 - TIMER_0_DIVIDER)    // timer 0 base
#endif

#define T_RPM_BASE (F_TMR0 / TIMER_0_DIVIDER)   /* RPM_BASE and display refresh timer0 interrupt,
                                                  1/400th sec. exact 399,956 */
#define BUTTON PORTBbits.RB0

#define DP_00 PORTBbits.RB1
#define DP_01 PORTBbits.RB2
#define DP_02 PORTBbits.RB3

#define RPM_INPUT PORTCbits.RC0 // for RPM_INPUT port test.

double MEAS_RPM_FACTORS[8] = {0.25, 0.5, 1.0, 2.0};
//double MEAS_RPS_FACTORS[8] = {0.00417, 0.0084, 0.017, 0.034, 0.067, 0.167, 0.333, 0.667};

unsigned char BCD_DISPLAY[4] = {0,0,0,0};
unsigned int MEAS_FACTORS[4] = {1600, 800, 400, 200};


/* pointer the current meas factor in MEAS_FACTORS table. */
unsigned int PTR_MEAS_FACTOR = 3;

/* Mirror the counting impulse. (TMR1H,L copy) */
unsigned int COUNTER_MIRROR;

typedef enum E_PRG_STATE {
  meas = 0,
  input_check = 1
};

/* Measure status word */
union {
  struct
  {
    /* Disable the next measuring cycle. */
    unsigned MEAS_INH: 1;
    /* Avaliable new counter data in.*/
    unsigned NEW_DATA: 1;
    /* RPM, or RPS meas dim. */
    unsigned RPM_RPS: 1;
  };
  }MEAS_STATUS;


int BUTTON_MIRROR, BUTTON_PRESSED;

enum E_PRG_STATE PRG_STATE = meas;

/* Used the RPM counter input check. */
int RPM_INPUT_MIRROR = 3;

/* MEAS_TIME_FACTOR: factor for measuring process's control.*/
int MEAS_TIME_FACTOR;

/* TIMER_COUNTER: counter for RPM rime base measuring. */
int TIMER_COUNTER;

/* Want to change auto range? */
int IS_AUTO_RANGE = 1;

/* diplya decimal point counter for the time multiplexed showing. */
unsigned int display_decimal = 0;
#ifdef DEBUG
unsigned int bit_flag;
#endif

void DisplayOverflow()
{ int i;
  for (i =0; i < 4; i++)
  {
    BCD_DISPLAY[i] = 12;  // u symbol onto display.
  }
}

unsigned char BCD_BUF[4];

void DisplayResult(unsigned int value)
{ int i; 
  int res = value;
  int div;
  for (i = 0; i < 4; i++)
  {
    div = res % 10;
    res = res / 10;
    BCD_DISPLAY[3-i] = div;
  }
}

/* StartNewMeasure: Starting the TMR1 as counter to measuring the inpulse.
 */

void StartNewMeasure()
{
  MEAS_TIME_FACTOR = MEAS_FACTORS[PTR_MEAS_FACTOR];
  INTCONbits.GIE = 0;   /* Global interrupt disabled. */
  TIMER_COUNTER = 0;
  TMR1L = 0;
  TMR1H = 0;
  T1CONbits.TMR1ON = 1;     /* Restart the TMR1 timer */
  INTCONbits.GIE = 1;   /* Global interrupt enabled. */ 
}

/******************************************************************************/
/* General interrupt handler for treats the timer, and counter interrupts     */
/******************************************************************************/

void interrupt isr(void)
{
    /* Determine which flag generated the interrupts */
    if(INTCONbits.RBIF) // If RB port changed. Not using now ...
    {
//        int_counter++;
        INTCONbits.RBIF = 0; /* Clear RB port changed interrupt flag. */
    }
    else if (PIR1bits.TMR1IF)   // TMR 1 overflow interrupt we must change measuring range
    {
      T1CONbits.TMR1ON = 0; // Stop the T1 immediately.
      MEAS_STATUS.NEW_DATA = 0;         //
      if (IS_AUTO_RANGE)
      { /* overflow decrease RPM sampling time. */
//        MEAS_TIME_FACTOR = MEAS_FACTORS[PTR_MEAS_FACTOR];
      } else
      {
        DisplayOverflow();  /* Overflow symbol to display.*/
      }
      StartNewMeasure();  // Always we have starting new measuring...
      PIR1bits.TMR1IF = 0;  // Clear timer1 interrupt flag
    }
    else if (PIR1bits.TMR2IF) // TMR 2 the time base for RPM measuring
    {

    }
    else if (INTCONbits.T0IF) /* T0 interrupt we can take the next display
                               * decimal number, and the RPM meter time base is
                               * here too. */
    {
    /* Is the measuring time out ?*/
      if ((TIMER_COUNTER++ == MEAS_TIME_FACTOR) && (!MEAS_STATUS.MEAS_INH))
      {
        T1CONbits.TMR1ON = 0;     /* STOP the TMR1 counter */
        COUNTER_MIRROR = TMR1H << 8;
        COUNTER_MIRROR |= TMR1L & 0xFF;
        MEAS_STATUS.NEW_DATA = 1; /* Signal the main program, that avaliable the new
                                   * RPM data in TMR1. */
//        StartNewMeasure();
      };

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

/* Not used, only testing purpose.*/
void wait()
{ int i;
  for (i = 0; i < 10000; i++)
  {

  };
}

unsigned int CalculateRPM()
{ 
  unsigned int result;

  if (PTR_MEAS_FACTOR > 2)
  {
    result = COUNTER_MIRROR << (PTR_MEAS_FACTOR - 2);
  } else
  {
    result = COUNTER_MIRROR >> (PTR_MEAS_FACTOR - 2);
  }

  return result;
  
}

/*
 * 
 */
int main(int, char**){

  int i;
  unsigned int RESULT;
  
  /* ----------------- PORT SETTINGS -----------------------------*/

  TRISC = 0b00001111;       // PORTC 4-7 out, 0-3 in

  TRISB = 0b11110001;

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
  T1CONbits.T1OSCEN = 0;  // Enabled T1 timer (input the T1CKI/RC0 port)
//  T1CONbits.TMR1ON = 1; // On the T1 timer
  TMR1H = 0;
  TMR1L = 0;
  T1CONbits.T1CKPS = 0b00;  //

  /* ------------ Global interrupt enabled ----------------------------*/

//  INTCONbits.GIE = 1;   // global interrupt enabled

  PORTA = 0xFE;
//  PORTBbits.RB7 = 0;

  /* Beginning initialize. */

  MEAS_STATUS.MEAS_INH = 0;
  StartNewMeasure();

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

          if (MEAS_STATUS.NEW_DATA) /* Is avaliable new measured data ?*/
          {
            RESULT = CalculateRPM();
            DisplayResult(RESULT);
            MEAS_STATUS.NEW_DATA = 0; /* The data aquisticun successed, and
                                       * new data request. */
            StartNewMeasure();
          };
          
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

  return (EXIT_SUCCESS);
}

