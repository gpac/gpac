/*
 *  libgpac_hack.h
 *  mp4client
 *
 *  Created by bouqueau on 17/06/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */


/*exports for dlopen*/
#define gf_log_lt _gf_log_lt
#define AVI_close _AVI_close
#define gf_term_del _gf_term_del
#define gf_sleep _gf_sleep
#define gf_sc_release_screen_buffer _gf_sc_release_screen_buffer
#define gf_log_set_callback _gf_log_set_callback
#define gf_prompt_get_char _gf_prompt_get_char
#define gf_set_progress _gf_set_progress
#define gf_term_new _gf_term_new
#define gf_term_process_step _gf_term_process_step
#define gf_sc_get_screen_buffer _gf_sc_get_screen_buffer
#define gf_iphone_set_sdl_audio_module _gf_iphone_set_sdl_audio_module
#define gf_term_step_clocks _gf_term_step_clocks
#define gf_prompt_set_echo_off _gf_prompt_set_echo_off
#define gf_log_get_tools _gf_log_get_tools
#define gf_log_get_level _gf_log_get_level
#define gf_cfg_set_key _gf_cfg_set_key
#define gf_cfg_get_section_count _gf_cfg_get_section_count
#define gf_term_get_service_info _gf_term_get_service_info
#define gf_term_set_size _gf_term_set_size
#define gf_sys_get_rti _gf_sys_get_rti
#define gf_term_play_from_time _gf_term_play_from_time
#define gf_malloc _gf_malloc
#define gf_log_set_tools_levels _gf_log_set_tools_levels
#define gf_log_set_tool_level _gf_log_set_tool_level
#define gf_iphone_set_sdl_video_module _gf_iphone_set_sdl_video_module
#define gf_term_get_option _gf_term_get_option
#define gf_term_user_event _gf_term_user_event
#define gf_modules_get_file_name _gf_modules_get_file_name
#define gf_mx_new _gf_mx_new
#define gf_list_count _gf_list_count
#define gf_free _gf_free
#define gf_term_get_world_info _gf_term_get_world_info
#define gf_cfg_get_section_name _gf_cfg_get_section_name
#define gf_term_navigate_to _gf_term_navigate_to
#define gf_modules_del _gf_modules_del
#define gf_modules_new _gf_modules_new
#define gf_sys_init _gf_sys_init
#define gf_log _gf_log
#define gf_term_get_object_info _gf_term_get_object_info
#define gf_mx_p _gf_mx_p
#define gf_mx_v _gf_mx_v
#define gf_mx_del _gf_mx_del
#define gf_term_process_flush _gf_term_process_flush
#define gf_cfg_get_key_name _gf_cfg_get_key_name
#define AVI_write_frame _AVI_write_frame
#define gf_cfg_del _gf_cfg_del
#define gf_term_get_channel_net_info _gf_term_get_channel_net_info
#define gf_term_process_shortcut _gf_term_process_shortcut
#define gf_cfg_init _gf_cfg_init
#define gf_term_get_download_info _gf_term_get_download_info
#define gf_sys_clock _gf_sys_clock
#define gf_term_get_object _gf_term_get_object
#define gf_term_set_option _gf_term_set_option
#define gf_sys_close _gf_sys_close
#define gf_term_connect_from_time _gf_term_connect_from_time
#define AVI_open_output_file _AVI_open_output_file
#define gf_cfg_get_key _gf_cfg_get_key
#define AVI_set_video _AVI_set_video
#define gf_term_set_speed _gf_term_set_speed
#define gf_cfg_get_key_count _gf_cfg_get_key_count
#define gf_term_object_subscene_type _gf_term_object_subscene_type
#define gf_term_get_framerate _gf_term_get_framerate
#define gf_error_to_string _gf_error_to_string
#define gf_stretch_bits _gf_stretch_bits
#define gf_list_del _gf_list_del
#define gf_list_get _gf_list_get
#define gf_term_disconnect _gf_term_disconnect
#define gf_term_is_supported_url _gf_term_is_supported_url
#define gf_list_new _gf_list_new
#define gf_modules_get_option _gf_modules_get_option
#define gf_term_dump_scene _gf_term_dump_scene
#define gf_prompt_has_input _gf_prompt_has_input
#define gf_term_scene_update _gf_term_scene_update
#define gf_term_connect _gf_term_connect
#define gf_term_get_object_count _gf_term_get_object_count
#define gf_modules_get_count _gf_modules_get_count
#define gf_term_get_root_object _gf_term_get_root_object
#define gf_term_get_time_in_ms _gf_term_get_time_in_ms
#define gf_term_connect_with_path _gf_term_connect_with_path
#define gf_log_parse_tools _gf_log_parse_tools
#define gf_log_parse_level _gf_log_parse_level
#define gf_term_switch_quality _gf_term_switch_quality
#define gf_term_release_screen_buffer _gf_term_release_screen_buffer
#define gf_term_get_screen_buffer _gf_term_get_screen_buffer
#define gf_f64_open _gf_f64_open
#define gf_img_png_enc _gf_img_png_enc
#define utf8_to_ucs4 _utf8_to_ucs4

/*includes both terminal and od browser*/
#include <gpac/terminal.h>
#include <gpac/term_info.h>
#include <gpac/constants.h>
#include <gpac/options.h>
#include <gpac/modules/service.h>

/*ISO 639 languages*/
#include <gpac/iso639.h>
#include "sdl_out.h"
#include "dlfcn.h"


/*exports for dlopen*/
#include <gpac/internal/avilib.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#undef gf_log_lt
extern void (*gf_log_lt)(u32 ll, u32 lt);
#undef AVI_close
extern int (*AVI_close)(avi_t *AVI);
#undef gf_term_del
extern GF_Err (*gf_term_del)(GF_Terminal *term);
#undef gf_sleep
extern void (*gf_sleep)(u32 ms);
#undef gf_sc_release_screen_buffer
extern GF_Err (*gf_sc_release_screen_buffer)(GF_Compositor *sr, GF_VideoSurface *framebuffer);
#undef gf_prompt_get_char
extern char (*gf_prompt_get_char)();
#undef gf_set_progress
extern void (*gf_set_progress)(char *title, u32 done, u32 total);
#undef gf_term_new
extern GF_Terminal *(*gf_term_new)(GF_User *user);
#undef gf_term_process_step
extern GF_Err (*gf_term_process_step)(GF_Terminal *term);
#undef gf_sc_get_screen_buffer
extern GF_Err (*gf_sc_get_screen_buffer)(GF_Compositor *sr, GF_VideoSurface *framebuffer, Bool depth_buffer);
#undef gf_iphone_set_sdl_audio_module
extern void (*gf_iphone_set_sdl_audio_module)(void* (*SDL_Module) (void));
#undef gf_term_step_clocks
extern GF_Err (*gf_term_step_clocks)(GF_Terminal * term, u32 ms_diff);
#undef gf_prompt_set_echo_off
extern void (*gf_prompt_set_echo_off)(Bool echo_off);
#undef gf_log_get_tools
extern u32 (*gf_log_get_tools)();
#undef gf_log_get_level
extern u32 (*gf_log_get_level)();
#undef gf_cfg_set_key
extern GF_Err (*gf_cfg_set_key)(GF_Config *cfgFile, const char *secName, const char *keyName, const char *keyValue);
#undef gf_cfg_get_section_count
extern u32 (*gf_cfg_get_section_count)(GF_Config *cfgFile);
#undef gf_term_get_service_info
extern GF_Err (*gf_term_get_service_info)(GF_Terminal *term, GF_ObjectManager *odm, NetInfoCommand *netcom);
#undef gf_term_set_size
extern GF_Err (*gf_term_set_size)(GF_Terminal *term, u32 NewWidth, u32 NewHeight);
#undef gf_sys_get_rti
extern Bool (*gf_sys_get_rti)(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags);
#undef gf_term_play_from_time
extern u32 (*gf_term_play_from_time)(GF_Terminal *term, u64 from_time, u32 pause_at_first_frame);
#undef gf_malloc
extern void *(*gf_malloc)(size_t size);
#undef gf_log_set_tools_levels
extern void (*gf_log_set_tools_levels)(const char *tools);
#undef gf_log_set_tool_level
extern void (*gf_log_set_tool_level)(const char *tools);
#undef gf_iphone_set_sdl_video_module
extern void (*gf_iphone_set_sdl_video_module)(void* (*SDL_Module) (void));
#undef gf_term_get_option
extern u32 (*gf_term_get_option)(GF_Terminal *term, u32 opt_type);
#undef gf_term_user_event
extern Bool (*gf_term_user_event)(GF_Terminal *term, GF_Event *event);
#undef gf_modules_get_file_name
extern const char *(*gf_modules_get_file_name)(GF_ModuleManager *pm, u32 index);
#undef gf_mx_new
extern GF_Mutex *(*gf_mx_new)(const char *name);
#undef gf_list_count
extern u32 (*gf_list_count)(GF_List *ptr);
#undef gf_free
extern void (*gf_free)(void *ptr);
#undef gf_term_get_world_info
extern const char *(*gf_term_get_world_info)(GF_Terminal *term, GF_ObjectManager *scene_od, GF_List *descriptions);
#undef gf_cfg_get_section_name
extern const char *(*gf_cfg_get_section_name)(GF_Config *cfgFile, u32 secIndex);
#undef gf_term_navigate_to
extern void (*gf_term_navigate_to)(GF_Terminal *term, const char *toURL);
#undef gf_modules_del
extern void (*gf_modules_del)(GF_ModuleManager *pm);
#undef gf_modules_new
extern GF_ModuleManager *(*gf_modules_new)(const char *directory, GF_Config *cfgFile);
#undef gf_sys_init
extern void (*gf_sys_init)(Bool enable_memory_tracker);
#undef gf_log
extern void (*gf_log)(const char *fmt, ...);
#undef gf_term_get_object_info
extern GF_Err (*gf_term_get_object_info)(GF_Terminal *term, GF_ObjectManager *odm, GF_MediaInfo *info);
#undef gf_mx_p
extern u32 (*gf_mx_p)(GF_Mutex *mx);
#undef gf_mx_v
extern u32 (*gf_mx_v)(GF_Mutex *mx);
#undef gf_mx_del
extern void (*gf_mx_del)(GF_Mutex *mx);
#undef gf_term_process_flush
extern GF_Err (*gf_term_process_flush)(GF_Terminal *term);
#undef gf_cfg_get_key_name
extern const char *(*gf_cfg_get_key_name)(GF_Config *cfgFile, const char *secName, u32 keyIndex);
#undef AVI_write_frame
extern int (*AVI_write_frame)(avi_t *AVI, char *data, long bytes, int keyframe);
#undef gf_cfg_del
extern void (*gf_cfg_del)(GF_Config *iniFile);
#undef gf_term_get_channel_net_info
extern Bool (*gf_term_get_channel_net_info)(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, u32 *chid, NetStatCommand *netcom, GF_Err *ret_code);
#undef gf_term_process_shortcut
extern void (*gf_term_process_shortcut)(GF_Terminal *term, GF_Event *ev);
#undef gf_cfg_init
extern GF_Config *(*gf_cfg_init)(const char *fileName, Bool *is_new);
#undef gf_term_get_download_info
extern Bool (*gf_term_get_download_info)(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, const char **server, const char **path, u32 *bytes_done, u32 *total_bytes, u32 *bytes_per_sec);
#undef gf_sys_clock
extern u32 (*gf_sys_clock)();
#undef gf_term_get_object
extern GF_ObjectManager *(*gf_term_get_object)(GF_Terminal *term, GF_ObjectManager *scene_od, u32 index);
#undef gf_term_set_option
extern GF_Err (*gf_term_set_option)(GF_Terminal *term, u32 opt_type, u32 opt_value);
#undef gf_sys_close
extern void (*gf_sys_close)();
#undef gf_term_connect_from_time
extern void (*gf_term_connect_from_time)(GF_Terminal *term, const char *URL, u64 time_in_ms, Bool pause_at_first_frame);
#undef AVI_open_output_file
extern avi_t* (*AVI_open_output_file)(char * filename);
#undef gf_cfg_get_key
extern const char *(*gf_cfg_get_key)(GF_Config *cfgFile, const char *secName, const char *keyName);
#undef AVI_set_video
extern void (*AVI_set_video)(avi_t *AVI, int width, int height, double fps, char *compressor);
#undef gf_term_set_speed
extern void (*gf_term_set_speed)(GF_Terminal *term, Fixed speed);
#undef gf_cfg_get_key_count
extern u32 (*gf_cfg_get_key_count)(GF_Config *cfgFile, const char *secName);
#undef gf_term_object_subscene_type
extern u32 (*gf_term_object_subscene_type)(GF_Terminal *term, GF_ObjectManager *odm);
#undef gf_term_get_framerate 
extern Double (*gf_term_get_framerate)(GF_Terminal *term, Bool absoluteFPS);
#undef gf_error_to_string
extern const char *(*gf_error_to_string)(GF_Err e);
#undef gf_stretch_bits
extern GF_Err (*gf_stretch_bits)(GF_VideoSurface *dst, GF_VideoSurface *src, GF_Window *dst_wnd, GF_Window *src_wnd, u8 alpha, Bool flip, GF_ColorKey *colorKey, GF_ColorMatrix * cmat);
#undef gf_list_del
extern void (*gf_list_del)(GF_List *ptr);
#undef gf_list_get
extern void *(*gf_list_get)(GF_List *ptr, u32 itemNumber);
#undef gf_term_disconnect
extern void (*gf_term_disconnect)(GF_Terminal *term);
#undef gf_term_is_supported_url
extern Bool (*gf_term_is_supported_url)(GF_Terminal *term, const char *fileName, Bool use_parent_url, Bool no_mime_check);
#undef gf_list_new
extern GF_List *(*gf_list_new)(void);
#undef gf_modules_get_option
extern const char *(*gf_modules_get_option)(GF_BaseInterface *interface_obj, const char *secName, const char *keyName);
#undef gf_term_dump_scene
extern GF_Err (*gf_term_dump_scene)(GF_Terminal *term, char *rad_name, char **filename, Bool xml_dump, Bool skip_proto, GF_ObjectManager *odm);
#undef gf_prompt_has_input
extern Bool (*gf_prompt_has_input)();
#undef gf_term_scene_update
extern GF_Err (*gf_term_scene_update)(GF_Terminal *term, char *type, char *com);
#undef gf_term_connect
extern void (*gf_term_connect)(GF_Terminal *term, const char *URL);
#undef gf_term_get_object_count
extern u32 (*gf_term_get_object_count)(GF_Terminal *term, GF_ObjectManager *scene_od);
#undef gf_modules_get_count
extern u32 (*gf_modules_get_count)(GF_ModuleManager *pm);
#undef gf_term_get_root_object
extern GF_ObjectManager *(*gf_term_get_root_object)(GF_Terminal *term);
#undef gf_term_get_time_in_ms
extern u32 (*gf_term_get_time_in_ms)(GF_Terminal *term);
#undef gf_term_connect_with_path
extern void (*gf_term_connect_with_path)(GF_Terminal *term, const char *URL, const char *parent_URL);
#undef gf_log_set_callback
extern gf_log_cbk (*gf_log_set_callback)(void *usr_cbk, gf_log_cbk cbk);
#undef gf_log_parse_tools
extern u32 (*gf_log_parse_tools)(const char *val);
#undef gf_log_parse_level
extern u32 (*gf_log_parse_level)(const char *val);
#undef gf_term_switch_quality
extern void (*gf_term_switch_quality)(GF_Terminal *term, Bool up);
#undef gf_term_release_screen_buffer
extern GF_Err (*gf_term_release_screen_buffer)(GF_Terminal *term, GF_VideoSurface *framebuffer);
#undef gf_term_get_screen_buffer
extern GF_Err (*gf_term_get_screen_buffer)(GF_Terminal *term, GF_VideoSurface *framebuffer);
#undef gf_f64_open
extern FILE *(*gf_f64_open)(const char *file_name, const char *mode);
#undef gf_img_png_enc
extern GF_Err (*gf_img_png_enc)(char *data, u32 width, u32 height, s32 stride, u32 pixel_format, char *dst, u32 *dst_size);
#undef utf8_to_ucs4
extern u32 (*utf8_to_ucs4)(u32 *ucs4_buf, u32 utf8_len, unsigned char *utf8_buf);

