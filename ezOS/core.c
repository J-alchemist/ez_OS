#include "core.h"

/*****************  core  global variables   ********/
u8_t                        ezos_isr_nesting;      // 嵌套      
u8_t                        ezos_running;          // os是否运行   
volatile u32_t              ezos_ticks;            // os运行的时间片

volatile u16_t              ezos_curr_running_priority;     // 当前运行任务的优先级
volatile u16_t              ezos_next_run_priority;         // 下一个任务的优先级  

volatile os_tcb_t           *ezos_curr_running_task_ptcb;   // 当前任务的tcb指针                     
volatile os_tcb_t           *ezos_next_run_task_ptcb;       // 下一个任务的tcb指针

u8_t                        ezos_task_ready_group;          // 任务就绪组
u8_t                        ezos_task_ready_tbl[OS_CFG_TASK_READY_TBL_SIZE];        // 任务就绪表


/********************** tcb链表的增、删、改、查 ***********************/
static os_tcb_t *ezos_list_head_tcbs = NULL;
static os_tcb_t *ezos_list_tcbs_tail = NULL;        // 增加尾指针：快速添加任务

static void tcb_slist_add_tail(os_tcb_t *node)
{
    os_tcb_t *ptcb = NULL;

    if(ezos_list_head_tcbs == NULL){        // 更新头指针
        ezos_list_head_tcbs = node;     
        ezos_list_tcbs_tail = node;
        return;
    }

    ptcb = ezos_list_tcbs_tail;
    ptcb->list_next_tcb = node;
    ezos_list_tcbs_tail = node;     // 更新尾指针
}

// 遍历整个表，找到尾指针
#define tcb_slist_for_each_entry(node)              \
    for(node = ezos_list_head_tcbs; node; node = node->list_next_tcb)

// 清理内存块
static void os_mem_clear(u8_t *src, u16_t size)
{
    while (size > 0){
        *src++ = (u8_t)0;
        size--;
    }
}
/**********************task tcb heap tcb内存池操作***********************/
static os_tcb_heap_t    ezos_tcb_heaps[OS_CFG_TASKS_MAX + OS_SYSTEM_TASKS_NUM];

void os_tcb_heaps_init(void)            // tcb内存池初始化
{
    os_mem_clear((u8_t *)&ezos_tcb_heaps[0], 
                 (OS_CFG_TASKS_MAX + OS_SYSTEM_TASKS_NUM) * sizeof(os_tcb_heap_t));

    ezos_list_head_tcbs = NULL;
    ezos_list_tcbs_tail = NULL;
}

os_tcb_t *os_tcb_malloc(void)       // 为某个任务申请一个未使用的tcb块
{
    u16_t i;
    os_tcb_t *ptcb = NULL;
    for(i = 0; i < (OS_CFG_TASKS_MAX + OS_SYSTEM_TASKS_NUM); i++){
        if(ezos_tcb_heaps[i].flag_used == OS_FLAG_UNUSED){

            ezos_tcb_heaps[i].flag_used = OS_FLAG_USED; // 标记使用
            ptcb = &ezos_tcb_heaps[i].tcb;              // 返回该tcb地址（同时也是栈顶指针）
            
            ptcb->list_next_tcb = NULL;
            
            return ptcb;
        }
    }

    return (os_tcb_t *)0;
}

void os_tcb_free(os_tcb_t *tcb)
{
    u16_t i;
    for(i = 0; i < (OS_CFG_TASKS_MAX + OS_SYSTEM_TASKS_NUM); i++){
        if(tcb == &ezos_tcb_heaps[i].tcb){
            ezos_tcb_heaps[i].flag_used = OS_FLAG_UNUSED;
            os_mem_clear((u8_t *)tcb, sizeof(os_tcb_t));
        }
    }
}

/*********************** task priority table tcb优先级信息表 *****************************/

// 任务优先级表, 数组下标表示对应优先级，取出的指针是该优先级任务的tcb指针，首地址也是栈指针
static os_task_priority_info_t  ezos_task_priority_info_tbl[OS_CFG_LOWEST_PRIORITY + 1];

void os_task_priority_info_tbl_init(void)       // 任务优先级表初始化
{
    u16_t i;
    for(i = 0; i < (OS_CFG_LOWEST_PRIORITY + 1); i++) {
        os_mem_clear((u8_t *)&ezos_task_priority_info_tbl[0], 
                     (OS_CFG_LOWEST_PRIORITY + 1) * sizeof(os_task_priority_info_t));
    } 
}

s16_t os_task_priority_info_tbl_insert(u8_t priority, os_tcb_t *ptcb)
{ 
    if((priority > OS_CFG_LOWEST_PRIORITY) || 
       (priority == 0)){
        return OS_ERR_PRIORITY_INVALID;
    }

    if(ezos_task_priority_info_tbl[priority].ptcb != NULL){
        return OS_ERR_PRIORITY_EXIST;
    }

    ezos_task_priority_info_tbl[priority].ptcb = ptcb;

    return OS_ERR_NONE;
}

// 删除某个优先级下的任务控制块
s16_t os_task_priority_info_tbl_delete(u8_t priority)
{
    if((priority > OS_CFG_LOWEST_PRIORITY) || 
       (priority == 0)){
        return OS_ERR_PRIORITY_INVALID;
    }

    ezos_task_priority_info_tbl[priority].ptcb = (os_tcb_t *)0;

    return OS_ERR_NONE;
}

/*********************** 查找最高优先级的就绪任务（空间换时间） ***********************/
// 查找一个8bit的字节，从最低位开始为1的bit位置
const u8_t ezos_lowest_bit_of_byte_map[256] = {
    0u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x00 to 0x0F */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x10 to 0x1F */
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x20 to 0x2F */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x30 to 0x3F */
    6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x40 to 0x4F */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x50 to 0x5F */
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x60 to 0x6F */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x70 to 0x7F */
    7u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x80 to 0x8F */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0x90 to 0x9F */
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xA0 to 0xAF */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xB0 to 0xBF */
    6u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xC0 to 0xCF */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xD0 to 0xDF */
    5u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, /* 0xE0 to 0xEF */
    4u, 0u, 1u, 0u, 2u, 0u, 1u, 0u, 3u, 0u, 1u, 0u, 2u, 0u, 1u, 0u  /* 0xF0 to 0xFF */
};

/**
 * 空间换时间的快速查找
 * 详细解释: 从就绪表中获取最高优先级就绪任务
 * ezos_task_ready_tbl[]
 * 下标：任务组，哪一组有任务就绪，该bit为1
 * 数值：组内编号
*/
static u8_t os_get_highest_ready_task_priority(void)   
{
    u8_t x, y;

    y = ezos_lowest_bit_of_byte_map[ezos_task_ready_group];     // 得到最高优先级组
    x = ezos_lowest_bit_of_byte_map[ezos_task_ready_tbl[y]];    // 得到某组下的最高优先级任务

    // y<<3，相当于第几组乘以8，再加上组内编号即可
    ezos_next_run_priority = (u8_t)((y << 3) + x);      // 下一个就绪任务的优先级

    return ezos_next_run_priority;
}

static void os_update_next_run_task_ptcb(void)          // 更新下一个任务tcb
{
    // 根据优先级找到对应任务的tcb地址，tcb首地址也是其 任务栈的地址
    ezos_next_run_task_ptcb = ezos_task_priority_info_tbl[ezos_next_run_priority].ptcb;
}

// 任务tcb初始化
static s16_t os_task_tcb_init(os_tcb_t *ptcb, u8_t priority, os_stk_t *pstack)
{
    u16_t rc;
    if((ptcb == NULL) || (pstack == NULL)) {
        return OS_ERR_TCB_INIT_PARAM_ILLEGAL;
    }

    if((priority == 0) || (priority > OS_CFG_LOWEST_PRIORITY)) {
        return OS_ERR_TCB_INIT_PARAM_ILLEGAL;
    }

    ptcb->pTopOfStack = pstack;
    ptcb->taskPriority = priority;
    ptcb->taskDlyTicks = 0;                       // Task is not delayed 

    //  priority/8
    ptcb->taskReadyGroup_bit = (priority >> 3);   // priority / 8   任务组
    // 保留低3位
    ptcb->taskReadyTbl_bit = (priority & 0x07);   // 0000 0111      组内编号

    // 掩码
    ptcb->taskReadyGroup_mask = (1 << ptcb->taskReadyGroup_bit);
    ptcb->taskReadyTbl_mask = (1 << ptcb->taskReadyTbl_bit);

    // 标记
    ptcb->taskStatus = OS_TASK_STATUS_READY;      // 就绪态任务

    // 更新就绪组和就绪表
    ezos_task_ready_group |= ptcb->taskReadyGroup_mask;  // 记录有就绪任务的组别
    ezos_task_ready_tbl[ptcb->taskReadyGroup_bit] |= ptcb->taskReadyTbl_mask;   // 第xx组的xx任务就绪，ready_tbl[0]数据的某位代表第0组的某个任务就绪

    tcb_slist_add_tail(ptcb);       // 添加该任务块tcb到尾指针

    rc = os_task_priority_info_tbl_insert(priority, ptcb);         // 注册该优先级下的任务信息表tcb

    return rc;
}


/*********************** **************** ***********************/
// os是否在运行 
u8_t os_get_running_status(void)
{
    return ezos_running;
}

void os_isr_nesting_inc(void)
{
    ezos_isr_nesting++;
}

/*********************** **************** ***********************/
static void os_gd_init(void)
{
    ezos_ticks          = 0;        // 时间片数量
    ezos_isr_nesting    = 0;        // 中断嵌套
    ezos_running        = 0;        // OS是否在运行
}


static void os_task_ready_table_init(void)          // 任务就绪表初始化
{
    u8_t i;
    ezos_task_ready_group = 0;
    for(i = 0; i < OS_CFG_TASK_READY_TBL_SIZE; i++){
        ezos_task_ready_tbl[i] = 0;
    }

    ezos_curr_running_priority = 0;
    ezos_next_run_priority = 0;
}

s16_t os_task_create(void (*task)(void *arg),   
                    void *arg,
                    os_stk_t *stack,
                    u8_t priority)                  // 任务创建函数
{
    os_stk_t *psp;
    os_tcb_t *ptcb;
    u16_t rc;

    os_cpu_psr_t cpu_psr = 0;

    if((priority > OS_CFG_LOWEST_PRIORITY) ||
       (priority == 0)){
        return OS_ERR_PRIORITY_INVALID;
    }
    // 该优先级下存在任务，tcb已经被占用
    if(ezos_task_priority_info_tbl[priority].ptcb != NULL){
        return OS_ERR_PRIORITY_EXIST;
    } 

    OSEnterCritical();
    if(ezos_isr_nesting > 0) {   // 确保中断关闭之后，没有中断发生
        OSExitCritical();
        return OS_ERR_TASK_CREATE_WITHIN_ISR;
    } 
    OSExitCritical();

    ptcb = os_tcb_malloc();     // 为某个任务申请一个未使用的tcb块
    if(ptcb == NULL){
        return OS_ERR_TASK_CREATE_MALLOC_TCB;
    }
    // [core]
    // 初始化任务栈
    psp = port_task_stack_init(task, arg, stack);
    // 初始化任务控制块
    rc = os_task_tcb_init(ptcb, priority, psp);

    if(rc == OS_ERR_NONE){
        if(ezos_running){
            // os_sched();
        }
    }else{
        OSEnterCritical();
        os_task_priority_info_tbl_delete(priority);     // 创建失败，删除tcb
        OSExitCritical();     
    }

    return rc;
}


/*************************** 时间片、调度等处理 ********************************/
// 每次进入一次sysTick中断就调用一次
void os_ticks_increment(void)
{
    os_cpu_psr_t cpu_psr = 0;
    os_tcb_t *ptcb = NULL;

    OSEnterCritical();
    ezos_ticks++;               // 系统时间片
    OSExitCritical();

    tcb_slist_for_each_entry(ptcb) {        // 遍历所有任务的tcb

        OSEnterCritical();
        // 每个任务的延时
        if(ptcb->taskDlyTicks > 0) {
            ptcb->taskDlyTicks--;           // 每个任务要执行需要等待的时间片

            if(ptcb->taskDlyTicks == 0) {   // 等待的时间片到达，切换为就绪态
                if(ptcb->taskStatus == OS_TASK_STATUS_SLEEPING){
                    ptcb->taskStatus = OS_TASK_STATUS_READY;

                    // 任务加入就绪表(对应位置一)
                    ezos_task_ready_group                         |= ptcb->taskReadyGroup_mask;
                    ezos_task_ready_tbl[ptcb->taskReadyGroup_bit] |= ptcb->taskReadyTbl_mask;
                }
            }

        }
        OSExitCritical();
    }
}

static void os_sched_next(void)
{   
    // get下一个任务的优先级
    ezos_next_run_priority = os_get_highest_ready_task_priority();
    // 确认是否需要任务切换
    if(ezos_next_run_priority != ezos_curr_running_priority) {
        // 更新tcb of next task
        os_update_next_run_task_ptcb();
        // trigger Pensv
        port_trigger_PendSV_Handler();
    }
}

void os_sched(void)
{
    os_cpu_psr_t cpu_psr = 0;

    OSEnterCritical();
    if(ezos_isr_nesting == 0) {     // 没有isr在执行，执行任务调度 
        os_sched_next();
    }
    OSExitCritical();
}

// isr退出时，需要进行一次任务调度
// 在isr活跃期间禁止切入线程模式
void os_sched_isr_exit(void)
{
    os_cpu_psr_t cpu_psr = 0;

    if(ezos_running) {
        OSEnterCritical();
        if(ezos_isr_nesting > 0) {
            if(--ezos_isr_nesting == 0)
                os_sched_next();
        }
        OSExitCritical();
    } 
}

/*************************** 延时函数 ********************************/
void os_time_dly(u32_t ticks)      
{
    u8_t y;
    os_cpu_psr_t cpu_psr = 0;

    if(ezos_isr_nesting > 0) {  // 禁止中断调用延时函数
        return;
    }

    if(ticks > 0) {
        OSEnterCritical();
        
        // 更改任务就绪表
        y = ezos_curr_running_task_ptcb->taskReadyGroup_bit;   
        ezos_task_ready_tbl[y] &= ~ezos_curr_running_task_ptcb->taskReadyTbl_mask;

        if(ezos_task_ready_tbl[y] == 0) {       // y组无就绪任务
            ezos_task_ready_group &= ~ezos_curr_running_task_ptcb->taskReadyGroup_mask;
        }
        // 任务的tcb信息表
        ezos_curr_running_task_ptcb->taskDlyTicks = ticks;                  // 任务等待时间
        ezos_curr_running_task_ptcb->taskStatus = OS_TASK_STATUS_SLEEPING;  // 更改任务状态

        OSExitCritical();
        os_sched();   // 延时触发一次调度
    }
}

s16_t os_task_dlyms(u32_t ms)
{
    u32_t ticks;

    if(ezos_isr_nesting > 0) {
        return OS_ERR_TASK_DLY_ISR;
    }
    // 计算时间片数（注意搞个0.5转为浮点）
    ticks = OS_CFG_TICKS_WITHIN_ONE_SECOND * (ms + 0.5) / 1000;

    os_time_dly(ticks);

    return OS_ERR_NONE;
}

// 异常函数
// 栈初始化的时候默认的异常返回函数（未正常入栈LR时，PC返回时就会跳转到此处）
void os_task_return(void)      // 作为异常返回PC的链接位置    
{
    while (1) { 
        os_time_dly(OS_CFG_TICKS_WITHIN_ONE_SECOND);
    }
}

/***************************idle task********************************/
static os_stk_t esos_sys_idle_task_stack[OS_CFG_IDLE_TASK_STACK_SIZE];
volatile u32_t  esos_sys_idle_task_cnt;

void os_sys_idle_task(void *arg)
{
    os_cpu_psr_t cpu_psr = 0;
    esos_sys_idle_task_cnt = 0;

    while(1) {
        OSEnterCritical();
        esos_sys_idle_task_cnt++;
        OSExitCritical();
    }
}

static void os_sys_idle_task_init(void)
{
    os_task_create(os_sys_idle_task, 
                   (void *)0,
                   &esos_sys_idle_task_stack[OS_CFG_IDLE_TASK_STACK_SIZE - 1],
                   OS_CFG_LOWEST_PRIORITY);
}

/****************************** app调用函数 *****************************/
// OS初始化
void OSInit(void)
{
    os_gd_init();                           // OS核心全局变量
  
    os_tcb_heaps_init();                    // tcb堆空间申请及初始化
    
    os_task_priority_info_tbl_init();       // 优先级表
   
    os_task_ready_table_init();             // 任务就绪表

    os_sys_idle_task_init();                // 创建空闲任务

    port_set_PendSV_Handler_lowest_priority();  // pendsv设置为最低优先级
}

// OS启动
void OSStart(void)
{
    os_get_highest_ready_task_priority();   // 获取最高优先级就绪任务
    os_update_next_run_task_ptcb();         // 从优先级信息表，更新下一个任务tcb
    port_Systick_init();                    // 初始化内核定时器
    port_start_first_task();                // 重置msp,触发svc 
}