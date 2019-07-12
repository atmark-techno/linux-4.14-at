awl13-y +=	awl13_log.o \
		awl13_ioctl.o \
		awl13_fw.o \
		awl13_sysfs.o \
		awl13_wid.o

awl13-$(CONFIG_ARMADILLO_WLAN_AWL13_USB) += awl13_usbdrv.o
awl13-$(CONFIG_ARMADILLO_WLAN_AWL13_SDIO) += awl13_sdiodrv.o

obj-$(CONFIG_ARMADILLO_WLAN_AWL13) += awl13.o

obj-$(CONFIG_ARMADILLO_WLAN_AWL13_WMM)	+= awl13_wmm.o
