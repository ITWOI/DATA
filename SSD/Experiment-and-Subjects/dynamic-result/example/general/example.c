#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct task_struct *taskThread = 0;
static int unsigned *bufferAddr = 0;
static int unsigned packetsNumber = 0;
static int unsigned bufferRemainCap = 0;


void delay(unsigned int delaytime)
{
    unsigned int uidt;
    for (uidt=0; uidt<delaytime; uidt++)
    {; }
    return;
}

void task2(void)
{	
	volatile unsigned int *RBR;
	volatile unsigned int *LSR;
	disable_irq(1);
	RBR = bufferAddr;
	LSR = bufferAddr + 1;
	enable_irq(1);
   /*
   ...
   */
	LSR = 0x130000FF;
	packetsNumber = packetsNumber + 1;
}

void task1(void)
{
	delay(200);
   /*
   ...
   */
	bufferAddr = 0x130000A0;
	task2();
}
irqreturn_t irq_handler1(int irq, void *dev_id, struct pt_regs *regs)
{
	bufferAddr = 0x130000A0;
	if(bufferAddr == 0x130000A0)
	{
		packetsNumber = packetsNumber + 1;
		if(bufferRemainCap > 50)
		{
			bufferRemainCap = bufferRemainCap - 1;
		}
	}
   /*
   ...
   */
	return (irqreturn_t) IRQ_HANDLED;
 }

irqreturn_t irq_handler2(int irq, void *dev_id, struct pt_regs *regs)
{
   /*
   ...
   */
	bufferRemainCap = 100;
   /*
   ...
   */
	return (irqreturn_t) IRQ_HANDLED;
}

int stakeTask(void *data)
{

    while(!kthread_should_stop())
    {
        msleep(1000);
        task1();
    }
    return 0;
}

static int __init dataRace_init(void)
{
    taskThread = kthread_run(stakeTask, NULL, "stakeTask");
    request_irq(10,(irq_handler_t)irq_handler1,IRQF_SHARED,"irq_handler1",(void *)(irq_handler1));
    request_irq(11,(irq_handler_t)irq_handler2,IRQF_SHARED,"irq_handler2",(void *)(irq_handler2));
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
