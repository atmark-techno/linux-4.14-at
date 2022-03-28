###############################################################################
#
# (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>
#
# All Rights Reserved.
#
# Cinterion provides this source code under the GPL v2 License.
# The GPL v2 license is available at
#
# http://opensource.org/licenses/gpl-license.php
#
###############################################################################

###############################################################################
# User parameters - Start
###############################################################################

# Use multiplex protocol in host or in client mode. This provides not the
# possibility for building the driver as client driver just by modifying this
# flag. It is only for simpler reuse of some source files.
client_mode         := 0

# Enable or disable trace output
trace_enabled       := 1

# Enable or disable power management
power_mgmt_enabled  := 0

# Path to kernel sources
ifneq ($(BB_KERNEL_SOURCES),)
  kernel_src := $(BB_KERNEL_SOURCES)
else
  kernel_src := /lib/modules/$(shell uname -r)/build
endif

###############################################################################
# User parameters - End
###############################################################################


ifneq ($(client_mode),)
  Features := -DCLIENT_MODE=$(client_mode)
endif
ifneq ($(trace_enabled),)
  Features := $(Features) -DTRACE_ENABLED=$(trace_enabled)
endif
ifneq ($(power_mgmt_enabled),)
  Features := $(Features) -DPWRMGMT_ENABLED=$(power_mgmt_enabled)
endif


ifneq ($(KERNELRELEASE),)
  ifeq ($(M),)
    BuiltInKernelModule=1
  endif
endif

ifneq ($(BuiltInKernelModule),)

# Make section for kernel integration

ifneq ($(ANDROID_BUILD_TOP),)
  # Android has the Linux kernel in the kernel/ subdirectory
  include $(ANDROID_BUILD_TOP)/kernel/drivers/misc/linmux/major_devnums
else
  include $(PWD)/drivers/misc/linmux/major_devnums
endif

EXTRA_CFLAGS = -DTTY_DEVNUM=$(tty_major_devnum) -DFS_DEVNUM=$(fs_major_devnum) -DFS_DEVNAME='"$(fs_device_name)"' $(Features)

obj-y += linmuxtty.o
obj-y += linmuxcfg.o
obj-y += linmuxpwr.o
obj-y += os_wrap.o
obj-y += ringbuffer.o
obj-y += baseport.o
obj-y += mux_interface.o
obj-y += muxchannel.o
obj-y += mux_msgqueue.o
obj-y += muxdbg.o
obj-y += ddmpadp.o
obj-y += ddmpfra.o
obj-y += ddmpfunc.o
obj-y += ddmphdlc.o
obj-y += ddmpiov2.o
obj-y += ddmpmem.o
obj-y += ddmppkt.o
obj-y += ddmpstat.o
obj-y += ddmptype.o

else # BuiltInKernelModule

# Make section for loadable kernel module

ifeq ($(M),)
  ifneq ($(ANDROID_BUILD_TOP),)
    include $(PWD)/kernel/drivers/misc/linmux/major_devnums
  else
    include $(PWD)/major_devnums
  endif
else
  include $(M)/major_devnums
endif

ifeq ($(LinmuxModuleName),)
  LinmuxModuleName := linmux
endif

Instances := 1
Ports := 3

ifeq ($(LinmuxInstDir),)
  ifeq ($(shell test -d "/lib/modules/$(shell uname -r)/kernel/drivers/tty/serial";echo $$?), 0)
    LinmuxInstDir := /lib/modules/$(shell uname -r)/kernel/drivers/tty/serial
  else
    LinmuxInstDir := /lib/modules/$(shell uname -r)/kernel/drivers/serial
  endif
endif

ifeq ($(LinmuxInstScript),)
  LinmuxInstScript := linmux-install
endif

EXTRA_CFLAGS = -DTTY_DEVNUM=$(tty_major_devnum) -DFS_DEVNUM=$(fs_major_devnum) -DFS_DEVNAME='"$(fs_device_name)"' $(Features)

obj-m += $(LinmuxModuleName).o

$(LinmuxModuleName)-objs := \
  linmuxtty.o          \
  linmuxcfg.o          \
  linmuxpwr.o          \
  os_wrap.o            \
  ringbuffer.o         \
  baseport.o           \
  mux_interface.o      \
  muxchannel.o         \
  mux_msgqueue.o       \
  muxdbg.o             \
  ddmpadp.o            \
  ddmpfra.o            \
  ddmpfunc.o           \
  ddmphdlc.o           \
  ddmpiov2.o           \
  ddmpmem.o            \
  ddmppkt.o            \
  ddmpstat.o           \
  ddmptype.o

$(LinmuxModuleName):
	$(MAKE) -C $(kernel_src) M=$(PWD) modules

all: clean $(LinmuxModuleName) instscript

clean:
	$(MAKE) -C $(kernel_src) M=$(PWD) clean
	-rm $(LinmuxInstScript)

install: uninstall all
	cp $(LinmuxModuleName).ko $(LinmuxInstDir)
	insmod $(LinmuxInstDir)/$(LinmuxModuleName).ko Instances=$(Instances) Ports=$(Ports)
	mknod /dev/$(fs_device_name) c $(fs_major_devnum) 0

uninstall:
	-unlink /dev/$(fs_device_name)
	-rmmod $(LinmuxModuleName)
	-rm $(LinmuxInstDir)/$(LinmuxModuleName).ko

instscript:
	echo "#!/bin/sh" > $(LinmuxInstScript)
	echo insmod $(LinmuxInstDir)/$(LinmuxModuleName).ko Instances=$(Instances) Ports=$(Ports) >> $(LinmuxInstScript)
	echo mknod /dev/$(fs_device_name) c $(fs_major_devnum) 0 >> $(LinmuxInstScript)
	chmod +x $(LinmuxInstScript)

endif # BuiltInKernelModule

