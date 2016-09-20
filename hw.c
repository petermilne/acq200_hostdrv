#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("GPL");


static void get_max(void) 
{
	void *buf;
	int order;

	for (order = 12; order >= 0; --order){
		buf = __get_free_pages(GFP_KERNEL, order);
		if (buf){
			printk(KERN_ALERT "%2d [0x%08x bytes] SUCCESS\n",
			       order, (1<<order)*4096);
			break;
		}else{
			printk(KERN_ALERT "%2d [0x%08x bytes] FAIL\n",
			       order, (1<<order)*4096);
		}
	}
}


static int hello_init(void) 
{
	printk(KERN_ALERT "Hello World\n");
	get_max();
	get_max();
	return 0;
}


static void hello_exit(void)
{
	printk(KERN_ALERT "Ciao\n");
}


module_init(hello_init);
module_exit(hello_exit);
