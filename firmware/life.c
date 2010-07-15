//Conway's Game of Rush
//Grant Elliott
//August 2005

#define F_CPU 8000000UL

#include <stdio.h>
#include <avr/io.h>
#include <avr/delay.h>
#include <avr/sleep.h>
#include "life.h"

uint8_t compatmode = 0;
volatile uint16_t colony, lastcolony;
volatile uint8_t mode = AWAKE;


#include "lifecomm.c"

const struct RULE rule_set={0b00000000,0b00000110,0b00000100};
volatile struct OUTSIDE border;

uint32_t commtimeout = 40000;


#define RESPAWN 25 // how many turns to respawn once we're dead
uint8_t deadcount = 0, staticcount = 0;

int main()
{
  uint32_t a;
  init();
  if ( !(PINC & _BV(5)) ) {
    compatmode = 1;
  }
  set_random();
  
  if (compatmode)
    commtimeout = 40000;
  else
    commtimeout = 60000;

  while(1) {
    while (mode == SLEEP) {
      colony = 0xFFFF;
      display();

      SMCR = _BV(SM1) | _BV(SE); // enable sleep
      sleep_cpu();
      _delay_ms(100);
    }
    mode = AWAKE;
    SMCR = 0;  // disable sleep
    reset_border();
    for (a=0;a<commtimeout;a++) {
      if (inprogress())
	break;
    }
    transmit(3,&fetch_trans_data);
    while (inprogress()) {}

    lastcolony = colony;
    evolve();

    if (!compatmode) {
      if (colony == 0xFFFF) {
	deadcount++;
      } else {
	deadcount = 0;
      }
      if (colony == lastcolony) {
	staticcount++;
      } else {
	staticcount = 0;
      }
      
      if ((deadcount == RESPAWN) || (staticcount == RESPAWN*2) && !compatmode) {
	staticcount = deadcount = 0;
	set_random();
      }
    }
  }
  return 0;
}

void reset_border()
{
  border.ns=0;
  border.ew=0;
  border.nd=0;
}

unsigned char fetch_trans_data(unsigned char index)
{
  uint8_t B, D;
  B = colony & 0xFF;
  D = colony >> 8;

  //This scheme only requires 20 bits, but we transmit bytewise, so it uses 24
  //Rather than simply transmit the entire map, we use an efficient scheme and
  //leave four bits available for other uses.
  //Also want to transmit corners first to ensure time for diagonal swapping
  switch(index) {
  case 0:
    //transmit N and S sides: bit0123=N, bit4567=S;
    return (~B&0x0F)|(~D&0xF0);
    break;
  case 1:
    //transmit edges: bit01=E, bit 23=W
    return ((~B&0b10000000)>>7)|((~D&0b00001000)>>2)|
      ((~B&0b00010000)>>2)|((~D&0b00000001)<<3);
    break;
  case 2:
    //bounce diagonals: bit0=NW, bit1=NE, bit2=SW, bit3=SE, bit4=EN, bit5=ES, bit6=WN, bit7=WS
    return ((border.ns&0b00010000)>>4)|((border.ns&0b10000000)>>6)|
      ((border.ns&0b00000001)<<2)|(border.ns&0b00001000)|
      (border.ew&0b00010000)|((border.ew&0b10000000)>>2)|
      ((border.ew&0b00000001)<<6)|((border.ew&0b00001000)<<4);
    break;
  default:
    return 0;
    break;
  }
}

void rx_process(unsigned char dir, unsigned char index, unsigned char data)
{
  switch (index) {
  case 0:
    //receive N and S borders
    switch (dir) {
    case 0:		//E
      border.ew|=(data&0x01)<<4;
      border.ew|=(data&0x10)<<3;
      break;
    case 1:		//N
      border.ns|=(data&0xF0);
      break;
    case 2:		//S
      border.ns|=(data&0x0F);
      break;
    case 3:		//W
      border.ew|=(data&0x08)>>3;
      border.ew|=(data&0x80)>>4;
      break;
    }
    break;
  case 1:
    switch (dir)
      {
      case 0:		//E
	border.ew|=(data&0x0C)<<3;
	break;
      case 3:		//W
	border.ew|=(data&0x03)<<1;
	break;
      }
    break;
  case 2:
    switch (dir)
      {
      case 0:		//E
	border.nd|=(data&0x01)<<1;	//NW tx -> NE rx
	border.nd|=(data&0x04)<<1;	//SW tx -> SE rx
	break;
      case 1:		//N
	border.nd|=(data&0x80)>>7;	//Ws tx -> NW rx
	border.nd|=(data&0x20)>>4; //ES tx -> NE rx
	break;
      case 2:		//S
	border.nd|=(data&0x40)>>4;	//WN tx -> SW rx
	border.nd|=(data&0x10)>>1;	//EN tx -> SE rx
	break;
      case 3:		//W
	border.nd|=(data&0x02)>>1;	//NE tx -> NW rx
	border.nd|=(data&0x08)>>1;	//SE tx -> SW rx
	break;
      }
    break;
  }
}

void init()
{
  PORTB=0;
  DDRB=0xFF;
  PORTD=0;
  DDRD=0xFF;
  DDRC&= 0b10011111;
  PORTC|=0b01100000;
  DIDR0=0x00;
  ADMUX=0x20;
  ADCSRA=0x86;
  init_rx(&rx_process);


  PCMSK1 |= _BV(5); // turn on pinchange interrupt for reset
}

void set_random()
{
  unsigned char i,t1=0,t2=0;
  for (i=0;i<8;i++) {
    t1|=(read_adc(7)&0b00000001)<<i;
    _delay_ms(2);
    t2|=(read_adc(7)&0b00000001)<<i;
    _delay_ms(2);
  }
  
  //t2 = 0xFF;
  //t1 = 0x8F;
  
  colony = t2;
  colony <<= 8;
  colony |= t1;
  display();
}

unsigned char read_adc(unsigned char adc_input)
{
  ADMUX=adc_input|0x20;
  // Start the AD conversion
  ADCSRA|=0x40;
  // Wait for the AD conversion to complete
  while ((ADCSRA & 0x10)==0);
  ADCSRA|=0x10;
  return ADCH;
}

void evolve()
{
  
  uint8_t B, D;
  B = colony & 0xFF;
  D = colony >> 8;
  
  unsigned char i,j,cnt,newstate,map[6],temp[2]={0,0};
  
  map[0]= (0b00100000&(border.nd<<4)) |	//NE (bit1)
    (0b00011110&(border.ns>>3)) |
    (0b00000001&(border.nd));	//NW (bit0)
  map[1]= (0b00100000&(border.ew<<1)) |
    (0b00011110&(~B<<1)) |
    (0b00000001&(border.ew));
  map[2]= (0b00100000&(border.ew)) |
    (0b00011110&(~B>>3)) |
    (0b00000001&(border.ew>>1));
  map[3]= (0b00100000&(border.ew>>1)) |
    (0b00011110&(~D<<1)) |
    (0b00000001&(border.ew>>2));
  map[4]= (0b00100000&(border.ew>>2)) |
    (0b00011110&(~D>>3)) |
    (0b00000001&(border.ew>>3));
  map[5]= (0b00100000&(border.nd<<2)) |	//SE (bit3)
    (0b00011110&(border.ns<<1)) |
    (0b00000001&(border.nd>>2));	//SW (bit2)
  for (i=0;i<4;i++) {
    for (j=0;j<4;j++) {
      cnt=((map[i]>>j)&1)+((map[i]>>j>>1)&1)+((map[i]>>j>>2)&1)+
	((map[i+1]>>j)&1)+((map[i+1]>>j>>2)&1)+
	((map[i+2]>>j)&1)+((map[i+2]>>j>>1)&1)+((map[i+2]>>j>>2)&1);
      if ((map[i+1]>>j>>1)&1)
	{
	  //cell currently alive
	  if (cnt==0)
	    newstate=1&rule_set.config;
	  else
	    newstate=1&(rule_set.live>>(cnt-1));
	} else {
	//cell currently dead
	newstate=1&(rule_set.born>>(cnt-1));
      }
      temp[i>>1]|=newstate<<(j|((i&1)<<2));
    }
  }
  colony = temp[1];
  colony <<= 8;
  colony |= temp[0];
  colony = ~colony;
  
  display();
}

void display(void) {
  PORTB=~(colony & 0xFF);
  PORTD=~(colony >> 8);
}
