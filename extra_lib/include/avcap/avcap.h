/*
 * (c) 2005, 2008 Nico Pranke <Nico.Pranke@googlemail.com>, Robin Luedtke <RobinLu@gmx.de> 
 *
 * This file is part of avcap.
 *
 * avcap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avcap.  If not, see <http://www.gnu.org/licenses/>.
 */

/* avcap is free for non-commercial use.
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>
 */


#ifndef AVCAP_H
#define AVCAP_H

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif
#include "avcap/DeviceCollector.h"
#include "avcap/DeviceDescriptor.h"
#include "avcap/CaptureDevice.h"
#include "avcap/FormatManager.h"
#include "avcap/CaptureHandler.h"
#include "avcap/CaptureManager.h"
#include "avcap/Control_avcap.h"
#include "avcap/ControlManager.h"
#include "avcap/Connector.h"
#include "avcap/ConnectorManager.h"
#include "avcap/IOBuffer.h"
#include "avcap/Tuner_avcap.h"
#include "avcap/log.h"

#endif
