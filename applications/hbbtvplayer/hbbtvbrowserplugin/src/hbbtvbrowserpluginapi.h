/*
 *		Copyright (c) 2010-2011 Telecom-Paristech
 *                 All Rights Reserved
 *	GPAC is free software; you can redistribute it and/or modify
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
 *		Authors:    Stanislas Selle		
 *				
 */
#ifndef hbbtvbrowserpluginapi_h
#define hbbtvbrowserpluginapi_h

#include <stdlib.h>

///Callbacks
void OnNoFullscreenSetWindow(int x, int y, int width, int height);
void OnAPPLICATION_Show();
void OnAPPLICATION_Hide();
void OnVIDBRC_SetFullScreen(int fullscreenparam);
void OnKEYSET_SetValue(double param);
void OnKEYSET_Allocate();
#endif
