/*
 * To use USB or SDIO, include does awl13_usbdrv.h or awl13_sdiodrv.h
 */
#ifndef _AWL13_DEVICE_H_
#define _AWL13_DEVICE_H_


/*********************
 * Common Driver Info
 *********************/
#define AWL13_DRVINFO				"awl13"
#define AWL13_VERSION				"3.0.2"

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
#include "awl13_sdiodrv.h"
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
#include "awl13_usbdrv.h"
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */



#endif /* _AWL13_DRV_H_ */
