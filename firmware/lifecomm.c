//Conway's Game of Rush
//Communication Library
//Grant Elliott
//August 2005

#include <stdio.h>
#include <avr/io.h>
#include <avr/delay.h>
#include <avr/signal.h>
#include <avr/interrupt.h>
#include "lifecomm.h"

volatile unsigned char rx_status;
//Top nib - Last pin states measured
//Bot nib - Transmissions in progress
//Bit ordering - WSNE (matches PORTC ordering)
volatile unsigned char rx_safety;
volatile unsigned char rx_data[4]={0,0,0,0};
volatile signed char rx_bit[4]={0,0,0,0};//the location of the next bit to load
volatile void (*rx_handler)(unsigned char, unsigned char, unsigned char);

void transmit(unsigned char numbytes, unsigned char (*txfunc)(unsigned char))
{
	unsigned char mybyte=0,data,i;
	PORTC&=0b11111110;	//initiate a transfer by pulling low
	_delay_ms(TX_DELAY);
	//send two garbage bits to sync... this shouldn't be necessary... it's a hack
	PORTC|=0b0000001;
	_delay_ms(TX_DELAY);
	PORTC&=0b11111110;
	_delay_ms(TX_DELAY);
	_delay_ms(TX_DELAY);
	PORTC|=0b0000001;
	_delay_ms(TX_DELAY);
	for (mybyte=0;mybyte<numbytes;mybyte++)
	{
		data=txfunc(mybyte);
		for (i=0;i<8;i++)
		{
			PORTC=(PORTC&0b11111110)|((~data)&1); //set up for the transfer		
			_delay_ms(TX_DELAY);
			PORTC^=0b00000001;					//the bit transmission
			_delay_ms(TX_DELAY);
			data=data>>1;
		}
	}
	PORTC|=1;	//pull high again to get ready for next transmission
}

volatile unsigned char inprogress()
{
	return rx_status&0x0F;
}

void init_rx(void (*func)(unsigned char, unsigned char, unsigned char))
{
	DDRC|=0b00000001;	//C0 is broadcast
	DDRC&=0b11100001;	//C1-C4 are receive
	PORTC|=0b00011111;	//pullups enabled, broadcast line high
	PCICR=0x02;
	PCMSK1=0x1E;
	TIMSK0=0x00;
	TCCR0A=0x00;
	TCCR0B=0x03;
	TCNT0=0x00;
	OCR0A=0x00;
	OCR0B=0x00;
	ASSR=0x00;
	TIMSK2=0x00;
	TCCR2A=0x00;
	TCCR2B=0x04;
	TCNT2=0x00;
	OCR2A=0x00;
	OCR2B=0x00;
	sei();
	rx_status=0xF0;
	rx_safety=0x00;
	rx_handler=func;
}

SIGNAL(SIG_PIN_CHANGE1)
{

  if (!compatmode) {
    if ((PCMSK1 & _BV(5)) && !(PINC & _BV(5))) { // button
      colony = 0xA5A5;
      display();
      if (mode == SLEEP) {
	mode = AWAKE;
	SMCR = 0;  // disable sleep
	return;
      }
      uint16_t temp16 = 0;
      while ( !(PINC & _BV(5)) && (temp16 <= 200) ) {
	_delay_ms(10);
	temp16++;
      }
      if (temp16 >= 200) {
	// held down for a long time, time to power down
	colony = 0xFFFF;
	display();
	while (! (PINC & _BV(5)) );
	_delay_ms(10);
	mode = SLEEP;
      } else {
	set_random();
      }
      return;
    } 
    
    if (mode == SLEEP)
      return;
  }

	unsigned char temp;
	signed char i;
	temp=((0x0F&(PCMSK1>>1))|(0xF0&(PCMSK1<<3)));
	temp&=((rx_status<<4)&((PINC<<3)^rx_status)&0xF0)|((~((PINC>>1)|rx_status))&0x0F);
	for (i=3;i>=0;i--)
	{
		if ((temp>>i)&1)
		{
			//Start a reception
			rx_status|=0x01<<i;
			rx_bit[i]=-2;
			//Disable PCINT for this side
			PCMSK1&=~(0b00000010<<i);
			//Enable and set the appropriate timer
			rx_safety|=0x01<<i;
			switch(i)
			{
				case 0:
					OCR0A=RX_DELAY+TCNT0;//E
					TIMSK0|=0b00000010;
				break;
				case 1:
					OCR0B=RX_DELAY+TCNT0;//N
					TIMSK0|=0b00000100;
				break;
				case 2:
					OCR2A=RX_DELAY+TCNT2;//S
					TIMSK2|=0b00000010;
				break;
				case 3:
					OCR2B=RX_DELAY+TCNT2;//W
					TIMSK2|=0b00000100;
				break;
			}
		} else if ((temp>>4>>i)&1)
		{
			//Receive a bit
			if (rx_bit[i]>=0) //ignore magic sync bits
			{
				rx_data[i]=(rx_data[i]&~(0x01<<(rx_bit[i]&0x07)))|(((PINC>>1>>i)&1)<<(rx_bit[i]&0x07));
				if ((rx_bit[i]&0x07)==0x07)
				{
					rx_handler(i,(rx_bit[i]&0x7F)>>3,rx_data[i]);
				}
			}	
			rx_bit[i]++;
			//Disable PCINT for this side
			PCMSK1&=~(0b00000010<<i);
			//set the appropriate timer
			rx_safety|=0x01<<i;
			switch(i)
			{
				case 0:
					OCR0A=RX_DELAY+TCNT0;//E
				break;
				case 1:
					OCR0B=RX_DELAY+TCNT0;//N
				break;
				case 2:
					OCR2A=RX_DELAY+TCNT2;//S
				break;
				case 3:
					OCR2B=RX_DELAY+TCNT2;//W
				break;
			}
		}
	}
}

SIGNAL(SIG_OUTPUT_COMPARE0A)//E
{
	if (rx_safety&0b00000001)
	{
		//record curent state
		rx_status=(rx_status&0b11101111)|((PINC<<3)&0b00010000);	
		//enable the PCINT
		PCMSK1|=0b00000010;
		//setup the safety timout
		rx_safety&=0b11111110;
		OCR0A=RX_TIMEOUT+TCNT0;
	} else {
		//disable the timer
		TIMSK0&=0b11111101;
		//clear the flag
		rx_status&=0b11111110;
		//process the data we got
		//rx_handler(0,(rx_bit[0]&0x7F)>>3,rx_data[0]);
		//enable the PCINT
		PCMSK1|=0b00000010;
	}
}

SIGNAL(SIG_OUTPUT_COMPARE0B)//N
{
	if (rx_safety&0b00000010)
	{	
		//record curent state
		rx_status=(rx_status&0b11011111)|((PINC<<3)&0b00100000);	
		//enable the PCINT
		PCMSK1|=0b00000100;
		//setup the safety timout
		rx_safety&=0b11111101;
		OCR0B=RX_TIMEOUT+TCNT0;
	} else {
		//disable the timer
		TIMSK0&=0b11111011;
		//clear the flag
		rx_status&=0b11111101;
		//process the data we got
		//rx_handler(1,(rx_bit[1]&0x7F)>>3,rx_data[1]);
		//enable the PCINT
		PCMSK1|=0b00000100;
	}
}

SIGNAL(SIG_OUTPUT_COMPARE2A)//S
{
	if (rx_safety&0b00000100)
	{
		//record curent state
		rx_status=(rx_status&0b10111111)|((PINC<<3)&0b01000000);	
		//enable the PCINT
		PCMSK1|=0b00001000;
		//setup the safety timout
		rx_safety&=0b11111011;
		OCR2A=RX_TIMEOUT+TCNT2;
	} else {
		//disable the timer
		TIMSK2&=0b11111101;
		//clear the flag
		rx_status&=0b11111011;	
		//process the data we got
		//rx_handler(2,(rx_bit[2]&0x7F)>>3,rx_data[2]);
		//enable the PCINT
		PCMSK1|=0b00001000;
	}
}

SIGNAL(SIG_OUTPUT_COMPARE2B)//W
{
	if (rx_safety&0b00001000)
	{
		//record curent state
		rx_status=(rx_status&0b01111111)|((PINC<<3)&0b10000000);	
		//enable the PCINT
		PCMSK1|=0b00010000;
		//setup the safety timout
		rx_safety&=0b11110111;
		OCR2B=RX_TIMEOUT+TCNT2;
	} else {
		//disable the timer
		TIMSK2&=0b11111011;
		//clear the flag
		rx_status&=0b11110111;	
		//process the data we got
		//rx_handler(3,(rx_bit[3]&0x7F)>>3,rx_data[3]);
		//enable the PCINT
		PCMSK1|=0b00010000;
	}
}
