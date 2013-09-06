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

#ifndef __HBBTVTERMINAL__
#define __HBBTVTERMINAL__

#define _WIN32_WINNT 0x0510 

#ifdef __cplusplus
extern "C" {
#endif


#include <gpac/configuration.h>
#include <gpac/mpegts.h>
#include <gpac/tools.h>
#include <gpac/events.h>
#include <gpac/options.h>
#include <gpac/terminal.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/term_info.h>
#include <hbbtvbrowserpluginapi.h>

#ifdef __cplusplus
}
#endif

#include <iostream>

#include <webkit/webkit.h>
#include <webkit/WebKitDOMUIEvent.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG  true
#define PROJECTNAME "HbbTVTerminal"
#define TRACEINFO if (DEBUG) { fprintf(stderr, " BOB \x1b[%i;3%im%s\x1b[0m : call %s\n", 1, 4, PROJECTNAME, __FUNCTION__); }
#define NOTIMPLEMENTED if (DEBUG) { printf("SEGAR %s :  %s is NOT IMPLEMENTED : TODO \n", PROJECTNAME, __FUNCTION__); }

#define TRANSPARENCE true

#define HBBTV_VIDEO_WIDTH 1280
#define HBBTV_VIDEO_HEIGHT 720

typedef struct GraphicInterface
{
    WebKitWebView 	*pWebView;
    GtkWidget       *pTVView;
    GtkWidget       *pBackgroundView;
    GtkWidget       *pMainWindow;
    GtkWidget       *pTVWindow;
    GtkWidget       *pWebWindow;
    GtkWidget       *pBackgroundWindow;
    GtkWidget	  	*pEntry;
} sGraphicInterface;


enum listHBBTVKeys{
	HBBTV_VK_ENTER	= 13,
	HBBTV_VK_PAUSE	= 19,
	HBBTV_VK_ESCAPE	= 27,
	HBBTV_VK_LEFT		= 37,
	HBBTV_VK_RIGHT	= 39,
	HBBTV_VK_UP		= 38,
	HBBTV_VK_DOWN		= 40,
	HBBTV_VK_0		= 48,
	HBBTV_VK_1		= 49,
	HBBTV_VK_2		= 50,
	HBBTV_VK_3		= 51,
	HBBTV_VK_4		= 52,
	HBBTV_VK_5		= 53,
	HBBTV_VK_6		= 54,
	HBBTV_VK_7		= 55,
	HBBTV_VK_8		= 56,
	HBBTV_VK_9		= 57,
	HBBTV_VK_RED		= 403,
	HBBTV_VK_YELLOW	= 405,
	HBBTV_VK_GREEN	= 404,
	HBBTV_VK_BLUE		= 406,
	HBBTV_VK_REWIND	= 412,
	HBBTV_VK_STOP		= 413,
	HBBTV_VK_PLAY		= 415,
	HBBTV_VK_FAST_FWD	= 417,	
	HBBTV_VK_TELETEXT = 459
};


enum listRegisteredKeys {
	RK_OTHER =  	 0,
	RK_RED = 		 1,
	RK_GREEN = 		 2,
	RK_YELLOW =	 	 3,
	RK_BLUE = 		 4,
	RK_NAVIGATION =  5,
	RK_VCR =		 6,	
	RK_SCROLL =		 7,
	RK_INFO = 		 8,
	RK_NUMERIC =	 9,
	RK_ALPHA = 		10	
};

typedef struct PlayerInterface
{
    sGraphicInterface* ui;
    Bool no_url;
    char* input_data;
    char* url;
    GF_Terminal *m_term;
    GF_User *m_user;
    void* Demuxer;
    GF_Mutex *Get_demux_info_mutex;
    int TVwake;
    Bool keyregistered[11];
    Bool init;
    Bool is_connected;
    Bool app_in_action;

} sPlayerInterface;

int ui_init(sGraphicInterface *ui);

int init_gpac(sPlayerInterface* player_interf);
int init_browser(sPlayerInterface* player_interf);
int stop_gpac(sPlayerInterface *player_interf);
int playpause(sPlayerInterface *player_interf);
int term_play(sPlayerInterface *player_interf);
int term_pause(sPlayerInterface *player_interf);
int init_player(sPlayerInterface* player_interf);


typedef struct
{
	GF_M2TS_AIT* ait;

	char* data;
	u32 table_id;
	u32 data_size;
} AIT_TO_PROCESS;


typedef struct
{
	GF_M2TS_DSMCC_SECTION* dsmcc_sections;
	char* buff;

	/*added not in the spec*/
	u8 first_section_received;
}GF_M2TS_GATHER_DSMCC_SECTION;

#define MAX_audio_index 16

class Channel
{
public:
    /* Constructor */
    Channel(u32 TSservice_ID, char* TSchannel_name);
    /* Destructor */
    void Destroy_Channel();
    /* Fonctions */
    u32 Add_service_id(u32 service_id);
    u32 Add_channel_name(char* chan_name);
    u32 Add_video_ID(u32 video_index);
    u32 Add_audio_ID(u32 current_audio_index);
    u32 Add_ait_pid(u32 ait_pid);
    u32 Add_pmt_pid(u32 pmt_pid);
    u32 Add_App_info(GF_M2TS_CHANNEL_APPLICATION_INFO*add_ait);
    void Check_Info_Done();
    void Incr_audio_index(int index);
    void Set_audio_index(u32 index);
    /* Getter */
    u32 Get_service_id();
    char* Get_channel_name();
    u32 Get_video_ID();
    u32 Get_audio_ID(u32 indice);
    u32 Get_ait_pid();
    u32 Get_pmt_pid();
    Bool Get_processed();
    u32 Get_audio_index();
    u32 Get_nb_chan_audio_stream();
    GF_M2TS_CHANNEL_APPLICATION_INFO* Get_App_info();
    GF_M2TS_AIT_APPLICATION* App_to_play(Bool isConnected,u8 MaxPriority);
	GF_M2TS_AIT_APPLICATION* Get_App(u32 application_id);

private:
    u32 service_ID;
    u32 number;
    char* channel_name;
    u32 video_ID;
    u32 audio_ID[MAX_audio_index];
    u32 AIT_PID;
    u32 PMT_PID;
    Bool processed;
    u32 current_audio_index;
    u32 nb_chan_audio_stream;
    GF_M2TS_CHANNEL_APPLICATION_INFO* ChannelApp; 

};

class HbbtvDemuxer
{
public:
	/* Constructeur */
	HbbtvDemuxer(char *input_data, char* url,  Bool dsmcc, Bool no_url,sPlayerInterface* player_interf);

	/* Destructeur */

	/* Fonction */
	u32 HbbtvDemuxer_DemuxStart();
	Bool ait_already_received(char *data,u32 pid);
	void GetAppInfoFromAit(GF_M2TS_AIT* ait);
	Channel* Zap_channel(u32 service_id,int zap);
	void Create_Channel(GF_M2TS_Program* pmt);
	void Check_PMT_Processing();
	void Channel_check();
	u32 Check_application_priority(Channel* chan, GF_M2TS_AIT* ait);
	/* Getter */
	GF_M2TS_Demuxer* Get_Ts();	
	GF_List* Get_ChannelList();
	GF_List* Get_AIT_To_Process_list();
	char* Get_Input_data();
	Bool Get_if_dsmcc_process();
	GF_Thread * Get_Demux_Thread();
	GF_Mutex * Get_Demux_Mutex();
	void* Get_User();
	int Check_all_channel_info_received(int Timer);
	char* Get_Force_URL();
	Bool Get_Ignore_TS_URL();
	Bool Get_ait_to_proces();
	Channel* Get_Channel(u32 service_id);
	GF_Err Get_application_info(GF_M2TS_CHANNEL_APPLICATION_INFO*app_info);
	/* Setter */
	void Set_Ts(GF_M2TS_Demuxer* ts);
	void Set_ait_to_process(Bool on);	

private:
	/* Fonction */

	u32 GetDemuxStartFunction();	

	/* Attribut */
	GF_M2TS_Demuxer *Demuxts;
	GF_List* Channels;
	GF_List* Ait_To_Process;

	/* Thread for Demux */
	GF_Thread *ts_demux_thread;
	/* Mutex for Demux */
	GF_Mutex *ts_demux_mutex;

	/*local file playing*/
	char *Input_data;
	char *Service_URL;
	char *Force_URL;
	Bool process_dsmcc;
	Bool Ignore_TS_URL;
	Bool No_URL;
	u32 nb_ait;
	u32 file_size;
	Bool ait_to_process;
	u32 nb_prog_pmt_received;
	Bool all_prog_pmt_received;

	/*callback to push AIT information when a AIT is received*/
	void (*on_ait_event)(void *player_interf, GF_M2TS_CHANNEL_APPLICATION_INFO*app_info);
	/*private user data - To the PlayerInterface*/
	void *user;

};


/* Global Functions */

u32 On_hbbtv_received_section(void *ptr, GF_Event *event);
u32 DemuxThreadStart(HbbtvDemuxer *hbbtv_demuxer);
u32 DemuxStart(void *par);
GF_Err get_app_url(sPlayerInterface* player_interf, GF_M2TS_CHANNEL_APPLICATION_INFO*app_info);
int put_app_url(sPlayerInterface* player_interf);
u32 Get_application_for_channel(HbbtvDemuxer* hbbtv_demuxer,u32 service_id);

int change_geometry( int width, int height);
int hbbtvterm_scan_channel(sPlayerInterface* player_interf);
Channel* ZapChannel(HbbtvDemuxer *hbbtv_demuxer,u32 service_id,int zap);
int hbbtvterm_channel_zap(sPlayerInterface* player_interf,int up_down);
int hbbtvterm_get_channel_on_air(sPlayerInterface* player_interf, u32 service_id, int zap);
void resizevideoplayer(sPlayerInterface* player_interf, int width, int height);

void get_window_position(GtkWidget* Widget, int* x, int* y);
void set_window_position(GtkWidget* Widget, int x, int y);

GF_Config* check_config_file();
Bool is_connected();

/* HBBTV Functions */


/* HBBTV Button */
void on_redbuttonclicked(GtkWidget *widget, gpointer data);
void on_greenbuttonclicked(GtkWidget *widget, gpointer data);
void on_yellowbuttonclicked(GtkWidget *widget, gpointer data);
void on_bluebuttonclicked(GtkWidget *widget, gpointer data);

/*Navigation */
void  on_upbuttonclicked(GtkWidget *widget, gpointer data);
void on_downbuttonclicked(GtkWidget *widget, gpointer data);
void on_leftbuttonclicked(GtkWidget *widget, gpointer data);
void on_rightbuttonclicked(GtkWidget *widget, gpointer data);
void on_returnbuttonclicked(GtkWidget *widget, gpointer data);
void on_exitbuttonclicked(GtkWidget *widget, gpointer data);
void on_2buttonclicked(GtkWidget *widget, gpointer data);

/* Control */
void on_onoffbuttonclicked(GtkWidget *widget, gpointer data);
void on_playpausebuttonclicked(GtkWidget *widget, gpointer data);
void on_playbuttonclicked(GtkWidget *widget, gpointer data);
void on_pausebuttonclicked(GtkWidget *widget, gpointer data);
void on_langbuttonclicked(GtkWidget *widget, gpointer data);
void on_chanupbuttonclicked(GtkWidget *widget, gpointer data);
void on_chandownbuttonclicked(GtkWidget *widget, gpointer data);
void on_channelbuttonclicked(GtkWidget *widget, gpointer data);
void on_teletextbuttonclicked(GtkWidget *widget, gpointer data);



#ifdef __cplusplus
}
#endif

#endif // __HBBTVTERMINAL__
