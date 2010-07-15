
#define F_CPU 8000000UL


struct RULE {unsigned char config; unsigned char live; unsigned char born;};
struct OUTSIDE {unsigned char ns; unsigned char ew; unsigned char nd;};


void init(void);
void set_random(void);
void evolve(void);
struct RULE choose_rules();
uint8_t read_adc(uint8_t adc_input);
uint8_t fetch_trans_data(uint8_t index);
void rx_process(uint8_t dir, uint8_t index, uint8_t data);
void reset_border();
#define AWAKE 0
#define SLEEP 1
