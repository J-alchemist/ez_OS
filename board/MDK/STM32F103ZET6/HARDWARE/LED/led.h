#ifndef __LED_H
#define __LED_H	 
#include "sys.h"

// for test ezOS
#define LED0 PBout(5)// PB5
#define LED1 PEout(5)// PE5	

void LED_Init(void);
void led_ctrl(int led_num, int on_off);

					    
#endif
