/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre - Jean-Claude Moissinac
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
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

#include <gpac/base_coding.h>
#include <gpac/events.h>
#include <gpac/scenegraph_svg.h>
#include <gpac/internal/scenegraph_dev.h>

#define DUMP_COORDINATES 1

/*
	    list of supported events in Tiny 1.2 as of 2005/09/10:
		repeat is somehow a special case ...
*/
u32 gf_dom_event_type_by_name(const char *name)
{
	if (!strcmp(name, "abort"))	return GF_EVENT_ABORT;
	if (!strcmp(name, "activate"))	return GF_EVENT_ACTIVATE;
	if (!strcmp(name, "begin"))		return GF_EVENT_BEGIN;
	if (!strcmp(name, "beginEvent"))	return GF_EVENT_BEGIN_EVENT;
	if (!strcmp(name, "click"))		return GF_EVENT_CLICK;
	if (!strcmp(name, "end"))		return GF_EVENT_END;
	if (!strcmp(name, "endEvent"))	return GF_EVENT_END_EVENT;
	if (!strcmp(name, "error"))		return GF_EVENT_ERROR;
	if (!strcmp(name, "focusin"))	return GF_EVENT_FOCUSIN;
	if (!strcmp(name, "focusout"))	return GF_EVENT_FOCUSOUT;
	if (!strcmp(name, "keydown"))	return GF_EVENT_KEYDOWN;
	if (!strcmp(name, "keypress") || !stricmp(name, "accesskey"))	return GF_EVENT_KEYDOWN;
	if (!strcmp(name, "keyup"))		return GF_EVENT_KEYUP;
	if (!strcmp(name, "load"))		return GF_EVENT_LOAD;
	if (!strcmp(name, "SVGLoad"))	return GF_EVENT_LOAD;
	if (!strcmp(name, "longkeypress") || !stricmp(name, "longaccesskey"))	return GF_EVENT_LONGKEYPRESS;
	if (!strcmp(name, "mousedown")) return GF_EVENT_MOUSEDOWN;
	if (!strcmp(name, "mousemove")) return GF_EVENT_MOUSEMOVE;
	if (!strcmp(name, "mouseout"))	return GF_EVENT_MOUSEOUT;
	if (!strcmp(name, "mouseover")) return GF_EVENT_MOUSEOVER;
	if (!strcmp(name, "mouseup"))	return GF_EVENT_MOUSEUP;
	if (!strcmp(name, "repeat"))	return GF_EVENT_REPEAT;
	if (!strcmp(name, "repeatEvent"))	return GF_EVENT_REPEAT_EVENT;
	if (!strcmp(name, "resize"))	return GF_EVENT_RESIZE;
	if (!strcmp(name, "scroll"))	return GF_EVENT_SCROLL;
	if (!strcmp(name, "textInput"))	return GF_EVENT_TEXTINPUT;
	if (!strcmp(name, "unload"))	return GF_EVENT_UNLOAD;
	if (!strcmp(name, "zoom"))		return GF_EVENT_ZOOM;

	/*LASeR events*/
	if (!strcmp(name, "activatedEvent"))	return GF_EVENT_ACTIVATED;
	if (!strcmp(name, "deactivatedEvent"))	return GF_EVENT_DEACTIVATED;
	if (!strcmp(name, "executionTime"))	return GF_EVENT_EXECUTION_TIME;
	if (!strcmp(name, "pause"))	return GF_EVENT_PAUSE;
	if (!strcmp(name, "pausedEvent"))	return GF_EVENT_PAUSED_EVENT;
	if (!strcmp(name, "play"))	return GF_EVENT_PLAY;
	if (!strcmp(name, "repeatKey"))	return GF_EVENT_REPEAT_KEY;
	if (!strcmp(name, "resumedEvent"))	return GF_EVENT_RESUME_EVENT;
	if (!strcmp(name, "shortAccessKey"))	return GF_EVENT_SHORT_ACCESSKEY;
	/*LASeR unofficial events*/
	if (!strcmp(name, "battery"))		return GF_EVENT_BATTERY;
	if (!strcmp(name, "cpu"))		return GF_EVENT_CPU;
	GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[DOM Events] Unknown event found \"%s\"\n", name));
	return GF_EVENT_UNKNOWN;
}

const char *gf_dom_event_get_name(u32 type)
{

	switch (type) {
	case GF_EVENT_ABORT: return "abort";
	case GF_EVENT_ACTIVATE: return "activate";
	case GF_EVENT_BEGIN: return "begin";
	case GF_EVENT_BEGIN_EVENT: return "beginEvent";
	case GF_EVENT_CLICK: return "click";
	case GF_EVENT_END: return "end";
	case GF_EVENT_END_EVENT: return "endEvent";
	case GF_EVENT_ERROR: return "error";
	case GF_EVENT_FOCUSIN: return "focusin";
	case GF_EVENT_FOCUSOUT: return "focusout";
	case GF_EVENT_KEYDOWN: return "keydown";
	case GF_EVENT_KEYUP: return "keyup";
	case GF_EVENT_LOAD: return "load";
	case GF_EVENT_LONGKEYPRESS: return "longaccesskey";
	case GF_EVENT_MOUSEDOWN: return "mousedown";
	case GF_EVENT_MOUSEMOVE: return "mousemove";
	case GF_EVENT_MOUSEOUT: return "mouseout";
	case GF_EVENT_MOUSEOVER: return "mouseover";
	case GF_EVENT_MOUSEUP: return "mouseup";
	case GF_EVENT_REPEAT: return "repeat";
	case GF_EVENT_REPEAT_EVENT: return "repeatEvent";
	case GF_EVENT_RESIZE: return "resize";
	case GF_EVENT_SCROLL: return "scroll";
	case GF_EVENT_TEXTINPUT: return "textInput";
	case GF_EVENT_UNLOAD: return "unload";
	case GF_EVENT_ZOOM: return "zoom";

	/*LASeR events*/
	case GF_EVENT_ACTIVATED: return "activatedEvent";
	case GF_EVENT_DEACTIVATED: return "deactivatedEvent";
	case GF_EVENT_EXECUTION_TIME: return "executionTime";
	case GF_EVENT_PAUSE: return "pause";
	case GF_EVENT_PAUSED_EVENT: return "pausedEvent";
	case GF_EVENT_PLAY: return "play";
	case GF_EVENT_REPEAT_KEY: return "repeatKey";
	case GF_EVENT_RESUME_EVENT: return "resumedEvent";
	case GF_EVENT_SHORT_ACCESSKEY: return "shortAccessKey";
	/*LASeR unofficial events*/
	case GF_EVENT_BATTERY: return "battery";
	case GF_EVENT_CPU: return "cpu";

	default: return "unknown";
	}
}

static const struct predef_keyid {u32 key_code;  const char *name; } predefined_key_identifiers[] = 
{
	{ GF_KEY_ACCEPT, "Accept" },
	{ GF_KEY_AGAIN, "Again" },
	{ GF_KEY_ALLCANDIDATES, "AllCandidates" },
	{ GF_KEY_ALPHANUM, "Alphanumeric" },
	{ GF_KEY_ALT, "Alt" },
	{ GF_KEY_ALTGRAPH, "AltGraph" },
	{ GF_KEY_APPS, "Apps" },
	{ GF_KEY_ATTN, "Attn" },
	{ GF_KEY_BROWSERBACK, "BrowserBack" },
	{ GF_KEY_BROWSERFAVORITES, "BrowserFavorites" },
	{ GF_KEY_BROWSERFORWARD, "BrowserForward" },
	{ GF_KEY_BROWSERHOME, "BrowserHome" },
	{ GF_KEY_BROWSERREFRESH, "BrowserRefresh" },
	{ GF_KEY_BROWSERSEARCH, "BrowserSearch" },
	{ GF_KEY_BROWSERSTOP, "BrowserStop" },
	{ GF_KEY_CAPSLOCK, "CapsLock" },
	{ GF_KEY_CLEAR, "Clear" },
	{ GF_KEY_CODEINPUT, "CodeInput" },
	{ GF_KEY_COMPOSE, "Compose" },
	{ GF_KEY_CONTROL, "Control" },
	{ GF_KEY_CRSEL, "Crsel" },
	{ GF_KEY_CONVERT, "Convert" },
	{ GF_KEY_COPY, "Copy"  },
	{ GF_KEY_CUT, "Cut" },
	{ GF_KEY_DOWN, "Down" },
	{ GF_KEY_END, "End" },
	{ GF_KEY_ENTER, "Enter" },
	{ GF_KEY_ERASEEOF, "EraseEof" },
	{ GF_KEY_EXECUTE, "Execute" },
	{ GF_KEY_EXSEL, "Exsel" },
	{ GF_KEY_F1, "F1" },
	{ GF_KEY_F2, "F2" },
	{ GF_KEY_F3, "F3" },
	{ GF_KEY_F4, "F4" },
	{ GF_KEY_F5, "F5" },
	{ GF_KEY_F6, "F6" },
	{ GF_KEY_F7, "F7" },
	{ GF_KEY_F8, "F8" },
	{ GF_KEY_F9, "F9" },
	{ GF_KEY_F10, "F10" },
	{ GF_KEY_F11, "F11" },
	{ GF_KEY_F12, "F12" },
	{ GF_KEY_F13, "F13" },
	{ GF_KEY_F14, "F14" },
	{ GF_KEY_F15, "F15" },
	{ GF_KEY_F16, "F16" },
	{ GF_KEY_F17, "F17" },
	{ GF_KEY_F18, "F18" },
	{ GF_KEY_F19, "F19" },
	{ GF_KEY_F20, "F20" },
	{ GF_KEY_F21, "F21" },
	{ GF_KEY_F22, "F22" },
	{ GF_KEY_F23, "F23" },
	{ GF_KEY_F24, "F24" },
	{ GF_KEY_FINALMODE, "FinalMode" },
	{ GF_KEY_FIND, "Find" },
	{ GF_KEY_FULLWIDTH, "FullWidth" },
	{ GF_KEY_HALFWIDTH, "HalfWidth" },
	{ GF_KEY_HANGULMODE, "HangulMode" },
	{ GF_KEY_HANJAMODE, "HanjaMode"   },
	{ GF_KEY_HELP, "Help" },
	{ GF_KEY_HIRAGANA, "Hiragana" },
	{ GF_KEY_HOME, "Home" },
	{ GF_KEY_INSERT, "Insert" },
	{ GF_KEY_JAPANESEHIRAGANA, "JapaneseHiragana" },
	{ GF_KEY_JAPANESEKATAKANA, "JapaneseKatakana" },
	{ GF_KEY_JAPANESEROMAJI, "JapaneseRomaji" },
	{ GF_KEY_JUNJAMODE, "JunjaMode" },
	{ GF_KEY_KANAMODE, "KanaMode"   },
	{ GF_KEY_KANJIMODE, "KanjiMode" },
	{ GF_KEY_KATAKANA, "Katakana"   },
	{ GF_KEY_LAUNCHAPPLICATION1, "LaunchApplication1" },
	{ GF_KEY_LAUNCHAPPLICATION2, "LaunchApplication2" },
	{ GF_KEY_LAUNCHMAIL, "LaunchMail" },
	{ GF_KEY_LEFT, "Left" },
	{ GF_KEY_META, "Meta" },
	{ GF_KEY_MEDIANEXTTRACK, "MediaNextTrack" },
	{ GF_KEY_MEDIAPLAYPAUSE, "MediaPlayPause" },
	{ GF_KEY_MEDIAPREVIOUSTRACK, "MediaPreviousTrack" },
	{ GF_KEY_MEDIASTOP, "MediaStop" },
	{ GF_KEY_MODECHANGE, "ModeChange" },
	{ GF_KEY_NONCONVERT, "Nonconvert" },
	{ GF_KEY_NUMLOCK, "NumLock" },
	{ GF_KEY_PAGEDOWN, "PageDown" },
	{ GF_KEY_PAGEUP, "PageUp" },
	{ GF_KEY_PASTE, "Paste" },
	{ GF_KEY_PAUSE, "Pause" },
	{ GF_KEY_PLAY, "Play" },
	{ GF_KEY_PREVIOUSCANDIDATE, "PreviousCandidate" },
	{ GF_KEY_PRINTSCREEN, "PrintScreen" },
	{ GF_KEY_PROCESS, "Process" },
	{ GF_KEY_PROPS, "Props" },
	{ GF_KEY_RIGHT, "Right" },
	{ GF_KEY_ROMANCHARACTERS, "RomanCharacters" },
	{ GF_KEY_SCROLL, "Scroll" },
	{ GF_KEY_SELECT, "Select" },
	{ GF_KEY_SELECTMEDIA, "SelectMedia" },
	{ GF_KEY_SHIFT, "Shift" },
	{ GF_KEY_STOP, "Stop" },
	{ GF_KEY_UP, "Up" },
	{ GF_KEY_UNDO, "Undo" },
	{ GF_KEY_VOLUMEDOWN, "VolumeDown" },
	{ GF_KEY_VOLUMEMUTE, "VolumeMute" },
	{ GF_KEY_VOLUMEUP, "VolumeUp" },
	{ GF_KEY_WIN, "Win" },
	{ GF_KEY_ZOOM, "Zoom" },
	{ GF_KEY_BACKSPACE, "U+0008" },
	{ GF_KEY_TAB, "U+0009" },
	{ GF_KEY_CANCEL, "U+0018" },
	{ GF_KEY_ESCAPE, "U+001B" },
	{ GF_KEY_SPACE, "U+0020" },
	{ GF_KEY_EXCLAMATION, "U+0021" },
	{ GF_KEY_QUOTATION, "U+0022" },
	{ GF_KEY_NUMBER, "U+0023" },
	{ GF_KEY_DOLLAR, "U+0024" },
	{ GF_KEY_AMPERSAND, "U+0026" },
	{ GF_KEY_APOSTROPHE, "U+0027" },
	{ GF_KEY_LEFTPARENTHESIS, "U+0028" },
	{ GF_KEY_RIGHTPARENTHESIS, "U+0029" },
	{ GF_KEY_STAR, "U+002A" },
	{ GF_KEY_PLUS, "U+002B" },
	{ GF_KEY_COMMA, "U+002C" },
	{ GF_KEY_HYPHEN, "U+002D" },
	{ GF_KEY_FULLSTOP, "U+002E" },
	{ GF_KEY_SLASH, "U+002F" },
	{ GF_KEY_0, "U+0030" },
	{ GF_KEY_1, "U+0031" },
	{ GF_KEY_2, "U+0032" },
	{ GF_KEY_3, "U+0033" },
	{ GF_KEY_4, "U+0034" },
	{ GF_KEY_5, "U+0035" },
	{ GF_KEY_6, "U+0036" },
	{ GF_KEY_7, "U+0037" },
	{ GF_KEY_8, "U+0038" },
	{ GF_KEY_9, "U+0039" },
	{ GF_KEY_COLON, "U+003A" },
	{ GF_KEY_SEMICOLON, "U+003B" },
	{ GF_KEY_LESSTHAN, "U+003C" },
	{ GF_KEY_EQUALS, "U+003D" },
	{ GF_KEY_GREATERTHAN, "U+003E" },
	{ GF_KEY_QUESTION, "U+003F" },
	{ GF_KEY_AT, "U+0040" },
	{ GF_KEY_A, "U+0041" },
	{ GF_KEY_B, "U+0042" },
	{ GF_KEY_C, "U+0043" },
	{ GF_KEY_D, "U+0044" },
	{ GF_KEY_E, "U+0045" },
	{ GF_KEY_F, "U+0046" },
	{ GF_KEY_G, "U+0047" },
	{ GF_KEY_H, "U+0048" },
	{ GF_KEY_I, "U+0049" },
	{ GF_KEY_J, "U+004A" },
	{ GF_KEY_K, "U+004B" },
	{ GF_KEY_L, "U+004C" },
	{ GF_KEY_M, "U+004D" },
	{ GF_KEY_N, "U+004E" },
	{ GF_KEY_O, "U+004F" },
	{ GF_KEY_P, "U+0050" },
	{ GF_KEY_Q, "U+0051" },
	{ GF_KEY_R, "U+0052" },
	{ GF_KEY_S, "U+0053" },
	{ GF_KEY_T, "U+0054" },
	{ GF_KEY_U, "U+0055" },
	{ GF_KEY_V, "U+0056" },
	{ GF_KEY_W, "U+0057" },
	{ GF_KEY_X, "U+0058" },
	{ GF_KEY_Y, "U+0059" },
	{ GF_KEY_Z, "U+005A" },
	{ GF_KEY_LEFTSQUAREBRACKET, "U+005B" },
	{ GF_KEY_BACKSLASH, "U+005C" },
	{ GF_KEY_RIGHTSQUAREBRACKET, "U+005D" },
	{ GF_KEY_CIRCUM, "U+005E" },
	{ GF_KEY_UNDERSCORE, "U+005F" },
	{ GF_KEY_GRAVEACCENT, "U+0060" },
	{ GF_KEY_LEFTCURLYBRACKET, "U+007B" },
	{ GF_KEY_PIPE, "U+007C" },
	{ GF_KEY_RIGHTCURLYBRACKET, "U+007D" },
	{ GF_KEY_DEL, "U+007F" },
	{ GF_KEY_INVERTEXCLAMATION, "U+00A1" },
	{ GF_KEY_DEADGRAVE, "U+0300" },
	{ GF_KEY_DEADEACUTE, "U+0301" },
	{ GF_KEY_DEADCIRCUM, "U+0302" },
	{ GF_KEY_DEADTILDE, "U+0303" },
	{ GF_KEY_DEADMACRON, "U+0304" },
	{ GF_KEY_DEADBREVE, "U+0306" },
	{ GF_KEY_DEADABOVEDOT, "U+0307" },
	{ GF_KEY_DEADDIARESIS, "U+0308" },
	{ GF_KEY_DEADRINGABOVE, "U+030A" },
	{ GF_KEY_DEADDOUBLEACUTE, "U+030B" },
	{ GF_KEY_DEADCARON, "U+030C" },
	{ GF_KEY_DEADCEDILLA, "U+0327" },
	{ GF_KEY_DEADOGONEK, "U+0328" },
	{ GF_KEY_DEADIOTA, "U+0345" },
	{ GF_KEY_EURO, "U+20AC" },
	{ GF_KEY_DEADVOICESOUND, "U+3099" },
	{ GF_KEY_DEADSEMIVOICESOUND, "U+309A" }
};
const char *gf_dom_get_key_name(u32 key_identifier)
{
	u32 count = sizeof(predefined_key_identifiers) / sizeof(struct predef_keyid);
	if (!key_identifier || count<=key_identifier) return "Unknown";
	return predefined_key_identifiers[key_identifier-1].name;
}


u32 gf_dom_get_key_type(char *key_name)
{
	if (strlen(key_name) == 1) {
		char c[2];
		c[0] = key_name[0];
		c[1] = 0;
		strupr(c);
		if (c[0] >= 'A' && c[0] <= 'Z') 
			return (GF_KEY_A + (c[0] - 'A') );
		
		if (c[0] >= '0' && c[0] <= '9')
			return ( GF_KEY_0 + (c[0] - '0') );
		
		switch (c[0]) {
		case '@': return GF_KEY_AT;
		case '*': return GF_KEY_STAR;
		case '#': return GF_KEY_NUMBER;
		case ' ': return GF_KEY_SPACE;
		case '!': return GF_KEY_EXCLAMATION;
		case '"': return GF_KEY_QUOTATION;
		case '$': return GF_KEY_DOLLAR;
		case '&': return GF_KEY_AMPERSAND;
		case '\'': return GF_KEY_APOSTROPHE;
		case '(': return GF_KEY_LEFTPARENTHESIS;
		case ')': return GF_KEY_RIGHTPARENTHESIS;
		case '+': return GF_KEY_PLUS;
		case ',': return GF_KEY_COMMA;
		case '-': return GF_KEY_HYPHEN;
		case '.': return GF_KEY_FULLSTOP;
		case '/': return GF_KEY_SLASH;
		case ':': return GF_KEY_COLON;
		case ';': return GF_KEY_SEMICOLON;
		case '<': return GF_KEY_LESSTHAN;
		case '=': return GF_KEY_EQUALS;
		case '>': return GF_KEY_GREATERTHAN;
		case '?': return GF_KEY_QUESTION;
		case '[': return GF_KEY_LEFTSQUAREBRACKET;
		case '\\': return GF_KEY_BACKSLASH;
		case ']': return GF_KEY_RIGHTSQUAREBRACKET;
		case '^': return GF_KEY_CIRCUM;
		case '_': return GF_KEY_UNDERSCORE;
		case '`': return GF_KEY_GRAVEACCENT;
		case '{': return GF_KEY_LEFTCURLYBRACKET;
		case '|': return GF_KEY_PIPE;
		case '}': return GF_KEY_RIGHTCURLYBRACKET;
		case '�': return GF_KEY_INVERTEXCLAMATION;
		default: return GF_KEY_UNIDENTIFIED;
		}
	} else {
		u32 i, count;
		count = sizeof(predefined_key_identifiers) / sizeof(struct predef_keyid);
		for (i=0; i<count; i++) {
			if (!stricmp(key_name, predefined_key_identifiers[i].name)) {
				return predefined_key_identifiers[i].key_code;
			}
		}
		return GF_KEY_UNIDENTIFIED;
	}
}

/* Basic SVG datatype parsing functions */
static const struct predef_col { const char *name; u8 r; u8 g; u8 b; } predefined_colors[] = 
{
	{"aliceblue",240, 248, 255},
	{"antiquewhite",250, 235, 215},
	{"aqua", 0, 255, 255},
	{"aquamarine",127, 255, 212},
	{"azure",240, 255, 255},
	{"beige",245, 245, 220},
	{"bisque",255, 228, 196},
	{"black", 0, 0, 0},
	{"blanchedalmond",255, 235, 205},
	{"blue", 0, 0, 255},
	{"blueviolet",138, 43, 226},
	{"brown",165, 42, 42},
	{"burlywood",222, 184, 135},
	{"cadetblue", 95, 158, 160},
	{"chartreuse",127, 255, 0},
	{"chocolate",210, 105, 30},
	{"coral",255, 127, 80},
	{"lightpink",255, 182, 193},
	{"lightsalmon",255, 160, 122},
	{"lightseagreen", 32, 178, 170},
	{"lightskyblue",135, 206, 250},
	{"lightslategray",119, 136, 153},
	{"lightslategrey",119, 136, 153},
	{"lightsteelblue",176, 196, 222},
	{"lightyellow",255, 255, 224},
	{"lime", 0, 255, 0},
	{"limegreen", 50, 205, 50},
	{"linen",250, 240, 230},
	{"magenta",255, 0, 255},
	{"maroon",128, 0, 0},
	{"mediumaquamarine",102, 205, 170},
	{"mediumblue", 0, 0, 205},
	{"mediumorchid",186, 85, 211},
	{"cornflowerblue",100, 149, 237},
	{"cornsilk",255, 248, 220},
	{"crimson",220, 20, 60},
	{"cyan", 0, 255, 255},
	{"darkblue", 0, 0, 139},
	{"darkcyan", 0, 139, 139},
	{"darkgoldenrod",184, 134, 11},
	{"darkgray",169, 169, 169},
	{"darkgreen", 0, 100, 0},
	{"darkgrey",169, 169, 169},
	{"darkkhaki",189, 183, 107},
	{"darkmagenta",139, 0, 139},
	{"darkolivegreen", 85, 107, 47},
	{"darkorange",255, 140, 0},
	{"darkorchid",153, 50, 204},
	{"darkred",139, 0, 0},
	{"darksalmon",233, 150, 122},
	{"darkseagreen",143, 188, 143},
	{"darkslateblue", 72, 61, 139},
	{"darkslategray", 47, 79, 79},
	{"darkslategrey", 47, 79, 79},
	{"darkturquoise", 0, 206, 209},
	{"darkviolet",148, 0, 211},
	{"deeppink",255, 20, 147},
	{"deepskyblue", 0, 191, 255},
	{"dimgray",105, 105, 105},
	{"dimgrey",105, 105, 105},
	{"dodgerblue", 30, 144, 255},
	{"firebrick",178, 34, 34},
	{"floralwhite",255, 250, 240},
	{"forestgreen", 34, 139, 34},
	{"fuchsia",255, 0, 255},
	{"gainsboro",220, 220, 220},
	{"ghostwhite",248, 248, 255},
	{"gold",255, 215, 0},
	{"goldenrod",218, 165, 32},
	{"gray",128, 128, 128},
	{"grey",128, 128, 128},
	{"green", 0, 128, 0},
	{"greenyellow",173, 255, 47},
	{"honeydew",240, 255, 240},
	{"hotpink",255, 105, 180},
	{"indianred",205, 92, 92},
	{"indigo", 75, 0, 130},
	{"ivory",255, 255, 240},
	{"khaki",240, 230, 140},
	{"lavender",230, 230, 25},
	{"lavenderblush",255, 240, 245},
	{"mediumpurple",147, 112, 219},
	{"mediumseagreen", 60, 179, 113},
	{"mediumslateblue",123, 104, 238},
	{"mediumspringgreen", 0, 250, 154},
	{"mediumturquoise", 72, 209, 204},
	{"mediumvioletred",199, 21, 133},
	{"midnightblue", 25, 25, 112},
	{"mintcream",245, 255, 250},
	{"mistyrose",255, 228, 225},
	{"moccasin",255, 228, 181},
	{"navajowhite",255, 222, 173},
	{"navy", 0, 0, 128},
	{"oldlace",253, 245, 230},
	{"olive",128, 128, 0},
	{"olivedrab",107, 142, 35},
	{"orange",255, 165, 0},
	{"orangered",255, 69, 0},
	{"orchid",218, 112, 214},
	{"palegoldenrod",238, 232, 170},
	{"palegreen",152, 251, 152},
	{"paleturquoise",175, 238, 238},
	{"palevioletred",219, 112, 147},
	{"papayawhip",255, 239, 213},
	{"peachpuff",255, 218, 185},
	{"peru",205, 133, 63},
	{"pink",255, 192, 203},
	{"plum",221, 160, 221},
	{"powderblue",176, 224, 230},
	{"purple",128, 0, 128},
	{"red",255, 0, 0},
	{"rosybrown",188, 143, 143},
	{"royalblue", 65, 105, 225},
	{"saddlebrown",139, 69, 19},
	{"salmon",250, 128, 114},
	{"sandybrown",244, 164, 96},
	{"seagreen", 46, 139, 87},
	{"seashell",255, 245, 238},
	{"sienna",160, 82, 45},
	{"silver",192, 192, 192},
	{"skyblue",135, 206, 235},
	{"slateblue",106, 90, 205},
	{"slategray",112, 128, 144},
	{"slategrey",112, 128, 144},
	{"snow",255, 250, 250},
	{"springgreen", 0, 255, 127},
	{"steelblue", 70, 130, 180},
	{"tan",210, 180, 140},
	{"teal", 0, 128, 128},
	{"lawngreen",124, 252, 0},
	{"lemonchiffon",255, 250, 205},
	{"lightblue",173, 216, 230},
	{"lightcoral",240, 128, 128},
	{"lightcyan",224, 255, 255},
	{"lightgoldenrodyellow",250, 250, 210},
	{"lightgray",211, 211, 211},
	{"lightgreen",144, 238, 144},
	{"lightgrey",211, 211, 211},
	{"thistle",216, 191, 216},
	{"tomato",255, 99, 71},
	{"turquoise", 64, 224, 208},
	{"violet",238, 130, 238},
	{"wheat",245, 222, 179},
	{"white",255, 255, 255},
	{"whitesmoke",245, 245, 245},
	{"yellow",255, 255, 0},
	{"yellowgreen",154, 205, 50}

};


/* Basic SVG datatype parsing functions */
static const struct sys_col { const char *name; u8 type; } system_colors[] = 
{
	{"ActiveBorder", SVG_COLOR_ACTIVE_BORDER},
	{"ActiveCaption", SVG_COLOR_ACTIVE_CAPTION},
	{"AppWorkspace", SVG_COLOR_APP_WORKSPACE},
	{"Background", SVG_COLOR_BACKGROUND},
	{"ButtonFace", SVG_COLOR_BUTTON_FACE},
	{"ButtonHighlight", SVG_COLOR_BUTTON_HIGHLIGHT},
	{"ButtonShadow", SVG_COLOR_BUTTON_SHADOW},
	{"ButtonText", SVG_COLOR_BUTTON_TEXT},
	{"CaptionText", SVG_COLOR_CAPTION_TEXT},
	{"GrayText", SVG_COLOR_GRAY_TEXT},
	{"Highlight", SVG_COLOR_HIGHLIGHT},
	{"HighlightText", SVG_COLOR_HIGHLIGHT_TEXT},
	{"InactiveBorder", SVG_COLOR_INACTIVE_BORDER},
	{"InactiveCaption", SVG_COLOR_INACTIVE_CAPTION},
	{"InactiveCaptionText", SVG_COLOR_INACTIVE_CAPTION_TEXT},
	{"InfoBackground", SVG_COLOR_INFO_BACKGROUND},
	{"InfoText", SVG_COLOR_INFO_TEXT},
	{"Menu", SVG_COLOR_MENU},
	{"MenuText", SVG_COLOR_MENU_TEXT},
	{"Scrollbar", SVG_COLOR_SCROLLBAR},
	{"ThreeDDarkShadow", SVG_COLOR_3D_DARK_SHADOW},
	{"ThreeDFace", SVG_COLOR_3D_FACE},
	{"ThreeDHighlight", SVG_COLOR_3D_HIGHLIGHT},
	{"ThreeDLightShadow", SVG_COLOR_3D_LIGHT_SHADOW},
	{"ThreeDShadow", SVG_COLOR_3D_SHADOW},
	{"Window", SVG_COLOR_WINDOW},
	{"WindowFrame", SVG_COLOR_WINDOW_FRAME},
	{"WindowText", SVG_COLOR_WINDOW_TEXT},
};

/* parses an color from a named color HTML or CSS 2 */
static void svg_parse_named_color(SVG_Color *col, char *attribute_content)
{
	u32 i, count;
	count = sizeof(predefined_colors) / sizeof(struct predef_col);
	for (i=0; i<count; i++) {
		if (!strcmp(attribute_content, predefined_colors[i].name)) {
			col->red = INT2FIX(predefined_colors[i].r) / 255;
			col->green = INT2FIX(predefined_colors[i].g) / 255;
			col->blue = INT2FIX(predefined_colors[i].b) / 255;
			col->type = SVG_COLOR_RGBCOLOR;
			return;
		}
	}
	count = sizeof(system_colors) / sizeof(struct sys_col);
	for (i=0; i<count; i++) {
		if (!strcmp(attribute_content, system_colors[i].name)) {
			col->type = system_colors[i].type;
			return;
		}
	}
}

const char *gf_svg_get_system_paint_server_name(u32 paint_type)
{
	u32 i, count;
	count = sizeof(system_colors) / sizeof(struct sys_col);
	for (i=0; i<count; i++) {
		if (paint_type == system_colors[i].type) return system_colors[i].name;
	}
	return "undefined";
}

u32 gf_svg_get_system_paint_server_type(const char *name)
{
	u32 i, count;
	count = sizeof(system_colors) / sizeof(struct sys_col);
	for (i=0; i<count; i++) {
		if (!strcmp(name, system_colors[i].name)) return system_colors[i].type;
	}
	return 0;
}

/* Reads an SVG Color 
   either #RRGGBB, #RGB, rgb(r,g,b) in [0,255] , colorname, or 'r g b' in [0,1]
   ignores any space, comma, semi-column before and any space after
   TODO: 
	transform the char into char and duplicate the input, instead of modifying it
		be more robust to errors in color description ex rgb(0 0 0)
*/
static void svg_parse_color(SVG_Color *col, char *attribute_content)
{
	char *str = attribute_content;
	while (str[strlen(attribute_content)-1] == ' ') str[strlen(attribute_content)-1] = 0;
	while (*str != 0 && (*str == ' ' || *str == ',' || *str == ';')) str++;

	if (!strcmp(str, "currentColor")) {
		col->type = SVG_COLOR_CURRENTCOLOR;
		return;
	} else if (!strcmp(str, "inherit")) {
		col->type = SVG_COLOR_INHERIT;
		return;
	} else if (str[0]=='#') {
		u32 val;
		sscanf(str+1, "%x", &val);
		if (strlen(str) == 7) {
			col->red = INT2FIX((val>>16) & 0xFF) / 255;
			col->green = INT2FIX((val>>8) & 0xFF) / 255;
			col->blue = INT2FIX(val & 0xFF) / 255;
		} else {
			col->red = INT2FIX((val>>8) & 0xF) / 15;
			col->green = INT2FIX((val>>4) & 0xF) / 15;
			col->blue = INT2FIX(val & 0xF) / 15;
		}
		col->type = SVG_COLOR_RGBCOLOR;
	} else if (strstr(str, "rgb(") || strstr(str, "RGB(")) {
		Float _val;
		u8 is_percentage= 0;
		if (strstr(str, "%")) is_percentage = 1;
		str = strstr(str, "(");
		str++;
		sscanf(str, "%f", &_val); col->red = FLT2FIX(_val);
		str = strstr(str, ",");
		str++;
		sscanf(str, "%f", &_val); col->green = FLT2FIX(_val);
		str = strstr(str, ",");
		str++;
		sscanf(str, "%f", &_val); col->blue = FLT2FIX(_val);
		if (is_percentage) {
			col->red /= 100;
			col->green /= 100;
			col->blue /= 100;
		} else {
			col->red /= 255;
			col->green /= 255;
			col->blue /= 255;
		}
		col->type = SVG_COLOR_RGBCOLOR;
	} else if ((str[0] >= 'a' && str[0] <= 'z')
		|| (str[0] >= 'A' && str[0] <= 'Z')) {
		svg_parse_named_color(col, str);
	} else {
		Float _r, _g, _b;
		sscanf(str, "%f %f %f", &_r, &_g, &_b);
		col->red = FLT2FIX(_r);
		col->green = FLT2FIX(_g);
		col->blue = FLT2FIX(_b);
		col->type = SVG_COLOR_RGBCOLOR;
	}
}

/* 
	Reads a float in scientific notation 
		trims any space, comma, semi-column before or after
		reads an optional + or -
		then reads a digit between 0 and 9
		optionally followed by an '.' and digits between 0 and 9
		optionally followed by e or E and digits between 0 and 9
	Returns the number of char read in d 
*/
static u32 svg_parse_float(char *d, Fixed *f, Bool is_angle) 
{
	Bool is_negative = 0;
	Float _val = 0;
	u32 i = 0;
	while ((d[i] != 0) && strchr(" ,;\r\n", d[i])) i++;
	if (!d[i]) goto end;
	if (d[i] == '+') i++;
	if (d[i] == '-') {
		is_negative = 1;
		i++;
	}
	while (d[i] >= '0' && d[i] <= '9' && d[i] != 0) {
		_val = _val*10 + (d[i]-'0');
		i++;
	}
	if (!d[i]) goto end;
	if (d[i] == '.') {
		u32 nb_digit_after = 0;
		i++;
		while (d[i] >= '0' && d[i] <= '9' && d[i] != 0) {
			_val = _val*10 + (d[i]-'0');
			nb_digit_after++;
			i++;
		}
		_val /= (Float)pow(10,nb_digit_after);
		if (!d[i]) goto end;
	}
	if (d[i] == 'e' || d[i] == 'E') {
		u32 exp = 0;
		i++;
		if (d[i] == '+' || d[i] == '-') i++;
		while (d[i] >= '0' && d[i] <= '9' && d[i] != 0) {
			exp = exp*10 + (d[i]-'0');
			i++;
		}
		_val *= (Float)pow(10, exp);
	}
end:
	if (is_negative) _val *= -1;
	//_val = atof(d);
	if (is_angle) {
		_val/=180;
		(*f) = gf_mulfix(FLT2FIX(_val), GF_PI);
	} else {
		(*f) = FLT2FIX(_val);
	}
	while (d[i] != 0 && (d[i] == ' ' || d[i] == ',' || d[i] == ';')) i++;
	return i;
}

/*
   Parse an Offset Value, i.e +/- Clock Value
*/
static void svg_parse_clock_value(char *d, Double *clock_value) 
{
	char *tmp;
	s32 sign = 1;
	if (d[0] == '+') d++;
	if (d[0] == '-') { sign = -1; d++; }
	while (*d == ' ') d++;

	if ((tmp = strchr(d, ':'))) {
		/* Full or Partial Clock value */
		tmp++;
		if ((tmp = strchr(tmp, ':'))) {
			/* Full Clock value : hh:mm:ss(.frac) */
			u32 hours;
			u32 minutes;
			Float seconds;
			sscanf(d, "%d:%d:%f", &hours, &minutes, &seconds);
			*clock_value = hours*3600 + minutes*60 + seconds;
		} else {
			/* Partial Clock value : mm:ss(.frac) */
			s32 minutes;
			Float seconds;
			sscanf(d, "%d:%f", &minutes, &seconds);
			*clock_value = minutes*60 + seconds;
		}
	} else if ((tmp = strstr(d, "h"))) {
		Float f;
		sscanf(d, "%fh", &f);
		*clock_value = 3600*f;
	} else if (strstr(d, "min")) {
		Float f;
		sscanf(d, "%fmin", &f);
		*clock_value = 60*f;
	} else if ((tmp = strstr(d, "ms"))) {
		Float f;
		sscanf(d, "%fms", &f);
		*clock_value = f/1000;
	} else if (strchr(d, 's')) {
		Float f;
		sscanf(d, "%fs", &f);
		*clock_value = f;
	} else {
		*clock_value = atof(d);
	}
	*clock_value *= sign;
}
/* Parses one SVG time value:
	  'indefinite', 
	  'name.begin', 'name.end', 
	  wallclock,
	  accessKey,
	  events, 
	  clock value.
 */
static void smil_parse_time(GF_Node *e, SMIL_Time *v, char *d) 
{
	char *tmp;

	/* Offset Values */
	if ((d[0] >= '0' && d[0] <= '9') || d[0] == '+' || d[0] == '-'){
		v->type = GF_SMIL_TIME_CLOCK;
		svg_parse_clock_value(d, &(v->clock));
		return;
	} 
	
	/* Indefinite Values */
	else if (!strcmp(d, "indefinite")) {
		v->type = GF_SMIL_TIME_INDEFINITE;
		return;
	} 

	/* Wallclock Values */
	else if ((tmp = strstr(d, "wallclock("))) {
		u32 year, month, day;
		u32 hours, minutes;
		u32 nhours, nminutes;
		Float seconds;
		char *tmp1, *tmp2;

		v->type = GF_SMIL_TIME_WALLCLOCK;
		tmp += 10;
		if ((tmp1 = strchr(tmp, 'T')) ) {
			/* From tmp to wallStartTime, we parse a date */
			sscanf(tmp, "%d-%d-%dT", &year, &month, &day);
			tmp1++;
			tmp = tmp1;
		} 	
		if ((tmp1 = strchr(tmp, ':')) ) {
			if ((tmp2 = strchr(tmp1, ':')) ) {
				/* HHMMSS */
				sscanf(tmp, "%d:%d:%f", &hours, &minutes, &seconds);		
			} else {
				/* HHMM */
				sscanf(tmp, "%d:%d", &hours, &minutes);		
			}
		}
		if (strchr(tmp, 'Z')) {
			return;
		} else {
			if ( (tmp1 = strchr(tmp, '+')) ) {
				sscanf(tmp1, "%d:%d", &nhours, &nminutes);		
			} else if ( (tmp1 = strchr(tmp, '-')) ) {
				sscanf(tmp1, "%d:%d", &nhours, &nminutes);		
			}
		}
		return;
	} 

	/* AccessKey Values */
	else if ((tmp = strstr(d, "accessKey("))) {
		char *sep;
		v->type = GF_SMIL_TIME_EVENT;
		v->event.type = GF_EVENT_KEYDOWN;
		v->element = e->sgprivate->scenegraph->RootNode;
		tmp+=10;
		sep = strchr(d, ')');
		sep[0] = 0;
		v->event.parameter = gf_dom_get_key_type(tmp);
		sep++;
		if ((tmp = strchr(sep, '+')) || (tmp = strchr(sep, '-'))) {
			char c = *tmp;
			tmp++;
			svg_parse_clock_value(tmp, &(v->clock));
			if (c == '-') v->clock *= -1;
		} 
		return;
	} 

	else {
		Bool had_param = 0;
		char *tmp2;
		v->type = GF_SMIL_TIME_EVENT;
		if ((tmp = strchr(d, '.'))) {
			tmp[0] = 0;
			v->element_id = strdup(d);
			tmp[0] = '.';
			tmp++;
		} else {
			tmp = d;
		}
		if ((tmp2 = strchr(tmp, '('))) {
			tmp2[0] = 0;
			v->event.type = gf_dom_event_type_by_name(tmp);
			tmp2[0] = '(';
			tmp2++;
			had_param = 1;
			v->event.parameter = atoi(tmp2);
			tmp = strchr(tmp2, ')');
			if (!tmp) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] expecting ')' in SMIL Time %s\n", d));
				return;
			}			
			tmp++;
		} 
		if ((tmp2 = strchr(tmp, '+')) || (tmp2 = strchr(tmp, '-'))) {
			char c = *tmp2;
			char *tmp3 = tmp2;
			tmp2[0] = 0;
			tmp3--;
			while (*tmp3==' ') { *tmp3=0; tmp3--; }
			if (v->event.type == 0) v->event.type = gf_dom_event_type_by_name(tmp);
			if (!had_param && (v->event.type == GF_EVENT_REPEAT || v->event.type == GF_EVENT_REPEAT_EVENT))
				v->event.parameter = 1;
			tmp2[0] = c;
			tmp2++;
			svg_parse_clock_value(tmp2, &(v->clock));
			if (c == '-') v->clock *= -1;
		} else {
			if (v->event.type == 0) v->event.type = gf_dom_event_type_by_name(tmp);
			if (!had_param && (v->event.type == GF_EVENT_REPEAT || v->event.type == GF_EVENT_REPEAT_EVENT))
				v->event.parameter = 1;
		}
	}
}

/* Parses a list of SVG transformations and collapses them in the given matrix */
static void svg_parse_transformlist(GF_Matrix2D *mat, char *attribute_content) 
{
	GF_Matrix2D tmp;

	char *str;
	u32 i;
	
	gf_mx2d_init(*mat);
	
	str = attribute_content;
	i = 0;
	while (str[i] != 0) {
		while (str[i] == ' ') i++;
		if (str[i] == ',') i++;
		while (str[i] == ' ') i++;
		if (strstr(str+i, "scale")==str+i) {
			Fixed sx, sy;
			i += 5;
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == '(') {
				i++;
				i+=svg_parse_float(&(str[i]), &sx, 0);
				if (str[i] == ')') {
					sy = sx;
				} else {
					i+=svg_parse_float(&(str[i]), &sy, 0);
				}
				i++;
			}
			gf_mx2d_init(tmp);
			gf_mx2d_add_scale(&tmp, sx, sy);
			gf_mx2d_add_matrix(&tmp, mat);
			gf_mx2d_copy(*mat, tmp);
			while(str[i] != 0 && str[i] == ' ') i++;
		} else if (strstr(str+i, "translate")==str+i) {
			Fixed tx, ty;
			i += 9;
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == '(') {
				i++;
				i+=svg_parse_float(&(str[i]), &tx, 0);
				if (str[i] == ')') {
					ty = 0;
				} else {
					i+=svg_parse_float(&(str[i]), &ty, 0);
				}
				i++;
			}
			gf_mx2d_init(tmp);
			gf_mx2d_add_translation(&tmp, tx, ty);
			gf_mx2d_add_matrix(&tmp, mat);
			gf_mx2d_copy(*mat, tmp);
			while(str[i] != 0 && str[i] == ' ') i++;
		} else if (strstr(str+i, "rotate")==str+i) {
			Fixed angle, cx, cy;
			i += 6;
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == '(') {
				i++;
				i+=svg_parse_float(&(str[i]), &angle, 1);
				if (str[i] == ')') {
					cx = cy = 0;
				} else {
					i+=svg_parse_float(&(str[i]), &cx, 0);
					i+=svg_parse_float(&(str[i]), &cy, 0);
				}
				i++;
			}
			gf_mx2d_init(tmp);
			gf_mx2d_add_rotation(&tmp, cx, cy, angle);
			gf_mx2d_add_matrix(&tmp, mat);
			gf_mx2d_copy(*mat, tmp);
			while(str[i] != 0 && str[i] == ' ') i++;
		} else if (strstr(str+i, "skewX")==str+i) {
			Fixed angle;
			i += 5;
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == '(') {
				i++;
				i+=svg_parse_float(&(str[i]), &angle, 1);
				i++;
			}
			gf_mx2d_init(tmp);
			gf_mx2d_add_skew_x(&tmp, angle);
			gf_mx2d_add_matrix(&tmp, mat);
			gf_mx2d_copy(*mat, tmp);
			while(str[i] != 0 && str[i] == ' ') i++;
		} else if (strstr(str+i, "skewY")==str+i) {
			Fixed angle;
			i += 5;
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == '(') {
				i++;
				i+=svg_parse_float(&(str[i]), &angle, 1);
				i++;
			}
			gf_mx2d_init(tmp);
			gf_mx2d_add_skew_y(&tmp, angle);
			gf_mx2d_add_matrix(&tmp, mat);
			gf_mx2d_copy(*mat, tmp);
			while(str[i] != 0 && str[i] == ' ') i++;
		} else if (strstr(str+i, "matrix")==str+i) {
			i+=6;
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == '(') {
				i++;
				i+=svg_parse_float(&(str[i]), &(tmp.m[0]), 0);
				i+=svg_parse_float(&(str[i]), &(tmp.m[3]), 0);
				i+=svg_parse_float(&(str[i]), &(tmp.m[1]), 0);
				i+=svg_parse_float(&(str[i]), &(tmp.m[4]), 0);
				i+=svg_parse_float(&(str[i]), &(tmp.m[2]), 0);
				i+=svg_parse_float(&(str[i]), &(tmp.m[5]), 0);
				i++;
			}
			gf_mx2d_add_matrix(&tmp, mat);
			gf_mx2d_copy(*mat, tmp);
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == ')') i++;
		} 
	}
}

/* Parses an SVG transform attribute and collapses all in the given matrix */
static Bool svg_parse_transform(SVG_Transform *t, char *attribute_content) 
{
	char *str;
	u32 i;
	str = attribute_content;
	i = 0;

	if ((str = strstr(attribute_content, "ref("))) {
		t->is_ref = 1;
		gf_mx2d_init(t->mat);
		str+=3;
		while(str[i] != 0 && str[i] == ' ') i++;
		if (str[i] == 's' && str[i+1] == 'v' && str[i+2] == 'g') {
			i+=3;
			while(str[i] != 0 && str[i] == ' ') i++;					
			if (str[i] == ',') i++;
			else if (str[i] == ')') {
				i++;
				return GF_OK;
			}
			i+=svg_parse_float(&(str[i]), &(t->mat.m[2]), 0);
			i+=svg_parse_float(&(str[i]), &(t->mat.m[5]), 0);
			while(str[i] != 0 && str[i] == ' ') i++;
			if (str[i] == ')')  i++;
			i++;
			return GF_OK;
		} else {
			while(str[i] != 0 && str[i] != ')') i++;
			i++;
			return GF_NOT_SUPPORTED;
		}
		
	} else {
		svg_parse_transformlist(&t->mat, attribute_content);
	}
	return GF_OK;
}

#undef REMOVE_ALLOC

#if USE_GF_PATH    

//#define PARSE_PATH_ONLY

static void svg_parse_path(SVG_PathData *path, char *attribute_content) 
{
	char *d = attribute_content;
	
	/* Point used to start a new subpath when the previous subpath is closed */
	SVG_Point prev_m_pt;
	/* Point used to convert relative 'lower-case commands' into absolute */
	SVG_Point rel_ref_pt;
	/* Points used to convert S, T commands into C, Q */
	SVG_Point orig, ct_orig, ct_end, end;
	/* Used by elliptical arcs */
	Fixed x_axis_rotation, large_arc_flag, sweep_flag;

	char c, prev_c;
	u32 i;

	if (*d == 0) return;

	i = 0;
	prev_c = 'M';
	orig.x = orig.y = ct_orig.x = ct_orig.y = prev_m_pt.x = prev_m_pt.y = rel_ref_pt.x = rel_ref_pt.y = 0;
	while(1) {
		while ( (d[i]==' ') || (d[i] =='\t') || (d[i] =='\r') || (d[i] =='\n') ) i++;			
		c = d[i];
		if (! c) break;
next_command:
		switch (c) {
		case 'm':
		case 'M':
			i++;
			i += svg_parse_float(&(d[i]), &(orig.x), 0);
			i += svg_parse_float(&(d[i]), &(orig.y), 0);				
			if (c == 'm') {
				orig.x += rel_ref_pt.x;
				orig.y += rel_ref_pt.y;
			}
#ifndef PARSE_PATH_ONLY
			gf_path_add_move_to(path, orig.x, orig.y);
#endif
			rel_ref_pt = orig;
			prev_m_pt = orig;
			/*provision for nextCurveTo when no curve is specified:
				"If there is no previous command or if the previous command was not an C, c, S or s, 
				assume the first control point is coincident with the current point.
			*/
			ct_orig = orig;
			prev_c = c;
			break;
		case 'L':
		case 'l':
			i++;
			i += svg_parse_float(&(d[i]), &(orig.x), 0);
			i += svg_parse_float(&(d[i]), &(orig.y), 0);				
			if (c == 'l') {
				orig.x += rel_ref_pt.x;
				orig.y += rel_ref_pt.y;
			}
#ifndef PARSE_PATH_ONLY
			gf_path_add_line_to(path, orig.x, orig.y);
#endif
			rel_ref_pt = orig;
			orig = end;
			/*cf above*/
			ct_orig = orig;
			prev_c = c;
			break;
		case 'H':
		case 'h':
			i++;				
			i += svg_parse_float(&(d[i]), &(orig.x), 0);
			if (c == 'h') {
				orig.x += rel_ref_pt.x;
			}
			orig.y = rel_ref_pt.y;
#ifndef PARSE_PATH_ONLY
			gf_path_add_line_to(path, orig.x, orig.y);			
#endif
			rel_ref_pt.x = orig.x;
			orig = end;
			/*cf above*/
			ct_orig = orig;
			prev_c = c;
			break;
		case 'V':
		case 'v':
			i++;				
			i += svg_parse_float(&(d[i]), &(orig.y), 0);
			if (c == 'v') {
				orig.y += rel_ref_pt.y;
			}
			orig.x = rel_ref_pt.x;
#ifndef PARSE_PATH_ONLY
			gf_path_add_line_to(path, orig.x, orig.y);			
#endif
			rel_ref_pt.y = orig.y;
			orig = end;
			/*cf above*/
			ct_orig = orig;
			prev_c = c;
			break;
		case 'C':
		case 'c':
			i++;				
			i += svg_parse_float(&(d[i]), &(ct_orig.x), 0);
			i += svg_parse_float(&(d[i]), &(ct_orig.y), 0);
			if (c == 'c') {
				ct_orig.x += rel_ref_pt.x;
				ct_orig.y += rel_ref_pt.y;
			}
			i += svg_parse_float(&(d[i]), &(ct_end.x), 0);
			i += svg_parse_float(&(d[i]), &(ct_end.y), 0);
			if (c == 'c') {
				ct_end.x += rel_ref_pt.x;
				ct_end.y += rel_ref_pt.y;
			}
			i += svg_parse_float(&(d[i]), &(end.x), 0);
			i += svg_parse_float(&(d[i]), &(end.y), 0);
			if (c == 'c') {
				end.x += rel_ref_pt.x;
				end.y += rel_ref_pt.y;
			}
#ifndef PARSE_PATH_ONLY
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
#endif
			rel_ref_pt = end;
			ct_orig = ct_end;
			orig = end;
			prev_c = c;
			break;
		case 'S':
		case 's':
			i++;				
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			i += svg_parse_float(&(d[i]), &(ct_end.x), 0);
			i += svg_parse_float(&(d[i]), &(ct_end.y), 0);
			if (c == 's') {
				ct_end.x += rel_ref_pt.x;
				ct_end.y += rel_ref_pt.y;
			}
			i += svg_parse_float(&(d[i]), &(end.x), 0);
			i += svg_parse_float(&(d[i]), &(end.y), 0);
			if (c == 's') {
				end.x += rel_ref_pt.x;
				end.y += rel_ref_pt.y;
			}
#ifndef PARSE_PATH_ONLY
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
#endif
			rel_ref_pt = end;
			ct_orig = ct_end;
			orig = end;
			prev_c = c;
			break;
		case 'Q':
		case 'q':
			i++;				
			i += svg_parse_float(&(d[i]), &(ct_orig.x), 0);
			i += svg_parse_float(&(d[i]), &(ct_orig.y), 0);				
			if (c == 'q') {
				ct_orig.x += rel_ref_pt.x;
				ct_orig.y += rel_ref_pt.y;
			}
			i += svg_parse_float(&(d[i]), &(end.x), 0);
			i += svg_parse_float(&(d[i]), &(end.y), 0);				
			if (c == 'q') {
				end.x += rel_ref_pt.x;
				end.y += rel_ref_pt.y;
			}
#ifndef PARSE_PATH_ONLY
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);			
#endif
			rel_ref_pt = end;
			orig = end;
			prev_c = c;
			break;
		case 'T':
		case 't':
			i++;				
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			i += svg_parse_float(&(d[i]), &(end.x), 0);
			i += svg_parse_float(&(d[i]), &(end.y), 0);				
			if (c == 't') {
				end.x += rel_ref_pt.x;
				end.y += rel_ref_pt.y;
			}
#ifndef PARSE_PATH_ONLY
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);
#endif
			rel_ref_pt = end;
			orig = end;
			prev_c = c;
			break;
		case 'A':
		case 'a':
			i++;				

			i += svg_parse_float(&(d[i]), &(orig.x), 0);	
			i += svg_parse_float(&(d[i]), &(orig.y), 0);				
			if (c == 'a') {
				orig.x += rel_ref_pt.x;
				orig.y += rel_ref_pt.y;
			}

			i += svg_parse_float(&(d[i]), &(x_axis_rotation), 0);	
			i += svg_parse_float(&(d[i]), &(large_arc_flag), 0);				
			i += svg_parse_float(&(d[i]), &(sweep_flag), 0);	
			
			i += svg_parse_float(&(d[i]), &(end.x), 0);	
			i += svg_parse_float(&(d[i]), &(end.y), 0);				
			if (c == 'a') {
				end.x += rel_ref_pt.x;
				end.y += rel_ref_pt.y;
			}
			//gf_path_add_svg_arc_to(path, orig.x, orig.y, x_axis_rotation, large_arc_flag, sweep_flag, end.x, end.y);
			rel_ref_pt = end;
			ct_orig = end;
			prev_c = c;
			break;
		case 'Z':
		case 'z':
			i++;				
#ifndef PARSE_PATH_ONLY
			gf_path_close(path);
#endif
			prev_c = c;
			rel_ref_pt = prev_m_pt;
			break;
		default:
			i--;
			switch (prev_c) {
			case 'M':
				c = 'L';
				break;
			case 'm':
				c = 'l';
				break;
			default:
				c = prev_c;
			}
			goto next_command;
		}
	}
}
#else
/* TODO: Change the function to handle elliptical arcs, requires changing data structure */
static void svg_parse_path(SVG_PathData *d_attribute, char *attribute_content) 
{
	GF_List *d_commands = d_attribute->commands;
	GF_List *d_points = d_attribute->points;
	char *d = attribute_content;

	if (strlen(d)) {
		SVG_Point *pt, cur_pt, prev_m_pt;
		u8 *command;
		u32 i, k;
		char c, prev_c = 'M';
#ifdef REMOVE_ALLOC
		GF_SAFEALLOC(pt, SVG_Point)
#endif
		i = 0;
		cur_pt.x = cur_pt.y = 0;
		prev_m_pt.x = prev_m_pt.y = 0;
		while(1) {
			while ( (d[i]==' ') || (d[i] =='\t') ) i++;			
			c = d[i];
			if (! c) break;
next_command:
			switch (c) {
			case 'm':
			case 'M':
				i++;
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_M;

				GF_SAFEALLOC(pt, SVG_Point)
				gf_list_add(d_points, pt);
#endif
				i += svg_parse_float(&(d[i]), &(pt->x), 0);
				i += svg_parse_float(&(d[i]), &(pt->y), 0);				
				if (c == 'm') {
					pt->x += cur_pt.x;
					pt->y += cur_pt.y;
				}
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				prev_m_pt = cur_pt;
				prev_c = c;
				break;
			case 'L':
			case 'l':
				i++;
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_L;
				
				GF_SAFEALLOC(pt, SVG_Point)
				gf_list_add(d_points, pt);
#endif
				i += svg_parse_float(&(d[i]), &(pt->x), 0);
				i += svg_parse_float(&(d[i]), &(pt->y), 0);				
				if (c == 'l') {
					pt->x += cur_pt.x;
					pt->y += cur_pt.y;
				}
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				prev_c = c;
				break;
			case 'H':
			case 'h':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_L;

				GF_SAFEALLOC(pt, SVG_Point)
				gf_list_add(d_points, pt);
#endif
				i += svg_parse_float(&(d[i]), &(pt->x), 0);
				if (c == 'h') {
					pt->x += cur_pt.x;
				}
				pt->y = cur_pt.y;
				cur_pt.x = pt->x;
				prev_c = c;
				break;
			case 'V':
			case 'v':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_L;

				GF_SAFEALLOC(pt, SVG_Point)
				gf_list_add(d_points, pt);
#endif
				i += svg_parse_float(&(d[i]), &(pt->y), 0);
				if (c == 'v') {
					pt->y += cur_pt.y;
				}
				pt->x = cur_pt.x;
				cur_pt.y = pt->y;
				prev_c = c;
				break;
			case 'C':
			case 'c':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_C;
#endif
				
				for (k=0; k<3; k++) {
#ifndef REMOVE_ALLOC
					GF_SAFEALLOC(pt, SVG_Point)
					gf_list_add(d_points, pt);
#endif
					i += svg_parse_float(&(d[i]), &(pt->x), 0);
					i += svg_parse_float(&(d[i]), &(pt->y), 0);				
					if (c == 'c') {
						pt->x += cur_pt.x;
						pt->y += cur_pt.y;
					}
				}				
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				prev_c = c;
				break;
			case 'S':
			case 's':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_S;
#endif
				
				for (k=0; k<2; k++) {
#ifndef REMOVE_ALLOC
					GF_SAFEALLOC(pt, SVG_Point)
					gf_list_add(d_points, pt);
#endif
					i += svg_parse_float(&(d[i]), &(pt->x), 0);
					i += svg_parse_float(&(d[i]), &(pt->y), 0);				
					if (c == 's') {
						pt->x += cur_pt.x;
						pt->y += cur_pt.y;
					}
				}				
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				prev_c = c;
				break;
			case 'Q':
			case 'q':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_Q;
#endif
				
				for (k=0; k<2; k++) {
#ifndef REMOVE_ALLOC
					GF_SAFEALLOC(pt, SVG_Point)
					gf_list_add(d_points, pt);
#endif
					i += svg_parse_float(&(d[i]), &(pt->x), 0);
					i += svg_parse_float(&(d[i]), &(pt->y), 0);				
					if (c == 'q') {
						pt->x += cur_pt.x;
						pt->y += cur_pt.y;
					}
				}				
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				prev_c = c;
				break;
			case 'T':
			case 't':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_T;
				
				GF_SAFEALLOC(pt, SVG_Point)
				gf_list_add(d_points, pt);
#endif
				i += svg_parse_float(&(d[i]), &(pt->x), 0);
				i += svg_parse_float(&(d[i]), &(pt->y), 0);				
				if (c == 't') {
					pt->x += cur_pt.x;
					pt->y += cur_pt.y;
				}
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				prev_c = c;
				break;
			case 'A':
			case 'a':
				{
					Fixed tmp;
					i++;				
#ifndef REMOVE_ALLOC
					GF_SAFEALLOC(command, u8)
					gf_list_add(d_commands, command);
					*command = SVG_PATHCOMMAND_A;
	
					GF_SAFEALLOC(pt, SVG_Point)
					gf_list_add(d_points, pt);
#endif
					i += svg_parse_float(&(d[i]), &(pt->x), 0);	
					i += svg_parse_float(&(d[i]), &(pt->y), 0);				

					i += svg_parse_float(&(d[i]), &(tmp), 0);	
					i += svg_parse_float(&(d[i]), &(tmp), 0);				
					i += svg_parse_float(&(d[i]), &(tmp), 0);	
					
#ifndef REMOVE_ALLOC
					GF_SAFEALLOC(pt, SVG_Point)
					gf_list_add(d_points, pt);
#endif
					i += svg_parse_float(&(d[i]), &(pt->x), 0);	
					i += svg_parse_float(&(d[i]), &(pt->y), 0);				
					if (c == 'a') {
						pt->x += cur_pt.x;
						pt->y += cur_pt.y;
					}
					cur_pt.x = pt->x;
					cur_pt.y = pt->y;
				}
				prev_c = c;
				break;
			case 'Z':
			case 'z':
				i++;				
#ifndef REMOVE_ALLOC
				GF_SAFEALLOC(command, u8)
				gf_list_add(d_commands, command);
				*command = SVG_PATHCOMMAND_Z;
#endif
				prev_c = c;
				cur_pt = prev_m_pt;
				break;
			default:
				i--;
				switch (prev_c) {
				case 'M':
					c = 'L';
					break;
				case 'm':
					c = 'l';
					break;
				default:
					c = prev_c;
				}
				goto next_command;
			}
		}
	}
}
#endif

static void svg_parse_iri(GF_Node *elt, XMLRI *iri, char *attribute_content)
{
	/* TODO: Handle xpointer(id()) syntax */
	if (attribute_content[0] == '#') {
		iri->string = strdup(attribute_content);
		iri->target = gf_sg_find_node_by_name(elt->sgprivate->scenegraph, attribute_content + 1);
		if (!iri->target) {
			iri->type = XMLRI_STRING;
		} else {
			iri->type = XMLRI_ELEMENTID;
			gf_svg_register_iri(elt->sgprivate->scenegraph, iri);
		}
	} else {
		iri->type = XMLRI_STRING;
		iri->string = strdup(attribute_content);
	}
}

static void svg_parse_idref(GF_Node *elt, XML_IDREF *iri, char *attribute_content)
{
	iri->type = XMLRI_ELEMENTID;
	iri->target = gf_sg_find_node_by_name(elt->sgprivate->scenegraph, attribute_content);
	if (!iri->target) {
		iri->string = strdup(attribute_content);
	} else {
		gf_svg_register_iri(elt->sgprivate->scenegraph, iri);
	}
}

/* Parses a paint attribute: none, inherit or color */
static void svg_parse_paint(GF_Node *n, SVG_Paint *paint, char *attribute_content)
{
	if (!strcmp(attribute_content, "none")) {
		paint->type = SVG_PAINT_NONE;
	} else if (!strcmp(attribute_content, "inherit")) {
		paint->type = SVG_PAINT_INHERIT;
	} else if (!strncmp(attribute_content, "url(", 4) ) {
		u32 len = strlen(attribute_content);
		paint->type = SVG_PAINT_URI;
		attribute_content[len-1] = 0;
		svg_parse_iri(n, &paint->iri, attribute_content+4);
		attribute_content[len-1] = ')';
	} else {
		paint->type = SVG_PAINT_COLOR;
		svg_parse_color(&paint->color, attribute_content);
	}
}


static u32 svg_parse_number(SVG_Number *number, char *value_string, Bool clamp0to1)
{
	char *unit = NULL;
	u32 len = 0;
	if (!strcmp(value_string, "inherit")) {
		number->type = SVG_NUMBER_INHERIT;
		return 7;
	} else if (!strcmp(value_string, "auto")) {
		number->type = SVG_NUMBER_AUTO;
		return 4;
	} else if (!strcmp(value_string, "auto-reverse")) {
		number->type = SVG_NUMBER_AUTO_REVERSE;
		return 12;
	} else if ((unit = strstr(value_string, "%")) ) {
		number->type = SVG_NUMBER_PERCENTAGE;
	} else if ((unit = strstr(value_string, "em"))) {
		number->type = SVG_NUMBER_EMS;
	} else if ((unit = strstr(value_string, "ex"))) {
		number->type = SVG_NUMBER_EXS;
	} else if ((unit = strstr(value_string, "px"))) {
		number->type = SVG_NUMBER_PX;
	} else if ((unit = strstr(value_string, "cm"))) {
		number->type = SVG_NUMBER_CM;
	} else if ((unit = strstr(value_string, "mm"))) {
		number->type = SVG_NUMBER_MM;
	} else if ((unit = strstr(value_string, "in"))) {
		number->type = SVG_NUMBER_IN;
	} else if ((unit = strstr(value_string, "pt"))) {
		number->type = SVG_NUMBER_PT;
	} else if ((unit = strstr(value_string, "pc"))) {
		number->type = SVG_NUMBER_PC;
	} else {
		number->type = SVG_NUMBER_VALUE;
	}
	if (unit) len = strlen(unit); 
	len+=svg_parse_float(value_string, &(number->value), 0);

	if (clamp0to1) number->value = MAX(0, MIN(1, number->value));
	return len;
}

static void svg_parse_visibility(SVG_Visibility *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_VISIBILITY_INHERIT;
	} else if (!strcmp(value_string, "visible")) {
		*value = SVG_VISIBILITY_VISIBLE;
	} else if (!strcmp(value_string, "hidden")) {
		*value = SVG_VISIBILITY_HIDDEN;
	} else if (!strcmp(value_string, "collapse")) {
		*value = SVG_VISIBILITY_COLLAPSE;
	} 
}

static void svg_parse_display(SVG_Display *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_DISPLAY_INHERIT;
	} else if (!strcmp(value_string, "none")) {
		*value = SVG_DISPLAY_NONE;
	} else if (!strcmp(value_string, "inline")) {
		*value = SVG_DISPLAY_INLINE;
	} else if (!strcmp(value_string, "block")) {
		*value = SVG_DISPLAY_BLOCK;
	} else if (!strcmp(value_string, "list-item")) {
		*value = SVG_DISPLAY_LIST_ITEM;
	} else if (!strcmp(value_string, "run-in")) {
		*value = SVG_DISPLAY_RUN_IN;
	} else if (!strcmp(value_string, "compact")) {
		*value = SVG_DISPLAY_COMPACT;
	} else if (!strcmp(value_string, "marker")) {
		*value = SVG_DISPLAY_MARKER;
	} else if (!strcmp(value_string, "table")) {
		*value = SVG_DISPLAY_TABLE;
	} else if (!strcmp(value_string, "inline-table")) {
		*value = SVG_DISPLAY_INLINE_TABLE;
	} else if (!strcmp(value_string, "table-row-group")) {
		*value = SVG_DISPLAY_TABLE_ROW_GROUP;
	} else if (!strcmp(value_string, "table-header-group")) {
		*value = SVG_DISPLAY_TABLE_HEADER_GROUP;
	} else if (!strcmp(value_string, "table-footer-group")) {
		*value = SVG_DISPLAY_TABLE_FOOTER_GROUP;
	} else if (!strcmp(value_string, "table-row")) {
		*value = SVG_DISPLAY_TABLE_ROW;
	} else if (!strcmp(value_string, "table-column-group")) {
		*value = SVG_DISPLAY_TABLE_COLUMN_GROUP;
	} else if (!strcmp(value_string, "table-column")) {
		*value = SVG_DISPLAY_TABLE_COLUMN;
	} else if (!strcmp(value_string, "table-cell")) {
		*value = SVG_DISPLAY_TABLE_CELL;
	} else if (!strcmp(value_string, "table-caption")) {
		*value = SVG_DISPLAY_TABLE_CAPTION;
	} 
}

static void svg_parse_displayalign(SVG_DisplayAlign *value, char *value_string)
{ 
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_DISPLAYALIGN_INHERIT;
	} else if (!strcmp(value_string, "auto")) {
		*value = SVG_DISPLAYALIGN_AUTO;
	} else if (!strcmp(value_string, "before")) {
		*value = SVG_DISPLAYALIGN_BEFORE;
	} else if (!strcmp(value_string, "center")) {
		*value = SVG_DISPLAYALIGN_CENTER;
	} else if (!strcmp(value_string, "after")) {
		*value = SVG_DISPLAYALIGN_AFTER;
	}
}

static void svg_parse_textalign(SVG_TextAlign *value, char *value_string)
{ 
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_TEXTALIGN_INHERIT;
	} else if (!strcmp(value_string, "start")) {
		*value = SVG_TEXTALIGN_START;
	} else if (!strcmp(value_string, "center")) {
		*value = SVG_TEXTALIGN_CENTER;
	} else if (!strcmp(value_string, "end")) {
		*value = SVG_TEXTALIGN_END;
	} 
}

static void svg_parse_pointerevents(SVG_PointerEvents *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_POINTEREVENTS_INHERIT;
	} else if (!strcmp(value_string, "visiblePainted")) {
		*value = SVG_POINTEREVENTS_VISIBLEPAINTED;
	} else if (!strcmp(value_string, "visibleFill")) {
		*value = SVG_POINTEREVENTS_VISIBLEFILL;
	} else if (!strcmp(value_string, "visibleStroke")) {
		*value = SVG_POINTEREVENTS_VISIBLESTROKE;
	} else if (!strcmp(value_string, "visible")) {
		*value = SVG_POINTEREVENTS_VISIBLE;
	} else if (!strcmp(value_string, "painted")) {
		*value = SVG_POINTEREVENTS_PAINTED;
	} else if (!strcmp(value_string, "fill")) {
		*value = SVG_POINTEREVENTS_FILL;
	} else if (!strcmp(value_string, "stroke")) {
		*value = SVG_POINTEREVENTS_STROKE;
	} else if (!strcmp(value_string, "all")) {
		*value = SVG_POINTEREVENTS_ALL;
	} else if (!strcmp(value_string, "boundingBox")) {
		*value = SVG_POINTEREVENTS_BOUNDINGBOX;
	} else if (!strcmp(value_string, "none")) {
		*value = SVG_POINTEREVENTS_NONE;
	}
}

static void svg_parse_renderinghint(SVG_RenderingHint *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_RENDERINGHINT_INHERIT;
	} else if (!strcmp(value_string, "auto")) {
		*value = SVG_RENDERINGHINT_AUTO;
	} else if (!strcmp(value_string, "optimizeQuality")) {
		*value = SVG_RENDERINGHINT_OPTIMIZEQUALITY;
	} else if (!strcmp(value_string, "optimizeSpeed")) {
		*value = SVG_RENDERINGHINT_OPTIMIZESPEED;
	} else if (!strcmp(value_string, "optimizeLegibility")) {
		*value = SVG_RENDERINGHINT_OPTIMIZELEGIBILITY;
	} else if (!strcmp(value_string, "crispEdges")) {
		*value = SVG_RENDERINGHINT_CRISPEDGES;
	} else if (!strcmp(value_string, "geometricPrecision")) {
		*value = SVG_RENDERINGHINT_GEOMETRICPRECISION;
	}
}

static void svg_parse_vectoreffect(SVG_VectorEffect *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_VECTOREFFECT_INHERIT;
	} else if (!strcmp(value_string, "none")) {
		*value = SVG_VECTOREFFECT_NONE;
	} else if (!strcmp(value_string, "non-scaling-stroke")) {
		*value = SVG_VECTOREFFECT_NONSCALINGSTROKE;
	}
}

static void svg_parse_playbackorder(SVG_VectorEffect *value, char *value_string)
{
	if (!strcmp(value_string, "forwardOnly")) {
		*value = SVG_PLAYBACKORDER_FORWARDONLY;
	} else if (!strcmp(value_string, "all")) {
		*value = SVG_PLAYBACKORDER_ALL;
	}
}

static void svg_parse_timelinebegin(SVG_TimelineBegin *value, char *value_string)
{
	if (!strcmp(value_string, "onStart")) {
		*value = SVG_TIMELINEBEGIN_ONSTART;
	} else if (!strcmp(value_string, "onLoad")) {
		*value = SVG_TIMELINEBEGIN_ONLOAD;
	}
}

static void svg_parse_xmlspace(XML_Space *value, char *value_string)
{
	if (!strcmp(value_string, "default")) {
		*value = XML_SPACE_DEFAULT;
	} else if (!strcmp(value_string, "preserve")) {
		*value = XML_SPACE_PRESERVE;
	}
}

static void svg_parse_xmlev_propagate(XMLEV_Propagate *value, char *value_string)
{
	if (!strcmp(value_string, "continue")) {
		*value = XMLEVENT_PROPAGATE_CONTINUE;
	} else if (!strcmp(value_string, "stop")) {
		*value = XMLEVENT_PROPAGATE_STOP;
	}
}

static void svg_parse_xmlev_defaultAction(XMLEV_DefaultAction *value, char *value_string)
{
	if (!strcmp(value_string, "cancel")) {
		*value = XMLEVENT_DEFAULTACTION_CANCEL;
	} else if (!strcmp(value_string, "perform")) {
		*value = XMLEVENT_DEFAULTACTION_PERFORM;
	}
}

static void svg_parse_xmlev_phase(XMLEV_Phase *value, char *value_string)
{
	if (!strcmp(value_string, "default")) {
		*value = XMLEVENT_PHASE_DEFAULT;
	} else if (!strcmp(value_string, "capture")) {
		*value = XMLEVENT_PHASE_CAPTURE;
	}
}

static void svg_parse_overflow(SVG_Overflow *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_OVERFLOW_INHERIT;
	} else if (!strcmp(value_string, "visible")) {
		*value = SVG_OVERFLOW_VISIBLE;
	} else if (!strcmp(value_string, "hidden")) {
		*value = SVG_OVERFLOW_HIDDEN;
	} else if (!strcmp(value_string, "scroll")) {
		*value = SVG_OVERFLOW_SCROLL;
	} else if (!strcmp(value_string, "auto")) {
		*value = SVG_OVERFLOW_AUTO;
	}
}

static void svg_parse_textanchor(SVG_TextAnchor *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_TEXTANCHOR_INHERIT;
	} else if (!strcmp(value_string, "start")) {
		*value = SVG_TEXTANCHOR_START;
	} else if (!strcmp(value_string, "middle")) {
		*value = SVG_TEXTANCHOR_MIDDLE;
	} else if (!strcmp(value_string, "end")) {
		*value = SVG_TEXTANCHOR_END;
	}
}

static void svg_parse_clipfillrule(SVG_FillRule *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_FILLRULE_INHERIT;
	} else if (!strcmp(value_string, "nonzero")) {
		*value = SVG_FILLRULE_NONZERO;
	} else if (!strcmp(value_string, "evenodd")) {
		*value = SVG_FILLRULE_EVENODD;
	} 
}

static void svg_parse_strokelinejoin(SVG_StrokeLineJoin *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_STROKELINEJOIN_INHERIT;
	} else if (!strcmp(value_string, "miter")) {
		*value = SVG_STROKELINEJOIN_MITER;
	} else if (!strcmp(value_string, "round")) {
		*value = SVG_STROKELINEJOIN_ROUND;
	} else if (!strcmp(value_string, "bevel")) {
		*value = SVG_STROKELINEJOIN_BEVEL;
	} 
}

static void svg_parse_strokelinecap(SVG_StrokeLineCap *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_STROKELINECAP_INHERIT;
	} else if (!strcmp(value_string, "butt")) {
		*value = SVG_STROKELINECAP_BUTT;
	} else if (!strcmp(value_string, "round")) {
		*value = SVG_STROKELINECAP_ROUND;
	} else if (!strcmp(value_string, "square")) {
		*value = SVG_STROKELINECAP_SQUARE;
	} 
}

static void svg_parse_fontfamily(SVG_FontFamily *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		value->type = SVG_FONTFAMILY_INHERIT;
	} else {
		value->type = SVG_FONTFAMILY_VALUE;
		value->value = strdup(value_string);
	}
}

static void svg_parse_fontstyle(SVG_FontStyle *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_FONTSTYLE_INHERIT;
	} else if (!strcmp(value_string, "normal")) {
		*value = SVG_FONTSTYLE_NORMAL;
	} else if (!strcmp(value_string, "italic")) {
		*value = SVG_FONTSTYLE_ITALIC;
	} else if (!strcmp(value_string, "oblique")) {
		*value = SVG_FONTSTYLE_OBLIQUE;
	} 
}

static void svg_parse_fontweight(SVG_FontWeight *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_FONTWEIGHT_INHERIT;
	} else if (!strcmp(value_string, "normal")) {
		*value = SVG_FONTWEIGHT_NORMAL;
	} else if (!strcmp(value_string, "bold")) {
		*value = SVG_FONTWEIGHT_BOLD;
	} else if (!strcmp(value_string, "bolder")) {
		*value = SVG_FONTWEIGHT_BOLDER;
	} else if (!strcmp(value_string, "lighter")) {
		*value = SVG_FONTWEIGHT_LIGHTER;
	} else if (!strcmp(value_string, "100")) {
		*value = SVG_FONTWEIGHT_100;
	} else if (!strcmp(value_string, "200")) {
		*value = SVG_FONTWEIGHT_200;
	} else if (!strcmp(value_string, "300")) {
		*value = SVG_FONTWEIGHT_300;
	} else if (!strcmp(value_string, "400")) {
		*value = SVG_FONTWEIGHT_400;
	} else if (!strcmp(value_string, "500")) {
		*value = SVG_FONTWEIGHT_500;
	} else if (!strcmp(value_string, "600")) {
		*value = SVG_FONTWEIGHT_600;
	} else if (!strcmp(value_string, "700")) {
		*value = SVG_FONTWEIGHT_700;
	} else if (!strcmp(value_string, "800")) {
		*value = SVG_FONTWEIGHT_800;
	} else if (!strcmp(value_string, "900")) {
		*value = SVG_FONTWEIGHT_900;
	} 
}

static void svg_parse_fontvariant(SVG_FontVariant *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_FONTVARIANT_INHERIT;
	} else if (!strcmp(value_string, "normal")) {
		*value = SVG_FONTVARIANT_NORMAL;
	} else if (!strcmp(value_string, "small-caps")) {
		*value = SVG_FONTVARIANT_SMALLCAPS;
	} 
}

static void svg_parse_boolean(SVG_Boolean *value, char *value_string)
{
	if (!strcmp(value_string, "1") || !strcmp(value_string, "true"))
		*value = 1;
	else
		*value = 0;
}


static void smil_parse_time_list(GF_Node *e, GF_List *values, char *begin_or_end_list)
{
	SMIL_Time *value;
	char value_string[500];
	char *str = begin_or_end_list, *tmp;
	u32 len;

	/* get rid of leading spaces */
	while (*str == ' ') str++;

	while ((tmp = strchr(str, ';'))) {
		len = tmp-str;
		memcpy(value_string, str, len);
		while (value_string[len - 1] == ' ' && len > 0) len--;
		value_string[len] = 0;

		GF_SAFEALLOC(value, SMIL_Time)
		smil_parse_time(e, value, value_string);
 		gf_list_add(values, value);

		str = tmp + 1;
		while (*str == ' ') str++;
	}

	len = strlen(str);
	memcpy(value_string, str, len);
	while (value_string[len - 1] == ' ' && len > 0) len--;
	value_string[len] = 0;

	GF_SAFEALLOC(value, SMIL_Time)
	smil_parse_time(e, value, value_string);
 	gf_list_add(values, value);

	/* sorting timing values */
	if (gf_list_count(values) > 1) {
		SMIL_Time *v, *sv;
		GF_List *sorted = gf_list_new();
		u32 i, count;
		u8 added = 0;
		do {
			v = (SMIL_Time*)gf_list_get(values, 0);
			gf_list_rem(values, 0);
			added = 0;
			count = gf_list_count(sorted);
			for (i=0; i<count; i++) {
				sv = (SMIL_Time*)gf_list_get(sorted, i);
				if (v->type >= GF_SMIL_TIME_EVENT) {
					/* unresolved or indefinite so add at the end of the sorted list */
					gf_list_add(sorted, v);
					added = 1;
					break;
				} else {
					if (sv->type >= GF_SMIL_TIME_EVENT) {
						gf_list_insert(sorted, v, i);
						added = 1;
						break;
					} else {
						if (v->clock <= sv->clock) {
							gf_list_insert(sorted, v, i);
							added = 1;
							break;
						}
					}
				}
			}
			if (!added) gf_list_add(sorted, v);
		} while (gf_list_count(values) > 0);

		count = gf_list_count(sorted);
		for (i = 0; i < count; i++) {
			gf_list_add(values, gf_list_get(sorted, i));
		}
		gf_list_del(sorted);
	}
}

static void smil_parse_attributeType(SMIL_AttributeType *value, char *value_string)
{
	if (!strcmp(value_string, "auto")) {
		*value = SMIL_ATTRIBUTETYPE_AUTO;
	} else if (!strcmp(value_string, "XML")) {
		*value = SMIL_ATTRIBUTETYPE_XML;
	} else if (!strcmp(value_string, "CSS")) {
		*value = SMIL_ATTRIBUTETYPE_CSS;
	}
}

static void smil_parse_min_max_dur_repeatdur(SMIL_Duration *value, char *value_string)
{
	if (!strcmp(value_string, "indefinite")) {
		value->type = SMIL_DURATION_INDEFINITE;
	} else if (!strcmp(value_string, "media")) {
		value->type = SMIL_DURATION_MEDIA;
	} else {
		Double ftime;
		svg_parse_clock_value(value_string, &ftime);
		value->clock_value = ftime;
		value->type = SMIL_DURATION_DEFINED;
	}
}

static void smil_parse_repeatcount(SMIL_RepeatCount *value, char *value_string)
{
	if (!strcmp(value_string, "indefinite")) {
		value->type = SMIL_REPEATCOUNT_INDEFINITE;
	} else {
		Float _val;
		sscanf(value_string, "%f", &_val);
		value->type = SMIL_REPEATCOUNT_DEFINED;
		value->count = FLT2FIX(_val);
	}
}

static void smil_parse_fill(SMIL_Fill *value, char *value_string)
{
	if (!strcmp(value_string, "freeze")) {
		*value = SMIL_FILL_FREEZE;
	} else if (!strcmp(value_string, "remove")) {
		*value = SMIL_FILL_REMOVE;
	}
}

static void smil_parse_restart(SMIL_Restart *value, char *value_string)
{
	if (!strcmp(value_string, "always")) {
		*value = SMIL_RESTART_ALWAYS;
	} else if (!strcmp(value_string, "whenNotActive")) {
		*value = SMIL_RESTART_WHENNOTACTIVE;
	} else if (!strcmp(value_string, "never")) {
		*value = SMIL_RESTART_NEVER;
	}
}

static void smil_parse_calcmode(SMIL_CalcMode *value, char *value_string)
{
	if (!strcmp(value_string, "discrete")) {
		*value = SMIL_CALCMODE_DISCRETE;
	} else if (!strcmp(value_string, "linear")) {
		*value = SMIL_CALCMODE_LINEAR;
	} else if (!strcmp(value_string, "paced")) {
		*value = SMIL_CALCMODE_PACED;
	} else if (!strcmp(value_string, "spline")) {
		*value = SMIL_CALCMODE_SPLINE;
	} 
}

static void smil_parse_additive(SMIL_Additive *value, char *value_string)
{
	if (!strcmp(value_string, "replace")) {
		*value = SMIL_ADDITIVE_REPLACE;
	} else if (!strcmp(value_string, "sum")) {
		*value = SMIL_ADDITIVE_SUM;
	} 
}

static void smil_parse_accumulate(SMIL_Accumulate *value, char *value_string)
{
	if (!strcmp(value_string, "none")) {
		*value = SMIL_ACCUMULATE_NONE;
	} else if (!strcmp(value_string, "sum")) {
		*value = SMIL_ACCUMULATE_SUM;
	} 
}

static void smil_parse_syncBehaviorOrDefault(SMIL_SyncBehavior *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SMIL_SYNCBEHAVIOR_INHERIT;
	} else if (!strcmp(value_string, "default")) {
		*value = SMIL_SYNCBEHAVIOR_DEFAULT;
	} else if (!strcmp(value_string, "locked")) {
		*value = SMIL_SYNCBEHAVIOR_LOCKED;
	} else if (!strcmp(value_string, "canSlip")) {
		*value = SMIL_SYNCBEHAVIOR_CANSLIP;
	} else if (!strcmp(value_string, "independent")) {
		*value = SMIL_SYNCBEHAVIOR_INDEPENDENT;
	} 
}

static void smil_parse_syncToleranceOrDefault(SMIL_SyncTolerance *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		value->type = SMIL_SYNCTOLERANCE_INHERIT;
	} else if (!strcmp(value_string, "default")) {
		value->type = SMIL_SYNCTOLERANCE_DEFAULT;
	} else {
		value->type = SMIL_SYNCBEHAVIOR_LOCKED;
		svg_parse_clock_value(value_string, &(value->value));
	}
}

static void svg_parse_viewbox(SVG_ViewBox *value, char *value_string)
{
	char *str = value_string;
	if (!strcmp(str, "none")) {
		value->is_set = 0;
	} else {
		u32 i = 0;
		value->is_set = 1;
		i+=svg_parse_float(&(str[i]), &(value->x), 0);
		i+=svg_parse_float(&(str[i]), &(value->y), 0);
		i+=svg_parse_float(&(str[i]), &(value->width), 0);
		i+=svg_parse_float(&(str[i]), &(value->height), 0);
	}
}

static void svg_parse_coordinates(GF_List *values, char *value_string)
{
	SVG_Coordinate *c;
	u32 i = 0;
	char *str = value_string;
	u32 len = strlen(str);

	while (gf_list_count(values)) {
		c = (SVG_Coordinate*)gf_list_get(values, 0);
		gf_list_rem(values, 0);
		free(c);
	}
	while (i < len) {
		GF_SAFEALLOC(c, SVG_Coordinate)
		i+=svg_parse_number(c, &(str[i]), 0);
		gf_list_add(values, c);
	}
}

u32 svg_parse_point(SVG_Point *p, char *value_string)
{
	u32 i = 0;
	i+=svg_parse_float(&(value_string[i]), &(p->x), 0);
	i+=svg_parse_float(&(value_string[i]), &(p->y), 0);
	return i;
}

static u32 svg_parse_point_into_matrix(GF_Matrix2D *p, char *value_string)
{
	u32 i = 0;
	gf_mx2d_init(*p);
	i+=svg_parse_float(&(value_string[i]), &(p->m[2]), 0);
	i+=svg_parse_float(&(value_string[i]), &(p->m[5]), 0);
	return i;
}

static void svg_parse_points(GF_List *values, char *value_string)
{
	u32 i = 0;
	char *str = value_string;
	u32 len = strlen(str);
	while (i < len) {
		SVG_Point *p;
		GF_SAFEALLOC(p, SVG_Point)
		i += svg_parse_point(p, &str[i]);
		gf_list_add(values, p);
	}
}

static void svg_parse_floats(GF_List *values, char *value_string, Bool is_angle)
{
	u32 i = 0;
	char *str = value_string;
	u32 len = strlen(str);
	while (i < len) {
		Fixed *f;
		GF_SAFEALLOC(f, Fixed)
		i+=svg_parse_float(&(str[i]), f, is_angle);
		gf_list_add(values, f);
	}
}

static void svg_string_list_add(GF_List *values, char *string, u32 string_type)
{
	XMLRI *iri;
	switch (string_type) {
	case 1:
		iri = (XMLRI*)malloc(sizeof(XMLRI));
		iri->type = XMLRI_STRING;
		iri->string = strdup(string);
		gf_list_add(values, iri);
		break;
	default:
		gf_list_add(values, strdup(string));
		break;
	}
}

static void svg_parse_strings(GF_List *values, char *value_string, u32 string_type)
{
	char *next, *sep = value_string;

	while (gf_list_count(values)) {
		next = (char*)gf_list_last(values);
		gf_list_rem_last(values);
		free(next);
	}
	
	while (1) {
		while (sep && sep[0]==' ') sep++;
		if (!sep) break;
		next = strchr(sep, ';');
		if (!next) {
			svg_string_list_add(values, sep, string_type);
			break;
		}
		next[0]=0;
		svg_string_list_add(values, sep, string_type);
		next[0]=';';
		sep = next+1;
	}
}

static void svg_parse_strokedasharray(SVG_StrokeDashArray *value, char *value_string)
{
	if (!strcmp(value_string, "none")) {
		value->type = SVG_STROKEDASHARRAY_NONE;
	} else if (!strcmp(value_string, "inherit")) {
		value->type = SVG_STROKEDASHARRAY_INHERIT;
	} else {
		Array *vals = &(value->array);
		GF_List *values = gf_list_new();
		u32 i = 0;
		u32 len = strlen(value_string);
		char *str = value_string;
		while (i < len) {
			Fixed *f;
			GF_SAFEALLOC(f, Fixed)
			i+=svg_parse_float(&(str[i]), f, 0);
			gf_list_add(values, f);
		}
		vals->count = gf_list_count(values);
		vals->vals = (Fixed *) malloc(sizeof(Fixed)*vals->count);
		for (i = 0; i < vals->count; i++) {
			Fixed *f = (Fixed *)gf_list_get(values, i);
			vals->vals[i] = *f;
			free(f);
		}
		gf_list_del(values);
		value->type = SVG_STROKEDASHARRAY_ARRAY;
	}
}

static void svg_parse_zoomandpan(SVG_ZoomAndPan *value, char *value_string)
{
	if (!strcmp(value_string, "disable")) {
		*value = SVG_ZOOMANDPAN_DISABLE;
	} else if (!strcmp(value_string, "magnify")) {
		*value = SVG_ZOOMANDPAN_MAGNIFY;
	} 
}

static void svg_parse_preserveaspectratio(SVG_PreserveAspectRatio *par, char *attribute_content)
{
	char *content = attribute_content;
	while (*content == ' ') content++;
	if (strstr(content, "defer")) {
		par->defer = 1;
		content += 4;
	} else {
		content = attribute_content;
	}
	while (*content == ' ') content++;
	if (strstr(content, "none")) {
		par->align = SVG_PRESERVEASPECTRATIO_NONE;
		content+=4;
	} else if (strstr(content, "xMinYMin")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMINYMIN;
		content+=8;
	} else if (strstr(content, "xMidYMin")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMIDYMIN;
		content+=8;
	} else if (strstr(content, "xMaxYMin")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMAXYMIN;
		content+=8;
	} else if (strstr(content, "xMinYMid")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMINYMID;
		content+=8;
	} else if (strstr(content, "xMidYMid")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
		content+=8;
	} else if (strstr(content, "xMaxYMid")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMAXYMID;
		content+=8;
	} else if (strstr(content, "xMinYMax")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMINYMAX;
		content+=8;
	} else if (strstr(content, "xMidYMax")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMIDYMAX;
		content+=8;
	} else if (strstr(content, "xMaxYMax")) {
		par->align = SVG_PRESERVEASPECTRATIO_XMAXYMAX;
		content+=8;
	}
	while (*content == ' ') content++;
	if (*content == 0) return;
	if (strstr(content, "meet")) {
		par->meetOrSlice = SVG_MEETORSLICE_MEET;
	} else if (strstr(content, "slice")) {
		par->meetOrSlice = SVG_MEETORSLICE_SLICE;
	}
}

static void svg_parse_animatetransform_type(SVG_TransformType *anim_transform_type, char *attribute_content)
{
	*anim_transform_type = SVG_TRANSFORM_MATRIX;
	if (!strcmp(attribute_content, "scale")) {
		*anim_transform_type = SVG_TRANSFORM_SCALE;
	} else if (!strcmp(attribute_content, "rotate")) {
		*anim_transform_type = SVG_TRANSFORM_ROTATE;
	} else if (!strcmp(attribute_content, "translate")) {
		*anim_transform_type = SVG_TRANSFORM_TRANSLATE;
	} else if (!strcmp(attribute_content, "skewX")) {
		*anim_transform_type = SVG_TRANSFORM_SKEWX;
	} else if (!strcmp(attribute_content, "skewY")) {
		*anim_transform_type = SVG_TRANSFORM_SKEWY;
	}
}

static void svg_parse_focushighlight(SVG_FocusHighlight *fh, char *attribute_content)
{
	if (!strcmp(attribute_content, "auto")) {
		*fh = SVG_FOCUSHIGHLIGHT_AUTO;
	} else if (!strcmp(attribute_content, "none")) {
		*fh = SVG_FOCUSHIGHLIGHT_NONE;
	}
}

static void svg_parse_focusable(SVG_Focusable *f, char *attribute_content)
{
	if (!strcmp(attribute_content, "true")) {
		*f = SVG_FOCUSABLE_TRUE;
	} else if (!strcmp(attribute_content, "false")) {
		*f = SVG_FOCUSABLE_FALSE;
	} else {
		*f = SVG_FOCUSABLE_AUTO;
	}
}

static void svg_parse_initialvisibility(SVG_InitialVisibility *iv, char *attribute_content)
{
	if (!strcmp(attribute_content, "whenStarted")) {
		*iv = SVG_INITIALVISIBILTY_WHENSTARTED;
	} else if (!strcmp(attribute_content, "always")) {
		*iv = SVG_INITIALVISIBILTY_ALWAYS;
	}
}

static void svg_parse_overlay(SVG_Overlay *o, char *attribute_content)
{
	if (!strcmp(attribute_content, "none")) {
		*o = SVG_OVERLAY_NONE;
	} else if (!strcmp(attribute_content, "top")) {
		*o = SVG_OVERLAY_TOP;
	}
}

static void svg_parse_transformbehavior(SVG_TransformBehavior *tb, char *attribute_content)
{
	if (!strcmp(attribute_content, "geometric")) {
		*tb = SVG_TRANSFORMBEHAVIOR_GEOMETRIC;
	} else if (!strcmp(attribute_content, "pinned")) {
		*tb = SVG_TRANSFORMBEHAVIOR_PINNED;
	} else if (!strcmp(attribute_content, "pinned90")) {
		*tb = SVG_TRANSFORMBEHAVIOR_PINNED90;
	} else if (!strcmp(attribute_content, "pinned180")) {
		*tb = SVG_TRANSFORMBEHAVIOR_PINNED180;
	} else if (!strcmp(attribute_content, "pinned270")) {
		*tb = SVG_TRANSFORMBEHAVIOR_PINNED270;
	}
}

static void svg_parse_focus(GF_Node *e,  SVG_Focus *o, char *attribute_content)
{
	if (o->target.string) free(o->target.string);
	o->target.string = NULL;
	o->target.target = NULL;

	if (!strcmp(attribute_content, "self")) o->type = SVG_FOCUS_SELF;
	else if (!strcmp(attribute_content, "auto")) o->type = SVG_FOCUS_AUTO;
	else if (!strnicmp(attribute_content, "url(", 4)) {
		char *sep = strrchr(attribute_content, ')');
		if (sep) sep[0] = 0;
		o->type = SVG_FOCUS_IRI;
		svg_parse_iri(e, &o->target, attribute_content+4);
		if (sep) sep[0] = ')';
	}
}

/* end of Basic SVG datatype parsing functions */

void svg_parse_one_anim_value(GF_Node *n, SMIL_AnimateValue *anim_value, char *attribute_content, u8 anim_value_type)
{
	GF_FieldInfo info;
	info.fieldType = anim_value_type;
	info.far_ptr = gf_svg_create_attribute_value(anim_value_type);
	if (info.far_ptr) gf_svg_parse_attribute(n, &info, attribute_content, 0);

	anim_value->value = info.far_ptr;
	anim_value->type = anim_value_type;
}

void svg_parse_anim_values(GF_Node *n, SMIL_AnimateValues *anim_values, char *anim_values_string, u8 anim_value_type)
{
	u32 i = 0;
	char *str;
	s32 psemi = -1;
	GF_FieldInfo info;
	info.fieldType = anim_value_type;
	anim_values->type = anim_value_type;
	
	str = anim_values_string;
	while (1) {
		if (str[i] == ';' || str[i] == 0) {
			u32 single_value_len = 0;
			char c;
			single_value_len = i - (psemi+1);
			c = str [ (psemi+1) + single_value_len];
			str [ (psemi+1) + single_value_len] = 0;
			info.far_ptr = gf_svg_create_attribute_value(anim_value_type);
			if (info.far_ptr) {
				gf_svg_parse_attribute(n, &info, str + (psemi+1), anim_value_type);
				gf_list_add(anim_values->values, info.far_ptr);
			}
			str [ (psemi+1) + single_value_len] = c;
			psemi = i;
			if (!str[i]) return;
		}
		i++;
	}
}

GF_Err laser_parse_choice(LASeR_Choice *choice, char *attribute_content)
{
	if (!strcmp(attribute_content, "none")) {
		choice->type = LASeR_CHOICE_NONE;
	} else if (!strcmp(attribute_content, "all")) {
		choice->type = LASeR_CHOICE_ALL;
	} else {
		choice->type = LASeR_CHOICE_N;
		choice->choice_index = atoi(attribute_content);
	}
	return GF_OK;
}

GF_Err laser_parse_size(LASeR_Size *size, char *attribute_content)
{
	char *str = attribute_content;
	u32 i = 0;
	i+=svg_parse_float(&(str[i]), &(size->width), 0);
	i+=svg_parse_float(&(str[i]), &(size->height), 0);
	return GF_OK;
}

GF_Err gf_svg_parse_element_id(GF_Node *n, const char *nodename, Bool warning_if_defined)
{
	GF_SceneGraph *sg = gf_node_get_graph((GF_Node *)n);
#if 0
	SVG_SA_Element *unided_elt;
	GF_SceneGraph *sg = gf_node_get_graph(n);

	unided_elt = (SVG_SA_Element *)gf_sg_find_node_by_name(sg, (char *) nodename);
	if (unided_elt) {
		/* An element with the same id is already in the document
		   Is it in an update, in which case it may be normal, otherwise it's an error.*/		
		if (!warning_if_defined) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] element with id='%s' already defined in document.\n", nodename));
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] element with id='%s' already defined in document.\n", nodename));
			gf_node_set_id(n, gf_node_get_id((GF_Node*)unided_elt), nodename);
		}
	} else {
		u32 id;
		if (sscanf(nodename, "N%d", &id) == 1) {
			GF_Node *n;
			id++;
			n = gf_sg_find_node(sg, id);
			if (n) { /* an existing node was found with this binary id, reassign a new one */
				u32 nID = gf_sg_get_next_available_node_id(sg);
				const char *nname = gf_node_get_name(n);
				gf_node_set_id(n, nID, nname);
			}
		} else {
			id = gf_sg_get_next_available_node_id(sg);
		}
		gf_node_set_id(n, id, nodename);
	}
#else
	u32 id;
	id = gf_sg_get_max_node_id(sg) + 1;
	gf_node_set_id(n, id, nodename);
#endif
	return GF_OK;
}

/* Parse an SVG attribute */
GF_Err gf_svg_parse_attribute(GF_Node *n, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type)
{
	u32 len;
	while (*attribute_content == ' ') attribute_content++;
	len = strlen(attribute_content);
	while (attribute_content[len-1] == ' ') { attribute_content[len-1] = 0; len--; }

	switch (info->fieldType) {
	case SVG_Boolean_datatype:
		svg_parse_boolean((SVG_Boolean *)info->far_ptr, attribute_content);
	    break;
	case SVG_Color_datatype:
		svg_parse_color((SVG_Color *)info->far_ptr, attribute_content);
	    break;
	case SVG_Paint_datatype:
		svg_parse_paint(n, (SVG_Paint *)info->far_ptr, attribute_content);
		break;

/* beginning of keyword type parsing */
	case SVG_FillRule_datatype:
		svg_parse_clipfillrule((SVG_FillRule *)info->far_ptr, attribute_content);
		break;
	case SVG_StrokeLineJoin_datatype:
		svg_parse_strokelinejoin((SVG_StrokeLineJoin *)info->far_ptr, attribute_content);
		break;
	case SVG_StrokeLineCap_datatype:
		svg_parse_strokelinecap((SVG_StrokeLineCap *)info->far_ptr, attribute_content);
		break;
	case SVG_FontStyle_datatype:
		svg_parse_fontstyle((SVG_FontStyle *)info->far_ptr, attribute_content);
		break;
	case SVG_FontWeight_datatype:
		svg_parse_fontweight((SVG_FontWeight *)info->far_ptr, attribute_content);
		break;
	case SVG_FontVariant_datatype:
		svg_parse_fontvariant((SVG_FontVariant *)info->far_ptr, attribute_content);
		break;
	case SVG_TextAnchor_datatype:
		svg_parse_textanchor((SVG_TextAnchor *)info->far_ptr, attribute_content);
		break;
	case SVG_Display_datatype:
		svg_parse_display((SVG_Display *)info->far_ptr, attribute_content);
		break;
	case SVG_Visibility_datatype:
		svg_parse_visibility((SVG_Visibility *)info->far_ptr, attribute_content);
		break;
	case SVG_Overflow_datatype:
		svg_parse_overflow((SVG_Overflow *)info->far_ptr, attribute_content);
		break;
	case SVG_ZoomAndPan_datatype:
		svg_parse_zoomandpan((SVG_ZoomAndPan *)info->far_ptr, attribute_content);
		break;
	case SVG_DisplayAlign_datatype:
		svg_parse_displayalign((SVG_DisplayAlign *)info->far_ptr, attribute_content);
		break;
	case SVG_TextAlign_datatype:
		svg_parse_textalign((SVG_TextAlign *)info->far_ptr, attribute_content);
		break;
	case SVG_PointerEvents_datatype:
		svg_parse_pointerevents((SVG_PointerEvents *)info->far_ptr, attribute_content);
		break;
	case SVG_RenderingHint_datatype:
		svg_parse_renderinghint((SVG_RenderingHint *)info->far_ptr, attribute_content);
		break;
	case SVG_VectorEffect_datatype:
		svg_parse_vectoreffect((SVG_VectorEffect *)info->far_ptr, attribute_content);
		break;
	case SVG_PlaybackOrder_datatype:
		svg_parse_playbackorder((SVG_PlaybackOrder *)info->far_ptr, attribute_content);
		break;
	case SVG_TimelineBegin_datatype:
		svg_parse_timelinebegin((SVG_TimelineBegin *)info->far_ptr, attribute_content);
		break;
	case XML_Space_datatype:
		svg_parse_xmlspace((XML_Space *)info->far_ptr, attribute_content);
		break;
	case XMLEV_Propagate_datatype:
		svg_parse_xmlev_propagate((XMLEV_Propagate *)info->far_ptr, attribute_content);
		break;
	case XMLEV_DefaultAction_datatype:
		svg_parse_xmlev_defaultAction((XMLEV_DefaultAction *)info->far_ptr, attribute_content);
		break;
	case XMLEV_Phase_datatype:
		svg_parse_xmlev_phase((XMLEV_Phase *)info->far_ptr, attribute_content);
		break;
	case SMIL_SyncBehavior_datatype:
		smil_parse_syncBehaviorOrDefault((SMIL_SyncBehavior *)info->far_ptr, attribute_content);
		break;
	case SMIL_SyncTolerance_datatype:
		smil_parse_syncToleranceOrDefault((SMIL_SyncTolerance *)info->far_ptr, attribute_content);
		break;
	case SMIL_AttributeType_datatype:
		smil_parse_attributeType((SMIL_AttributeType *)info->far_ptr, attribute_content);
		break;	
	case SMIL_CalcMode_datatype:
		smil_parse_calcmode((SMIL_CalcMode *)info->far_ptr, attribute_content);
		break;
	case SMIL_Additive_datatype:
		smil_parse_additive((SMIL_CalcMode *)info->far_ptr, attribute_content);
		break;
	case SMIL_Accumulate_datatype:
		smil_parse_accumulate((SMIL_Accumulate *)info->far_ptr, attribute_content);
		break;
	case SMIL_Restart_datatype:
		smil_parse_restart((SMIL_Restart *)info->far_ptr, attribute_content);
		break;
	case SMIL_Fill_datatype:
		smil_parse_fill((SMIL_Fill *)info->far_ptr, attribute_content);
		break;
	case SVG_GradientUnit_datatype:
		*((SVG_GradientUnit *)info->far_ptr) = !strcmp(attribute_content, "userSpaceOnUse") ? SVG_GRADIENTUNITS_USER : SVG_GRADIENTUNITS_OBJECT;
		break;
	case SVG_FocusHighlight_datatype:
		svg_parse_focushighlight((SVG_FocusHighlight*)info->far_ptr, attribute_content);
		break;
	case SVG_Focusable_datatype:
		svg_parse_focusable((SVG_Focusable*)info->far_ptr, attribute_content);
		break;

	case SVG_InitialVisibility_datatype:
		svg_parse_initialvisibility((SVG_InitialVisibility*)info->far_ptr, attribute_content);
		break;
	case SVG_Overlay_datatype:
		svg_parse_overlay((SVG_Overlay*)info->far_ptr, attribute_content);
		break;
	case SVG_TransformBehavior_datatype:
		svg_parse_transformbehavior((SVG_TransformBehavior*)info->far_ptr, attribute_content);
		break;
	case SVG_SpreadMethod_datatype:
		if (!strcmp(attribute_content, "reflect")) *(u8*)info->far_ptr = SVG_SPREAD_REFLECT;
		else if (!strcmp(attribute_content, "repeat")) *(u8*)info->far_ptr = SVG_SPREAD_REPEAT;
		else *(u8*)info->far_ptr = SVG_SPREAD_PAD;
		break;
/* end of keyword type parsing */

	/* keyword | floats */
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype: 
	case SVG_FontSize_datatype:
	case SVG_Rotate_datatype:
	case SVG_Number_datatype:
		svg_parse_number((SVG_Number*)info->far_ptr, attribute_content, 0);
		break;

	case SMIL_AnimateValue_datatype:
		svg_parse_one_anim_value(n, (SMIL_AnimateValue*)info->far_ptr, attribute_content, anim_value_type);
		break;
	case SMIL_AnimateValues_datatype:
		svg_parse_anim_values(n, (SMIL_AnimateValues*)info->far_ptr, attribute_content, anim_value_type);
		break;

	case XMLRI_datatype:
		svg_parse_iri(n, (XMLRI*)info->far_ptr, attribute_content);
		break;
	case XML_IDREF_datatype:
		svg_parse_idref(n, (XMLRI*)info->far_ptr, attribute_content);
		break;
	case SMIL_AttributeName_datatype:
		((SMIL_AttributeName *)info->far_ptr)->name = strdup(attribute_content);
		break;
	case SMIL_Times_datatype:
		smil_parse_time_list(n, *(GF_List **)info->far_ptr, attribute_content);
		break;
	case SMIL_Duration_datatype:
		smil_parse_min_max_dur_repeatdur((SMIL_Duration*)info->far_ptr, attribute_content);
		break;
	case SMIL_RepeatCount_datatype:
		smil_parse_repeatcount((SMIL_RepeatCount*)info->far_ptr, attribute_content);
		break;
	case SVG_PathData_datatype:
		svg_parse_path((SVG_PathData*)info->far_ptr, attribute_content);
		break;
	case SVG_Points_datatype:
		svg_parse_points(*(GF_List **)(info->far_ptr), attribute_content);
		break;
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
		svg_parse_floats(*(GF_List **)(info->far_ptr), attribute_content, 0);
		break;
	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
		svg_parse_coordinates(*(GF_List **)(info->far_ptr), attribute_content);
		break;
	case SVG_ViewBox_datatype:
		svg_parse_viewbox((SVG_ViewBox*)info->far_ptr, attribute_content);
		break;
	case SVG_StrokeDashArray_datatype:
		svg_parse_strokedasharray((SVG_StrokeDashArray*)info->far_ptr, attribute_content);
		break;
	case SVG_FontFamily_datatype:
		svg_parse_fontfamily((SVG_FontFamily*)info->far_ptr, attribute_content);
		break;
	case SVG_Motion_datatype:
		svg_parse_point_into_matrix((GF_Matrix2D*)info->far_ptr, attribute_content);
		break;
	case SVG_Transform_datatype:
		svg_parse_transform((SVG_Transform*)info->far_ptr, attribute_content);
		break;
	case SVG_Transform_Translate_datatype:
		{
			u32 i = 0;
			SVG_Point *p = (SVG_Point *)info->far_ptr;;
			i+=svg_parse_float(&(attribute_content[i]), &(p->x), 0);
			if (attribute_content[i] == 0) {
				p->y = 0;
			} else {
				i+=svg_parse_float(&(attribute_content[i]), &(p->y), 0);
			}
		}
		break;
	case SVG_Transform_Scale_datatype:
		{
			u32 i = 0;
			SVG_Point *p = (SVG_Point *)info->far_ptr;;
			i+=svg_parse_float(&(attribute_content[i]), &(p->x), 0);
			if (attribute_content[i] == 0) {
				p->y = p->x;
			} else {
				i+=svg_parse_float(&(attribute_content[i]), &(p->y), 0);
			}
		}
		break;
	case SVG_Transform_SkewX_datatype:
	case SVG_Transform_SkewY_datatype:
		{
			Fixed *p = (Fixed *)info->far_ptr;
			svg_parse_float(attribute_content, p, 1);
		}
		break;
	case SVG_Transform_Rotate_datatype:
		{
			u32 i = 0;
			SVG_Point_Angle *p = (SVG_Point_Angle *)info->far_ptr;;
			i+=svg_parse_float(&(attribute_content[i]), &(p->angle), 1);
			if (attribute_content[i] == 0) {
				p->y = p->x = 0;
			} else {
				i+=svg_parse_float(&(attribute_content[i]), &(p->x), 0);
				i+=svg_parse_float(&(attribute_content[i]), &(p->y), 0);
			}					
		}
		break;
	case SVG_PreserveAspectRatio_datatype:
		svg_parse_preserveaspectratio((SVG_PreserveAspectRatio*)info->far_ptr, attribute_content);
		break;
	case SVG_TransformType_datatype:
		svg_parse_animatetransform_type((SVG_TransformType*)info->far_ptr, attribute_content);
		break;

	case SVG_ID_datatype:
		/* This should not be use when parsing a LASeR update */
		/* If an ID is parsed outside from the parser (e.g. script), we may try to resolve animations ... */
		gf_svg_parse_element_id(n, attribute_content, 0);
		break;

	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
		*(SVG_String *)info->far_ptr = strdup(attribute_content);
		break;

	case SVG_FeatureList_datatype:
	case SVG_ExtensionList_datatype:
	case SVG_LanguageIDs_datatype:
		svg_parse_strings(*(GF_List **)info->far_ptr, attribute_content, 0);
		break;
	case SVG_ListOfIRI_datatype:
		svg_parse_strings(*(GF_List **)info->far_ptr, attribute_content, 1);
		break;

	case XMLEV_Event_datatype:
	{
		XMLEV_Event *xml_ev = (XMLEV_Event *)info->far_ptr;
		char *sep = strchr(attribute_content, '(');
		if (sep) {
			sep[0] = 0;
			xml_ev->type = gf_dom_event_type_by_name(attribute_content);
			sep[0] = '(';
			if ((xml_ev->type == GF_EVENT_REPEAT) || (xml_ev->type == GF_EVENT_REPEAT_EVENT)) {
				char _v;
				sscanf(sep, "(%c)", &_v);
				xml_ev->parameter = _v;
			} else { /* key events ... */
				char *sep2 = strchr(attribute_content, ')');
				sep2[0] = 0;
				xml_ev->parameter = gf_dom_get_key_type(sep+1);
				sep2[0] = ')';
			}			
		} else {
			xml_ev->parameter = 0;
			xml_ev->type = gf_dom_event_type_by_name(attribute_content);
		}
	}
		break;

	case SVG_Focus_datatype:
		svg_parse_focus(n, (SVG_Focus*)info->far_ptr, attribute_content);
		break;
	case LASeR_Choice_datatype:
		laser_parse_choice((LASeR_Choice*)info->far_ptr, attribute_content);
		break;
	case LASeR_Size_datatype:
		laser_parse_size((LASeR_Size*)info->far_ptr, attribute_content);
		break;
	case LASeR_TimeAttribute_datatype:
		if (!strcmp(attribute_content, "end")) *(u8 *)info->far_ptr = LASeR_TIMEATTRIBUTE_END;
		else *(u8 *)info->far_ptr = LASeR_TIMEATTRIBUTE_BEGIN;
		break;
	case SVG_Clock_datatype:
		svg_parse_clock_value(attribute_content, (SVG_Clock*)info->far_ptr);
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] Cannot parse attribute %s\n", info->name, gf_svg_attribute_type_to_string(info->fieldType)));
		break;
	}
	return GF_OK;
}

void svg_parse_one_style(GF_Node *n, char *one_style) 
{
	GF_FieldInfo info;
	char *c, sep;
	u32 attributeNameLen;

	while (*one_style == ' ') one_style++;
	c = strchr(one_style, ':');
	if (!c) return;
	attributeNameLen = (c - one_style);
	sep = one_style[attributeNameLen];
	one_style[attributeNameLen] = 0;
	if (!gf_node_get_field_by_name(n, one_style, &info)) {
		c++;
		gf_svg_parse_attribute(n, &info, c, 0);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] Attribute %s does not belong to element %s.\n", one_style, gf_node_get_class_name(n)));
	}
	one_style[attributeNameLen] = sep;
}

void gf_svg_parse_style(GF_Node *n, char *style) 
{
	u32 i = 0;
	char *str = style;
	s32 psemi = -1;
	
	while (1) {
		if (str[i] == ';' || str[i] == 0) {
			u32 single_value_len = 0;
			single_value_len = i - (psemi+1);
			if (single_value_len) {
				char c = str[psemi+1 + single_value_len];
				str[psemi+1 + single_value_len] = 0;
				svg_parse_one_style(n, str + psemi+1);
				str[psemi+1 + single_value_len] = c;
				psemi = i;
			}
			if (!str[i]) return;
		}
		i++;
	}

}

void *gf_svg_create_attribute_value(u32 attribute_type)
{
	switch (attribute_type) {
	case SVG_Boolean_datatype:
		{
			SVG_Boolean *b;
			GF_SAFEALLOC(b, SVG_Boolean)
			return b;
		}
		break;
	case SVG_Color_datatype:
		{
			SVG_Color *color;
			GF_SAFEALLOC(color, SVG_Color)
			return color;
		}
		break;
	case SVG_Paint_datatype:
		{
			SVG_Paint *paint;				
			GF_SAFEALLOC(paint, SVG_Paint)
			return paint;
		}
		break;
	
	/* keyword types */
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_TransformType_datatype:
	case SVG_FocusHighlight_datatype:
	case SVG_InitialVisibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_Overlay_datatype:
	case SVG_TransformBehavior_datatype:
	case SVG_SpreadMethod_datatype:
	case SVG_Focusable_datatype:
		{
			u8 *keyword;
			GF_SAFEALLOC(keyword, u8)
			return keyword;
		}
		break;

	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Rotate_datatype:
	case SVG_Number_datatype:
		{
			SVG_Number *number;
			GF_SAFEALLOC(number, SVG_Number)
			return number;
		}
		break;	
	
	case SVG_StrokeDashArray_datatype:
		{
			SVG_StrokeDashArray *array;
			GF_SAFEALLOC(array, SVG_StrokeDashArray)
			return array;
		}
		break;

	case SVG_Motion_datatype:
		{
			GF_Matrix2D *p;
			GF_SAFEALLOC(p, GF_Matrix2D)
			gf_mx2d_init(*p);
			return p;
		}
		break;

	case SVG_Transform_datatype:
		{
			SVG_Transform *p;
			GF_SAFEALLOC(p, SVG_Transform)
			gf_mx2d_init(p->mat);
			return p;
		}
		break;

	case SVG_Transform_Translate_datatype:
	case SVG_Transform_Scale_datatype:
		{
			SVG_Point *p;
			GF_SAFEALLOC(p, SVG_Point)
			return p;
		}
		break;

	case SVG_Transform_SkewX_datatype:
	case SVG_Transform_SkewY_datatype:
		{
			Fixed *p;
			GF_SAFEALLOC(p, Fixed)
			return p;
		}
		break;

	case SVG_Transform_Rotate_datatype:
		{
			SVG_Point_Angle *p;
			GF_SAFEALLOC(p, SVG_Point_Angle)
			return p;
		}
		break;

	case SVG_ViewBox_datatype:
		{
			SVG_ViewBox *viewbox;
			GF_SAFEALLOC(viewbox, SVG_ViewBox)
			return viewbox;
		}
		break;
	case XMLRI_datatype:
	case XML_IDREF_datatype:
		{
			XMLRI *iri;
			GF_SAFEALLOC(iri, XMLRI)
			return iri;
		}
		break;
	case SVG_FontFamily_datatype:
		{
			SVG_FontFamily *fontfamily;
			GF_SAFEALLOC(fontfamily, SVG_FontFamily)
			return fontfamily;
		}
		break;
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
		{
			SVG_String *string;
			GF_SAFEALLOC(string, SVG_String)
			return string;
		}
		break;
	case SVG_FeatureList_datatype:
	case SVG_ExtensionList_datatype:
	case SVG_LanguageIDs_datatype:
	case SVG_ListOfIRI_datatype:
	case SVG_Points_datatype:
	case SVG_Coordinates_datatype:
	case SMIL_Times_datatype:
	case SMIL_KeySplines_datatype:
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SVG_Numbers_datatype:
		{
			ListOfXXX *list;
			GF_SAFEALLOC(list, ListOfXXX)
			*list = gf_list_new();
			return list;
		}
		break;
	case SVG_PreserveAspectRatio_datatype:
		{
			SVG_PreserveAspectRatio *par;
			GF_SAFEALLOC(par, SVG_PreserveAspectRatio)
			return par;
		}
		break;
	case SVG_PathData_datatype:
		{
			SVG_PathData *path;
			GF_SAFEALLOC(path, SVG_PathData);
#if USE_GF_PATH
			gf_path_reset(path);
			path->fineness = FIX_ONE;
#else 
			path->commands = gf_list_new();
			path->points = gf_list_new();
#endif
			return path;
		}
		break;
	case LASeR_Choice_datatype:
		{
			LASeR_Choice *ch;
			GF_SAFEALLOC(ch, LASeR_Choice)
			return ch;
		}
	case SVG_Focus_datatype:
		{
			SVG_Focus *foc;
			GF_SAFEALLOC(foc, SVG_Focus)
			return foc;
		}
	case SMIL_AttributeName_datatype:
		{
			SMIL_AttributeName *an;
			GF_SAFEALLOC(an, SMIL_AttributeName)
			return an;
		}
	case SMIL_RepeatCount_datatype:
		{
			SMIL_RepeatCount *rc;
			GF_SAFEALLOC(rc, SMIL_RepeatCount)
			return rc;
		}
	case SMIL_Duration_datatype:
		{
			SMIL_Duration *sd;
			GF_SAFEALLOC(sd, SMIL_Duration)
			return sd;
		}
	case SMIL_AnimateValue_datatype:
		{
			SMIL_AnimateValue *av;
			GF_SAFEALLOC(av, SMIL_AnimateValue)
			return av;
		}
		break;
	case SMIL_AnimateValues_datatype:
		{
			SMIL_AnimateValues *av;
			GF_SAFEALLOC(av, SMIL_AnimateValues)
			av->values = gf_list_new();
			return av;
		}
		break;
	case SVG_Clock_datatype:
		{
			SVG_Clock *ck;
			GF_SAFEALLOC(ck, SVG_Clock)
			return ck;
		}
		break;

	case XMLEV_Event_datatype:
		{
			XMLEV_Event *e;
			GF_SAFEALLOC(e, XMLEV_Event);
			return e;
		}
		break;
	case LASeR_Size_datatype:
		{
			LASeR_Size *s;
			GF_SAFEALLOC(s, LASeR_Size);
			return s;
		}
		break;

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] Cannot create attribute value - Type %s not supported.\n", gf_svg_attribute_type_to_string(attribute_type)));
		break;
	} 
	return NULL;
}

static void svg_dump_color(SVG_Color *col, char *attValue)
{
	if (col->type == SVG_COLOR_CURRENTCOLOR) strcpy(attValue, "currentColor");
	else if (col->type == SVG_COLOR_INHERIT) strcpy(attValue, "inherit");
	else if (col->type !=SVG_COLOR_RGBCOLOR) {
		u32 i, count;
		count = sizeof(system_colors) / sizeof(struct sys_col);
		for (i=0; i<count; i++) {
			if (col->type == system_colors[i].type) {
				strcpy(attValue, system_colors[i].name);
				return;
			}
		}
	} else {
		u32 i, count = sizeof(predefined_colors) / sizeof(struct predef_col);
		u32 r, g, b;
		r = FIX2INT(255*col->red);
		g = FIX2INT(255*col->green);
		b = FIX2INT(255*col->blue);
		for (i=0; i<count; i++) {
			if (
				(r == predefined_colors[i].r)
				&& (g == predefined_colors[i].g)
				&& (b == predefined_colors[i].b)
			) {
				strcpy(attValue, predefined_colors[i].name);
				return;
			}
		}
		sprintf(attValue, "#%02X%02X%02X", r, g, b);
		/*compress it...*/
		if ( (attValue[1]==attValue[2]) && (attValue[3]==attValue[4]) && (attValue[5]==attValue[6]) )
			sprintf(attValue, "#%c%c%c", attValue[1], attValue[3], attValue[5]);
	}
}

static void svg_dump_number(SVG_Number *l, char *attValue)
{
	if (l->type==SVG_NUMBER_INHERIT) strcpy(attValue, "inherit");
	else if (l->type == SVG_NUMBER_AUTO) strcpy(attValue, "auto");
	else if (l->type == SVG_NUMBER_AUTO_REVERSE) strcpy(attValue, "auto-reverse");
	else {
		sprintf(attValue, "%g", FIX2FLT(l->value) );
		if (l->type == SVG_NUMBER_PERCENTAGE) strcat(attValue, "%");
		else if (l->type == SVG_NUMBER_EMS) strcat(attValue, "em");
		else if (l->type == SVG_NUMBER_EXS) strcat(attValue, "ex");
		else if (l->type == SVG_NUMBER_PX) strcat(attValue, "px");
		else if (l->type == SVG_NUMBER_CM) strcat(attValue, "cm");
		else if (l->type == SVG_NUMBER_MM) strcat(attValue, "mm");
		else if (l->type == SVG_NUMBER_IN) strcat(attValue, "in");
		else if (l->type == SVG_NUMBER_PT) strcat(attValue, "pt");
		else if (l->type == SVG_NUMBER_PC) strcat(attValue, "pc");
	}
}

static void svg_dump_iri(XMLRI*iri, char *attValue)
{
	if (iri->type == XMLRI_ELEMENTID) {
		const char *name;
		name = gf_node_get_name((GF_Node *)iri->target);
		if (name) sprintf(attValue, "#%s", gf_node_get_name((GF_Node *)iri->target));
		else  sprintf(attValue, "#N%d", gf_node_get_id((GF_Node *)iri->target) - 1);
	}
	else if ((iri->type == XMLRI_STRING) && iri->string) strcpy(attValue, iri->string);
	else strcpy(attValue, "");
}

static void svg_dump_idref(XMLRI*iri, char *attValue)
{
	const char *name;
	if (iri->target) {
		name = gf_node_get_name((GF_Node *)iri->target);
		if (name) sprintf(attValue, "%s", gf_node_get_name((GF_Node *)iri->target));
		else sprintf(attValue, "N%d", gf_node_get_id((GF_Node *)iri->target) - 1);
		return;
	}
	if (iri->string) strcpy(attValue, iri->string);
}

#if USE_GF_PATH
static void svg_dump_path(SVG_PathData *path, char *attValue)
{
	char szT[1000];
	GF_Point2D *pt, last_pt, *ct1, *ct2, *end;
	u32 i, *contour;
	strcpy(attValue, "");

	contour = path->contours;

	for (i=0; i<path->n_points; ) {
		switch (path->tags[i]) {
		case GF_PATH_CURVE_ON:
		case GF_PATH_CLOSE:
			pt = &path->points[i];
			if (!i || (*contour == i-1) ) {
				sprintf(szT, "M%g %g", FIX2FLT(pt->x), FIX2FLT(pt->y));
			} else if (path->tags[i]==GF_PATH_CLOSE) {
				sprintf(szT, " z");
			} else {
				if (i && (last_pt.x==pt->x)) sprintf(szT, " V%g", FIX2FLT(pt->y));
				else if (i && (last_pt.y==pt->y)) sprintf(szT, " H%g", FIX2FLT(pt->x));
				sprintf(szT, " L%g %g", FIX2FLT(pt->x), FIX2FLT(pt->y));
			}
			strcat(attValue, szT);
			last_pt = *pt;
			i++;
			break;
		case GF_PATH_CURVE_CONIC:
			ct1 = &path->points[i];
			end = &path->points[i+2];
			sprintf(szT, " Q%g %g %g %g", FIX2FLT(ct1->x), FIX2FLT(ct1->y), FIX2FLT(end->x), FIX2FLT(end->y));
			strcat(attValue, szT);
			last_pt = *end;
			i+=2;
			break;
		case GF_PATH_CURVE_CUBIC:
			ct1 = &path->points[i];
			ct2 = &path->points[i+1];
			end = &path->points[i+2];
			sprintf(szT, " C%g %g %g %g %g %g", FIX2FLT(ct1->x), FIX2FLT(ct1->y), FIX2FLT(ct2->x), FIX2FLT(ct2->y), FIX2FLT(end->x), FIX2FLT(end->y));
			strcat(attValue, szT);
			last_pt = *end;
			i+=3;
			break;
		}
	}

}
#else
static void svg_dump_point(SVG_Point *pt, char *attValue)
{
	if (pt) sprintf(attValue, "%g %g ", FIX2FLT(pt->x), FIX2FLT(pt->y) );
}
static void svg_dump_path(SVG_PathData *path, char *attValue)
{
	char szT[1000];
	u32 i, pt_i, count;
	count = gf_list_count(path->commands);
	pt_i = 0;
	strcpy(attValue, "");
	for (i = 0; i < count; i++) {
		u8 command = *(u8 *)gf_list_get(path->commands, i);
		switch(command) {
		case SVG_PATHCOMMAND_M:
			strcat(attValue, "M");			
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_L:
			strcat(attValue, "L");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_C:
			strcat(attValue, "C");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_S:
			strcat(attValue, "S");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_Q:
			strcat(attValue, "Q");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_T:
			strcat(attValue, "T");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_A:
			strcat(attValue, "A");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			strcat(attValue, "0 0 0 ");
			svg_dump_point((SVG_Point*)gf_list_get(path->points, pt_i), szT);
			strcat(attValue, szT);
			pt_i++;
			break;
		case SVG_PATHCOMMAND_Z:
			strcat(attValue, "Z");
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] unknown path command %d\n", command));
			break;
		}
	}
}
#endif

static void svg_dump_access_key(XMLEV_Event *evt, char *attValue)
{
	u32 i, count;
	strcpy(attValue, "accessKey(");
	count = sizeof(predefined_key_identifiers) / sizeof(struct predef_keyid);
	for (i=0; i<count; i++) {
		if (evt->parameter == predefined_key_identifiers[i].key_code) {
			strcat(attValue, predefined_key_identifiers[i].name);
			break;
		}
	}
	/* OLD LASeR CODE 
	switch (evt->parameter) {
	case 0: strcat(attValue, "UP"); break;
	case 1: strcat(attValue, "DOWN"); break;
	case 2: strcat(attValue, "LEFT"); break;
	case 3: strcat(attValue, "RIGHT"); break;
	case 4: strcat(attValue, "FIRE"); break;
	case 5: strcat(attValue, "NO_KEY"); break;
	case 6: strcat(attValue, "ANY_KEY"); break;
	case 7: strcat(attValue, "SOFT_KEY_1"); break;
	case 8: strcat(attValue, "SOFT_KEY_2"); break;
	case 35: strcat(attValue, "#"); break;
	case 42: strcat(attValue, "*"); break;
	case 48: strcat(attValue, "0"); break;
	case 49: strcat(attValue, "1"); break;
	case 50: strcat(attValue, "2"); break;
	case 51: strcat(attValue, "3"); break;
	case 52: strcat(attValue, "4"); break;
	case 53: strcat(attValue, "5"); break;
	case 54: strcat(attValue, "6"); break;
	case 55: strcat(attValue, "7"); break;
	case 56: strcat(attValue, "8"); break;
	case 57: strcat(attValue, "9"); break;
	*/
	strcat(attValue, ")");
}

static void gf_svg_dump_matrix(GF_Matrix2D *matrix, char *attValue)
{
	/*try to do a simple decomposition...*/
	if (!matrix->m[1] && !matrix->m[3]) {
		sprintf(attValue, "translate(%g,%g)", FIX2FLT(matrix->m[2]), FIX2FLT(matrix->m[5]) );
		if ((matrix->m[0]!=FIX_ONE) || (matrix->m[4]!=FIX_ONE)) {
			char szT[1024];
			if ((matrix->m[0]==-FIX_ONE) && (matrix->m[4]==-FIX_ONE)) {
				strcpy(szT, " rotate(180)");
			} else {
				sprintf(szT, " scale(%g,%g)", FIX2FLT(matrix->m[0]), FIX2FLT(matrix->m[4]) );
			}
			strcat(attValue, szT);
		}
	} else if (matrix->m[1] == - matrix->m[3]) {
		Fixed angle = gf_asin(matrix->m[3]);
		Fixed cos_a = gf_cos(angle);
		if (ABS(cos_a)>FIX_EPSILON) {
			Fixed sx, sy;
			sx = gf_divfix(matrix->m[0], cos_a);
			sy = gf_divfix(matrix->m[4], cos_a);
			angle = gf_divfix(180*angle, GF_PI);
			if ((sx==sy) && ( ABS(FIX_ONE - ABS(sx) ) < FIX_ONE/100)) {
				sprintf(attValue, "translate(%g,%g) rotate(%g)", FIX2FLT(matrix->m[2]), FIX2FLT(matrix->m[5]), FIX2FLT(gf_divfix(angle*180, GF_PI) ) );
			} else {
				sprintf(attValue, "translate(%g,%g) scale(%g,%g) rotate(%g)", FIX2FLT(matrix->m[2]), FIX2FLT(matrix->m[5]), FIX2FLT(sx), FIX2FLT(sy), FIX2FLT(gf_divfix(angle*180, GF_PI) ) );
			}
		} else {
			Fixed a = angle;
			if (a<0) a += GF_2PI;
			sprintf(attValue, "translate(%g,%g) rotate(%g)", FIX2FLT(matrix->m[2]), FIX2FLT(matrix->m[5]), FIX2FLT(gf_divfix(a*180, GF_PI) ) );
		}
	} 
	/*default*/
	if (!strlen(attValue))
	sprintf(attValue, "matrix(%g %g %g %g %g %g)", FIX2FLT(matrix->m[0]), FIX2FLT(matrix->m[3]), FIX2FLT(matrix->m[1]), FIX2FLT(matrix->m[4]), FIX2FLT(matrix->m[2]), FIX2FLT(matrix->m[5]) );
}

GF_Err gf_svg_dump_attribute(GF_Node *elt, GF_FieldInfo *info, char *attValue)
{
	u8 intVal = *(u8 *)info->far_ptr;
	strcpy(attValue, "");
	
	switch (info->fieldType) {
	case SVG_Boolean_datatype:
		sprintf(attValue, "%s", *(SVG_Boolean *)info->far_ptr ? "true" : "false");
	    break;
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info->far_ptr;
		svg_dump_color(col, attValue);
	}
	    break;
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info->far_ptr;
		if (paint->type == SVG_PAINT_NONE) strcpy(attValue, "none");
		else if (paint->type == SVG_PAINT_INHERIT) strcpy(attValue, "inherit");
		else if (paint->type == SVG_PAINT_URI) {
			strcat(attValue, "url(#");
			svg_dump_iri(&paint->iri, attValue);
			strcat(attValue, ")");
		} else svg_dump_color(&paint->color, attValue);
	}
		break;

/* beginning of keyword type parsing */
	case SVG_FillRule_datatype:
		if (intVal == SVG_FILLRULE_INHERIT) strcpy(attValue, "inherit");
		else if (intVal == SVG_FILLRULE_NONZERO) strcpy(attValue, "nonzero");
		else strcpy(attValue, "evenodd");
		break;

	case SVG_StrokeLineJoin_datatype:
		if (intVal==SVG_STROKELINEJOIN_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_STROKELINEJOIN_MITER) strcpy(attValue, "miter");
		else if (intVal==SVG_STROKELINEJOIN_ROUND) strcpy(attValue, "round");
		else if (intVal==SVG_STROKELINEJOIN_BEVEL) strcpy(attValue, "bevel");
		break;
	case SVG_StrokeLineCap_datatype:
		if (intVal==SVG_STROKELINECAP_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_STROKELINECAP_BUTT) strcpy(attValue, "butt");
		else if (intVal==SVG_STROKELINECAP_ROUND) strcpy(attValue, "round");
		else if (intVal==SVG_STROKELINECAP_SQUARE) strcpy(attValue, "square");
		break;
	case SVG_FontStyle_datatype:
		if (intVal==SVG_FONTSTYLE_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_FONTSTYLE_NORMAL) strcpy(attValue, "normal");
		else if (intVal==SVG_FONTSTYLE_ITALIC) strcpy(attValue, "italic");
		else if (intVal==SVG_FONTSTYLE_OBLIQUE) strcpy(attValue, "oblique");
		break;
	case SVG_FontWeight_datatype:
		if (intVal==SVG_FONTWEIGHT_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_FONTWEIGHT_NORMAL) strcpy(attValue, "normal");
		else if (intVal==SVG_FONTWEIGHT_BOLD) strcpy(attValue, "bold");
		else if (intVal==SVG_FONTWEIGHT_BOLDER) strcpy(attValue, "bolder");
		else if (intVal==SVG_FONTWEIGHT_LIGHTER) strcpy(attValue, "lighter");
		else if (intVal==SVG_FONTWEIGHT_100) strcpy(attValue, "100");
		else if (intVal==SVG_FONTWEIGHT_200) strcpy(attValue, "200");
		else if (intVal==SVG_FONTWEIGHT_300) strcpy(attValue, "300");
		else if (intVal==SVG_FONTWEIGHT_400) strcpy(attValue, "400");
		else if (intVal==SVG_FONTWEIGHT_500) strcpy(attValue, "500");
		else if (intVal==SVG_FONTWEIGHT_600) strcpy(attValue, "600");
		else if (intVal==SVG_FONTWEIGHT_700) strcpy(attValue, "700");
		else if (intVal==SVG_FONTWEIGHT_800) strcpy(attValue, "800");
		else if (intVal==SVG_FONTWEIGHT_900) strcpy(attValue, "900");
		break;
	case SVG_FontVariant_datatype:
		if (intVal==SVG_FONTVARIANT_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_FONTVARIANT_NORMAL) strcpy(attValue, "normal");
		else if (intVal==SVG_FONTVARIANT_SMALLCAPS) strcpy(attValue, "small-caps");
		break;
	case SVG_TextAnchor_datatype:
		if (intVal==SVG_TEXTANCHOR_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_TEXTANCHOR_START) strcpy(attValue, "start");
		else if (intVal==SVG_TEXTANCHOR_MIDDLE) strcpy(attValue, "middle");
		else if (intVal==SVG_TEXTANCHOR_END) strcpy(attValue, "end");
		break;
	case SVG_Display_datatype:
		if (intVal==SVG_DISPLAY_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_DISPLAY_NONE) strcpy(attValue, "none");
		else if (intVal==SVG_DISPLAY_INLINE) strcpy(attValue, "inline");
		else if (intVal==SVG_DISPLAY_BLOCK) strcpy(attValue, "block");
		else if (intVal==SVG_DISPLAY_LIST_ITEM) strcpy(attValue, "list-item");
		else if (intVal==SVG_DISPLAY_RUN_IN) strcpy(attValue, "run-in");
		else if (intVal==SVG_DISPLAY_COMPACT) strcpy(attValue, "compact");
		else if (intVal==SVG_DISPLAY_MARKER) strcpy(attValue, "marker");
		else if (intVal==SVG_DISPLAY_TABLE) strcpy(attValue, "table");
		else if (intVal==SVG_DISPLAY_INLINE_TABLE) strcpy(attValue, "inline-table");
		else if (intVal==SVG_DISPLAY_TABLE_ROW_GROUP) strcpy(attValue, "table-row-group");
		else if (intVal==SVG_DISPLAY_TABLE_HEADER_GROUP) strcpy(attValue, "table-header-group");
		else if (intVal==SVG_DISPLAY_TABLE_FOOTER_GROUP) strcpy(attValue, "table-footer-group");
		else if (intVal==SVG_DISPLAY_TABLE_ROW) strcpy(attValue, "table-row");
		else if (intVal==SVG_DISPLAY_TABLE_COLUMN_GROUP) strcpy(attValue, "table-column-group");
		else if (intVal==SVG_DISPLAY_TABLE_COLUMN) strcpy(attValue, "table-column");
		else if (intVal==SVG_DISPLAY_TABLE_CELL) strcpy(attValue, "table-cell");
		else if (intVal==SVG_DISPLAY_TABLE_CAPTION) strcpy(attValue, "table-caption");
		break;
	case SVG_Visibility_datatype:
		if (intVal==SVG_VISIBILITY_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_VISIBILITY_VISIBLE) strcpy(attValue, "visible");
		else if (intVal==SVG_VISIBILITY_HIDDEN) strcpy(attValue, "hidden");
		else if (intVal==SVG_VISIBILITY_COLLAPSE) strcpy(attValue, "collapse");
		break;
	case SVG_Overflow_datatype:
		if (intVal==SVG_OVERFLOW_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_OVERFLOW_VISIBLE) strcpy(attValue, "visible");
		else if (intVal==SVG_OVERFLOW_HIDDEN) strcpy(attValue, "hidden");
		else if (intVal==SVG_OVERFLOW_SCROLL) strcpy(attValue, "scroll");
		else if (intVal==SVG_OVERFLOW_AUTO) strcpy(attValue, "auto");
		break;
	case SVG_ZoomAndPan_datatype:
		if (intVal==SVG_ZOOMANDPAN_DISABLE) strcpy(attValue, "disable");
		else strcpy(attValue, "magnify");
		break;
	case SVG_DisplayAlign_datatype:
		if (intVal==SVG_DISPLAYALIGN_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_DISPLAYALIGN_AUTO) strcpy(attValue, "auto");
		else if (intVal==SVG_DISPLAYALIGN_BEFORE) strcpy(attValue, "before");
		else if (intVal==SVG_DISPLAYALIGN_CENTER) strcpy(attValue, "center");
		else if (intVal==SVG_DISPLAYALIGN_AFTER) strcpy(attValue, "after");
		break;
	case SVG_TextAlign_datatype:
		if (intVal==SVG_TEXTALIGN_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_TEXTALIGN_START) strcpy(attValue, "start");
		else if (intVal==SVG_TEXTALIGN_CENTER) strcpy(attValue, "center");
		else if (intVal==SVG_TEXTALIGN_END) strcpy(attValue, "end");
		break;
	case SVG_PointerEvents_datatype:
		if (intVal==SVG_POINTEREVENTS_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_POINTEREVENTS_VISIBLEPAINTED) strcpy(attValue, "visiblePainted");
		else if (intVal==SVG_POINTEREVENTS_VISIBLEFILL) strcpy(attValue, "visibleFill");
		else if (intVal==SVG_POINTEREVENTS_VISIBLESTROKE) strcpy(attValue, "visibleStroke");
		else if (intVal==SVG_POINTEREVENTS_VISIBLE) strcpy(attValue, "visible");
		else if (intVal==SVG_POINTEREVENTS_PAINTED) strcpy(attValue, "painted");
		else if (intVal==SVG_POINTEREVENTS_FILL) strcpy(attValue, "fill");
		else if (intVal==SVG_POINTEREVENTS_STROKE) strcpy(attValue, "stroke");
		else if (intVal==SVG_POINTEREVENTS_ALL) strcpy(attValue, "all");
		else if (intVal==SVG_POINTEREVENTS_NONE) strcpy(attValue, "none");
		else if (intVal==SVG_POINTEREVENTS_BOUNDINGBOX) strcpy(attValue, "boundingBox");
		break;
	case SVG_RenderingHint_datatype:
		if (intVal==SVG_RENDERINGHINT_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_RENDERINGHINT_AUTO) strcpy(attValue, "auto");
		else if (intVal==SVG_RENDERINGHINT_OPTIMIZEQUALITY) strcpy(attValue, "optimizeQuality");
		else if (intVal==SVG_RENDERINGHINT_OPTIMIZESPEED) strcpy(attValue, "optimizeSpeed");
		else if (intVal==SVG_RENDERINGHINT_OPTIMIZELEGIBILITY) strcpy(attValue, "optimizeLegibility");
		else if (intVal==SVG_RENDERINGHINT_CRISPEDGES) strcpy(attValue, "crispEdges");
		else if (intVal==SVG_RENDERINGHINT_GEOMETRICPRECISION) strcpy(attValue, "geometricPrecision");
		break;
	case SVG_VectorEffect_datatype:
		if (intVal==SVG_VECTOREFFECT_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SVG_VECTOREFFECT_NONE) strcpy(attValue, "none");
		else if (intVal==SVG_VECTOREFFECT_NONSCALINGSTROKE) strcpy(attValue, "non-scaling-stroke");
		break;
	case SVG_PlaybackOrder_datatype:
		if (intVal== SVG_PLAYBACKORDER_FORWARDONLY) strcpy(attValue, "forwardOnly");
		else if (intVal== SVG_PLAYBACKORDER_ALL) strcpy(attValue, "all");
		break;
	case SVG_TimelineBegin_datatype:
		if (intVal== SVG_TIMELINEBEGIN_ONSTART) strcpy(attValue, "onStart");
		else if (intVal== SVG_TIMELINEBEGIN_ONLOAD) strcpy(attValue, "onLoad");
		break;
	case XML_Space_datatype:
		if (intVal==XML_SPACE_DEFAULT) strcpy(attValue, "default");
		else if (intVal==XML_SPACE_PRESERVE) strcpy(attValue, "preserve");
		break;
	case XMLEV_Propagate_datatype:
		if (intVal==XMLEVENT_PROPAGATE_CONTINUE) strcpy(attValue, "continue");
		else if (intVal==XMLEVENT_PROPAGATE_STOP) strcpy(attValue, "stop");
		break;
	case XMLEV_DefaultAction_datatype:
		if (intVal==XMLEVENT_DEFAULTACTION_CANCEL) strcpy(attValue, "cancel");
		else if (intVal==XMLEVENT_DEFAULTACTION_PERFORM) strcpy(attValue, "perform");
		break;
	case XMLEV_Phase_datatype:
		if (intVal==XMLEVENT_PHASE_DEFAULT) strcpy(attValue, "default");
		else if (intVal==XMLEVENT_PHASE_CAPTURE) strcpy(attValue, "capture");
		break;
	case SMIL_SyncBehavior_datatype:
		if (intVal==SMIL_SYNCBEHAVIOR_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SMIL_SYNCBEHAVIOR_DEFAULT) strcpy(attValue, "default");
		else if (intVal==SMIL_SYNCBEHAVIOR_LOCKED) strcpy(attValue, "locked");
		else if (intVal==SMIL_SYNCBEHAVIOR_CANSLIP) strcpy(attValue, "canSlip");
		else if (intVal==SMIL_SYNCBEHAVIOR_INDEPENDENT) strcpy(attValue, "independent");
		break;
	case SMIL_SyncTolerance_datatype:
		if (intVal==SMIL_SYNCTOLERANCE_INHERIT) strcpy(attValue, "inherit");
		else if (intVal==SMIL_SYNCTOLERANCE_DEFAULT) strcpy(attValue, "default");
		else if (intVal==SMIL_SYNCBEHAVIOR_LOCKED) {
			strcpy(attValue, "default");
			/*FIXME - DIMP SMIL TIMES*/
			//svg_parse_clock_value(value_string, &(value->value));
		}
		break;
	case SMIL_AttributeType_datatype:
		if (intVal==SMIL_ATTRIBUTETYPE_AUTO) strcpy(attValue, "auto");
		else if (intVal==SMIL_ATTRIBUTETYPE_XML) strcpy(attValue, "XML");
		else if (intVal==SMIL_ATTRIBUTETYPE_CSS) strcpy(attValue, "CSS");
		break;	
	case SMIL_CalcMode_datatype:
		if (intVal==SMIL_CALCMODE_DISCRETE) strcpy(attValue, "discrete");
		else if (intVal==SMIL_CALCMODE_LINEAR) strcpy(attValue, "linear");
		else if (intVal==SMIL_CALCMODE_PACED) strcpy(attValue, "paced");
		else if (intVal==SMIL_CALCMODE_SPLINE) strcpy(attValue, "spline");
		break;
	case SMIL_Additive_datatype:
		if (intVal==SMIL_ADDITIVE_REPLACE) strcpy(attValue, "replace");
		else if (intVal==SMIL_ADDITIVE_SUM) strcpy(attValue, "sum");
		break;
	case SMIL_Accumulate_datatype:
		if (intVal==SMIL_ACCUMULATE_NONE) strcpy(attValue, "none");
		else if (intVal==SMIL_ACCUMULATE_SUM) strcpy(attValue, "sum");
		break;
	case SMIL_Restart_datatype:
		if (intVal==SMIL_RESTART_ALWAYS) strcpy(attValue, "always");
		else if (intVal==SMIL_RESTART_WHENNOTACTIVE) strcpy(attValue, "whenNotActive");
		else if (intVal==SMIL_RESTART_NEVER) strcpy(attValue, "never");
		break;
	case SMIL_Fill_datatype:
		if (intVal==SMIL_FILL_FREEZE) strcpy(attValue, "freeze");
		else if (intVal==SMIL_FILL_REMOVE) strcpy(attValue, "remove");
		break;

	case SVG_GradientUnit_datatype:
		if (intVal==SVG_GRADIENTUNITS_USER) strcpy(attValue, "userSpaceOnUse");
		else if (intVal==SVG_GRADIENTUNITS_OBJECT) strcpy(attValue, "objectBoundingBox");
		break;
	case SVG_InitialVisibility_datatype:
		if (intVal==SVG_INITIALVISIBILTY_WHENSTARTED) strcpy(attValue, "whenStarted");
		else if (intVal==SVG_INITIALVISIBILTY_ALWAYS) strcpy(attValue, "always");
		break;
	case SVG_FocusHighlight_datatype:
		if (intVal==SVG_FOCUSHIGHLIGHT_AUTO) strcpy(attValue, "auto");
		else if (intVal==SVG_FOCUSHIGHLIGHT_NONE) strcpy(attValue, "none");
		break;
	case SVG_Overlay_datatype:
		if (intVal==SVG_OVERLAY_NONE) strcpy(attValue, "none");
		else if (intVal==SVG_OVERLAY_TOP) strcpy(attValue, "top");
		break;
	case SVG_TransformBehavior_datatype:
		if (intVal==SVG_TRANSFORMBEHAVIOR_GEOMETRIC) strcpy(attValue, "geometric");
		else if (intVal==SVG_TRANSFORMBEHAVIOR_PINNED) strcpy(attValue, "pinned");
		else if (intVal==SVG_TRANSFORMBEHAVIOR_PINNED90) strcpy(attValue, "pinned90");
		else if (intVal==SVG_TRANSFORMBEHAVIOR_PINNED180) strcpy(attValue, "pinned180");
		else if (intVal==SVG_TRANSFORMBEHAVIOR_PINNED270) strcpy(attValue, "pinned270");
		break;
	case SVG_SpreadMethod_datatype:
		if (intVal==SVG_SPREAD_REFLECT) strcpy(attValue, "reflect");
		else if (intVal==SVG_SPREAD_REFLECT) strcpy(attValue, "repeat");
		else strcpy(attValue, "pad");
		break;

	case LASeR_Choice_datatype:
		if (intVal==LASeR_CHOICE_ALL) strcpy(attValue, "all");
		else if (intVal==LASeR_CHOICE_NONE) strcpy(attValue, "none");
		else if (intVal==LASeR_CHOICE_N) {
			char szT[1000];
			sprintf(szT, "%d", ((LASeR_Choice *)info->far_ptr)->choice_index);		
			strcat(attValue, szT);
		}
		break;
	case LASeR_Size_datatype:
		{
			char szT[1000];
			sprintf(szT, "%g %g", FIX2FLT(((LASeR_Size *)info->far_ptr)->width), FIX2FLT(((LASeR_Size *)info->far_ptr)->height));		
			strcat(attValue, szT);
		}
		break;
	case LASeR_TimeAttribute_datatype:
		strcpy(attValue, (intVal==LASeR_TIMEATTRIBUTE_END) ? "end" : "begin");
		break;
/* end of keyword type parsing */

	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Rotate_datatype:
	case SVG_Number_datatype:
#if DUMP_COORDINATES
		svg_dump_number((SVG_Number *)info->far_ptr, attValue);
#endif
		break;

	case XMLRI_datatype:
		svg_dump_iri((XMLRI*)info->far_ptr, attValue);
		break;
	case XML_IDREF_datatype:
		svg_dump_idref((XMLRI*)info->far_ptr, attValue);
		break;
	case SVG_ListOfIRI_datatype:
	{
		GF_List *l = *(GF_List **)info->far_ptr;
		u32 i, count = gf_list_count(l);
		for (i=0; i<count; i++) {
			char szT[1024];
			XMLRI *iri = (XMLRI *)gf_list_get(l, i);
			svg_dump_iri(iri, szT);
			if (strlen(szT)) {
				if (strlen(attValue)) strcat(attValue, " ");
				strcat(attValue, szT);
			}
		}
	}
		break;

	case SVG_PathData_datatype:
#if DUMP_COORDINATES
		svg_dump_path((SVG_PathData *)info->far_ptr, attValue);
#endif
		break;
	case SVG_Points_datatype:
	{
#if DUMP_COORDINATES
		GF_List *l = *(GF_List **) info->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l);
		for (i=0; i<count; i++) {
			char szT[1000];
			SVG_Point *p = (SVG_Point *)gf_list_get(l, i);
			sprintf(szT, "%g %g", FIX2FLT(p->x), FIX2FLT(p->x));
			if (i) strcat(attValue, " ");
			strcat(attValue, szT);
		}
#endif
	}
		break;
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	{
		GF_List *l = *(GF_List **) info->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l);
		for (i=0; i<count; i++) {
			char szT[1000];
			Fixed *p = (Fixed *)gf_list_get(l, i);
			sprintf(szT, "%g", FIX2FLT(*p));
			if (i) strcat(attValue, " ");
			strcat(attValue, szT);
		}
	}
		break;
	case SVG_Coordinates_datatype:
	{
#if DUMP_COORDINATES
		GF_List *l = *(GF_List **) info->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l);
		for (i=0; i<count; i++) {
			char szT[1000];
			SVG_Coordinate *p = (SVG_Coordinate *)gf_list_get(l, i);
			svg_dump_number((SVG_Length *)p, szT);
			if (strstr(szT, "pt")) {
				fprintf(stderr, "found pt in output\n");
			}
			if (i) strcat(attValue, " ");
			strcat(attValue, szT);
		}
#endif
	}
		break;
	case SVG_ViewBox_datatype:
	{
		SVG_ViewBox *v = (SVG_ViewBox *)info->far_ptr;
		if (v->is_set)
			sprintf(attValue, "%g %g %g %g", FIX2FLT(v->x), FIX2FLT(v->y), FIX2FLT(v->width), FIX2FLT(v->height) );
		else 
			strcat(attValue, "none");
	}
		break;
	case SVG_StrokeDashArray_datatype:
	{
		SVG_StrokeDashArray *p = (SVG_StrokeDashArray *)info->far_ptr;
		if (p->type==SVG_STROKEDASHARRAY_NONE) strcpy(attValue, "none");
		else if (p->type==SVG_STROKEDASHARRAY_INHERIT) strcpy(attValue, "inherit");
		else if (p->type==SVG_STROKEDASHARRAY_ARRAY) {
			u32 i = 0;
			for (i=0; i<p->array.count; i++) {
				char szT[1000];
				sprintf(szT, "%g", FIX2FLT(p->array.vals[i]));
				if (i) strcat(attValue, " ");
				strcat(attValue, szT);
			}
		}
	}
		break;
	case SVG_FontFamily_datatype:
	{
		SVG_FontFamily *f = (SVG_FontFamily *)info->far_ptr;
		strcpy(attValue, (f->type==SVG_FONTFAMILY_INHERIT) ? "inherit" : (const char *) f->value);
	}
		break;
	case SVG_PreserveAspectRatio_datatype:
	{
		SVG_PreserveAspectRatio *par = (SVG_PreserveAspectRatio *)info->far_ptr;
		if (par->defer) strcat(attValue, "defer ");
		if (par->align == SVG_PRESERVEASPECTRATIO_NONE) strcat(attValue, "none");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMINYMIN) strcat(attValue, "xMinYMin");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMIDYMIN) strcat(attValue, "xMidYMin");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMAXYMIN) strcat(attValue, "xMaxYMin");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMINYMID) strcat(attValue, "xMinYMid");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMIDYMID) strcat(attValue, "xMidYMid");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMAXYMID) strcat(attValue, "xMaxYMid");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMINYMAX) strcat(attValue, "xMinYMax");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMIDYMAX) strcat(attValue, "xMidYMax");
		else if (par->align == SVG_PRESERVEASPECTRATIO_XMAXYMAX) strcat(attValue, "xMaxYMax");
		if (par->meetOrSlice== SVG_MEETORSLICE_SLICE) strcat(attValue, " slice");
	}
		break;
	case SVG_Clock_datatype:
		sprintf(attValue, "%g", * (SVG_Clock *)info->far_ptr );
		break;

	case SVG_ID_datatype:
	case SVG_LanguageID_datatype:
	case SVG_GradientOffset_datatype:
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
		if (*(SVG_String *)info->far_ptr) 
			strcpy(attValue, *(SVG_String *)info->far_ptr );
		break;
	case SVG_Focus_datatype:
	{
		SVG_Focus *foc = (SVG_Focus *)info->far_ptr;
		if (foc->type==SVG_FOCUS_SELF) strcpy(attValue, "self");
		else if (foc->type==SVG_FOCUS_AUTO) strcpy(attValue, "auto");
		else sprintf(attValue, "#%s", foc->target.string);
	}
		break;

	/*not sure what we'll put in requiredFormats*/
	case SVG_FormatList_datatype:
	case SVG_LanguageIDs_datatype:
	case SVG_FontList_datatype:
	{
		GF_List *l1 = *(GF_List **) info->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		for (i=0; i<count; i++) {
			char *p1 = (char *)gf_list_get(l1, i);
			if (strlen(attValue)) strcat(attValue, " ");
			strcat(attValue, p1);
		}
	}
		break;
	case SVG_Numbers_datatype:
	{
#if DUMP_COORDINATES
		GF_List *l1 = *(GF_List **) info->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		for (i=0; i<count; i++) {
			char szT[1024];
			SVG_Number *p = (SVG_Number *)gf_list_get(l1, i);
			svg_dump_number(p, attValue);
			if (i) strcat(attValue, " ");
			strcat(attValue, szT);
		}
#endif
	}
		break;

	case SVG_Motion_datatype:
		{
#if DUMP_COORDINATES
			GF_Matrix2D *m = (GF_Matrix2D *)info->far_ptr;
			sprintf(attValue, "%g %g", FIX2FLT(m->m[2]), FIX2FLT(m->m[5]));
#endif
		}
		break;

	case SVG_Transform_datatype:
		{
			SVG_Transform *t= (SVG_Transform *)info->far_ptr;
			if (t->is_ref) {
				sprintf(attValue, "ref(svg,%g,%g)", FIX2FLT(t->mat.m[2]), FIX2FLT(t->mat.m[5]) );
			} else {
				gf_svg_dump_matrix(&t->mat, attValue);
			}
		}
		break;

	case SVG_Transform_Translate_datatype:
		{
			SVG_Point *pt = (SVG_Point *)info->far_ptr;
#if DUMP_COORDINATES
			sprintf(attValue, "%g %g", FIX2FLT(pt->x), FIX2FLT(pt->y) );
#endif
		}
		break;

	case SVG_Transform_Scale_datatype:
		{
			SVG_Point *pt = (SVG_Point *)info->far_ptr;
#if DUMP_COORDINATES
			if (pt->x == pt->y) {
				sprintf(attValue, "%g", FIX2FLT(pt->x));
			} else {
				sprintf(attValue, "%g %g", FIX2FLT(pt->x), FIX2FLT(pt->y) );
			}
#endif
		}
		break;

	case SVG_Transform_SkewX_datatype:
	case SVG_Transform_SkewY_datatype:
		{
			Fixed *f = (Fixed *)info->far_ptr;
#if DUMP_COORDINATES
			sprintf(attValue, "%g", FIX2FLT( 180 * gf_divfix(*f, GF_PI) ));
#endif
		}
		break;

	case SVG_Transform_Rotate_datatype:
		{
			SVG_Point_Angle *pt = (SVG_Point_Angle *)info->far_ptr;
#if DUMP_COORDINATES
			if (pt->x || pt->y) {
				sprintf(attValue, "%g %g %g", FIX2FLT( 180 * gf_divfix(pt->angle, GF_PI) ), FIX2FLT(pt->x), FIX2FLT(pt->y) );
			} else {
				sprintf(attValue, "%g", FIX2FLT(gf_divfix(180 * pt->angle, GF_PI) ));
			}
#endif
		}
		break;

	case SMIL_AttributeName_datatype:
	{
		SMIL_AttributeName *att_name = (SMIL_AttributeName *) info->far_ptr;
		if (att_name->name) {
			strcpy(attValue, att_name->name);
			return GF_OK;
		}
		if (att_name->tag) {
			strcpy(attValue, gf_svg_get_attribute_name(att_name->tag));
			return GF_OK;
		}

#if 0
		GF_Node *t=NULL;
		u32 i, count;
		if (!elt->xlink) break;
		t = (GF_Node *) elt->xlink->href.target;
		if (!t) break;
		count = gf_node_get_field_count(t);
		for (i=0; i<count; i++) {
			GF_FieldInfo fi;
			gf_node_get_field(t, i, &fi);
			if (fi.far_ptr == att_name->field_ptr) {
				sprintf(attValue, fi.name);
				return GF_OK;
			}
		}
#endif
	}
		break;
	case SMIL_Times_datatype:
	{
		u32 i, count;
		GF_Node *par = gf_node_get_parent((GF_Node *)elt, 0);
		GF_List *l = *(GF_List **) info->far_ptr;
		count = gf_list_count(l);
		for (i=0; i<count; i++) {
			char szBuf[1000];
			SMIL_Time *t = (SMIL_Time *)gf_list_get(l, i);
			if (i) strcat(attValue, ";");
			if (t->type == GF_SMIL_TIME_CLOCK) {
				sprintf(szBuf, "%gs", t->clock);
				strcat(attValue, szBuf);
			} else if (t->type==GF_SMIL_TIME_INDEFINITE) {
				strcat(attValue, "indefinite");
			} else if (t->type==GF_SMIL_TIME_WALLCLOCK) {
				u32 h, m, s;
				/*TODO - day month and year*/
				h = (u32) t->clock * 3600;
				m = (u32) (t->clock * 60 - 60*h);
				s = (u32) (t->clock - 3600*h - 60*m);
				sprintf(szBuf, "wallclock(%d:%d:%d)", h, m, s);
				strcat(attValue, szBuf);
			}
			else if (t->type==GF_SMIL_TIME_EVENT) {
				if (t->event.type == GF_EVENT_KEYDOWN) {
					svg_dump_access_key(&t->event, szBuf);
					strcat(attValue, szBuf);
				} else {
					if (t->element_id) {
						strcat(attValue, t->element_id);
						strcat(attValue, ".");
					} else if (t->element && (t->element!=par) && gf_node_get_id(t->element) ) {
						const char *name = gf_node_get_name(t->element);
						if (name) {
							strcat(attValue, name);
						} else {
							sprintf(szBuf, "N%d", gf_node_get_id(t->element)-1 );
							strcat(attValue, szBuf);
						}
						strcat(attValue, ".");
					}
					strcat(attValue, gf_dom_event_get_name(t->event.type));
				}
				if (t->clock) {
					sprintf(szBuf, "%gs", t->clock);
					strcat(attValue, "+");
					strcat(attValue, szBuf);
				}
			}
		}
	}
		break;
	case SMIL_Duration_datatype:
	{
		SMIL_Duration *dur = (SMIL_Duration *)info->far_ptr;
		if (dur->type == SMIL_DURATION_INDEFINITE) strcpy(attValue, "indefinite");
		else if (dur->type == SMIL_DURATION_MEDIA) strcpy(attValue, "media");
		else if (dur->type == SMIL_DURATION_DEFINED) sprintf(attValue, "%gs", dur->clock_value);
		else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] smil duration not assigned\n"));
		}
	}
		break;
	case SMIL_RepeatCount_datatype:
	{
		SMIL_RepeatCount *rep = (SMIL_RepeatCount *)info->far_ptr;
		if (rep->type == SMIL_REPEATCOUNT_INDEFINITE) strcpy(attValue, "indefinite");
		else if (rep->type == SMIL_REPEATCOUNT_DEFINED) sprintf(attValue, "%g", FIX2FLT(rep->count) );
		else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] smil repeat count not assigned\n"));
		}
	}
		break;
	case SVG_TransformType_datatype:
	{
		SVG_TransformType tr = *(SVG_TransformType *)info->far_ptr;
		if (tr == SVG_TRANSFORM_MATRIX) strcpy(attValue, "matrix");
		else if (tr == SVG_TRANSFORM_SCALE) strcpy(attValue, "scale");
		else if (tr == SVG_TRANSFORM_ROTATE) strcpy(attValue, "rotate");
		else if (tr == SVG_TRANSFORM_TRANSLATE) strcpy(attValue, "translate");
		else if (tr == SVG_TRANSFORM_SKEWX) strcpy(attValue, "skewX");
		else if (tr == SVG_TRANSFORM_SKEWY) strcpy(attValue, "skewY");
	}
		break;

	case SMIL_AnimateValue_datatype:
	{
		GF_FieldInfo a_fi;
		SMIL_AnimateValue*av = (SMIL_AnimateValue*)info->far_ptr;
		a_fi.fieldIndex = 0;
		a_fi.fieldType = av->type;
		a_fi.name = info->name;
		a_fi.far_ptr = av->value;
		gf_svg_dump_attribute(elt, &a_fi, attValue);
	}
		break;
	case SMIL_AnimateValues_datatype:
	{
		GF_FieldInfo a_fi;
		u32 i, count;
		SMIL_AnimateValues *av = (SMIL_AnimateValues*)info->far_ptr;
		if (av->type) {
			count = gf_list_count(av->values);
			a_fi.fieldIndex = 0;
			a_fi.fieldType = av->type;
			a_fi.name = info->name;
			for (i=0; i<count; i++) {
				char szBuf[1024];
				a_fi.far_ptr = gf_list_get(av->values, i);
				gf_svg_dump_attribute(elt, &a_fi, szBuf);
				if (i) strcat(attValue, ";");
				strcat(attValue, szBuf);
			}
		}
	}
		break;

	case XMLEV_Event_datatype:
	{
		XMLEV_Event *d = (XMLEV_Event *)info->far_ptr;
		if (d->parameter) {
			svg_dump_access_key(d, attValue);
		} else {
			strcpy(attValue, gf_dom_event_get_name(d->type));
		}
		break;
	}
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] dumping for field %s of type %s not supported\n", info->name, gf_svg_attribute_type_to_string(info->fieldType)));
		break;
	}
	return GF_OK;
}

GF_Err gf_svg_dump_attribute_indexed(GF_Node *elt, GF_FieldInfo *info, char *attValue)
{
	strcpy(attValue, "");
	switch (info->fieldType) {
	case SVG_PointerEvents_datatype:
		break;
	case SVG_ListOfIRI_datatype:
		strcpy(attValue, (char *) info->far_ptr);
		break;

	case SVG_Points_datatype:
	{
#if DUMP_COORDINATES
		SVG_Point *p = (SVG_Point *)info->far_ptr;
		sprintf(attValue, "%g %g", FIX2FLT(p->x), FIX2FLT(p->y));
#endif
	}
		break;
	case SMIL_KeyPoints_datatype:
	case SMIL_KeyTimes_datatype:
	case SMIL_KeySplines_datatype:
	{
		Fixed *p = (Fixed *)info->far_ptr;
		sprintf(attValue, "%g", FIX2FLT(*p));
	}
		break;
	case SVG_Coordinates_datatype:
	{
#if DUMP_COORDINATES
		SVG_Coordinate *p = (SVG_Coordinate *)info->far_ptr;
		svg_dump_number((SVG_Length *)p, attValue);
		if (strstr(attValue, "pt")) {
			fprintf(stderr, "found pt in output\n");
		}
#endif
	}
		break;
	case SVG_ViewBox_datatype:
	{
		Fixed *v = (Fixed *)info->far_ptr;
		sprintf(attValue, "%g", FIX2FLT(*v));
	}
		break;
	case SVG_StrokeDashArray_datatype:
	{
		Fixed *p = (Fixed *)info->far_ptr;
		sprintf(attValue, "%g", FIX2FLT(*p));
	}
		break;
	case SMIL_Times_datatype:
	{
		SMIL_Time *t = (SMIL_Time *)info->far_ptr;
		if (t->type == GF_SMIL_TIME_CLOCK) {
			sprintf(attValue, "%gs", t->clock);
		} else if (t->type==GF_SMIL_TIME_INDEFINITE) {
			strcpy(attValue, "indefinite");
		} else if (t->type==GF_SMIL_TIME_WALLCLOCK) {
			u32 h, m, s;
			/*TODO - day month and year*/
			h = (u32) t->clock * 3600;
			m = (u32) (t->clock * 60 - 60*h);
			s = (u32) (t->clock - 3600*h - 60*m);
			sprintf(attValue, "wallclock(%d:%d:%d)", h, m, s);
		}
		else if (t->type==GF_SMIL_TIME_EVENT) {
			GF_Node *par = gf_node_get_parent((GF_Node *)elt, 0);
			if (t->event.type == GF_EVENT_KEYDOWN) {
				svg_dump_access_key(&t->event, attValue);
			} else {
				strcpy(attValue, "");
				if (t->element_id) {
					strcat(attValue, t->element_id);
					strcat(attValue, ".");
				} else if (t->element && (t->element!=par) && gf_node_get_id(t->element) ) {
					const char *name = gf_node_get_name(t->element);
					if (name) {
						strcat(attValue, name);
					} else {
						sprintf(attValue, "N%d", gf_node_get_id(t->element)-1 );
					}
					strcat(attValue, ".");
				}
				strcat(attValue, gf_dom_event_get_name(t->event.type));
			}
			if (t->clock) {
				char szBuf[100];
				sprintf(szBuf, "%gs", t->clock);
				strcpy(attValue, "+");
				strcat(attValue, szBuf);
			}
		}
	}
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] dumping for indexed field %s of type %s not supported\n", info->name, gf_svg_attribute_type_to_string(info->fieldType)));
		break;
	}
	return GF_OK;
}

static Bool svg_viewbox_equal(SVG_ViewBox *v1, SVG_ViewBox *v2)
{
	if (v1->is_set != v2->is_set) return 0;
	if (!v1->is_set) 
		return 1;
	else {
		if ( (v1->x == v2->x)  && (v1->y == v2->y) && (v1->width == v2->width) && (v1->height == v2->height) )
			return 1;
		else 
			return 0;
	}
}

static Bool svg_colors_equal(SVG_Color *c1, SVG_Color *c2)
{
	if (c1->type != c2->type) return 0;
	if (c1->red != c2->red) return 0;
	if (c1->green != c2->green) return 0;
	if (c1->blue != c2->blue) return 0;
	return 1;
}
static Bool svg_numbers_equal(SVG_Length *l1, SVG_Length *l2)
{
	if (l1->type!=l2->type) return 0;
	return (l1->value==l2->value) ? 1 : 0;
}
static Bool svg_iris_equal(XMLRI*iri1, XMLRI*iri2)
{
	u32 type1, type2;
	type1 = iri1->type;
	type2 = iri2->type;
	/*ignore undef hrefs, these are internall ones*/
	if ((iri1->type == XMLRI_ELEMENTID) && iri1->target) {
		if (!gf_node_get_id((GF_Node *)iri1->target)) type1 = 0;
	}
	if ((iri2->type == XMLRI_ELEMENTID) && iri2->target) {
		if (!gf_node_get_id((GF_Node *)iri2->target)) type2 = 0;
	}
	if (type1 != type2) return 0;
	if ((type1 == XMLRI_ELEMENTID) && (iri1->target == iri2->target) ) return 1;
	if (iri1->string && iri2->string && !strcmp(iri1->string, iri2->string)) return 1;
	if (!iri1->string && !iri2->string) return 1;
	return 0;
}
static Bool svg_matrices_equal(GF_Matrix2D *m1, GF_Matrix2D *m2)
{
	if (m1->m[0] != m2->m[0]) return 0;
	if (m1->m[1] != m2->m[1]) return 0;
	if (m1->m[2] != m2->m[2]) return 0;
	if (m1->m[3] != m2->m[3]) return 0;
	if (m1->m[4] != m2->m[4]) return 0;
	if (m1->m[5] != m2->m[5]) return 0;
	return 1;
}

Bool gf_svg_attributes_equal(GF_FieldInfo *f1, GF_FieldInfo *f2)
{
	u32 v1, v2;
	if (f1->fieldType!=f2->fieldType) return 0;
	if (f1->far_ptr && !f2->far_ptr) return 0;
	if (f2->far_ptr && !f1->far_ptr) return 0;
	if (!f1->far_ptr) return 1; 
	v1 = *(u8 *)f1->far_ptr;
	v2 = *(u8 *)f2->far_ptr;

	switch (f1->fieldType) {
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
	case SVG_FocusHighlight_datatype:
	case SVG_TransformType_datatype:
	case SVG_Overlay_datatype:
	case SVG_TransformBehavior_datatype:
	case SVG_SpreadMethod_datatype:
	case SVG_InitialVisibility_datatype:
	case LASeR_Choice_datatype:
	case LASeR_TimeAttribute_datatype:
		return (v1==v2) ? 1 : 0;
	case SVG_Color_datatype:
		return svg_colors_equal((SVG_Color *)f1->far_ptr, (SVG_Color *)f2->far_ptr);

	case SVG_Paint_datatype:
	{
		SVG_Paint *p1 = (SVG_Paint *)f1->far_ptr;
		SVG_Paint *p2 = (SVG_Paint *)f2->far_ptr;
		if (p1->type != p2->type) return 0;
		if (p1->type==SVG_PAINT_COLOR) return svg_colors_equal(&p1->color, &p2->color);
		else if (p1->type==SVG_PAINT_URI) return svg_iris_equal(&p1->iri, &p2->iri);
		return 1;
	}
		break;

	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Rotate_datatype:
	case SVG_Number_datatype:
		return svg_numbers_equal((SVG_Number *)f1->far_ptr, (SVG_Number *)f2->far_ptr);
	case XMLRI_datatype:
		return svg_iris_equal((XMLRI*)f1->far_ptr, (XMLRI*)f2->far_ptr);
	case SVG_ListOfIRI_datatype:
	{
		GF_List *l1 = *(GF_List **)f1->far_ptr;
		GF_List *l2 = *(GF_List **)f2->far_ptr;
		u32 i, count = gf_list_count(l1);
		if (gf_list_count(l2)!=count) return 0;
		for (i=0; i<count; i++) {
			if (!svg_iris_equal((XMLRI*)gf_list_get(l1, i), (XMLRI*)gf_list_get(l2, i) )) return 0;
		}
		return 1;
	}

	case SVG_PathData_datatype:
	{
		SVG_PathData *d1 = (SVG_PathData *)f1->far_ptr;
		SVG_PathData *d2 = (SVG_PathData *)f2->far_ptr;
		u32 i;
		/*FIXME - be less lazy..*/
#if USE_GF_PATH
		if (d1->n_points != d2->n_points) return 0;
		if (d1->n_contours != d2->n_contours) return 0;
		for (i=0; i<d1->n_points; i++) {
			if (d1->points[i].x != d2->points[i].x) return 0;
			if (d1->points[i].y != d2->points[i].y) return 0;
		}
		for (i=0; i<d1->n_points; i++) {
			if (d1->tags[i] != d2->tags[i]) return 0;
		}
		for (i=0; i<d1->n_contours; i++) {
			if (d1->contours[i] != d2->contours[i]) return 0;
		}
		return 1;
#else
		if (!gf_list_count(d1->commands) && !gf_list_count(d2->commands)) return 1;
#endif
		return 0;
	}
	case SVG_Points_datatype:
	{
		GF_List *l1 = *(GF_List **) f1->far_ptr;
		GF_List *l2 = *(GF_List **) f2->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		if (gf_list_count(l2)!=count) return 0;
		for (i=0; i<count; i++) {
			SVG_Point *p1 = (SVG_Point *)gf_list_get(l1, i);
			SVG_Point *p2 = (SVG_Point *)gf_list_get(l2, i);
			if (p1->x != p2->x) return 0;
			if (p1->y != p2->y) return 0;
		}
		return 1;
	}
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	{
		GF_List *l1 = *(GF_List **) f1->far_ptr;
		GF_List *l2 = *(GF_List **) f2->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		if (gf_list_count(l2)!=count) return 0;
		for (i=0; i<count; i++) {
			Fixed *p1 = (Fixed *)gf_list_get(l1, i);
			Fixed *p2 = (Fixed *)gf_list_get(l2, i);
			if (*p1 != *p2) return 0;
		}
		return 1;
	}
	case SVG_Coordinates_datatype:
	{
		GF_List *l1 = *(GF_List **) f1->far_ptr;
		GF_List *l2 = *(GF_List **) f2->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		if (gf_list_count(l2) != count) return 0;
		for (i=0; i<count; i++) {
			SVG_Coordinate *p1 = (SVG_Coordinate *)gf_list_get(l1, i);
			SVG_Coordinate *p2 = (SVG_Coordinate *)gf_list_get(l2, i);
			if (!svg_numbers_equal(p1, p2)) return 0;
		}
		return 1;
	}
	case SVG_ViewBox_datatype:
	{
		SVG_ViewBox *v1 = (SVG_ViewBox *)f1->far_ptr;
		SVG_ViewBox *v2 = (SVG_ViewBox *)f2->far_ptr;
		return svg_viewbox_equal(v1, v2);
	}
	case SVG_StrokeDashArray_datatype:
	{
		SVG_StrokeDashArray *p1 = (SVG_StrokeDashArray *)f1->far_ptr;
		SVG_StrokeDashArray *p2 = (SVG_StrokeDashArray *)f2->far_ptr;
		if (p1->type!=p2->type) return 0;
		if (p1->type==SVG_STROKEDASHARRAY_ARRAY) {
			u32 i = 0;
			if (p1->array.count != p2->array.count) return 0;
			for (i=0; i<p1->array.count; i++) {
				if (p1->array.vals[i] != p2->array.vals[i]) return 0;
			}
		}
		return 1;
	}
	case SVG_FontFamily_datatype:
	{
		SVG_FontFamily *ff1 = (SVG_FontFamily *)f1->far_ptr;
		SVG_FontFamily *ff2 = (SVG_FontFamily *)f2->far_ptr;
		if (ff1->type!=ff2->type) return 0;
		if (ff1->type==SVG_FONTFAMILY_INHERIT) return 1;
		return (ff1->value && ff2->value && !strcmp(ff1->value, ff2->value)) ? 1 : 0;
	}

	case SVG_Clock_datatype:
		return (* (SVG_Clock *)f1->far_ptr == * (SVG_Clock *)f2->far_ptr) ? 1 : 0;

	/* required for animateMotion */
	case SVG_Motion_datatype:
		return svg_matrices_equal((GF_Matrix2D*)f1->far_ptr, (GF_Matrix2D*)f2->far_ptr);

	case SVG_Transform_datatype:
		{
			SVG_Transform *t1 = (SVG_Transform *)f1->far_ptr;
			SVG_Transform *t2 = (SVG_Transform *)f2->far_ptr;
			if (t1->is_ref == t2->is_ref)
				return svg_matrices_equal(&t1->mat, &t2->mat);
			else 
				return 0;
		}

	case SVG_Transform_Translate_datatype:
	case SVG_Transform_Scale_datatype:
		{
			SVG_Point *p1 = (SVG_Point *)f1->far_ptr;
			SVG_Point *p2 = (SVG_Point *)f2->far_ptr;
			if (p1->x != p2->x) return 0;
			if (p1->y != p2->y) return 0;
			return 1;
		}

	case SVG_Transform_SkewX_datatype:
	case SVG_Transform_SkewY_datatype:
		{
			Fixed *p1 = (Fixed *)f1->far_ptr;
			Fixed *p2 = (Fixed *)f2->far_ptr;
			return (*p1 == *p2);
		}

	case SVG_Transform_Rotate_datatype:
		{
			SVG_Point_Angle *p1 = (SVG_Point_Angle *)f1->far_ptr;
			SVG_Point_Angle *p2 = (SVG_Point_Angle *)f2->far_ptr;
			if (p1->x != p2->x) return 0;
			if (p1->y != p2->y) return 0;
			if (p1->angle != p2->angle) return 0;
			return 1;
		}


	case SVG_ID_datatype:
	case SVG_LanguageID_datatype:
	case SVG_GradientOffset_datatype:
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	{
		char *str1 = *(SVG_String *)f1->far_ptr;
		char *str2 = *(SVG_String *)f2->far_ptr;
		if (!str1 && !str2) return 1;
		return (str1 && str2 && !strcmp(str1, str2)) ? 1 : 0;
	}

	case SVG_Focus_datatype:
	{
		SVG_Focus *foc1 = (SVG_Focus *) f1->far_ptr;
		SVG_Focus *foc2 = (SVG_Focus *)f2->far_ptr;
		if (foc1->type!=foc2->type) return 0;
		if (foc1->type != SVG_FOCUS_IRI) return 1;
		return (foc1->target.string && foc2->target.string && !strcmp(foc1->target.string, foc2->target.string)) ? 1 : 0;
	}
		break;

	/*not sure what we'll put in requiredFormats*/
	case SVG_FormatList_datatype:
	case SVG_LanguageIDs_datatype:
	case SVG_FontList_datatype:
	{
		GF_List *l1 = *(GF_List **) f1->far_ptr;
		GF_List *l2 = *(GF_List **) f2->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		if (gf_list_count(l2) != count) return 0;
		for (i=0; i<count; i++) {
			char *p1 = (char *)gf_list_get(l1, i);
			char *p2 = (char *)gf_list_get(l2, i);
			if (strcmp(p1, p2)) return 0;
		}
		return 1;
	}
	case SVG_Numbers_datatype:
	{
		GF_List *l1 = *(GF_List **) f1->far_ptr;
		GF_List *l2 = *(GF_List **) f2->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		if (gf_list_count(l2) != count) return 0;
		for (i=0; i<count; i++) {
			SVG_Number *p1 = (SVG_Number *)gf_list_get(l1, i);
			SVG_Number *p2 = (SVG_Number *)gf_list_get(l2, i);
			if (!svg_numbers_equal(p1, p2)) return 0;
		}
		return 1;
	}
	case SMIL_Times_datatype:
	{
		GF_List *l1 = *(GF_List **) f1->far_ptr;
		GF_List *l2 = *(GF_List **) f2->far_ptr;
		u32 i = 0;
		u32 count = gf_list_count(l1);
		if (gf_list_count(l2) != count) return 0;
		for (i=0; i<count; i++) {
			SMIL_Time *p1 = (SMIL_Time *)gf_list_get(l1, i);
			SMIL_Time *p2 = (SMIL_Time *)gf_list_get(l2, i);
			if (p1->type != p2->type) return 0;
			if (p1->clock != p2->clock) return 0;
			if (p1->type==GF_SMIL_TIME_EVENT) {
				if (p1->event.type != p2->event.type) return 0;
				if (p1->event.parameter != p2->event.parameter) return 0;
			}
		}
		return 1;
	}
	case SMIL_Duration_datatype:
	{
		SMIL_Duration *d1 = (SMIL_Duration *)f1->far_ptr;
		SMIL_Duration *d2 = (SMIL_Duration *)f2->far_ptr;
		if (d1->type != d2->type) return 0;
		if (d1->clock_value != d2->clock_value) return 0;
		return 1;
	}
	case SMIL_RepeatCount_datatype:
	{
		SMIL_RepeatCount *d1 = (SMIL_RepeatCount *)f1->far_ptr;
		SMIL_RepeatCount *d2 = (SMIL_RepeatCount *)f2->far_ptr;
		if (d1->type != d2->type) return 0;
		if (d1->count != d2->count) return 0;
		return 1;
	}

	case SMIL_AttributeName_datatype:
	{
		SMIL_AttributeName *att1 = (SMIL_AttributeName *) f1->far_ptr;
		SMIL_AttributeName *att2 = (SMIL_AttributeName *) f2->far_ptr;
		/*TODO check me...*/
		if (att2->field_ptr == att1->field_ptr) return 1;
		return 0;
	}

	case SMIL_AnimateValue_datatype:
	{
		SMIL_AnimateValue *av1 = (SMIL_AnimateValue*)f1->far_ptr;
		SMIL_AnimateValue *av2 = (SMIL_AnimateValue*)f2->far_ptr;
		if (av1->value != av2->value) return 0;
		return 1;
	}
		break;

	case SMIL_AnimateValues_datatype:
	{
		u32 count;
		SMIL_AnimateValues *av1 = (SMIL_AnimateValues*)f1->far_ptr;
		SMIL_AnimateValues *av2 = (SMIL_AnimateValues*)f2->far_ptr;
		if (av1->type != av2->type) return 0;
		if ( (count = gf_list_count(av1->values) ) != gf_list_count(av1->values)) return 0;
		return count ? 0 : 1;
	}
	case XMLEV_Event_datatype:
	{
		XMLEV_Event *d1 = (XMLEV_Event *)f1->far_ptr;
		XMLEV_Event *d2 = (XMLEV_Event *)f2->far_ptr;
		if (d1->type != d2->type) return 0;
		if (d1->parameter != d2->parameter) return 0;
		return 1;
	}
	case LASeR_Size_datatype:
	{
		LASeR_Size *sz1 = (LASeR_Size *)f1->far_ptr;
		LASeR_Size *sz2 = (LASeR_Size *)f2->far_ptr;
		if (sz1->width != sz2->width) return 0;
		if (sz1->height != sz2->height) return 0;
		return 1;
	}
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING|GF_LOG_COMPOSE, ("[SVG Parsing] comparaison for field %s of type %s not supported\n", f1->name, gf_svg_attribute_type_to_string(f1->fieldType)));
		return 0;
	}
}

static void svg_color_clamp(SVG_Color *a)
{
	a->red   = MAX(0, MIN(FIX_ONE, a->red));
	a->green = MAX(0, MIN(FIX_ONE, a->green));
	a->blue  = MAX(0, MIN(FIX_ONE, a->blue));
}

static GF_Err svg_color_muladd(Fixed alpha, SVG_Color *a, Fixed beta, SVG_Color *b, SVG_Color *c, Bool clamp)
{
	if (a->type != SVG_COLOR_RGBCOLOR || b->type != SVG_COLOR_RGBCOLOR) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] only RGB colors are additive\n"));
		return GF_BAD_PARAM;
	}
	c->type = SVG_COLOR_RGBCOLOR;
	c->red	 = gf_mulfix(alpha, a->red) + gf_mulfix(beta, b->red);
	c->green = gf_mulfix(alpha, a->green) + gf_mulfix(beta, b->green);
	c->blue  = gf_mulfix(alpha, a->blue) + gf_mulfix(beta, b->blue);
	if (clamp) svg_color_clamp(c);
	return GF_OK;
}

static GF_Err svg_number_muladd(Fixed alpha, SVG_Number *a, Fixed beta, SVG_Number *b, SVG_Number *c)
{
	if (a->type != b->type) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] cannot add lengths of mismatching types\n"));
		return GF_BAD_PARAM;
	}
	if (a->type == SVG_NUMBER_INHERIT || a->type == SVG_NUMBER_AUTO) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] cannot add lengths\n"));
		return GF_BAD_PARAM;
	}
	c->value = gf_mulfix(alpha, a->value) + gf_mulfix(beta, b->value);
	return GF_OK;
}

static GF_Err svg_viewbox_muladd(Fixed alpha, SVG_ViewBox *a, Fixed beta, SVG_ViewBox *b, SVG_ViewBox *c)
{
	c->is_set = 1;
	c->x = gf_mulfix(alpha, a->x) + gf_mulfix(beta, b->x);
	c->y = gf_mulfix(alpha, a->y) + gf_mulfix(beta, b->y);
	c->width = gf_mulfix(alpha, a->width) + gf_mulfix(beta, b->width);
	c->height= gf_mulfix(alpha, a->height) + gf_mulfix(beta, b->height);
	return GF_OK;
}

static GF_Err svg_point_muladd(Fixed alpha, SVG_Point *pta, Fixed beta, SVG_Point *ptb, SVG_Point *ptc)
{
	ptc->x = gf_mulfix(alpha, pta->x) + gf_mulfix(beta, ptb->x);
	ptc->y = gf_mulfix(alpha, pta->y) + gf_mulfix(beta, ptb->y);
	return GF_OK;
}

static GF_Err svg_point_angle_muladd(Fixed alpha, SVG_Point_Angle *pta, Fixed beta, SVG_Point_Angle *ptb, SVG_Point_Angle *ptc)
{
	ptc->x = gf_mulfix(alpha, pta->x) + gf_mulfix(beta, ptb->x);
	ptc->y = gf_mulfix(alpha, pta->y) + gf_mulfix(beta, ptb->y);
	ptc->angle = gf_mulfix(alpha, pta->angle) + gf_mulfix(beta, ptb->angle);
	return GF_OK;
}

static GF_Err svg_points_muladd(Fixed alpha, SVG_Points *a, Fixed beta, SVG_Points *b, SVG_Points *c)
{
	u32 a_count = gf_list_count(*a);
	u32 i;

	if (a_count != gf_list_count(*b)) return GF_BAD_PARAM;

	while (gf_list_count(*c)) {
		SVG_Point *ptc = (SVG_Point *)gf_list_get(*c, 0);
		gf_list_rem(*c, 0);
		free(ptc);
	}
	for (i = 0; i < a_count; i ++) {
		SVG_Point *ptc;
		SVG_Point *pta = (SVG_Point *)gf_list_get(*a, i);
		SVG_Point *ptb = (SVG_Point *)gf_list_get(*b, i);
		GF_SAFEALLOC(ptc, SVG_Point)
		svg_point_muladd(alpha, pta, beta, ptb, ptc);
		gf_list_add(*c, ptc);
	}

	return GF_OK;
}

static GF_Err svg_points_copy(SVG_Points *a, SVG_Points *b)
{
	u32 i, count;

	count = gf_list_count(*a);
	for (i = 0; i < count; i++) {
		SVG_Point *pt = (SVG_Point *)gf_list_get(*a, i);
		free(pt);
	}
	gf_list_reset(*a);
	
	count = gf_list_count(*b);
	for (i = 0; i < count; i ++) {
		SVG_Point *ptb = (SVG_Point *)gf_list_get(*b, i);
		SVG_Point *pta;
		GF_SAFEALLOC(pta, SVG_Point)
		*pta = *ptb;
		gf_list_add(*a, pta);
	}
	return GF_OK;

}

static GF_Err svg_numbers_muladd(Fixed alpha, SVG_Numbers *a, Fixed beta, SVG_Numbers *b, SVG_Numbers *c)
{
	u32 a_count = gf_list_count(*a);
	u32 i;

	if (a_count != gf_list_count(*b)) return GF_BAD_PARAM;

	gf_list_reset(*c);
	for (i = 0; i < a_count; i ++) {
		SVG_Number *nc;
		SVG_Number *na = (SVG_Number *)gf_list_get(*a, i);
		SVG_Number *nb = (SVG_Number *)gf_list_get(*b, i);
		GF_SAFEALLOC(nc, SVG_Number)
		svg_number_muladd(alpha, na, beta, nb, nc);
		gf_list_add(*c, nc);
	}
	return GF_OK;
}

static GF_Err svg_numbers_copy(SVG_Numbers *a, SVG_Numbers *b)
{
	u32 i, count;

	count = gf_list_count(*a);
	for (i = 0; i < count; i++) {
		SVG_Coordinate *c = (SVG_Coordinate *)gf_list_get(*a, i);
		free(c);
	}
	gf_list_reset(*a);
	
	count = gf_list_count(*b);
	for (i = 0; i < count; i ++) {
		SVG_Number *na;
		GF_SAFEALLOC(na, SVG_Number)
		*na = *(SVG_Number *)gf_list_get(*b, i);
		gf_list_add(*a, na);
	}
	return GF_OK;
}

#if USE_GF_PATH
static GF_Err svg_path_copy(SVG_PathData *a, SVG_PathData *b)
{
	if (a->contours) free(a->contours);
	if (a->points) free(a->points); 
	if (a->tags) free(a->tags);

	a->contours = (u32 *)malloc(sizeof(u32)*b->n_contours);
	a->points = (GF_Point2D *) malloc(sizeof(GF_Point2D)*b->n_points);
	a->tags = (u8 *) malloc(sizeof(u8)*b->n_points);
	memcpy(a->contours, b->contours, sizeof(u32)*b->n_contours);
	a->n_contours = b->n_contours;
	memcpy(a->points, b->points, sizeof(GF_Point2D)*b->n_points);
	memcpy(a->tags, b->tags, sizeof(u8)*b->n_points);
	a->n_alloc_points = a->n_points = b->n_points;
	a->flags = b->flags;
	a->bbox = b->bbox;
	a->fineness = b->fineness;
	return GF_OK;
}
#else
static GF_Err svg_path_copy(SVG_PathData *a, SVG_PathData *b)
{
	u32 i, count;
	count = gf_list_count(a->commands);
	for (i = 0; i < count; i++) {
		u8 *command = (u8 *)gf_list_get(a->commands, i);
		free(command);
	}
	gf_list_reset(a->commands);
	count = gf_list_count(a->points);
	for (i = 0; i < count; i++) {
		SVG_Point *pt = (SVG_Point *)gf_list_get(a->points, i);
		free(pt);
	}
	gf_list_reset(a->points);
	
	count = gf_list_count(b->commands);
	for (i = 0; i < count; i ++) {
		u8 *nc = (u8 *)malloc(sizeof(u8));
		*nc = *(u8*)gf_list_get(b->commands, i);
		gf_list_add(a->commands, nc);
	}
	count = gf_list_count(b->points);
	for (i = 0; i < count; i ++) {
		SVG_Point *pta;
		GF_SAFEALLOC(pta, SVG_Point)
		*pta = *(SVG_Point *)gf_list_get(b->points, i);
		gf_list_add(a->points, pta);
	}
	return GF_OK;
}
#endif

#if USE_GF_PATH
static GF_Err svg_path_muladd(Fixed alpha, SVG_PathData *a, Fixed beta, SVG_PathData *b, SVG_PathData *c)
{
	u32 i;

	if (a->n_points != b->n_points) return GF_BAD_PARAM;
	gf_path_reset(c);
	svg_path_copy(c, a);

	for (i=0; i<a->n_points; i++) {
		svg_point_muladd(alpha, (SVG_Point *) &a->points[i], beta, (SVG_Point *) &b->points[i], (SVG_Point *) &c->points[i]);
	}
	c->flags |= GF_PATH_BBOX_DIRTY;
	c->flags &= ~GF_PATH_FLATTENED;
	return GF_OK;
}
#else
static GF_Err svg_path_muladd(Fixed alpha, SVG_PathData *a, Fixed beta, SVG_PathData *b, SVG_PathData *c)
{
	u32 i, ccount, pcount;

	ccount = gf_list_count(a->commands);
	pcount = gf_list_count(a->points);

	if (pcount != gf_list_count(b->points)) return GF_BAD_PARAM;

#if 0
	if (ccount != gf_list_count(b->commands)) return GF_BAD_PARAM;
	for (i = 0; i < ccount; i++) {
		u8 *ac = gf_list_get(a->commands, i);
		u8 *bc = gf_list_get(b->commands, i);
		if (*ac != *bc) return GF_BAD_PARAM;
	}
#endif

	while (gf_list_count(c->commands)) {
		u8 *command = (u8 *)gf_list_last(c->commands);
		free(command);
		gf_list_rem_last(c->commands);
	}
	while (gf_list_count(c->points)) {
		SVG_Point *pt = (SVG_Point *)gf_list_last(c->points);
		free(pt);
		gf_list_rem_last(c->points);
	}
	
	for (i = 0; i < ccount; i++) {
		u8 *nc = (u8 *)malloc(sizeof(u8));
		*nc = *(u8*)gf_list_get(a->commands, i);
		gf_list_add(c->commands, nc);
	}
	for (i = 0; i < pcount; i++) {
		SVG_Point *pta = (SVG_Point *)gf_list_get(a->points, i);
		SVG_Point *ptb = (SVG_Point *)gf_list_get(b->points, i);
		SVG_Point *ptc;
		GF_SAFEALLOC(ptc, SVG_Point)
		svg_point_muladd(alpha, pta, beta, ptb, ptc);
		gf_list_add(c->points, ptc);
	}
	return GF_OK;
}
#endif


static GF_Err svg_dasharray_muladd(Fixed alpha, SVG_StrokeDashArray *a, Fixed beta, SVG_StrokeDashArray *b, SVG_StrokeDashArray *c)
{
	u32 i;
	if (a->type != b->type) return GF_BAD_PARAM;
	if (a->array.count != b->array.count) return GF_BAD_PARAM;

	c->type = a->type;
	c->array.count = a->array.count;
	c->array.vals = (Fixed *) malloc(sizeof(Fixed)*c->array.count);
	for (i = 0; i < c->array.count; i++) {
		c->array.vals[i] = gf_mulfix(alpha, a->array.vals[i]) + gf_mulfix(beta, b->array.vals[i]);
	}
	return GF_OK;
}

static GF_Err svg_dasharray_copy(SVG_StrokeDashArray *a, SVG_StrokeDashArray *b)
{
	a->type = b->type;
	a->array.count = b->array.count;
	a->array.vals = (Fixed*)malloc(sizeof(Fixed)*a->array.count);
	memcpy(a->array.vals, b->array.vals, sizeof(Fixed)*a->array.count);
	return GF_OK;
}

static GF_Err svg_matrix_muladd(Fixed alpha, GF_Matrix2D *a, Fixed beta, GF_Matrix2D *b, GF_Matrix2D *c)
{
	if ((alpha == beta) && (alpha == FIX_ONE) ) {
		GF_Matrix2D tmp;
		gf_mx2d_copy(tmp, *b);
		gf_mx2d_add_matrix(&tmp, a);
		gf_mx2d_copy(*c, tmp);
	} else {
		c->m[0] = gf_mulfix(alpha, a->m[0]) + gf_mulfix(beta, b->m[0]);
		c->m[1] = gf_mulfix(alpha, a->m[1]) + gf_mulfix(beta, b->m[1]);
		c->m[2] = gf_mulfix(alpha, a->m[2]) + gf_mulfix(beta, b->m[2]);
		c->m[3] = gf_mulfix(alpha, a->m[3]) + gf_mulfix(beta, b->m[3]);
		c->m[4] = gf_mulfix(alpha, a->m[4]) + gf_mulfix(beta, b->m[4]);
		c->m[5] = gf_mulfix(alpha, a->m[5]) + gf_mulfix(beta, b->m[5]);
	}
	return GF_OK;
}

static GF_Err laser_size_muladd(Fixed alpha, LASeR_Size *sza, Fixed beta, LASeR_Size *szb, LASeR_Size *szc)
{
	szc->width  = gf_mulfix(alpha, sza->width)  + gf_mulfix(beta, szb->width);
	szc->height = gf_mulfix(alpha, sza->height) + gf_mulfix(beta, szb->height);
	return GF_OK;
}

/* c = alpha * a + beta * b */
GF_Err gf_svg_attributes_muladd(Fixed alpha, GF_FieldInfo *a, 
							 Fixed beta, GF_FieldInfo *b, 
							 GF_FieldInfo *c, 
							 Bool clamp)
{
	if (!a->far_ptr || !b->far_ptr || !c->far_ptr) return GF_BAD_PARAM;

	if (a->fieldType != b->fieldType) {
		if (a->fieldType != SVG_Transform_datatype &&
			a->fieldType != SVG_Transform_Scale_datatype &&
			a->fieldType != SVG_Transform_Translate_datatype &&
			a->fieldType != SVG_Transform_Rotate_datatype &&
			a->fieldType != SVG_Transform_SkewX_datatype &&
			a->fieldType != SVG_Transform_SkewY_datatype &&
			a->fieldType != SVG_Motion_datatype)
		return GF_BAD_PARAM;
	}

	/* by default a and c are of the same type, except for matrix related types */
	c->fieldType = a->fieldType;

	switch (a->fieldType) {

	/* Numeric types */
	case SVG_Color_datatype:
		return svg_color_muladd(alpha, (SVG_Color*)a->far_ptr, beta, (SVG_Color*)b->far_ptr, (SVG_Color*)c->far_ptr, clamp);

	case SVG_Paint_datatype:
		{
			SVG_Paint *pa = (SVG_Paint *)a->far_ptr;
			SVG_Paint *pb = (SVG_Paint *)b->far_ptr;
			SVG_Paint *pc = (SVG_Paint *)c->far_ptr;
			if (pa->type != pb->type || pa->type != SVG_PAINT_COLOR || pb->type != SVG_PAINT_COLOR) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] only color paints are additive\n"));
				return GF_BAD_PARAM;
			}
			pc->type = SVG_PAINT_COLOR;
			return svg_color_muladd(alpha, &pa->color, beta, &pb->color, &pc->color, clamp);
		}

	case SVG_Number_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_FontSize_datatype:
		return svg_number_muladd(alpha, (SVG_Number*)a->far_ptr, beta, (SVG_Number*)b->far_ptr, (SVG_Number*)c->far_ptr);

	case SVG_ViewBox_datatype:
		return svg_viewbox_muladd(alpha, (SVG_ViewBox*)a->far_ptr, beta, (SVG_ViewBox*)b->far_ptr, (SVG_ViewBox*)c->far_ptr);

	case SVG_Points_datatype:
		return svg_points_muladd(alpha, (GF_List **)a->far_ptr, beta, (GF_List **)b->far_ptr, (GF_List **)c->far_ptr);

	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
		return svg_numbers_muladd(alpha, (GF_List **)a->far_ptr, beta, (GF_List **)b->far_ptr, (GF_List **)c->far_ptr);

	case SVG_PathData_datatype:
		return svg_path_muladd(alpha, (SVG_PathData*)a->far_ptr, beta, (SVG_PathData*)b->far_ptr, (SVG_PathData*)c->far_ptr);

	case SVG_StrokeDashArray_datatype:
		return svg_dasharray_muladd(alpha, (SVG_StrokeDashArray*)a->far_ptr, beta, (SVG_StrokeDashArray*)b->far_ptr, (SVG_StrokeDashArray*)c->far_ptr);

	case SVG_Motion_datatype:
		return svg_matrix_muladd(alpha, (GF_Matrix2D*)a->far_ptr, beta, (GF_Matrix2D*)b->far_ptr, (GF_Matrix2D*)c->far_ptr);

	case SVG_Transform_datatype:
		if (b->fieldType == SVG_Transform_datatype) {
			SVG_Transform *ta = (SVG_Transform *)a->far_ptr;
			SVG_Transform *tb = (SVG_Transform *)b->far_ptr;
			SVG_Transform *tc = (SVG_Transform *)c->far_ptr;
			if (ta->is_ref == tb->is_ref) {
				return svg_matrix_muladd(alpha, &ta->mat, beta, &tb->mat, &tc->mat);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
				return GF_NOT_SUPPORTED;
			}
		} else {
			/* a and c are matrices but b is not */
			GF_Matrix2D tmp;
			if (alpha != FIX_ONE) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
				return GF_NOT_SUPPORTED;
			}
			gf_mx2d_init(tmp);
			switch (b->fieldType) {
			case SVG_Transform_Translate_datatype:
				gf_mx2d_add_translation(&tmp, gf_mulfix(((SVG_Point *)b->far_ptr)->x, beta), gf_mulfix(((SVG_Point *)b->far_ptr)->y, beta));
				break;
			case SVG_Transform_Scale_datatype:
				gf_mx2d_add_scale(&tmp, gf_mulfix(((SVG_Point *)b->far_ptr)->x, beta), gf_mulfix(((SVG_Point *)b->far_ptr)->y, beta));
				break;
			case SVG_Transform_Rotate_datatype:
				gf_mx2d_add_rotation(&tmp, gf_mulfix(((SVG_Point_Angle *)b->far_ptr)->x, beta), gf_mulfix(((SVG_Point_Angle *)b->far_ptr)->y, beta), gf_mulfix(((SVG_Point_Angle *)b->far_ptr)->angle, beta));
				break;
			case SVG_Transform_SkewX_datatype:
				gf_mx2d_add_skew_x(&tmp, gf_mulfix(*(Fixed*)b->far_ptr, beta));
				break;
			case SVG_Transform_SkewY_datatype:
				gf_mx2d_add_skew_y(&tmp, gf_mulfix(*(Fixed*)b->far_ptr, beta));
				break;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] copy of attributes %s not supported\n", a->name));
				return GF_NOT_SUPPORTED;
			}
			gf_mx2d_add_matrix(&tmp, &((SVG_Transform*)a->far_ptr)->mat);
			gf_mx2d_copy(((SVG_Transform*)c->far_ptr)->mat, tmp);
			return GF_OK;
		}

	case SVG_Transform_Translate_datatype:
		if (b->fieldType == SVG_Transform_Translate_datatype) {
			return svg_point_muladd(alpha, (SVG_Point*)a->far_ptr, beta, (SVG_Point*)b->far_ptr, (SVG_Point*)c->far_ptr);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
			return GF_NOT_SUPPORTED;
		}

	case SVG_Transform_Scale_datatype:
		if (b->fieldType == SVG_Transform_Scale_datatype) {
			return svg_point_muladd(alpha, (SVG_Point*)a->far_ptr, beta, (SVG_Point*)b->far_ptr, (SVG_Point*)c->far_ptr);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
			return GF_NOT_SUPPORTED;
		}

	case SVG_Transform_Rotate_datatype:
		if (b->fieldType == SVG_Transform_Rotate_datatype) {
			svg_point_angle_muladd(alpha, (SVG_Point_Angle*)a->far_ptr, beta, (SVG_Point_Angle*)b->far_ptr, (SVG_Point_Angle*)c->far_ptr);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
			return GF_NOT_SUPPORTED;
		}

	case SVG_Transform_SkewX_datatype:
		if (b->fieldType == SVG_Transform_SkewX_datatype) {
			*(Fixed*)c->far_ptr = gf_mulfix(alpha, *(Fixed*)a->far_ptr) + gf_mulfix(beta, *(Fixed*)b->far_ptr);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
			return GF_NOT_SUPPORTED;
		}

	case SVG_Transform_SkewY_datatype:
		if (b->fieldType == SVG_Transform_SkewY_datatype) {
			*(Fixed*)c->far_ptr = gf_mulfix(alpha, *(Fixed*)a->far_ptr) + gf_mulfix(beta, *(Fixed*)b->far_ptr);
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] matrix operations not supported\n"));
			return GF_NOT_SUPPORTED;
		}

	case SVG_String_datatype:
	{
		u32 len;
		char *res;
		SVG_String *s_a = (SVG_String *)a->far_ptr;
		SVG_String *s_b = (SVG_String *)b->far_ptr;
		u32 len_a = strlen(*s_a);
		u32 len_b = strlen(*s_b);
		len_a = FIX2INT(alpha * len_a);
		len_b = FIX2INT(beta * len_b);
		len = len_a + len_b + 1;
		res = (char*)malloc(sizeof(char) * len);
		memcpy(res, *s_a, len_a);
		memcpy(res+len_a, *s_b, len_b);
		res[len-1] = 0;
		s_a = (SVG_String*)c->far_ptr;
		if (*s_a) free(*s_a);
		*s_a = res;
	}
		break;
	case LASeR_Size_datatype:
		laser_size_muladd(alpha, (LASeR_Size*)a->far_ptr, beta, (LASeR_Size*)b->far_ptr, (LASeR_Size*)c->far_ptr);
		break;

	/* Keyword types */
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
	case SVG_SpreadMethod_datatype:
	case SVG_TransformType_datatype:

	/* Unsupported types */
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
	case SVG_FontFamily_datatype:
	case XMLRI_datatype:
	case SVG_ListOfIRI_datatype:
	case SVG_FormatList_datatype:
	case SVG_LanguageIDs_datatype:
	case SVG_FontList_datatype:
	case SVG_Clock_datatype:
	case SVG_Focus_datatype:
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SMIL_AnimateValue_datatype:
	case SMIL_AnimateValues_datatype:
	case SMIL_AttributeName_datatype:
	case SMIL_Times_datatype:
	case SMIL_Duration_datatype:
	case SMIL_RepeatCount_datatype:
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[SVG Parsing] addition for attributes %s of type %s not supported\n", a->name, gf_svg_attribute_type_to_string(a->fieldType)));
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

/* *a = *b, copy by value */
GF_Err gf_svg_attributes_copy(GF_FieldInfo *a, GF_FieldInfo *b, Bool clamp)
{
	if (!a->far_ptr || !b->far_ptr) return GF_BAD_PARAM;
	switch (a->fieldType) {
	/* Numeric types */
	case SVG_Color_datatype:
		*((SVG_Color *)a->far_ptr) = *((SVG_Color *)b->far_ptr);
		if (clamp) svg_color_clamp((SVG_Color *)a->far_ptr);
		break;

	case SVG_Paint_datatype:
		{
			SVG_Paint *pa = (SVG_Paint *)a->far_ptr;
			SVG_Paint *pb = (SVG_Paint *)b->far_ptr;
			pa->type = pb->type;
			if (pb->type == SVG_PAINT_URI) {
				GF_FieldInfo tmp_a, tmp_b;
				tmp_a.fieldType = tmp_b.fieldType = XMLRI_datatype;
				tmp_a.far_ptr = &pa->iri;
				tmp_b.far_ptr = &pb->iri;
				gf_svg_attributes_copy(&tmp_a, &tmp_b, 0);
			} else {
				pa->color = pb->color;
			}
			return GF_OK;
		}
		break;

	case SVG_Number_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_FontSize_datatype:
		*((SVG_Number *)a->far_ptr) = *((SVG_Number *)b->far_ptr);
		break;

	case SVG_ViewBox_datatype:
		*((SVG_ViewBox *)a->far_ptr) = *((SVG_ViewBox *)b->far_ptr);
		break;

	case SVG_Points_datatype:
		return svg_points_copy((GF_List**)a->far_ptr, (GF_List**)b->far_ptr);

	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
		return svg_numbers_copy((GF_List**)a->far_ptr, (GF_List**)b->far_ptr);

	case SVG_PathData_datatype:
		return svg_path_copy((SVG_PathData*)a->far_ptr, (SVG_PathData*)b->far_ptr);

	case SVG_StrokeDashArray_datatype:
		return svg_dasharray_copy((SVG_StrokeDashArray*)a->far_ptr, (SVG_StrokeDashArray*)b->far_ptr);

	case SVG_Motion_datatype:
		gf_mx2d_copy(*(GF_Matrix2D *)a->far_ptr, *(GF_Matrix2D *)b->far_ptr);
		return GF_OK;

	case SVG_Transform_datatype:
		switch (b->fieldType) {
		case SVG_Transform_Translate_datatype:
			gf_mx2d_init(((SVG_Transform *)a->far_ptr)->mat);
			gf_mx2d_add_translation(&((SVG_Transform *)a->far_ptr)->mat, ((SVG_Point*)b->far_ptr)->x, ((SVG_Point*)b->far_ptr)->y);
			break;
		case SVG_Transform_Scale_datatype:
			gf_mx2d_init(((SVG_Transform *)a->far_ptr)->mat);
			gf_mx2d_add_scale(&((SVG_Transform *)a->far_ptr)->mat, ((SVG_Point*)b->far_ptr)->x, ((SVG_Point*)b->far_ptr)->y);
			break;
		case SVG_Transform_Rotate_datatype:
			gf_mx2d_init(((SVG_Transform *)a->far_ptr)->mat);
			gf_mx2d_add_rotation(&((SVG_Transform *)a->far_ptr)->mat, ((SVG_Point_Angle*)b->far_ptr)->x, ((SVG_Point_Angle*)b->far_ptr)->y, ((SVG_Point_Angle*)b->far_ptr)->angle);
			break;
		case SVG_Transform_SkewX_datatype:
			gf_mx2d_init(((SVG_Transform *)a->far_ptr)->mat);
			gf_mx2d_add_skew_x(&((SVG_Transform *)a->far_ptr)->mat, *(Fixed *)b->far_ptr);
			break;
		case SVG_Transform_SkewY_datatype:
			gf_mx2d_init(((SVG_Transform *)a->far_ptr)->mat);
			gf_mx2d_add_skew_y(&((SVG_Transform *)a->far_ptr)->mat, *(Fixed *)b->far_ptr);
			break;
		case SVG_Transform_datatype:
			gf_mx2d_copy(((SVG_Transform *)a->far_ptr)->mat, ((SVG_Transform *)b->far_ptr)->mat);
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[SVG Parsing] forbidden type of transform\n"));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	
	/* Keyword types */
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
	case SVG_TransformType_datatype:
		*(u8 *)a->far_ptr = *(u8 *)b->far_ptr;
		return GF_OK;

	/* Other types */
	case SVG_LanguageID_datatype:
	case SVG_ContentType_datatype:
	case SVG_String_datatype:
		a->far_ptr = strdup((char *)b->far_ptr);
		return GF_OK;

	case SVG_FontFamily_datatype:
		((SVG_FontFamily *)a->far_ptr)->type = ((SVG_FontFamily *)b->far_ptr)->type;
		if ( ((SVG_FontFamily *)a->far_ptr)->value) free( ((SVG_FontFamily *)a->far_ptr)->value );
		((SVG_FontFamily *)a->far_ptr)->value = strdup(((SVG_FontFamily *)b->far_ptr)->value);
		return GF_OK;

	case XMLRI_datatype:
	case XML_IDREF_datatype:
		((XMLRI *)a->far_ptr)->type = ((XMLRI *)b->far_ptr)->type;
		if (((XMLRI *)a->far_ptr)->string) free(((XMLRI *)a->far_ptr)->string);
		if (((XMLRI *)b->far_ptr)->string) {
			((XMLRI *)a->far_ptr)->string = strdup(((XMLRI *)b->far_ptr)->string);
		} else {
			((XMLRI *)a->far_ptr)->string = strdup("");
		}
		((XMLRI *)a->far_ptr)->target = ((XMLRI *)b->far_ptr)->target;
		if (((XMLRI *)a->far_ptr)->type == XMLRI_ELEMENTID) {
			GF_Node *n = (GF_Node *) ((XMLRI *)b->far_ptr)->target;
			/*TODO Check if assigning IRI from # scenegraph can happen*/
			if (n) gf_svg_register_iri(gf_node_get_graph(n), (XMLRI*)a->far_ptr);
		}
		return GF_OK;
	
	case SVG_Focus_datatype:
	{
		((SVG_Focus *)a->far_ptr)->type = ((SVG_Focus *)b->far_ptr)->type;
		if ( ((SVG_Focus *)b->far_ptr)->target.string) 
			((SVG_Focus *)a->far_ptr)->target.string = strdup( ((SVG_Focus *)b->far_ptr)->target.string);
	}
		return GF_OK;

	/* Unsupported types */
	case SVG_ListOfIRI_datatype:
	case SVG_FormatList_datatype:
	case SVG_LanguageIDs_datatype:
	case SVG_FontList_datatype:
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
	case SVG_Clock_datatype:
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SMIL_AnimateValue_datatype:
	case SMIL_AnimateValues_datatype:
	case SMIL_AttributeName_datatype:
	case SMIL_Times_datatype:
	case SMIL_Duration_datatype:
	case SMIL_RepeatCount_datatype:
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[SVG Parsing] copy of attributes %s of type %s not supported\n", a->name, gf_svg_attribute_type_to_string(a->fieldType)));
		return GF_OK;
	}
	return GF_OK;
}

/* c = a + b */
GF_Err gf_svg_attributes_add(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Bool clamp)
{
	return gf_svg_attributes_muladd(FIX_ONE, a, FIX_ONE, b, c, clamp);
}

Bool gf_svg_attribute_is_interpolatable(u32 type) 
{
	switch (type) {
	/* additive types which can really be interpolated */
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	case SVG_Number_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_FontSize_datatype:
	case SVG_ViewBox_datatype:
	case SVG_Points_datatype:
	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
	case SVG_PathData_datatype:
	case SVG_Motion_datatype:
	case SVG_Transform_datatype:
	case SVG_Transform_Translate_datatype:
	case SVG_Transform_Scale_datatype:
	case SVG_Transform_Rotate_datatype:
	case SVG_Transform_SkewX_datatype:
	case SVG_Transform_SkewY_datatype:
	case LASeR_Size_datatype:
		return 1;
	} 
	return 0;
}

GF_Err gf_svg_attributes_interpolate(GF_FieldInfo *a, GF_FieldInfo *b, GF_FieldInfo *c, Fixed coef, Bool clamp)
{
	if (!a->far_ptr || !b->far_ptr || !c->far_ptr) return GF_BAD_PARAM;

	c->fieldType = a->fieldType;
	
	switch (a->fieldType) {

	/* additive types which can really be interpolated */
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	case SVG_Number_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_FontSize_datatype:
	case SVG_ViewBox_datatype:
	case SVG_Points_datatype:
	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
	case SVG_PathData_datatype:
	case SVG_Motion_datatype:
	case SVG_Transform_datatype:
	case SVG_Transform_Translate_datatype:
	case SVG_Transform_Scale_datatype:
	case SVG_Transform_Rotate_datatype:
	case SVG_Transform_SkewX_datatype:
	case SVG_Transform_SkewY_datatype:
	case LASeR_Size_datatype:
		return gf_svg_attributes_muladd(FIX_ONE-coef, a, coef, b, c, clamp);

	/* discrete types: interpolation is the selection of one of the 2 values 
		using the coeff and a the 0.5 threshold */
	/* Keyword types */
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
	case SVG_TransformType_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:

	/* Other non keyword types but which can still be discretely interpolated */
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
	case SVG_FontFamily_datatype:
	case XMLRI_datatype:
	case SVG_ListOfIRI_datatype:
	case SVG_FormatList_datatype:
	case SVG_LanguageIDs_datatype:
	case SVG_FontList_datatype:
	case SVG_Clock_datatype:
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
	case LASeR_Choice_datatype:
	case LASeR_TimeAttribute_datatype:
		if (coef < FIX_ONE/2) {
			gf_svg_attributes_copy(c, a, clamp);
		} else {
			gf_svg_attributes_copy(c, b, clamp);
		}
		return GF_OK;

	/* Unsupported types */
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SMIL_AnimateValue_datatype:
	case SMIL_AnimateValues_datatype:
	case SMIL_AttributeName_datatype:
	case SMIL_Times_datatype:
	case SMIL_Duration_datatype:
	case SMIL_RepeatCount_datatype:
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[SVG Parsing] interpolation for attributes %s of type %s not supported\n", a->name, gf_svg_attribute_type_to_string(a->fieldType)));
		return GF_OK;
	}
	return GF_OK;

}

Bool gf_svg_is_current_color(GF_FieldInfo *a)
{
	switch (a->fieldType) {
	case SVG_Color_datatype:
		return (((SVG_Color *)a->far_ptr)->type == SVG_COLOR_CURRENTCOLOR);
		break;
	case SVG_Paint_datatype:
		if ( ((SVG_Paint *)a->far_ptr)->type == SVG_PAINT_COLOR) {
			return (((SVG_Paint *)a->far_ptr)->color.type == SVG_COLOR_CURRENTCOLOR);
		} else {
			return 0;
		}
		break;
	}
	return 0;
}



char *gf_svg_attribute_type_to_string(u32 att_type)
{
	switch (att_type) {
	case SVG_FillRule_datatype:			return "FillRule"; 
	case SVG_StrokeLineJoin_datatype:	return "StrokeLineJoin"; 
	case SVG_StrokeLineCap_datatype:	return "StrokeLineCap"; 
	case SVG_FontStyle_datatype:		return "FontStyle"; 
	case SVG_FontWeight_datatype:		return "FontWeight"; 
	case SVG_FontVariant_datatype:		return "FontVariant"; 
	case SVG_TextAnchor_datatype:		return "TextAnchor"; 
	case SVG_TransformType_datatype:	return "TransformType"; 
	case SVG_Display_datatype:			return "Display"; 
	case SVG_Visibility_datatype:		return "Visibility"; 
	case SVG_Overflow_datatype:			return "Overflow"; 
	case SVG_ZoomAndPan_datatype:		return "ZoomAndPan"; 
	case SVG_DisplayAlign_datatype:		return "DisplayAlign"; 
	case SVG_PointerEvents_datatype:	return "PointerEvents"; 
	case SVG_RenderingHint_datatype:	return "RenderingHint"; 
	case SVG_VectorEffect_datatype:		return "VectorEffect"; 
	case SVG_PlaybackOrder_datatype:	return "PlaybackOrder"; 
	case SVG_TimelineBegin_datatype:	return "TimelineBegin"; 
	case XML_Space_datatype:			return "XML_Space"; 
	case XMLEV_Propagate_datatype:		return "XMLEV_Propagate"; 
	case XMLEV_DefaultAction_datatype:	return "XMLEV_DefaultAction"; 
	case XMLEV_Phase_datatype:			return "XMLEV_Phase"; 
	case SMIL_SyncBehavior_datatype:	return "SMIL_SyncBehavior"; 
	case SMIL_SyncTolerance_datatype:	return "SMIL_SyncTolerance"; 
	case SMIL_AttributeType_datatype:	return "SMIL_AttributeType"; 
	case SMIL_CalcMode_datatype:		return "SMIL_CalcMode"; 
	case SMIL_Additive_datatype:		return "SMIL_Additive"; 
	case SMIL_Accumulate_datatype:		return "SMIL_Accumulate"; 
	case SMIL_Restart_datatype:			return "SMIL_Restart"; 
	case SMIL_Fill_datatype:			return "SMIL_Fill"; 
	case SVG_GradientUnit_datatype:		return "GradientUnit"; 
	case SVG_InitialVisibility_datatype:return "InitialVisibility"; 
	case SVG_FocusHighlight_datatype:	return "FocusHighlight"; 
	case SVG_Overlay_datatype:			return "Overlay"; 
	case SVG_TransformBehavior_datatype:return "TransformBehavior"; 
	case SVG_SpreadMethod_datatype:		return "SpreadMethod"; 
	case SVG_TextAlign_datatype:		return "TextAlign"; 
	case SVG_Number_datatype:			return "Number"; 
	case SVG_FontSize_datatype:			return "FontSize"; 
	case SVG_Length_datatype:			return "Length"; 
	case SVG_Coordinate_datatype:		return "Coordinate"; 
	case SVG_Rotate_datatype:			return "Rotate"; 
	case SVG_Numbers_datatype:			return "Numbers"; 
	case SVG_Points_datatype:			return "Points"; 
	case SVG_Coordinates_datatype:		return "Coordinates"; 
	case SVG_FeatureList_datatype:		return "FeatureList"; 
	case SVG_ExtensionList_datatype:	return "ExtensionList"; 
	case SVG_FormatList_datatype:		return "FormatList"; 
	case SVG_FontList_datatype:			return "FontList"; 
	case SVG_ListOfIRI_datatype:		return "ListOfIRI"; 
	case SVG_LanguageIDs_datatype:		return "LanguageIDs"; 
	case SMIL_KeyTimes_datatype:		return "SMIL_KeyTimes"; 
	case SMIL_KeySplines_datatype:		return "SMIL_KeySplines"; 
	case SMIL_KeyPoints_datatype:		return "SMIL_KeyPoints"; 
	case SMIL_Times_datatype:			return "SMIL_Times"; 
	case SMIL_AnimateValue_datatype:	return "SMIL_AnimateValue"; 
	case SMIL_AnimateValues_datatype:	return "SMIL_AnimateValues"; 
	case SMIL_Duration_datatype:		return "SMIL_Duration"; 
	case SMIL_RepeatCount_datatype:		return "SMIL_RepeatCount"; 
	case SMIL_AttributeName_datatype:	return "SMIL_AttributeName"; 
	case SVG_Boolean_datatype:			return "Boolean"; 
	case SVG_Color_datatype:			return "Color"; 
	case SVG_Paint_datatype:			return "Paint"; 
	case SVG_PathData_datatype:			return "PathData"; 
	case SVG_FontFamily_datatype:		return "FontFamily"; 
	case SVG_ID_datatype:				return "ID"; 
	case XMLRI_datatype:				return "IRI"; 
	case XML_IDREF_datatype:			return "IDREF"; 
	case SVG_StrokeDashArray_datatype:	return "StrokeDashArray"; 
	case SVG_PreserveAspectRatio_datatype:return "PreserveAspectRatio"; 
	case SVG_ViewBox_datatype:			return "ViewBox"; 
	case SVG_GradientOffset_datatype:	return "GradientOffset"; 
	case SVG_Focus_datatype	:			return "Focus"; 
	case SVG_Clock_datatype	:			return "Clock"; 
	case SVG_String_datatype	:		return "String"; 
	case SVG_ContentType_datatype:		return "ContentType"; 
	case SVG_LanguageID_datatype:		return "LanguageID"; 
	case XMLEV_Event_datatype:			return "XMLEV_Event"; 
	case SVG_Motion_datatype:			return "Motion"; 
	case SVG_Transform_datatype:		return "Transform"; 
	case SVG_Transform_Translate_datatype:return "Translate"; 
	case SVG_Transform_Scale_datatype:	return "Scale"; 
	case SVG_Transform_SkewX_datatype:	return "SkewX"; 
	case SVG_Transform_SkewY_datatype:	return "SkewY"; 
	case SVG_Transform_Rotate_datatype:	return "Rotate"; 
	case LASeR_Choice_datatype:			return "LASeR_Choice"; 
	case LASeR_Size_datatype:			return "LASeR_Size"; 
	case LASeR_TimeAttribute_datatype:	return "LASeR_TimeAttribute"; 
	default:							return "UnknownType";
	}
}
