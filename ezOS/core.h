#ifndef __EZOS_CORE_H
#define __EZOS_CORE_H

#include "ezos_struct.h"
#include "port.h"

// 给port、app层使用
// errorCode OS内核运行错误码
#define OS_ERR_NONE                                     (0)
#define OS_ERR_PRIORITY_EXIST                           (-1)
#define OS_ERR_PRIORITY_INVALID                         (-2)
#define OS_ERR_TASK_CREATE_WITHIN_ISR                   (-3)
#define OS_ERR_TASK_CREATE_MALLOC_TCB                   (-4)
#define OS_ERR_TCB_INIT_PARAM_ILLEGAL                   (-5)
#define OS_ERR_TASK_DLY_ISR                             (-6)

// OS创建、启动
void OSInit(void);
void OSStart(void);

// PC的异常返回函数（LR的值）
void os_task_return(void);
// OS是否在跑
u8_t os_get_running_status(void);
// 任务创建
s16_t os_task_create(void (*task)(void *arg), void *arg, os_stk_t *stack, u8_t priority);
// 任务延时
s16_t os_task_dlyms(u32_t ms);



#endif