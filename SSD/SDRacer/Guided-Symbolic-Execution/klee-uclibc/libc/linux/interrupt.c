#include "stdlib.h"

int disable_irq(int i)
{
	return 0;
}
int enable_irq(int i)
{
	return 0;
}
int kthread_create_on_node()
{
	return 0;
}

int wake_up_process()
{
	return 0;
}

int __request_region()
{
	return 0;
}
int printk()
{
	return 0;
}

int kthread_should_stop()
{
	return 0;
}

int msleep()
{
	return 0;
}
int request_threaded_irq()
{
	return 0;
}
int no_pci_devices()
{
	return 0;
}
#define no_pci_devices()        (1)

int __tty_alloc_driver()
{
	return 0;
}

int tso_count_descs()
{
	return 0;
}

void *__kmalloc(int size, int type)
{
    return malloc(size);
}

int __const_udelay()
{
	return 0;
}
