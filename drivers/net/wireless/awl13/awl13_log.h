/*
 * To use USB, include does awl13_log.h.
 */
#ifndef _AWL13_LOG_H_
#define _AWL13_LOG_H_


#include <linux/version.h>
#define FIRMWARE_SIZE_MAX (288 * 1024)
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
#define FIRMWARE_BLOCK_MAX_SIZE	(128 * 1024)		/* in byte */
#define FIRMWARE_N_BLOCKS ((FIRMWARE_SIZE_MAX % FIRMWARE_BLOCK_MAX_SIZE) ? \
		   ((FIRMWARE_SIZE_MAX / FIRMWARE_BLOCK_MAX_SIZE) + 1) : \
		   (FIRMWARE_SIZE_MAX / FIRMWARE_BLOCK_MAX_SIZE))
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) */


/***************
 * log and dump
 ***************/
extern int awl13_print(int level, const char *format, ...);
extern int awl13_dump(int level, void *buf, int len);

extern int awl13_log_flag;

#define LOGLVL_1 0x1 /* EMERG, ALERT, CRIT, ERR */
#define LOGLVL_2 0x2 /* WARNING, NOTICE, INFO */
#define LOGLVL_3 0x4 /* DEBUG */
#define LOGLVL_4 0x8 /* DEBUG-DUMP */

#define LOGLVL_5 0x10  /* DEVELOP */
#define LOGLVL_6 0x20  /* DEVELOP-DUMP */
#define LOGLVL_7 0x40  /* DEVELOP */
#define LOGLVL_8 0x80  /* DEVELOP-DUMP */

#define awl_err(args...) awl13_print(LOGLVL_1, KERN_ERR "awl13: " args)
#define awl_info(args...) awl13_print(LOGLVL_2, KERN_INFO "awl13: " args)

#if defined(DEBUG)
#define awl_debug(args...) awl13_print(LOGLVL_3, KERN_INFO "awl13: " args)
#define awl_dump(b,s) awl13_dump(LOGLVL_4, b,s)
#define awl_develop(args...) awl13_print(LOGLVL_5, KERN_INFO "awl13: " args)
#define awl_devdump(b,s) awl13_dump(LOGLVL_6, b,s)
#define awl_develop2(args...) awl13_print(LOGLVL_7, KERN_INFO "awl13: " args)
#define awl_devdump2(b,s) awl13_dump(LOGLVL_8, b,s)
#else
#define awl_debug(args...)
#define awl_dump(b,s)
#define awl_develop(args...)
#define awl_devdump(b,s)
#define awl_develop2(args...)
#define awl_devdump2(b,s)
#endif

#endif /* _AWL13_LOG_H_ */
