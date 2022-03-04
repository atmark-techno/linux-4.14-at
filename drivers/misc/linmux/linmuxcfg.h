/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Linmuxcfg.h
//
// This file contains the definition of the linmux configuration interface.
// For easy configuration via a command line tool using ioctls the linmux
// driver exports a character device interface. This interface is defined
// here. The header is used by the linmux driver and the configuration
// utility as well. For a detailed description of the different
// configuration ioctls and their usage refer to the help text of the linmux
// configuration program in "../config/linmuxcfg.c".
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __LINMUXCFG_H
#define __LINMUXCFG_H

//////////////////////////////////////////////////////////////////////////////

#define MUX_TTY0_MAJOR                   TTY_DEVNUM
#define MUX_FS_MAJOR                     FS_DEVNUM
#define MUX_CFG_NAME                     FS_DEVNAME

#define MAX_INITCMD_NUM                  4
#define MAX_PORTNAME_LEN                 32
#define MAX_INTCMD_LEN                   256

#define MAX_CFG_STR_LEN                  MAX_INTCMD_LEN

#define DEFAULT_BASEPORT_NAME            "/dev/ttyS"
#define DEFAULT_TTYPORT_NAME             "ttyMux"

#define DEFAULT_NUM_OF_INSTANCES         1
#define DEFAULT_NUM_OF_PORTS             3
#define DEFAULT_BAUD_RATE                115200
#define DEFAULT_HDLC_WINDOW_SIZE         4
#define DEFAULT_HDLC_FRAME_SIZE          (0x4000 - 8)
#define DEFAULT_START_DELAY              9000

//////////////////////////////////////////////////////////////////////////////

#define IOCTL_MUXCFG_RELOAD              _IOW(MUX_FS_MAJOR,    0, pMuxCfgData)
#define IOCTL_MUXCFG_SET_BAUDRATE        _IOW(MUX_FS_MAJOR,    1, pMuxCfgData)
#define IOCTL_MUXCFG_GET_BAUDRATE        _IOWR(MUX_FS_MAJOR,   2, pMuxCfgData)
#define IOCTL_MUXCFG_SET_BASEPORT        _IOW(MUX_FS_MAJOR,    3, pMuxCfgData)
#define IOCTL_MUXCFG_GET_BASEPORT        _IOWR(MUX_FS_MAJOR,   4, pMuxCfgData)
#define IOCTL_MUXCFG_SET_CHNNUM          _IOW(MUX_FS_MAJOR,    5, pMuxCfgData)
#define IOCTL_MUXCFG_GET_CHNNUM          _IOWR(MUX_FS_MAJOR,   6, pMuxCfgData)
#define IOCTL_MUXCFG_SET_DEVNAME         _IOW(MUX_FS_MAJOR,    7, pMuxCfgData)
#define IOCTL_MUXCFG_GET_DEVNAME         _IOWR(MUX_FS_MAJOR,   8, pMuxCfgData)
#define IOCTL_MUXCFG_SET_INITCMD         _IOW(MUX_FS_MAJOR,    9, pMuxCfgData)
#define IOCTL_MUXCFG_GET_INITCMD         _IOWR(MUX_FS_MAJOR,  10, pMuxCfgData)
#define IOCTL_MUXCFG_SET_DEINITCMD       _IOW(MUX_FS_MAJOR,   11, pMuxCfgData)
#define IOCTL_MUXCFG_GET_DEINITCMD       _IOWR(MUX_FS_MAJOR,  12, pMuxCfgData)
#define IOCTL_MUXCFG_SET_MAXMUXVER       _IOW(MUX_FS_MAJOR,   13, pMuxCfgData)
#define IOCTL_MUXCFG_GET_MAXMUXVER       _IOWR(MUX_FS_MAJOR,  14, pMuxCfgData)
#define IOCTL_MUXCFG_SET_HDLCWINSIZE     _IOW(MUX_FS_MAJOR,   15, pMuxCfgData)
#define IOCTL_MUXCFG_GET_HDLCWINSIZE     _IOWR(MUX_FS_MAJOR,  16, pMuxCfgData)
#define IOCTL_MUXCFG_SET_HDLCFRAMESIZE   _IOW(MUX_FS_MAJOR,   17, pMuxCfgData)
#define IOCTL_MUXCFG_GET_HDLCFRAMESIZE   _IOWR(MUX_FS_MAJOR,  18, pMuxCfgData)
#define IOCTL_MUXCFG_SET_INIT            _IOW(MUX_FS_MAJOR,   19, pMuxCfgData)
#define IOCTL_MUXCFG_GET_INIT            _IOWR(MUX_FS_MAJOR,  20, pMuxCfgData)
#define IOCTL_MUXCFG_SET_SWITCHOFF       _IOW(MUX_FS_MAJOR,   21, pMuxCfgData)
#define IOCTL_MUXCFG_GET_SWITCHOFF       _IOWR(MUX_FS_MAJOR,  22, pMuxCfgData)
#define IOCTL_MUXCFG_SET_TRACEMASK       _IOW(MUX_FS_MAJOR,   23, pMuxCfgData)
#define IOCTL_MUXCFG_GET_TRACEMASK       _IOWR(MUX_FS_MAJOR,  24, pMuxCfgData)
#define IOCTL_MUXCFG_SET_STARTDELAY      _IOW(MUX_FS_MAJOR,   25, pMuxCfgData)
#define IOCTL_MUXCFG_GET_STARTDELAY      _IOWR(MUX_FS_MAJOR,  26, pMuxCfgData)

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  int   iInstance;
  int   iIndex;
  int   iValue;
  char  strBuf[MAX_CFG_STR_LEN];
} MuxCfgData, *pMuxCfgData;

//////////////////////////////////////////////////////////////////////////////

extern unsigned long gInstances;
extern unsigned long gPorts;

//////////////////////////////////////////////////////////////////////////////

#endif // __LINMUXCFG_H

