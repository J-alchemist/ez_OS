#include "port.h"
#include "core.h"
#include "core_cm3.h"
// 临界区关中断
__asm u16_t port_cpu_xpsr_save(void)
{
    MRS     R0, PRIMASK         // 保存寄存器PRIMASK到R0
    CPSID   I                   // 关中断(PRIMASK=1)，除了NMI和硬fault
    BX      LR                  // 跳转到LR寄存器，等同于mov  pc,lr
}
// 开中断
__asm void port_cpu_xpsr_restore(u16_t new_value)
{
    MSR     PRIMASK, R0         // 从R0恢复PRIMASK寄存器,PRIMASK=0
    BX      LR
}

// 设置Pensv最低优先级
void port_set_PendSV_Handler_lowest_priority(void)
{
    NVIC_SetPriority(PendSV_IRQn, 0xFF);        // core_cm3.h
}

// 初始化[栈帧]
// 传入的栈底地址，高地址向低地址增长（地址递减）
os_stk_t *port_task_stack_init(void (*task)(void *arg), 
                            void *arg, 
                            os_stk_t *stack)
{           
    // os进入异常[前]，cpu自动入栈
    // 自动入栈寄存器
    // 自动入栈的为栈帧
    *stack = 0x01000000u;                       /* xPSR */
    stack--;                        
    *stack = (u32_t)task;                       /* Entry Point */
    stack--;
    *stack = (u32_t)os_task_return;             /* R14 (LR) */
    stack -= 5;                                 /* R12, R3, R2, R1, R0 */
    *stack = (u32_t)arg;

    // 需要手动入栈的寄存器
    // os进入异常[后]，程序员在执行函数内部入栈需要的寄存器
    stack -= 8;                                 /* R11, R10, R9, R8, R7, R6, R5, R4 */

    return stack;    
}

__asm void port_PendSV_Handler(void)
{
    extern ezos_curr_running_task_ptcb;
    extern ezos_curr_running_priority;
    extern ezos_next_run_priority;

    PRESERVE8 

    // 关闭中段(PRIMASK = 1)
    cpsid   i                                   
    // save current task context
    // 保存当前堆栈，psp到r0
    mrs     r0, psp
    isb
    // 入栈: 将r4～r11从r0指向的地址存放数据，r0自动递减
    stmdb   r0!, {r4-r11}

    // 入栈完毕，更新cur_task的栈顶指针
    ldr     r1, =ezos_curr_running_task_ptcb        
    ldr     r1, [r1]                            
    str     r0, [r1]                            

    // 更新priority
    ldr     r0, =ezos_curr_running_priority
    ldr     r1, =ezos_next_run_priority 
    ldrb    r2, [r1] 
    strb    r2, [r0]

    // 更新tcb
    ldr     r0, =ezos_curr_running_task_ptcb    
    ldr     r1, =ezos_next_run_task_ptcb
    ldr     r2, [r1]
    str     r2, [r0]      

    // 获取下一个任务的栈顶指针 
    ldr     r0, [r2]     
    // 从该地址开始出栈下一个任务的   
    ldmia   r0!, {r4-r11}
    // 更新psp，让出中断后cpu好自动出栈其余寄存器
    msr     psp, r0
    isb
    // 开中断，跳转
    cpsie   i  
    bx      r14 
    nop 
}

__asm void port_SVC_Handler(void)
{
    extern ezos_next_run_task_ptcb;
    extern ezos_running;

    PRESERVE8

    // 获取下一个要运行任务的堆栈地址，存在r0
    ldr r1, =ezos_next_run_task_ptcb
    ldr r1, [r1]
    ldr r0, [r1]
    // 下一个任务的堆栈首地址给到r0
    // 从r0出栈一半寄存器到r4～r11，递增r0地址
    ldmia r0!, {r4-r11}
    // 进程堆栈psp指向r0，也就是指向下一个任务的堆栈地址，用于cpu自动出栈一半的寄存器
    msr psp, r0 
    isb 

    // 设置ezos_running=1，OS运行起来了
    ldr r0, =ezos_running
    movs r1, #1
    strb r1, [r0]
	
    // 更新priority
    // set: ezos_curr_running_priority = ezos_next_run_priority
	ldr     r0, =ezos_curr_running_priority
    ldr     r1, =ezos_next_run_priority 
    ldrb    r2, [r1] 
    strb    r2, [r0]  

    // 更新tcb
    // set: ezos_curr_running_task_ptcb = ezos_next_run_task_ptcb
    ldr     r0, =ezos_curr_running_task_ptcb    
    ldr     r1, =ezos_next_run_task_ptcb
    ldr     r2, [r1]
    str     r2, [r0]                            

    // 开所有中断(BASEPRI)
    mov r0, #0
    msr basepri, r0 

    // 设置exc_return
    // LR = LR | 0xd，改变exc_return: 0xfffffff9 -> 0xfffffffd
    // 退出isr，跳转到r14，因为exc_return = 0xfffffffd，cpu会运行在线程模式，使用任务堆栈指针psp
    orr r14, #0xd
    bx r14
}

__asm void port_start_first_task(void)      
{
    PRESERVE8

    // 利用scb->vtor重置msp
    // 0xE000ED08地址是VTOR寄存器，设置向量表地址
    // 从0xE000ED08取一个数出来作为地址，再从这个地址取值给到r0
    ldr r0, =0xE000ED08
    ldr r0, [r0]
    ldr r0, [r0]

    // 读取r0值到msp，重置msp
    msr msp, r0

    // 使能中断(PRIMASK\FAULTMASK) 
    cpsie i 
    cpsie f 
    // 内存屏蔽: 等待所有前面的指令完成后再执行后面的指令
    dsb 
    isb 

    // call svc
    svc 0
    nop 
    nop 
}

void port_Systick_init(void)            // 内核定时器
{
    u32_t reload;

    // systick为最低优先级中断
    NVIC_SetPriority(SysTick_IRQn, 0xFF);
    /* 2. set SysTick clock source */
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    reload = SystemCoreClock / 8000000;
    /* 3. set SysTick reload cnt */
    reload *= (1000000 / OS_CFG_TICKS_WITHIN_ONE_SECOND);
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    SysTick->LOAD = reload;
    /* 4. start SysTick  */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; 
}

// 向SCB寄存器第28位写1
void port_trigger_PendSV_Handler(void)      // 触发PendSV
{
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}


// 只负责判断任务是否就绪，触发Pendsv
// 实际是在Pendsv函数进行任务切换
void port_SysTick_Handler(void)         // 滴答定时器中断
{
    os_cpu_psr_t cpu_psr = 0;

    if(0 == os_get_running_status()) {
        return;
    }

    OSEnterCritical();
    os_isr_nesting_inc();       // os ticks num +1
    OSExitCritical();

    os_ticks_increment();       // 遍历任务表
    os_sched_isr_exit();        // 触发PendSV
}