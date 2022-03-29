/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Linmuxcfg.c
//
// This file contains the implementation of the linmux configuration
// interface. For easy configuration via a command line tool using ioctls
// the linmux driver exports a character device interface. This interface
// is implemeted here.
//
//////////////////////////////////////////////////////////////////////////////

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>

#include "linmuxcfg.h"
#include "baseport.h"
#include "muxdbg.h"

//////////////////////////////////////////////////////////////////////////////

#define INSTANCE_STATE_IDLE           0
#define INSTANCE_STATE_RELOADING      1

//////////////////////////////////////////////////////////////////////////////

// The number of available driver instances, will be configurable as "Instances" on module load
unsigned long gInstances = DEFAULT_NUM_OF_INSTANCES;
module_param_named(Instances, gInstances, ulong, 0);
MODULE_PARM_DESC(Instances, "The number of multiplexer instances");

// The number of available multiplex ports, will be configurable as "Ports" on module load
unsigned long gPorts = DEFAULT_NUM_OF_PORTS;
module_param_named(Ports, gPorts, ulong, 0);
MODULE_PARM_DESC(Ports, "The default number of multiplexer ports");

//////////////////////////////////////////////////////////////////////////////

int mux_load_driver(unsigned int iInstance);
void mux_unload_driver(unsigned int iInstance);

static int mux_fs_open(struct inode *, struct file *);
static int mux_fs_release(struct inode *, struct file *);
static long mux_fs_ioctl(struct file *, unsigned int, unsigned long);

//////////////////////////////////////////////////////////////////////////////

struct file_operations fops = {
  .open            = mux_fs_open,
  .release         = mux_fs_release,
  .unlocked_ioctl  = mux_fs_ioctl,
};

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// Thread safe incrementation of a driver instances open count.
//
// Parameters:
// pBasePort: A pointer to the base port object as reference to the driver
//            instance.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int IncOpenCount(PT_BASEPORT pBasePort) {
  int iRet = -EAGAIN;

  oswrap_SpinLock(&(pBasePort->m_slLockOpenCount));
  if (pBasePort->m_iInstanceState == INSTANCE_STATE_IDLE) {
    pBasePort->m_iOpenCount++;
    iRet = 0;
  }
  oswrap_SpinUnlock(&(pBasePort->m_slLockOpenCount));
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// Thread safe decrementation of a driver instances open count.
//
// Parameters:
// pBasePort: A pointer to the base port object as reference to the driver
//            instance.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void DecOpenCount(PT_BASEPORT pBasePort) {
  oswrap_SpinLock(&(pBasePort->m_slLockOpenCount));
  if (pBasePort->m_iOpenCount) {
    pBasePort->m_iOpenCount--;
  }
  oswrap_SpinUnlock(&(pBasePort->m_slLockOpenCount));
}

//////////////////////////////////////////////////////////////////////////////
//
// Unload and reload a linmux driver instance. This is necessary to make some
// of the settings valid.
//
// Parameters:
// iInstance: The number of the driver instance to be reloaded.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static int ReloadInstance(int iInstance) {
  PT_BASEPORT pBasePort = pMuxBasePorts[iInstance];
  oswrap_SpinLock(&(pBasePort->m_slLockOpenCount));
  if (pBasePort->m_iOpenCount || (pBasePort->m_iInstanceState != INSTANCE_STATE_IDLE)) {
    oswrap_SpinUnlock(&(pBasePort->m_slLockOpenCount));
    return -EBUSY;
  }
  pBasePort->m_iInstanceState = INSTANCE_STATE_RELOADING;
  oswrap_SpinUnlock(&(pBasePort->m_slLockOpenCount));

  mux_unload_driver(iInstance);
  mux_load_driver(iInstance);

  oswrap_SpinLock(&(pBasePort->m_slLockOpenCount));
  pBasePort->m_iInstanceState = INSTANCE_STATE_IDLE;
  oswrap_SpinUnlock(&(pBasePort->m_slLockOpenCount));
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// The open function of the linmux character device interface.
//
// Parameters:
// inode: The inode structure representing the device (unused).
// file : The file structure representing the device (unused).
//
// Return:
// Since we only increment the open counter always 0 indicating success.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_fs_open(struct inode *inode, struct file *file) {
  DBG_ENTER(ZONE_FCT_CFG_IFACE, "Inode=%p, File=%p", inode, file);
  try_module_get(THIS_MODULE);
  DBG_LEAVE(ZONE_FCT_CFG_IFACE, "Ret=0");
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// The close function of the linmux character device interface.
//
// Parameters:
// inode: The inode structure representing the device (unused).
// file : The file structure representing the device (unused).
//
// Return:
// Since we only decrement the open counter always 0 indicating success.
//
//////////////////////////////////////////////////////////////////////////////
static int mux_fs_release(struct inode *inode, struct file *file) {
  DBG_ENTER(ZONE_FCT_CFG_IFACE, "Inode=%p, File=%p", inode, file);
  module_put(THIS_MODULE);
  DBG_LEAVE(ZONE_FCT_CFG_IFACE, "Ret=0");
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// The linmux character device's ioctl handler. Here all configuration ioctls
// are arriving and handled. For a detailed description of the different
// configuration ioctls and their usage refer to the help text of the linmux
// configuration program in "../config/linmuxcfg.c".
//
// Parameters:
// file       : The file structure representing the device (unused).
// ioctl_num  : The ictl to be handled.
// ioctl_param: Data belonging to the passed ioctl.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
static long mux_fs_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
  PT_BASEPORT pBasePort;
  MuxCfgData Data;
  long lRet = -EINVAL;
  int iLen;

  DBG_ENTER(ZONE_FCT_CFG_IFACE, "File=%p, Cmd=%x, Param=%p", file, ioctl_num, ioctl_param);
  lRet = copy_from_user((void*)&Data, (const void __user *)ioctl_param, sizeof(MuxCfgData));
  if (lRet == 0) {
    if ((Data.iInstance >= 0) && (Data.iInstance < gInstances)) {
      pBasePort = pMuxBasePorts[Data.iInstance];
      switch (ioctl_num) {
        case IOCTL_MUXCFG_RELOAD:
          lRet = ReloadInstance(Data.iInstance);
          break;
        case IOCTL_MUXCFG_SET_BAUDRATE:
          bp_SetBaudRate(pBasePort, Data.iValue);
          lRet = 0;
          break;
        case IOCTL_MUXCFG_GET_BAUDRATE:
          Data.iValue = bp_GetBaudRate(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_BASEPORT:
          iLen = strlen(Data.strBuf);
          if ((iLen < MAX_PORTNAME_LEN) && (iLen > 0)) {
            bp_SetBasePortName(pBasePort, Data.strBuf);
            lRet = 0;
          }
          break;
        case IOCTL_MUXCFG_GET_BASEPORT:
          MEMCLR(Data.strBuf, sizeof(Data.strBuf));
          strcpy(Data.strBuf, bp_GetBasePortName(pBasePort));
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_CHNNUM:
          if (Data.iValue >= 0) {
            bp_SetMaxChannels(pBasePort, Data.iValue);
            lRet = 0;
          }
          break;
        case IOCTL_MUXCFG_GET_CHNNUM:
          Data.iValue = bp_GetMaxChannels(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_DEVNAME:
          iLen = strlen(Data.strBuf);
          if ((iLen < MAX_PORTNAME_LEN) && (iLen > 0)) {
            bp_SetDeviceName(pBasePort, Data.strBuf);
            lRet = 0;
          }
          break;
        case IOCTL_MUXCFG_GET_DEVNAME:
          MEMCLR(Data.strBuf, sizeof(Data.strBuf));
          strcpy(Data.strBuf, bp_GetDeviceName(pBasePort));
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_INITCMD:
          if ((Data.iIndex >= 0) && (Data.iIndex < MAX_INITCMD_NUM)) {
            if (strlen(Data.strBuf) < MAX_INTCMD_LEN) {
              bp_SetInitCmd(pBasePort, Data.iIndex, Data.strBuf);
              lRet = 0;
            }
          }
          break;
        case IOCTL_MUXCFG_GET_INITCMD:
          if ((Data.iIndex >= 0) && (Data.iIndex < MAX_INITCMD_NUM)) {
            MEMCLR(Data.strBuf, sizeof(Data.strBuf));
            strcpy(Data.strBuf, bp_GetInitCmd(pBasePort, Data.iIndex));
            lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          }
          break;
        case IOCTL_MUXCFG_SET_DEINITCMD:
          if ((Data.iIndex >= 0) && (Data.iIndex < MAX_INITCMD_NUM)) {
            if (strlen(Data.strBuf) < MAX_INTCMD_LEN) {
              bp_SetDeinitCmd(pBasePort, Data.iIndex, Data.strBuf);
              lRet = 0;
            }
          }
          break;
        case IOCTL_MUXCFG_GET_DEINITCMD:
          if ((Data.iIndex >= 0) && (Data.iIndex < MAX_INITCMD_NUM)) {
            MEMCLR(Data.strBuf, sizeof(Data.strBuf));
            strcpy(Data.strBuf, bp_GetDeinitCmd(pBasePort, Data.iIndex));
            lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          }
          break;
        case IOCTL_MUXCFG_SET_MAXMUXVER:
          bp_SetMaxMuxVersion(pBasePort, Data.iValue);
          lRet = 0;
          break;
        case IOCTL_MUXCFG_GET_MAXMUXVER:
          Data.iValue = bp_GetMaxMuxVersion(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_HDLCWINSIZE:
          if ((Data.iValue >= 1) && (Data.iValue <= 7)) {
            bp_SetHdlcWindowSize(pBasePort, Data.iValue);
            lRet = 0;
          }
          break;
        case IOCTL_MUXCFG_GET_HDLCWINSIZE:
          Data.iValue = bp_GetHdlcWindowSize(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_HDLCFRAMESIZE:
          if ((Data.iValue >= 1) && (Data.iValue <= MAXFRAMESIZE)) {
            bp_SetHdlcFrameSize(pBasePort, Data.iValue);
            lRet = 0;
          }
          break;
        case IOCTL_MUXCFG_GET_HDLCFRAMESIZE:
          Data.iValue = bp_GetHdlcFrameSize(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_INIT:
          bp_SetInitEnabled(pBasePort, Data.iValue ? TRUE : FALSE);
          lRet = 0;
          break;
        case IOCTL_MUXCFG_GET_INIT:
          Data.iValue = bp_GetInitEnabled(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_SWITCHOFF:
          bp_SetSwitchOffEnabled(pBasePort, Data.iValue ? TRUE : FALSE);
          lRet = 0;
          break;
        case IOCTL_MUXCFG_GET_SWITCHOFF:
          Data.iValue = bp_GetSwitchOffEnabled(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_TRACEMASK:
          TraceMask = Data.iValue;
          lRet = 0;
          break;
        case IOCTL_MUXCFG_GET_TRACEMASK:
          Data.iValue = TraceMask;
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        case IOCTL_MUXCFG_SET_STARTDELAY:
          bp_SetStartDelay(pBasePort, Data.iValue);
          lRet = 0;
          break;
        case IOCTL_MUXCFG_GET_STARTDELAY:
          Data.iValue = bp_GetStartDelay(pBasePort);
          lRet = copy_to_user((void __user *)ioctl_param, (void*)&Data, sizeof(MuxCfgData));
          break;
        default:
          DBGPRINT(ZONE_ERR, "Invalid ioctl passed to mux_fs_ioctl(%x)", ioctl_num);
          break;
      }
    } else {
      DBGPRINT(ZONE_ERR, "Invalid data passed to mux_fs_ioctl(%x)", ioctl_num);
    }
  } else {
    DBGPRINT(ZONE_ERR, "Failed to copy data from user mode");
  }
  DBG_LEAVE(ZONE_FCT_CFG_IFACE, "File=%p, Cmd=%x, Param=%p, Ret=%d", file, ioctl_num, ioctl_param, lRet);
  return lRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The init function of the linmux character device. Here the base port
// objects are allocated and initialized.
//
// Parameters:
// None.
//
// Return:
// 0 in case of success, a negative error value in case of an error.
//
//////////////////////////////////////////////////////////////////////////////
int mux_fs_init(void) {
  int iRet, i;
  DBG_ENTER(ZONE_FCT_CFG_IFACE, "Instances=%d, Ports=%d", gInstances, gPorts);
  iRet = register_chrdev(MUX_FS_MAJOR, MUX_CFG_NAME, &fops);
  if (iRet < 0) {
    DBGPRINT(ZONE_ERR, "Registering char device failed with %d", iRet);
  } else {
    pMuxBasePorts = oswrap_GetMem(sizeof(PT_BASEPORT) * gInstances);
    if (!pMuxBasePorts) {
      DBGPRINT(ZONE_ERR, "Failed to allocate memory");
      iRet = -ENOMEM;
    } else {
      MEMCLR(pMuxBasePorts, sizeof(PT_BASEPORT) * gInstances);
      for (i = 0; i < gInstances; i++) {
        pMuxBasePorts[i] = bp_Create(i);
        if (!pMuxBasePorts[i]) {
          iRet = -ENOMEM;
          break;
        }
      }
    }
  }
  DBG_LEAVE(ZONE_FCT_CFG_IFACE, "Ret=%i", iRet);
  return iRet;
}

//////////////////////////////////////////////////////////////////////////////
//
// The exit function of the linmux character device. Here the allocated base
// port objects destroyed and released.
//
// Parameters:
// None.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void mux_fs_exit(void) {
  int i;
  DBG_ENTER(ZONE_FCT_CFG_IFACE, "");
  if (pMuxBasePorts) {
    for (i = 0; i < gInstances; i++) {
      bp_Destroy(pMuxBasePorts[i]);
    }
    oswrap_FreeMem(pMuxBasePorts);
    pMuxBasePorts = NULL;
  }
  unregister_chrdev(MUX_FS_MAJOR, MUX_CFG_NAME);
  DBG_LEAVE(ZONE_FCT_CFG_IFACE, "");
}

