#ifndef __EZOS_STRUCT_H
#define __EZOS_STRUCT_H

#include "port.h"           // 基础数据结构

// 内核自带默认任务数量
#define OS_SYSTEM_TASKS_NUM                             (1)    

// 任务就绪表大小
#define OS_CFG_TASK_READY_TBL_SIZE                      (8)     

// 任务最低优先级限制（must not modify!!! otherwise: OS core will be abnormal）
#define OS_CFG_TASK_PRIORITY_LOWEST                     (63)   

// idle任务栈大小
#define OS_CFG_IDLE_TASK_STACK_SIZE                     (64)

// tcb具体结构
typedef struct os_tcb {
    volatile os_stk_t   *pTopOfStack;           // 指向任务栈顶的指针   unsigned int
    u32_t               taskDlyTicks;           // 任务是否需要延时执行
    u8_t                taskStatus;             // 任务状态
    u8_t                taskPriority;           // 任务优先级

    u8_t                taskReadyTbl_bit;       // 任务组内编号
    u8_t                taskReadyGroup_bit;     // 任务就绪组

    u8_t                taskReadyTbl_mask;      // 组内编号掩码（对应的某位为1）
    u8_t                taskReadyGroup_mask;    // 组编号掩码（对应的某位为1）
    
    struct os_tcb       *list_next_tcb;         // 下一个任务tcb地址
} os_tcb_t;


// 任务控制块的堆结构
#define OS_FLAG_UNUSED           (0)
#define OS_FLAG_USED             (1)
typedef struct os_tcb_heap { 
    u8_t        flag_used;              // 是否使用
    os_tcb_t    tcb;                    // 任务控制块
} os_tcb_heap_t;


// 任务优先级信息表，首地址也是 指向任务栈顶的指针
// 实现优先级-任务tcb的一一绑定
typedef struct os_task_priority_info {
    os_tcb_t    *ptcb;                          // 每个优先级下限制一个任务（类似ucosii不支持时间片轮转）
} os_task_priority_info_t;


// 任务状态 
typedef enum{
    OS_TASK_STATUS_READY = 0,       // 就绪
    OS_TASK_STATUS_SLEEPING,        // 挂起
    OS_TASK_STATUS_RUNNING          // run
} os_task_status_t;



#endif