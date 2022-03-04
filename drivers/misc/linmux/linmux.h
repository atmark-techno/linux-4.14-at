/******************************************************************************

 (c) 2013 Gemalto M2M GmbH <wmSupport@gemalto.com>

 All Rights Reserved.

 Cinterion provides this source code under the GPL v2 License.
 The GPL v2 license is available at

 http://opensource.org/licenses/gpl-license.php

******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
//
// Linmux.h
//
// This file contains global definitions and prototypes for the linmux
// driver.
//
//////////////////////////////////////////////////////////////////////////////


#ifndef __LINMUX_H
#define __LINMUX_H


#include "baseport.h"

//////////////////////////////////////////////////////////////////////////////

#define LINMUX_PLATFORM_NAME     "LinMux"

//////////////////////////////////////////////////////////////////////////////

int IncOpenCount(PT_BASEPORT pBasePort);
void DecOpenCount(PT_BASEPORT pBasePort);
int mux_fs_init(void);
void mux_fs_exit(void);

#endif // __LINMUX_H

