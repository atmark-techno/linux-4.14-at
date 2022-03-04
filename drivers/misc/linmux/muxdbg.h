/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#ifndef __MUX_MUXDBG_H
#define __MUX_MUXDBG_H

#include "global.h"
#include "os_wrap.h"

//////////////////////////////////////////////////////////////////////////////
// Trace macros and prototypes
//////////////////////////////////////////////////////////////////////////////
#if defined(TRACE_ENABLED) && TRACE_ENABLED
  #pragma GCC diagnostic ignored "-Wformat"

  void DbgPrint(const DWORD dwDbgZone, const char *pre, const char *func, const bool fForceFunc, const bool fThread, const char *fmt, ...);
  void TraceHexData(const DWORD dwDbgZone, const BYTE* pData, const DWORD dwLen);
  void TraceCRLFString(const DWORD dwDbgZone, const char *szIntro, const char *szTrace);
  void TraceTermios(DWORD dwDbgZone, struct termios *pTermios);
  void TraceSerialStruct(DWORD dwDbgZone, struct serial_struct *pSerInfo);

  #define TEXT(s)                     s

  #define MUX_EXIT(...)               MUXDBG(ZONE_MUX_EXIT, __VA_ARGS__)
  #define MUXDBG(a, ...)              DbgPrint(a, ""   , ""      , false, false, __VA_ARGS__)
  #define DBGPRINT(a, ...)            DbgPrint(a, ""   , __func__, false, false, __VA_ARGS__)
  #define DBG_ENTER(a, ...)           DbgPrint(a, "-> ", __func__, true , true , __VA_ARGS__)
  #define DBG_LEAVE(a, ...)           DbgPrint(a, "<- ", __func__, true , true , __VA_ARGS__)
  #define MUXDBGHEX(a, Data, Len)     TraceHexData(a, Data, Len)
  #define MUXDBGCRLF(a, T1, T2)       TraceCRLFString(a, T1, T2)
  #define MUXDBGTERMIOS(a, p)         TraceTermios(a, p)
  #define MUXDBGSERSTRUCT(a, p)       TraceSerialStruct(a, p)
  #define STRBOOL(b)                  (b ? "true" : "false")
  #define STRPTR(p)                   (p ? p : "<NULL>")
#else // TRACE_ENABLED
  #define MUX_EXIT(f, ...)
  #define MUXDBG(a, ...)
  #define DBGPRINT(a, ...)
  #define DBG_ENTER(a, ...)
  #define DBG_LEAVE(a, ...)
  #define MUXDBGHEX(a, Data, Len)
  #define MUXDBGCRLF(a, T1, T2)
  #define MUXDBGTERMIOS(a, p)
  #define MUXDBGSERSTRUCT(a, p)
  #define STRBOOL(b)
  #define STRPTR(p)
#endif // TRACE_ENABLED


//////////////////////////////////////////////////////////////////////////////
// debug zones
//////////////////////////////////////////////////////////////////////////////
#define ZONE_RESERVED                 0x00000001
#define ZONE_ERR                      0x00000002
#define ZONE_MODULE_EXIT              0x00000004
#define ZONE_DRV_INIT                 0x00000008

#define ZONE_GENERAL_INFO             0x00000010
#define ZONE_AT_CMDS                  0x00000020
#define ZONE_FCT_CFG_IFACE            0x00000040
#define ZONE_FCT_POWER_MGM            0x00000080

#define ZONE_FCT_BASEPORT             0x00000100
#define ZONE_FCT_MUXCHAN              0x00000200
#define ZONE_MUX_VIRT_SIGNAL          0x00000400
#define ZONE_FCT_TTY_IFACE            0x00000800

#define ZONE_MUX_FRAME_ERROR          0x00001000
#define ZONE_MUX_PROT1                0x00002000
#define ZONE_MUX_PROT2                0x00004000
#define ZONE_MUX_EXIT                 0x00008000

#define ZONE_MUX_PROT_HDLC_ERROR      0x00010000
#define ZONE_MUX_PROT_HDLC            0x00020000
#define ZONE_MUX_PROT_HDLC_FRM        0x00040000

#define ZONE_FCT_OSWRAP_THREAD        0x00100000
#define ZONE_FCT_OSWRAP_WQUEUE        0x00200000
#define ZONE_FCT_OSWRAP_TTY           0x00400000
#define ZONE_FCT_OSWRAP_UTILITY       0x00800000
#define ZONE_FCT_OSWRAP_SYNC          0x01000000
#define ZONE_FCT_OSWRAP_CRITSECT      0x02000000

#define ZONE_RAW_DATA                 0x80000000

//////////////////////////////////////////////////////////////////////////////
// default trace mask
//////////////////////////////////////////////////////////////////////////////
#if defined(CLIENT_MODE) && CLIENT_MODE
#define DEFAULT_TRACE_MASK            (ZONE_DRV_INIT | ZONE_ERR | ZONE_MUX_EXIT)
#else
#define DEFAULT_TRACE_MASK            (ZONE_DRV_INIT)
#endif

extern unsigned long TraceMask;

#endif // __MUX_MUXDBG_H

