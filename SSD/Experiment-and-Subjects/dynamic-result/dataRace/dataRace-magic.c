#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include "magic-instruction.h"

#define MASK_ALL8F              0xFFFFFFFF
#define MASK_HI16               0xFFFF0000
#define MASK_LO16               0x0000FFFF
#define MASK_LO08               0x000000FF
#define MASK_LO04               0x0000000F

#define TRUE            0xEB90146F
#define FALSE           0x00000000

#define OUTPUT_SIZE            3
#define INPUT_SIZE             8

#define IO3_UART1_CS           ((volatile unsigned int *)0x130000A0)
#define IO3_UART2_CS           ((volatile unsigned int *)0x130000C0)
#define IO3_UART3_CS           ((volatile unsigned int *)0x130000E0)

static volatile unsigned int *tmpAddr = 0;
static struct task_struct *taskThread = 0;
typedef struct TAG_BUFFER_INPUT
{
	unsigned int wrlen;
	unsigned int wrbuf[OUTPUT_SIZE];
	unsigned int rdlen;
	unsigned int rdbuf[INPUT_SIZE];
	unsigned int bflgComm;
	unsigned int Status;
} SInput;

SInput mInput[3];

void Delay(unsigned int delaytime)
{
    unsigned int uidt;

    for (uidt=0; uidt<delaytime; uidt++)
    {
        ;
    }

    return;
}

void ReadASIC(volatile unsigned int *BasedAddr, unsigned int *rdbuf, unsigned int *rdlen, int Size, unsigned int *FlgComm)
{
	int ird;
	volatile unsigned int *RBR;
	volatile unsigned int *LSR;
	volatile unsigned int *FCR;

	RBR = BasedAddr;
	FCR = BasedAddr + 2;
	LSR = BasedAddr + 5;

	for (ird=0; ird<Size; ird++)
	{
		rdbuf[ird] = 0x00;
	}

	(*rdlen) = 0;

	while ((((*LSR) & 0x1) == 0x1) && ((*rdlen) < (unsigned int)Size))
	{
		rdbuf[*rdlen] = (*RBR) & MASK_LO08;

		(*rdlen) = (*rdlen) + 1;
	}

	*FCR = 0xE9;
	*FCR = 0xEF;

	if ((*rdlen) < (unsigned int)Size)
	{
		*FlgComm = FALSE;
	}
	else
	{
		*FlgComm = TRUE;
	}

	return;
}

void WriteASIC(volatile unsigned int *BasedAddr, unsigned int *wrbuf, unsigned int *wrlen)
{
	unsigned int jwr;
	volatile unsigned int *TBR;
	volatile unsigned int *LSR;
	volatile unsigned int *FCR;

	TBR = BasedAddr;
	FCR = BasedAddr + 2;
	LSR = BasedAddr + 5;

	*FCR = 0xE9;
	*FCR = 0xEF;
	Delay(200);		

	if (((*LSR) & 0x20) == 0x20)
	{
		for (jwr=0; jwr<(*wrlen); jwr++)
		{
			*TBR = (*(wrbuf + jwr)) & MASK_LO08;
		}
	}

	(*wrlen) = 0;

	return;
}

void IntProcess(void)
{
	mInput[0].rdlen = 0;
	mInput[1].wrlen = 2;
	ReadASIC(tmpAddr, mInput[0].rdbuf, &mInput[0].rdlen, INPUT_SIZE, &mInput[0].bflgComm);
	WriteASIC(tmpAddr, mInput[1].wrbuf, &mInput[1].wrlen);

	return;
}

void Task(void)
{
	ReadASIC(tmpAddr, mInput[1].rdbuf, &mInput[1].rdlen, INPUT_SIZE, &mInput[1].bflgComm);
	WriteASIC(tmpAddr, mInput[2].wrbuf, &mInput[2].wrlen);
	return;
}

int stakeTask(void *data)
{
    
    while(!kthread_should_stop())
    {
        msleep(1000);
        Task();
    }
    return 0;
}
static int __init dataRace_init(void)
{
    tmpAddr = (volatile unsigned int* )kmalloc(sizeof(int)*10,GFP_KERNEL);
    printk(KERN_ALERT"mInput addr:%p,%p,%d\n",tmpAddr,tmpAddr+2,sizeof(int));
    volatile unsigned int *LSR = tmpAddr + 5;
    *LSR = ((*LSR) | 0x20) | 0x1;
	mInput[1].rdlen = 0;
    mInput[2].wrlen = 2;
    taskThread = kthread_run(stakeTask, NULL, "stakeTask");
    //SA_SHIRQ
    request_irq(10,(irq_handler_t)IntProcess,IRQF_SHARED,"IntProcess",(void *)(IntProcess));

    return 0;
}

static void __exit dataRace_exit(void)
{
    kthread_stop(taskThread);
    printk(KERN_ALERT "Goodbye, keyboard\n");
}

module_init(dataRace_init);
module_exit(dataRace_exit);
MODULE_LICENSE("GPL");
