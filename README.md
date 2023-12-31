# ##################################
# ####       ez_OS    ##############
# ##################################
# ----Talk is cheap.Show me your code.
本系统参照ucosII实现，不支持两个及以上相同的任务优先级的任务，不支持时间片轮转  
core.c相关：os内核实现  
port.c相关：Cortex-M3内核相关实现

#  based on Cortex-CM3
进入异常函数前：cpu自动入栈 xPSR、R14 (LR)、R12, R3, R2, R1, R0  
进入异常函数后：手动看需要入栈那些: R11, R10, R9, R8, R7, R6, R5, R4  
exc_return = 0xfffffffd，cpu会运行在线程模式&&使用任务堆栈指针psp


## Cortex-M3的栈示意图，r0～xpsr为Cortex-M3的栈帧结构
```
(sp)--> ----------------(高地址，从xPSR开始入栈)
        ----------------  
        ----------------
        ----------------
        ---------------- 
        ----------------
        ----------------
        ----------------R0（自动入栈结束）
        ----------------R11（手动入栈）
        ----------------
        ----------------
        ----------------
        ----------------
        ----------------
        ----------------
        ----------------（低地址，R4）
```
# 过程分析
CPU上电进入：特权级（内核态）-线程 模式，堆栈指针使用主堆栈指针msp，进入OS初始化，运行port_start_first_task设置系统状态：  
|<-  ...... cpu上电        ->|<-      cpu状态设置       ->|  任务切换... ...

    |     内核态-线程模式      |       内核态-中段模式                  |    用户态-线程模式  |   内核态-中段模式                                   |               |
    |      sp=msp            |          sp=msp                     |       sp=psp      |     sp=msp                                        |    as task1   |
    |    重置msp,触发svc      |   出栈首个任务，设置psp指向和exc_return  |      执行任务      | 拿到cpu自动入栈之后的psp，手动入栈task1剩余，出栈task2  |               |
      port_start_first_task->           svc hander->                      task1->                        pendsv ->                                task2     


### SCB->VTOR
0xE000ED08地址是 SCB->VTOR 寄存器，用于设置向量表基地址，可以用来重置msp主进程堆栈。
```
    ldr r0, =0xE000ED08
    ldr r0, [r0]
    ldr r0, [r0]

    msr msp, r0
```

# CM3-特殊寄存器：
PRIMASK\FAULTMASK\BASEPRI\xPSR   
为了快速地关中断，Cortex-M内核专门设置了一条CPS指令，有4种用法：
```
    CPSID I;PRIMASK = 1;关中断
    CPSIE I;PRIMASK = 0;开中断
    CPSID F;FAULTMASK = 1;关异常
    CPSIE F;FAULTMASK = 0;开异常
```  
Cortex-M3的中断屏蔽寄存器组(属于特殊功能寄存器)
这三个寄存器用于控制异常的使能和失能：
```  
    1、PRIMASK：这是个只有1个位的寄存器。当它置1时， 就关掉所有可屏蔽的异常，只剩下NMI和硬fault可以响应。它的缺省值是0，表示没有关中断。  
    2、FAULTMASK：这是个只有1个位的寄存器。当它置1时，只有NMI才能响应，所有其它的异常，包括中断和fault，通通闭嘴。它的缺省值也是0，表示没有关异常。  
    3、BASEPRI：这个寄存器最多有9位（由表达优先级的位数决定）。它定义了被屏蔽优先级的阈值。当它被设成某个值后，所有优先级号大于等于此值的中断都被关（优先级号越大，优先级越低）。但若被设成0，则不关闭任何中断，0也是缺省值。
```

特殊功能寄存器只能被专用的MSR和MRS指令访问：  
```
    MRS R0, BASEPRI   ;读取BASEPRI到R0中  
    MRS R0, FAULTMASK ;读取FAULTMASK到R0中  
    MRS R0, PRIMASK   ;读取PRIMASK到R0中  
    MSR BASEPRI, R0   ;写入R0到BASEPRI中  
    MSR FAULTMASK, R0 ;写入R0到FAULTMASK中  
    MSR PRIMASK, R0   ;写入R0到PRIMASK中  
只有在特权级下，才允许访问这3个寄存器。  
```
程序状态寄存器xPSR包括以下三个状态寄存器:  
```
    应用PSR(APSR)：记录算术逻辑单元ALU(arithmetic and logic unit)标志  
    执行PSR(EPSR)：执行状态   
    中断PSR(IPSR)：正服务的中断号    
```
# arm汇编基础
1、Rn和[Rn] 寻址的区别：
[Rn] 表示间接寻址，类似指针, 在汇编指令中， Rn和[Rn]的区别就是，前者Rn操作是直接取寄存器Rn中的值，[Rn]则是将寄存器Rn存放的值认为是一个地址，然后取这个地址中的值。  
2、LDR指令和LDR伪指令：
LDR后面带 ‘=’ 的就是LDR伪指令。LDR伪指令的设计目的是为了把一个32位立即数或内存地址存入到一个寄存器。CPU在遇到LDR伪指令时，会判断这个32位立即数或地址，来决定是使用MOV指令，还是使用LDR 指令。
eg.:
```
typedef struct tcb{
		unsigned int *pstk;
		u16 a;
		u16 b;
} tcb_t;

static unsigned int currStack[32];
static tcb_t currTCB;
volatile tcb_t *pCurrTCB = NULL;

__asm void asm_func(void)
{
	extern pCurrTCB
		
	PRESERVE8 
	
	ldr	r3, =pCurrTCB  				/* [1] */
	ldr r1, [r3]					/* [2] */
	ldr r0, [r1]					/* [3] */
	
	ldmia r0, {r4-r11}				/* */
	nop 
}

int main(void)
{	 
	currTCB.pstk = &currStack[31];
	currTCB.a = 0x0102;
	currTCB.b = 0x0304;
	
	currStack[31] = 11;
	
	pCurrTCB = &currTCB;
	
	printf("currStack[31] addr:0x%X\n", &currStack[31]);
	printf("currTCB's addr:0x%X\n", &currTCB);
	printf("pCurrTCB's addr:0x%X\n", &pCurrTCB);
	printf("pCurrTCB's addr:0x%X\n", pCurrTCB);
	
	asm_func();
		 
	return 0;
}	 
```
打印结果：
```
&currStack[31]:0x200000AC
&currTCB:0x20000000
&pCurrTCB:0x20000008
pCurrTCB:0x20000000
```
关键指令分析：
```
ldr	r3, =pCurrTCB  				/* [1] */
ldr r1, [r3]					/* [2] */
ldr r0, [r1]					/* [3] */
```
1.将pCurrTCB的地址作为立即数赋值给r3，执行该命令后，r3 = 0x20000008， 而0x20000008地址中的内容是0x20000000  
2.[r3] 的含义是，间接引用，将r3中的值转换成一个内存地址，然后再将这个内存地址中的值赋值给r1，所以我们可以简单推导：  
  a. 寄存器r3中的值为0x20000008  
  b. 将0x20000008当成内存地址。  
  c. 将内存地址0x20000008中的内容0x20000000 赋值给r1.  
3.将r1中的值作为地址，取出该地址指向的值给r0  
  a. 寄存器r1中的值为0x20000000  
  b. 将0x20000000当成内存地址。  
  c. 将内存地址0x20000000中的内容0x200000AC 赋值给r0  


# ARM平台的内存屏障指令
```
dsb：数据同步屏障指令。它的作用是等待所有前面的指令完成后再执行后面的指令。  
dmb：数据内存屏障指令。它的作用是等待前面访问内存的指令完成后再执行后面访问内存的指令。  
isb：指令同步屏障。它的作用是等待流水线中所有指令执行完成后再执行后面的指令。  
strb r1, [r0]：将寄存器r1中的一个8位字节数据写入以寄存器r0为地址的存储器中  
```
# 批量 出栈、入栈 
```
ldmia r0!, {r4-r11}: 用于将任务栈r0中的存储地址由低到高的内容加载到CPU寄存器r4-r11中, r自动递增。（出栈）  
stmdb r0!, {r4-r11} 是 ARM 汇编语言中的一条指令，用于将寄存器 r4 到 r11 的值存储到内存中，并递减寄存器 r0 的值。（入栈）  
```

Lastly, thank you pig brother [a csdn author]!