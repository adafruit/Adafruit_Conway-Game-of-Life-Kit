//Conway's Game of Rush
//Communication Library
//Grant Elliott
//August 2005

#define RX_DELAY	200 //cycles (1/125kHz)
#define RX_TIMEOUT	250 //cycles (1/125kHz)
#define TX_DELAY	1	//ms

void transmit(unsigned char numbytes, unsigned char (*func)(unsigned char));
void init_rx(void (*func)(unsigned char, unsigned char, unsigned char));
volatile unsigned char in_progress();