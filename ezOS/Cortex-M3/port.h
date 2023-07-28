#ifndef __EZOS_PORT_H
#define __EZOS_PORT_H

/**
 * @file port.h
 * @author GuoJian (1210267211@qq.com)
 * @brief 基于Cortex-M3
 * @version 0.1
 * @date 2023-07-21
 * @copyright Copyright (c) 2023
 * 
 */

#include "core.h"               // os实现
#include "misc.h"               // 工作频率
#include "stm32f10x.h"          // 中断向量
#include "core_cm3.h"           // asm

#ifndef NULL
#define NULL    0
#endif

/***********************************************/
// tasks max num
#define OS_CFG_TASKS_MAX                            (32)

// the lowest priority，idle任务的优先级
#define OS_CFG_LOWEST_PRIORITY                      (63)

// ticks within one seconds, hz
#define OS_CFG_TICKS_WITHIN_ONE_SECOND              (1000)

/***********************************************/

typedef unsigned char           u8_t;
typedef signed char             s8_t;
typedef unsigned short          u16_t;
typedef signed short            s16_t;
typedef unsigned int            u32_t;
typedef signed int              s32_t;
typedef unsigned long long      u64_t;
typedef signed long long        s64_t;
typedef unsigned int            os_stk_t;
typedef unsigned int            os_size_t;
typedef unsigned int            os_cpu_psr_t;       // cpu状态

/***********************************************/
#define port_SysTick_Handler    SysTick_Handler
#define port_PendSV_Handler     PendSV_Handler
#define port_SVC_Handler		SVC_Handler

/***********************************************/
// 临界区
u16_t port_cpu_xpsr_save(void);
void port_cpu_xpsr_restore(u16_t new_value);

#define OSEnterCritical()           {cpu_psr = port_cpu_xpsr_save();}
#define OSExitCritical()            {port_cpu_xpsr_restore(cpu_psr);}

// 异常: SysTick\PendSV\SVC
void port_SysTick_Handler(void);
void port_PendSV_Handler(void);
void port_SVC_Handler(void);

// 任务栈初始化
os_stk_t *port_task_stack_init(void (*task)(void *arg), 
                            void *arg, 
                            os_stk_t *stack);
void port_start_first_task(void);

// PendSV，设置优先级最低
void port_trigger_PendSV_Handler(void);
void port_set_PendSV_Handler_lowest_priority(void);

// 内核定时器初始化
void port_Systick_init(void);

/************ 外部函数声明 from core.c  **********/
extern void os_task_return(void);
extern void os_ticks_increment(void);
extern void os_sched_isr_exit(void);
extern void os_isr_nesting_inc(void);
/***********************************************/


#endif