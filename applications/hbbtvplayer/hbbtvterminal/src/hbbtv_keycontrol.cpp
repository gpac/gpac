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
 *		Authors: Stanislas Selle - Jonathan Sillan		
 *				
 */
#include "hbbtvterminal.h"



void gtksendkey(sPlayerInterface* player_interf,int keycode);

void gtksendkey(sPlayerInterface* player_interf,int keycode)
{
	TRACEINFO;
	GdkEvent *KeyEvent;
	
	fprintf(stderr, "\x1b[%i;3%imKeyCode\x1b[0m: %d\n",1, 4,keycode);
	
	KeyEvent = gdk_event_new (GDK_KEY_PRESS);
	/* Key Value */
	KeyEvent->key.keyval = gdk_unicode_to_keyval(keycode);	
	/* GDK_BUTTON1_MASK refers to left mouse button */
	KeyEvent->key.state = GDK_BUTTON1_MASK;
	/* Send the key event to Web Window */
	
	KeyEvent->key.window = player_interf->ui->pWebWindow->window;
	gtk_main_do_event (KeyEvent);
	//gdk_event_free(KeyEvent);
}

/* HBBTV Button */

void on_redbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	fprintf(stderr, "\x1b[%i;3%imBOUTON PRESSED\n\x1b[0m", 1, 5);
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	if (player_interf->keyregistered[RK_RED]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("RED is registered by the application : send HBBTV_VK_RED to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_RED);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("RED is not registered by the application\n"));
	}
	
}

void on_greenbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;
	if (player_interf->keyregistered[RK_GREEN]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("GREEN is registered by the application : send HBBTV_VK_GREEN to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_GREEN);	
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("GREEN is not registered by the application\n"));
	}
}

void on_yellowbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;
	if (player_interf->keyregistered[RK_YELLOW]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("YELLOW is registered by the application : send HBBTV_VK_YELLOW to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_YELLOW);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("YELLOW is not registered by the application\n"));
	}	
}

void on_bluebuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;
	if (player_interf->keyregistered[RK_BLUE]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("BLUE is registered by the application : send HBBTV_VK_BLUE to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_BLUE);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("BLUE is not registered by the application\n"));
	}	
}

/* Navigation */

void on_returnbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	if (player_interf->keyregistered[RK_NAVIGATION]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is registered by the application : send HBBTV_VK_ENTER to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_ENTER);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is not registered by the application\n"));
	}	
}

void on_exitbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	int posx,posy;
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	//~ gtk_window_set_transient_for(GTK_WINDOW(player_interf->ui->pWebWindow),GTK_WINDOW(player_interf->ui->pTVWindow));
	get_window_position(player_interf->ui->pWebWindow, &posx, &posy);	
	resizevideoplayer(player_interf, HBBTV_VIDEO_WIDTH, HBBTV_VIDEO_HEIGHT);	
	set_window_position(player_interf->ui->pTVWindow, posx, posy);
	gtk_window_set_position(GTK_WINDOW(player_interf->ui->pTVWindow),GTK_WIN_POS_CENTER_ALWAYS);
	webkit_web_view_load_uri(player_interf->ui->pWebView, player_interf->url);
}

void on_upbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	if (player_interf->keyregistered[RK_NAVIGATION]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is registered by the application : send HBBTV_VK_UP to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_UP);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is not registered by the application\n"));
	}
}

void on_downbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	if (player_interf->keyregistered[RK_NAVIGATION]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is registered by the application : send HBBTV_VK_DOWN to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_DOWN);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is not registered by the application\n"));
	}	
}

void on_leftbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	if (player_interf->keyregistered[RK_NAVIGATION]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is registered by the application : send HBBTV_VK_LEFT to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_LEFT);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE,("NAVIGATION is not registered by the application\n"));
	}
}

void on_rightbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;	
	if (player_interf->keyregistered[RK_NAVIGATION]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is registered by the application : send HBBTV_VK_RIGHT to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_RIGHT);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NAVIGATION is not registered by the application\n"));
	}	
}



/* Control */

void on_playpausebuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
 	sPlayerInterface* player_interf = (sPlayerInterface*)data; 	
	set_window_position(player_interf->ui->pTVWindow, 0, 0);
}

void on_playbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf = (sPlayerInterface*)data;
	if (player_interf->keyregistered[RK_VCR]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("VCR is registered by the application : send HBBTV_VK_PLAY to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_PLAY);
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("VCR is not registered by the application\n"));
	}	
	
}

void on_pausebuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf = (sPlayerInterface*)data;
	if (player_interf->keyregistered[RK_VCR]){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("VCR is registered by the application : send HBBTV_VK_PAUSE to the Application\n"));
		gtksendkey(player_interf,HBBTV_VK_PAUSE); 
	}else{
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("VCR is not registered by the application\n"));
	}	
}

void on_onoffbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf;
	player_interf = (sPlayerInterface*)data;

	if (player_interf->TVwake == FALSE) {
		init_gpac(player_interf);
		player_interf->TVwake = TRUE;
	}
	else {
		stop_gpac(player_interf);
		player_interf->TVwake = FALSE;
	}
	TRACEINFO;
}

void on_langbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf;
	u32 current_service_id;
	HbbtvDemuxer *hbbtvdemuxer;
	GF_MediaInfo odi;
	u32 index_audio,audio_ID;	
	
	player_interf = (sPlayerInterface*)data;
	hbbtvdemuxer = ( HbbtvDemuxer *)player_interf->Demuxer;
	current_service_id = index_audio = audio_ID = 0;	
	
	/* Get the current channel struct */
	current_service_id = gf_term_get_current_service_id(player_interf->m_term);
	Channel* chan = (Channel*)ZapChannel(hbbtvdemuxer,current_service_id,0);	
	if(chan->Get_nb_chan_audio_stream() > 1){
		GF_ObjectManager *root_odm = gf_term_get_root_object(player_interf->m_term);
		if (root_odm){
			if (gf_term_get_object_info(player_interf->m_term, root_odm, &odi) == GF_OK){ 
				if (odi.od) {
					/* Increment the audio index to get the next audio stream */
					chan->Incr_audio_index(0);					
					gf_term_select_object(player_interf->m_term, gf_term_get_object(player_interf->m_term, root_odm, chan->Get_audio_ID(chan->Get_audio_index())));
				}
			}
		}
	}
}

void on_teletextbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf;
	player_interf = (sPlayerInterface*)data;
	gtksendkey(player_interf,HBBTV_VK_TELETEXT);

}

void on_channelbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf;
	GtkButton* button;
	int key_code;
	char* label;
	player_interf = (sPlayerInterface*)data;
	button = (GtkButton*) widget;
	label = (char*)gtk_button_get_label(button);
		
	switch(atoi(label)){
		case 0:
			key_code = HBBTV_VK_0;
			break;
		case 1:
			key_code = HBBTV_VK_1;
			break;
		case 2:
			key_code = HBBTV_VK_2;
			break;
		case 3:
			key_code = HBBTV_VK_3;
			break;
		case 4:
			key_code = HBBTV_VK_4;
			break;
		case 5:
			key_code = HBBTV_VK_5;
			break;
		case 6:
			key_code = HBBTV_VK_6;
			break;
		case 7:
			key_code = HBBTV_VK_7;
			break;
		case 8:
			key_code = HBBTV_VK_8;
			break;
		case 9:
			key_code = HBBTV_VK_9;
			break;
		default:
			 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Wrong Key Code %d Defaut channel 1 \n",atoi(label)));
			key_code = HBBTV_VK_1;
			break;
		}
	if (player_interf->keyregistered[RK_NUMERIC]){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NUMERIC is registered by the application : send HBBTV_VK_%i to the Application\n",atoi(label)));
		gtksendkey(player_interf,key_code);
	}else{
		hbbtvterm_get_channel_on_air(player_interf,atoi(label),0);
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("NUMERIC is not registered by the application, change channel function\n"));
	}				
	
	
}


void on_chanupbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf;
	player_interf = (sPlayerInterface*)data;

	hbbtvterm_channel_zap(player_interf,1);

	TRACEINFO;
}


void on_chandownbuttonclicked(GtkWidget *widget, gpointer data)
{
	TRACEINFO;
	sPlayerInterface* player_interf;
	player_interf = (sPlayerInterface*)data;

	hbbtvterm_channel_zap(player_interf,-1);

	TRACEINFO;
}

int playpause(sPlayerInterface* player_interf)
{
	int  is_pause = gf_term_get_option( player_interf->m_term, GF_OPT_PLAY_STATE);
	fprintf(stdout, "[Status: %s]\n", is_pause ? "Playing" : "Paused");
	gf_term_set_option( player_interf->m_term, GF_OPT_PLAY_STATE, is_pause ? GF_STATE_PLAYING : GF_STATE_PAUSED);
	return 1;
}

