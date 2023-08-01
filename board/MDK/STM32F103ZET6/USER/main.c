#include "delay.h"
#include "sys.h"
#include "core.h"
#include "led.h"

// 任务栈大小
#define TEST_TASK1_STK_SIZE				(64)
#define TEST_TASK2_STK_SIZE				(64)
// 优先级
#define TEST_TASK1_PRIORITY				(2)
#define TEST_TASK2_PRIORITY				(3)

// 栈,默认高向低地址增长 
static os_stk_t task1_stk[TEST_TASK1_STK_SIZE];
static os_stk_t task2_stk[TEST_TASK2_STK_SIZE];

// task1
static void test_task1(void *arg)
{
	while(1) {
		led_ctrl(0, 1);
		os_task_dlyms(200);
		led_ctrl(0, 0);
		os_task_dlyms(200);
	}
}
// tsak2
static void test_task2(void *arg)
{
	while(1){
		led_ctrl(1, 0);
		os_task_dlyms(200);
		led_ctrl(1, 1);
		os_task_dlyms(200);
	}
}

// main
int main(void)
{	
	OSInit();	// init OS
	LED_Init();	// init stm32 led hardware

	os_task_create(test_task1, 
					NULL, 
					&task1_stk[TEST_TASK1_STK_SIZE-1], 
					TEST_TASK1_PRIORITY);

	os_task_create(test_task2, 
					NULL, 
					&task2_stk[TEST_TASK2_STK_SIZE-1], 
					TEST_TASK2_PRIORITY);

	OSStart();	// start OS

	return 0;
}

