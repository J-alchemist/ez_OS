#include "led.h"

void LED_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
 	
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOE, ENABLE);	 
	
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;				 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
    GPIO_Init(GPIOB, &GPIO_InitStructure);					 
    GPIO_SetBits(GPIOB,GPIO_Pin_5);						

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;	    		 
    GPIO_Init(GPIOE, &GPIO_InitStructure);	  				 
    GPIO_SetBits(GPIOE,GPIO_Pin_5); 						 
}


void led_ctrl(int led_num, int on_off)
{
    switch(led_num){
    case 0:
        if(on_off){
            GPIO_ResetBits(GPIOB, GPIO_Pin_5);
        }else{
            GPIO_SetBits(GPIOB, GPIO_Pin_5);
        }
        break;
    case 1:
        if(on_off){
            GPIO_ResetBits(GPIOE, GPIO_Pin_5);
        }else{
            GPIO_SetBits(GPIOE, GPIO_Pin_5);
        }
        break;
    default:
        break;
    }
}
 
