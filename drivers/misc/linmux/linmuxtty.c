/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Linmuxtty.c
//
// This file contains the implementation of the linmux tty interface.
//
//////////////////////////////////////////////////////////////////////////////

#include <linux/module.h>
#include <linux/version.h>

#include "linmux.h"

#include "muxdbg.h"
#include "os_wrap.h"
#include "baseport.h"
#include "muxchannel.h"
#include "linmuxcfg.h"


//////////////////////////////////////////////////////////////////////////////
// Metainformation
//////////////////////////////////////////////////////////////////////////////
#define LINMUX_VERSION "3.10"
MODULE_VERSION(LINMUX_VERSION);
MODULE_AUTHOR("Gemalto M2M GmbH <wmSupport@gemalto.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Module providing the Gemalto M2M modules multiplexer capability.");
MODULE_SUPPORTED_DEVICE("Gemalto M2M modules");

//////////////////////////////////////////////////////////////////////////////
// Macros
//////////////////////////////////////////////////////////////////////////////
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
  #define C_CFLAG(tty) tty->termios->c_cflag
#else // KERNEL_VERSION
  #define C_CFLAG(tty) tty->termios.c_cflag
#endif // KERNEL_VERSION

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
  #define TTY_PARAMS   struct tty_struct *tty
#else // KERNEL_VERSION
  #define TTY_PARAMS   struct tty_struct *tty, struct file *filp
#endif // KERNEL_VERSION

//////////////////////////////////////////////////////////////////////////////
// tty support and callbacks
//////////////////////////////////////////////////////////////////////////////

static int   mux_tty_open(struct tty_struct *tty, struct file * filp);
static void  mux_tty_close(struct tty_struct *tty, struct file * filp);
static int   mux_tty_write(struct tty_struct *tty, const unsigned char *buf, int count);
static int   mux_tty_put_char(struct tty_struct *tty, unsigned char ch);
static void  mux_wait_until_sent(struct tty_struct *tty, int timeout);
static int   mux_tty_write_room(struct tty_struct *tty);
static int   mux_tty_chars_in_buffer(struct tty_struct *tty);
static void  mux_flush_buffer(struct tty_struct *tty);
static void  mux_tty_set_termios(struct tty_struct *tty, struct ktermios *old_termios);
static int   mux_tty_install(struct tty_driver *driver, struct tty_struct *tty);
static void  mux_tty_cleanup(struct tty_struct *tty);
static void  mux_tty_unthrottle(struct tty_struct *tty);
static void  mux_tty_shutdown(struct tty_struct *tty);
static int   mux_tty_ioctl(TTY_PARAMS, unsigned int cmd, unsigned long arg);
static int   mux_tty_tiocmget(TTY_PARAMS);
static int   mux_tty_tiocmset(TTY_PARAMS, unsigned int set, unsigned int clear);

//////////////////////////////////////////////////////////////////////////////

static const struct tty_operations if_mux_ops = {
  .open             = mux_tty_open,
  .close            = mux_tty_close,
  .ioctl            = mux_tty_ioctl,
  .write            = mux_tty_write,
  .put_char         = mux_tty_put_char,
  .wait_until_sent  = mux_wait_until_sent,
  .write_room       = mux_tty_write_room,
  .chars_in_buffer  = mux_tty_chars_in_buffer,
  .flush_buffer     = mux_flush_buffer,
  .set_termios      = mux_tty_set_termios,
  .tiocmget         = mux_tty_tiocmget,
  .tiocmset         = mux_tty_tiocmset,
  .unthrottle       = mux_tty_unthrottle,
  .shutdown         = mux_tty_shutdown,
  .install          = mux_tty_install,
  .cleanup          = mux_tty_cleanup,
};

//////////////////////////////////////////////////////////////////////////////

extern struct platform_driver MuxPlatformDriver;

//////////////////////////////////////////////////////////////////////////////

void mux_dev_release(struct device *dev);

//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
//
// In kernel 2.6.36 the usage of the big kernel lock inside the tty driver
// has been replaced by a local locking mechanism. This new mechanism isn't
// recursive any longer. So calling tty_open() inside our implementation of
// tty_open() will cause a system freeze because the mutex is already aquired
// by the tty framework outside of our mux driver (same for tty_close()).
// Because of this situation we need to remove and restore the new tty lock
// inside our tty_open() and tty_close() implementation for kernel 2.6.36 and
// later.
// Since kernel 3.7 the global tty locking has been separeated for each port.
// So the lock on our virtual port doesn't interfere any more with the lock
// when opening the base port and the intermediate unlocking isn't required
// anymore.
//
// Parameters:
// None.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void release_global_tty_lock(void) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "");
  tty_unlock();
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "");
#endif // KERNEL_VERSION
#endif // KERNEL_VERSION
}
void restore_global_tty_lock(void) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "");
  tty_lock();
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "");
#endif // KERNEL_VERSION
#endif // KERNEL_VERSION
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty open function for a multiplex channel.
//
// Parameters:
// tty : The tty structure representing the device.
// filp: The file structure representing the device.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_open(struct tty_struct *tty, struct file *filp) {
  int iRet;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, pFile=%p, Index=%d, minor_start=%d, pMuxChn=%p", tty, filp, tty->index, tty->driver->minor_start, tty->driver_data);
  release_global_tty_lock();
  iRet = mc_Open(tty->driver_data);
  restore_global_tty_lock();
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty close function for a multiplex channel.
//
// Parameters:
// tty : The tty structure representing the device.
// filp: The file structure representing the device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_tty_close(struct tty_struct *tty, struct file *filp) {
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, pFile=%p, Index=%d, pMuxChn=%p", tty, filp, tty->index, tty->driver_data);
  release_global_tty_lock();
  mc_Close(tty->driver_data);
  restore_global_tty_lock();
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty ioctl function for a multiplex channel.
//
// Parameters:
// tty : The tty structure representing the device.
// filp: The file structure representing the device.
// cmd : The ioctl to be executed.
// arg : The ioctl data.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_ioctl(TTY_PARAMS, unsigned int cmd, unsigned long arg) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
  // In kernel 2.6.38 the file object has been removed from the function
  // parameters. Nevertheless this parameter is still required in the
  // n_tty_ioctl_helper() function. This function does some default data
  // magic for unhandled commands without hardware access. Without this
  // default handling the ppp stack doesn't work properly. Because the filp
  // parameter is nowhere used inside the n_tty_ioctl_helper() function
  // except a check for a null pointer since kernel 2.6.39 we simply pass a
  // nonsense pointer.
  struct file *filp = (struct file*)0xdeadbeef;
#endif // KERNEL_VERSION
  PT_MUXCHAN  pMuxChn = tty->driver_data;
  int         iRet;

  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pFile=%p, Arg=%ul, Cmd=0x%x, pMuxChn=%p", tty, tty->index, filp, arg, cmd, pMuxChn);
  iRet = mc_GetErrCodeOnState(pMuxChn);
  if (iRet == 0) {
    switch(cmd) {
      case TIOCGSERIAL: {
        struct serial_struct SerInfo;
        MEMCLR(&SerInfo, sizeof(SerInfo));
        iRet = -EFAULT;
        if (arg) {
          // fake emulate a 16550 uart to make userspace code happy
          SerInfo.type            = PORT_16550A;
          SerInfo.line            = tty->index;
          SerInfo.port            = 0;
          SerInfo.irq             = 0;
          SerInfo.flags           = ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
          SerInfo.xmit_fifo_size  = 1024;
          SerInfo.baud_base       = 115200;
          SerInfo.close_delay     = 5*HZ;
          SerInfo.closing_wait    = 30*HZ;

          if (copy_to_user((void __user *)arg, &SerInfo, sizeof(SerInfo)) == 0) {
            iRet = 0;
          }
        }
        DBGPRINT(ZONE_FCT_TTY_IFACE, "mux_tty_ioctl(TIOCGSERIAL) -> %d", iRet);
        if (iRet == 0) MUXDBGSERSTRUCT(ZONE_FCT_TTY_IFACE, &SerInfo);
        break; }
      case TIOCSSERIAL: {
        struct serial_struct SerInfo;
        if (copy_from_user(&SerInfo, (void __user *)arg, sizeof(SerInfo)) == 0) {
          // TODO: Evaluate the given settings and do something useful
          iRet = 0;
        } else {
          iRet = -EFAULT;
        }
        DBGPRINT(ZONE_FCT_TTY_IFACE, "mux_tty_ioctl(TIOCSSERIAL) -> %d", iRet);
        if (iRet == 0) MUXDBGSERSTRUCT(ZONE_FCT_TTY_IFACE, &SerInfo);
        break;}
      case TIOCSERGETLSR: {
        // A UART's Line-Status-Register register holds the following information:
        // Bit 0 - Data available
        // Bit 1 - Overrun error
        // Bit 2 - Parity error
        // Bit 3 - Framing error
        // Bit 4 - Break signal received
        // Bit 5 - THR is empty
        // Bit 6 - THR is empty, and line is idle
        // Bit 7 - Errornous data in FIFO
        DWORD dwBytesInTx, dwBytesInRx;
        // We can only simulate information about incoming and outgoing data
        if (mc_GetBufferInfo(pMuxChn, &dwBytesInRx, &dwBytesInTx, NULL, NULL)) {
          iRet = (dwBytesInRx ? 1 : 0) | (dwBytesInTx ? 0 : 0x30);
        }
        break; }
      case TIOCMIWAIT:
        iRet = mc_WaitForCommEvent(pMuxChn, arg);
        break;
      case TCFLSH:
        switch(arg) {
          case TCIFLUSH:
            iRet = mc_ClearBuffers(pMuxChn, TRUE, FALSE);
            break;
          case TCOFLUSH:
            iRet = mc_ClearBuffers(pMuxChn, FALSE, TRUE);
            break;
          case TCIOFLUSH:
            iRet = mc_ClearBuffers(pMuxChn, TRUE, TRUE);
            break;
          default:
            iRet = n_tty_ioctl_helper(tty, filp, cmd, arg);
            break;
        }
        break;
      default:
        iRet = n_tty_ioctl_helper(tty, filp, cmd, arg);
        break;
    }
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, Cmd=0x%x, pMuxChn=%p", iRet, tty->index, cmd, pMuxChn);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty write function for a multiplex channel.
//
// Parameters:
// tty  : The tty structure representing the device.
// buf  : Pointer to the data to be written.
// count: The number of bytes to be written.
//
// Return:
// The number of bytes actually being written, a negative error value in case
// of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_write(struct tty_struct *tty, const unsigned char *buf, int count) {
  int iRet;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pBuf=%p, Count=%d, pMuxChn=%p", tty, tty->index, buf, count, tty->driver_data);
  MUXDBGHEX(ZONE_RAW_DATA, (PBYTE)buf, count);
  iRet = mc_WriteData(tty->driver_data, (PBYTE)buf, count);
  if ((iRet >= 0) && (iRet < count)) {
    DBGPRINT(ZONE_FCT_TTY_IFACE, "mux_tty_write(): Only %d of %d bytes written", iRet, count);
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty put_cahr function to write a single byte to a multiplex channel.
//
// Parameters:
// tty: The tty structure representing the device.
// ch : The byte to be written.
//
// Return:
// The number of bytes actually being written, a negative error value in case
// of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_put_char(struct tty_struct *tty, unsigned char ch) {
  int iRet;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, Char=%d, pMuxChn=%p", tty, tty->index, ch, tty->driver_data);
  iRet = mc_WriteData(tty->driver_data, &ch, 1);
  if (iRet != 1) {
    DBGPRINT(ZONE_FCT_TTY_IFACE, "mux_tty_put_char(): Byte not written!");
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty wait_until_sent function of a multiplex channel. This function
// blocks until all outstanding data in the tx queue has been sent to the
// module.
//
// Parameters:
// tty     : The tty structure representing the device.
// timeout : Timout in milliseconds for the wait call.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_wait_until_sent(struct tty_struct *tty, int timeout) {
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, Timeout=%d, pMuxChn=%p", tty, tty->index, timeout, tty->driver_data);
  mc_WaitForTxEmpty(tty->driver_data, timeout);
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty write_room function of a multiplex channel. This function returns
// the space in bytes free in the tx queue.
//
// Parameters:
// tty: The tty structure representing the device.
//
// Return:
// The number of free bytes in the tx queue, a negative error value in case
// of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_write_room(struct tty_struct *tty) {
  DWORD dwBytesInTx, dwTxSize;
  int iRet;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p", tty, tty->index, tty->driver_data);
  iRet = mc_GetBufferInfo(tty->driver_data, NULL, &dwBytesInTx, NULL, &dwTxSize);
  if (iRet == 0) {
    iRet = dwTxSize - dwBytesInTx;
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty chars_in_buffer function of a multiplex channel. This function
// returns number of unsent bytes in the tx queue.
//
// Parameters:
// tty: The tty structure representing the device.
//
// Return:
// The number unsent bytes in the tx queue, a negative error value in case
// of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_chars_in_buffer(struct tty_struct *tty) {
  DWORD  dwBytesInTx;
  int    iRet;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p", tty, tty->index, tty->driver_data);
  iRet = mc_GetBufferInfo(tty->driver_data, NULL, &dwBytesInTx, NULL, NULL);
  if (iRet == 0) {
    iRet = dwBytesInTx;
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty flush_buffer function of a multiplex channel. This function
// deletes all unsent bytes from the tx queue without sending them.
//
// Parameters:
// tty: The tty structure representing the device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_flush_buffer(struct tty_struct *tty) {
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p", tty, tty->index, tty->driver_data);
  mc_ClearBuffers(tty->driver_data, FALSE, TRUE);
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty unthrottle function of a multiplex channel. This function caues
// the data flow to be continued after having been throttled.
//
// Parameters:
// tty: The tty structure representing the device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_tty_unthrottle(struct tty_struct *tty) {
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p, pBasePort=%p", tty, tty->index, tty->driver_data, tty->driver->driver_state);
  mc_ContinueReceive(tty->driver_data);
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty set_termios function of a multiplex channel. Usually this
// function adjusts several settings of a serial port. For our virtual
// multiplex channels only the setting of the hardware flow control is of
// interest.
//
// Parameters:
// tty        : The tty structure representing the device (containing
//              already the new settings).
// old_termios: The ktermios structure with the old settings.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_tty_set_termios(struct tty_struct *tty, struct ktermios *old_termios) {
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pOldTermios=%p, pMuxChn=%p", tty, tty->index, old_termios, tty->driver_data);
  if ((old_termios->c_cflag & CRTSCTS) != (C_CFLAG(tty) & CRTSCTS)) {
    mc_SetFlowCtrl(tty->driver_data, (C_CFLAG(tty) & CRTSCTS) ? TRUE : FALSE);
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty tiocmget function of a multiplex channel. This function
// returns the current state of the incoming modem signals.
//
// Parameters:
// tty : The tty structure representing the device.
// filp: The file structure representing the device.
//
// Return:
// The current state of the modem signals, a negative error value in case
// of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_tiocmget(TTY_PARAMS) {
  int iRet;
  DWORD dwState;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p", tty, tty->index, tty->driver_data);
  iRet = mc_GetModemState(tty->driver_data, &dwState);
  if (iRet == 0) {
    iRet = (int)dwState;
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=0x%x, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty tiocmset function of a multiplex channel. This function sets the
// state of the outgoing modem signals.
//
// Parameters:
// tty  : The tty structure representing the device.
// filp : The file structure representing the device.
// set  : The modem signals to be set.
// clear: The modem signals to be cleared.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_tiocmset(TTY_PARAMS, unsigned int set, unsigned int clear) {
  int iRet;
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, Set=0x%x, Clear=0x%x, pMuxChn=%p", tty, tty->index, set, clear, tty->driver_data);
  iRet = mc_SetModemState(tty->driver_data, set);
  if (iRet == 0) {
    iRet = mc_ClearModemState(tty->driver_data, clear);
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Index=%d, pMuxChn=%p", iRet, tty->index, tty->driver_data);
  return iRet;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
//////////////////////////////////////////////////////////////////////////////
//
// Allocate and initialize a device's tty structure. This function wasn't
// exported prior to kernel 2.6.31. So we have to implement it for older
// kernel versions locally.
//
// Parameters:
// tty: The tty structure to be initialized.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int tty_init_termios_local(struct tty_struct *tty) {
  struct ktermios *tp;
  int idx = tty->index;

  tp = tty->driver->termios[idx];
  if (tp == NULL) {
    tp = kzalloc(sizeof(struct ktermios[2]), GFP_KERNEL);
    if (tp == NULL) {
      return -ENOMEM;
    }
    MEMCPY(tp, &tty->driver->init_termios, sizeof(struct ktermios));
    tty->driver->termios[idx] = tp;
  }
  tty->termios = tp;
  tty->termios_locked = tp + 1;

  // Compatibility until drivers always set this
  tty->termios->c_ispeed = tty_termios_input_baud_rate(tty->termios);
  tty->termios->c_ospeed = tty_termios_baud_rate(tty->termios);
  return 0;
}
#define TTY_INIT_TERMIOS tty_init_termios_local
#else // KERNEL_VERSION
#define TTY_INIT_TERMIOS tty_init_termios
#endif // KERNEL_VERSION

//////////////////////////////////////////////////////////////////////////////
//
// Create and initialize the tty port device.
//
// Parameters:
// tty : The parent tty object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void CreateTtyPort(struct tty_struct *tty) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p", tty, tty->index, tty->driver_data);
  if (!tty->port) {
    tty->port = oswrap_GetMem(sizeof(struct tty_port));
  }
  if (tty->port) {
    tty_port_init(tty->port);
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
#endif // KERNEL_VERSION
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy the tty port device.
//
// Parameters:
// tty : The parent tty object.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void DestroyTtyPort(struct tty_struct *tty) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p", tty, tty->index, tty->driver_data);
  if (tty->port) {
    tty_port_destroy(tty->port);
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
#endif // KERNEL_VERSION
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty install function. It is called when a mux port is opened for the
// first time and is used to initialize the mux channel object.
//
// Parameters:
// driver: The driver object representing the linmux driver instance.
// tty   : The tty structure representing the device.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_tty_install(struct tty_driver *driver, struct tty_struct *tty) {
  PT_BASEPORT pBasePort = tty->driver->driver_state;
  int         iRet = 0;
  DWORD       dwDlci = (tty->index - tty->driver->minor_start) + 1;
  PT_MUXCHAN  pMuxChan;

  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pDriver=%p, pTty=%p, Name=%s, Index=%d, pMuxChn=%p", driver, tty, driver->name, tty->index, tty->driver_data);

  if (tty->driver_data == NULL) {
    iRet = IncOpenCount(pBasePort);
    if (iRet) {
      DBG_LEAVE(ZONE_ERR, "Error: Driver locked!");
      return iRet;
    }
  }

  pMuxChan = mc_Create(pBasePort, dwDlci, tty);
  if (pMuxChan) {
    CreateTtyPort(tty);
  } else {
    DBG_LEAVE(ZONE_ERR, "Error: No mux channel object!");
    DecOpenCount(pBasePort);
    return -ENOMEM;
  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
  iRet = tty_standard_install(driver, tty);
#else // KERNEL_VERSION
  iRet = TTY_INIT_TERMIOS(tty);
  if (iRet == 0) {
    tty_driver_kref_get(driver);
    tty->count++;
    driver->ttys[tty->index] = tty;
  }
#endif // KERNEL_VERSION
  if (iRet == 0) {
    mc_SetFlowCtrl(pMuxChan, (C_CFLAG(tty) & CRTSCTS) ? TRUE : FALSE);
    tty->driver_data = pMuxChan;
  } else {
    mc_Destroy(pMuxChan);
    DestroyTtyPort(tty);
    DecOpenCount(pBasePort);
  }

  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Ret=%i, Name=%s, Index=%d, pMuxChn=%p", iRet, driver->name, tty->index, tty->driver_data);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty shutdown function. It is called when the last opened mux port has
// been closed and is used to tidy up and remove the the mux channel object.
//
// Parameters:
// tty: The tty structure representing the device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_tty_shutdown(struct tty_struct *tty) {
  PT_BASEPORT pBasePort = tty->driver->driver_state;
  PT_MUXCHAN  pMuxChan = tty->driver_data;

  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, pMuxChn=%p, pBasePort=%p", tty, tty->index, tty->driver_data, tty->driver->driver_state);

  if (pMuxChan) {
    mc_Destroy(pMuxChan);
    DestroyTtyPort(tty);
    tty->driver_data = NULL;
  }

  // Standard shutdown processing
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
  tty_shutdown(tty);
#else // KERNEL_VERSION
  tty_free_termios(tty);
#endif // KERNEL_VERSION
#endif // KERNEL_VERSION

  DecOpenCount(pBasePort);

  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////
//
// The tty cleanup function. It is called to finally tidy up remaining port
// resources.
//
// Parameters:
// tty: The tty structure representing the device.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_tty_cleanup(struct tty_struct *tty) {
  DBG_ENTER(ZONE_FCT_TTY_IFACE, "pTty=%p, Index=%d, Port=%p, pMuxChn=%p", tty, tty->index, tty->port, tty->driver_data);
  // Free possibly existing port object
  if (tty->port) {
    oswrap_FreeMem(tty->port);
    tty->port = NULL;
  }
  DBG_LEAVE(ZONE_FCT_TTY_IFACE, "Index=%d, pMuxChn=%p", tty->index, tty->driver_data);
}

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// Load and initialize a linmux driver instance. This function is called from
// mux_serial_init() for all driver instances.
//
// Parameters:
// iInstance: The number of the driver instance to be loaded.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int mux_load_driver(unsigned int iInstance) {
  int iResult;
  struct tty_driver *pDriver;
  PT_BASEPORT pBasePort = pMuxBasePorts[iInstance];

  pBasePort->m_pMuxTtyDriver = alloc_tty_driver(bp_GetMaxChannels(pBasePort));
  if (!pBasePort->m_pMuxTtyDriver) {
    DBGPRINT(ZONE_ERR, "alloc_tty_driver() failed -> -ENOMEM");
    return -ENOMEM;
  }

  pDriver = pBasePort->m_pMuxTtyDriver;
  pDriver->owner = THIS_MODULE;
  pDriver->driver_name = "mux";
  pDriver->name = bp_GetDeviceName(pBasePort);
  pDriver->major = MUX_TTY0_MAJOR + iInstance;

  pDriver->minor_start = 0;
  pDriver->type = TTY_DRIVER_TYPE_SERIAL;
  pDriver->subtype = SERIAL_TYPE_NORMAL;
  pDriver->flags = TTY_DRIVER_REAL_RAW; // TTY_DRIVER_DYNAMIC_DEV
  pDriver->init_termios.c_iflag = 0;
  pDriver->init_termios.c_oflag = 0;
  pDriver->init_termios.c_lflag = 0;
  pDriver->init_termios.c_cflag = B115200 | CS8 | CRTSCTS | CREAD | HUPCL;
  pDriver->init_termios.c_ispeed = 115200;
  pDriver->init_termios.c_ospeed = 115200;
  pDriver->driver_state = pBasePort;

  tty_set_operations(pDriver, &if_mux_ops);

  iResult = tty_register_driver(pDriver);
  if (iResult) {
    DBGPRINT(ZONE_ERR, "tty_register_driver failed() -> %d", iResult);
    put_tty_driver(pDriver);
    pBasePort->m_pMuxTtyDriver = NULL;
    return iResult;
  }

#if defined(PWRMGMT_ENABLED) && PWRMGMT_ENABLED
  pBasePort->m_pPlatformDevice = platform_device_alloc(LINMUX_PLATFORM_NAME, iInstance);
  if (!pBasePort->m_pPlatformDevice) {
    DBGPRINT(ZONE_ERR, "platform_device_alloc() failed -> -ENOMEM");
    return -ENOMEM;
  }

  pBasePort->m_pPlatformDevice->dev.release = mux_dev_release;
  pBasePort->m_pPlatformDevice->dev.platform_data = pBasePort;
  iResult = platform_device_add(pBasePort->m_pPlatformDevice);
  if (iResult) {
    DBGPRINT(ZONE_ERR, "platform_device_register() failed -> %d", iResult);
    platform_device_put(pBasePort->m_pPlatformDevice);
    pBasePort->m_pPlatformDevice = NULL;
    return iResult;
  }
#endif // PWRMGMT_ENABLED

  pBasePort->m_iOpenCount = 0;

  return iResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// Unload a linmux driver instance. This function is called from
// mux_serial_exit() for all driver instances.
//
// Parameters:
// iInstance: The number of the driver instance to be unloaded.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void mux_unload_driver(unsigned int iInstance) {
  if (pMuxBasePorts && pMuxBasePorts[iInstance]) {
    if (pMuxBasePorts[iInstance]->m_pPlatformDevice) {
      platform_device_del(pMuxBasePorts[iInstance]->m_pPlatformDevice);
      platform_device_put(pMuxBasePorts[iInstance]->m_pPlatformDevice);
      pMuxBasePorts[iInstance]->m_pPlatformDevice = NULL;
    }
    if (pMuxBasePorts[iInstance]->m_pMuxTtyDriver) {
      tty_unregister_driver(pMuxBasePorts[iInstance]->m_pMuxTtyDriver);
      put_tty_driver(pMuxBasePorts[iInstance]->m_pMuxTtyDriver);
      pMuxBasePorts[iInstance]->m_pMuxTtyDriver = NULL;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Destroy the linmux driver instance.
//
// Parameters:
// None.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void mux_serial_destroy(void) {
  int i;

  for (i = 0; i < gInstances; i++) {
    mux_unload_driver(i);
  }

  mux_fs_exit();

  platform_driver_unregister(&MuxPlatformDriver);
}

//////////////////////////////////////////////////////////////////////////////
//
// The linmux init function. It is called when the driver is loaded by the
// system.
//
// Parameters:
// None.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int __init mux_serial_init(void) {
  int result, i;

  DBGPRINT(ZONE_DRV_INIT, "*** Enter Cinterion Wireless Modules Serial Multiplex Driver ***");
  DBGPRINT(ZONE_DRV_INIT, "Kernel-Version: %d.%d.%d", LINUX_VERSION_CODE >> 16, (LINUX_VERSION_CODE >> 8) & 0xff, LINUX_VERSION_CODE & 0xff);
  DBGPRINT(ZONE_DRV_INIT, "LinMux-Version: %s", LINMUX_VERSION);

  result = platform_driver_register(&MuxPlatformDriver);
  if (result) {
    DBGPRINT(ZONE_ERR, "platform_driver_register failed() -> %d", result);
  } else {
    result = mux_fs_init();
    if (result) {
      DBGPRINT(ZONE_ERR, "mux_fs_init() failed -> %d", result);
      mux_serial_destroy();
    } else {
      for (i = 0; i < gInstances; i++) {
        result = mux_load_driver(i);
        if (result) {
          mux_serial_destroy();
          break;
        }
      }
    }
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////
//
// The linmux exit function. It is called when the driver is unloaded by the
// system.
//
// Parameters:
// None.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
static void __exit mux_serial_exit(void) {
  mux_serial_destroy();
  DBGPRINT(ZONE_DRV_INIT, "*** Exit Cinterion Wireless Modules Serial Multiplex Client Driver ***");
}

//////////////////////////////////////////////////////////////////////////////

module_init(mux_serial_init);
module_exit(mux_serial_exit);

