// EdgeInterrupt.c
// Runs on LM4F120 or TM4C123
// Request an interrupt on the falling edge of PF4 (when the user
// button is pressed) and increment a counter in the interrupt.  Note
// that button bouncing is not addressed.
// Daniel Valvano
// September 14, 2013

/* This example accompanies the book
   "Embedded Systems: Introduction to ARM Cortex M Microcontrollers"
   ISBN: 978-1469998749, Jonathan Valvano, copyright (c) 2013
   Volume 1, Program 9.4
   
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013
   Volume 2, Program 5.6, Section 5.5

 Copyright 2013 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// user button connected to PF4 (increment counter on falling edge)

#define NVIC_EN0_R              (*((volatile unsigned long *)0xE000E100))  // IRQ 0 to 31 Set Enable Register
#define NVIC_PRI7_R             (*((volatile unsigned long *)0xE000E41C))  // IRQ 28 to 31 Priority Register
#define GPIO_PORTF_DIR_R        (*((volatile unsigned long *)0x40025400))
#define GPIO_PORTF_IS_R         (*((volatile unsigned long *)0x40025404))
#define GPIO_PORTF_IBE_R        (*((volatile unsigned long *)0x40025408))
#define GPIO_PORTF_IEV_R        (*((volatile unsigned long *)0x4002540C))
#define GPIO_PORTF_IM_R         (*((volatile unsigned long *)0x40025410))
#define GPIO_PORTF_RIS_R        (*((volatile unsigned long *)0x40025414))
#define GPIO_PORTF_ICR_R        (*((volatile unsigned long *)0x4002541C))
#define GPIO_PORTF_AFSEL_R      (*((volatile unsigned long *)0x40025420))
#define GPIO_PORTF_PUR_R        (*((volatile unsigned long *)0x40025510))
#define GPIO_PORTF_PDR_R        (*((volatile unsigned long *)0x40025514))
#define GPIO_PORTF_DEN_R        (*((volatile unsigned long *)0x4002551C))
#define GPIO_PORTF_AMSEL_R      (*((volatile unsigned long *)0x40025528))
#define GPIO_PORTF_PCTL_R       (*((volatile unsigned long *)0x4002552C))
#define SYSCTL_RCGC2_R          (*((volatile unsigned long *)0x400FE108))
#define SYSCTL_RCGC2_GPIOF      0x00000020  // port F Clock Gating Control

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

// global variable visible in Watch window of debugger
// increments at least once per button press
volatile long counter = 0;

void EdgeCounter_Init(void){  
	counter = 0;               // (b) initialize counter	
	// GPIO for PF0 (SW2)
	SYSCTL_RCGC2_R |= 0x00000020;   // (a) activate clock for port F
	GPIO_PORTF_DIR_R &= ~0x01;      // (c) make PF0 input (typically for built-in button)
	GPIO_PORTF_AFSEL_R &= ~0x01;    //     disable alt function on PF0
	GPIO_PORTF_DEN_R |= 0x01;       //     enable digital I/O on PF0
	GPIO_PORTF_PCTL_R &= ~0x0000000F; // configure PF0 as GPIO
	GPIO_PORTF_AMSEL_R = 0;         //     disable analog functionality on PF
	GPIO_PORTF_PUR_R |= 0x01;       //     enable weak pull-up on PF0
	// GPIO for PF4 (SW1)
  SYSCTL_RCGC2_R |= 0x00000020; // (a) activate clock for port F
  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
	
	// Interrupt for PF0
  GPIO_PORTF_IS_R &= ~0x01;     // (d) PF0 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x01;    //     PF0 is not both edges
  GPIO_PORTF_IEV_R &= ~0x01;    //     PF0 falling edge event
  GPIO_PORTF_ICR_R |= 0x01;      // (e) clear flag0
  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF0
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFFFFFF0F)|0x00000080; // (g) priority 4
	// 0xFFFFFF0F = 1111 1111 1111 1111 1111 1111 0000 1111
	// 0x00000080 = 0000 0000 0000 0000 0000 0000 1000 0000
  NVIC_EN0_R = 0x10000000;      // (h) enable interrupt 28 in NVIC
	// group 0 , INTA (5,6,7), priority 4 => 100
	// 28 / 32 = 0 => EN0  , 28 % 30 = 28 => Bit-28 (0x10000000)
	// 28 / 4  = 7 => PRI7 , 28 % 4  = 0  => INTA
	
	// Interrupt for PF4
  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R |= 0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R |= 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	
  EnableInterrupts();           // (i) Clears the I bit
}

// ISR Code
void GPIOPortF_Handler(void){
	// chechk for PF0 (SW2)
	if( GPIO_PORTF_RIS_R &(1<<0) ){
		GPIO_PORTF_ICR_R |= 0x01;      // acknowledge flag0
		counter -= 2 ;
		if( counter < 0 ){	counter = 14;	}
	}
	// chechk for PF4 (SW1)
	if( GPIO_PORTF_RIS_R &(1<<4) ){
		GPIO_PORTF_ICR_R |= 0x10;      // acknowledge flag4
		counter += 2 ;
		if( counter >= 16 ){	counter = 0;	}
	}
}

//debug code
int main(void){
  EdgeCounter_Init();           // initialize GPIO Port F interrupt
  while(1){
    
  }
}
