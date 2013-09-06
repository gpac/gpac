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
 *		Authors:     Jonathan Sillan		
 *				
 */

#include "hbbtvterminal.h"

static u32 New_ait_received(HbbtvDemuxer* hbbtv_demuxer,GF_M2TS_AIT *ait, char  *data, u32 data_size, u32 table_id);
static u32 On_hbbtv_ait_section(sPlayerInterface* player_interf, GF_Event *event);
static u32 On_hbbtv_dsmcc_section(sPlayerInterface* player_interf, GF_Event *event);
static u32 On_hbbtv_get_tsdemuxer(sPlayerInterface* player_interf, GF_Event *event);



/* Constructor */
HbbtvDemuxer::HbbtvDemuxer(char *input_data, char* url, Bool dsmcc, Bool no_url,sPlayerInterface* player_interf)
{

	Demuxts = NULL;

	Channels = gf_list_new();
 	Input_data = gf_strdup(input_data);	
	user = player_interf;
	player_interf->Demuxer = this;
	ait_to_process = 0;
	nb_prog_pmt_received = 0;
	all_prog_pmt_received =0;
	No_URL = 0;
	Ignore_TS_URL = 0;	
	if(dsmcc){
		process_dsmcc = 1;
	}else{
		process_dsmcc = 0;
	}

	if(no_url){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] No URL \n"));
 		No_URL = 1;
 	}else if(url){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Forced URL %s \n",url));
		Force_URL = gf_strdup(url);
		Ignore_TS_URL = 1;
	}
 	nb_ait = 0;
	ts_demux_mutex = gf_mx_new("HBBTV_TS_Demux_Mutex");

	ts_demux_thread = gf_th_new("HBBTV_TS_Demux_Thread");
	
}

/* Destructor */

/* Getter */

GF_M2TS_Demuxer* HbbtvDemuxer::Get_Ts()
{
	return Demuxts;
}

GF_List* HbbtvDemuxer::Get_AIT_To_Process_list()
{
	return Ait_To_Process;
}

char* HbbtvDemuxer::Get_Input_data()
{
	return Input_data;
}

Bool HbbtvDemuxer::Get_if_dsmcc_process()
{
	return process_dsmcc;
}

GF_Thread * HbbtvDemuxer::Get_Demux_Thread()
{
	return ts_demux_thread;
}

GF_Mutex * HbbtvDemuxer::Get_Demux_Mutex()
{
	return ts_demux_mutex;
}

GF_List * HbbtvDemuxer::Get_ChannelList(){
	return Channels;
}


Channel* HbbtvDemuxer::Get_Channel(u32 service_id){
	u32 nb_channel,i;

	nb_channel = gf_list_count(Channels);

	for(i=0;i<nb_channel;i++){
		Channel* Chan = (Channel*)gf_list_get(Channels,i);
		if(Chan->Get_service_id() == service_id){
			return Chan;
		}
	}
	return NULL;
}

void* HbbtvDemuxer::Get_User(){

	return user;	
}


char* HbbtvDemuxer::Get_Force_URL(){
	return Force_URL;
}

Bool HbbtvDemuxer::Get_Ignore_TS_URL(){
	return Ignore_TS_URL;
}

Bool HbbtvDemuxer::Get_ait_to_proces(){
	return ait_to_process;
}


/* Setter */
void HbbtvDemuxer::Set_Ts(GF_M2TS_Demuxer *on_ts){	
	Demuxts = on_ts;
}

void HbbtvDemuxer::Set_ait_to_process(Bool on){
	ait_to_process = on;
}

/* Class Fonction */
GF_Err HbbtvDemuxer::Get_application_info(GF_M2TS_CHANNEL_APPLICATION_INFO*ChannelApp)
{
	if(!Ignore_TS_URL && !No_URL){	
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Get application for service %d \n",ChannelApp->service_id));
		return get_app_url((sPlayerInterface*)user,ChannelApp);	
      }else{
      
	if(!No_URL){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Forced URL %s \n",Force_URL));
	}else{
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] URL blocked by the user. No Application to play  \n"));;
	}
	return GF_OK;
	
      }
}

Channel* HbbtvDemuxer::Zap_channel(u32 service_id,int zap){
	  u32 count_list;
	  u32 i ;
	  
	  count_list = gf_list_count(Channels);
// 	   GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] list: %d\n",count_list));
	  for(i=0;i<count_list;i++){
	      Channel* chan = ( Channel*)gf_list_get(Channels,i);
	      if(chan->Get_service_id() == service_id){
		if(zap != 0){
		  /* zap is use for changing channel. It could be +1 or -1 in order to take the next/previous service */
		  GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] i: %d zap %d\n",i,zap));
		  		    
		   i= (i+zap+count_list)%count_list; /* loop on the channels */
		  
		  GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] i: %d list %d\n",i,count_list));
		}
		Channel* chan = ( Channel*)gf_list_get(Channels,i);		
		return chan;
	      }
	  }
	  
	  /* If the prog goes here that means Channels list is empty or 
	  no object from this current service_id has already been processed */
// 	  GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] add service_id: %d \n",service_id));
	  Channel* chan = new Channel(service_id,NULL);
	  gf_list_add(Channels,chan);
	  return chan;
}

void HbbtvDemuxer::Channel_check(){
	  u32 count_list;
	  u32 i ;
	  
	  count_list = gf_list_count(Channels);
	  for(i=0;i<count_list;i++){
	      Channel* chan = ( Channel*)gf_list_get(Channels,i);
	      chan->Check_Info_Done();
	  }
  }
	      

int HbbtvDemuxer::Check_all_channel_info_received(int Timer){
	
	if(!Demuxts){
	    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTV]No PAT received \n"));
	    return 0;
	}	
	/* Check if all the PMT have been processed or wait until 5 secondes to starts the program */
	if(all_prog_pmt_received || Timer == 5){	  
	    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] nb_prog_pmt_received %d\n",nb_prog_pmt_received));	  
	    return 1;
	}
	return 0;
}

void HbbtvDemuxer::Create_Channel(GF_M2TS_Program *prog){  

	Channel* chan = new Channel(prog->number,NULL);
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal]Service number:%d\n",prog->number));
	chan->Add_pmt_pid(prog->pmt_pid);
	gf_list_add(Channels,chan);
	
}

void HbbtvDemuxer::Check_PMT_Processing(){  	
	nb_prog_pmt_received++;	
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("Demuxts->programs %d\n",gf_list_count(Demuxts->programs)));	
	if(nb_prog_pmt_received == gf_list_count(Demuxts->programs)){
		all_prog_pmt_received = 1;
	}	
}


/* Global Functions */

u32 DemuxThreadStart(HbbtvDemuxer *hbbtv_demuxer){

	return gf_th_run(hbbtv_demuxer->Get_Demux_Thread(),DemuxStart,(void*)hbbtv_demuxer);
}

u32 On_hbbtv_received_section(void *ptr, GF_Event *event){
	u32 e;
	
	e = GF_OK;
	HbbtvDemuxer* hbbtv_demuxer = (HbbtvDemuxer*)ptr;
	sPlayerInterface* player_interf = (sPlayerInterface*)hbbtv_demuxer->Get_User();	
	

	if (event->type == GF_EVENT_FORWARDED)
	{
		if(event->forwarded_event.service_event_type == GF_M2TS_EVT_PAT_FOUND && hbbtv_demuxer->Get_Ts() == NULL){
			e = On_hbbtv_get_tsdemuxer(player_interf,event);			
			if(e != 0){
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Error in receiving the TS Demuxer \n"));

			}
		}
		if(event->forwarded_event.service_event_type == GF_M2TS_EVT_AIT_FOUND){
			e = On_hbbtv_ait_section(player_interf,event);
		}
		if(event->forwarded_event.service_event_type == GF_M2TS_EVT_DSMCC_FOUND){
			e = On_hbbtv_dsmcc_section(player_interf,event);
		}
		if(event->forwarded_event.service_event_type == GF_M2TS_EVT_PMT_FOUND){	
			gf_mx_p(hbbtv_demuxer->Get_Demux_Mutex());
			GF_M2TS_Program * prog = (GF_M2TS_Program*)event->forwarded_event.param;			
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("PMT PID %d \n",prog->pmt_pid));
			hbbtv_demuxer->Create_Channel(prog);	
			hbbtv_demuxer->Check_PMT_Processing();
			gf_mx_v(hbbtv_demuxer->Get_Demux_Mutex());
		}		
	}	
	return e;

  
}
 
static u32 On_hbbtv_ait_section(sPlayerInterface* player_interf, GF_Event *event) 
{
	HbbtvDemuxer* hbbtv_demuxer = (HbbtvDemuxer*)player_interf->Demuxer;
	GF_M2TS_AIT_CARRY* ait_carry;
	
	
	ait_carry = (GF_M2TS_AIT_CARRY*)event->forwarded_event.param;	
	
	/* Make sure we are not modifying the AIT List at the same time */
	gf_mx_p(hbbtv_demuxer->Get_Demux_Mutex());
	if(player_interf->init){		
		Get_application_for_channel(hbbtv_demuxer,ait_carry->service_id);			
	}
		
	/* Unlock the mutex */
	gf_mx_v(hbbtv_demuxer->Get_Demux_Mutex());

	return 0;
}

u32 Get_application_for_channel(HbbtvDemuxer* hbbtv_demuxer,u32 service_id){
	u32 nb_channel;
	Channel* Chan;
	GF_M2TS_CHANNEL_APPLICATION_INFO* ChanAppInfo;
	GF_Err e;

	Chan = hbbtv_demuxer->Get_Channel(service_id);
	if(!Chan){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] service ID %d is not available\n",service_id));
		return GF_BAD_PARAM;
	}	

	ChanAppInfo = gf_m2ts_get_channel_application_info(hbbtv_demuxer->Get_Ts()->ChannelAppList,service_id);	
	
	if(!ChanAppInfo){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] service ID %d no application available\n",service_id));
		return GF_BAD_PARAM;
	}	
	
	Chan->Add_App_info(ChanAppInfo);	
	e = hbbtv_demuxer->Get_application_info(ChanAppInfo);
	if(e == GF_OK){
	  GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Application found for the service ID %d is ON\n",ChanAppInfo->service_id));
	}else{
	  GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] This application for this service ID %d does not belong to the current one.\n\n",ChanAppInfo->service_id));
	}
	
	return GF_OK;
	
} 

static u32 On_hbbtv_dsmcc_section(sPlayerInterface* player_interf, GF_Event *event) 
{
	HbbtvDemuxer* hbbtv_demuxer = (HbbtvDemuxer*)player_interf->Demuxer;
	GF_M2TS_Demuxer* ts = hbbtv_demuxer->Get_Ts();		
	GF_M2TS_DSMCC_OVERLORD* dsmcc_overlord;

	GF_M2TS_SL_PCK* pck = (GF_M2TS_SL_PCK*)event->forwarded_event.param;
	
	dsmcc_overlord = gf_m2ts_get_dmscc_overlord(ts->dsmcc_controler,pck->stream->service_id);

	if (dsmcc_overlord && dsmcc_overlord->get_index){
			GF_M2TS_AIT_APPLICATION* Application;
			Channel* Chan;
			Chan = (Channel*)hbbtv_demuxer->Get_Channel(dsmcc_overlord->service_id);
			//if(!Chan)
			Application = Chan->Get_App(dsmcc_overlord->application_id);	
			Application->url_received = 1;			
			put_app_url(player_interf);
	}

	return 0;
}

static u32 On_hbbtv_get_tsdemuxer(sPlayerInterface* player_interf, GF_Event *event) 
{
	HbbtvDemuxer* hbbtv_demuxer = (HbbtvDemuxer*)player_interf->Demuxer;
	GF_M2TS_Demuxer *ts = (GF_M2TS_Demuxer*)event->forwarded_event.param;
	hbbtv_demuxer->Set_Ts(ts);	
	if(hbbtv_demuxer->Get_Ts() != NULL){
		if(hbbtv_demuxer->Get_if_dsmcc_process() && !ts->process_dmscc){	
			gf_m2ts_demux_dmscc_init(ts);
		}
		return GF_OK;
	}
		
	return GF_BAD_PARAM;
}

u32 DemuxStart(void *par){

	GF_Err e;
	HbbtvDemuxer* hbbtv_demuxer = (HbbtvDemuxer*) par;
	e = TSDemux_Demux_Setup(hbbtv_demuxer->Get_Ts(), hbbtv_demuxer->Get_Input_data(), 0);
	if(e)
	{
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HBBTV] Error during TS demux \n"));
	}
	return e;

}

Channel* ZapChannel(HbbtvDemuxer *hbbtv_demuxer,u32 service_id,int zap){  
	return hbbtv_demuxer->Zap_channel(service_id,zap);
}
