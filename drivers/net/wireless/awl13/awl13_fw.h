/*
 * awl13_fw.h
 *
 *  Copyright (C) 2008 Nissin Systems Co.,Ltd.
 *  Copyright (C) 2008-2011 Atmark Techno, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 2008-04-02    Created by Nissin Systems Co.,Ltd.
 * 2009-06-19   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 */

#ifndef _AWL13_FW_H_
#define _AWL13_FW_H_

struct awl13_firmware_info {
	uint32_t id;			/* 0x00: identification */
#define AWL13_FW_ID (0x57464C57)	/* "WLFW" */
	uint32_t size;			/* 0x04: size (excl. T_FWINF) */
	uint32_t copyaddr;		/* 0x08: copy address */
	uint32_t entryaddr;		/* 0x0C: entry address */
	uint32_t checksum;		/* 0x10: check sum value */
	uint32_t enckey;		/* 0x14: encryption key */
	uint32_t version;		/* 0x18: version */
	uint32_t reserved1;		/* 0x1C: reserved */
};

#ifndef CONFIG_ARMADILLO_WLAN_AWL13_USB
extern int awl13_firmware_load(struct sdio_func *func);
#else /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
extern int awl13_firmware_load(struct awl13_private *priv);
#endif /* CONFIG_ARMADILLO_WLAN_AWL13_USB */
extern int awl13_firmware_setup(struct awl13_private *priv);
extern int awl13_firmware_setup_without_load(struct awl13_private *priv);


#endif /* _AWL13_FW_H_ */
