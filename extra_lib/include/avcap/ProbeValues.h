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


#ifndef PROBEVALUES_H
#define PROBEVALUES_H
 
namespace avcap {

//! Standard resolutions to probe, if querying resolutions is not supported by the API/device
static int NumResolutions = 13;
static unsigned int Resolutions[][2] = 
{
	{640, 480},
	{640, 360},
	{352, 288}, 
	{352, 240}, 
	{320, 240}, 
	{176, 144},
	{160, 120},
	{80, 60},
	{720, 480},
	{720, 576},
	{800, 600},
	{1280, 1024},
	{1600, 1200}
};

}

#endif
