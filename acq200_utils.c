/* acq200_utils - useful stuff */

#define CONFIG_GENERIC_HARDIRQS 1

#include <linux/module.h>

#include <linux/interrupt.h>

#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/pci.h>
#include <linux/sched.h>


#include <asm/io.h>       /* ioremap()         */
#include <asm/irq.h>
#include <asm/uaccess.h>  /* VERIFY_READ|WRITE */
#include <asm/system.h>   /* cli(), *_flags */
#include <asm/segment.h>  /* memcpy and such */


#include "acq200_debug.h"
#include "acq200.h"

struct MASK_ITERATOR {
	unsigned mask;
	unsigned icursor;
	enum state { MI_IDLE, MI_STARTED, MI_FINISHED } state;
};

int hasNext( struct MASK_ITERATOR* it ) {
	unsigned next_cursor = 0;

	switch( it->state ){
	case MI_IDLE:
		next_cursor = 0x80000000;
		it->icursor = 31;
		break;
	case MI_STARTED:
		next_cursor = (1 << it->icursor) >> 1;
		break;
	case MI_FINISHED:
		return 0;
	}
	for ( ; next_cursor != 0; next_cursor >>= 1 ){
		if ( it->mask & next_cursor ){
			return 1;
		}
	}

	return 0;
}
int getNext( struct MASK_ITERATOR* it ) {
	unsigned next_cursor = 0;

	switch( it->state ){
	case MI_IDLE:
		next_cursor = 0x80000000;
		it->state = MI_STARTED;
		it->icursor = 31;
		break;
	case MI_STARTED:
		next_cursor = 1 << --it->icursor;
		break;
	case MI_FINISHED:
		return -1;
	}
	for ( ; next_cursor != 0; next_cursor >>= 1, --it->icursor ){
		if ( it->mask & next_cursor ){
			return it->icursor;
		}
	}

	it->state = MI_FINISHED;
	return -1;
}



int acq200_intsDisable( unsigned irqs )
/**< disable interrupts defined by clear bits in irqs */
{
	struct MASK_ITERATOR it = { irqs };

	while ( hasNext( &it ) ){
		disable_irq_nosync( getNext( &it ) );
	}
	return 0;
}
int acq200_intsEnable( unsigned irqs )
/**< enable interrupts defined by set bits in irqs */
{
	struct MASK_ITERATOR it = { irqs };

	while ( hasNext( &it ) ){
		enable_irq( getNext( &it ) );
	}
	return 0;
}

static unsigned get_mask(char mdef)
{
	char cb[2];
	cb[0] = mdef;
	cb[1] = '\0';
	return simple_strtoul(cb, 0, 16);
}

int acq200_intsMask(struct Acq200Device* device)
/**< disable interrupts defined by clear bits in irqs
 *  re-enable interrupts that are not masked this time, but were last time.
 * */
{
	int inew = strlen(device->imask) -1;	/* index new mask from back */
	int ia = 0;
	int irq = 0;

	for (ia = NR_IRQS/4-1; ia >= 0; --ia, --inew){
		unsigned new_mask = inew>=0? get_mask(device->imask[inew]): 0;
		unsigned active_mask = device->imask_a[ia];
		int bit;

		for (bit = 0x1; (bit & 0x0f) != 0; bit <<= 1, ++irq){

			dbg(2+((!(new_mask&bit))&&(active_mask&bit)),
				"%d %s %s",
				irq,
				(new_mask&bit)? "DISABLE":
				(active_mask&bit)? "enable": "NOT MINE",
				acq200_debug > 2? "STUB":"");
			if (acq200_debug > 2) continue;

			if (new_mask&bit){
				disable_irq_nosync(irq);
				active_mask |= bit;
			}else{
				if (active_mask&bit){
					enable_irq(irq);
					active_mask &= ~bit;
				}
			}
		}
		device->imask_a[ia] = active_mask;
	}

	return 0;
}

int acq200_bkl(struct Acq200Device* device, int bkl_action)
{
	switch(bkl_action){
	case BKL_LOCK:
		spin_lock_irqsave(&device->bkl.lock, device->bkl.flags);
		device->bkl.is_locked = 1;
		break;
	case BKL_UNLOCK:
		spin_unlock_irqrestore(&device->bkl.lock, device->bkl.flags);
		device->bkl.is_locked = 0;
		info();
		break;
	case BKL_QUERY:
	default:
		;
	}
	return device->bkl.is_locked;
}
