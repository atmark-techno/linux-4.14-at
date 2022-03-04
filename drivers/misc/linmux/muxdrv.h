/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

#ifndef __MUXDRV_H
#define __MUXDRV_H

// Used AT commands
#define ATCMD_MANDATORY_INIT          { "AT&S1\r", "AT&S0\\Q3\r" }
#define ATCMD_START_MUX               "AT+CMUX=0\r"
#define ATCMD_SHUTDOWN                "AT^SMSO\r"

// The COMM_MASK used in the ReadDataThread()
#define ISALIVE_COMM_MASK             (EV_DSR)

// Timing values
#define TIMEOUT_MODULE_STD            5000
#define TIMEOUT_MODULE_START          30000
#define TIMEOUT_MODULE_STOP           60000
#define TIMEOUT_STATE_CHANGE          30000
#define TIME_MODULE_OFF_MIN           2000

// Array with baud rates to be scanned during baud rate synchronization.
// The first zero is the placeholder for the required baud rate filled
// by the scanning algorithm.
#define BAUDRATES_TO_SCAN             { 0, 115200, 230400, 57600, 38400 }
#define BAUDRATES_RETRIES             2

#endif // __MUXDRV_H
