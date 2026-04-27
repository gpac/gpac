/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Motion Spell 2026
 *					All rights reserved
 *
 *  This file is part of GPAC / Filters sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _GF_SCTE35_DEV_H_
#define _GF_SCTE35_DEV_H_

#include <gpac/tools.h>

/*! SCTE-35 DASH-IF binary scheme URI for emsg / InbandEventStream signaling */
#define GF_SCTE35_SCHEME_URI_INBAND "urn:scte:scte35:2013:bin"

/*! SCTE-35 DASH-IF scheme URI for out-of-band (in the manifest) signaling */
#define GF_SCTE35_SCHEME_URI_OUTBAND "urn:scte:scte35:2014:xml+bin"

#endif /*_GF_SCTE35_DEV_H_*/
