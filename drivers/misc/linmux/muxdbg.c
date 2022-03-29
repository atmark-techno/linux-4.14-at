/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// muxdbg.c
//
// This file contains the implementation of the linmux trace capabilities.
//
//////////////////////////////////////////////////////////////////////////////

#include "linmux.h"
#include "global.h"

//////////////////////////////////////////////////////////////////////////////

unsigned long TraceMask = DEFAULT_TRACE_MASK;

//////////////////////////////////////////////////////////////////////////////

#if defined(TRACE_ENABLED) && TRACE_ENABLED

//////////////////////////////////////////////////////////////////////////////
//
// Conditional trace output.
//
// Parameters:
// dwDbgZone : The debug zone defining if trace is printed or not.
// pre       : Prefix to be printed.
// func      : The name of the calling function.
// fForceFunc: Flag indicating if the function name should always be printed.
// fThread   : Flag indicating if the name of the current thread shall be
//             printed.
// fmt       : printf like format string.
// ...       : printf like variable argument list.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void DbgPrint(const DWORD dwDbgZone, const char *pre, const char *func, const bool fForceFunc, const bool fThread, const char *fmt, ...) {
  if (TraceMask & dwDbgZone) {
    char strPre[128] = "";
    char strThr[128] = "";
    char strVar[512] = "";
    if (strlen(func)) {
      if (fForceFunc) {
        snprintf(strPre, sizeof(strPre), "%s: %s %s() ", LINMUX_PLATFORM_NAME, pre, func);
      } else if (dwDbgZone & ZONE_ERR) {
        snprintf(strPre, sizeof(strPre), "%s: %s Error in %s() ", LINMUX_PLATFORM_NAME, pre, func);
      } else if (dwDbgZone & ZONE_MUX_EXIT) {
        snprintf(strPre, sizeof(strPre), "%s: %s Exit in %s() ", LINMUX_PLATFORM_NAME, pre, func);
      } else {
        snprintf(strPre, sizeof(strPre), "%s: %s ", LINMUX_PLATFORM_NAME, pre);
      }
    } else {
      snprintf(strPre, sizeof(strPre), "%s: %s ", LINMUX_PLATFORM_NAME, pre);
    }
    if (fThread) {
      snprintf(strThr, sizeof(strThr), " [%s]", current->comm);
    }
    if (strlen(fmt)) {
      char *p;
      va_list args;
      va_start(args, fmt);
      vsnprintf(strVar, sizeof(strVar), fmt, args);
      va_end(args);
      // Don't allow <cr> or <lf> in string
      for (p = strVar; *p != '\0'; p++) {
        if ((*p == '\r') || (*p == '\n')) {
          *p = ' ';
        }
      }
    }
    printk("%s%s%s\n", strPre, strVar, strThr);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Trace raw hex data.
//
// Parameters:
// dwDbgZone: The debug zone defining if trace is printed or not.
// pData    : Data to be traced.
// dwLen    : The number of bytes to be traced.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void TraceHexData(const DWORD dwDbgZone, const BYTE* pData, const DWORD dwLen) {
  if (TraceMask & dwDbgZone) {
    static const char  HexDigits[] = "0123456789ABCDEF";
    static const int   dwCharsPerLine = 16;
    char               szOut[80];
    int                i, j, iHex, iAscii;

    for (i = 0; i < dwLen; i += dwCharsPerLine) {
      MEMSET(szOut, ' ', sizeof(szOut));
      iHex = 0;
      iAscii = (dwCharsPerLine * 3) + 5;
      for (j = 0; (j < dwCharsPerLine) && ((i + j) < dwLen); j++) {
        szOut[iHex++] = HexDigits[pData[i + j] >> 4];
        szOut[iHex++] = HexDigits[pData[i + j] & 0x0f];
        iHex++;
        if ((pData[i + j] >= 32) && (pData[i + j] <= 126)) {
          szOut[iAscii++] = pData[i + j];
        } else {
          szOut[iAscii++] = '.';
        }
      }
      szOut[iAscii] = '\0';
      DBGPRINT(dwDbgZone, "%s", szOut);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Format a CHAR string into a WCHAR string replacing '\r' and '\n' with
// visable chars
//
//
// Parameters:
// dwDbgZone: The debug zone defining if trace is printed or not.
// szIntro: The text to be displayed at the beginning.
// szTrace: The CHAR string to be traced with replaced <cr><lf>
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void TraceCRLFString(const DWORD dwDbgZone, const char *szIntro, const char *szTrace) {
  if (TraceMask & dwDbgZone) {
    const char  szCR[] = "<cr>";
    const char  szLF[] = "<lf>";
    char        szBuf[256];
    UINT        iLenCR = strlen(szCR);
    UINT        iLenLF = strlen(szLF);
    UINT        iPrint;
    UINT        i;
    const UINT  iMaxCharsPerLine = 200;

    if (!szIntro) {
      szBuf[0] = '\0';
    } else {
      strcpy(szBuf, szIntro);
    }
    iPrint = strlen(szBuf);
    for (i = 0; i < strlen(szTrace); i++) {
      // Check if we have to start a new line
      if (iPrint >= iMaxCharsPerLine) {
        DBGPRINT(dwDbgZone, "%s", szBuf);
        szBuf[0] = '\0';
        iPrint = strlen(szBuf);
      }

      // Check for chars to be replaced
      if (szTrace[i] == '\r') {
        strcpy(&szBuf[iPrint], szCR);
        iPrint += iLenCR;
      } else if (szTrace[i] == '\n') {
        strcpy(&szBuf[iPrint], szLF);
        iPrint += iLenLF;
      } else {
        szBuf[iPrint] = szTrace[i];
        iPrint++;
        szBuf[iPrint] = '\0';
      }
    }
    DBGPRINT(dwDbgZone, "%s", szBuf);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Trace content of a termios structure.
//
// Parameters:
// dwDbgZone: The debug zone defining if trace is printed or not.
// pTermios : Pointer to structure to be traced.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void TraceTermios(DWORD dwDbgZone, struct termios *pTermios) {
  if (TraceMask & dwDbgZone) {
    DBGPRINT(dwDbgZone, "Termios %p (octal output):", pTermios);
    DBGPRINT(dwDbgZone, "c_iflag = %o"              , pTermios->c_iflag);
    DBGPRINT(dwDbgZone, "c_oflag = %o"              , pTermios->c_oflag);
    DBGPRINT(dwDbgZone, "c_cflag = %o"              , pTermios->c_cflag);
    DBGPRINT(dwDbgZone, "c_lflag = %o"              , pTermios->c_lflag);
    DBGPRINT(dwDbgZone, "c_line  = %o"              , pTermios->c_line);
    DBGPRINT(dwDbgZone, "c_cc[NCSS]:");
    MUXDBGHEX(dwDbgZone, pTermios->c_cc, NCCS);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Trace content of a serial_struct structure.
//
// Parameters:
// dwDbgZone: The debug zone defining if trace is printed or not.
// pSerInfo : Pointer to structure to be traced.
//
// Return:
// None.
//
//////////////////////////////////////////////////////////////////////////////
void TraceSerialStruct(DWORD dwDbgZone, struct serial_struct *pSerInfo) {
  if (TraceMask & dwDbgZone) {
    DBGPRINT(dwDbgZone, "serial_struct %p:"    , pSerInfo);
    DBGPRINT(dwDbgZone, "type  = %d"           , pSerInfo->type);
    DBGPRINT(dwDbgZone, "line  = %d"           , pSerInfo->line);
    DBGPRINT(dwDbgZone, "port  = %d"           , pSerInfo->port);
    DBGPRINT(dwDbgZone, "irq   = 0x%08x"       , pSerInfo->irq);
    DBGPRINT(dwDbgZone, "flags = 0x%08x"       , pSerInfo->flags);
    DBGPRINT(dwDbgZone, "xmit_fifo_size = %d"  , pSerInfo->xmit_fifo_size);
    DBGPRINT(dwDbgZone, "closing_wait = 0x%08x", pSerInfo->closing_wait);
  }
}

#endif // TRACE_ENABLED

