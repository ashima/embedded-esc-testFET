#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <avr/wdt.h>

//#include "TWISlaveMem14.c"
extern "C" void setup(uint8_t addr, uint8_t* bs, int rlen, int wlen);

// We use the watchdog timer to guard against bugs that would leave FETs on for more than
// the minimum watchdog timeout (~15ms). The following code is adapted from
// http://www.nongnu.org/avr-libc/user-manual/group__avr__watchdog.html
uint8_t mcucsr_mirror __attribute__ ((section(".noinit")));

void get_mcucsr(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));
void get_mcucsr(void) {
  mcucsr_mirror = MCUCSR;
  MCUCSR = 0;
  wdt_disable();
}

/*
each element of uint8_t states[] will contain, from LSB to MSB:
[Ap, An, Bp, Bn, Cp, Cn, sense_common] = map(lambda n: 2**n, [0,1,2,3,4,5,6])

We need to map these into PORTB,C,D

Ideally, we could define each FET pin on the ESC via:

Cp = (PORTD, 5)

Cp_pin = Pin<PORTD, 5>

Then we could loop through ds

*/
//typedef union {
//  uint8_t val;
//  struct bits {
//    uint8_t Ap:1;
//    uint8_t An:1;
//    uint8_t Bp:1;
//    uint8_t Bn:1;
//    uint8_t Cp:1;
//    uint8_t Cn:1;
//    bool com:1;
//    uint8_t res:1;
//  } bits;
//} state_t;
//
//typedef struct {
//    uint8_t Ap      :1;
//    uint8_t An      :1;
//    uint8_t Bp      :1;
//    uint8_t res3    :5;
//} PortD;
//
//typedef union {
//  uint8_t val;
//  struct bits {
//    uint8_t res0:2;
//    uint8_t Cn :1;
//    uint8_t res4:5;
//  } bits;
//} PortB;
//
//uint8_t pB_test(uint8_t v) {
//  state_t vv; vv.val = v;
//  PortB p;
//  
//  p.bits.Cn = vv.bits.Cn;
////  #ifdef PortA
////  p.Bp = ((state_t*)&v)->Bp;
////  #endif
//  
//  return p.val;
//}
//
//uint8_t pD_test(uint8_t v) {
//  state_t vv; vv.val = v;
//  PortD p;
//  
//  p.Ap = vv.bits.Ap;
//  p.An = vv.bits.An;
//  p.Bp = vv.bits.Bp;
////  #ifdef PortA
////  p.Bp = ((state_t*)&v)->Bp;
////  #endif
//  
//  return *((uint8_t*)&p);
//}
//
//#define _Cp(z) (z ? PORTD : PD5)

/*
src
dst
eor dst, dst
bst src, 0
bld dst, 4
bst src, 1
bld dst, 2
...
#if Cp_port = PORTD
bst src, Cp_s
bld dst, Cp
#endif 
*/

// note that these can't just be changed willy-nilly, due to 
#define Sense_Common PD6
#define Cp PD5
#define Bn PD3
#define Bp PD2
#define An PD1
#define Ap PD0
#define Cn PB2

#define Select_PortD_Pins(u8) ((u8 & 0x4F) + ((u8 & 0x10) ? (1<<Cp) : 0))
#define Select_PortB_Pins(u8) ((u8 & 0x20) ? (1<<Cn) : 0)
#define Select_DDRD_Pins(u8) (u8 & 0x40)

const uint8_t sense_common_pin = 1<<Sense_Common;
const uint8_t PortD_all = (1<<Cp) | (1<<Bn) | (1<<Bp) | (1<<An) | (1<<Ap) | sense_common_pin;
const uint8_t PortB_all = (1<<Cn);

#define Max_Sample_Count 0x100
#define Max_State_Count 0x10

uint8_t states[Max_State_Count];
uint16_t state_waits[Max_State_Count];
uint8_t waveform[Max_Sample_Count];// __attribute__ ((section (".i2c_memory")));
uint8_t prescalar = 7;
volatile uint8_t which_adc;
bool high_res;
uint8_t sample_count;
uint16_t twi_delay;

uint8_t * volatile _w;
volatile uint8_t _samples_left;
uint8_t _states_PortD[Max_State_Count];
uint8_t _states_PortB[Max_State_Count];
uint8_t _states_DDRD[Max_State_Count];

const uint8_t t_signal     = (1<<PB4);
//const uint8_t t_ADC_sample = (1<<PB4);
//const uint8_t t_capture    = (1<<PB5);

ISR(ADC_vect) {
  //PORTB |= t_ADC_sample;
  
  // if high_res, we are actually dropping ADCH (_w is uint8_t*), but we must always
  // read ADCH, in order to let the ADC save more samples
  if (high_res) {
    *_w++ = ADCL;
    //volatile uint8_t dummy = ADCH;
    uint8_t dummy;
    asm volatile ("in %0, %1" : "=r" (dummy) : "I" (_SFR_IO_ADDR(ADCH)) );
  } else {
    *_w++ = ADCH;
  }
  
  if (--_samples_left == 0) {
    ADCSRA = 0;
    //PORTB &= ~t_capture;
  }

  //PORTB &= ~t_ADC_sample;
} 

volatile bool _timer1 = false;

void stop_timer1() {
  TCCR1B = 0;
}

ISR(TIMER1_COMPA_vect) {
  _timer1 = true;
  stop_timer1();
}

void initialize_timer1() {
  TCCR1A = 0;
  TCCR1B = 0;
  
  TIMSK = (1<<OCIE1A);
}

void start_timer1(uint16_t ocr1a) {
  TCNT1 = 0;
  OCR1A = ocr1a;
  TIFR = (1<<OCF1A);
  _timer1 = false;
  TCCR1B = (1<<CS11) | (1<<WGM12);
}

void initialize_ADC() {
  //PORTB |= t_capture;

  _w = waveform;
  _samples_left = sample_count;
  
  if (high_res) {
    ADMUX = (1<<REFS1) | (1<<REFS0) | // 2.56V ref
            (0<<ADLAR) |              // LSB of value is LSB of ADCL
            (which_adc<<MUX0);
  } else {
    ADMUX = (1<<REFS0) |              // AVcc voltage reference
            (1<<ADLAR)|              // MSB of value is MSB of ADCH
            (which_adc<<MUX0);
  }
  // 10-bit resolution "requires an input clock frequency between 50 kHz and 200 kHz";
  // 16 MHz / 128 == 125 kHz
  ADCSRA = (1<<ADEN) |
           (1<<ADFR) |
           (1<<ADIE) |
           (prescalar<<ADPS0);

  ADCSRA |= (1<<ADSC);
}

// returns a count of states, ignoring the last N if they all have zero state & zero wait
uint8_t init_states() {
  uint8_t nonzero_idx = -1;
  
  for (int i = 0; i < Max_State_Count; i++) {
    if (states[i] != 0 || state_waits[i] != 0)
      nonzero_idx = i;
    
    _states_PortD[i] = Select_PortD_Pins(states[i]);
    _states_PortB[i] = Select_PortB_Pins(states[i]);
    _states_DDRD[i] = Select_DDRD_Pins(states[i]);
  }
  
  return nonzero_idx + 1;
}

extern "C" void TWIUserError(uint8_t n) {
}

volatile bool _do_capture;

extern "C" void TWIUserSignal(uint8_t n) {
  //PORTB |= t_signal;
  
  switch (n) {
    case 1:
      _do_capture = true;
      break;
    case 6:
      for (uint16_t i = twi_delay; i != 0; i--)
        asm volatile ("nop");
      break;
    case 7:
      // we do not want to enable interrupts here
      start_timer1(twi_delay);
      while ((TIFR & (1<<OCF1A)) == 0)
        asm volatile ("nop");
      stop_timer1();
      break;
  }

  //PORTB &= ~t_signal;
}

void capture() {
  
  for (uint16_t i = 0; i < Max_Sample_Count; i++)
    waveform[i] = i % 2 ? 0 : 0xFF;

  uint8_t mask_PortD = ~PortD_all;
  uint8_t mask_PortB = ~PortB_all;
  uint8_t state_cnt = init_states(); // also writes to _states_PortD and _states_PortB

  initialize_ADC();
  wdt_enable(WDTO_15MS);  

  // capture a few samples before we do anything
  if (sample_count >= 5)
    while (_samples_left != sample_count - 4)
    {}

  for (int i = 0; i < state_cnt; i++) {
    PORTD = (PORTD & mask_PortD) | _states_PortD[i];
    PORTB = (PORTB & mask_PortB) | _states_PortB[i];
    if (_states_DDRD[i])
      DDRD |= sense_common_pin;
    else
      DDRD &= ~sense_common_pin;
    
    wdt_reset();
    
    if (state_waits[i] > 0) {
      start_timer1(state_waits[i]);
    
      while (!_timer1)
      {}
    }
  }
  
  PORTD &= mask_PortD;
  PORTB &= mask_PortB;
  DDRD &= ~sense_common_pin;
  
  wdt_disable();
  
  for (uint8_t i = 0; i < Max_State_Count; i++)
    states[i] = state_waits[i] = 0;
    
}  

ISR(TIMER2_COMP_vect) {
  SPDR = TWSR;
  while ((SPSR & (1<<SPIF)) == 0)
  {}
}
//ISR(TIMER4_COMPB_vect) {
ISR(TIMER2_OVF_vect) {
  SPDR = TWCR;
  while ((SPSR & (1<<SPIF)) == 0)
  {}
}

int main() {
  // initialize FET pins
  PORTD &= ~PortD_all;
  DDRD |= PortD_all;
  PORTB &= ~PortB_all;
  DDRB |= PortB_all;
  
  // SPI port
  DDRB |= (1<<PB3) | (1<<PB4) | (1<<PB5);
  
  // TWI slave
  setup(0x50, 0, RAMEND, RAMEND);
  
  ACSR |= (1<<ACD);
  
  // ground the pFET drivers
  const uint8_t pFET_ground_pin = 1<<PD4;
  DDRD |= pFET_ground_pin;// | sense_common_pin;
  PORTD &= ~pFET_ground_pin;
  
  initialize_timer1();
  
  DDRB |= (1<<PB3) | (1<<PB5); // PB3 is MOSI, PB5 is SCK
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
	SPSR = (1<<SPI2X);

  TCCR2 = (1<<CS21) | (0<<CS20);
  OCR2 = 240;
  TIMSK |= (1<<OCIE2) | (1<<TOIE2);
  
    
  sei();
  
  states[0] = 0x02;
  state_waits[0] = 10;
  
  while(true) {
    if (_do_capture) {
      capture();
      _do_capture = false;
    }
    
    // delay SPI debugging if I2C interface activated
    if ((PINC & (1<<PC5)) == 0)
      TCNT2 = 0;
  }

  return 0;
}