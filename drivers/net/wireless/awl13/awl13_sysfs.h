/*
 * awl13_sysfs.h
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
 * 2008-07-18    Created by Nissin Systems Co.,Ltd.
 * 2009-06-19   Modified by Atmark Techno, Inc.
 * 2011-11-25   Modified by Atmark Techno, Inc.
 */

#ifndef _AWL13_SYSFS_H_
#define _AWL13_SYSFS_H_

extern int  awl13_sysfs_init(struct awl13_private *priv);
extern void awl13_sysfs_remove(struct awl13_private *priv);

#endif /* _AWL13_SYSFS_H_ */
