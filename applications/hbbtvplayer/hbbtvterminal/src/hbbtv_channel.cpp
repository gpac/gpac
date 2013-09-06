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
 *		Authors:  Jonathan Sillan		
 *				
 */

#include "hbbtvterminal.h"

static Bool app_priority_test(GF_M2TS_AIT_APPLICATION*,u8 app_priority,u8 app_transport, u8 MaxPriority);

/* Constructor */
Channel::Channel(u32 TSservice_ID, char* TSchannel_name){  
 	
	u8 i;
	service_ID = TSservice_ID;
	if(TSchannel_name){
	  channel_name = gf_strdup(channel_name);
	}else{
	  channel_name = NULL;
	}
	
	video_ID = 0;
	for(i=0 ; i<MAX_audio_index ; i++){
	  audio_ID[i] = 0;  
	}
	ChannelApp = NULL;
	AIT_PID = 0;
	PMT_PID = 0;
	processed = 0;
	current_audio_index = 0;
	nb_chan_audio_stream =0;
}

/* Destructor */
void Channel::Destroy_Channel(){  
 	
	service_ID = 0;
	if(channel_name){
	  gf_free(channel_name);
	}
	gf_free(this);
	
}
/* Getter */
u32 Channel::Get_service_id(){  
	return service_ID; 
}

char* Channel::Get_channel_name(){
	return channel_name;
}

u32 Channel::Get_video_ID(){
	return video_ID; 
}

u32 Channel::Get_audio_ID(u32 indice){
	if(audio_ID[indice] == 0 || indice >= nb_chan_audio_stream){
		indice = 0;
	}
	current_audio_index = indice;
	return audio_ID[indice];
}

u32 Channel::Get_ait_pid(){
	return AIT_PID;
}

u32 Channel::Get_pmt_pid(){
	return PMT_PID;
}

Bool Channel::Get_processed(){
	return processed;
}

u32 Channel::Get_audio_index(){
	return current_audio_index;
}

u32 Channel::Get_nb_chan_audio_stream(){
	return nb_chan_audio_stream;
}

GF_M2TS_CHANNEL_APPLICATION_INFO*Channel::Get_App_info(){
	return ChannelApp;
}

GF_M2TS_AIT_APPLICATION* Channel::App_to_play(Bool isConnected,u8 MaxPriority){
	u32 i;
	Bool App_selected;
	u32 app_index;
	u8 app_priority;
	u8 app_transport;
	
	App_selected = 0;
	app_priority = 0;
	app_index = 0;
	app_transport = 0;
		
	for(i = 0 ; i<ChannelApp->nb_application; i++){ 
		GF_M2TS_AIT_APPLICATION* App_info = (GF_M2TS_AIT_APPLICATION*)gf_list_get(ChannelApp->Application,i);
		if((isConnected && App_info->broadband) || App_info->broadcast){
			if(App_info->application_control_code == AUTOSTART){
				if(app_priority_test(App_info,app_priority,isConnected,MaxPriority)){
					app_priority = App_info->priority;
					app_index = i;
					App_selected = 1;
					if(!app_transport && App_info->broadband && isConnected){
						app_transport = BROADBAND;
					}else{
						app_transport = BROADCAST;
					}
					
				}				
			}
		}		
	}
	if(App_selected){
		GF_M2TS_AIT_APPLICATION* App_info = (GF_M2TS_AIT_APPLICATION*)gf_list_get(ChannelApp->Application,app_index);
		return App_info;
	}
	return NULL;
}

GF_M2TS_AIT_APPLICATION* Channel::Get_App(u32 application_id){
	u32 i;
		
	for(i = 0 ; i<ChannelApp->nb_application; i++){ 
		GF_M2TS_AIT_APPLICATION* App_info = (GF_M2TS_AIT_APPLICATION*)gf_list_get(ChannelApp->Application,i);
		if(App_info->application_id == application_id){
			return App_info;
		}		
	}
	return NULL;
}



/* Class Fonction */

u32 Channel::Add_service_id(u32 service_id){
  if(service_id){
	service_ID = service_id;
	return GF_OK;
  }
  return GF_BAD_PARAM;
}

u32 Channel::Add_channel_name(char* chan_name){
    if(chan_name != NULL){
	channel_name = gf_strdup(chan_name);
	return GF_OK;
    }
    return GF_BAD_PARAM;
}
u32 Channel::Add_video_ID(u32 video_index){
    if(video_index){
	video_ID = video_index;
	return GF_OK;
    }
    return GF_BAD_PARAM;
}

u32 Channel::Add_audio_ID(u32 audio_index){
	if(audio_index){
	u32 i;
	for(i = 0; i<MAX_audio_index;i++){
	  if(audio_ID[i]==0){
		 nb_chan_audio_stream++;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("Add audio ID[%d] %d to channel %d -- total of %d audio stream on this channel\n",i,audio_index,service_ID,nb_chan_audio_stream)); 		
	    audio_ID[i] = audio_index;
	    return GF_OK;
	  }
	} 	
    }
    return GF_BAD_PARAM;
}

u32 Channel::Add_ait_pid(u32 ait_pid){
    if(ait_pid){	
	AIT_PID = ait_pid;
	return GF_OK;
    }
    return GF_BAD_PARAM;
}

u32 Channel::Add_pmt_pid(u32 pmt_pid){
    if(pmt_pid){	
	PMT_PID = pmt_pid;
	return GF_OK;
    }
    return GF_BAD_PARAM;
}

u32 Channel::Add_App_info(GF_M2TS_CHANNEL_APPLICATION_INFO*chan_app){
    if(chan_app){	
		ChannelApp = chan_app;
		return GF_OK;
    }
    return GF_BAD_PARAM;
}


void Channel::Check_Info_Done(){
	processed = 1;
}

void Channel::Set_audio_index(u32 index){
	current_audio_index = index;
}

/* Incr_audio_index
 * 
 * int mode : 1 ou 0
 * 
 * Increment ou decrement the audio stream index
 */
void Channel::Incr_audio_index(int mode){
	if(mode){
		current_audio_index--;
	}else{
		current_audio_index++;
	}
}
	
/* Global Functions */

static Bool app_priority_test(GF_M2TS_AIT_APPLICATION* App_info,u8 app_priority,u8 isConnected,u8 MaxPriority){

	if(!(App_info->priority >= MaxPriority && (MaxPriority >0)) && 
	((app_priority < App_info->priority) || (app_priority == App_info->priority))
	&& ((isConnected && App_info->broadband)||App_info->broadcast)){
		return 1;
	}
	
	return 0;
}
