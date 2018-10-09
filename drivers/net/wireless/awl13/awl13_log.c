
#include <linux/netdevice.h>
#include "awl13_log.h"


int  awl13_log_flag	 = LOGLVL_1 | LOGLVL_2  /*| LOGLVL_7 */;


/******************************************************************************
 * awl13_print -
 *
 *****************************************************************************/
int 
awl13_print(int level, const char *format, ...)
{
	if (awl13_log_flag & level) {
		int ret;
		va_list args;
		va_start(args, format);
		ret = vprintk(format, args);
		va_end(args);
		return ret;
	}
	return 0;
}

/******************************************************************************
 * awl13_dump -
 *
 *****************************************************************************/
int
awl13_dump(int level, void *buf, int len)
{
	unsigned char *ptr = buf;
	int i;

	if (awl13_log_flag & level) {
		for (i=0; i<len; i++) {
			printk("%02x ", ptr[i]);
			if (i%16 == 15)
				printk("\n");
		}
		if (i%16 != 0)
			printk("\n");
	}
	return 0;
}
