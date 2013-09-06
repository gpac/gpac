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
 *
 *		Authors:      Jonathan Sillan		
 *				
 */
#include "hbbtvterminal.h"


GF_Config* check_config_file(){
	Bool firstlaunch;
	
	firstlaunch = 0;
	/*init config and modules*/
	return gf_cfg_init(NULL,&firstlaunch);
}

Bool is_connected(){	
	GF_Err e;
	
	GF_Socket* sock;	 
	
	sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (sock == NULL) {			
		return 0;
	}		
	e = gf_sk_connect(sock, "www.google.fr", 4096, NULL);
	gf_sk_del(sock); 
	
	if(e){
		return 0;
	}else{
		return 1;
	}
}


