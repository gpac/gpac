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
#ifdef XP_UNIX
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#else
#include <gdk/gdk.h>
#include <gdk/gdkwin32.h> 
#endif

#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSContextRef.h>

//static XID handler;
static void activate_uri_entry_cb (GtkWidget* entry, gpointer data);

sPlayerInterface* player_interf;

static gboolean on_deleteevent( GtkWidget *widget, GdkEvent  *event, gpointer   data)
{
	g_print ("delete event occurred\n");
	TRACEINFO;
	return TRUE;
}

static gboolean on_windowconfigure(GtkWidget *	widget, GdkEvent  *event, gpointer data)
{
	TRACEINFO;

	static int oldx = 0;
	static int oldy = 0;
	int x, y, olddatax, olddatay;
	
	sPlayerInterface* player_interf = (sPlayerInterface*)data;

	gtk_window_get_position(GTK_WINDOW(player_interf->ui->pWebWindow), &olddatax, &olddatay);

	x = event->configure.x;
	y = event->configure.y;

	int deltax = x - oldx;
	int deltay = y - oldy;

	gtk_window_move(GTK_WINDOW(player_interf->ui->pTVWindow),olddatax + deltax, olddatay + deltay);
	gtk_window_move(GTK_WINDOW(player_interf->ui->pBackgroundWindow),olddatax + deltax, olddatay + deltay);

	oldx = x;
	oldy = y;
	return TRUE;
}


static void on_destroyevent(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
	TRACEINFO;
}


int ui_init(sPlayerInterface* player_interf)
{
	GtkWidget   *pWindow;
	GtkWidget   *pTVWindow;
	GtkWidget   *pWebWindow;
	GtkWidget   *pMainPositionTable;
	GtkWidget   *pBrowserPositionTable;
	GtkWidget   *pRCUPositionTable;
	GtkWidget   *pColorButtonTable;
	GtkWidget   *pRedButton;
	GtkWidget   *pGreenButton;
	GtkWidget   *pYellowButton;
	GtkWidget   *pBlueButton;
	GtkWidget   *pArrowButtonTable;
	GtkWidget   *pUpButton;
	GtkWidget   *pDownButton;
	GtkWidget   *pLeftButton;
	GtkWidget   *pRightButton;
	GtkWidget   *pUpArrow;
	GtkWidget   *pDownArrow;
	GtkWidget   *pLeftArrow;
	GtkWidget   *pRightArrow;
	GtkWidget   *pEnterButton;
	GtkWidget   *pBackButton;
	GtkWidget   *pEntry;
	GtkWidget   *pNumPadTable;
	GtkWidget   *pNumber[10];
	GtkWidget   *pPlayButton;
	GtkWidget   *pStopButton;
	GtkWidget   *pPauseButton;
	GtkWidget   *pPlayPauseButton;
	GtkWidget   *pFastMoveTable;
	GtkWidget   *pFastForwardButton;
	GtkWidget   *pFastRewindButton;
	GtkWidget   *pLangButton;
	GtkWidget   *pExitButton;
	GtkWidget   *pOnOffButton;
	GtkWidget   *pTeletextButton;
	GtkWidget   *pProgramSelectionTable;
	GtkWidget   *pPreviousProgramButton;
	GtkWidget   *pNextProgramButton;
	GtkWidget   *pQuitButton;
	GtkWidget   *pTestButton;
	GtkWidget   *pTest2Button;
	GtkWidget   *pGoButton;
	GtkWidget   *pWebScrollWindow;
	GtkWidget   *pBackgroundWindow;

	GdkColor color;
	int Error,posx,posy;

	TRACEINFO;

	sGraphicInterface* ui = player_interf->ui;

	/** Main window **/
	pBackgroundWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	pTVWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	pWebWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	pWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	ui->pMainWindow = pWindow;
	ui->pTVWindow = pTVWindow;
	ui->pWebWindow = pWebWindow;
	ui->pBackgroundWindow = pBackgroundWindow;
	 
	gtk_window_set_title(GTK_WINDOW(pWindow), "RCU");
	gtk_window_set_title(GTK_WINDOW(pTVWindow), "TV");
	gtk_window_set_title(GTK_WINDOW(pWebWindow), "WEB");
	gtk_window_set_title(GTK_WINDOW(pBackgroundWindow), "BACK");

	gtk_window_set_decorated(GTK_WINDOW(pWebWindow), false);
	gtk_window_set_decorated(GTK_WINDOW(pTVWindow), false);
	gtk_window_set_decorated(GTK_WINDOW(pBackgroundWindow), false);
	gtk_window_set_transient_for(GTK_WINDOW(pTVWindow),GTK_WINDOW(pBackgroundWindow));
	gtk_window_set_transient_for(GTK_WINDOW(pWebWindow),GTK_WINDOW(pTVWindow));
	
	gtk_window_set_resizable(GTK_WINDOW(pTVWindow), false);
	gtk_window_set_resizable(GTK_WINDOW(pWebWindow), false);
	gtk_window_set_resizable(GTK_WINDOW(pBackgroundWindow), false);
	
	/** Quit button **/
	pQuitButton =  gtk_button_new_with_label("Quit");

	/** Tests button **/
	pTestButton =  gtk_button_new_with_label("Test");
	pTest2Button =  gtk_button_new_with_label("Test2");

	/** Entry bar for URL **/
	pEntry = gtk_entry_new();
	ui->pEntry = pEntry;

	/** GO Button **/
	pGoButton =  gtk_button_new_with_label("Go");

	/** Browser Zone **/
	pWebScrollWindow = gtk_scrolled_window_new(NULL, NULL);
	//gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pWebScrollWindow), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);

	ui->pWebView = WEBKIT_WEB_VIEW(webkit_web_view_new());
	
	webkit_web_view_set_transparent(ui->pWebView, TRUE);

	if (ui->pWebView)
		gtk_container_add(GTK_CONTAINER(pWebScrollWindow), GTK_WIDGET(ui->pWebView));

	/** Background Zone **/
	ui->pBackgroundView = gtk_drawing_area_new();
	gdk_color_parse("#000044", &color);
 	gtk_widget_modify_bg ( GTK_WIDGET(ui->pBackgroundView), GTK_STATE_NORMAL, &color);

	/** TV zone **/
	ui->pTVView = gtk_drawing_area_new();

	/** RCU zone **/

	/* create Red color */
	gdk_color_parse("#FF0000", &color);
	pRedButton = gtk_button_new();
 	gtk_widget_modify_bg ( GTK_WIDGET(pRedButton), GTK_STATE_NORMAL, &color);
	gtk_button_set_label(GTK_BUTTON(pRedButton)," ");
	/* create Green color */
	gdk_color_parse("#00FF00", &color);
	pGreenButton = gtk_button_new();
 	gtk_widget_modify_bg ( GTK_WIDGET(pGreenButton), GTK_STATE_NORMAL, &color);
	gtk_button_set_label(GTK_BUTTON(pGreenButton)," ");
	/* create Yellow color */
	gdk_color_parse("#FFFF00", &color);
	pYellowButton = gtk_button_new();
 	gtk_widget_modify_bg ( GTK_WIDGET(pYellowButton), GTK_STATE_NORMAL, &color);
	gtk_button_set_label(GTK_BUTTON(pYellowButton)," ");
	/* create Blue color */
	gdk_color_parse("#0000FF", &color);
	pBlueButton = gtk_button_new();
 	gtk_widget_modify_bg ( GTK_WIDGET(pBlueButton), GTK_STATE_NORMAL, &color);
	gtk_button_set_label(GTK_BUTTON(pBlueButton)," ");

	pUpButton       = gtk_button_new();
	pDownButton     = gtk_button_new();
	pLeftButton     = gtk_button_new();
	pRightButton    = gtk_button_new();

	pUpArrow        = gtk_arrow_new(GTK_ARROW_UP,GTK_SHADOW_OUT);
	pDownArrow      = gtk_arrow_new(GTK_ARROW_DOWN,GTK_SHADOW_OUT);
	pLeftArrow      = gtk_arrow_new(GTK_ARROW_LEFT,GTK_SHADOW_OUT);
	pRightArrow     = gtk_arrow_new(GTK_ARROW_RIGHT,GTK_SHADOW_OUT);

	gtk_container_add(GTK_CONTAINER(pUpButton), pUpArrow);
	gtk_container_add(GTK_CONTAINER(pDownButton), pDownArrow);
	gtk_container_add(GTK_CONTAINER(pLeftButton), pLeftArrow);
	gtk_container_add(GTK_CONTAINER(pRightButton), pRightArrow);

	pEnterButton    = gtk_button_new_with_label("Enter");
	pBackButton     = gtk_button_new_with_label("Back");

	pNumber[0] = gtk_button_new_with_label("0");
	pNumber[1] = gtk_button_new_with_label("1");
	pNumber[2] = gtk_button_new_with_label("2");
	pNumber[3] = gtk_button_new_with_label("3");
	pNumber[4] = gtk_button_new_with_label("4");
	pNumber[5] = gtk_button_new_with_label("5");
	pNumber[6] = gtk_button_new_with_label("6");
	pNumber[7] = gtk_button_new_with_label("7");
	pNumber[8] = gtk_button_new_with_label("8");
	pNumber[9] = gtk_button_new_with_label("9");

	pPreviousProgramButton  = gtk_button_new_with_label("P-");
	pNextProgramButton      = gtk_button_new_with_label("P+");

	pPlayButton         = gtk_button_new_with_label("Play");
	pStopButton         = gtk_button_new_with_label("Stop");
	pPauseButton        = gtk_button_new_with_label("Pause");
	pPlayPauseButton        = gtk_button_new_with_label("PlayPause");

	pFastForwardButton      = gtk_button_new_with_label(">>");
	pFastRewindButton       = gtk_button_new_with_label("<<");
	pTeletextButton         = gtk_button_new_with_label("Teletext");
	pLangButton          = gtk_button_new_with_label("Language");
	pExitButton         = gtk_button_new_with_label("Exit");
	pOnOffButton        = gtk_button_new_with_label("OnOff");

	pColorButtonTable = gtk_table_new(4,1,false);
	gtk_table_attach(GTK_TABLE(pColorButtonTable),pRedButton,    0,1,0,1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);
	gtk_table_attach(GTK_TABLE(pColorButtonTable),pGreenButton,  0,1,1,2, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);
	gtk_table_attach(GTK_TABLE(pColorButtonTable),pYellowButton, 0,1,2,3, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);
	gtk_table_attach(GTK_TABLE(pColorButtonTable),pBlueButton,   0,1,3,4, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);

	pArrowButtonTable = gtk_table_new(3,3, false);
	gtk_table_attach(GTK_TABLE(pArrowButtonTable), pUpButton,    1,2,0,1,GtkAttachOptions(GTK_FILL|GTK_EXPAND), GtkAttachOptions(GTK_FILL|GTK_EXPAND), 0,0 );
	gtk_table_attach(GTK_TABLE(pArrowButtonTable), pDownButton,  1,2,2,3,GtkAttachOptions(GTK_FILL|GTK_EXPAND), GtkAttachOptions(GTK_FILL|GTK_EXPAND), 0,0 );
	gtk_table_attach(GTK_TABLE(pArrowButtonTable), pLeftButton,  0,1,1,2,GtkAttachOptions(GTK_FILL|GTK_EXPAND), GtkAttachOptions(GTK_FILL|GTK_EXPAND), 0,0 );
	gtk_table_attach(GTK_TABLE(pArrowButtonTable), pRightButton, 2,3,1,2,GtkAttachOptions(GTK_FILL|GTK_EXPAND), GtkAttachOptions(GTK_FILL|GTK_EXPAND), 0,0 );

	pNumPadTable = gtk_table_new(4,3,false);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[0], 1, 2, 3, 4, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[1], 0, 1, 0, 1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[2], 1, 2, 0, 1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[3], 2, 3, 0, 1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[4], 0, 1, 1, 2, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[5], 1, 2, 1, 2, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[6], 2, 3, 1, 2, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[7], 0, 1, 2, 3, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[8], 1, 2, 2, 3, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pNumPadTable), pNumber[9], 2, 3, 2, 3, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);

	pFastMoveTable = gtk_table_new(1,2,false);
	gtk_table_attach(GTK_TABLE(pFastMoveTable), pFastRewindButton,  0,1,0,1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);
	gtk_table_attach(GTK_TABLE(pFastMoveTable), pFastForwardButton, 1,2,0,1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);

	pProgramSelectionTable = gtk_table_new(1,2, false);
	gtk_table_attach(GTK_TABLE(pProgramSelectionTable), pPreviousProgramButton, 0,1,0,1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);
	gtk_table_attach(GTK_TABLE(pProgramSelectionTable), pNextProgramButton,     1,2,0,1, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL),0,0);

	/** create the toplevel tables **/
	pRCUPositionTable = gtk_table_new(24,1,false);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pColorButtonTable,       0, 1,  0,  4, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pArrowButtonTable,       0, 1,  4,  7, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pEnterButton,            0, 1,  7,  8, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pBackButton,             0, 1,  8,  9, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pNumPadTable,            0, 1,  9, 13, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pPlayButton,             0, 1, 13, 14, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pStopButton,             0, 1, 14, 15, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pPauseButton,            0, 1, 15, 16, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pPlayPauseButton,        0, 1, 16, 17, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pFastMoveTable,          0, 1, 17, 18, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pProgramSelectionTable,  0, 1, 18, 19, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pTeletextButton,         0, 1, 19, 20, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pLangButton,              0, 1, 20, 21, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pExitButton,             0, 1, 21, 22, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pOnOffButton,            0, 1, 22, 23, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);
	gtk_table_attach(GTK_TABLE(pRCUPositionTable), pQuitButton,             0, 1, 23, 24, GtkAttachOptions(GTK_FILL|GTK_EXPAND),GtkAttachOptions(GTK_FILL|GTK_EXPAND),0,0);

	pBrowserPositionTable = gtk_table_new(3,3,false);
	//gtk_table_attach(GTK_TABLE(pBrowserPositionTable), pEntry,          0, 2, 0, 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0,0);
	//gtk_table_attach(GTK_TABLE(pBrowserPositionTable), pGoButton,       2, 3, 0, 1, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GTK_FILL, 0,0);
	gtk_table_attach(GTK_TABLE(pBrowserPositionTable), pWebScrollWindow, 0, 3, 1, 3, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0,0);

	/** Group toplevel tables in the main table **/
	pMainPositionTable =  gtk_table_new(4,7,false);
	gtk_table_attach(GTK_TABLE(pMainPositionTable), pRCUPositionTable,      6, 7, 0, 3, GtkAttachOptions(NULL), GtkAttachOptions(NULL), 0,0);
	//gtk_table_attach(GTK_TABLE(pMainPositionTable), pBottomBarTable,        0, 7, 3, 4, GtkAttachOptions(GTK_EXPAND | GTK_FILL), GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0,0);

	gtk_container_add(GTK_CONTAINER(pWindow), pMainPositionTable);
	gtk_container_add(GTK_CONTAINER(pTVWindow), ui->pTVView);
	gtk_container_add(GTK_CONTAINER(pBackgroundWindow), ui->pBackgroundView);
	//~ gtk_container_add(GTK_CONTAINER(pWebWindow), ui->pWebView);
	gtk_container_add(GTK_CONTAINER(pWebWindow), pBrowserPositionTable);

	/** SET SIZE **/
	gtk_widget_set_size_request(GTK_WIDGET(ui->pTVView),  HBBTV_VIDEO_WIDTH,HBBTV_VIDEO_HEIGHT);
	gtk_widget_set_size_request(GTK_WIDGET(ui->pBackgroundView),  HBBTV_VIDEO_WIDTH,HBBTV_VIDEO_HEIGHT);
	gtk_widget_set_size_request(GTK_WIDGET(pBrowserPositionTable), HBBTV_VIDEO_WIDTH,HBBTV_VIDEO_HEIGHT);
	
	gtk_window_set_position(GTK_WINDOW(pWebWindow), GTK_WIN_POS_CENTER_ALWAYS);	
	gtk_window_set_position(GTK_WINDOW(pBackgroundWindow), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_position(GTK_WINDOW(pTVWindow),  GTK_WIN_POS_CENTER_ALWAYS);

	/** Connect Callback **/
	//** expose drawable

	// connect to deleteevent function
	g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(on_deleteevent), NULL);

	// connect to destroy function
	g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK(on_destroyevent), NULL);
	g_signal_connect(G_OBJECT(pWebWindow), "destroy", G_CALLBACK(on_destroyevent), NULL);
	g_signal_connect(G_OBJECT(pQuitButton), "clicked", G_CALLBACK(on_destroyevent), NULL);

	//connect button and entry
	/* Control */
	g_signal_connect (G_OBJECT(pEntry), "activate", G_CALLBACK (activate_uri_entry_cb), ui);
	g_signal_connect (G_OBJECT(pGoButton), "clicked", G_CALLBACK (activate_uri_entry_cb), ui);
	g_signal_connect (G_OBJECT(pOnOffButton), "clicked", G_CALLBACK (on_onoffbuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pPlayPauseButton), "clicked", G_CALLBACK (on_playpausebuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pPlayButton), "clicked", G_CALLBACK (on_playbuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pPauseButton), "clicked", G_CALLBACK (on_pausebuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pLangButton), "clicked", G_CALLBACK (on_langbuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pNextProgramButton), "clicked", G_CALLBACK (on_chanupbuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pPreviousProgramButton), "clicked", G_CALLBACK (on_chandownbuttonclicked), player_interf );
	g_signal_connect (G_OBJECT(pNumber[1]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[2]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[3]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[4]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[5]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[6]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[7]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[8]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[9]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pNumber[0]), "clicked", G_CALLBACK (on_channelbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pTeletextButton), "clicked", G_CALLBACK (on_teletextbuttonclicked), player_interf);
	
	
	/* HBBTV Button */
	g_signal_connect (G_OBJECT(pRedButton), 	"clicked", G_CALLBACK (on_redbuttonclicked), 	player_interf);
	g_signal_connect (G_OBJECT(pGreenButton), 	"clicked", G_CALLBACK (on_greenbuttonclicked), 	player_interf);
	g_signal_connect (G_OBJECT(pYellowButton), 	"clicked", G_CALLBACK (on_yellowbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pBlueButton), 	"clicked", G_CALLBACK (on_bluebuttonclicked), 	player_interf);
	
	/* Navigation */
	g_signal_connect (G_OBJECT(pUpButton), 	"clicked", G_CALLBACK (on_upbuttonclicked), 	player_interf);
	g_signal_connect (G_OBJECT(pDownButton), 	"clicked", G_CALLBACK (on_downbuttonclicked), 	player_interf);
	g_signal_connect (G_OBJECT(pLeftButton), 	"clicked", G_CALLBACK (on_leftbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pRightButton), 	"clicked", G_CALLBACK (on_rightbuttonclicked), 	player_interf);
	g_signal_connect (G_OBJECT(pEnterButton), 	"clicked", G_CALLBACK (on_returnbuttonclicked), player_interf);
	g_signal_connect (G_OBJECT(pExitButton), 	"clicked", G_CALLBACK (on_exitbuttonclicked), 	player_interf);
	

	//	gtk_widget_add_events(GTK_WIDGET(pWebWindow), GDK_CONFIGURE);
	// g_signal_connect(G_OBJECT(pTVWindow), "configure-event", G_CALLBACK(on_windowconfigure), pBackgroundWindow);
	// g_signal_connect(G_OBJECT(pWebWindow), "configure-event", G_CALLBACK(on_windowconfigure), player_interf);

	/** realisation, map  and show **/
	gtk_widget_realize(ui->pBackgroundView);
	gtk_widget_realize(ui->pTVView);

	Error = init_gpac(player_interf);
	if(Error){
		 GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Error during gpac intialization \n"));
		 return GF_IO_ERR;
	}

 	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(ui->pWebView));
	GdkColormap *rgba = gdk_screen_get_rgba_colormap (screen);

	gtk_widget_set_colormap(GTK_WIDGET(ui->pWebView), rgba);
	gtk_widget_set_colormap(GTK_WIDGET(pBrowserPositionTable), rgba);

	/** center the windows **/
	
	gtk_widget_set_default_colormap(rgba);
	gtk_widget_show_all(pWindow);
	gtk_widget_show_all(pBackgroundWindow);
	gtk_widget_show_all(pTVWindow);
	gtk_widget_show_all(pWebWindow);
	gtk_window_present (GTK_WINDOW(pWebWindow));

	return GF_OK;
}


int init_playerinterface(sPlayerInterface* player_interf, char* input_data, char* url, Bool no_url)
{

	HbbtvDemuxer *hbbtv_demuxer;
	int Error;
	int ChannelScanTimer;
	u32 i,nb_chan;

	hbbtv_demuxer = ( HbbtvDemuxer *)player_interf->Demuxer;
	ChannelScanTimer = 0;

	player_interf->ui = (sGraphicInterface*) gf_calloc(1,sizeof(sGraphicInterface));

	
	player_interf->input_data = (char*)gf_calloc(strlen(input_data)+1,sizeof(char));
	sprintf(player_interf->input_data,"%s",input_data);
	player_interf->input_data[strlen(input_data)] = 0;

	player_interf->url = url;
	player_interf->no_url = no_url;
	player_interf->TVwake = FALSE;
	player_interf->is_connected = is_connected();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("player_interf->is_connected %d\n",player_interf->is_connected));


	Error = ui_init(player_interf);
	if(Error){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Initialization error \n"));
		return GF_IO_ERR;
	}

	if(player_interf->url) {
		init_browser(player_interf);
	}

	while(!hbbtv_demuxer->Check_all_channel_info_received(ChannelScanTimer)) {
		ChannelScanTimer++;
		gf_sleep(1000);
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Wainting for the PMTs \n"));					
	}
	
	hbbtvterm_scan_channel(player_interf);

	/* The check for init is made inside this function. AIT received have to be processed before getting new ones */
	
	nb_chan = gf_list_count(hbbtv_demuxer->Get_ChannelList());

	for(i=0;i<nb_chan;i++){		
	      Channel* chan = ( Channel*)gf_list_get(hbbtv_demuxer->Get_ChannelList(),i);	      
	      Get_application_for_channel(hbbtv_demuxer,chan->Get_service_id());		
	}
	player_interf->init = 1;	
	
	put_app_url(player_interf);	  
	
	

	return GF_OK;

}


int init_gpac(sPlayerInterface* player_interf)
{
	TRACEINFO;	
	char config_path[GF_MAX_PATH];
	const char *str;
	const char *gpac_cfg;
	sGraphicInterface *ui = player_interf->ui;
	

#ifdef XP_UNIX
	gpac_cfg = (char *) ".gpacrc";
	strcpy((char *) config_path, getenv("HOME"));
#else
	gpac_cfg = "GPAC.cfg";
	strcpy((char*)config_path,"D:\\code\\trunk\\gpac_public\\bin\\win32\\debug");
#endif

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Configuration path : %s/%s \n",config_path,gpac_cfg));

	player_interf->m_user = (GF_User*)gf_malloc(sizeof(GF_User));
	memset(player_interf->m_user, 0, sizeof(GF_User));
	//~ player_interf->m_user->config = gf_cfg_new((const char *) config_path, gpac_cfg);
				
	/*init config and modules*/
	player_interf->m_user->config = check_config_file();
	if(!player_interf->m_user->config){
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Configuration failed. COnfiguration file not found \n"));
			return GF_BAD_PARAM;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Configuration initialized \n"));

	str = gf_cfg_get_key( player_interf->m_user->config, "General", "ModulesDirectory");
	player_interf->m_user->modules = gf_modules_new(str,  player_interf->m_user->config);

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] %d modules loaded \n",gf_modules_get_count( player_interf->m_user->modules)));

	player_interf->m_user->opaque =  player_interf->Demuxer;
	player_interf->m_user->EventProc = On_hbbtv_received_section;

#ifdef XP_UNIX
	player_interf->m_user->os_window_handler = (void*)GDK_WINDOW_XID(gtk_widget_get_window(ui->pTVView));
	player_interf->m_user->os_display = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(ui->pTVView));
	XSynchronize((Display *)  player_interf->m_user->os_display, True);
#else
	player_interf->m_user->os_window_handler = (void*)GDK_WINDOW_HWND(gtk_widget_get_window(ui->pTVView));
	player_interf->m_user->os_display = gdk_drawable_get_display (gtk_widget_get_window(ui->pTVView));
	gdk_display_sync((GdkDisplay *)player_interf->m_user->os_display);
#endif

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] User initialized \n"));

	if (( player_interf->m_term = gf_term_new(player_interf->m_user))) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Terminal created.\n"));
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Error when creating terminal\n"));
		return GF_IO_ERR;
	}

	gf_term_set_size( player_interf->m_term, HBBTV_VIDEO_WIDTH,HBBTV_VIDEO_HEIGHT);


	if (strnicmp(player_interf->input_data, "udp://", 6)
		&& strnicmp(player_interf->input_data, "mpegts-udp://", 13)
		&& strnicmp(player_interf->input_data, "mpegts-tcp://", 13)
		&& strnicmp(player_interf->input_data, "dvb://", 6)
	&& strnicmp(player_interf->input_data, "http://", 6)) {

		FILE *test = fopen(player_interf->input_data, "rb");
		if (!test) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] %s  file cannot be oppened \n",player_interf->input_data));
			return GF_BAD_PARAM;
		}
		else {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] %s file oppened successfully \n", player_interf->input_data));
			fclose(test);
		}

	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Input URL : %s\n", player_interf->input_data));
	gf_term_connect( player_interf->m_term, (const char*)player_interf->input_data );
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Terminal connected\n"));
	
	return GF_OK;
}


int stop_gpac(sPlayerInterface* player_interf)
{
	TRACEINFO;
	gf_term_disconnect( player_interf->m_term);
	gf_term_del( player_interf->m_term);
	gf_modules_del( player_interf->m_user->modules);
	gf_cfg_del( player_interf->m_user->config);

	return TRUE;
}


static void activate_uri_entry_cb (GtkWidget* entry, gpointer data)
{
	TRACEINFO;
	sGraphicInterface *pUI;

	pUI = (sGraphicInterface*)data;
	const gchar* uri = gtk_entry_get_text (GTK_ENTRY(pUI->pEntry));
	g_assert (uri);
	webkit_web_view_load_uri (pUI->pWebView, uri);
}

int term_play(sPlayerInterface* player_interf)
{
	gf_term_set_option(player_interf->m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
	return 1;
}


int term_pause(sPlayerInterface* player_interf)
{
	gf_term_set_option(player_interf->m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
	return 1;
}


int init_browser(sPlayerInterface* player_interf)
{
	if(!player_interf->no_url) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Initialized the browser with HBBTV Application URL : %s   \n",player_interf->url));
  		webkit_web_view_load_uri(player_interf->ui->pWebView, player_interf->url);
		return GF_OK;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] URL blocked by the user. No Application to play  \n"));
	return GF_OK;
}


int init_player(sPlayerInterface* player_interf)
{
	/* Init the player with the fisrt channel of the stream */

	HbbtvDemuxer* hbbtvdemuxer = (HbbtvDemuxer*)player_interf->Demuxer;
	Channel* chan = (Channel*)gf_list_get(hbbtvdemuxer->Get_ChannelList(),0);
	hbbtvterm_get_channel_on_air(player_interf,chan->Get_service_id(),0);

	return GF_OK;
}

GF_Err get_app_url(sPlayerInterface* player_interf, GF_M2TS_CHANNEL_APPLICATION_INFO*ChannelApp)
{
	u32 current_service_id;	

	current_service_id = player_interf->m_term->root_scene->selected_service_id;
	
	if(!ChannelApp){
		return GF_IO_ERR;
	}
	
	if((ChannelApp->service_id == current_service_id)) {
		GF_M2TS_AIT_APPLICATION* app_info;
		Channel* Chan;	
		
		HbbtvDemuxer* hbbtvdemuxer = (HbbtvDemuxer*)player_interf->Demuxer;
		Chan = (Channel*)ZapChannel(hbbtvdemuxer,current_service_id,0);
				
		app_info = Chan->App_to_play(player_interf->is_connected,0);	

		if(!app_info){
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] This service id %d does not have an autostart application. \n",ChannelApp->service_id,current_service_id));
			return GF_IO_ERR;
		}		
	
		if(player_interf->init){
			if(app_info->broadband){
				player_interf->url = app_info->http_url;
			}else if(app_info->broadcast){
				player_interf->url = app_info->carousel_url;
			} 		
  			webkit_web_view_load_uri(player_interf->ui->pWebView, player_interf->url);  				
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] HBBTV Application URL : %s   \n",player_interf->url));			
			return GF_OK;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Cannot start the application until player initialization is finished \n"));
		return GF_IO_ERR;
	}	
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("ChannelApp->service_id %d is wrong current_service_id %d !!\n",ChannelApp->service_id,current_service_id));
	return GF_IO_ERR;


}

int put_app_url(sPlayerInterface* player_interf)
{
	u32 current_service_id;
	Bool ignore_TS_url;
	GF_Err e;
	HbbtvDemuxer* hbbtvdemux = (HbbtvDemuxer*)player_interf->Demuxer;

	ignore_TS_url = hbbtvdemux->Get_Ignore_TS_URL();
	e = GF_OK;
	
	if(!ignore_TS_url && !player_interf->no_url){
		
		current_service_id = player_interf->m_term->root_scene->selected_service_id;
		Channel* chan = (Channel*)ZapChannel(hbbtvdemux,current_service_id,0);		
		e = get_app_url(player_interf,chan->Get_App_info());
		if(e == GF_OK) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Application started\n"));
				return GF_OK;
		}		    
	    player_interf->url = "";
	    webkit_web_view_load_uri(player_interf->ui->pWebView, player_interf->url);
	    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal]No Application found for this service ID %d \n",current_service_id));
	    return GF_IO_ERR;

	}else{

	    if(!player_interf->no_url){
		    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Forced URL %s  Reaload Application \n",player_interf->url));		   
		     webkit_web_view_load_uri(player_interf->ui->pWebView, player_interf->url);
	    }else{
		    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] URL blocked by the user. \n"));		    
	    }
	    return GF_OK;
	}
}


int hbbtvterm_scan_channel(sPlayerInterface* player_interf)
{

	GF_MediaInfo odi;
	u32 i, count;
	
	HbbtvDemuxer* HbbtvDemux = (HbbtvDemuxer*)player_interf->Demuxer;

	GF_ObjectManager *root_odm = gf_term_get_root_object(player_interf->m_term);
	if (!root_odm){		
		 return GF_IO_ERR;
	 }

	if (gf_term_get_object_info(player_interf->m_term, root_odm, &odi) != GF_OK) return GF_IO_ERR;
	if (!odi.od) {	
		return GF_IO_ERR;
	}
	count = gf_term_get_object_count(player_interf->m_term, root_odm);	
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Count: %d \n",count));

	for (i=0; i<count; i++) {		
		GF_ObjectManager *odm;
		odm = gf_term_get_object(player_interf->m_term, root_odm, i);
		if (!odm) break;
		if (gf_term_get_object_info(player_interf->m_term, odm, &odi) == GF_OK) {
			u32 service_id = odi.od->ServiceID;
			Channel* chan = (Channel*)ZapChannel(HbbtvDemux,service_id,0);
			
			if (odi.od_type==GF_STREAM_VISUAL && !chan->Get_processed()) {				
				chan->Add_video_ID(i);
 				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Service: %d chan->video_index : %d \n",service_id,chan->Get_video_ID()));
			}else if(odi.od_type==GF_STREAM_AUDIO && !chan->Get_processed()) {				
				chan->Add_audio_ID(i);
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal]Service:%d chan->current_audio_index : %d \n",service_id,chan->Get_audio_ID(0)));;
 			}
 			
		}
	}
	HbbtvDemux->Channel_check();

	return GF_OK;

}


int hbbtvterm_channel_zap(sPlayerInterface* player_interf,int up_down)
{

	u32 current_service_id;

	current_service_id = gf_term_get_current_service_id(player_interf->m_term);
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Current_service_id : %d \n",current_service_id));
	hbbtvterm_get_channel_on_air(player_interf,current_service_id,up_down);

	return GF_OK;

}


int hbbtvterm_get_channel_on_air(sPlayerInterface* player_interf, u32 service_id, int zap)
{

	GF_MediaInfo odi;

	HbbtvDemuxer* hbbtvdemuxer = (HbbtvDemuxer*)player_interf->Demuxer;
	Channel* chan = (Channel*)ZapChannel(hbbtvdemuxer,service_id,zap);
	GF_ObjectManager *root_odm = gf_term_get_root_object(player_interf->m_term);
	if (!root_odm) return GF_IO_ERR;

	if (gf_term_get_object_info(player_interf->m_term, root_odm, &odi) != GF_OK) return GF_IO_ERR;
	if (!odi.od) {
		return GF_IO_ERR;
	}

 	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal]service_id:%d video pid : %d audio pid:%d\n",chan->Get_service_id(),chan->Get_video_ID(),chan->Get_audio_ID(0)));
 	 	
 	gf_term_select_object(player_interf->m_term, gf_term_get_object(player_interf->m_term, root_odm, chan->Get_video_ID()));
	gf_term_select_object(player_interf->m_term, gf_term_get_object(player_interf->m_term, root_odm, chan->Get_audio_ID(0)));	
		
	put_app_url(player_interf);

	return GF_OK;

}

/** On Windows Functions **/

void resizevideoplayer(sPlayerInterface* player_interf, int width, int height)
{
	TRACEINFO;
	gf_term_set_size(player_interf->m_term,width,height);
	gtk_widget_set_size_request(GTK_WIDGET(player_interf->ui->pTVWindow),width,height);

}

void get_window_position(GtkWidget* Widget, int* x, int* y)
{
	//TRACEINFO;
	gtk_window_get_position(GTK_WINDOW(Widget), &(*x), &(*y));
}

void set_window_position(GtkWidget* Widget, int x, int y)
{
	//~ //TRACEINFO;
	gtk_window_move(GTK_WINDOW(Widget), x, y);
}


void OnAPPLICATION_Show()
{
	TRACEINFO;
	gtk_window_set_opacity(GTK_WINDOW(player_interf->ui->pWebWindow),1);
}

void OnAPPLICATION_Hide()
{
	TRACEINFO;
	gtk_window_set_opacity(GTK_WINDOW(player_interf->ui->pWebWindow),0);
}


void OnVIBRC_SetChannel(int channel_number){
	//~ hbbtvterm_get_channel_on_air(player_interf, service_id, 0);
}

void OnKEYSET_SetValue(double param)
{
	TRACEINFO;
	fprintf(stderr, "\t param transmitted : %i\n", (int)param);
	
	int 	KEYMASK_RED 		= 0x1;
	int 	KEYMASK_GREEN 		= 0x2;
	int 	KEYMASK_YELLOW 		= 0x4;
	int 	KEYMASK_BLUE 		= 0x8;
	int 	KEYMASK_NAVIGATION 	= 0x10;
	int 	KEYMASK_VCR 		= 0x20;
	int 	KEYMASK_SCROLL 		= 0x40;
	int 	KEYMASK_INFO 		= 0x80;
	int 	KEYMASK_NUMERIC 	= 0x100;
	int 	KEYMASK_ALPHA 		= 0x200;
	int 	KEYMASK_OTHER 		= 0x400;
	
	player_interf->keyregistered[RK_RED]        = (((int)param & KEYMASK_RED))	      ? true : false;
	player_interf->keyregistered[RK_GREEN]      = (((int)param & KEYMASK_GREEN))	  ? true : false;
	player_interf->keyregistered[RK_YELLOW]     = (((int)param & KEYMASK_YELLOW))	  ? true : false;
	player_interf->keyregistered[RK_BLUE]       = (((int)param & KEYMASK_BLUE))		  ? true : false;
	player_interf->keyregistered[RK_NAVIGATION] = (((int)param & KEYMASK_NAVIGATION)) ? true : false;
	player_interf->keyregistered[RK_VCR] 		= (((int)param & KEYMASK_VCR))		  ? true : false;
	player_interf->keyregistered[RK_SCROLL] 	= (((int)param & KEYMASK_SCROLL))	  ? true : false;
	player_interf->keyregistered[RK_INFO] 		= (((int)param & KEYMASK_INFO))		  ? true : false;
	player_interf->keyregistered[RK_NUMERIC] 	= (((int)param & KEYMASK_NUMERIC))	  ? true : false;
	player_interf->keyregistered[RK_ALPHA] 		= (((int)param & KEYMASK_ALPHA))      ? true : false;
	player_interf->keyregistered[RK_OTHER] 		= (((int)param & KEYMASK_OTHER))	  ? true : false;	
}

void OnNoFullscreenSetWindow(int x, int y, int width, int height)
{
	TRACEINFO;
	OnVIDBRC_SetFullScreen(false);
}
void OnVIDBRC_SetFullScreen(int fullscreenparam)
{
	TRACEINFO;
	fprintf(stderr,"SET FULLSCREEN OnVIDBRC_SetFullScreen Param : %d\n",fullscreenparam);
	
	///Getting the videobroadcast element.
	WebKitDOMDocument *pDOMdoc  = webkit_web_view_get_dom_document(player_interf->ui->pWebView);
	WebKitDOMNodeList *objectslist = webkit_dom_document_get_elements_by_tag_name(pDOMdoc,"object");
	gulong l = webkit_dom_node_list_get_length(objectslist);
	gulong i = 0;
	bool videofound = false;
	WebKitDOMNode* videonode;
	char *videonodetype;	
	while ((i < l) && !videofound)
	{
			videonode = webkit_dom_node_list_item(objectslist,i);
			videonodetype = webkit_dom_element_get_attribute(WEBKIT_DOM_ELEMENT(videonode),"type");
			if (!(strcmp(videonodetype, "video/broadcast")))
				videofound = true;
			else
				i++;
	}
	
	if (!videofound)
	{
		fprintf(stderr,"object video broadcast not found \n");
		return;
	}
	else
	{		
		fprintf(stderr,"object video broadcast found \n");
		WebKitDOMElement* videoelt = WEBKIT_DOM_ELEMENT(videonode);
		
		///Getting the position of the WebView
		gint posx, posy;
		get_window_position(player_interf->ui->pWebWindow, &posx, &posy);
		fprintf(stderr,"WebWindow Left : %d ,  WebWindow Top : %d \n", posx, posy);

		///Getting the videobroadcast geometry
		glong left, top;
		glong eltwidth, eltheight ;
		///Setting the new size and position of the TVWindow broadcast
		int newx;
		int newy;

		if (fullscreenparam){
			left = 0;
			top = 0;
			eltwidth = HBBTV_VIDEO_WIDTH;
			eltheight = HBBTV_VIDEO_HEIGHT;
			get_window_position(player_interf->ui->pWebWindow, &newx, &newy);			
		}else{
			gtk_window_set_position(GTK_WINDOW(player_interf->ui->pTVWindow), GTK_WIN_POS_NONE);
			///Getting the top and left values of the video/broadcast element  .
			left = webkit_dom_element_get_offset_left(videoelt);
			top = webkit_dom_element_get_offset_top(videoelt);
			///Getting the width and height values of the video/broadcast element  .
			eltwidth = webkit_dom_element_get_offset_width(videoelt);
			eltheight = webkit_dom_element_get_offset_height(videoelt);
			newx = posx + left;
			newy = posy + top;		
		}		

		fprintf(stderr,"Videoelt offset : Left : %d ,  Top : %d, ", left, top);
		fprintf(stderr,"EltWidth : %d ,  EltHeight : %d \n", eltwidth, eltheight);		
		set_window_position(player_interf->ui->pTVWindow, newx, newy);
		fprintf(stderr,"Supposed TVWindow new position : %d x %d \n", newx, newy);				
		resizevideoplayer(player_interf, eltwidth, eltheight);	
		if(fullscreenparam){
			gtk_window_set_position(GTK_WINDOW(player_interf->ui->pTVWindow), GTK_WIN_POS_CENTER_ALWAYS);
		}
		fprintf(stderr,"Checking position : \n ");			
		gint checkx, checky;
		get_window_position(player_interf->ui->pTVWindow, &checkx, &checky);
		fprintf(stderr,"CHECK TVWindow Left : %d , CHECK TVWindow Top : %d \n", checkx, checky);
	}
		 		
}

void OnKEYSET_Allocate()
{
	webkit_web_view_execute_script(player_interf->ui->pWebView, 
	"   var KeyEvent = new Object(); \
		KeyEvent.VK_RED = 403; \
		KeyEvent.VK_YELLOW = 405;\
		KeyEvent.VK_GREEN = 404;\
		KeyEvent.VK_BLUE = 406;\
		KeyEvent.VK_UP = 38;\
		KeyEvent.VK_DOWN = 40;\
		KeyEvent.VK_LEFT = 37;\
		KeyEvent.VK_RIGHT = 39;\
		KeyEvent.VK_PLAY = 415;\
		KeyEvent.VK_STOP	= 413;\
		KeyEvent.VK_PAUSE = 19;\
		KeyEvent.VK_FAST_FWD = 417;\
		KeyEvent.VK_REWIND = 412;\
		KeyEvent.VK_TELETEXT = 459;\
		KeyEvent.VK_ESCAPE = 27;\
		KeyEvent.VK_ENTER = 13;\
		KeyEvent.VK_0 = 48;\
		KeyEvent.VK_1 = 49;\
		KeyEvent.VK_2 = 50;\
		KeyEvent.VK_3 = 51;\
		KeyEvent.VK_4 = 52;\
		KeyEvent.VK_5 = 53;\
		KeyEvent.VK_6 = 54;\
		KeyEvent.VK_7 = 55;\
		KeyEvent.VK_8 = 56;\
		KeyEvent.VK_9 = 57;		");
}


static void usage()
{
	printf("\nUsage: hbbtvterminal -input=input_data [-url=url] [-no_url]\n");
	printf("-input=input_data : input data to process (dvb://, udp://, or file)\n");
	printf("-url=url : force the player to connect to an url. Ignore the url(s) found in the input data\n");
	printf("-no_url : the player will not connect to HBBTV services \n");
	printf("-dsmcc : enable the DSMCC data carousel processing \n");
}


/****************************************************************************/
/** getargs																	*/
/** @author Stanislas Selle													*/
/** @date 2011/06/15														*/
/** gets args from agrv and set the options into the arguments				*/
/****************************************************************************/

static void getargs(int argc, char *argv[], char* &input_data, Bool* dsmcc, char* &url, Bool* no_url)
{
	u32 i;

	no_url = 0;
	i = 0;
	input_data = NULL;
	url = NULL;

	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (arg[0]=='-') {
			if (!strnicmp(arg, "-input=", 7)) {
				input_data = arg+7;
			}else if (!strnicmp(arg, "-url=", 5)) {
					url = arg+5;
			}else if (!strnicmp(arg, "-no_url", 7)) {
						*no_url = 1;
			}else if (!strnicmp(arg, "-dsmcc", 6)) {						
						*dsmcc = 1;
			}else {
				usage();
				exit(0);
			}			
		}else {
			usage();
			exit(0);
		}
	}

	if( !input_data && (!url || *no_url)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Input data is  needed if no URL is given.\n"));
		usage();
		exit(0);
	}
	if( !*dsmcc) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Ignoring DSMCC data !! \n"));
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Starting the processing of the TS file %s \n",input_data));

}

int main (int argc, char* argv[])
{
	char* input_data;
	char* url;
	Bool dsmcc;
	Bool no_url;
	int Error;

	dsmcc = no_url = 0;

	url = input_data = NULL;

	//TRACEINFO;
	gf_sys_init(1);
	gf_log_set_tool_level(GF_LOG_MODULE,GF_LOG_INFO);
	printf("GPAC HBBTV Terminal (c) Telecom ParisTech 2011\n");	

	getargs(argc, argv, input_data , &dsmcc, url, &no_url);

	GF_SAFEALLOC(player_interf, sPlayerInterface);

	HbbtvDemuxer* hbbtv_demuxer = new HbbtvDemuxer(input_data, url,dsmcc, no_url, player_interf);

	gtk_init(&argc, &argv);

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	Error = init_playerinterface(player_interf,input_data,url,no_url);
	if(Error){
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[HBBTVTerminal] Error - Aborting the processing \n"));
		return 0;
	}
	//~ gf_term_set_option(player_interf->m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);

	gtk_main();
	//free(ui);
}

