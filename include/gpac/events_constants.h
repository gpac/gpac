/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Events management
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


#ifndef _GF_EVENTS_CONSTANTS_H_
#define _GF_EVENTS_CONSTANTS_H_



/*
		minimal event system

	DO NOT CHANGE THEIR POSITION IN THE LIST, USED TO SPEED UP FILTERING OF USER INPUT EVENTS
*/

/*events*/
typedef enum {

	/******************************************************

		Events used for both GPAC internals and DOM Events

	*******************************************************/
	/*MouseEvents*/
	GF_EVENT_CLICK,
	GF_EVENT_MOUSEUP,
	GF_EVENT_MOUSEDOWN,
	GF_EVENT_MOUSEOVER,
	GF_EVENT_MOUSEOUT,
	/*!! ALL MOUSE EVENTS SHALL BE DECLARED BEFORE MOUSEMOVE !! */
	GF_EVENT_MOUSEMOVE,
	/*mouse wheel event*/
	GF_EVENT_MOUSEWHEEL,

	/*Key Events*/
	GF_EVENT_KEYUP,
	GF_EVENT_KEYDOWN, /* covers KeyDown, KeyPress and AccessKey */
	GF_EVENT_LONGKEYPRESS,
	/*character input*/
	GF_EVENT_TEXTINPUT,


	/******************************************************

		Events used for DOM Events only

	*******************************************************/
	GF_EVENT_TEXTSELECT,

	/*DOM UIEvents*/
	GF_EVENT_FOCUSIN,
	GF_EVENT_FOCUSOUT,
	GF_EVENT_ACTIVATE,
	GF_EVENT_CHANGE,
	GF_EVENT_FOCUS,
	GF_EVENT_BLUR,
	/*SVG (HTML) Events*/
	GF_EVENT_LOAD,
	GF_EVENT_UNLOAD,
	GF_EVENT_ABORT,
	GF_EVENT_ERROR,
	GF_EVENT_RESIZE,
	GF_EVENT_SCROLL,
	GF_EVENT_ZOOM,
	GF_EVENT_BEGIN, /*this is a fake event, it is NEVER fired, only used in SMIL begin*/
	GF_EVENT_BEGIN_EVENT,
	GF_EVENT_END, /*this is a fake event, it is NEVER fired, only used in SMIL end*/
	GF_EVENT_END_EVENT,
	GF_EVENT_REPEAT, /*this is a fake event, it is NEVER fired, only used in SMIL repeat*/
	GF_EVENT_REPEAT_EVENT,

	/*DOM MutationEvents - NOT SUPPORTED YET*/
	GF_EVENT_TREE_MODIFIED,
	GF_EVENT_NODE_INSERTED,
	GF_EVENT_NODE_REMOVED,
	GF_EVENT_NODE_INSERTED_DOC,
	GF_EVENT_NODE_REMOVED_DOC,
	GF_EVENT_ATTR_MODIFIED,
	GF_EVENT_CHAR_DATA_MODIFIED,
	GF_EVENT_NODE_NAME_CHANGED,
	GF_EVENT_ATTR_NAME_CHANGED,

	GF_EVENT_DCCI_PROP_CHANGE,

	/*LASeR events*/
	GF_EVENT_ACTIVATED,
	GF_EVENT_DEACTIVATED,
	GF_EVENT_PAUSE,
	GF_EVENT_PAUSED_EVENT,
	GF_EVENT_PLAY,
	GF_EVENT_REPEAT_KEY,
	GF_EVENT_RESUME_EVENT,
	GF_EVENT_SHORT_ACCESSKEY,
	/*pseudo-event, only used in LASeR coding*/
	GF_EVENT_EXECUTION_TIME,

	/*HTML5 media events*/
	GF_EVENT_MEDIA_SETUP_BEGIN,	/*not HTML5 but should be :)*/
	GF_EVENT_MEDIA_SETUP_DONE,	/*not HTML5 but should be :)*/
	GF_EVENT_MEDIA_LOAD_START,
	GF_EVENT_MEDIA_LOAD_DONE,	/*not HTML5 but should be :)*/
	GF_EVENT_MEDIA_PROGRESS, 
	GF_EVENT_MEDIA_SUSPEND, 
	GF_EVENT_MEDIA_EMPTIED, 
	GF_EVENT_MEDIA_STALLED, 
	GF_EVENT_MEDIA_LOADED_METADATA, 
	GF_EVENT_MEDIA_LODADED_DATA, 
	GF_EVENT_MEDIA_CANPLAY, 
	GF_EVENT_MEDIA_CANPLAYTHROUGH, 
	GF_EVENT_MEDIA_PLAYING, 
	GF_EVENT_MEDIA_WAITING, 
	GF_EVENT_MEDIA_SEEKING, 
	GF_EVENT_MEDIA_SEEKED, 
	GF_EVENT_MEDIA_ENDED, 
	GF_EVENT_MEDIA_DURATION_CHANGED, 
	GF_EVENT_MEDIA_TIME_UPDATE, 
	GF_EVENT_MEDIA_RATECHANGE, 
	GF_EVENT_MEDIA_VOLUME_CHANGED, 

	GF_EVENT_HTML_MSE_SOURCE_OPEN,
	GF_EVENT_HTML_MSE_SOURCE_ENDED,
	GF_EVENT_HTML_MSE_SOURCE_CLOSE,
	GF_EVENT_HTML_MSE_UPDATE_START,
	GF_EVENT_HTML_MSE_UPDATE,
	GF_EVENT_HTML_MSE_UPDATE_END,
	GF_EVENT_HTML_MSE_UPDATE_ERROR,
	GF_EVENT_HTML_MSE_UPDATE_ABORT,
	GF_EVENT_HTML_MSE_ADD_SOURCE_BUFFER,
	GF_EVENT_HTML_MSE_REMOVE_SOURCE_BUFFER,

	GF_EVENT_BATTERY,
	GF_EVENT_CPU,
	GF_EVENT_UNKNOWN,


	/******************************************************

		Events used for GPAC internals only

	*******************************************************/

	/*same as mousedown, generated internally by GPAC*/
	GF_EVENT_DBLCLICK,

	/*scene attached event, dispatched when the root node of a scene is loaded and
	attached to the window or parent object (animation, inline, ...)*/
	GF_EVENT_SCENE_ATTACHED,

	/*VP resize attached event, dispatched when viewport of a scene is being modified
	attached to the window or parent object (animation, inline, ...)*/
	GF_EVENT_VP_RESIZE,

	/*window events*/
	/*size has changed - indicate new w & h in .x end .y fields of event.
	When sent from gpac to a video plugin, indicates the output size should be changed. This is only sent when the plugin
	manages the output video himself
	When sent from a video plugin to gpac, indicates the output size has been changed. This is only sent when the plugin
	manages the output video himself
	*/
	GF_EVENT_SIZE,
	/*signals the scene size (if indicated in scene) upon connection (sent to the user event proc only)
		if scene size hasn't changed (seeking or other) this event is not sent
	*/
	GF_EVENT_SCENE_SIZE,
	GF_EVENT_SHOWHIDE,	/*window show/hide (minimized or other). This is also sent to the user to signal focus switch in fullscreen*/
	GF_EVENT_SET_CURSOR,	/*set mouse cursor*/
	GF_EVENT_SET_CAPTION,	/*set window caption*/
	GF_EVENT_MOVE,		/*move window*/
	GF_EVENT_REFRESH, /*window needs repaint (whenever needed, eg restore, hide->show, background refresh, paint)*/
	GF_EVENT_QUIT,	/*app is being closed - associated structure is evt.message to carry any potential reason for quiting*/
	/*video hw setup message:
		- when sent from gpac to plugin, indicates that the plugin should re-setup hardware context due to a window resize:
			* for 2D output, this means resizing the backbuffer if needed (depending on HW constraints)
			* for 3D output, this means re-setup of OpenGL context (depending on HW constraints). Depending on windowing systems
			and implementations, it could be possible to resize a window without destroying the GL context.

		- when sent from plugin to gpac, indicates that hardware has been setup. 
		- when sent from gpac to user, indicate aspect ratio has been modified and video output is ready
	*/
	GF_EVENT_VIDEO_SETUP,
	/*queries the list of system colors - only exchanged between compositor and video output*/
	GF_EVENT_SYS_COLORS,

	/*indicates some text has been pasted - from video output to compositor only*/
	GF_EVENT_PASTE_TEXT,
	/*queries for text to be copied - from video output to compositor only*/
	GF_EVENT_COPY_TEXT,

	/*terminal events*/
	GF_EVENT_CONNECT,	/*signal URL is connected*/
	GF_EVENT_DURATION,	/*signal duration of presentation*/
	GF_EVENT_EOS,	/*signal End of scene playback*/
	GF_EVENT_AUTHORIZATION,	/*indicates a user and pass is queried*/
	GF_EVENT_NAVIGATE, /*indicates the user app should load or jump to the given URL.*/
	GF_EVENT_NAVIGATE_INFO, /*indicates the link or its description under the mouse pointer*/
	GF_EVENT_MESSAGE, /*message from the MPEG-4 terminal*/
	GF_EVENT_PROGRESS, /*progress message from the MPEG-4 terminal*/
	GF_EVENT_FORWARDED, /*event forwarded by service (MPEG-2, RTP, ...)*/
	GF_EVENT_VIEWPOINTS,	/*indicates viewpoint list has changed - no struct associated*/
	GF_EVENT_STREAMLIST,	/*indicates stream list has changed - no struct associated - only used when no scene info is present*/
	GF_EVENT_METADATA, /*indicates a change in associated metadata*/
	GF_EVENT_MIGRATE, /*indicates a session migration request*/
	GF_EVENT_DISCONNECT, /*indicates the current url should be disconnected*/
	GF_EVENT_RESOLUTION, /*indicates the screen resolution has changed*/
	GF_EVENT_OPENFILE,
    /* Events for Keyboad */
    GF_EVENT_TEXT_EDITING_START,
    GF_EVENT_TEXT_EDITING_END,

	GF_EVENT_ADDON_DETECTED,
} GF_EventType;

/*GPAC/DOM3 key codes*/
typedef enum {
	GF_KEY_UNIDENTIFIED = 0,
	GF_KEY_ACCEPT = 1, /* "Accept"    The Accept (Commit) key.*/
	GF_KEY_AGAIN, /* "Again"  The Again key.*/
	GF_KEY_ALLCANDIDATES, /* "AllCandidates"    The All Candidates key.*/
	GF_KEY_ALPHANUM, /*"Alphanumeric"    The Alphanumeric key.*/
	GF_KEY_ALT, /*"Alt"    The Alt (Menu) key.*/
	GF_KEY_ALTGRAPH, /*"AltGraph"    The Alt-Graph key.*/
	GF_KEY_APPS, /*"Apps"    The Application key.*/
	GF_KEY_ATTN, /*"Attn"    The ATTN key.*/
	GF_KEY_BROWSERBACK, /*"BrowserBack"    The Browser Back key.*/
	GF_KEY_BROWSERFAVORITES, /*"BrowserFavorites"    The Browser Favorites key.*/
	GF_KEY_BROWSERFORWARD, /*"BrowserForward"    The Browser Forward key.*/
	GF_KEY_BROWSERHOME, /*"BrowserHome"    The Browser Home key.*/
	GF_KEY_BROWSERREFRESH, /*"BrowserRefresh"    The Browser Refresh key.*/
	GF_KEY_BROWSERSEARCH, /*"BrowserSearch"    The Browser Search key.*/
	GF_KEY_BROWSERSTOP, /*"BrowserStop"    The Browser Stop key.*/
	GF_KEY_CAPSLOCK, /*"CapsLock"    The Caps Lock (Capital) key.*/
	GF_KEY_CLEAR, /*"Clear"    The Clear key.*/
	GF_KEY_CODEINPUT, /*"CodeInput"    The Code Input key.*/
	GF_KEY_COMPOSE, /*"Compose"    The Compose key.*/
	GF_KEY_CONTROL, /*"Control"    The Control (Ctrl) key.*/
	GF_KEY_CRSEL, /*"Crsel"    The Crsel key.*/
	GF_KEY_CONVERT, /*"Convert"    The Convert key.*/
	GF_KEY_COPY, /*"Copy"    The Copy key.*/
	GF_KEY_CUT, /*"Cut"    The Cut key.*/
	GF_KEY_DOWN, /*"Down"    The Down Arrow key.*/
	GF_KEY_END, /*"End"    The End key.*/
	GF_KEY_ENTER, /*"Enter"    The Enter key.
                   Note: This key identifier is also used for the Return (Macintosh numpad) key.*/
	GF_KEY_ERASEEOF, /*"EraseEof"    The Erase EOF key.*/
	GF_KEY_EXECUTE, /*"Execute"    The Execute key.*/
	GF_KEY_EXSEL, /*"Exsel"    The Exsel key.*/
	GF_KEY_F1, /*"F1"    The F1 key.*/
	GF_KEY_F2, /*"F2"    The F2 key.*/
	GF_KEY_F3, /*"F3"    The F3 key.*/
	GF_KEY_F4, /*"F4"    The F4 key.*/
	GF_KEY_F5, /*"F5"    The F5 key.*/
	GF_KEY_F6, /*"F6"    The F6 key.*/
	GF_KEY_F7, /*"F7"    The F7 key.*/
	GF_KEY_F8, /*"F8"    The F8 key.*/
	GF_KEY_F9, /*"F9"    The F9 key.*/
	GF_KEY_F10, /*"F10"    The F10 key.*/
	GF_KEY_F11, /*"F11"    The F11 key.*/
	GF_KEY_F12, /*"F12"    The F12 key.*/
	GF_KEY_F13, /*"F13"    The F13 key.*/
	GF_KEY_F14, /*"F14"    The F14 key.*/
	GF_KEY_F15, /*"F15"    The F15 key.*/
	GF_KEY_F16, /*"F16"    The F16 key.*/
	GF_KEY_F17, /*"F17"    The F17 key.*/
	GF_KEY_F18, /*"F18"    The F18 key.*/
	GF_KEY_F19, /*"F19"    The F19 key.*/
	GF_KEY_F20, /*"F20"    The F20 key.*/
	GF_KEY_F21, /*"F21"    The F21 key.*/
	GF_KEY_F22, /*"F22"    The F22 key.*/
	GF_KEY_F23, /*"F23"    The F23 key.*/
	GF_KEY_F24, /*"F24"    The F24 key.*/
	GF_KEY_FINALMODE, /*"FinalMode"    The Final Mode (Final) key used on some asian keyboards.*/
	GF_KEY_FIND, /*"Find"    The Find key.*/
	GF_KEY_FULLWIDTH, /*"FullWidth"    The Full-Width Characters key.*/
	GF_KEY_HALFWIDTH, /*"HalfWidth"    The Half-Width Characters key.*/
	GF_KEY_HANGULMODE, /*"HangulMode"    The Hangul (Korean characters) Mode key.*/
	GF_KEY_HANJAMODE, /*"HanjaMode"    The Hanja (Korean characters) Mode key.*/
	GF_KEY_HELP, /*"Help"    The Help key.*/
	GF_KEY_HIRAGANA, /*"Hiragana"    The Hiragana (Japanese Kana characters) key.*/
	GF_KEY_HOME, /*"Home"    The Home key.*/
	GF_KEY_INSERT, /*"Insert"    The Insert (Ins) key.*/
	GF_KEY_JAPANESEHIRAGANA, /*"JapaneseHiragana"    The Japanese-Hiragana key.*/
	GF_KEY_JAPANESEKATAKANA, /*"JapaneseKatakana"    The Japanese-Katakana key.*/
	GF_KEY_JAPANESEROMAJI, /*"JapaneseRomaji"    The Japanese-Romaji key.*/
	GF_KEY_JUNJAMODE, /*"JunjaMode"    The Junja Mode key.*/
	GF_KEY_KANAMODE, /*"KanaMode"    The Kana Mode (Kana Lock) key.*/
	GF_KEY_KANJIMODE, /*"KanjiMode"    The Kanji (Japanese name for ideographic characters of Chinese origin) Mode key.*/
	GF_KEY_KATAKANA, /*"Katakana"    The Katakana (Japanese Kana characters) key.*/
	GF_KEY_LAUNCHAPPLICATION1, /*"LaunchApplication1"    The Start Application One key.*/
	GF_KEY_LAUNCHAPPLICATION2, /*"LaunchApplication2"    The Start Application Two key.*/
	GF_KEY_LAUNCHMAIL, /*"LaunchMail"    The Start Mail key.*/
	GF_KEY_LEFT, /*"Left"    The Left Arrow key.*/
	GF_KEY_META, /*"Meta"    The Meta key.*/
	GF_KEY_MEDIANEXTTRACK, /*"MediaNextTrack"    The Media Next Track key.*/
	GF_KEY_MEDIAPLAYPAUSE, /*"MediaPlayPause"    The Media Play Pause key.*/
	GF_KEY_MEDIAPREVIOUSTRACK, /*"MediaPreviousTrack"    The Media Previous Track key.*/
	GF_KEY_MEDIASTOP, /*"MediaStop"    The Media Stok key.*/
	GF_KEY_MODECHANGE, /*"ModeChange"    The Mode Change key.*/
	GF_KEY_NONCONVERT, /*"Nonconvert"    The Nonconvert (Don't Convert) key.*/
	GF_KEY_NUMLOCK, /*"NumLock"    The Num Lock key.*/
	GF_KEY_PAGEDOWN, /*"PageDown"    The Page Down (Next) key.*/
	GF_KEY_PAGEUP, /*"PageUp"    The Page Up key.*/
	GF_KEY_PASTE, /*"Paste"    The Paste key.*/
	GF_KEY_PAUSE, /*"Pause"    The Pause key.*/
	GF_KEY_PLAY, /*"Play"    The Play key.*/
	GF_KEY_PREVIOUSCANDIDATE, /*"PreviousCandidate"    The Previous Candidate function key.*/
	GF_KEY_PRINTSCREEN, /*"PrintScreen"    The Print Screen (PrintScrn, SnapShot) key.*/
	GF_KEY_PROCESS, /*"Process"    The Process key.*/
	GF_KEY_PROPS, /*"Props"    The Props key.*/
	GF_KEY_RIGHT, /*"Right"    The Right Arrow key.*/
	GF_KEY_ROMANCHARACTERS, /*"RomanCharacters"    The Roman Characters function key.*/
	GF_KEY_SCROLL, /*"Scroll"    The Scroll Lock key.*/
	GF_KEY_SELECT, /*"Select"    The Select key.*/
	GF_KEY_SELECTMEDIA, /*"SelectMedia"    The Select Media key.*/
	GF_KEY_SHIFT, /*"Shift"    The Shift key.*/
	GF_KEY_STOP, /*"Stop"    The Stop key.*/
	GF_KEY_UP, /*"Up"    The Up Arrow key.*/
	GF_KEY_UNDO, /*"Undo"    The Undo key.*/
	GF_KEY_VOLUMEDOWN, /*"VolumeDown"    The Volume Down key.*/
	GF_KEY_VOLUMEMUTE, /*"VolumeMute"    The Volume Mute key.*/
	GF_KEY_VOLUMEUP, /*"VolumeUp"    The Volume Up key.*/
	GF_KEY_WIN, /*"Win"    The Windows Logo key.*/
	GF_KEY_ZOOM, /*"Zoom"    The Zoom key.*/
	GF_KEY_BACKSPACE, /*"U+0008"    The Backspace (Back) key.*/
	GF_KEY_TAB, /*"U+0009"    The Horizontal Tabulation (Tab) key.*/
	GF_KEY_CANCEL, /*"U+0018"    The Cancel key.*/
	GF_KEY_ESCAPE, /*"U+001B"    The Escape (Esc) key.*/
	GF_KEY_SPACE, /*"U+0020"    The Space (Spacebar) key.*/
	GF_KEY_EXCLAMATION, /*"U+0021"    The Exclamation Mark (Factorial, Bang) key (!).*/
	GF_KEY_QUOTATION, /*"U+0022"    The Quotation Mark (Quote Double) key (").*/
	GF_KEY_NUMBER, /*"U+0023"    The Number Sign (Pound Sign, Hash, Crosshatch, Octothorpe) key (#).*/
	GF_KEY_DOLLAR, /*"U+0024"    The Dollar Sign (milreis, escudo) key ($).*/
	GF_KEY_AMPERSAND, /*"U+0026"    The Ampersand key (&).*/
	GF_KEY_APOSTROPHE, /*"U+0027"    The Apostrophe (Apostrophe-Quote, APL Quote) key (').*/
	GF_KEY_LEFTPARENTHESIS, /*"U+0028"    The Left Parenthesis (Opening Parenthesis) key (().*/
	GF_KEY_RIGHTPARENTHESIS, /*"U+0029"    The Right Parenthesis (Closing Parenthesis) key ()).*/
	GF_KEY_STAR, /*"U+002A"    The Asterix (Star) key (*).*/
	GF_KEY_PLUS, /*"U+002B"    The Plus Sign (Plus) key (+).*/
	GF_KEY_COMMA, /*"U+002C"    The Comma (decimal separator) sign key (,).*/
	GF_KEY_HYPHEN, /*"U+002D"    The Hyphen-minus (hyphen or minus sign) key (-).*/
	GF_KEY_FULLSTOP, /*"U+002E"    The Full Stop (period, dot, decimal point) key (.).*/
	GF_KEY_SLASH, /*"U+002F"    The Solidus (slash, virgule, shilling) key (/).*/
	GF_KEY_0, /*"U+0030"    The Digit Zero key (0).*/
	GF_KEY_1, /*"U+0031"    The Digit One key (1).*/
	GF_KEY_2, /*"U+0032"    The Digit Two key (2).*/
	GF_KEY_3, /*"U+0033"    The Digit Three key (3).*/
	GF_KEY_4, /*"U+0034"    The Digit Four key (4).*/
	GF_KEY_5, /*"U+0035"    The Digit Five key (5).*/
	GF_KEY_6, /*"U+0036"    The Digit Six key (6).*/
	GF_KEY_7, /*"U+0037"    The Digit Seven key (7).*/
	GF_KEY_8, /*"U+0038"    The Digit Eight key (8).*/
	GF_KEY_9, /*"U+0039"    The Digit Nine key (9).*/
	GF_KEY_COLON, /*"U+003A"    The Colon key (:).*/
	GF_KEY_SEMICOLON, /*"U+003B"    The Semicolon key (;).*/
	GF_KEY_LESSTHAN, /*"U+003C"    The Less-Than Sign key (<).*/
	GF_KEY_EQUALS, /*"U+003D"    The Equals Sign key (=).*/
	GF_KEY_GREATERTHAN, /*"U+003E"    The Greater-Than Sign key (>).*/
	GF_KEY_QUESTION, /*"U+003F"    The Question Mark key (?).*/
	GF_KEY_AT, /*"U+0040"    The Commercial At (@) key.*/
	GF_KEY_A, /*"U+0041"    The Latin Capital Letter A key (A).*/
	GF_KEY_B, /*"U+0042"    The Latin Capital Letter B key (B).*/
	GF_KEY_C, /*"U+0043"    The Latin Capital Letter C key (C).*/
	GF_KEY_D, /*"U+0044"    The Latin Capital Letter D key (D).*/
	GF_KEY_E, /*"U+0045"    The Latin Capital Letter E key (E).*/
	GF_KEY_F, /*"U+0046"    The Latin Capital Letter F key (F).*/
	GF_KEY_G, /*"U+0047"    The Latin Capital Letter G key (G).*/
	GF_KEY_H, /*"U+0048"    The Latin Capital Letter H key (H).*/
	GF_KEY_I, /*"U+0049"    The Latin Capital Letter I key (I).*/
	GF_KEY_J, /*"U+004A"    The Latin Capital Letter J key (J).*/
	GF_KEY_K, /*"U+004B"    The Latin Capital Letter K key (K).*/
	GF_KEY_L, /*"U+004C"    The Latin Capital Letter L key (L).*/
	GF_KEY_M, /*"U+004D"    The Latin Capital Letter M key (M).*/
	GF_KEY_N, /*"U+004E"    The Latin Capital Letter N key (N).*/
	GF_KEY_O, /*"U+004F"    The Latin Capital Letter O key (O).*/
	GF_KEY_P, /*"U+0050"    The Latin Capital Letter P key (P).*/
	GF_KEY_Q, /*"U+0051"    The Latin Capital Letter Q key (Q).*/
	GF_KEY_R, /*"U+0052"    The Latin Capital Letter R key (R).*/
	GF_KEY_S, /*"U+0053"    The Latin Capital Letter S key (S).*/
	GF_KEY_T, /*"U+0054"    The Latin Capital Letter T key (T).*/
	GF_KEY_U, /*"U+0055"    The Latin Capital Letter U key (U).*/
	GF_KEY_V, /*"U+0056"    The Latin Capital Letter V key (V).*/
	GF_KEY_W, /*"U+0057"    The Latin Capital Letter W key (W).*/
	GF_KEY_X, /*"U+0058"    The Latin Capital Letter X key (X).*/
	GF_KEY_Y, /*"U+0059"    The Latin Capital Letter Y key (Y).*/
	GF_KEY_Z, /*"U+005A"    The Latin Capital Letter Z key (Z).*/
	GF_KEY_LEFTSQUAREBRACKET, /*"U+005B"    The Left Square Bracket (Opening Square Bracket) key ([).*/
	GF_KEY_BACKSLASH, /*"U+005C"    The Reverse Solidus (Backslash) key (\).*/
	GF_KEY_RIGHTSQUAREBRACKET, /*"U+005D"    The Right Square Bracket (Closing Square Bracket) key (]).*/
	GF_KEY_CIRCUM, /*"U+005E"    The Circumflex Accent key (^).*/
	GF_KEY_UNDERSCORE, /*"U+005F"    The Low Sign (Spacing Underscore, Underscore) key (_).*/
	GF_KEY_GRAVEACCENT, /*"U+0060"    The Grave Accent (Back Quote) key (`).*/
	GF_KEY_LEFTCURLYBRACKET, /*"U+007B"    The Left Curly Bracket (Opening Curly Bracket, Opening Brace, Brace Left) key ({).*/
	GF_KEY_PIPE, /*"U+007C"    The Vertical Line (Vertical Bar, Pipe) key (|).*/
	GF_KEY_RIGHTCURLYBRACKET, /*"U+007D"    The Right Curly Bracket (Closing Curly Bracket, Closing Brace, Brace Right) key (}).*/
	GF_KEY_DEL, /*"U+007F"    The Delete (Del) Key.*/
	GF_KEY_INVERTEXCLAMATION, /*"U+00A1"    The Inverted Exclamation Mark key (�).*/
	GF_KEY_DEADGRAVE, /*"U+0300"    The Combining Grave Accent (Greek Varia, Dead Grave) key.*/
	GF_KEY_DEADEACUTE, /*"U+0301"    The Combining Acute Accent (Stress Mark, Greek Oxia, Tonos, Dead Eacute) key.*/
	GF_KEY_DEADCIRCUM, /*"U+0302"    The Combining Circumflex Accent (Hat, Dead Circumflex) key.*/
	GF_KEY_DEADTILDE, /*"U+0303"    The Combining Tilde (Dead Tilde) key.*/
	GF_KEY_DEADMACRON, /*"U+0304"    The Combining Macron (Long, Dead Macron) key.*/
	GF_KEY_DEADBREVE, /*"U+0306"    The Combining Breve (Short, Dead Breve) key.*/
	GF_KEY_DEADABOVEDOT, /*"U+0307"    The Combining Dot Above (Derivative, Dead Above Dot) key.*/
	GF_KEY_DEADDIARESIS, /*"U+0308"    The Combining Diaeresis (Double Dot Abode, Umlaut, Greek Dialytika, Double Derivative, Dead Diaeresis) key.*/
	GF_KEY_DEADRINGABOVE, /*"U+030A"    The Combining Ring Above (Dead Above Ring) key.*/
	GF_KEY_DEADDOUBLEACUTE, /*"U+030B"    The Combining Double Acute Accent (Dead Doubleacute) key.*/
	GF_KEY_DEADCARON, /*"U+030C"    The Combining Caron (Hacek, V Above, Dead Caron) key.*/
	GF_KEY_DEADCEDILLA, /*"U+0327"    The Combining Cedilla (Dead Cedilla) key.*/
	GF_KEY_DEADOGONEK, /*"U+0328"    The Combining Ogonek (Nasal Hook, Dead Ogonek) key.*/
	GF_KEY_DEADIOTA, /*"U+0345"    The Combining Greek Ypogegrammeni (Greek Non-Spacing Iota Below, Iota Subscript, Dead Iota) key.*/
	GF_KEY_EURO, /*"U+20AC"    The Euro Currency Sign key (�).*/
	GF_KEY_DEADVOICESOUND, /*"U+3099"    The Combining Katakana-Hiragana Voiced Sound Mark (Dead Voiced Sound) key.*/
	GF_KEY_DEADSEMIVOICESOUND, /*"U+309A"    The Combining Katakana-Hiragana Semi-Voiced Sound Mark (Dead Semivoiced Sound) key. */
	/* STB */
	GF_KEY_CHANNELUP, /*ChannelUp*/
	GF_KEY_CHANNELDOWN, /*ChannelDown*/
	GF_KEY_TEXT, /*Text*/
	GF_KEY_INFO, /*Info*/
	GF_KEY_EPG, /*EPG*/
	GF_KEY_RECORD, /*Record*/
	GF_KEY_BEGINPAGE, /*BeginPage*/
    /* end STB */

	/*non-dom keys, used in LASeR*/
	GF_KEY_CELL_SOFT1,	/*soft1 key of cell phones*/
	GF_KEY_CELL_SOFT2,	/*soft2 key of cell phones*/

	/*for joystick handling*/
	GF_KEY_JOYSTICK
} GF_KeyCode;


/*key modifiers state - set by terminal (not set by video driver)*/
typedef enum
{
	GF_KEY_MOD_SHIFT = (1),
	GF_KEY_MOD_CTRL = (1<<2),
	GF_KEY_MOD_ALT = (1<<3),

	GF_KEY_EXT_NUMPAD = (1<<4),
	GF_KEY_EXT_LEFT = (1<<5),
	GF_KEY_EXT_RIGHT = (1<<6)
} GF_KeyModifier;

/*sensor signaling*/
enum
{
	GF_CURSOR_NORMAL = 0x00,
	GF_CURSOR_ANCHOR,
	GF_CURSOR_TOUCH,
	/*discSensor, cylinderSensor, sphereSensor*/
	GF_CURSOR_ROTATE,
	/*proximitySensor & proximitySensor2D*/
	GF_CURSOR_PROXIMITY,
	/*planeSensor & planeSensor2D*/
	GF_CURSOR_PLANE,
	/*collision*/
	GF_CURSOR_COLLIDE,
	GF_CURSOR_HIDE,
};

/*Mutation AttrChangeType Signaling*/
enum
{
	GF_MUTATION_ATTRCHANGE_MODIFICATION = 0x01,
	GF_MUTATION_ATTRCHANGE_ADDITION = 0x02,
	GF_MUTATION_ATTRCHANGE_REMOVAL = 0x03,
};

enum
{
	/*events forwarded from MPEG-2 stack*/
	GF_EVT_FORWARDED_MPEG2 = 0,
	/*events forwarded from RTP/RTSP/IP stack (not used yet)*/
	GF_EVT_FORWARDED_RTP_RTSP
};

#endif
