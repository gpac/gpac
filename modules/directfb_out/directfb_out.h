#ifndef _DIRECTFB_OUT_H_
#define _DIRECTFB_OUT_H_


#include <gpac/modules/video_out.h>
#include <gpac/scenegraph_svg.h> 

/* DirectFB */
#define __DIRECT__STDTYPES__	//prevent u8, s8, ... definitions by directFB as we have them in GPAC
#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_util.h>
#include <direct/util.h>

static int do_xor       = 0;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...)                                                     \
          do {                                                             \
               err = x;                                                    \
               if (err != DFB_OK) {                                        \
                    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
                    DirectFBErrorFatal( #x, err );                         \
               }                                                           \
          } while (0)
          
#define SET_DRAWING_FLAGS( flags ) \
          ctx->primary->SetDrawingFlags( ctx->primary, (flags) | (do_xor ? DSDRAW_XOR : 0) )
          
typedef struct _DeviceInfo DeviceInfo;

struct _DeviceInfo {
     DFBInputDeviceID           device_id;
     DFBInputDeviceDescription  desc;
     DeviceInfo                *next;
};

struct predef_keyid {
  u32 key_code;  
  const char *name; 
};

#if 0
#define PredefineKeyID(predefined_key_identifiers) struct predef_keyid predefined_key_identifiers[] = {\
	{ GF_KEY_ACCEPT, "Accept" },\
	{ GF_KEY_AGAIN, "Again" },\
	{ GF_KEY_ALLCANDIDATES, "AllCandidates" },\
	{ GF_KEY_ALPHANUM, "Alphanumeric" },\
	{ GF_KEY_ALT, "Alt" },\
	{ GF_KEY_ALTGRAPH, "AltGraph" },\
	{ GF_KEY_APPS, "Apps" },\
	{ GF_KEY_ATTN, "Attn" },\
	{ GF_KEY_BROWSERBACK, "BrowserBack" },\
	{ GF_KEY_BROWSERFAVORITES, "BrowserFavorites" },\
	{ GF_KEY_BROWSERFORWARD, "BrowserForward" },\
	{ GF_KEY_BROWSERHOME, "BrowserHome" },\
	{ GF_KEY_BROWSERREFRESH, "BrowserRefresh" },\
	{ GF_KEY_BROWSERSEARCH, "BrowserSearch" },\
	{ GF_KEY_BROWSERSTOP, "BrowserStop" },\
	{ GF_KEY_CAPSLOCK, "CapsLock" },\
	{ GF_KEY_CLEAR, "Clear" },\
	{ GF_KEY_CODEINPUT, "CodeInput" },\
	{ GF_KEY_COMPOSE, "Compose" },\
	{ GF_KEY_CONTROL, "Control" },\
	{ GF_KEY_CRSEL, "Crsel" },\
	{ GF_KEY_CONVERT, "Convert" },\
	{ GF_KEY_COPY, "Copy"  },\
	{ GF_KEY_CUT, "Cut" },\
	{ GF_KEY_DOWN, "Down" },\
	{ GF_KEY_END, "End" },\
	{ GF_KEY_ENTER, "Enter" },\
	{ GF_KEY_EPG, "EPG" }, \
	{ GF_KEY_ERASEEOF, "EraseEof" },\
	{ GF_KEY_EXECUTE, "Execute" },\
	{ GF_KEY_EXSEL, "Exsel" },\
	{ GF_KEY_F1, "F1" },\
	{ GF_KEY_F2, "F2" },\
	{ GF_KEY_F3, "F3" },\
	{ GF_KEY_F4, "F4" },\
	{ GF_KEY_F5, "F5" },\
	{ GF_KEY_F6, "F6" },\
	{ GF_KEY_F7, "F7" },\
	{ GF_KEY_F8, "F8" },\
	{ GF_KEY_F9, "F9" },\
	{ GF_KEY_F10, "F10" },\
	{ GF_KEY_F11, "F11" },\
	{ GF_KEY_F12, "F12" },\
	{ GF_KEY_F13, "F13" },\
	{ GF_KEY_F14, "F14" },\
	{ GF_KEY_F15, "F15" },\
	{ GF_KEY_F16, "F16" },\
	{ GF_KEY_F17, "F17" },\
	{ GF_KEY_F18, "F18" },\
	{ GF_KEY_F19, "F19" },\
	{ GF_KEY_F20, "F20" },\
	{ GF_KEY_F21, "F21" },\
	{ GF_KEY_F22, "F22" },\
	{ GF_KEY_F23, "F23" },\
	{ GF_KEY_F24, "F24" },\
	{ GF_KEY_FINALMODE, "FinalMode" },\
	{ GF_KEY_FIND, "Find" },\
	{ GF_KEY_FULLWIDTH, "FullWidth" },\
	{ GF_KEY_HALFWIDTH, "HalfWidth" },\
	{ GF_KEY_HANGULMODE, "HangulMode" },\
	{ GF_KEY_HANJAMODE, "HanjaMode"   },\
	{ GF_KEY_HELP, "Help" },\
	{ GF_KEY_HIRAGANA, "Hiragana" },\
	{ GF_KEY_HOME, "Home" },\
	{ GF_KEY_INFO, "Info" }, \
	{ GF_KEY_INSERT, "Insert" },\
	{ GF_KEY_JAPANESEHIRAGANA, "JapaneseHiragana" },\
	{ GF_KEY_JAPANESEKATAKANA, "JapaneseKatakana" },\
	{ GF_KEY_JAPANESEROMAJI, "JapaneseRomaji" },\
	{ GF_KEY_JUNJAMODE, "JunjaMode" },\
	{ GF_KEY_KANAMODE, "KanaMode"   },\
	{ GF_KEY_KANJIMODE, "KanjiMode" },\
	{ GF_KEY_KATAKANA, "Katakana"   },\
	{ GF_KEY_LAUNCHAPPLICATION1, "LaunchApplication1" },\
	{ GF_KEY_LAUNCHAPPLICATION2, "LaunchApplication2" },\
	{ GF_KEY_LAUNCHMAIL, "LaunchMail" },\
	{ GF_KEY_LEFT, "Left" },\
	{ GF_KEY_META, "Meta" },\
	{ GF_KEY_MEDIANEXTTRACK, "MediaNextTrack" },\
	{ GF_KEY_MEDIAPLAYPAUSE, "MediaPlayPause" },\
	{ GF_KEY_MEDIAPREVIOUSTRACK, "MediaPreviousTrack" },\
	{ GF_KEY_MEDIASTOP, "MediaStop" },\
	{ GF_KEY_MODECHANGE, "ModeChange" },\
	{ GF_KEY_NONCONVERT, "Nonconvert" },\
	{ GF_KEY_NUMLOCK, "NumLock" },\
	{ GF_KEY_PAGEDOWN, "PageDown" },\
	{ GF_KEY_PAGEUP, "PageUp" },\
	{ GF_KEY_PASTE, "Paste" },\
	{ GF_KEY_PAUSE, "Pause" },\
	{ GF_KEY_PLAY, "Play" },\
	{ GF_KEY_PREVIOUSCANDIDATE, "PreviousCandidate" },\
	{ GF_KEY_PRINTSCREEN, "PrintScreen" },\
	{ GF_KEY_PROCESS, "Process" },\
	{ GF_KEY_PROPS, "Props" },\
	{ GF_KEY_RIGHT, "Right" },\
	{ GF_KEY_ROMANCHARACTERS, "RomanCharacters" },\
	{ GF_KEY_SCROLL, "Scroll" },\
	{ GF_KEY_SELECT, "Select" },\
	{ GF_KEY_SELECTMEDIA, "SelectMedia" },\
	{ GF_KEY_SHIFT, "Shift" },\
	{ GF_KEY_STOP, "Stop" },\
	{ GF_KEY_UP, "Up" },\
	{ GF_KEY_UNDO, "Undo" },\
	{ GF_KEY_VOLUMEDOWN, "VolumeDown" },\
	{ GF_KEY_VOLUMEMUTE, "VolumeMute" },\
	{ GF_KEY_VOLUMEUP, "VolumeUp" },\
	{ GF_KEY_WIN, "Win" },\
	{ GF_KEY_ZOOM, "Zoom" },\
	{ GF_KEY_BACKSPACE, "U+0008" },\
	{ GF_KEY_TAB, "U+0009" },\
	{ GF_KEY_CANCEL, "U+0018" },\
	{ GF_KEY_ESCAPE, "U+001B" },\
	{ GF_KEY_SPACE, "U+0020" },\
	{ GF_KEY_EXCLAMATION, "U+0021" },\
	{ GF_KEY_QUOTATION, "U+0022" },\
	{ GF_KEY_NUMBER, "U+0023" },\
	{ GF_KEY_DOLLAR, "U+0024" },\
	{ GF_KEY_AMPERSAND, "U+0026" },\
	{ GF_KEY_APOSTROPHE, "U+0027" },\
	{ GF_KEY_LEFTPARENTHESIS, "U+0028" },\
	{ GF_KEY_RIGHTPARENTHESIS, "U+0029" },\
	{ GF_KEY_STAR, "U+002A" },\
	{ GF_KEY_PLUS, "U+002B" },\
	{ GF_KEY_COMMA, "U+002C" },\
	{ GF_KEY_HYPHEN, "U+002D" },\
	{ GF_KEY_FULLSTOP, "U+002E" },\
	{ GF_KEY_SLASH, "U+002F" },\
	{ GF_KEY_0, "U+0030" },\
	{ GF_KEY_1, "U+0031" },\
	{ GF_KEY_2, "U+0032" },\
	{ GF_KEY_3, "U+0033" },\
	{ GF_KEY_4, "U+0034" },\
	{ GF_KEY_5, "U+0035" },\
	{ GF_KEY_6, "U+0036" },\
	{ GF_KEY_7, "U+0037" },\
	{ GF_KEY_8, "U+0038" },\
	{ GF_KEY_9, "U+0039" },\
	{ GF_KEY_COLON, "U+003A" },\
	{ GF_KEY_SEMICOLON, "U+003B" },\
	{ GF_KEY_LESSTHAN, "U+003C" },\
	{ GF_KEY_EQUALS, "U+003D" },\
	{ GF_KEY_GREATERTHAN, "U+003E" },\
	{ GF_KEY_QUESTION, "U+003F" },\
	{ GF_KEY_AT, "U+0040" },\
	{ GF_KEY_A, "U+0041" },\
	{ GF_KEY_B, "U+0042" },\
	{ GF_KEY_C, "U+0043" },\
	{ GF_KEY_D, "U+0044" },\
	{ GF_KEY_E, "U+0045" },\
	{ GF_KEY_F, "U+0046" },\
	{ GF_KEY_G, "U+0047" },\
	{ GF_KEY_H, "U+0048" },\
	{ GF_KEY_I, "U+0049" },\
	{ GF_KEY_J, "U+004A" },\
	{ GF_KEY_K, "U+004B" },\
	{ GF_KEY_L, "U+004C" },\
	{ GF_KEY_M, "U+004D" },\
	{ GF_KEY_N, "U+004E" },\
	{ GF_KEY_O, "U+004F" },\
	{ GF_KEY_P, "U+0050" },\
	{ GF_KEY_Q, "U+0051" },\
	{ GF_KEY_R, "U+0052" },\
	{ GF_KEY_S, "U+0053" },\
	{ GF_KEY_T, "U+0054" },\
	{ GF_KEY_U, "U+0055" },\
	{ GF_KEY_V, "U+0056" },\
	{ GF_KEY_W, "U+0057" },\
	{ GF_KEY_X, "U+0058" },\
	{ GF_KEY_Y, "U+0059" },\
	{ GF_KEY_Z, "U+005A" },\
	{ GF_KEY_LEFTSQUAREBRACKET, "U+005B" },\
	{ GF_KEY_BACKSLASH, "U+005C" },\
	{ GF_KEY_RIGHTSQUAREBRACKET, "U+005D" },\
	{ GF_KEY_CIRCUM, "U+005E" },\
	{ GF_KEY_UNDERSCORE, "U+005F" },\
	{ GF_KEY_GRAVEACCENT, "U+0060" },\
	{ GF_KEY_LEFTCURLYBRACKET, "U+007B" },\
	{ GF_KEY_PIPE, "U+007C" },\
	{ GF_KEY_RIGHTCURLYBRACKET, "U+007D" },\
	{ GF_KEY_DEL, "U+007F" },\
	{ GF_KEY_INVERTEXCLAMATION, "U+00A1" },\
	{ GF_KEY_DEADGRAVE, "U+0300" },\
	{ GF_KEY_DEADEACUTE, "U+0301" },\
	{ GF_KEY_DEADCIRCUM, "U+0302" },\
	{ GF_KEY_DEADTILDE, "U+0303" },\
	{ GF_KEY_DEADMACRON, "U+0304" }, \
	{ GF_KEY_DEADBREVE, "U+0306" },\
	{ GF_KEY_DEADABOVEDOT, "U+0307" },\
	{ GF_KEY_DEADDIARESIS, "U+0308" }, \
	{ GF_KEY_DEADRINGABOVE, "U+030A" },\
	{ GF_KEY_DEADDOUBLEACUTE, "U+030B" },\
	{ GF_KEY_DEADCARON, "U+030C" },\
	{ GF_KEY_DEADCEDILLA, "U+0327" },\
	{ GF_KEY_DEADOGONEK, "U+0328" },\
	{ GF_KEY_DEADIOTA, "U+0345" },\
	{ GF_KEY_EURO, "U+20AC" },\
	{ GF_KEY_DEADVOICESOUND, "U+3099" },\
	{ GF_KEY_DEADSEMIVOICESOUND, "U+309A" }\
};
#endif 
typedef struct
{
    /* the super interface */
    IDirectFB *dfb;
    /* the primary surface */
    IDirectFBSurface *primary;

    /* screen width, height */
    u32 width, height, pixel_format;
    Bool use_systems_memory, disable_acceleration, disable_aa, is_init, disable_display;

     /* acceleration */
    int accel_drawline, accel_fillrect;
    DFBSurfaceFlipFlags flip_mode;

    /*===== for key events =====*/

    /* Input interfaces: devices and its buffer */
    IDirectFBEventBuffer *events;

     /*=============================
	if using window
    ============================= */
    
    /* DirectFB window */
    IDirectFBWindow *window;
    
    /* display layer */
    IDirectFBDisplayLayer *layer;

} DirectFBVidCtx;

void *DirectFBNewVideo();
void DirectFBDeleteVideo(void *ifce);

#endif
