import {WebGLContext} from 'webgl'
import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'
import { AudioMixer as amix } from 'gpaccore'
import * as os from 'os'


const UPDATE_PID = 1;
const UPDATE_SIZE = 1<<1;
const UPDATE_POS = 1<<2;
const UPDATE_CHILD = 1<<3;
const UPDATE_ALLOW_STRING = 1<<4;
const UPDATE_FX = 1<<5;

/*
	global modules imported for our imported modules
*/
globalThis.sys = sys;
globalThis.evg = evg;
globalThis.os = os;

/*
	global variables for our imported modules
*/

//video playing state
globalThis.video_playing = false;
//audio playing state
globalThis.audio_playing = false;
//output video time
globalThis.video_time = 0;
//output video timescale
globalThis.video_timescale = 0;
//output video width
globalThis.video_width = 0;
//output video height
globalThis.video_height = 0;
//output audio time
globalThis.audio_time = 0;
//output audio timescale
globalThis.audio_timescale = 0;
//output audio samplerate
globalThis.samplerate =0;
//output audio channels
globalThis.channels =0;
//current UTC clock in ms
globalThis.current_utc_clock = Date.now();
//is the canvas YUV or not ? This is used at least by scenes/shape.js to cache RGB static textures to YUV
globalThis.canvas_yuv = false;
//scene update flag indicating the pid has been reconfigured
globalThis.UPDATE_PID = UPDATE_PID;
//scene update flag indicating the scene size has changed
globalThis.UPDATE_SIZE = UPDATE_SIZE;
//scene update flag indicating the scene properties other than size have changed
globalThis.UPDATE_POS = UPDATE_POS;
//scene update allows string for number
globalThis.UPDATE_ALLOW_STRING = UPDATE_ALLOW_STRING;
//scene update flag indicating that if a property with this flag is changed, mix and transition effects must be recomputed (used for OpenGL shader setup)
globalThis.UPDATE_FX = UPDATE_FX;
//indicates if GPU (WebGL) is used
globalThis.use_gpu = false;
//indicates if EVG blit is enabled
globalThis.blit_enabled = evg.BlitEnabled;

/*
	global functions for our imported modules
*/
/*gets media time of output (no args) or of source of given id. Return
 -4 if not found
 -3 if not playing
 -2 if in prefetch
 -1 if timing not yet known
 media time in seconds (float) otherwise
*/
globalThis.get_media_time = get_media_time;
/*resolves URL against media playlist URL. Returns resolved url (string)*/
globalThis.resolve_url = resolve_url;


/*
	global functions for canvas access

Each scene module is associated an array of PidLink objects.
Each transition module is passed an array of PidLink objects.

A PidLink object has the following properties:
- pid: associated input visual pid, null if source is an offscreen group
- texture: associated input visual texture (constructed by avmix, but customized by each module)

The pid of a PidLink shall not be modified. The following variables can be checked for a pid:
- pck: current pck if any
- frame_ts: current frame timestamp in output video timescale
- rotate: video Rotation property
- Mirror: video Mirror property 

Each scene must implement:
- update(): check value of this.update_flag to see if scene has to be rebuild
- fullscreen(): return index of pidlink from inputs which is fullscreen (occupies the entire scene area), or -1 if none
- identity(): return true if scene is identity: scene shows exactly the video witout any change in aspect ratio
- is_opaque(): return true if scene is opaque (entire scene area covered and not transparent), false otherwise
- draw(canvas): draw scene on canvas. The canvas can be EVG canvas or WebGL. All texture parameters modification (repeat flags, alpha, matrix, etc) must be done in this call, as the same texture may be used by different scenes

Note: scenes setup to skip drawing and forward input (fullscreen, opaque, identity) must check this.force_draw is not set to true, indicating
that the scene shall be drawn (due to canvas pixel format change for example)

Note: scenes using canvas.blit must use the provided variable this.screen_rect corresponding to the blit area on canvas, {x,y}={0,0} at top-left. If null, this means blit cannot be used (rotations & co)


*/

/*clears canvas - function shall only be used inside scene.draw()
\param color: color to use
\param clip clipper to use (type IRect) , or null/not specified
*/
globalThis.canvas_clear = canvas_clear;
/*sets axis-aligned clipper on canvas - function shall only be used inside scene.draw()
\param clip clipper to use (type IRect), or null to reset
\param use_stack if true, push and intersect the stack if clipper set or pop if clipper not set, otherwise change clipper directly without checking the clipper stack
*/
globalThis.canvas_set_clipper = canvas_set_clipper;
/*draws path on canvas - function shall only be used inside scene.draw()
\param path path (type Path) to draw
\param stencil stencil to use (type Texture or Stencil)
*/
globalThis.canvas_draw = canvas_draw;
/*blits image on canvas - function shall only be used inside scene.draw()
\param texture texture to blit (type Texture)
\param dst_rc destination window (type IRect)
*/
globalThis.canvas_blit = canvas_blit;
/*texture path on canvas - function shall only be used inside scene.draw()
The input textures are fteched from the sequences associated with the scene 
\param path path (type Path) to draw
\param op_type (=0) multitexture operand type
\param op_param (=0) multitexture operand param
\param op_param (=null) multitexture second texture
*/
globalThis.canvas_draw_sources = canvas_draw_sources;

/*sets mask mode on canvas (2D or 3D) - function shall only be used inside scene.draw()
\param mode EVG mask mode
*/
globalThis.canvas_set_mask_mode = canvas_set_mask_mode;

/*gets texture associated with an offscreen group, typically used in scene.update()
\param group_id ID of offscreen group
\return associated texture or null if not found ot not offscreen
*/
globalThis.get_group_texture = get_group_texture;

/*gets texture associated with a sequence, typically used in scene.update() and scene.draw()
\param group_id ID of offscreen group
\return associated texture or null if not found ot not offscreen
*/
globalThis.get_sequence_texture = get_sequence_texture;

/*gets screen rectangle (axis-aligned bounds) in pixels for a given path. Shall only be used inside scene.draw()
\param path the path to check
\return rectangle object or null if error
*/
globalThis.get_screen_rect = get_screen_rect;


/*gets a scene by ID
\param id ID of the scene
\return scene object or null if error
*/
globalThis.get_scene = get_scene;

/*gets a group by ID
\param id ID of the group
\return group object or null if error
*/
globalThis.get_group = get_group;

/*removes a scene, group or sequence from playlist

The caller must be a scene module, in update callback.

\param id_or_elem ID of the object or object itself
*/
globalThis.remove_element = remove_element_mod;


/*parses a root playlist element and add it to the current playlist

The caller must be a scene module, in update callback.

\param elt JSON object
*/
globalThis.parse_element = parse_playlist_element_mod;

/*parses a scene

The caller must be a scene module, in update callback.

\param elt JSON object for the scene
\param parent parent group, NULL if at root of scene tree
\return scene object or null if error

Warning: the return value may get remove later on if underlying module loading fails
*/
globalThis.parse_scene = parse_scene_mod;

/*parses a group

The caller must be a scene module, in update callback.

\param elt JSON object for the group
\param parent parent group, NULL if at root of scene tree
\return group object or null if error
*/
globalThis.parse_group = parse_group_mod;

/*parses a new playlist

The caller must be a scene module, in update callback.

If the calling scene is no longer in the resulting scene tree, it will be added to the root of the scene tree.

\param pl JSON playlist object, use empty array to reset
*/
globalThis.reload_playlist = reload_playlist;

/*update an element

The caller must be a scene module, in update callback.

\param ID ID of element to update
\param property name of the property to update
\param value new value to set
\return true if success, false otherwise
*/
globalThis.update_element = update_element_mod;

/*queries a property of an element

The caller must be a scene module, in update callback.

\param ID element ID
\param property name  of property to query
\return property if success, undefined otherwise
*/
globalThis.query_element = query_element_mod;

/*checks if a given point is over a scene
\param evt GPAC mouse event

or

\param x x-coordinate of the point in output reference sapce in pixels (0 means center, x increase towards right)
\param y y-coordinate of the point in output reference sapce in pixels (0 means center, y increase towards top)

\return scene object under the mouse, or null if none
*/
globalThis.mouse_over = mouse_over_mod;


//metadata
filter.set_name("AVMix");
filter.set_desc("Audio Video Mixer");
filter.set_author("GPAC team");

//global options
filter.set_arg({ name: "pl", desc: "local playlist file to load", type: GF_PROP_STRING, def: "avmix.json" } );
filter.set_arg({ name: "live", desc: "live mode", type: GF_PROP_BOOL, def: "true"} );
filter.set_arg({ name: "gpu", desc: `enable GPU usage
  - off: no GPU
  - mix: only render textured path to GPU, use software rasterizer for the outlines, solid fills and gradients
  - all: try to use GPU for everything`, type: GF_PROP_UINT, def: "off", minmax_enum: 'off|mix|all', hint:"advanced"} );
filter.set_arg({ name: "thread", desc: "use threads for software rasterizer (-1 for all available cores)", type: GF_PROP_SINT, def: "-1", hint:"expert"} );
filter.set_arg({ name: "lwait", desc: "timeout in ms before considering no signal is present", type: GF_PROP_UINT, def: "1000", hint:"expert"} );
filter.set_arg({ name: "ltimeout", desc: "timeout in ms before restarting child processes", type: GF_PROP_UINT, def: "4000", hint:"expert"} );
filter.set_arg({ name: "maxdur", desc: "run for given seconds and exit, will not abort if 0 (used for live mode tests)", type: GF_PROP_DOUBLE, def: "0", hint:"expert"} );
filter.set_arg({ name: "updates", desc: "local JSON files for playlist updates", type: GF_PROP_STRING, hint:"advanced"} );
filter.set_arg({ name: "maxdepth", desc: "maximum depth of a branch in the scene graph", type: GF_PROP_UINT, def: "100", hint:"expert"} );

//video output options
filter.set_arg({ name: "vsize", desc: "output video size, 0 disable video output", type: GF_PROP_VEC2, def: "1920x1080"} );
filter.set_arg({ name: "fps", desc: "output video frame rate", type: GF_PROP_FRACTION, def: "25"} );
filter.set_arg({ name: "pfmt", desc: "output pixel format. Use \`rgba\` in GPU mode to force alpha channel", type: GF_PROP_PIXFMT, def: "yuv"} );
filter.set_arg({ name: "dynpfmt", desc: `allow dynamic change of output pixel format in software mode
  - off: pixel format is forced to desired value
  - init: pixel format is forced to format of fullscreen input in first generated frame
  - all: pixel format changes each time a full-screen input PID at same resolution is used`, type: GF_PROP_UINT, def: "init", minmax_enum: 'off|init|all', hint:"expert"} );

//audio output options
filter.set_arg({ name: "sr", desc: "output audio sample rate, 0 disable audio output", type: GF_PROP_UINT, def: "44100"} );
filter.set_arg({ name: "ch", desc: "number of output audio channels, 0 disable audio output", type: GF_PROP_UINT, def: "2"} );
filter.set_arg({ name: "afmt", desc: "output audio format (only s16, s32, flt and dbl are supported)", type: GF_PROP_PCMFMT, def: "s16"} );
filter.set_arg({ name: "alen", desc: "default number of samples per frame", type: GF_PROP_UINT, def: "1024", hint:"expert"} );

filter.max_pids = -1;
filter.make_sticky();
filter.maxdur=0;

let sources = [];
let reload_tests=null;
let reload_timeout=1.0;
let reload_loop=false;
let reload_idx=0;

let last_modification = 0;
let last_updates_modification = 0;
let init_utc = 0;
let init_clock_us = 0;

const TYPE_UNHANDLED = 0;
const TYPE_VIDEO = 1;
const TYPE_AUDIO = 2;

let playlist_url = null;

let active_scene = null;
let scene_in_update = null;

const SCENE_GL_NONE=0;
const SCENE_GL_ANY=1;
const SCENE_GL_ONLY=2;


let terminated = false;

let back_color = 'black';

let playlist_loaded = false;

filter.set_cap({id: "StreamType", value: "Visual", inout: true} );
filter.set_cap({id: "StreamType", value: "Audio", inout: true} );
filter.set_cap({id: "CodecID", value: "raw", inout: true} );

let modules_pending = 0;

let webgl = null;
filter.frame_pending=false;
let single_mod_help=false;
let mod_help_short=false;

const GROUP_OST_NONE = 0;
const GROUP_OST_DUAL = 1;
const GROUP_OST_MASK = 2;
const GROUP_OST_COLOR = 3;

const UNIT_RELATIVE = 0;
const UNIT_PIXEL = 1;


function build_help_mod(obj, name, mod_type, index)
{
	name = name.split('.')[0];
	let rad = '';
	if (mod_type==0) {
		if (!index) {
			filter._help += (mod_help_short ? '\n' : '# ' ) + 'Scene modules\n';
		}
		rad = 'Scene';
	}
	else if (mod_type==1) {
		if (!index) {
			filter._help += (mod_help_short ? '\n' : '# ' ) + 'Transition modules\n';
		}
		rad = 'Transition';
	}

	if (mod_help_short) {
		filter._help += ' - ' + name + ': ' + obj.description + '\n';
		filter.set_help(filter._help);
		return;
	} 

	filter._help += (single_mod_help ? '# ' : '## ') + rad + ' `' + name + '`';
	if (mod_type==1) {
		let inst = obj.load();
		if (typeof inst.get_shader_src == 'function') {
  		if (typeof inst.setup != 'function') {
				filter._help += ' - GPU only';
  		} else {
				filter._help += ' - software/GPU';  			
  		}
		} else {
				filter._help += ' - software only';  						
		}
	}
	filter._help += '\n' + obj.help + '\n';

	if (obj.options.length)
		filter._help += 'Options:\n';
	for (let i=0; i<obj.options.length; i++) {
		let opt = obj.options[i];
		if (typeof opt.name == 'undefined') continue;
		if (typeof opt.desc == 'undefined') continue;

		filter._help += '- ' + opt.name;
		let opts = "";
		if (typeof opt.value != 'undefined') {
			if (Array.isArray(opt.value)) {
				opts+='[]';
			} else if (typeof opt.value!='string') {
				opts+=opt.value;
			} else /*if (opt.value.length)*/ {
				opts+='\''+opt.value+'\'';
			}
		}
		if (mod_type==0) {
			if ((typeof opt.dirty == 'undefined') || (opt.dirty==-1)) {
				opts+=', not updatable';
			}
		}

		if (opts.length)
			filter._help += ' (' + opts + ')';

		filter._help += ': ' + opt.desc;
		filter._help += '\n';
	}
	filter.set_help(filter._help);
}

function build_help(obj, playlist_only)
{
	let path = filter.jspath;
	if (playlist_only) {
		filter._help = obj.help_playlist + '\n';
		filter.set_help(filter._help);
		return;
	}

	filter._help = obj.help + '\n';
	if (mod_help_short) {
		filter._help += '# More info\n';
		filter._help += 'For playlist syntax, use `gpac -h avmix:playlist`\n';
		filter._help += 'For module `NAME`, use `gpac -h avmix:NAME`\n';
		filter._help += 'For global variables available to modules, use `gpac -h avmix:global`\n';
		filter._help += '# Available modules\n';
	} else {
		filter._help += obj.help_playlist + '\n';
	}

	let	scripts = sys.enum_directory(path+'scenes/', "*.js");
	for (let i=0; i<scripts.length; i++) {
		let name = scripts[i].name;
		import('./scenes/' + name).then(obj => { build_help_mod(obj, name, 0, i); }).catch(err => {});
	}
	scripts = sys.enum_directory(path+'transitions/', "*.js");
	for (let i=0; i<scripts.length; i++) {
		let name = scripts[i].name;
		import('./transitions/' + name).then(obj => { build_help_mod(obj, name, 1, i); }).catch(err => {});
	}
	filter.set_help(filter._help);
}

filter.initialize = function() 
{
	let gpac_help = sys.get_opt("temp", "gpac-help");
	let gpac_doc = (sys.get_opt("temp", "gendoc") == "yes") ? true : false;
	if (gpac_help || gpac_doc) {
		let args = sys.args;
		globalThis.help_mod = gpac_doc ? null : sys.get_opt("temp", "gpac-js-help");

		if (help_mod) {
			let name;
			let path = filter.jspath;
			let	script = path+'scenes/'+help_mod+'.js';
			filter._help = '';
			single_mod_help = true;
			if (help_mod == 'global') {
				filter._help = 'Global JS properties available:\n';
				for (var propertyName in globalThis) {
					//skip GPAC constants
					if (propertyName.startsWith('GF_')) continue;
					if (propertyName.startsWith('JSF_')) continue;

					filter._help += '- ' + propertyName+ ': type ' + (typeof globalThis[propertyName]) + '\n';
				}
				filter.set_help(filter._help);
				return GF_OK;
			}
			if (help_mod == 'playlist') {
				import("./help.js").then(obj => { build_help(obj, true); }).catch(err => {});
				return GF_OK;
			}

			if (sys.file_exists(script)) {
				name = help_mod + '.js';
				import('./scenes/' + name).then(obj => { build_help_mod(obj, name, 0, 1); }).catch(err => {});
				return GF_OK;
			} 
			script = path+'transitions/'+help_mod+'.js';
			if (sys.file_exists(script)) {
				name = help_mod + '.js';
				import('./transitions/' + name).then(obj => { build_help_mod(obj, name, 1, 1); }).catch(err => {});
				return GF_OK;
			}
			print(GF_LOG_ERROR, 'No such module ' + help_mod + ' path ' + script);
			return GF_OK;
		}
		if ((gpac_help == "h") || (gpac_help == "ha"))
			mod_help_short = !gpac_doc;
		import("./help.js").then(obj => { build_help(obj, false); }).catch(err => {});
		return GF_OK;
	}

	if (!this.pl) {
		print(GF_LOG_ERROR, "No playlist specified, cannot run");
		return GF_BAD_PARAM;
	}
	if (!sys.file_exists(filter.pl)) {
		print(GF_LOG_ERROR, "Playlist " + filter.pl + " not found, cannot run");
		return GF_BAD_PARAM;
	}

	playlist_url = filter.pl.slice();
	let cwd = os.getcwd()[0] + '/';
	playlist_url = sys.url_cat(cwd, playlist_url);

	if (!load_playlist())
		return GF_BAD_PARAM;
	playlist_loaded = true;

	if (filter.updates) {
		last_updates_modification = sys.mod_time(filter.updates);
	}

	if (filter.live) {
		init_utc = sys.get_utc();
		init_clock_us = sys.clock_us();
	}
	if (filter.maxdepth<=0)
		filter.maxdepth = 100;

	if (this.gpu) {
		video_width = filter.vsize.x;
		video_height = filter.vsize.y;
			try {
				webgl = new WebGLContext(video_width, video_height);
				//by default do not output alpha channel
				if (filter.pfmt != 'rgba')
					filter.pfmt = 'rgb';
				globalThis.use_gpu = true;
				globalThis.blit_enabled = false;
			} catch (e) {
				print(GF_LOG_ERROR, "Failed to initialize WebGL, disabling GPU support");
				this.gpu = 0;
				globalThis.use_gpu = false;
				globalThis.blit_enabled = evg.BlitEnabled;
			}
	}

	configure_vout();
	configure_aout();
	if (filter.live)
		print(GF_LOG_INFO, 'Initialized at ' + new Date());;

	if (!filter.ltimeout)
			filter.ltimeout = 1000;
	
	if (filter.lwait > filter.ltimeout)
		filter.lwait = filter.ltimeout;
}


let pids=[];
let vout = null;
let aout = null;

let video_frame_dur_us = 0;
let video_max_resched_dur_us = 0;
let audio_frame_dur_us = 0;
let audio_max_resched_dur_us = 0;

let video_pfmt =0;
let vout_size =0;
let canvas = null;
let canvas_offscreen = null;
let canvas_reconfig = false;
let video_time_inc =0;

let audio_afmt =0;
let sample_size=0;
let max_aframe_size=0;
const sample_per_frame=1024;
let chan_buffer = null;

let sequences = [];

//static top-level group
let root_scene = {};
root_scene.scenes = [];
root_scene.parent = null;
root_scene.update_flag = 0;

let wait_for_pids = 0;
let wait_pid_play = 0;
let live_forced_play = false;

let use_canvas_3d = false;

let defaultOrthoProjectionMatrix = null;

let audio_mix = null;
let generate_default_scene = true;



function configure_vout()
{
	if (!filter.vsize.x || !filter.vsize.y) {
		if (vout) vout.remove();
		vout = null;	
		return;
	}
	if (filter.gpu) {
		print(GF_LOG_INFO, (filter.live ? 'Live ' : '' ) + 'Video output ' + filter.vsize.x + 'x' + filter.vsize.y + ' FPS ' + filter.fps.n + '/' + filter.fps.d + ' OpenGL pixfmt ' + filter.pfmt);
	} else {
		print(GF_LOG_INFO, (filter.live ? 'Live ' : '' ) + 'Video output ' + filter.vsize.x + 'x' + filter.vsize.y + ' FPS ' + filter.fps.n + '/' + filter.fps.d + ' pixfmt ' + filter.pfmt);
	}

	if (!vout) {
		vout = filter.new_pid();
		vout.name = "video";
		wait_pid_play++;
	} else if ((video_width==filter.vsize.x) && (video_height==filter.vsize.y) && (video_pfmt===filter.pfmt)) {
		return;
	}
	video_width = filter.vsize.x;
	video_height = filter.vsize.y;
	video_pfmt = filter.pfmt;

	vout.set_prop('StreamType', 'Visual');
	vout.set_prop('Width', video_width);
	vout.set_prop('Height', video_height);
	vout.set_prop('PixelFormat', video_pfmt);
	vout.set_prop('CodecID', 'raw');
	vout.set_prop('FPS', filter.fps);
	video_timescale = filter.fps.n;
	video_time_inc = filter.fps.d;
	video_frame_dur_us = 1000000 * video_time_inc / video_timescale;
	video_max_resched_dur_us = video_frame_dur_us/3;
	vout.set_prop('Timescale', filter.fps.n);
	//do not buffer output, this would trigger block on vout and break sync
	vout.set_prop('BufferLength', 0);

	if (sys.pixfmt_transparent(video_pfmt)) {
		print(GF_LOG_INFO, 'Enabling alpha on video output');
		back_color = 'none';
	}

	if (!webgl)
		set_canvas_yuv(video_pfmt);

	vout_size = sys.pixfmt_size(filter.pfmt, filter.vsize.x, filter.vsize.y);
	canvas_reconfig = true;

	defaultOrthoProjectionMatrix = new evg.Matrix().ortho(-video_width/2, video_width/2, -video_height/2, video_height/2, 1, -1);
}

function configure_aout()
{
	if (!filter.sr || !filter.ch) {
		if (aout) aout.remove();
		aout = null;	
		return;
	}
	print(GF_LOG_INFO, (filter.live ? 'Live ' : '' ) + 'Audio output ' + filter.sr + ' Hz ' + filter.ch + ' Channels');

	if (!aout) {
		aout = filter.new_pid();
		aout.name = "audio";
		wait_pid_play++;
	} else if ((samplerate==filter.sr) && (channels==filter.ch) && (audio_sfmt===filter.afmt)) {
		return;
	}
	samplerate = filter.sr;
	channels = filter.ch;
	audio_afmt = filter.afmt;

	aout.set_prop('StreamType', 'Audio');
	aout.set_prop('SampleRate', samplerate);
	aout.set_prop('NumChannels', channels);
	aout.set_prop('AudioFormat', audio_afmt);
	aout.set_prop('CodecID', 'raw');
	audio_timescale = samplerate;
	aout.set_prop('Timescale', audio_timescale);
	//do not send clock hints for aout, this would trigger block on vout and break sync
	aout.set_prop('BufferLength', 0);

	sample_size = sys.pcmfmt_depth(audio_afmt);
	max_aframe_size = sample_size * channels * sample_per_frame;
	chan_buffer = new Float32Array(channels);

	if (audio_afmt === 's32') {
		filter.get_sample = function(buf, idx) { return buf.getInt32(idx, true) / 4294967295; };
		filter.set_sample = function(buf, idx, val) { return buf.setInt32(idx, val * 4294967295, true); };
	} else if (audio_afmt === 's16') {
		filter.get_sample = function(buf, idx) { return buf.getInt16(idx, true) / 65535; };
		filter.set_sample = function(buf, idx, val) { return buf.setInt16(idx, val*65535, true); };
	} else if (audio_afmt === 'flt') {
		filter.get_sample = function(buf, idx) { return buf.getFloat32(idx); };
		filter.set_sample = function(buf, idx, val) { return buf.setFloat32(idx, val); };
	} else if (audio_afmt === 'dbl') {
		filter.get_sample = function(buf, idx) { return buf.getFloat64(idx); };
		filter.set_sample = function(buf, idx, val) { return buf.setFloat64(idx, val); };
	} else {
		print(GF_LOG_ERROR, 'Unsupported sample format ' + audio_afmt);
		sys.exit(1);
	}
  if (!filter.alen) filter.alen = 100;

  //above 20 samples consider this is a time discontinuity
  //use 50 max samples for cross-fade
  audio_mix = new amix(audio_afmt, channels, 20, 50);

	audio_frame_dur_us = 1000000 * filter.alen / audio_timescale;
	audio_max_resched_dur_us = audio_frame_dur_us/3;
}

filter.configure_pid = function(pid) 
{
	if (pids.indexOf(pid)<0) {
		pids.push(pid);
		pid.skipped = false;
		let s = get_source_by_pid(pid);
		if (s) {
			s.pids.push(pid);
			pid.source = s;
			if (s.playing) play_pid(pid, s);
			pid.pck = null;
			pid.old_timescale = pid.timescale;
			pid.is_blocking = false;
			pid.done = false;
			pid.fade = 0;
			pid.active = false;
			pid.texture = null;
			pid.audio_fade = null;
			s.timeline_init = false;
			pid.last_pck_time = /*	current_utc_clock =*/ Date.now();
		} else {
			if (!pid.skipped) {
				print(GF_LOG_ERROR, 'PID ' + pid.name + ' coming from unknown source !');
				return GF_FILTER_NOT_SUPPORTED;
			}
			return GF_EOS;
		}
	}

	let p = pid.get_prop('StreamType');
	let ts = 0;
	if (p == 'Visual') {
		//silently ignore
		if (!vout) return GF_EOS;

		pid.pfmt = pid.get_prop('PixelFormat');
		pid.type = TYPE_VIDEO;
		ts = video_timescale;

		pid.width = pid.get_prop('Width');
		pid.height = pid.get_prop('Height');
		pid.reconfigure = true;
		pid.mirror = pid.get_prop('Mirror');
		pid.rotate = pid.get_prop('Rotate');
	}	
	else if (p == 'Audio') {
		//silently ignore
		if (!aout) return GF_EOS;

		pid.type = TYPE_AUDIO;
		pid.data = null;
		ts = audio_timescale;

		pid.channels = pid.get_prop('NumChannels');
		pid.afmt = pid.get_prop('AudioFormat');
		let sr = pid.get_prop('SampleRate');
		if ((sr != filter.sr) || (pid.afmt != filter.afmt)) {
			pid.negociate_prop('SampleRate', filter.sr);
			pid.negociate_prop('AudioFormat', filter.afmt);
		}

		pid.ch_buf = new Float32Array(pid.channels);
		pid.samples_used = 0;
		pid.last_sample_time = 0;
	} else {
		pid.type = TYPE_UNHANDLED;
		//silently ignore
		return GF_EOS;
	}

	if (pid.timescale != ts) {
		pid.translate_timestamp = function(value) {
			value -= pid.init_ts;
			value *= ts;
			value /= pid.timescale;
			value += pid.init_clock;
			return value;
		};
	} else {
		pid.translate_timestamp = function(value) {
			value -= pid.init_ts;
			value += pid.init_clock;
			return value;
		};
	}

  let dur = pid.get_prop('Duration');
  if (dur && dur.d && (dur.n > pid.source.duration * dur.d))
  		pid.source.duration = dur.n / dur.d;

	if (pid.old_timescale != pid.timescale) {
		//todo
		pid.old_timescale = pid.timescale;
	}
	do_traverse(root_scene, scene => {
		scene.sequences.forEach(seq => {
			//this is an offscreen group
			if (typeof seq.offscreen != 'undefined') return;

			seq.sources.forEach(src => {
				if (pid.source === src) scene.resetup_pids = true;
			}); 
		}); 
	});
}

filter.remove_pid = function(pid) 
{
	let index = pids.indexOf(pid);
	if (pid.pck) {
		pid.pck.unref();
		pid.pck = null;
	}
  if (index < 0) return;
  pids.splice(index, 1);

  index = pid.source.pids.indexOf(pid);
  if (index>=0)
		pid.source.pids.splice(index, 1);

	do_traverse(root_scene, scene => {
		for (let i=0; i<scene.mod.pids.length; i++) {
			let pid_link = scene.mod.pids[i];
			if (pid_link.pid === pid) {
				scene.mod.pids.splice(i, 1);
				scene.resetup_pids = true;
				break;
			}
		}
		for (let i=0; i<scene.apids.length; i++) {
			let pid_link = scene.apids[i];
			if (pid_link.pid === pid) {
				scene.resetup_pids = true;
				break;
			}
		} 
	});
}

filter.update_arg = function(name, val)
{
}

filter.process_event = function(pid, evt)
{
	if (evt.type == GF_FEVT_USER) {

		event_watchers.forEach(watcher => {
			if ((watcher.evt>0) && (watcher.evt != evt.ui_type)) return;

			if (watcher.evt_param && (watcher.evt_param != sys.keyname(evt.keycode) ) ) return;

			if (watcher.mode==2) {
				watcher.fun.apply(watcher, [evt]);
			} else if (watcher.mode==3) {
				if (watcher.scene.mod && (typeof watcher.scene.mod[watcher.target] == 'function')) {
					watcher.scene.mod[watcher.target].apply(watcher.scene.mod, [evt]);
				}
			}
		});
		return;
	}

	if (evt.type == GF_FEVT_STOP) { 
		if (pid==aout) {
			audio_playing = false;
			if (!wait_pid_play) aout.eos = true;
		}
		else if (pid==vout) {
			video_playing = false;
			if (!wait_pid_play) vout.eos = true;
		}

		if (!audio_playing && !video_playing && !wait_pid_play) {
			do_terminate();

			//last stop, don't cancel
			if (!wait_pid_play) return false;
		}
		return true;
	} 
	if (evt.type == GF_FEVT_PLAY) { 
		if (pid==aout) {
			audio_playing = true;
		} else if (pid==vout) {
			video_playing = true;
		}
		terminated = false;
		if (wait_pid_play)
			wait_pid_play--;
		return true;
	}
	//cancel all other events	
	return true;
}

let last_forward_pixfmt = 0;
let video_inputs_ready, audio_inputs_ready;

let blocking_ref_pids = [];
let purge_sources = [];


function do_traverse(elm, fun)
{
	if (Array.isArray(elm.scenes)) {
		for (let i=0; i<elm.scenes.length; i++) {
			let scene = elm.scenes[i];
			let res = do_traverse(scene, fun);
			if (res == true) {
				return;
			}
		}
	} else {
		fun(elm);
	}
}


function do_traverse_all(elm, fun)
{
	let ret = fun(elm) || false;
	if (ret == true) return;

	if (Array.isArray(elm.scenes)) {
		elm.scenes.forEach(scene => {
			do_traverse_all(scene, fun);
		});
	}
}

let indent=0;
function do_traverse_all_indent(elm, fun)
{
	let ret = fun(elm) || false;
	if (ret == true) return;

	if (Array.isArray(elm.scenes)) {
		indent++;
		elm.scenes.forEach(scene => {
			do_traverse_all_indent(scene, fun);
		});
		indent--;
	}
}

function do_terminate()
{
	terminated = true;
	pids.forEach(pid => {
		if (pid.pck) {
			pid.pck.unref();
			pid.pck = null;
		}
		pid.send_event( new FilterEvent(GF_FEVT_STOP) ); 
		pid.discard = true;
	});
	root_scene.scenes.length = 0;
	sources.forEach(s => { stop_source(s, true); });
	filter.abort();
}

let do_audio=false;
let next_audio_gen_time = 0;
let next_video_gen_time = 0;

let last_pl_test_switch = 0;

filter.process = function()
{
	let do_video = false;
	if (terminated)
		return GF_EOS;

	if (filter.maxdur && (video_time || audio_time) ) {
		let abort = true;
		if (video_playing && (video_time < filter.maxdur*video_timescale)) abort = false;
		if (audio_playing && (audio_time < filter.maxdur*audio_timescale)) abort = false;
		if (wait_for_pids) abort = false;
		if (abort) {
			print(GF_LOG_INFO, 'maxdur ' +  filter.maxdur + ' reached, quit - video time ' + video_time + '/' + video_timescale + ' - audio time ' + audio_time + '/' + audio_timescale);
			do_terminate();
			return;
		}
	}

	if (vout && video_playing) {
		//if no audio or video behind audio, generate frame if not blocking
		if (!audio_playing || (audio_time * video_timescale >= video_time * audio_timescale)) {
			if (!vout.would_block && !filter.frame_pending) {
				do_video = true;
			}
		}
		//audio ahead of next video frame, don't do audio - if not generating video, do nothing 
		if (!do_video && (!audio_playing || (audio_time * video_timescale > (video_time+video_time_inc) * audio_timescale))) {
			//notfiy we are still alive
			filter.reschedule(0);
			if (filter.live) {
				let now = sys.clock_us();
				let next_time = (next_video_gen_time<next_audio_gen_time) ? next_video_gen_time : next_audio_gen_time;

				let diff = next_time - now;
				if (diff>1000)
					filter.reschedule(diff);
			}
			return;
		}
	}

	current_utc_clock = Date.now();

	if (reload_tests && (reload_idx<reload_tests.length) ) {
		if (last_pl_test_switch + reload_timeout*video_timescale < video_time) {
			last_pl_test_switch = video_time;
			filter.pl = sys.url_cat(filter.pl, reload_tests[reload_idx]);
			last_modification = 0;
			reload_idx++;
			if (reload_idx>=reload_tests.length) {
				if (reload_loop>=0) {
					reload_loop --;
					if (reload_loop<0) reload_loop=0;
				}
				if (reload_loop) reload_idx=0;
			}
		}
	}
	load_playlist();
	load_updates();
	//modules are pending, wait for fail/success - async cannot wait here because QuickJS will only try to load the modules once out of JS functions
	if (modules_pending)
		return GF_OK;

	//we had pending events, waiting for import module resolutions, flush them
	if (watchers_defered.length) {
		flush_watchers();
	}


	video_inputs_ready = video_playing;
	audio_inputs_ready = audio_playing;
	inactive_sources = 0;
	blocking_ref_pids.length = 0;
	sources.forEach(fetch_source);

	if (wait_for_pids) {
		let lwait = filter.lwait;
		if (!filter.live) lwait *= 10;
		if (current_utc_clock - wait_for_pids > lwait) {
			if (filter.live) {
				print(GF_LOG_WARNING, 'No input connection after ' + (current_utc_clock - wait_for_pids) + ' ms, starting');
				wait_for_pids = 0;
				live_forced_play = true;
			} else {
				print(GF_LOG_ERROR, 'No input connection after ' + (current_utc_clock - wait_for_pids) + ' ms, aborting');
				do_terminate();
				return;
			}
		}
		if (filter.connections_pending) {
			filter.reschedule(1000);
			return;
		}
		wait_for_pids = 0;
	}

	if (inactive_sources == sources.length) {
		if (filter.live) {
			//inactive, reschedule in 2 ms. A better way would be to compute next activation time
			filter.reschedule(2000);
			//but do generate frame
		} else {
			if (vout) vout.eos = true;
			if (aout) aout.eos = true;
			do_terminate();
			print(GF_LOG_INFO, 'sources are over, quit');
			return GF_EOS;
		}
	}

	timers.forEach(update_timer);
	update_scripts();

	if (filter.maxdur && (video_time >= filter.maxdur*video_timescale)) {
		if (do_video) {
			//no longer generating video, increase time for audio generation
			video_time += video_time_inc;
			do_video = false;
		}
	}

	do_audio = (aout && audio_playing) ? true : false;
	if (vout && video_playing) {
		if (do_video) {
			process_video();
		}
		//this typically happens upon init
		if (audio_playing && (audio_time * video_timescale > (video_time+video_time_inc) * audio_timescale)) {
			return;
		}
	}

	if (filter.maxdur && (audio_time >= filter.maxdur*audio_timescale)) {
		do_audio = false;
	}

	if (do_audio) {
		//video is not playing, we must update the scenes for audio
		if (!vout) update_display_list();

		process_audio();
		if (!video_playing) {
			video_time = audio_time * video_timescale;
			video_time /= audio_timescale;
		}
	}

	blocking_ref_pids.forEach(pid => {
		pid.pck.unref();
		pid.pck = null;
	});
	purge_sources.forEach(s => {
		sources.splice(sources.indexOf(s), 1);
	});
	purge_sources.length = 0;
}

function scene_resetup_pids(scene)
{
	//purge pid links
	scene.mod.pids.length = 0;
	scene.apids.length = 0;

	let prefetching = [];
	//check all pids from sources
	scene.sequences.forEach(seq => {
		//this is an offscreen group
		if (typeof seq.offscreen != 'undefined') {
			let pid_link = {};
			pid_link.pid = null;
			pid_link.sequence = null;
			pid_link.texture = seq.texture;
			scene.mod.pids.push(pid_link);
			return;
		}

		seq.sources.forEach(src => {
			if (src.in_prefetch) return;
			src.pids.forEach(pid => {	

				//check audio pids
		    if (pid.type == TYPE_AUDIO) {
					scene.apids.push(pid);
					return;
		    }

				//only check for video pids
		    if (pid.type != TYPE_VIDEO)
					return;

				//add pid link
				let pid_link = {};
				pid_link.pid = pid;
				pid_link.texture = null;
				pid_link.sequence = seq;
				if (pid.source.next_in_transition)
					prefetching.push(pid_link);
				else
					scene.mod.pids.push(pid_link);
			});
		}); 
	});

	prefetching.forEach(pid_link => {
		print(GF_LOG_DEBUG, 'transitioning to ' + pid_link.pid.source.logname + '.' + pid_link.pid.name);
		scene.mod.pids.push(pid_link);
	});

	scene.mod.pids.forEach(pid_link => {
		if (pid_link.pid)
			print(GF_LOG_DEBUG, 'created PID link for ' + pid_link.pid.source.logname + '.' + pid_link.pid.name);
	});

}

function create_pid_texture(pid)
{
	pid.texture = new evg.Texture(pid.pck);
	pid.texture._gl_modified = true;
	pid.texture.last_frame_ts = pid.frame_ts;
}

function scene_update_visual_pids(scene)
{
	let ready = true;
	scene.mod.pids.forEach(pidlink => {
		//offscreen group
		if (!pidlink.pid) return;

		if (!pidlink.pid.pck) {
			if (pidlink.pid.source.in_prefetch) {
				return;			
			}
			if (pidlink.pid.source.no_signal) {
				if (pidlink.texture) return;
			}
			ready = false;
			return;
		}

		//create texture object at PID level, so we don't end up uploading twice the same data to gpu
		if (!pidlink.pid.texture) {
			if (!pidlink.pid.pck) {
				ready = false;
				return;				
			}
			create_pid_texture(pidlink.pid);
		}

		if (!pidlink.texture) {
			pidlink.texture = pidlink.pid.texture;
			scene.mod.update_flag |= UPDATE_SIZE;
		} else if (pidlink.pid.frame_ts != pidlink.texture.last_frame_ts) {
			pidlink.texture.update(pidlink.pid.pck);
		  pidlink.texture._gl_modified = true;
		  pidlink.texture.last_frame_ts = pidlink.pid.frame_ts;
		}
	});
	return ready;
}


let round_scene_size = 0;

let global_transform = new evg.Matrix2D();
let global_branch_depth = 0;
let active_camera = null;

function update_scene_matrix(scene)
{
	let x=scene.x, y=scene.y, z=scene.z, cx=scene.cx, cy=scene.cy, cz=scene.cz;
	let rotation = scene.rotation;
	let hskew = scene.hskew;
	let vskew = scene.vskew;
	let hscale = scene.hscale;
	let vscale = scene.vscale;
	let zscale = scene.zscale;
	let axis = scene.axis;
	let orientation = scene.orientation;

	if (scene.untransform)	
		scene.mx_untransform = true;
	else if (scene.mx_untransform)
		scene.mx_untransform = false;

	//compute absolute values
	if (scene.units == UNIT_RELATIVE) {
		if (typeof x == 'number')
			x = x * reference_width/100;
		if (typeof y == 'number')
			y = y * reference_height/100;
		cx = cx * reference_width/100;
		cy = cy * reference_height/100;

		z = z * reference_height/100;
		cz = cz * reference_height/100;
	}
	//apply special values
	if (x == "y") x = y;
	else if (x == "-y") x = -y;

	if (y == "x") y = x;
	else if (y == "-x") y = -x;

	scene.mx.identity = true;

	try {
		if (scene.mxjs) {
			let is_ok = true;
			let mx_state = {
					x: x, y: y, z: z,
					cx: cx, cy: cy, cz,
					rotation: rotation,
					hscale: hscale, vscale: vscale, zscale: zscale,
					hskew: hskew, vskew: vskew,
					axis: axis,
					orientation: orientation,

					untransform: scene.mx_untransform, update:false, depth: scene.current_depth
			};

			let res = scene.mxjs.apply(scene, [mx_state]);
			if (mx_state.update) scene.update_flag = UPDATE_CHILD;
			if ((typeof res == 'boolean') && !res) {
				return false;
			}
			scene.mx_untransform = mx_state.untransform ? true : false;

			if (typeof mx_state.x == 'number') x = mx_state.x;
			else is_ok = false;
			if (typeof mx_state.y == 'number') y = mx_state.y;
			else is_ok = false;
			if (typeof mx_state.z == 'number') z = mx_state.z;
			else is_ok = false;

			if (typeof mx_state.cx == 'number') cx = mx_state.cx;
			else is_ok = false;
			if (typeof mx_state.cy == 'number') cy = mx_state.cy;
			else is_ok = false;
			if (typeof mx_state.cz == 'number') cz = mx_state.cz;
			else is_ok = false;

			if (typeof mx_state.rotation == 'number') rotation = mx_state.rotation;
			else is_ok = false;

			if (typeof mx_state.hscale == 'number') hscale = mx_state.hscale;
			else is_ok = false;
			if (typeof mx_state.vscale == 'number') vscale = mx_state.vscale;
			else is_ok = false;
			if (typeof mx_state.zscale == 'number') zscale = mx_state.zscale;
			else is_ok = false;

			if (typeof mx_state.hskew == 'number') hskew = mx_state.hskew;
			else is_ok = false;
			if (typeof mx_state.vskew == 'number') vskew = mx_state.vskew;
			else is_ok = false;

			if (!mx_state.axis || Array.isArray(mx_state.axis)) axis = mx_state.axis;
			else is_ok = false;
			if (!mx_state.orientation || Array.isArray(mx_state.orientation)) orientation = mx_state.orientation;
			else is_ok = false;

			if (!is_ok) {
				print(GF_LOG_ERROR, 'Error processing mxjs, result not a valid object');
				scene.mxjs = null;
			}
		}
	} catch (e) {
		print(GF_LOG_ERROR, 'Error processing mxjs : ' + e);
		scene.mxjs = null;
	}

	//validate axis, if 0,0,1 move to null (no 3D rotation)
	if (axis) {
		if (axis.length!=3) axis = null;
		else if (!axis[0] && !axis[1] && (axis[2]>0)) axis = null;
	}

	//validate orientation, if no angle, move to null (no 3D oriented scaling)
	if (orientation) {
		if (orientation.length!=4) orientation = null;
		else if (!orientation[3]) orientation = null;
	}

	let is_3d = false;
	if (z || cz
		|| (zscale != 1)
		|| orientation
		|| axis
	) {
		is_3d = true;
	}

	if (!is_3d) {
		if (scene.mx.is3D) scene.mx = new evg.Matrix2D();
		scene.mx.rotate(cx, cy, rotation * Math.PI / 180);
		if (hskew) scene.mx.skew_x(hskew);
		if (vskew) scene.mx.skew_y(vskew);
		scene.mx.scale(hscale, vscale);
		scene.mx.translate(x, y);
		return true;
	}
	//3D matrix
	if (!scene.mx.is3D) scene.mx = new evg.Matrix();
	if (x || y || scene.z)
		scene.mx.translate(x, y, z);

	let recenter = (cx || cy || cz) ? 1 : 0;
	if (recenter)
		scene.mx.translate(cx, cy, cz);

	if (rotation && axis) scene.mx.rotate(axis[0], axis[1], axis[2], rotation * Math.PI / 180);
	let	scale_rot = (orientation && orientation[3]) ? 1 : 0;
	if (scale_rot)
		scene.mx.rotate(orientation[0], orientation[1], orientation[2], orientation[3] * Math.PI / 180);

	if ((hscale != 1) || (vscale != 1) || (zscale != 1))
		scene.mx.scale(hscale, vscale, zscale);
	if (scale_rot)
		scene.mx.rotate(orientation[0], orientation[1], orientation[2], -orientation[3] * Math.PI / 180);

	if (recenter)
		scene.mx.translate(-cx, -cy, -cz);
	return true;
}

function get_dim(scene, is_width)
{
	let val;
	if (is_width) {
		val = scene.width;
		if (val == -1) return reference_width;
		if (val == "height") {
			if (scene.height == "width") return 0;
			return get_dim(scene, false);
		}
		if (scene.units == UNIT_RELATIVE)
			return val * reference_width / 100;
		return val;
	}

	val = scene.height;
	if (val == -1) return reference_height;
	if (val == "width") {
		if (scene.width == "height") return 0;
		return get_dim(scene, true);
	}
	if (scene.units == UNIT_RELATIVE)
		return val * reference_height / 100;
	return val;
}

let group_bounds = null;

let reference_width = 0;
let reference_height = 0;

function group_draw_offscreen(group)
{
	//store display list and canvas state
	let has_opaque_bck = has_opaque;
	let	display_list_bck = display_list;
	let global_transform_copy = global_transform.copy();
	let use_gpu_bck = globalThis.use_gpu;
	let canvas_bck = canvas;
	let webgl_bck = webgl;
	let round_scene_bck = round_scene_size;
	let group_bounds_bck = group_bounds;
	//reset state
	globalThis.use_gpu = false;
	globalThis.blit_enabled = false;
	global_transform.identity = true;
	display_list = [];
	webgl = null;
	round_scene_size = 0;

	//traverse while collecting bounds
	group_bounds = {x: 0,x: 0,w: 0,h: 0,}


	let ref_width_bck = reference_width;
	let ref_height_bck = reference_height;
	if (group.reference && (group.width>=0) && (group.height>=0)) {
		let rw = get_dim(group, true);
		let rh = get_dim(group, false);
		reference_width = rw;
		reference_height = rh;
	}

	group.scenes.forEach(scene => do_traverse_all(scene, update_scene));

	reference_width = ref_width_bck;
	reference_height = ref_height_bck;

	group.skip_draw = false;
	if ((group_bounds.w <= 0) || (group_bounds.h <= 0) || (group.width==0) || (group.height==0)) {
		group.skip_draw = true;
	}

	let old_w, old_h;
	if (group.canvas_offscreen) {
		old_w = group.canvas_offscreen.width;
		old_h = group.canvas_offscreen.height;
	} else {
		old_w = 0;
		old_h = 0;
	}

	let scaler = group.scaler;
	if (scaler<1) scaler = 1;
	let inv_scaler = 1.0 / scaler;

	let new_w = group_bounds.w;
	let new_h = group_bounds.h;

	if ((group.width>=0) && (group.height>=0)) {
		new_w = get_dim(group, true);
		new_h = get_dim(group, false);
	}
	if (new_w>video_width) new_w = video_width;
	if (new_h>video_height) new_h = video_height;

	new_w = Math.ceil(new_w / scaler);
	new_h = Math.ceil(new_h / scaler);
	while (new_w % 2) new_w++;
	while (new_h % 2) new_h++;

	if ((new_w != old_w) || (new_h != old_h)) {
		let pf;
		let old_pf = group.prev_pixfmt || '';
		let alpha=true;
		if (group.offscreen==GROUP_OST_MASK) {
			pf = 'algr';
		} else {
	    let a = sys.color_component(group.back_color, 0);
			if (a == 1.0) {
				pf = canvas_yuv ? 'yuv4' : 'rgb';
				alpha = false;
			}
			else
				pf = canvas_yuv ? 'y4ap' : 'rgba';
		}
		print(GF_LOG_DEBUG, 'Offscreen group ' + group.id + ' creating offscreen canvas ' + new_w + 'x' + new_h + ' pfmt ' + pf + ' scaler ' + scaler);

		group.canvas_offscreen = new evg.Canvas(new_w, new_h, pf);
		if (!group.canvas_offscreen) return;

		//don't delete texure once created, it might be used by some scenes
		if (!group.texture)
			group.texture = new evg.Texture(group.canvas_offscreen);
		else
			group.texture.update(group.canvas_offscreen);

		if (!group.texture) return;
		group.texture.set_pad_color(group.back_color);

		group.texture.filtering = (scaler > 1) ? GF_TEXTURE_FILTER_HIGH_QUALITY : GF_TEXTURE_FILTER_HIGH_SPEED;

		let mx = new evg.Matrix2D();
		group.texture_mx = mx;

		group.texture.repeat_s = false;
		group.texture.repeat_t = false;

		group.path = new evg.Path().rectangle(0, 0, new_w * scaler, new_h * scaler, true);

		group.prev_pixfmt = pf;
		//texture size changed but not pix format, keep opengl shader otherwise reset it
		if (pf != old_pf)
			group.texture._gl_texture = null;
	}

	if (!group.canvas_offscreen) return;

	print(GF_LOG_DEBUG, 'Redrawing offscreen group ' + group.id);
	group.tr_x = group_bounds.x + group_bounds.w/2;
	group.tr_y = group_bounds.y - group_bounds.h/2;

	global_transform.translate(-group.tr_x, -group.tr_y);
	display_list.forEach(scene => {
		scene.mx.add(global_transform, true);
		scene.mx.scale(inv_scaler, inv_scaler);
	});

	canvas = group.canvas_offscreen;
	canvas.clipper = null;
	canvas.clear(group.back_color);

	draw_display_list_2d();

	//restore state
	global_transform.copy(global_transform_copy);
	has_opaque = has_opaque_bck;
	display_list = display_list_bck;

	globalThis.use_gpu = use_gpu_bck;
	globalThis.blit_enabled = use_gpu_bck ? false : evg.BlitEnabled;
	canvas = canvas_bck;
	webgl = webgl_bck;
	round_scene_size = round_scene_bck;
	group_bounds = group_bounds_bck;

	group.texture._gl_modified = true;
}


function check_camera(group)
{
	if (group.position) return group;
	if (group.target) return group;
	if (group.up) return group;
	if (group.viewport) return group;
	if (group.fov != 45) return group;
	if (group.ar && (group.ar * video_height != video_width)) return group;
	if ((group.znear != 0) || (group.zfar != 0)) return group;
	return null;
}

let dummy_mx = new evg.Matrix2D();

function update_group(group)
{
	let invalidate_offscreen = false;
	let draw_regular = true;

	if (group.update_flag) {
		if (group.update_flag & UPDATE_CHILD)
				invalidate_offscreen = true;

		group.update_flag = 0;

		if (!update_scene_matrix(group))
			return;
	} else if (group.use && group.mxjs) {
		if (!update_scene_matrix(group))
			return;
	}

	if ( (group.opacity<1) || (group.scaler>1) || (group.offscreen>GROUP_OST_NONE)) {
		if (invalidate_offscreen) {
			group_draw_offscreen(group);
		}
		if (group.offscreen > GROUP_OST_DUAL) return;

		//we fail at setting up group opacity, use regular draw
		if (group.canvas_offscreen) draw_regular = false;
	}

	//regular traversal
	if (draw_regular) {
		if (global_branch_depth > filter.maxdepth) {
			print(GF_LOG_ERROR, 'Maximum depth ' + filter.maxdepth + ' reached, aborting scene traversal - try increasing using --maxdepth=N');
			return;
		}

		//we need a copy at each level
		let global_transform_copy = global_transform.copy();
		let last_camera = active_camera;
		let current_camera = check_camera(group);
		if (current_camera) active_camera = current_camera;

		if ((group.mx.is3D || current_camera) && !global_transform.is3D) {
			global_transform = new evg.Matrix(global_transform_copy);
		}

		if (!group.mx_untransform)
			global_transform.add(group.mx, true);
		else
			global_transform.copy(group.mx);

		global_branch_depth ++;

		let first_obj = display_list.length;

		let ref_width_bck = reference_width;
		let ref_height_bck = reference_height;
		if (group.reference && (group.width>=0) && (group.height>=0)) {
			let rw = get_dim(group, true);
			let rh = get_dim(group, false);
			reference_width = rw;
			reference_height = rh;
		}

		if (group.use) {
			let target = get_scene(group.use);
			if (!target) target = get_group(group.use);
			if (target) {
				if ((group.use_depth<0) || (group.use_depth>group.current_depth)) {
					group.current_depth++;
					do_traverse_all(target, update_scene);
					group.current_depth--;
				}
			}
		} else {
			group.scenes.forEach(scene => do_traverse_all(scene, update_scene));
		}

		global_transform = global_transform_copy;
		global_branch_depth --;

		if (current_camera) active_camera = last_camera;
		reference_width = ref_width_bck;
		reference_height = ref_height_bck;

		let last_obj = display_list.length;
		if (group.reverse && (last_obj>first_obj)) {
			let trunc_display_list = display_list.splice(first_obj, last_obj-first_obj);
			trunc_display_list.reverse.apply(trunc_display_list);
			display_list.push.apply(display_list, trunc_display_list);
		}

		return;
	}

	if (group.skip_draw) return;

  //update matrix, opacity and push group to display list
	if (!update_scene_matrix(group))
		return;

	dummy_mx.identity = true;
	dummy_mx.translate(group.tr_x, group.tr_y);
	group.mx.add(dummy_mx, true);

	if (!group.mx_untransform)
		group.mx.add(global_transform);

	let draw_ctx = {};
	draw_ctx.mx = group.mx.copy();
	draw_ctx.screen_rect = null; //TODO
	draw_ctx.scene = null;
	draw_ctx.group = group;
	draw_ctx.opaque_pid = null;
	draw_ctx.is_opaque = false;
	draw_ctx.zorder = group.zorder;
	draw_ctx.camera = check_camera(group);

	display_list.push(draw_ctx);

}

function update_scene(scene)
{
	//this is a group
	if (Array.isArray(scene.scenes)) {
		if (scene.active)
			update_group(scene);
		//abort traversing, it is done in update_group
		return true;
	}

	if (!scene.active) {
		return;
	}

	//apply styles if any
	scene.styles.forEach(sid => {
		let style = get_element_by_id(sid, styles);
		if (!style) return;
		if (!style.updated && !style.forced) return;
		for (var x in style ) {
			if (typeof scene.mod[x] != undefined) {
				scene.set(x, style[x]);
			}
		}
	});

	//recreate our set of inputs for the scene if any
	if (scene.resetup_pids) {
		scene_resetup_pids(scene);
		scene.resetup_pids = false;
		scene.mod.update_flag |= UPDATE_PID;
	}
	//not ready
	if (!scene_update_visual_pids(scene)) return;

	scene.no_signal_time = 0;
	//sequences defined, check for no sequence active if autoshow / nosig are set 
	if (scene.sequences.length && scene.check_active) {
		let disabled=true;
		let min_nosig = 0;
		scene.sequences.forEach (seq => {
			if (seq.active_state != 1) {
				if ( (seq.start_time > current_utc_clock) && (!min_nosig || (min_nosig>seq.start_time)) )
					min_nosig = seq.start_time;
				return;
			}
			seq.sources.forEach(s => {
				if (!s.in_prefetch) disabled = false;
			});
		});
		if (disabled) {
			//sequence not yet activated but we monitor no signal message
			if ((scene.nosig == 'before') && min_nosig) {}
			//sequence not activate, don't draw if autoshow
			else if (scene.nosig != 'all') {
				if (scene.autoshow) return;
			}
			scene.no_signal_time = min_nosig ? min_nosig : -1;
		}
	}
	//compute scene size in pixels
	let sw = get_dim(scene, true);
	let sh = get_dim(scene, false);
	if ((scene.mod.width != sw) || (scene.mod.height != sh)) {
		scene.mod.width = sw;
		scene.mod.height = sh;
		scene.mod.update_flag |= UPDATE_SIZE;
	}

	if (scene.mod.update_flag)
		scene.gl_uniforms_reload = true;
	else if (scene.gl_uniforms_reload) 
		scene.gl_uniforms_reload = false;


	if (scene.mod.update_flag & UPDATE_FX) {
		scene.transition = null;
		scene.gl_program = null;
	}

	//update the matrix
	if (!update_scene_matrix(scene))
		return;

	if (!scene.mx_untransform) {
		if (!scene.mx.is3D && global_transform.is3D)
				scene.mx = new evg.Matrix(scene.mx);

		scene.mx.add(global_transform);
	}


	//update the scene
	scene.mod.force_draw = false;
	scene_in_update = scene;
	let res = scene.mod.update();
	scene_in_update = null;
	if (!res) return;
	if (res==2) has_clip_mask = true;


	let cover_type = 0;
	let use_rotation = false;
	if (scene.mx.is3D) {
		use_rotation = true;
	} else {
		if (!scene.mx.xx || !scene.mx.yy) return;
		if ((scene.mx.xx<0) || (scene.mx.yy<0) || (scene.mx.xy) || (scene.mx.yx)) use_rotation = true;
	}

	scene.mod.screen_rect = null;

	let rc = {x: - scene.mod.width/2, y : scene.mod.height/2, w: scene.mod.width, h: scene.mod.height};
	rc = scene.mx.apply(rc);

	if (group_bounds) {
			group_bounds = sys.rect_union(group_bounds, rc);
	}

	//if final matrix has rotation
	if (use_rotation) {
			cover_type = 0;
	} else {
		let can_blit = false;

		if ((rc.w == video_width)
				&& (rc.h == video_height)
				&& (rc.x == -video_width/2)
				&& (rc.y == video_height/2)
		) {
			can_blit = true;
			cover_type = 1;
		}
		else if ((rc.w > video_width)
				&& (rc.h > video_height)
				&& (rc.x <= -video_width/2)
				&& (rc.y >= video_height/2)
				&& (rc.x+rc.w >= video_width/2)
				&& (rc.y - rc.h <= -video_height/2)
		) {
			cover_type = 2;
		}

		if ((rc.x >= -video_width/2)
			&& (rc.y <= video_height/2)
			&& (rc.x+rc.w <= video_width/2)
			&& (rc.y - rc.h >= -video_height/2)
		) {
			can_blit = true;
		}

		if (can_blit) {
			rc.x += video_width/2;
			rc.y = video_height/2 - rc.y;

			rc.x = Math.floor(rc.x);
			rc.y = Math.floor(rc.y);
			rc.w = Math.floor(rc.w);
			rc.h = Math.floor(rc.h);
			scene.mod.screen_rect = rc;
		}
	}

	let opaque_pid = null;
	let is_opaque = false;

	if (!has_clip_mask && cover_type) {
		let opaque_pid_idx = scene.mod.fullscreen();
		let skip_opaque_test = false;
		//no pids associated (inactive sequence), do not reuse input pck
		if (scene.sequences.length && !scene.mod.pids.length) {
			skip_opaque_test = true;
		}
		//scene is using mix effect between sequence, do not reuse input pck
		else if ((scene.sequences.length>1) && scene.transition) {
			skip_opaque_test = true;
		} else if (scene.mod.pids.length) {
			let s = scene.mod.pids[0].pid.source;
			//no signal on input, do not reuse input pck (which may be non-null if `hold` was set)
			if (s.no_signal)
				skip_opaque_test = true;
			//sequence us in active transision state, do not reuse input pck
			else if ((s.sequence.transition_state>1) && (s.sequence.transition_state<4))
				skip_opaque_test = true;
		}

		//no reuse, force scene draw
		if (skip_opaque_test) {
			scene.mod.force_draw = true;
		} else {
			//get opaque pid
			if ((opaque_pid_idx>=0) && (opaque_pid_idx<scene.mod.pids.length)) {
				opaque_pid = scene.mod.pids[opaque_pid_idx].pid;
			}
			if (!opaque_pid) {
				is_opaque = scene.mod.is_opaque() || false;
			} else {
				//disable reuse if transition is active on pid
				if ((opaque_pid.source.sequence.transition_state>1) && (opaque_pid.source.sequence.transition_state<4)) {
					opaque_pid = null;
					is_opaque = false;
					scene.mod.force_draw = true;
				}
			}
		}
	}

	if (opaque_pid) {
		has_opaque = true;
		if (pids.indexOf(opaque_pid) < 0) {
			print(GF_LOG_WARNING, 'Broken scene ' + scene.id + ' returned fullscreen pid not in PID list');
			scene.active = false;
			return;
		}
	} else if (!use_rotation && is_opaque) {
		has_opaque = true;
	} else {
		is_opaque = false;
	}

	let draw_ctx = {};
	draw_ctx.mx = scene.mx.copy();
	draw_ctx.screen_rect = scene.mod.screen_rect;
	draw_ctx.scene = scene;
	draw_ctx.group = null;
	draw_ctx.opaque_pid = opaque_pid;
	draw_ctx.is_opaque = is_opaque;
	draw_ctx.zorder = scene.zorder;
	draw_ctx.camera = check_camera(scene);
	if (!draw_ctx.camera) draw_ctx.camera = active_camera;

	display_list.push(draw_ctx);
}

function set_canvas_yuv(pfmt)
{
	globalThis.canvas_yuv = sys.pixfmt_yuv(pfmt);
	round_scene_size = 0;
	if (canvas_yuv) {
		if (pfmt == 'yuv4') {}
		else if (pfmt =='yp4l') {}
		else if (pfmt =='yp4a') {}
		else if (pfmt =='yv4p') {}
		else if (pfmt =='y4ap') {}
		else if (pfmt =='y4lp') {}
		else if ((pfmt == 'yuv2')
			|| (pfmt == 'yp2l')
			|| (pfmt == 'uyvy')
			|| (pfmt == 'vyuy')
			|| (pfmt == 'yuyv')
			|| (pfmt == 'yvyu')
			|| (pfmt == 'uyvl')
			|| (pfmt == 'vyul')
			|| (pfmt == 'yuyl')
			|| (pfmt == 'yvyl')
		) {
			round_scene_size = 1;
		}
		else {
			round_scene_size = 2;
		}
	}

}

let prev_display_list = [];
let display_list = [];
let has_clip_mask = false;
let has_opaque = false;

function set_active_scene(ctx)
{
	active_camera = null;
	if (ctx.group) {
		ctx.group.mx.copy(ctx.mx);
		active_camera = ctx.camera;
		active_scene = null;
		return true;
	}
	if (ctx.scene) {
		ctx.scene.mx.copy(ctx.mx);
		ctx.scene.mod.screen_rect = ctx.screen_rect;
		active_camera = ctx.camera;
		active_scene = ctx.scene;
		return true;
	}
	return false;
}

function round_rect(rc_in)
{
	let rc = rc_in;
	if (rc.x % 2) { rc.x--; rc.w++; }
	if (rc.w % 2) rc.w--;
	//only round y for 420 & co
	if (round_scene_size==2) {
		if (rc.y % 2) { rc.y--; rc.h++; }
		if (rc.h % 2) rc.h--;
	}
	return rc;
}

let mask_mode = 0;

function draw_display_list_2d()
{
	display_list.forEach(ctx => {
		if (!set_active_scene(ctx)) return;
		//offscreen group
		if (ctx.group) {
			//only used to get the matrix
			active_scene = ctx;
			ctx.group.texture.mx = ctx.group.texture_mx;
			ctx.group.texture.set_alphaf(ctx.group.opacity);
			canvas_draw(ctx.group.path, ctx.group.texture, true);
			return;
		}
		let scene = ctx.scene;

		//if mask is active or if transition is active, disable blit
		if (mask_mode
			|| (scene.mod.pids.length && scene.mod.pids[0].pid && ((scene.mod.pids[0].pid.source.sequence.transition_state==3) || scene.mod.pids[0].pid.source.no_signal))
		) {
			scene.mod.screen_rect = null;
		} else if (round_scene_size && scene.sequences.length && scene.mod.screen_rect) {
			scene.mod.screen_rect = round_rect(scene.mod.screen_rect);
		}

		if (scene.no_signal_time) {
			draw_scene_no_signal();
		}
		else if (1) {
				scene.mod.draw(canvas);
		} else {
			try {
				scene.mod.draw(canvas);
			} catch (e) {
				print(GF_LOG_ERROR, 'Error during scene ' + scene.id + ' draw, disabling scene - error was ');
				scene.active = false;
			}
		}

		canvas.matrix = null;
		canvas.path = null;
		//do not reset clipper
	});
}


function update_display_list()
{
  display_list.length = 0;
  has_clip_mask = false;
	has_opaque = false;
	reference_width = video_width;
	reference_height = video_height;
	active_camera = null;
	global_transform.identity = true;
	root_scene.scenes.forEach(elm => do_traverse_all(elm, update_scene) );
}

function process_video()
{
	if (!video_inputs_ready) {
		if (!video_time && !live_forced_play) {
			print(GF_LOG_DEBUG, 'video not init');
			do_audio = false;
			return;
		}
		if (!filter.live) {
			print(GF_LOG_DEBUG, 'waiting for video frame');
			return;
		}
	}

	if (filter.live) {
		let now = sys.clock_us();
		let vtime = init_clock_us + 1000000 * (video_time+1) * video_time_inc / video_timescale;

		if (vtime < now) {
			if (!video_inputs_ready)
				print(pids.length ? GF_LOG_WARNING : GF_LOG_DEBUG, 'video input frame(s) not ready but output frame due since ' + (now - vtime) + ' us, forcing generation');
		} else if (vtime - now > video_frame_dur_us) {
			let next = vtime - now;
			if (next > video_max_resched_dur_us) next = video_max_resched_dur_us;
			//first reset any timeout set previously (due do source not being ready)
			filter.reschedule(0);
			//then set our real timeout
			filter.reschedule(next);
			print(GF_LOG_DEBUG, 'video output frame due in ' + (vtime - now) + ' us, waiting ' + next);
			return;
		}
	}

	update_display_list();

  //sort display list - BROKEN if multiple scene instances
  display_list.sort( (e1, e2) => { return e1.zorder - e2.zorder ; } );

	let pid_background = null;
	let pid_background_forward = false;

	let frame = null;
	let pfmt = video_pfmt;
	let restore_pfmt = false;
  //reverse browse
	if (has_opaque) {
		for (let i=display_list.length; i>0; i--) {
			let ctx = display_list[i-1];

			if (ctx.opaque_pid || ctx.is_opaque) {
				display_list = display_list.splice(i-1);
			}

			if (ctx.opaque_pid) {
				pid_background = ctx.opaque_pid;
				pid_background_forward = (ctx.scene && ctx.scene.mod.identity()) || false;

				if (!pid_background.pck) {
					pid_background_forward = false;
					pid_background = null;
					has_opaque = false;
					if (ctx.scene) ctx.scene.mod.force_draw = true;
				}
				break;
			}
			if (ctx.is_opaque) {
				break;
			}
		}
	}


	if (!filter.gpu && has_opaque) {
		if (display_list.length>1)
			pid_background_forward = false;

		//init canvas 2D from this packet
		if (pid_background && (pid_background.width==video_width) && (pid_background.height==video_height)) {
			//mismatch in pixel format cannot reuse packet
			if (!filter.dynpfmt && (pid_background.pfmt != video_pfmt)) {
				pid_background_forward = false;
				display_list[0].scene.mod.force_draw = true;
			} else {
		  	if (last_forward_pixfmt != pid_background.pfmt) {
					last_forward_pixfmt = pid_background.pfmt;
					canvas_reconfig = true;
					if (last_forward_pixfmt != video_pfmt) {
						print(GF_LOG_DEBUG, 'reconfiguring output video pixel format to ' + last_forward_pixfmt);
						vout.set_prop('PixelFormat', last_forward_pixfmt);
						vout.set_prop('Stride', pid_background.get_prop('Stride') );
						vout.set_prop('StrideUV', pid_background.get_prop('StrideUV') );
					}
				}
				pfmt = last_forward_pixfmt;
				if (pid_background_forward) {
					//direct ref
					frame = vout.new_packet(pid_background.pck);
					print(GF_LOG_DEBUG, 'forwarding packet from source ' + pid_background.source.logname);
				} else {
					//copy packet (we may need to reuse source packet at next frame)
					frame = vout.new_packet(pid_background.pck, false, true);
					print(GF_LOG_DEBUG, 'copying packet from source ' + pid_background.source.logname);
				}
			}
			if (filter.dynpfmt==1) {
				if (last_forward_pixfmt) {
					video_pfmt = last_forward_pixfmt;
				}
				print(GF_LOG_INFO, 'Video output format will be ' + video_pfmt);
				filter.dynpfmt = 0;
			}
		} else {
			if (last_forward_pixfmt && (last_forward_pixfmt != video_pfmt))
					restore_pfmt = true;
		}

	} else if (last_forward_pixfmt && (last_forward_pixfmt != video_pfmt)) {
		restore_pfmt = true;
	}

	if (restore_pfmt) {
		vout.set_prop('PixelFormat', video_pfmt);
		vout.set_prop('Stride', null);
		vout.set_prop('StrideUV', null);
		last_forward_pixfmt = 0;
		canvas_reconfig = true;
	}			

	clip_stack.length = 0;
	global_branch_depth = 0;
  if (filter.gpu) {

		webgl.activate(true);


	  webgl.viewport(0, 0, video_width, video_height);
	  webgl.clearDepth(1.0);
  	webgl.enable(webgl.DEPTH_TEST);
  	webgl.enable(webgl.BLEND);
  	webgl.blendFunc(webgl.SRC_ALPHA, webgl.ONE_MINUS_SRC_ALPHA);
  	webgl.depthFunc(webgl.LEQUAL);
  	webgl.clear(webgl.DEPTH_BUFFER_BIT);

		if (!has_opaque) {
			let a = sys.color_component(back_color, 0);
			let r = sys.color_component(back_color, 1);
			let g = sys.color_component(back_color, 2);
			let b = sys.color_component(back_color, 3);
	  	webgl.clearColor(r, g, b, a);
		  webgl.clear(webgl.COLOR_BUFFER_BIT);
		}

	  wgl_clipper = null;

	  if (!display_list.length) {
			active_scene = null;
			draw_scene_no_signal(null);
			print('DL empty');
		} else {
			display_list.forEach(ctx => {
				if (!set_active_scene(ctx)) return;

				//offscreen group
				if (ctx.group) {
					//only used to get the matrix
					active_scene = ctx;
					ctx.group.texture.mx = ctx.group.texture_mx;
					ctx.group.texture.set_alphaf(ctx.group.opacity);
					canvas_draw(ctx.group.path, ctx.group.texture, true);
					return;
				}

				if (ctx.scene.gl_type == SCENE_GL_NONE) {
					ctx.scene.mod.draw();
				} else {
					ctx.scene.mod.draw_gl(webgl);
				}
			});
		}


		canvas_offscreen_deactivate();

	  webgl.disable(webgl.SCISSOR_TEST);

		webgl.flush();
		webgl.activate(false);

		frame = vout.new_packet(webgl, () => { filter.frame_pending=false; } );
		filter.frame_pending = true;
  } else {
		if (!frame) {
			frame = vout.new_packet(vout_size);
			print(GF_LOG_DEBUG, 'allocating packet for framebuffer');
		}

		if (!pid_background_forward) {
			if (!canvas) {
				try {
					canvas = new evg.Canvas(video_width, video_height, pfmt, frame.data);
				} catch (e) {
					print(GF_LOG_ERROR, 'Failed to create canvas: ' + e);
					do_terminate();
					return;
				}

				if (filter.thread) {
					canvas.enable_threading(filter.thread);
				}
				if (use_canvas_3d)
					canvas.enable_3d();

				if (typeof canvas.blit != 'function')
					globalThis.blit_enabled = false;
				else if (!globalThis.use_gpu)
					globalThis.blit_enabled = evg.BlitEnabled;

				canvas.pix_fmt = pfmt;
				set_canvas_yuv(pfmt);
			} else if (canvas_reconfig) {
				canvas_reconfig = false;	
				try {
					canvas.reassign(video_width, video_height, pfmt, frame.data);
				} catch (e) {
					print(GF_LOG_ERROR, 'Failed to reassign canvas: ' + e);
					do_terminate();
					return;
				}

				canvas.pix_fmt = pfmt;
				set_canvas_yuv(pfmt);
			} else {		
				canvas.reassign(frame.data);
			}

			//reset clipper
			canvas.clipper = null;

			if (!has_opaque) {
				print(GF_LOG_DEBUG, 'canvas clear to ' + back_color);
				canvas.clear(back_color);
			}

		  if (!display_list.length) {
				active_scene = null;
				draw_scene_no_signal(null);
			print('DL empty2');
			} else {
				draw_display_list_2d();
			}
		}
  }

	frame.cts = frame.dts = video_time;
	frame.dur = video_time_inc;
	frame.sap = GF_FILTER_SAP_1;
	frame.send();

	if (filter.live) {
		let now = sys.clock_us();
		if (!video_time) {
			init_utc = sys.get_utc();
			init_clock_us = now;
		}
		let vtime  = init_clock_us + 1000000*video_time * filter.fps.d / filter.fps.n;
		if (vtime + video_frame_dur_us < now) {
			print(GF_LOG_WARNING, 'sent video frame TS ' + (video_time) + '/' + video_timescale + ' too late by ' + Math.ceil(now - vtime - video_frame_dur_us) + ' us');
		} else if (vtime < now) {
			print(GF_LOG_DEBUG, 'sent video frame TS ' + (video_time) + '/' + video_timescale + ' (' + Math.ceil(now-vtime) + ' us early)');
		} else {
			print(GF_LOG_DEBUG, 'sent video frame TS ' + (video_time) + '/' + video_timescale);
		}
		next_video_gen_time = vtime + video_frame_dur_us;
	} else {
			print(GF_LOG_DEBUG, 'sent video frame TS ' + (video_time) + '/' + video_timescale);		
	}

	video_time += video_time_inc;

	let old_prev = prev_display_list;
	prev_display_list = display_list;
	display_list = old_prev;
	display_list.length = 0;

	styles.forEach(style => {
		style.updated = false;
	});
}

function process_audio()
{
	let nb_samples = 0;
	let i,j,k, nb_active_audio=0;
	let mix_pids = [];

	//if video is playing and not blocking, process audio event if blocking - this avoids cases where output frames are consumed by burst
	//and the video burst happens befor the audio burst: we need to send video to fill the burst then unblock audio, but video won't be procesed
	//if audio is blocking... 
	if (aout.would_block && (!video_playing || vout.would_block)) {
		return;
	}

	if (!audio_inputs_ready) {
		if (!audio_time && !live_forced_play)
			return;
	}
	if (!video_playing && filter.live) {
		let now = sys.clock_us();
		let atime = init_clock_us + 1000000 * (audio_time) / audio_timescale;

		if (atime - now > audio_frame_dur_us) {
			print(GF_LOG_DEBUG, 'audio output frame due in ' + (atime - now) + ' us, waiting');
			let next = atime - now;
			if (next > audio_max_resched_dur_us) next = audio_max_resched_dur_us;
			filter.reschedule(next);
			return;
		}

	}

	let empty_ok = false;
	pids.forEach(pid => {
		if (pid.type != TYPE_AUDIO) return;
		if (!pid.source.playing) return;
		if (pid.source.no_signal_state) {
			empty_ok = true;
			return;
		}
		if (pid.source.disabled) return;
		if (pid.eos) return;
		if (pid.done) return;
		nb_active_audio++;
		if (!pid.pck) return;
		let samp_avail = pid.nb_samples - pid.samples_used;
		if (!nb_samples || (samp_avail<nb_samples)) nb_samples = samp_avail;
		mix_pids.push(pid);
		if (!pid.data) {
			pid.data = pid.pck.data;
		}
		//default is mute
		pid.volume = 0;
		pid.detached = true;
	});

	if (nb_samples > filter.alen)
		nb_samples = filter.alen;


	let aframe = null;
	let empty=false;

	if (!nb_samples) {
		//todo - check if we need to move forward after some time (and drop input), to allow holes in timeline
		if (nb_active_audio) {
			if (!filter.live) {
				print(GF_LOG_DEBUG, 'waiting for audio frame');
				return;
			}
			if (!empty_ok) {
				let atime = init_clock_us + (audio_time + 2*filter.alen)*1000000/filter.sr;
				let now = sys.clock_us();
				if (atime >= now) {
					print(GF_LOG_DEBUG, 'waiting for audio frame');
					return;
				}
				if (!empty_ok)
					print(GF_LOG_WARNING, 'empty audio input but audio frame due since ' + Math.ceil(now-atime) + ' us');
			}
		}

		//send an empty packet of 100 samples
		aframe = aout.new_packet(channels * filter.alen);
		let output = new Uint16Array(aframe.data);
		output.fill(0);
		empty=true;
		nb_samples = filter.alen;
	} else {

		//apply volume per scene
		do_traverse(root_scene, scene => {
			scene.apids.forEach(pid => {
				pid.detached = false;
				if (pid.source.mix)
					pid.volume = scene.mod.volume * pid.source.mix_volume;
				else
					pid.volume = scene.mod.volume;

				//scene inactive
				if (!scene.active) {
					//pid was active, either mute or fade to 0
					if (pid.active) {
						if ((scene.mod.fade=='out') || (scene.mod.fade=='inout')) {
							pid.fade = 2; //fade to 0
						} else {
							pid.volume = 0;
						}
						pid.active = false;
					} else {
						pid.volume = 0;
					}
				}
				//scene active, pid was inactive: use full volume and fade from to 0
				else if (!pid.active) {
					if ((scene.mod.fade=='in') || (scene.mod.fade=='inout')) {
						pid.fade = 1; //fade from 0
					}
					pid.active = true;
					//remember audio fade type
					pid.audio_fade = scene.mod.fade;
				}
			});
		});

		//also do a fadeout for pids no longer active
		mix_pids.forEach(pid => {
			if (!pid.detached || !pid.active) return;
			if ((pid.audio_fade=='out') || (pid.audio_fade=='inout')) {
				pid.fade = 2; //fade to 0
			}
			pid.active = false;
		});

		aframe = aout.new_packet(nb_samples * channels * sample_size);

		audio_mix.mix(audio_time, aframe.data, mix_pids);

		mix_pids.forEach(pid => {
			if (pid.nb_samples <= pid.samples_used) {
				pid.nb_samples = 0;
				pid.samples_used = 0;
				pid.pck.unref();
				pid.pck = null;
				pid.data = null;
				pid.fade = 0;
			}
		});
	}

	aframe.cts = audio_time;
	aframe.dur = nb_samples;
	aframe.sap = GF_FILTER_SAP_1;
	aframe.send();

	let msg = 'sent ' + (empty ? 'empty ' : '') + 'audio frame TS ' + (audio_time) + '/' + audio_timescale;

	if (filter.live) {
		let now = sys.clock_us();
		if (!audio_time && !video_playing) {
			init_utc = sys.get_utc();
			init_clock_us = now;
		}

		let atime  = init_clock_us + 1000000*audio_time/audio_timescale;
		if (atime + nb_samples < now) {
			print(GF_LOG_WARNING, msg + ' too late by ' + Math.ceil(now - atime - nb_samples) + ' us');
		} else if (atime < now) {
			print(GF_LOG_DEBUG, msg + ' (' + (now-atime) + ' us early)');
		} else {
			print(GF_LOG_DEBUG, msg);
		}
		next_audio_gen_time = atime + audio_frame_dur_us;
	} else {
			print((empty && nb_active_audio) ? GF_LOG_ERROR : GF_LOG_DEBUG, msg);
	}

	audio_time += nb_samples;
}

let inactive_sources = 0;

function next_source(s, prefetch_check)
{
	let next_src_idx = -1;
	let src_idx = s.sequence.sources.indexOf(s);
	if (src_idx<0) {
		print(GF_LOG_WARNING, 'source ' + s.logname + ' was removed from parent sequence while active');
		if (s.sequence.sources.length)
			next_src_idx = 0;
	} else if (src_idx +1 < s.sequence.sources.length) {
		next_src_idx = src_idx+1;
	} else if (s.sequence.loop != 0) {
		if (!prefetch_check && (s.sequence.loop > 0))
			s.sequence.loop -= 1;
		next_src_idx = 0;
	}

	let next_src = null;
	if (next_src_idx>=0) {
		for (let i=next_src_idx; i<s.sequence.sources.length; i++) {
			next_src = s.sequence.sources[i];
			if (!next_src.disabled) break;
			next_src = null;
		}
		if (!next_src && next_src_idx) {
			for (let i=0; i<s.sequence.sources.length; i++) {
				next_src = s.sequence.sources[i];
				if (!next_src.disabled) break;
				next_src = null;
			}
		}
		if (!next_src) {
			print(GF_LOG_INFO, 'All sources in sequence disabled !');
			s.sequence.active_state = 2;
			if (s.sequence.id)
				trigger_watcher(s.sequence, 'active', false);
		}
	}
	return next_src;
}

function sequence_over(s, force_seq_reload)
{
	let is_transition_start = false;
	if (s.sequence.transition_state == 2) {
		is_transition_start = true;
		s.sequence.transition_state = 3;
	}
	//error loading transition, reset state
	else if (s.sequence.transition_state == 4) {
		s.sequence.transition_state = 0;
		s.video_time_at_init = 0;
		s.sequence.sources.forEach(src => { src.next_in_transition = false;});
	}
	//end of transition, reset state and return (next is already loaded)
	else if (s.sequence.transition_state == 3) {
		s.sequence.transition_state = 0;
		print(GF_LOG_INFO, 'End of transition for sequence ' + s.sequence.id);
		do_traverse(root_scene, scene => {
			if (scene.sequences.indexOf(s.sequence) >= 0) {
				scene.resetup_pids = true;
			}
		});
		s.sequence.sources.forEach(src => { src.next_in_transition = false;});
		print(GF_LOG_INFO, 'stopping source ' + s.logname);
  	s.video_time_at_init = 0;
		stop_source(s, true);
		return;
	}

	s.video_time_at_init = 0;
	let force_load_source = false;

	while (1) {

	//no packets for the desired time range
	if (!s.timeline_init && s.sequence.start_offset) {
		let dur = s.duration;
		if (s.media_stop > s.media_start)
			 dur = s.media_stop - s.media_start;
		if (dur < s.sequence.start_offset) {
			s.sequence.start_offset -= dur;
		} else {
			print(GF_LOG_DEBUG, 'reset sequence start offset ' + s.sequence.start_offset + ' to 0 - ' + s.duration);
			s.sequence.start_offset = 0;
		}
	}

	let next_src = next_source(s, false);
	let is_same_source = (!force_load_source && (next_src === s)) ? true : false;
	if (force_seq_reload)
		is_same_source = false;

	if (!is_transition_start) {
		print(GF_LOG_DEBUG, 'source ' + s.logname + ' is over, sequence start offset ' + s.sequence.start_offset);
		if (!is_same_source)
			stop_source(s, true);

		//transition has been started so next source is loaded, only reset scene pids
		if (s.sequence.transition_state==3) {
			do_traverse(root_scene, scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
						scene.resetup_pids = true;
						print(GF_LOG_DEBUG, 'end of transition, scene PID resetup');
				}
			});
			s.sequence.transition_state = 0;
			return;
		}
	}

	if (next_src) {
		if (next_src.in_prefetch) {
			print(GF_LOG_INFO, 'End of prefetch for ' + next_src.logname);
			next_src.in_prefetch = 0;
			next_src.pids.forEach(pid => {
				if (pid.type==TYPE_VIDEO) {
					let pck = pid.get_packet();
					print(GF_LOG_DEBUG,'got packet during prefetch: ' + ((pck==null) ? 'no' : 'yes'));
				}
			});

			do_traverse(root_scene, scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
						scene.resetup_pids = true;
						print(GF_LOG_DEBUG, 'end of prefetch, scene PID resetup');
				}
			});

		} else {
			print(GF_LOG_DEBUG, 'load next in sequence ' + next_src.logname);
			let dur = next_src.duration;
			if (next_src.media_stop > next_src.media_start)
					dur = next_src.media_stop - next_src.media_start;
			if (dur && (dur < s.sequence.start_offset)) {
				print(GF_LOG_DEBUG, 'source ' + next_src.logname + ' will end before sequence start offset ' + s.sequence.start_offset + ' - skipping');
				s = next_src;
				force_load_source = true;
				continue;	
			}

			if (!is_same_source) {
				open_source(next_src);
			} else {
				stop_source(next_src, false);
			}
			play_source(next_src);
		}
	} else if (! is_transition_start) {
		inactive_sources++;
		s.sequence.active_state = 2;
		print(GF_LOG_DEBUG, 'sequence over');
	}
	return;

	}
}

function fetch_source(s)
{
	let wait_audio=0, wait_video=0;
	if (s.disabled) {
		inactive_sources++;
		return;
	}
	if (s.removed) {
		inactive_sources++;
		return;
	}

	//check if source must be started
	if (!s.playing) {
		if (!s.sequence.active_state && (s.sequence.start_time>0) && (s.sequence.start_time <= current_utc_clock + s.prefetch_ms)) {
			if (s.sequence.skip_start_offset) {
				s.sequence.start_offset = 0;
				print(GF_LOG_INFO, 'Starting sequence ' + s.sequence.id + ' at UTC ' + current_utc_clock);
			} else {
				s.sequence.start_offset = (current_utc_clock + s.prefetch_ms - s.sequence.start_time ) / 1000.0;
				if (s.sequence.start_offset<0) s.sequence.start_offset = 0;
				print(GF_LOG_INFO, 'Starting sequence ' + s.sequence.id + ' at UTC ' + current_utc_clock + ' with start offset ' + s.sequence.start_offset + ' sec');
			}

			s.sequence.active_state = 1;
			if (s.sequence.id)
				trigger_watcher(s.sequence, 'active', true);

			if (s.media_start + s.media_stop < s.sequence.start_offset) {
					print(GF_LOG_INFO, 'source ' + s.logname + ' will end before sequence start_offset, skipping source');
					s.sequence.start_offset -= s.media_start + s.media_stop;
					inactive_sources++;
					sequence_over(s, false);
					return;
			} else {
					print(GF_LOG_INFO, 'source ' + s.logname + ' open');
				open_source(s);
				play_source(s);
				if (current_utc_clock < s.sequence.start_time) {
						s.in_prefetch = 2;
						inactive_sources++;
						return;
				}
			}
		} else {
			inactive_sources++;
			return;
		}
	}
	if (s.in_prefetch) {
		if ((s.in_prefetch==2) && (s.sequence.start_time>0) && (s.sequence.start_time <= current_utc_clock)) {
			s.in_prefetch = 0;
			print(GF_LOG_DEBUG, 'end of prefetch for ' + s.logname + ' , scene PID resetup');
			do_traverse(root_scene, scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
						scene.resetup_pids = true;
				}
			});
		} else {
			inactive_sources++;
			return;		
		}
	}

	//done with sequence
	if ((s.sequence.active_state==1) && (s.sequence.stop_time>0) && (s.sequence.stop_time <= current_utc_clock)) {
	  	s.video_time_at_init = 0;
			stop_source(s, true);
			s.sequence.transition_state = 0;
			do_traverse(root_scene, scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
					scene.resetup_pids = true;
				}
			});
			s.sequence.sources.forEach(src => { src.next_in_transition = false;});
			inactive_sources++;
			print(GF_LOG_INFO, 'End of sequence ' + s.sequence.id + ' at ' + (current_utc_clock-s.sequence.stop_time));
			s.sequence.active_state = 2;
			if (s.sequence.id)
				trigger_watcher(s.sequence, 'active', false);
			return;
	}

	let min_cts = 0;
	let min_timescale = 0;
	let nb_over = 0;
	let nb_active = 0;
	let force_seq_over = false;

  let source_restart = false;
  s.no_signal = 0;

	s.pids.forEach(pid => {
		if (pid.done) {
				nb_over++;
				return;
		}
		while (1) {

		let pck = pid.get_packet();
		if (!pck) {
			let force_wait_pid = !filter.live;
			if (pid.eos) {
				pid.done = true;
				nb_over++;
				return;
			}
			
			let tdiff = current_utc_clock - pid.last_pck_time;
			if (tdiff > filter.lwait) {
				force_wait_pid = false;
				if (s.no_signal < tdiff) s.no_signal = tdiff;
				//for video pid, drop ref !
				if (pid.pck) {
						pid.pck.unref();
						pid.pck = null;
				}
				if (current_utc_clock - pid.last_pck_time > filter.ltimeout) {
					s.timeline_init = false;
					if (s.has_ka_process) {
						pid.source.src.forEach(src => {
							if (!src.process_id) return;
							let res = os.waitpid(src.process_id, os.WNOHANG);
							let msg = (res[0] == src.process_id) ? ('exited with code ' + res[1]) : 'not responding, restarting';
							print(GF_LOG_ERROR, 'Process for source ' + pid.source.logname + ' (' + src.in + ') ' + msg);
							source_restart = true;
						});
					}
				}
				return;
			} 

			if (force_wait_pid || !pid.pck) {
				if (pid.type==TYPE_VIDEO)
					wait_video = true;
				else
					wait_audio = true;
			} 
			return;
		}
		if (pid.type==TYPE_UNHANDLED) {
			pid.drop_packet();
			return;
		}

		//we got a packet, if first one init clock
		if (!s.timeline_init) {
			if (!min_timescale || (min_cts * pid.timescale > pck.cts * min_timescale)) {
				min_cts = pck.cts;
				min_timescale = pid.timescale;
			}
			return;
		}

		//source is active (has packets)
		nb_active++;

		let check_transition = false;
		if (video_playing) {
			if (pid.type == TYPE_VIDEO) check_transition = true;
		} else {
			if (pid.type != TYPE_VIDEO) check_transition = true;
		}

		if (!s.sequence.prefetch_check && check_transition) {
			let check_end = s.duration;
			if ((s.media_stop>s.media_start) && (s.media_stop<s.duration)) check_end = s.media_stop;

			if (s.duration && s.hold && (s.media_stop > s.duration)) {
				s.sequence.transition_state = 4;
				s.transition_offset = 0;
				s.sequence.transition_dur = 0;
			}

			if ((s.sequence.transition_state<=1) && s.sequence.transition_effect && next_source(s, true) ) {
				let dur = s.sequence.transition_effect.dur || 0;
				if (dur) {
					s.sequence.transition_state = 1;
					s.sequence.transition_dur = dur;

					if (check_end > dur) check_end -= dur;
					else {
						check_end = 0;
					}
				}
			}

			if (check_end>0) {
				if ((pck.cts - pid.init_ts) > (s.transition_offset + check_end - s.media_start - s.prefetch_sec) * pid.timescale) {
					let next_src = next_source(s, true);
					s.sequence.start_offset = 0;
					if (next_src && (next_src != s)) {
						print(GF_LOG_INFO, 'time to prefetch next source ' + next_src.logname + ' in sequence ' + s.sequence.id +  ' - end time: ' + (check_end - s.media_start), pck.cts, pid.init_ts, pid.timescale, s.transition_offset);
						open_source(next_src);
						play_source(next_src);
						next_src.in_prefetch = 1;
						if ((!s.sequence.transition_state<=1) && s.sequence.transition_effect) {
							next_src.transition_offset = s.sequence.transition_dur;
							next_src.next_in_transition = true;
						}
					}
					s.sequence.prefetch_check = true;
				}
			}
		}
		//except for video, check this packet is not out of play range
		if ((s.media_stop>s.media_start) && (pid.type != TYPE_VIDEO)) {
			if ((pck.cts - pid.init_ts) > (s.transition_offset + s.media_stop - s.media_start) * pid.timescale) {
				pid.drop_packet();
				pid.done = true;
				pid.send_event( new FilterEvent(GF_FEVT_STOP) );
				nb_over++;
				return;
			}
		}

		if (check_transition && (s.sequence.transition_state==1)) {
			let check_end = s.duration;
			if ((s.media_stop>s.media_start) && (s.media_stop<s.duration)) check_end = s.media_stop;

			if (check_end > s.sequence.transition_dur)
				check_end -= s.sequence.transition_dur;
			else {
				check_end = 0;
			}

			if ((pck.cts - pid.init_ts) >= (s.transition_offset + check_end - s.media_start) * pid.timescale) {
				if (load_transition(s.sequence)) {
					s.next_in_transition = false;
					print(GF_LOG_INFO, 'time to activate transition in sequence ' + s.sequence.id + ' end_time: ' + (check_end - s.media_start) + ' cts ' + pck.cts + ' init_cts ' + pid.init_ts);
					s.sequence.transition_state = 2;
					s.sequence.transition_start = video_time;
				} else {
					//error
					s.sequence.transition_state = 4;
				}
			}
		}

		if (!pid.pck) {
			pid.pck = pck.ref();
			pid.drop_packet();
			pid.frame_ts = pid.translate_timestamp(pid.pck.cts);
			if (pid.pck.blocking_ref) {
				blocking_ref_pids.push(pid);
				pid.is_blocking = true;
			}
			pck = null;
		}

		pid.last_pck_time = current_utc_clock;

		if (pid.type==TYPE_AUDIO) {
			if (!audio_playing) {
				//audio not playing, discard packet if time before current video time
				if (pid.frame_ts * video_timescale < video_time * pid.timescale) {
					pid.pck.unref();
					pid.pck=null;
					continue;
				}
			}

			if ( (s.media_stop>s.media_start) && ((pid.pck.cts + pid.pck.dur) >= s.media_stop * pid.timescale)) {
				let diff_samples = Math.floor(s.media_stop * audio_timescale);
				if (audio_timescale == pid.timescale) {
					diff_samples -= Math.floor(pid.pck.cts);
				} else {
					diff_samples -= Math.floor(pid.pck.cts * audio_timescale / pid.timescale);
				}
				if (diff_samples>0) {
					pid.nb_samples = diff_samples;
					if ((pid.source.audio_fade=='out') || (pid.source.audio_fade=='inout')) {
						pid.fade = 2; //fade to 0
					}
					print(GF_LOG_DEBUG, 'End of playback pending for ' + pid.source.logname + ' - samples left ' + diff_samples);
				} else {
					//we are done
					pid.pck.unref();
					pid.pck = null;
					pid.done = true;
					pid.send_event( new FilterEvent(GF_FEVT_STOP) );
					nb_over++;
					print(GF_LOG_DEBUG, 'End of playback reached for ' + pid.source.logname);
				}
			} else {
				pid.nb_samples = pid.pck.size / pid.channels / sample_size;
			}
			return;
		}
		else if (pid.type!=TYPE_VIDEO) {
			return;
		}
		if (!video_playing) {
			if (pid.frame_ts * audio_timescale < audio_time * pid.timescale) {
				pid.pck.unref();
				pid.pck=null;
				continue;
			}
		}

		//check closest frame
		while (1) {
			pck = pid.get_packet();
			if (!pck) break;
			let next_ts = pid.translate_timestamp(pck.cts);
			if (next_ts > video_time) {
				let diff_next = next_ts - video_time;
				let diff_cur = video_time - pid.frame_ts;
				if (diff_next > diff_cur)
					break;
			}

			pid.pck.unref();
			pid.pck = pck.ref();
			pid.drop_packet();
			pid.frame_ts = next_ts;
		}
		//if current frame is out of play range (>=), consider source done
		if (s.media_stop>s.media_start) {
			if ((pid.pck.cts - pid.init_ts) >= (s.transition_offset + s.media_stop - s.media_start) * pid.timescale) {
				pid.drop_packet();
				pid.done = true;
				pid.send_event( new FilterEvent(GF_FEVT_STOP) );
				nb_over++;
				return;
			}
		}

		print(GF_LOG_DEBUG, 'Video from ' + s.logname + ' will draw pck CTS ' + pid.pck.cts + '/' + pid.timescale + ' translated ' + pid.frame_ts + ' video time ' + video_time);
		return;
	}
	});
	//done fetching pids

	if (s.pids.length && (nb_over == s.pids.length)) {
		nb_over = 1;
		let relaunch=false;
		s.src.forEach(src => {
			if (!src.process_id) return;
			let res = os.waitpid(src.process_id, os.WNOHANG);
			//child process is dead, if error >2, relaunch if keep alive
			if (res[0] == src.process_id) {
				if (res[1]>2) {
					print(GF_LOG_WARNING, 'Child process for src ' + s.logname + ' exit with error ' + res[1] + (s.keep_alive ? ", restarting source" : "") );
					relaunch = s.keep_alive ? 1 : 0;
				}
			}
			//process is still alive but we are done 
			else if (!source_restart) {
				print(GF_LOG_INFO, 'Child process for src ' + s.logname + ' still alive but eos notified');
			}
		});

		if (relaunch) {
			source_restart = true;
		} else if (s.duration && s.hold && (s.media_stop > s.duration)) {
			let pid = s.pids[0];
			let target_end, ref_time, ref_timescale;
			if (pid.type == TYPE_VIDEO) {
				ref_timescale = video_timescale;
				ref_time = video_time;
			} else {
				ref_timescale = audio_timescale;
				ref_time = audio_time;
			}
			target_end = pid.init_clock + ref_timescale * (s.media_stop - s.media_start);

			if (target_end > ref_time) {
				nb_over = 0;
				s.no_signal = true;
				if (!s.no_signal_state) {
					print(GF_LOG_INFO, 'source ' + s.logname + ' is over but hold enabled, entering no signal state');
				}
				s.no_signal_state = (target_end - ref_time) / ref_timescale;
				return;
			} else {
				s.no_signal = false;
				s.no_signal_state = false;
				//first source in sequence is done, reset start offset
				s.sequence.start_offset = 0;
				s.sequence.transition_state = 0;
				print(GF_LOG_INFO, 'source ' + s.logname + ' is over, exiting no signal state');
			}
		} else {
			//first source in sequence is done, reset start offset
			s.sequence.start_offset = 0;
			print(GF_LOG_INFO, 'source ' + s.logname + ' is over - transition state: ' + s.sequence.transition_state);
		}
  } else if (!s.pids.length && s.has_ka_process) {
  	nb_over = 0;
	  s.src.forEach(src => {
			if (src.process_id) {
				let res = os.waitpid(src.process_id, os.WNOHANG);
				if (res[0] == src.process_id) {
					print(GF_LOG_ERROR, 'Child process creation failed for source ' + src.in + ' - removing');
					nb_over ++;
				}
			}
	  });
	  if (nb_over == s.src.length) {
	  	nb_over = 1;
	  	force_seq_over = true;
			return;
	  } else {
	  	nb_over = 0;
	  }
	} else {		
		nb_over = 0;
	}

	if (!nb_active && source_restart) {
		nb_over=0;
		s.timeline_init=0;
		stop_source(s, true);
		open_source(s);
		play_source(s);
		video_inputs_ready = false;
	}

	//source is playing but no packets, we need to force reschedule to probe for no signal, we do that every second
	if (!nb_active && !nb_over) {
		filter.reschedule(1000);
	}
	if (!s.timeline_init && !nb_over) {
		if (s.no_signal) {
			return;
		}
		//no pids yet
		if (!min_timescale) {
			video_inputs_ready = audio_inputs_ready = false;
			return;
		}
		if (wait_video || wait_audio) {
			if (wait_video) video_inputs_ready = false;
			if (wait_audio) audio_inputs_ready = false;
			return;
		}

		print(GF_LOG_DEBUG, 'clock is init for source ' + s.logname + ' using TS ' + min_cts + '/' + min_timescale);
		s.pids.forEach(pid => {
			pid.init_ts = min_cts;
			if (min_timescale != pid.timescale) {
				pid.init_ts *= pid.timescale;
				pid.init_ts /= min_timescale;
			}
			if (pid.type==TYPE_VIDEO) {
				pid.init_clock = video_time;
			} else {
				pid.init_clock = audio_time;
				if ((pid.source.audio_fade=='in') || (pid.source.audio_fade=='inout')) {
					pid.fade = 1; //fade from 0
				}
			}
			print(GF_LOG_DEBUG, 'PID ' + s.logname + '.' + pid.name + ' clock init TS ' + pid.init_ts + ' clock value ' + pid.init_clock);
		});
		s.timeline_init = true;

		if (s.seek && !s.video_time_at_init) 
			s.video_time_at_init = video_time+1;

		fetch_source(s);
	}

	if (!force_seq_over && (!s.pids.length || !nb_over)) {
		if (s.sequence.transition_state == 2) {
			print(GF_LOG_DEBUG, 'activating next source for transition in sequence ' + s.sequence.id);
			sequence_over(s, false);
		}
		//active sources and waiting for inputs
		else if (s.pids.length) {
			if (wait_video) video_inputs_ready = false;
			if (wait_audio) audio_inputs_ready = false;			
		}
		return;
	}

	sequence_over(s, force_seq_over);
	//try at least one cycle before generating the frame
	video_inputs_ready = false;

	//we are done
	if (s.removed) {
		print(GF_LOG_INFO, 'schedule for remove source ' + s.logname);
		purge_sources.push(s);
	}
}


function get_source(id, src, par_seq)
{
	for (let i=0; i<sources.length; i++) {
		let elem = sources[i]; 
		if (par_seq && (elem.sequence != par_seq)) continue;

		if (id && (elem.id == id)) return elem;
		if (src && (elem.src.length == src.length)) {
			let diff = false;
			elem.src.forEach( (v, index) => {
				if (!(v.in === src[index].in)) diff = true; // Romain: if sources have the same input name they are discarded!!
				let id1 = v.id || 0;
				let id2 = src[index].id || 0;
				if (id1 && id2 && !(id1 === id2)) diff = true;
			} );
			if (!diff) return elem;
		}
	}
	return null;
}

function get_source_by_pid(pid)
{
	let type = pid.get_prop("StreamType");
	let res = null;
	sources.forEach( elem => { 
		elem.fsrc.forEach( (f, index) => {
			if (!pid.is_filter_in_parents(f)) return;

			if ((f.media_type=="all") 
				|| ((type=='Visual') && (f.media_type.indexOf('v')>=0))
				|| ((type=='Audio') && (f.media_type.indexOf('a')>=0))
				|| ((type=='Text') && (f.media_type.indexOf('t')>=0)) 
			) {
				pid.skipped = false;
				res = elem;
			} else {
				pid.skipped = true;
			}

		} );
	} );
	return res;
}


function play_pid(pid, source) {
	let evt = new FilterEvent(GF_FEVT_PLAY);
	evt.start_range = source.media_start + source.sequence.start_offset;
	if (source.video_time_at_init) {
		evt.start_range += (video_time - (source.video_time_at_init-1)) / video_timescale;
	} 

	pid.done = false;
	print(GF_LOG_DEBUG, 'Playing PID ' + source.logname + '.' + pid.name + ' @start=' + evt.start_range);
	pid.send_event(evt);
}

function play_source(s)
{
	if (s.playing) {
		print(GF_LOG_ERROR, 'Source is already playing ' + s.logname);
		return;
	}
	print(GF_LOG_DEBUG, 'Playing source ' + s.logname);
	if (!s.sequence.active_state)
		trigger_watcher(s.sequence, 'active', true);
	s.sequence.active_state = 1;
	s.playing = true;
	trigger_watcher(s, "active", true);
	s.timeline_init = false;
	s.pids.forEach(pid => { play_pid(pid, s); });
}

function stop_source(s, is_remove)
{
	if (s.playing) {
		s.playing = false;
		trigger_watcher(s, "active", false);
		s.sequence.prefetch_check = false;
		s.timeline_init=0;

		print(GF_LOG_DEBUG, 'Stoping source ' + s.logname);
	  s.src.forEach(src => {
			if (src.process_id) {
				let res = os.waitpid(src.process_id, os.WNOHANG);
				if (res[0] != src.process_id) {
					print(GF_LOG_DEBUG, 'killing process for source ' + src.in);
					os.kill(src.process_id, os.SIGABRT);
				}
				if (src.local_pipe) {
					try {
						sys.del(src.local_pipe);
					} catch (e) {

					}
					src.local_pipe = null;
				}
			}
			src.process_id = null;
	  });

		s.pids.forEach(pid => {
			if (pid.pck) {
					pid.pck.unref();
					pid.pck = null;
			}
			pid.send_event( new FilterEvent(GF_FEVT_STOP) ); 
		});
	}

	if (!is_remove) return;

	print(GF_LOG_DEBUG, 'Removing source ' + s.logname);
	s.fsrc.forEach( f => {
		f.remove();
	});
	s.fsrc.length = 0;
	s.pids.length = 0;
	s.opened = false;
}

function disable_source(s)
{
	s.disabled = true;
	if (!s.sequence.active_state)
		trigger_watcher(s.sequence, 'active', false);

	s.sequence.active_state = 0;
	s.fsrc.forEach( f => {
		f.remove();
	});
	s.fsrc = [];
	s.pids = [];
	print(GF_LOG_DEBUG, 'Disabling source ' + s.logname);
}

let current_pipe = 1;
let current_port = 2000;

function apply_links(links, target, fchain)
{
	let broken_links=false;
	links.forEach(link => {

	let link_arg = "";
	let link_idx = 0;
	let link_start = false;
	if (link.startsWith("@@")) {
		link = link.slice(2);
	} else {
		link = link.slice(1);
	}

	let sep = link.indexOf('#');
	if (sep<0) {
		if (link.length) 
			link_idx = parseInt(link);
	} else {
		let vals = arg.slice(sep);
		link_idx = parseInt(vals[0]);
		link_arg = vals[1];
	}

	if (!link_start) link_idx = fchain.length - link_idx - 1;
	if ((link_idx<0) || (link_idx>=fchain.length)) {
		print(GF_LOG_ERROR, 'Wrong filter index ' + link_idx + ' in link directive ' + link);
		broken_links = true;
		return;
	} 
	let f_src = fchain[link_idx];

	if (target) {
		target.set_source(f_src, link_arg);
	} else {
		filter.set_source_restricted(f_src, link_arg);
	}

	});
	return broken_links;
}

function open_source(s)
{
	if (s.opened) return;
	s.opened = true;
	s.sequence.prefetch_check = false;
//	filter.reset_source();
	wait_for_pids = current_utc_clock;

	print(GF_LOG_DEBUG, 'Open source ' + s.logname);

  s.has_ka_process = false;
	s.src.forEach(src => {

	let f, args, i;
	let port="";
	let opts="";
	let append_filter = null;
	let local_filter = null;
	if (typeof src.port == 'string') port = src.port;
	if (typeof src.opts == 'string') opts = src.opts;
	let use_raw = (typeof src.raw == 'boolean') ? src.raw : true;

	src.local_pipe = null;
	src.process_id = null;
	if (port.length) {
		let do_cat_url = true;
		let src_url = "";
		let rfopts = "reframer";
		if (use_raw) {			
			rfopts += ":raw=av";
		}
		//use real-time regulation
		if (filter.live) {
			rfopts += ":rt=on";
		}

		if (port==='pipe') {
			src.local_pipe = "gpipe_avmix_" + current_pipe;
			local_filter = "pipe://" + src.local_pipe + ":mkp:sigeos:block_size=10M";
			if (s.keep_alive) {
				local_filter += ':ka';
			  s.has_ka_process = true;
			}
			append_filter = rfopts + " @ dst=pipe://" + src.local_pipe + ':block_size=10M';
			current_pipe++;
			src_url += "gpac src=";
		}
		else if ((port==='tcp') || (port==='tcpu'))  {
			local_filter = port+"://127.0.0.1:" + current_port + "/:listen";				
			if (s.keep_alive) {
				local_filter += ':ka';
			  s.has_ka_process = true;
			}
			append_filter = rfopts + " @ dst=" + port + "://127.0.0.1:" + current_port + '/';
			current_port++;
			src_url += "gpac src=";
		}
		else {
			local_filter = port;
			do_cat_url = false;
		}
		//URL relative to playlist
		if (do_cat_url) {

			let idx = -1;
			let search=" :#?";
			for (let ci=0; ci<src.in.length; ci++) {
				let c = src.in.charAt(ci);
				if (search.indexOf(c) >= 0) {
					idx=ci;
					break;
				}
			}
			let cat_url;
			if (idx>=0) {
				cat_url = src.in.slice(0, idx); 
			} else {
				cat_url = src.in; 			
			}
			cat_url = sys.url_cat(filter.pl, cat_url);
			if (idx>=0) {
				cat_url += src.in.slice(idx); 
			}
			src_url += cat_url;
		} else {
			src_url += src.in;
		}

		if (append_filter) {
			src_url += ' ' + append_filter+':ext=gsf';
			if ((s.media_start>0) || s.video_time_at_init) {
				let start = s.media_start;
				if (s.video_time_at_init)
						start += (video_time - (s.video_time_at_init-1)) / video_timescale;
				src_url += ':start=' + start;
			}

			if (opts.length)
				src_url += ' ' + opts;
		}

		filter.lock_all(true);
		try { 
			f = filter.add_source(local_filter);
		} catch (e) {
			print(GF_LOG_ERROR, 'Add source ' + local_filter + ' failed: ' + e);
			disable_source(s);
			filter.lock_all(false);
			return;		
		}
		filter.set_source(f);
		filter.lock_all(false);

		if (typeof src.media != 'string') f.media_type = "all";
		else f.media_type = src.media;

		s.fsrc.push(f);

		args = src_url.split(' ');
		try {
			src.process_id = os.exec(args, {block: false} );
		} catch (e) {
			print(GF_LOG_ERROR, 'Failed to launch process for ' + args + ': ' + e);
		}
		print(GF_LOG_INFO, 'Launch process for ' + args + ' OK');
		return;
	} 

	//local load
	filter.lock_all(true);
	//parse command line
	args = src.in.split(' ');
	args = args.filter(function(item){return item;}); 
	let links = [];
	let fchain = [];
	let prev_f = null;
	for (i=0; i<args.length; i++) {
		let arg = args[i];
		if (arg.charAt(0) === "-")
			continue;

		if (arg.charAt(0) === "@") {
			links.push(arg);
			continue;
		}

		try { 
			if (prev_f) {
				f = filter.add_filter(arg);
			} else {
				//relative to playlist
				f = filter.add_source(arg, filter.pl);
				links.length = 0;
			}
		} catch (e) {
			print(GF_LOG_ERROR, 'Add ' + (prev_f ? 'filter' : 'source') + ' ' + arg + ' failed: ' + e);
			disable_source(s);
			filter.lock_all(false);
			return;		
		}

		//setup links
		if (apply_links(links, f, fchain)) {
			disable_source(s);
			filter.lock_all(false);
			return;					
		}

		//add to chain
		fchain.push(f);
		//remember source filters
		if (!prev_f) {
			if (typeof src.media != 'string') f.media_type = "all";
			else f.media_type = src.media;
			s.fsrc.push(f);
		}

		prev_f = f;
		links.length = 0;
	}
	if (links.length) {
		//setup links
		if (apply_links(links, null, fchain)) {
			disable_source(s);
			filter.lock_all(false);
			return;					
		}
		prev_f = null;	
	}
	if (prev_f) {
		filter.set_source_restricted(prev_f);
	}

	filter.lock_all(false);
	});
}

function get_sequence(seq_id)
{
	for (let i=0; i<sequences.length; i++) {
		if (sequences[i].id === seq_id) return sequences[i];
	}
	return null;
}

function get_sequence_by_json(seq_pl)
{
	let id = (seq_pl==null) ? "_seq_default" : seq_pl.id;
	for (let i=0; i<sequences.length; i++) {
		if (sequences[i].id === id) return sequences[i];
	}
	let seq = {};
	seq.id = id;
	seq.watchers = null;
	seq.pl_update = true;
	seq.active_state = 0;
	seq.start_time = 0;
	seq.stop_time = 0;
	seq.duration = 0;
	seq.start_offset = 0;
	seq.prefetch_check = false;
	seq.loop = false;

	seq.sources = [];
	seq.transition_state = 0;
	seq.transition_effect = null;
	seq.transition = null;
	seq.crc = 0;
	sequences.push(seq);
	print(GF_LOG_DEBUG, 'creating new sequence ID ' + id);
	return seq;
}

function parse_source_opts(src, pl_el)
{
	src.media_start = pl_el.start || 0;
	src.media_stop = pl_el.stop || 0;
	src.audio_fade = pl_el.fade || "inout";
	if (typeof pl_el.mix == 'boolean')
		src.mix = pl_el.mix;
	else
		src.mix = true;

	src.hold = pl_el.hold || false;
	src.seek = pl_el.seek || false;
	if (typeof pl_el.prefetch_ms == 'number') src.prefetch_ms = pl_el.prefetch;
	else src.prefetch_ms = 500;
	src.prefetch_sec = src.prefetch_ms / 1000;
}

function push_source(el, id, seq)
{
	let s = {};
	s.id = id || "";
	s.src = el.src;
	s.watchers = null;
	s.fsrc = [];
	s.pids = [];
	s.timeline_init=false;
	s.playing = false;
	s.duration = 0;
	s.disabled = false;
	s.opened = false;
	s.in_prefetch = 0;
	s.next_in_transition = false;
	s.transition_offset = 0;
	s.removed = 0;
	s.mix_volume = 1.0;
	s.keep_alive = el.keep_alive || false;
	s.video_time_at_init = 0;
	s.no_signal = false;
	s.no_signal_state = false;

	sources.push(s);
	if (s.id==="") {
		s.logname = el.src[0].in.split('\\').pop().split('/').pop(); 
	} else {
		s.logname = s.id; 
	}
	let parent_seq = get_sequence_by_json(seq);

	s.pl_update = true;
	parent_seq.sources.push(s);
	s.sequence = parent_seq;

	parse_source_opts(s, el);

	let do_start = (parent_seq.sources.length==1) ? true : false;
	if (parent_seq.start_time <0) {
			do_start = false;
	} else if (parent_seq.start_time > 0) {
			do_start = false;
//	} else if (parent_seq.start_time>0) {
//			parent_seq.start_offset = (current_utc_clock - parent_seq.start_time) / 1000.0;
	}

	if (do_start) {
		print(GF_LOG_DEBUG, 'open and play source');
		open_source(s);
		play_source(s);
	} else {
		print(GF_LOG_DEBUG, 'queue source in seq');		
	}
}

function parse_url(src_pl, seq_pl)
{
	if (!validate_source(src_pl))
		return;

	let id = src_pl.id || 0;

	let par_seq = seq_pl ? get_sequence_by_json(seq_pl) : null;
	let src = get_source(id, src_pl.src, par_seq);
	if (!src) {
		push_source(src_pl, id, seq_pl);
	} else {
		if (src.sequence != par_seq) {
			print(GF_LOG_ERROR, 'source update cannot change parent sequence, ignoring ' + JSON.stringify(seq_pl));
			return;
		}
		src.pl_update = true;

		src.src = src_pl.src;
		parse_source_opts(src, src_pl);
	}
}

function validate_source(pl)
{
	let valid=true;
	if (typeof pl.src == 'undefined') {
		print(GF_LOG_ERROR, 'Source element without `src`, ignoring URL element');
		return false;
	}
	pl.src.forEach(s => {
		if (typeof s.in == 'undefined') {
			print(GF_LOG_ERROR, 'Source element without input URL, ignoring URL element');
			valid=false;
			return;
		}
	});
	if (!valid)
		return false;

	for (var propertyName in pl) {
		if (propertyName == 'id') continue;
		if (propertyName == 'src') continue;
		if (propertyName == 'start') continue;
		if (propertyName == 'stop') continue;
		if (propertyName == 'volume') continue;
		if (propertyName == 'mix') continue;
		if (propertyName == 'fade') continue;
		if (propertyName == 'keep_alive') continue;
		if (propertyName == 'seek') continue;
		if (propertyName == 'hold') continue;

		if (propertyName.charAt(0) == '_') continue;

		print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in source ' + JSON.stringify(pl) );
	}

	pl.src.forEach(s => {
		for (var propertyName in s) {
			if (propertyName == 'id') continue;
			if (propertyName == 'port') continue;
			if (propertyName == 'in') continue;
			if (propertyName == 'opts') continue;
			if (propertyName == 'media') continue;
			if (propertyName == 'raw') continue;
			if (propertyName.charAt(0) == '_') continue;

			print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in sourceURL ' + JSON.stringify(pl) );
		}
	});

	return true;
}

function validate_seq(pl)
{
	let valid=true;
	pl.seq.forEach(s => {
		if (!validate_source(s)) {
			valid=false;
			return;
		}
	});
	if (!valid) return false;

	for (var propertyName in pl) {
		if (propertyName == 'id') continue;
		if (propertyName == 'loop') continue;
		if (propertyName == 'start') continue;
		if (propertyName == 'stop') continue;
		if (propertyName == 'transition') continue;
		if (propertyName == 'seq') continue;
		if (propertyName == 'skip') continue;
		if (propertyName.charAt(0) == '_') continue;

		print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in sequence ' + JSON.stringify(pl) );
	}

	return true;
}

function parse_seq(pl)
{
	if (!validate_seq(pl))
		return;

	let seq = get_sequence_by_json(pl);
	//got new seg
	seq.pl_update = true;

	let first = seq.crc ? false : true;
	let crc = sys.crc32(JSON.stringify(pl));
	if (crc != seq.crc) {
		seq.crc = crc;
	} else {
		seq.sources.forEach(s => {
			s.pl_update = true;
		});
		return;
	}

  //parse UTC start time. If reload and new start and active, we cannot modify the start time
	if (!first && (seq.start_time_json != pl.start) && (seq.active_state == 1)) {
		print(GF_LOG_ERROR, 'Cannot change sequence start time while active, ignoring start ' + pl.start);
	}
	//we're not started or over, parse the new start
	else if (seq.active_state != 1) {
		let old_start = seq.start_time;
		let old_start_json = seq.start_time_json;
		seq.start_time = parse_date_time(pl.start, true);
		if (seq.start_time<0) {
			seq.start_time = -1;
		}
		//start is a date, don't skip range adjustment
		else if ((typeof pl.start == 'string') && (pl.start.indexOf(':')>0)) {
			seq.skip_start_offset = false;
		}
		//start is not a date, source will start from its start range
		else {
			seq.skip_start_offset = true;
		}
		//seq is over, check if we need to restart
		if (seq.active_state>1) {
			if ((pl.start != "now") && (pl.start != old_start_json) && (seq.start_time > old_start))
				seq.active_state = 0;
		}
		seq.start_time_json = pl.start;
	}

	seq.stop_time = parse_date_time(pl.stop, true);
	if ((seq.stop_time<0) || (seq.stop_time<=seq.start_time)) 
		seq.stop_time = -1;

	seq.transition_effect = pl.transition || null;

	seq.loop = pl.loop || 0;
	if (seq.loop && (typeof pl.loop == 'boolean'))
		seq.loop = -1;
	pl.seq.forEach( p => { parse_url(p, pl); });


	if (seq.id && scene_in_update) {
		//we are called from a module, force seting up scenes using this sequence
		do_traverse(root_scene, scene => {
			let is_match = false;
			//either removed or not yet loaded
			if (!scene.mod) return;

			scene.sources.forEach( seq_id => {
				if (seq_id == seq.id) is_match = true;
			});
			if (is_match) {
				setup_scene(scene, scene.sources, null);
			}
		});
	}
}

function validate_scene(pl)
{
	if (typeof pl.js == 'undefined') {
		pl.js = 'shape';
	}
	else if (typeof pl.js != 'string') {
		print(GF_LOG_WARNING, 'Invalid scene.js, expecting string - ignoring element ' + JSON.stringify(pl) );
		return false;
	}
	if (Array.isArray(pl.sources)) {
		for (let i=0; i<pl.sources.length; i++) {
			let s_id = pl.sources[i];			
			if (typeof s_id != 'string') {
				print(GF_LOG_WARNING, 'Invalid scene.sources element ' + s_id + ', expecting string - ignoring element ' + JSON.stringify(pl) );
				return false;
			}
			let src = get_sequence(s_id);
			if (!src) {
				src = get_group(s_id);
				if (src && (src.offscreen==GROUP_OST_NONE)) src = null;
			}
			if (!src) {
				print(GF_LOG_WARNING, 'Invalid scene.sources element ' + s_id + ', source sequence not found - ignoring element ' + JSON.stringify(pl) );
				return false;
			}
		}
	} else if (typeof pl.sources !== 'undefined') {
			print(GF_LOG_WARNING, 'Invalid scene.sources element invalid, expecting undefined or string array - ignoring element ' + JSON.stringify(pl) );
	}

	let scene_id = pl.id || null;
	if (scene_id) {
		let nb_scenes=0;
		do_traverse(root_scene, s => {
			if (s.id === scene_id) nb_scenes++;
		});
		if (nb_scenes>1) {
			print(GF_LOG_WARNING, 'More than one scene defined with the same id ' + scene_id + ', ignoring element ' + JSON.stringify(pl) );
			return false;
		}
	}
	return true;
}


function get_element_by_id(elem_id, list)
{
	if (!elem_id) return null;

	for (let i=0; i<list.length; i++) {
		let e = list[i];
		if (elem_id==e.id)
				return e;
	}
	return null;
}


let ID_scenes = [];

function get_scene(scene_id)
{
	return get_element_by_id(scene_id, ID_scenes);
}

function check_transition(fx_id)
{
	let fx = null;

	do_traverse(root_scene, scene => {
		if (fx || !scene.transition_effect) return;

		let a_fx_id = scene.transition_effect.id || null;
		if (a_fx_id && (fx_id==a_fx_id)) {
			fx = scene.transition_effect;
		}
	});
	if (fx != null) return true;
	return false;
}

function get_a_trans(obj, fx_id, get_json)
{
	if (!obj.transition_effect) return null;

	let a_fx_id = obj.transition_effect.id || null;
	if (a_fx_id && (fx_id==a_fx_id)) {
		return get_json ? obj.transition_effect : obj.transition;
	}
	return null;
}

function get_transition(fx_id, get_json)
{
	let tr = null;
	do_traverse(root_scene, scene => {
		if (tr) return;
		tr = get_a_trans(scene, fx_id, get_json);
	});
	if (tr) return tr;

	for (let i=0; i<sequences.length; i++) {
		tr = get_a_trans(sequences[i], fx_id, get_json);
		if (tr) return tr;
	}
	return null;
}


function parse_scene(pl, parent)
{
	if (!validate_scene(pl))
		return null;

	if (pl.skip || false)
		return null;

	let scene_id = pl.id || null;
	let scene = get_scene(scene_id);
	if (!scene) {
		if (typeof pl.sources == 'undefined') pl.sources = [];

		create_scene(pl.sources, pl, parent);
	} else if (scene.pl_update) {
		print(GF_LOG_WARNING, "Multiple scenes with id `" + scene_id + "` defined, ignoring subsequent declarations");
		return null;
	} else {
		scene.pl_update = true;
		if (parent) parent.scenes.push(scene);
		else root_scene.scenes.push(scene);
		set_scene_options(scene, pl);

		return scene;
	}
}

function parse_config(pl)
{
	for (var propertyName in pl) {
		if (propertyName == 'gpu') {
			if (pl.gpu=='off') filter.gpu = 0;
			else if (pl.gpu=='mix') filter.gpu = 1;
			else if (pl.gpu=='all') filter.gpu = 2;
			else print(GF_LOG_WARNING, "Wrong syntax for option \`gpu\` in playlist config, ignoring");
		}
		else if (propertyName == 'vsize') {
			var s = pl.vsize.split('x');
			if (s.length==2) {
				filter.vsize.x = parseInt(s[0]);
				filter.vsize.y = parseInt(s[1]);
			}	
			else print(GF_LOG_WARNING, "Wrong syntax for option \`vsize\` in playlist config, ignoring");
		}
		else if (propertyName == 'fps') {
			if (typeof pl.fps == 'string') {
				var fps = pl.fps.split('/');
				filter.fps.n = parseInt(fps[0]);
				filter.fps.d = (fps.length>2) ? parseInt(fps[1]) : 1;
			} else if (typeof pl.fps == 'number') {
				filter.fps.n = pl.fps;
				filter.fps.d = 1;
			}	
			else print(GF_LOG_WARNING, "Wrong syntax for option \`fps\` in playlist config, ignoring");
		}
		else if (propertyName == 'dynpfmt') {
			if (pl.dynpfmt=='off') filter.dynpfmt=0;
			else if (pl.dynpfmt=='init') filter.dynpfmt=1;
			else if (pl.dynpfmt=='all') filter.dynpfmt=2;
			else print(GF_LOG_WARNING, "Wrong syntax for option \`dynpfmt\` in playlist, ignoring");
		}
		else if (propertyName == 'pfmt') filter.pfmt = pl.pfmt;
		else if (propertyName == 'afmt') filter.afmt = pl.afmt;
		else if ( (propertyName == 'thread') || (propertyName == 'lwait') || (propertyName == 'ltimeout')
			 || (propertyName == 'sr') || (propertyName == 'ch') || (propertyName == 'alen') || (propertyName == 'maxdur')
		) {
			if (typeof pl[propertyName]  != 'number') print(GF_LOG_WARNING, "Expecting number for option \`" + propertyName + "\` in playlist config, ignoring");
			else filter[propertyName] = pl[propertyName];
		}
		else if (propertyName == 'live') {
			if ((typeof pl[propertyName]  != 'number') && (typeof pl[propertyName]  != 'boolean')) print(GF_LOG_WARNING, "Expecting boolean for option \`" + propertyName + "\` in playlist config, ignoring");
			else filter[propertyName] = pl[propertyName];
		}
		else if ((propertyName == 'pfmt') || (propertyName == 'afmt')) {
			if (typeof pl[propertyName]  != 'string') print(GF_LOG_WARNING, "Expecting string for option \`" + propertyName + "\` in playlist config, ignoring");
			else filter[propertyName] = pl[propertyName];
		}
		else if (propertyName == 'reload_tests') {
			if (Array.isArray(pl.reload_tests) && (typeof pl.reload_tests[0] == 'string') ) {
				reload_tests = pl.reload_tests;
			}
			else if (typeof pl.reload_tests == 'string') {
				reload_tests = [];
				reload_tests.push(pl.reload_tests);
			} else {
				print(GF_LOG_WARNING, 'Expecting string or string array for option ' + propertyName + ' in playlist config, ignoring');
			}
		}
		else if ((propertyName == 'reload_timeout') || (propertyName == 'reload_loop')) {
			if (typeof pl[propertyName]  != 'number') print(GF_LOG_WARNING, "Expecting number for option \`" + propertyName + "\` in playlist config, ignoring");
			else if (propertyName == 'reload_timeout') reload_timeout = pl.reload_timeout;
			else if (propertyName == 'reload_loop') reload_loop = pl.reload_loop;
		}
		else if (propertyName == 'type' ) {}
		else if (propertyName.charAt(0) != '_') {
			print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in playlist config ' + JSON.stringify(pl) );
		}
	}
}

function is_scene_pl(pl)
{
		if (Array.isArray(pl.sources)
		|| (typeof pl.js == 'string')
		|| (typeof pl.x == 'number')
		|| (typeof pl.y == 'number')
		|| (typeof pl.width == 'number')
		|| (typeof pl.height == 'number')
	)
		return true;

	return false;
}

function validate_group(pl)
{
	let valid=true;

	if (Array.isArray(pl.scenes)) {
		pl.scenes.forEach(s => {
			if (Array.isArray(s.scenes)) {
				if (!validate_group(s))
					valid = false;
			}
			else if (is_scene_pl(s)) {
				if (!validate_scene(s))
					valid = false;
			}
		});
	}

	if (!valid) return false;

	for (var propertyName in pl) {
		if (propertyName == 'id') continue;
		if (propertyName == 'skip') continue;
		if (group_get_update_type(propertyName, false) >= -1) continue;
		if (propertyName.charAt(0) == '_') continue;

		print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in group ' + JSON.stringify(pl) );
	}

	return true;
}

let ID_groups = [];

function get_group(group_id)
{
	return get_element_by_id(group_id, ID_groups);
}


const group_transform_props = [
	{ name: "active", def_val: true, update_flag: 0},
	{ name: "zorder", def_val: 0, update_flag: 0},
	{ name: "x", def_val: 0, update_flag: UPDATE_POS|UPDATE_ALLOW_STRING},
	{ name: "y", def_val: 0, update_flag: UPDATE_POS|UPDATE_ALLOW_STRING},
	{ name: "rotation", def_val: 0, update_flag: UPDATE_POS},
	{ name: "cx", def_val: 0, update_flag: UPDATE_POS},
	{ name: "cy", def_val: 0, update_flag: UPDATE_POS},
	{ name: "hskew", def_val: 0, update_flag: UPDATE_POS},
	{ name: "vskew", def_val: 0, update_flag: UPDATE_POS},
	{ name: "vscale", def_val: 1, update_flag: UPDATE_POS},
	{ name: "hscale", def_val: 1, update_flag: UPDATE_POS},
	{ name: "untransform", def_val: false, update_flag: UPDATE_POS},
	{ name: "units", def_val: null, update_flag: -1},
	{ name: "mxjs", def_val: null, update_flag: -1},
	{ name: "z", def_val: 0, update_flag: UPDATE_CHILD},
	{ name: "cz", def_val: 0, update_flag: UPDATE_CHILD},
	{ name: "zscale", def_val: 1, update_flag: UPDATE_CHILD},
	{ name: "orientation", def_val: null, update_flag: UPDATE_CHILD},
	{ name: "axis", def_val: null, update_flag: UPDATE_CHILD},
	{ name: "position", def_val: null, update_flag: UPDATE_CHILD},
	{ name: "target", def_val: null, update_flag: UPDATE_CHILD},
	{ name: "up", def_val: null, update_flag: UPDATE_CHILD},
	{ name: "viewport", def_val: null, update_flag: UPDATE_CHILD|UPDATE_ALLOW_STRING},
	{ name: "fov", def_val: 45, update_flag: UPDATE_CHILD},
	{ name: "ar", def_val: 0, update_flag: UPDATE_CHILD},
	{ name: "znear", def_val: 0, update_flag: UPDATE_CHILD},
	{ name: "zfar", def_val: 0, update_flag: UPDATE_CHILD},
];


const group_props = [
  //first three are not updatable and no default value
	{ name: "scenes", def_val: null, update_flag: -1},
	{ name: "offscreen", def_val: null, update_flag: -1},
	{ name: "use", def_val: null, update_flag: -1},

	{ name: "opacity", def_val: 1, update_flag: UPDATE_CHILD},
	{ name: "scaler", def_val: 1, update_flag: UPDATE_CHILD},
	{ name: "back_color", def_val: 'none', update_flag: UPDATE_CHILD},
	{ name: "width", def_val: -1, update_flag: UPDATE_CHILD|UPDATE_ALLOW_STRING},
	{ name: "height", def_val: -1, update_flag: UPDATE_CHILD|UPDATE_ALLOW_STRING},
	{ name: "use_depth", def_val: -1, update_flag: UPDATE_CHILD},
	{ name: "reverse", def_val: false, update_flag: UPDATE_CHILD},
	{ name: "reference", def_val: false, update_flag: UPDATE_CHILD},
];


function parse_group_transform(scene, params)
{
	scene.position = null;
	scene.target = null;
	scene.up = null;
	scene.viewport = null;

	group_transform_props.forEach(o => {
		parse_val(params, o.name, scene, o.def_val, o.update_flag);
	});

	scene.units = UNIT_RELATIVE;
	if (typeof params.units == 'string') {
		if (params.units == 'pix') scene.units = UNIT_PIXEL;
		else if (params.units == 'rel') scene.units = UNIT_RELATIVE;
		else {
			print(GF_LOG_WARNING, 'Invalid unit type ' + params.units + ', using relative');
		}
	}
	scene.mx_untransform = false;
	scene.mxjs = (typeof params.mxjs == 'string') ? fn_from_script(['tr'], params.mxjs) : null;
}

function group_get_update_type(prop_name, transform_only)
{
	let i;

	for (i=0; i<group_transform_props.length; i++) {
		if (group_transform_props[i].name == prop_name) return group_transform_props[i].update_flag;
	}
	if (transform_only) return -2;

	for (i=0; i<group_props.length; i++) {
		if (group_props[i].name == prop_name) return group_props[i].update_flag;
	}
	return -2;
}

function parse_group(pl, parent)
{
	if (!validate_group(pl))
		return null;

	if (pl.skip || false)
		return null;

	let group_id = pl.id || null;
	let group = get_group(group_id);
	if (!group) {
		group = {};
		group.watchers = null;
		group.id = group_id;
		group.scenes = [];
		group.parent = parent;
		group.current_depth = 0;

		if (group_id) ID_groups.push(group);
		group.mx = new evg.Matrix2D();
		group.offscreen_canvas = null;
		group.update_flag = UPDATE_CHILD;

		group.get = function(prop) {
			if (typeof this[prop] != 'undefined') return this[prop];
			return undefined;
		}
		group.set = function(prop, val) {
			let update_type = group_get_update_type(prop, false);
			//no modifications allowed or not defined 
			if (update_type<0) {
				return;
			}

			let allow_string = (update_type & UPDATE_ALLOW_STRING) ? true : false;
			update_type &= ~UPDATE_ALLOW_STRING;

			if ((this[prop] != val) && check_prop_type(typeof this[prop], typeof val, allow_string)) {
				this.update_flag |= update_type;
				this[prop] = val;
				if (update_type)
					invalidate_parent(this);
				trigger_watcher(group, prop, val);
			}
		}
	} else if (group.pl_update) {
		print(GF_LOG_WARNING, "Multiple groups with id `" + group_id + "` defined, ignoring subsequent declarations");
		return null;
	}
	group.pl_update = true;

	group.update_flag |= UPDATE_POS;

	group_props.forEach(o => {
		if (o.def_val == null)  return;
			parse_val(pl, o.name, group, o.def_val, o.update_flag);
	});
	parse_group_transform(group, pl);

	//ad-hoc parsing for offscreen
	group.offscreen = GROUP_OST_NONE;
	if (typeof pl.offscreen == 'string') {
		if (pl.offscreen == 'mask') group.offscreen = GROUP_OST_MASK;
		else if (pl.offscreen == 'color') group.offscreen = GROUP_OST_COLOR;
		else if (pl.offscreen == 'dual') group.offscreen = GROUP_OST_DUAL;
		else if (pl.offscreen == 'none') group.offscreen = GROUP_OST_NONE;
		else {
			print(GF_LOG_WARNING, "Unknown group offscreen mode `" + pl.offscreen + "`, ignoring");
		}
	}
	//ad-hoc parsing for use
	group.use = (typeof pl.use == 'string') ? pl.use : null;

	//tag all scenes with ID as not present
	group.scenes.forEach(scene => {
		scene.pl_update = false;
	});

	group.scenes.length = 0;

	//parse
	if (! group.use) {
		pl.scenes.forEach(s => {
			if (Array.isArray(s.scenes) || (typeof s.use == 'string')) {
				parse_group(s, group);
			}
			else if (is_scene_pl(s)) {
				parse_scene(s, group);
			}
		});
	}

	if (parent) {
		parent.scenes.push(group);
	} else {
		root_scene.scenes.push(group);
	}
	return group;
}

let user_scripts=[];

function get_script(id)
{
	return get_element_by_id(id, user_scripts);
}

function fn_from_script(args, jscode)
{
	let url = sys.url_cat(playlist_url, jscode);
	if (sys.file_exists(url)) {
		return new Function(args, sys.load_file(url, true) );
	} else {
		return new Function(args, jscode);		
	}
}

function parse_script(pl)
{
	if (typeof pl.script != 'string') {
		print(GF_LOG_WARNING, 'Unrecognized script ' + JSON.stringify(pl) );
		return;
	}
	if (pl.skip || false) 
		return;

	let id = pl.id || null;
	let script_obj = id ? get_script(id) : null;
	if (!script_obj) {
		script_obj = {};
		script_obj.watchers = null;
		script_obj.id = id;
		user_scripts.push(script_obj);
	}	else if (script_obj.pl_update) {
		print(GF_LOG_WARNING, "Multiple scripts with id `" + id + "` defined, ignoring subsequent declarations");
		return null;
	}
	
	script_obj.pl_update = true;
	script_obj.next_time = 0;

	if (typeof pl.active == 'boolean')
		script_obj.active = pl.active;
	else
		script_obj.active = true;

	try {
		script_obj.run_script = fn_from_script([], pl.script);
	} catch (err) {
		print(GF_LOG_ERROR, 'Invalid script ' + pl.script + ' : ' + err);
		script_obj.run_script = null;
	}

	for (var propertyName in pl) {
		if (propertyName == 'skip') continue;
		if (propertyName == 'id') continue;
		if (propertyName == 'active') continue;
		if (propertyName == 'script') continue;
		if (propertyName.charAt(0) == '_') continue;

		print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in script ' + JSON.stringify(pl) );
	}

	return;
}

function update_scripts()
{
	for (let i=0; i<user_scripts.length; i++) {
		let script = user_scripts[i];
		if (!script.active) continue;

		if (!script.run_script) {
			user_scripts.splice(i, 1);
			i--;
			continue;
		}

		if (script.next_time) {
			if (video_playing && (script.next_time>video_time)) continue;
			if (audio_playing && (script.next_time>audio_time)) continue;
		} 

		try {
			let res = script.run_script.apply(script, []) || 0;
			if (typeof res == 'number') {
				if (res<0) res=0;

				if (video_playing) res = video_time + res*video_timescale;
				else res = audio_time + res*audio_timescale;
			}
			script.next_time = res;
		} catch (err) {
			user_scripts.splice(i, 1);
			i--;
			print(GF_LOG_ERROR, 'Error executing script  ' + JSON.stringify(script.run_script) + ' : ' + err + ' - removing');
		}
	}
}

function validate_watcher(pl)
{
	let valid=true;
	if ( typeof pl.watch != 'string') {
		print(GF_LOG_ERROR, 'Watcher watch must be a string, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	if (pl.watch != 'events') {
		let evt = pl.watch.split('(');
		if (sys.get_event_type(evt[0]) ) {
		}
		else if (pl.watch.indexOf('@') < 0) {
			print(GF_LOG_ERROR, 'Watcher watch must be in the form `ID@field`, ignoring ' + JSON.stringify(pl) );
			return false;
		}
	}
	if ( typeof pl.target != 'string') {
		print(GF_LOG_ERROR, 'Watcher target must be an array, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	if ((typeof pl.mode != 'string') && (typeof pl.mode != 'undefined')) {
		print(GF_LOG_ERROR, 'Watcher target must be an array, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	if ((pl.mode == 'set') && (pl.target.indexOf('@') < 0)) {
		print(GF_LOG_ERROR, 'Watcher target must be in the form `ID@field`, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	return valid;
}

let watchers=[];
let defer_parse_watchers = null;
let watchers_defered = [];
let event_watchers=[];

function remove_watcher(watcher, parent_only)
{
	let idx;
	if (watcher.parent) {
		idx = watcher.parent.watchers.indexOf(watcher);
		if (idx>=0) watcher.parent.watchers.splice(idx, 1);

		if (watcher.parent.watchers.length == 0) {
			watcher.parent.watchers = null;
		}
		watcher.parent = null;
	} else if (watcher.evt != 0) {
		idx = event_watchers.indexOf(watcher);
		if (idx>=0) event_watchers.splice(idx, 1);
	}

	if (!parent_only) {
		idx = watchers.indexOf(watcher);
		if (idx>=0) watchers.splice(idx, 1);
	}
}


function parse_watcher(pl)
{
	if (!validate_watcher(pl))
		return;

	if (pl.skip || false)
		return;

	let do_register = false;
	let id = pl.id || null;
	let watcher = id ? get_element_by_id(id, watchers) : null;
	if (!watcher) {
		watcher = {}
		watcher.id = id;
		watcher.evt = 0;
		do_register = true;
		watcher.parent = null;
	}	else if (watcher.pl_update) {
		print(GF_LOG_WARNING, "Multiple watchers with id `" + id + "` defined, ignoring subsequent declarations");
		return null;
	}
	watcher.pl_update = true;

	if (typeof pl.active == 'boolean')
		watcher.active = pl.active;
	else
		watcher.active = true;


	for (var propertyName in pl) {
		if (propertyName == 'skip') continue;
		if (propertyName == 'id') continue;
		if (propertyName == 'active') continue;
		if (propertyName == 'watch') continue;
		if (propertyName == 'mode') continue;
		if (propertyName == 'target') continue;
		if (propertyName == 'with') continue;
		if (propertyName.charAt(0) == '_') continue;

		print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in script ' + JSON.stringify(pl) );
	}

	let elem = null;
	if (pl.watch.indexOf('@')>=0) {
		let vals = pl.watch.split("@");
		let s_id = vals[0];

		elem = get_scene(s_id);
		if (!elem) elem = get_group(s_id);
		if (!elem) elem = get_script(s_id);
		if (!elem) elem = get_timer(s_id);
		if (!elem) elem = get_sequence(s_id);
		if (!elem) elem = get_source(s_id, null, null);

		if (!elem) {
			print(GF_LOG_WARNING, 'No watchable element with ID ' + s_id + ' discarding watcher');
			remove_watcher(watcher, false);
			return;
		}

		//parent changed
		if (watcher.src_id && s_id && (watcher.src_id != s_id)) {
			remove_watcher(watcher, true);
		}
		watcher.src_id = s_id;
		watcher.src_field = vals[1];
		watcher.evt = 0;
	} else {
		let evt = pl.watch.split('(');
		remove_watcher(watcher, true);
		watcher.src_id = null;

		if (evt != 'events') {
			watcher.evt = sys.get_event_type(evt[0]);
			if (!watcher.evt) {
				print(GF_LOG_WARNING, 'Unkown event type ' + pl.watch + ' discarding watcher');
				remove_watcher(watcher, false);
				return;
			}
			if (evt.length>1) {
				evt = evt[1].split(')');
				watcher.evt_param = evt[0];
			} else {
				watcher.evt_param = 0;
			}
		} else {
			watcher.evt = -1;
			watcher.evt_param = 0;
		}
	}

	let vals = [];
	if (pl.target.indexOf(' ')>=0) {}
	else if (pl.target.indexOf(')')>=0) {}
	else if (pl.target.indexOf('(')>=0) {}
	else vals = pl.target.split('.');

	if (vals.length==2) {
		let target_scene = get_scene(vals[0]);
		if (!target_scene) {
			print(GF_LOG_WARNING, 'Cannot find scene for target ' + pl.target + ', discarding watcher');
			remove_watcher(watcher);
			return;
		}
		watcher.mode = 3;
		watcher.scene = target_scene;
		watcher.target = vals[1];
	} else {
		let is_script = false;
		let vals = pl.target.split('@');
		if (vals.length>1) {
			if (vals.length>2) is_script = true;
			else if (vals[1].indexOf(' ')>=0) is_script = true;
			else if (vals[1].indexOf('{')>=0) is_script = true;
			else if (vals[1].indexOf('}')>=0) is_script = true;
			else if (vals[1].indexOf('[')>=0) is_script = true;
			else if (vals[1].indexOf(']')>=0) is_script = true;
			else if (vals[1].indexOf('()')>=0) is_script = true;
			else if (vals[1].indexOf(')')>=0) is_script = true;
			else if (vals[1].indexOf('.')>=0) is_script = true;
		} else if (vals.length==1) {
			is_script = true;
		}

		if (is_script) {
			watcher.mode = 2;
			if (watcher.evt)
				watcher.fun = fn_from_script(['evt'], pl.target);
			else
				watcher.fun = fn_from_script(['value'], pl.target);

			if (!watcher.fun) {
				print(GF_LOG_WARNING, 'Cannot parse action function ' + pl.target + ', discardinf watcher');
				remove_watcher(watcher);
				return;
			}
		} else {
			if (watcher.evt) {
				print(GF_LOG_WARNING, 'Cannot use target `ID@property` for event watchers, discarding watcher');
				remove_watcher(watcher, false);
				return;
			}
			watcher.mode = 0;
			watcher.target = pl.target;
			if (typeof pl.with != 'undefined') {
				watcher.with = pl.with;
				watcher.mode = 1;
			}
		}
	}

	if (do_register)
		watchers.push(watcher);

	if (elem) {
		if (!elem.watchers)
			elem.watchers = [];
		elem.watchers.push(watcher);
		watcher.parent = elem;
	} else {
		if (watcher.evt)
			event_watchers.push(watcher);
	}
	return;
}

function flush_watchers()
{
	watchers_defered.forEach(evt => {
		trigger_watcher(evt.elem, evt.prop, evt.val);
	});
	watchers_defered.length = 0;
}

function trigger_watcher(src, prop_name, value)
{
	if (defer_parse_watchers) {
		let evt = {elem: src, prop: prop_name, val: value};
		watchers_defered.push(evt);
		return;
	}

	if (!src.watchers) return;

	src.watchers.forEach(watcher => {
		if (!watcher.active) return;

		if (watcher.src_field != prop_name) return;

		if (watcher.in_callback) {
			print(GF_LOG_WARNING, 'recursive call for watcher ' + watcher.id + ' value was ' + src.id + '@' + prop_name+ '=' + value + ', ignoring');
			return;
		}
		watcher.in_callback = true;
		if (watcher.mode==0) {
			let cmd = {replace: watcher.target, with: value};
			parse_update_elem(cmd);
		} else if (watcher.mode==1) {
			let cmd = {replace: watcher.target, with: watcher.with};
			parse_update_elem(cmd);
		} else if (watcher.mode==2) {
			try {
				watcher.fun.apply(watcher, [value]);
			} catch (err) {
				print(GF_LOG_ERROR, 'Error executing action function: ' + err);
			}
		} else if (watcher.mode==3) {
			if (watcher.scene.mod) {
				if (typeof watcher.scene.mod[watcher.target] == 'function') {
					watcher.scene.mod[watcher.target].apply(watcher.scene.mod, [value, src.id, prop_name]);
				}
			}
		}
		watcher.in_callback = false;

	});
}

let styles=[];

function parse_style(pl)
{
	if (pl.skip || false)
		return;

	let id = pl.id || null;
	if (!id) return;
	let style = get_element_by_id(id, styles);
	if (style) {
		print(GF_LOG_WARNING, "Multiple styles with id `" + id + "` defined, ignoring subsequent declarations");
		return;
	}
	pl.forced = pl.forced || false;
	pl.updated = true;
	styles.push(pl);
}


function parse_playlist_element(pl)
{
	let type = pl.type || null;

	if (pl.skip || false)
		return;

	//guess type of root elements
	if (Array.isArray(pl.src))
		type = 'url';

	if (Array.isArray(pl.seq))
		type = 'seq';

	if (is_scene_pl(pl))
		type = 'scene';

	if (Array.isArray(pl.scenes) || (typeof pl.use == 'string'))
		type = 'group';

	if (Array.isArray(pl.keys) || Array.isArray(pl.anims))
		type = 'timer';

	if (typeof pl.script == 'string')
		type = 'script';

	if (typeof pl.watch == 'string')
		type = 'watch';

	//try config last
	if ((typeof pl.reload_timeout != 'undefined') || (typeof pl.reload_loop != 'undefined'))
		type = 'config';

	if (!type) {
		let nb_props = 0;
		for (var propertyName in pl) {
			if (typeof filter[propertyName] != 'undefined') {
				nb_props++;
			}
		}
		if (nb_props)
			type = 'config';
	}


	if (type==='url') {
		parse_url(pl, null);
	} else if (type==='seq') {
		parse_seq(pl);
	} else if (type==='scene') {
		parse_scene(pl, null);
	} else if (type==='group') {
		parse_group(pl, null);
	} else if (type==='timer') {
		parse_timer(pl);
	} else if (type==='script') {
			parse_script(pl);
	} else if (type==='watch') {
		if (defer_parse_watchers)
			defer_parse_watchers.push(pl);
		else
			parse_watcher(pl);
	} else if (type==='config') {
		if (!playlist_loaded) {
			parse_config(pl);
		}	
	} else if (type==='style') {
		parse_style(pl);
	} else {
		print(GF_LOG_WARNING, 'Root element type ' + type + ' not defined, ignoring element ' + JSON.stringify(pl) );
	}
}

function cleanup_list(type)
{
	let ID_list = null;
	let log_name = null;
	if (!type) { ID_list = ID_scenes; log_name = 'Scene'; }
	else if (type==1) { ID_list = ID_groups; log_name = 'Group'; }
	else if (type==2) { ID_list = user_scripts; log_name = 'Script'; }
	else if (type==3) { ID_list = watchers; log_name = 'Watcher'; }
	else if (type==4) { ID_list = timers; log_name = 'Timer'; }
	else
			return;

	for (let i=0; i<ID_list.length; i++) {
		let s = ID_list[i];
		if (s.pl_update) continue;

		if (s.id)
			print(GF_LOG_INFO, log_name + ' ' + s.id + ' removed');
		else
			print(GF_LOG_INFO, log_name + ' removed');

		ID_list.splice(i, 1);
		i--;

		//group
		if (type==1) {
			let len_before = ID_list.length;
			remove_scene_or_group(s);
			//cleanup of subtree removed a group with ID, restart
			if (len_before != ID_list.length) {
				i=-1;
			}
		}
		//watcher
		else if (type==3) {
			remove_watcher(s, true);
		}
	}
}

function load_playlist()
{
	let pl = null;
	if (arguments.length==1) {
		pl = arguments[0];
		if (!Array.isArray(pl))
			return;
	}

	if (!pl) {
		let last_mtime = sys.mod_time(filter.pl);
		if (!last_mtime || (last_mtime == last_modification))
			return true;

		print(GF_LOG_DEBUG, last_modification ? 'refreshing JSON config' : 'loading JSON config')

		let f = sys.load_file(filter.pl, true);
		if (!f) return  true;

		current_utc_clock = Date.now();

		try {
			pl = JSON.parse(f);
		} catch (e) {
			print(GF_LOG_ERROR, "Invalid JSON playlist specified: " + e);
			last_modification = last_mtime;
			return false;
		}
		last_modification = last_mtime;
	}

	print(GF_LOG_DEBUG, 'Playlist is ' + JSON.stringify(pl) );

	//mark all our objects present
	sequences.forEach(seq => {
		seq.pl_update = false;
		seq.sources.forEach(src => {
			src.pl_update = false;
		});
	});

	//mark all scenes with ID as not updated
	ID_scenes.forEach(scene => { scene.pl_update = false; });
	if (scene_in_update) scene_in_update.pl_update = true;

	//mark all groups with ID as not updated
	ID_groups.forEach(group => { group.pl_update = false; });

	//mark all scripts as not updated
	user_scripts.forEach(o => { o.pl_update = false; });
	//mark all watchers as not updated
	watchers.forEach(o => { o.pl_update = false; });
	//mark all timers as not updated
	timers.forEach(o => { o.pl_update = false; });

	//reset styles
	styles.length = 0;

	//reset root
	root_scene.scenes.length = 0;
	watchers_defered.length = 0;
	defer_parse_watchers = [];

	if (Array.isArray(pl) ) {
		pl.forEach(parse_playlist_element);
	} else {
		parse_playlist_element(pl);
	}

	//cleanup sources no longer in use
	//we don't use cleanup_list because we don't remove the source right away, this is done at next fetch_source
	for (let i=0; i<sources.length; i++) {
		let s = sources[i];
		if (s.pl_update) continue;
		let idx = s.sequence.sources.indexOf(s);
		if (idx>=0) {
	 		s.sequence.sources.splice(idx, 1);
		}
		print(GF_LOG_INFO, 'Source ' + s.logname + ' removed');
 		s.removed = true;
	}

	//cleanup scenes no longer in use
	cleanup_list(0);
	//cleanup groups no longer in use
	cleanup_list(1);
	//cleanup scripts no longer in use
	cleanup_list(2);
	//cleanup watchers no longer in use
	cleanup_list(3);
	//cleanup timers no longer in use
	cleanup_list(4);

	defer_parse_watchers.forEach( parse_watcher );
	defer_parse_watchers = null;

	if (scene_in_update) {
		if (!scene_in_update.parent && (root_scene.scenes.indexOf(scene_in_update) < 0)) {
			root_scene.scenes.push(scene_in_update);
		}
	}

	if (root_scene.scenes.length) {
		generate_default_scene = false;
	}

	//create default scene if needed
	if (!root_scene.scenes.length && sources.length && generate_default_scene) {
		print(GF_LOG_INFO, 'No scenes defined, generating default one');
		create_scene(null, {"id": "_gpac_scene_default", "js": "shape"}, null);
	}
	return true;
}

function parse_val(params, name, scene_obj, def_val, update_type)
{
	let new_val;
	let prop_set = (typeof scene_obj[name] == 'undefined') ? false : true;

	if (typeof params[name] == 'undefined') {
		//first setup, use default value
		if (! prop_set)
			new_val = def_val;
		else
			return;
	} else if (!Array.isArray(params[name]) && (typeof params[name] == 'object')) {
		return;
	} else {		
		new_val = params[name];
	}

	//any change to these properties will require a PID reconfig of the scene
	if (prop_set && (new_val != scene_obj[name])) {
		//scene obj
		if (typeof scene_obj.mod == 'object')
			scene_obj.mod.update_flag |= update_type;
		else
			scene_obj.update_flag |= update_type;
	}
	scene_obj[name] = new_val;
}




const scene_props = [
	{ name: "js", def_val: null, update_flag: -1, is_mod: false},
	{ name: "sources", def_val: null, update_flag: -1, is_mod: false},
	{ name: "mix", def_val: null, update_flag: -1, is_mod: false},
	{ name: "styles", def_val: [], update_flag: 0, is_mod: false},

	{ name: "width", def_val: -1, update_flag: UPDATE_SIZE, is_mod: false},
	{ name: "height", def_val: -1, update_flag: UPDATE_SIZE, is_mod: false},
	{ name: "volume", def_val: 1, update_flag: 0, is_mod: true},
	{ name: "fade", def_val: "inout", update_flag: 0, is_mod: true},
	{ name: "mix_ratio", def_val: 0, update_flag: 0, is_mod: true},
	{ name: "autoshow", def_val: true, update_flag: 0, is_mod: false},
	{ name: "nosig", def_val: "lost", update_flag: 0, is_mod: false},
];

function scene_get_update_type(prop_name)
{
	let i;
	for (i=0; i<scene_props.length; i++) {
		if (scene_props[i].name == prop_name) return scene_props[i].update_flag;
	}
	return -2;
}

function scene_is_mod_option(prop_name)
{
	let i;
	for (i=0; i<scene_props.length; i++) {
		if (scene_props[i].name == prop_name) return scene_props[i].is_mod;
	}
	return -2;
}
function scene_mod_option_update_time(scene, prop_name)
{
	if (!scene.options) return -2;
	for (let i=0; i<scene.options.length; i++) {
		let o = scene.options[i];
		if (o.name != prop_name) continue;
		if (typeof o.dirty == 'number')
			return o.dirty;
		return -1;
	}
	return -2;
}


function set_scene_options(scene, params)
{
	//scene has been removed (duplicated ids)
	if (!scene.mod) return;

	scene_props.forEach(o => {
		if (o.def_val != null) {
			if (o.is_mod)
				parse_val(params, o.name, scene.mod, o.def_val, o.update_flag);
			else {
				parse_val(params, o.name, scene, o.def_val, o.update_flag);
			}
		}
	});

	scene.check_active = false;
	if (scene.autoshow || (scene.nosig != 'lost'))
		scene.check_active = true;

	//common with group
	parse_group_transform(scene, params);

	//parse module options
	if (scene.options) {
		scene.options.forEach( o => {
				if (typeof o.name == 'undefined') return;
				if (typeof o.value == 'undefined') return;
				let prop_set = (typeof scene.mod[o.name] == 'undefined') ? false : true;
				let mix_strings = o.dirty || 0;
				mix_strings = mix_strings & UPDATE_ALLOW_STRING;

				if (params && check_prop_type(typeof params[o.name], typeof o.value, mix_strings) ) {
					if (prop_set && (scene.mod[o.name] != params[o.name])) {
						let modif = true;
						if (Array.isArray(o.value) && (scene.mod[o.name].length == params[o.name].length)) {
							modif = false;
							for (let i=0; i<scene.mod[o.name].length; i++) {
								if (scene.mod[o.name][i] != params[o.name][i]) {
									modif=true;
									break;
								}
							}
						}
						if (modif) {
							let update_type = o.dirty || UPDATE_SIZE;
							scene.mod.update_flag |= update_type;
						}
					}
					scene.mod[o.name] = params[o.name];
					return;
				}
				if (params && (typeof params[o.name] == 'string') && (typeof o.value == 'number') ) {
					 let val = globalThis[params[o.name] ];
					 if (typeof val == typeof o.value) {
							if (prop_set && (scene.mod[o.name] != val)) {
								let update_type = o.dirty || UPDATE_SIZE;
								scene.mod.update_flag |= update_type;
							}
	 						scene.mod[o.name] = val;
	 						return;
					 }
				}
				//allow string to string array
				if (Array.isArray(o.value) && o.value.length && (typeof params[o.name] == 'string') && (typeof o.value[0] == 'string') ) {
						let update_type = o.dirty || UPDATE_SIZE;
						scene.mod.update_flag |= update_type;
 						scene.mod[o.name] = [ params[o.name] ];
 						return;
				}
				if (params && (typeof params[o.name] != 'undefined') ) {
					print(GF_LOG_WARNING, 'Type mismatch for scene parameter ' + o.name + ': expecting ' + typeof o.value + ' got ' + params[o.name] + (prop_set ? ' - ignoring update' : ' - using default value'));
				}
				//set default value
				if (prop_set) return;
				scene.mod[o.name] = o.value;
		});
	}

	for (var propertyName in params) {
		//defined ones at module level
		if (typeof scene.mod[propertyName] != 'undefined') continue;
		//defined ones at scene level
		if (scene_get_update_type(propertyName) > -2) continue;
		if (group_get_update_type(propertyName, true) > -2) continue;
		if (propertyName == 'id') continue;
		if (propertyName == 'skip') continue;
		if (propertyName.charAt(0) == '_') continue;

		if (scene.options && (typeof scene.mod[propertyName] != 'undefined')) continue;

		print(GF_LOG_WARNING, 'Unrecognized scene option ' + propertyName + ' for ' + scene.id + ' ');
	}

	//check if we have a mix instruction
	if (!scene.sequences || (scene.sequences.length<=1)) {
		scene.transition_effect = null;
		scene.transition = null;
	} else {
		let old_fx = scene.transition_effect;
		scene.transition_effect = params.mix || null;

		if (old_fx && scene.transition_effect && (scene.transition_effect.type != old_fx.type)) {
			scene.transition = null;
		}

		if (!scene.transition && scene.transition_effect) {
			scene.transition_state = 0;
			load_transition(scene);
		}
	}
}

function setup_scene(scene, seq_ids, params)
{

	scene.sequences = [];
	scene.resetup_pids = true;
	scene.mod.pids = [];
	scene.apids = [];
	scene.transition_effect = null;
	scene.mod.mix_ratio = 0;
	scene.transition = null;

	scene.sources = seq_ids;

	//default scene
	if (!seq_ids) {
		scene.sequences.push( sequences[0] );
	} else if (typeof seq_ids != 'undefined') {
		seq_ids.forEach(sid =>{
			let s = get_sequence(sid);
			if (!s) {
				s = get_group(sid);
				if (s && (s.offscreen == GROUP_OST_NONE)) s = null;
			}
			if (s)
				scene.sequences.push(s);
		}); 
	}

	scene.mod.update_flag = UPDATE_PID;
	if (params)
		set_scene_options(scene, params);
}

function remove_scene_or_group(elmt)
{
	let par = elmt.parent ? elmt.parent : root_scene;
  let index = par.scenes.indexOf(elmt);
  if (index > -1) par.scenes.splice(index, 1);

  if (elmt.id) {
		//traverse down tree, remove all elements with ID
		do_traverse_all(elmt, e => {
			e.parent = null;
			if (!e.id) return;
			let id_list = Array.isArray(e.scenes) ? ID_groups : ID_scenes;
			let index = id_list.indexOf(e);
			if (index > -1) id_list.splice(index, 1);
		});
  }
}


function create_scene(seq_ids, params, parent)
{
	let script_src = sys.url_cat(playlist_url, params.js);


	if (! sys.file_exists(script_src)) {
		script_src = filter.jspath + 'scenes/' + params.js + '.js';

		if (! sys.file_exists(script_src)) {
			print(GF_LOG_ERROR, 'No such scene file ' + script_src);
			return null;
		}
	}

	modules_pending ++;
	let scene = {};
	scene.id = params.id || null;
	scene.watchers = null;
	scene.pl_update = true;
	scene.mod = null;
	scene.sequences = [];
	scene.transition_effect = params.mix || null;
	scene.gl_type = SCENE_GL_NONE;
	scene.parent = parent;
	scene.mx = new evg.Matrix2D();

	scene.get = function(prop) {
		if (typeof this[prop] != 'undefined') return this[prop];
		if (typeof this.mod[prop] != 'undefined') return this.mod[prop];
		return undefined;
	}

	scene.set = function(prop, val) {
		let obj = this;

		//special handling of scenes
		if (prop=='sources') {
			if (Array.isArray(val)) {
				setup_scene(obj, val, null);
			}
			return;
		}
		let update_type = group_get_update_type(prop, true);

		if (update_type==-2) {
			update_type = scene_mod_option_update_time(this, prop);
			if (update_type>=0) {
				obj = this.mod;
			} else if (update_type==-2) {
				update_type = scene_get_update_type(prop);
				if ((update_type>=0) && scene_is_mod_option(prop)) {
					obj = this.mod;
				}
			}
		}
		
		//no modifications allowed or not defined 
		if (update_type<0) {
			return;
		}

		let allow_string = (update_type & UPDATE_ALLOW_STRING) ? true : false;
		update_type &= ~UPDATE_ALLOW_STRING;

		if ((obj[prop] != val) && check_prop_type(typeof obj[prop], typeof val, allow_string)) {
			obj.update_flag |= update_type;
			obj[prop] = val;
			if (update_type)
				invalidate_parent(this);
			trigger_watcher(scene, prop, val);
		}
	}

	if (parent)
		parent.scenes.push(scene);
	else
		root_scene.scenes.push(scene);

	if (scene.id)
		ID_scenes.push(scene);

	import(script_src)
		  .then(obj => {
		  		modules_pending--;
		  		print(GF_LOG_DEBUG, 'Module ' + script_src + ' loaded');
		  		scene.mod = obj.load();
					scene.mod.id = scene.id || "unknown";

		  		if (typeof scene.mod.fullscreen != 'function')
		  				scene.mod.fullscreen = function () { return -1; };

		  		if (typeof scene.mod.is_opaque != 'function')
		  				scene.mod.is_opaque = function () { return false; };

		  		if (typeof scene.mod.identity != 'function')
		  				scene.mod.identity = function () { return false; };

		  		let has_draw = (typeof scene.mod.draw == 'function') ? true : false;
		  		let has_draw_gl = (typeof scene.mod.draw_gl == 'function') ? true : false;
		  		if (!has_draw && !has_draw_gl) {
		  				scene.mod.draw = function (canvas) {};
		  		} else if (has_draw && !has_draw_gl) {
		  			scene.gl_type = SCENE_GL_NONE;
		  		} else if (has_draw_gl) {
		  			if (!has_draw) {
			  			scene.gl_type = SCENE_GL_ONLY;
		  				scene.mod.draw = function (canvas) {};
		  				if (!filter.gpu) {
								print(GF_LOG_ERROR, 'GPU not enabled but required for scene ' + params.js + ' - disabling scene');
								remove_scene_or_group(scene);
								return;
		  				}
		  			} else {
			  			scene.gl_type = SCENE_GL_ANY;
		  			}
		  		}

		  		scene.options = obj.options || null;
		  		setup_scene(scene, seq_ids, params);
		  })
		  .catch(err => {
		  		modules_pending--;
					print(GF_LOG_ERROR, "Failed to load scene " + params.js + ' (' + script_src + '): ' + err);
					remove_scene_or_group(scene);
		  });

	//fixme, we need promises here
	return scene;
}

let timers=[];

function validate_timer(pl)
{
	let valid=true;
	if (! Array.isArray(pl.keys)) {
		print(GF_LOG_ERROR, 'Timer keys must be an array, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	if (pl.keys.length<=1) {
		print(GF_LOG_ERROR, 'Timer keys must be at least two, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	if (! Array.isArray(pl.anims)) {
		print(GF_LOG_ERROR, 'Timer anims must be an array, ignoring ' + JSON.stringify(pl) );
		return false;
	}

	let last_val = -1;
	pl.keys.forEach (v => { 
			if (typeof v != 'number') valid=false;
			if (last_val<0) {
				if (v) valid = false;
			}
			else if (last_val >= v) valid = false;
			last_val = v;
	})
	if (!valid) {
		print(GF_LOG_ERROR, 'Timer keys must be numbers, from 0 to 1 in ascending order, ignoring ' + JSON.stringify(pl) );
		return false;
	}
	pl.anims.forEach(anim => {
		if (typeof anim != 'object') {
			print(GF_LOG_ERROR, 'Timer anims must be objects not ' + typeof anim + ', ignoring ' + JSON.stringify(pl) );
			valid = false;
			return;
		}
		if (! Array.isArray(anim.values) || ! Array.isArray(anim.targets)) {
			print(GF_LOG_ERROR, 'Timer anims values / targets must be an array, ignoring ' + JSON.stringify(pl) );
			valid = false;
			return;
		}
		if (anim.values.length != pl.keys.length) {
			print(GF_LOG_ERROR, 'Timer anims values must be in same quantity as keys, ignoring ' + JSON.stringify(pl) );
			valid = false;
			return;
		}
		anim.values.forEach(val => {
			if (typeof val != 'string' && typeof val != 'number' && typeof val != 'boolean' && !Array.isArray(val)) {
				print(GF_LOG_ERROR, 'Timer anims values must be string or numbers, ignoring ' + JSON.stringify(pl) );
				valid = false;
				return;
			}
		});
		anim.targets.forEach(val => {
			if (typeof val != 'string') {
				print(GF_LOG_ERROR, 'Timer anims targets must be strings, ignoring ' + JSON.stringify(pl) );
				valid = false;
				return;
			}
			if (val.indexOf('@') < 0) {
				print(GF_LOG_ERROR, 'Timer anims targets must be in the form ID@field, ignoring ' + JSON.stringify(pl) );
				valid = false;
				return;
			}
		});
	});
	if (!valid) return false;

	for (var propertyName in pl) {
		if (propertyName == 'id') continue;
		if (propertyName == 'dur') continue;
		if (propertyName == 'loop') continue;
		if (propertyName == 'start') continue;
		if (propertyName == 'stop') continue;
		if (propertyName == 'keys') continue;
		if (propertyName == 'anims') continue;
		if (propertyName == 'pause') continue;
		if (propertyName.charAt(0) == '_') continue;

		print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in timer ' + JSON.stringify(pl) );
	}
	pl.anims.forEach(anim => {
		for (var propertyName in anim) {
			if (propertyName == 'values') continue;
			if (propertyName == 'color') continue;
			if (propertyName == 'angle') continue;
			if (propertyName == 'mode') continue;
			if (propertyName == 'postfun') continue;
			if (propertyName == 'end') continue;
			if (propertyName == 'targets') continue;
			if (propertyName.charAt(0) == '_') continue;

			print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in timer anim ' + JSON.stringify(anim) );
		}
	});

	return true;
}	

function parse_date_time(d, for_seq)
{
	let res = -1;
	if (typeof d == 'string') {
		//float as string means offset to current time, wether live or offline
		if (d.indexOf(':')<0) {
			res = parseFloat(d);
			//NaN
			if (res != res)
				res = -1;
			else {
				let now = for_seq ? current_utc_clock : (init_utc + video_time * 1000 / video_timescale);
				res = now + 1000 * res;
			}
		} else if (filter.live) {
			if (d === 'now') {
				res = current_utc_clock;
			} else {
				res = Date.parse(d);
				//NaN
				if (res != res)
					res = -1;
			}
		} else {
			print(GF_LOG_INFO, 'Date ' + d + ' found start/stop but non-live mode used, will use 0');
			res = 0;
		}
	} else if (typeof d == 'number') {
		if (d >= 0) {
			if (for_seq && !d) {
				res = filter.live ? 0 : 0;
			} else {
				res = (for_seq ? current_utc_clock : init_utc) + 1000 * d;
			}
		}
	} else if (typeof d == 'undefined') {
		res = for_seq ? 0 : init_utc;
	}
	return res;
}

function parse_timer(pl)
{
	if (!validate_timer(pl)) 
		return;

	let eval_start_time = false;
	let timer_id = pl.id || null;
	let timer = timer_id ? get_timer(timer_id) : null;

	if (!timer) {
		timer = {};
		timer.watchers = null;
		timers.push(timer);
		eval_start_time = true;
		timer.active_state = 0;
		timer.is_paused = false;
		timer.id = timer_id;
		timer.crc = 0;
	}	else if (timer.pl_update) {
		print(GF_LOG_WARNING, "Multiple timers with id `" + timer_id + "` defined, ignoring subsequent declarations");
		return null;
	}
	timer.pl_update = true;

	let crc = sys.crc32(JSON.stringify(pl));
	if (crc != timer.crc) {
		//we don't track changes of the timer, we blindly replace it 
		if (timer.crc)
				timer_restore(timer);
		
		timer.crc = crc;
	} else {
		return;
	}

	timer.keys = pl.keys;	
	timer.anims = [];
	pl.anims.forEach(anim =>{
		let a = {};
		a.values = anim.values;
		a.color = anim.color || false;
		a.mode = 0;
		a.fun = null;
		if (typeof anim.mode == 'string') {
			if (anim.mode == "discrete") a.mode = 1;
			else if (anim.mode == "linear") a.mode = 0;
			else {
				a.mode = 2;
				a.fun = fn_from_script(['interp'], anim.mode);
			}
		} 
		
		a.postfun = (typeof anim.postfun == 'string') ? fn_from_script(['res', 'interp'], anim.postfun) : null;

		let mod = anim.end || "freeze";
		a.restore = 0;
		if (typeof mod == 'string') {
			if (pl.mod == 'freeze') {}
			else if (mod == 'restore') { a.restore = 1; }
		}

		a.angle = anim.angle || false;

		a.targets = [];
		anim.targets.forEach( t => {
			let vals = t.split("@");
			let tar = {};
			tar.id = vals[0];
			tar.scene = get_scene(vals[0]);
			tar.group = get_group(vals[0]);
			tar.script = get_script(vals[0]);
			tar.watcher = get_element_by_id(vals[0], watchers);
			tar.style = get_element_by_id(vals[0], styles);
			tar.fx = null;
			tar.fx_obj = null;
			if (!tar.scene && !tar.group && check_transition(vals[0]))
				tar.fx = vals[0];

			if (!tar.scene && !tar.group && !tar.fx && !tar.script && !tar.style) {
				print(GF_LOG_ERROR, 'No object with ID ' + vals[0] + ' found, ignoring target');
				return;
			}

			tar.field = vals[1];

			if (tar.field=="id") {
				print(GF_LOG_ERROR, 'ID cannot be animated, ignoring target');
				return;
			}

			tar.scene_obj = false;

			//indexed anim
			if (tar.field.indexOf('[')>0) {
				vals = tar.field.split('[');
				tar.field = vals[0];
				let idx = vals[1].split(']');
				tar.idx = parseInt(idx[0]);
			} else {
				tar.idx = -1;
			}

			if (tar.scene) {
				if (tar.field == 'sources') {
					tar.update_type = UPDATE_PID;
				} else {
					//check transform props
					tar.update_type = group_get_update_type(tar.field, true);
					if (tar.update_type>=0)
							tar.scene_obj = true;
				}

				//check scene (not module) props
				if (tar.update_type == -2) {
					tar.update_type = scene_get_update_type(tar.field);
					if ((tar.update_type>=0) && !scene_is_mod_option(tar.field))
						tar.scene_obj = true;
				}
				if ((tar.update_type == -2) && tar.scene && !tar.scene.mod) {
					//scene module not loaded, defer
					tar.update_type = -3;
				}
				else if ((tar.update_type == -2) && tar.scene && tar.scene.options) {
					tar.update_type = scene_mod_option_update_time(tar.scene, tar.field);
				}
			} else if (tar.group) {
				tar.update_type = group_get_update_type(tar.field, false);
			} else if (tar.script) {
				if (tar.field!="active") {
					print(GF_LOG_ERROR, 'Property ' + tar.field + ' of script cannot be animated, ignoring target');
					return;
				}
				tar.update_type = 0;
			} else if (tar.watcher) {
				if (tar.field!="active") {
					print(GF_LOG_ERROR, 'Property ' + tar.field + ' of watcher cannot be animated, ignoring target');
					return;
				}
				tar.update_type = 0;
			} else if (tar.style) {
				tar.update_type = 0;
			} else {
				tar.update_type = UPDATE_SIZE;
			}

			if (tar.update_type == -2) {
				print(GF_LOG_ERROR, 'No property ' + tar.field + ' in ' + (tar.scene ? 'scene' : 'group') + ', ignoring target');
				return;
			}
			if (tar.update_type == -1) {
				print(GF_LOG_ERROR, 'Property ' + tar.field + ' in ' + (tar.scene ? 'scene' : 'group') + ' cannot be updated, ignoring target');
				return;
			}

			a.targets.push(tar);
		});
		timer.anims.push(a);
	}); 

	timer.loop = pl.loop || false;
	timer.pause = pl.pause || false;

	if (timer.active_state!=1) {
		eval_start_time = true;
	}

	if (eval_start_time) {
		timer.duration = pl.dur || 0;
		timer.start_time = parse_date_time(pl.start, false);
	}

	timer.stop_time = parse_date_time(pl.stop, false);
	if (timer.stop_time<=timer.start_time)
		timer.stop_time = -1;

	if (!timer.duration) timer.active_state = 2;
}

function timer_restore(timer)
{
	//restore values
	timer.anims.forEach(anim => {
		if (anim.restore != 1) return;

		anim.targets.forEach(target => {
			if (!target.scene) return;
			let scene = target.scene.mod;

			if (target.idx>=0) {
				if (Array.isArray(scene[target.field])) {
					scene.update_flag |= target.update_type;
					scene[target.field][target.idx] = target.orig_value;
				}
			} else {
				scene.update_flag |= target.update_type;
				scene[target.field] = target.orig_value;
			}
		});
	});
}

function invalidate_parent(elm)
{
	while (elm.parent) {
		elm.parent.update_flag |= UPDATE_CHILD;
		elm = elm.parent;
	}
}

function get_timer(timer_id)
{
	return get_element_by_id(timer_id, timers);
}

function update_timer(timer)
{
	let do_store=false;

	if (timer.active_state == 2) return;

	let now = init_utc + video_time * 1000 / video_timescale;
	if ((timer.start_time<0) || (timer.start_time > now)) {
		return;
	}

	if (!timer.active_state) {
		timer.active_state = 1;
		trigger_watcher(timer, "active", true);
		timer.activation_time = video_time;
		do_store = true;
	}

	if (timer.pause) {
		if (!timer.is_paused) {
			timer.pause_time = video_time;
			timer.is_paused = true;
		}
		return;
	} else if (timer.is_paused) {
		timer.activation_time += video_time - timer.pause_time;
		timer.is_paused = false;
	}

	let frac = (video_time - timer.activation_time) * video_time_inc / video_timescale;
	
	if (frac > timer.duration) {
		if (!timer.loop && (timer.stop_time<0) ) {
			timer.active_state = 2;
			trigger_watcher(timer, "active", false);
		} else {
			while (frac > timer.duration) frac -= timer.duration;
		}
	}
	if ((timer.stop_time > timer.start_time) && (timer.stop_time <= now)) {
		timer.active_state = 2;
		trigger_watcher(timer, "active", false);
	}

	if (timer.active_state == 2) {
		timer_restore(timer);
		return;
	}

	frac /= timer.duration;
	let s_key = timer.keys[0];
	let e_key = 1;
	let k_idx = 0;
	for (let i=1; i<timer.keys.length; i++) {
			if (frac <= timer.keys[i]) {
				e_key = timer.keys[i];
				break;
			}
			s_key = timer.keys[i];
			k_idx = i;
	}

	let interpolate = (frac - s_key) / (e_key - s_key);

	timer.anims.forEach(anim => {
		let res = 0;
		let value1 = anim.values[k_idx];
		let value2 = anim.values[k_idx+1];
		let interp = interpolate;

		if (anim.angle) { interp *= Math.PI; interp /= 180; }

		else if (anim.fun != null) {
				try {
					interp = anim.fun.apply(anim, [interp]);
					if (typeof interp != 'number')
						throw 'Interpolation function result is not a number !';
				} catch (e) {
					print(GF_LOG_ERROR, "Error processing anim fun " + anim.fun + ': ' + e);
					anim.fun = null;
					return;
				}
				if (interp<0) interp=0;
				else if (interp>1) interp=1;
		}

		if (anim.mode==1) {
			res = value1;
			if (anim.postfun) {
				try {
					res = anim.postfun.apply(anim, [res, interp]);
				} catch (e) {
					print(GF_LOG_ERROR, "Error processing anim postfun " + anim.postfun + ': ' + e);
					anim.postfun = null;
				}
			}
		}
		else if (anim.color) {
			res = sys.color_lerp(interp, value1, value2);
		}
		else if (typeof value1 != 'number') res = value1;
		else if (typeof value2 != 'number') res = value1;
		else {
			res = interp * value2 + (1-interp) * value1;

			if (anim.postfun) {
				try {
					res = anim.postfun.apply(anim, [res, interp]);
				} catch (e) {
					print(GF_LOG_ERROR, "Error processing anim postfun " + anim.postfun + ': ' + e);
					anim.postfun = null;
				}
			}
		}

		for (let i=0; i<anim.targets.length; i++) {
			let target = anim.targets[i];
			let watch_src=null;

			let anim_obj = null;

			if (target.scene) {
				anim_obj = target.scene.mod;
				watch_src = target.scene;
			}
			else if (target.group) {
				anim_obj = target.group;
			}
			else if (target.fx) {
				if (!target.fx_obj) {
					target.fx_obj = get_transition(target.fx, false);
					if (!target.fx_obj) {
						print(GF_LOG_WARNING, 'No transition with target ' + target.field + ' found, removing target');
						anim.targets.splice(i, 1);
						i--;
						continue;
					}
				}
				anim_obj = target.fx_obj.mod;
			}
			else if (target.script) {
				anim_obj = target.script;
			}
			else if (target.style) {
				anim_obj = target.style;
			}
			else if (target.watcher) {
				anim_obj = target.watcher;
			}

			if (!anim_obj) {
				print(GF_LOG_WARNING, 'Animation target ' + target.field + ' not found, removing target');
				anim.targets.splice(i, 1);
				i--;
				continue;
			}

			if (target.scene_obj) {
				anim_obj = target.scene;
			}

			if (typeof anim_obj[target.field] == 'undefined') {
				//special case for scene, some options are not in the scene module
				if (target.scene && ((target.update_type==UPDATE_PID) || (typeof target.scene[target.field] != 'undefined'))) {
					anim_obj = target.scene;
				}
				//otherwise error except for style
				else if (!target.style) {
					print(GF_LOG_WARNING, 'No property named ' + target.field + ' in target obj ID ' + target.id + ', removing target');
					anim.targets.splice(i, 1);
					i--;
					continue;
				}
			}
			let update_type = target.update_type;

			//update type was defered (scene not loaded)
			if ((update_type == -3) && target.scene) {
				if (typeof target.scene.mod[target.field] != 'undefined') {
					update_type = scene_mod_option_update_time(target.scene, target.field);
				}
				if (update_type<0) {
					print(GF_LOG_WARNING, 'No property named ' + target.field + ' in target obj ID ' + target.id + ', removing target');
					anim.targets.splice(i, 1);
					i--;
					continue;
				}
				target.update_type = update_type;
			}


			let allow_string = (update_type & UPDATE_ALLOW_STRING) ? true : false;
			update_type &= ~UPDATE_ALLOW_STRING;

			let final = res;
			if (!watch_src) watch_src = anim_obj;

			if (target.idx>=0) {
				if (Array.isArray(anim_obj[target.field])
					&& (target.idx<anim_obj[target.field].length)
					&& (typeof anim_obj[target.field][target.idx] == typeof final)
				) {
					if (do_store) {
						target.orig_value = anim_obj[target.field][target.idx];
					}
					if ((anim_obj[target.field][target.idx] != final)) {
						anim_obj.update_flag |= update_type;
						anim_obj[target.field][target.idx] = final;
						if (update_type)
							invalidate_parent(target.scene || target.group);

						trigger_watcher(watch_src, target.field, anim_obj[target.field]);
					}
				}
			}
			//we don't check type for style objects, and check ot for others
			else if (target.style || check_prop_type(typeof anim_obj[target.field], typeof final, allow_string)) {
				if (do_store) {
					target.orig_value = anim_obj[target.field];
				}
				if ((anim_obj[target.field] != final)) {
					print(GF_LOG_DEBUG, 'update ' + target.id + '.' + target.field + ' to ' + final);
					if (update_type == UPDATE_PID) {
						setup_scene(target.scene, final, null);
					} else {
						anim_obj.update_flag |= update_type;
						anim_obj[target.field] = final;

						if (target.style)
							target.style.updated = true;
						else if (update_type)
							invalidate_parent(target.group || target.scene);
					}
					trigger_watcher(watch_src, target.field, anim_obj[target.field]);
				}
			} else {
				print(GF_LOG_WARNING, 'Anim type mismatch for ' + target.field + ' got ' + typeof final + ' expecting ' + typeof anim_obj[target.field] + ', ignoring');
			}
		};

	});
}


function clip_to_output(clip)
{
	let rc = {};
	rc.x = clip.x - clip.w/2;
	rc.y = clip.y + clip.h/2;
	rc.w = clip.w;
	rc.h = clip.h;
	rc = active_scene.mx.apply(rc);

	rc.x = Math.floor(rc.x);
	rc.y = Math.floor(rc.y);
	rc.w = Math.floor(rc.w);
	rc.h = Math.floor(rc.h);
	if (!webgl) return round_rect(rc);
	return rc;
}

function canvas_clear(color, clip)
{
	if (!active_scene) return;
	if (active_scene.mx.is3D) {
		print(GF_LOG_ERROR, 'Cannot clear canvas in 3D context');
		return;
	}

	clip = clip_to_output(clip);

	if (webgl) {
		let a = sys.color_component(color, 0);
		let r = sys.color_component(color, 1);
		let g = sys.color_component(color, 2);
		let b = sys.color_component(color, 3);

		webgl.clearColor(r, g, b, a);

		webgl.enable(webgl.SCISSOR_TEST);
		webgl.scissor(video_width/2 + clip.x, clip.y - clip.h + video_height/2, clip.w, clip.h);
		webgl.clear(webgl.COLOR_BUFFER_BIT);
		webgl.disable(webgl.SCISSOR_TEST);

	} else {
		canvas.clear(clip, color);
	}
}

let wgl_clipper = null;

let clip_stack = [];
function canvas_set_clipper(clip, use_stack)
{
	if (!active_scene) return;
	if (active_scene.mx.is3D) {
		print(GF_LOG_ERROR, 'Cannot set clipper in 3D context');
		return;
	}

	if (use_stack) {
		if (!clip) {
			clip_stack.pop();
			clip = clip_stack.length ? clip_stack[clip_stack.length - 1] : null;
		} else {
			clip = clip_to_output(clip);

			if (clip_stack.length) {
				let prev_clip = clip_stack[clip_stack.length - 1];
				clip = sys.rect_intersect(prev_clip, clip);
			}
			clip_stack.push(clip);
		}
	} else {
		if (clip)
			clip = clip_to_output(clip);
	}
	if (webgl) {
		wgl_clipper = clip;
	} else {
		canvas.clipper = clip;
	}
}

let mask_canvas = null;
let mask_canvas_data = null;
let mask_texture = null;
let mask_gl_stencil = null;

function canvas_set_mask_mode(mode)
{
		if (use_gpu) {
			if (mode) {
				if (!mask_canvas_data) {
					mask_canvas_data = new ArrayBuffer(video_width * video_height);
					mask_canvas = new evg.Canvas(video_width, video_height, "grey", mask_canvas_data);
					mask_texture = webgl.createTexture();

					webgl.bindTexture(webgl.TEXTURE_2D, mask_texture);
					webgl.texParameteri(webgl.TEXTURE_2D, webgl.TEXTURE_WRAP_S, webgl.CLAMP_TO_EDGE);
					webgl.texParameteri(webgl.TEXTURE_2D, webgl.TEXTURE_WRAP_T, webgl.CLAMP_TO_EDGE);
					webgl.texParameteri(webgl.TEXTURE_2D, webgl.TEXTURE_MIN_FILTER, webgl.LINEAR);
					webgl.texParameteri(webgl.TEXTURE_2D, webgl.TEXTURE_MAG_FILTER, webgl.LINEAR);
					webgl.texImage2D(webgl.TEXTURE_2D, 0, webgl.LUMINANCE, video_width, video_height, 0, webgl.LUMINANCE, webgl.UNSIGNED_BYTE, mask_canvas_data);
					webgl.bindTexture(webgl.TEXTURE_2D, 0);

					mask_gl_stencil = new evg.SolidBrush();
					mask_gl_stencil.set_color("black");
				}
				let clear = null;
				if (mode == GF_EVGMASK_DRAW) {
					clear = 'black';
				}
				else if (mode == GF_EVGMASK_RECORD) {
					clear = 'white';
				}
				else if (mode == GF_EVGMASK_DRAW_NO_CLEAR) {
					mode = GF_EVGMASK_DRAW;
				}

				if (clear) {
					mask_canvas.clear(clear);
				}
			}
		} else {
			canvas.mask_mode = mode;
		}

		mask_mode = mode;
}


let canvas_offscreen_active = false;

function flush_offscreen_canvas()
{
 	canvas_offscreen._gl_texture.upload(canvas_offscreen._evg_texture);

  let glprog = canvas_offscreen._gl_program;
  webgl.useProgram(glprog.program);

  webgl.viewport(0, 0, video_width, video_height);

  //set video texture
  webgl.activeTexture(webgl.TEXTURE0);
  webgl.bindTexture(webgl.TEXTURE_2D, canvas_offscreen._gl_texture);
	webgl.uniform1i(glprog.textures[0].sampler, 0);

	canvas_offscreen._mesh.draw(webgl, glprog.attribLocations.vertexPosition, glprog.attribLocations.textureCoord);

  webgl.useProgram(null);
}

function canvas_offscreen_activate()
{
	if (canvas_offscreen_active) return true;

	if (!canvas_offscreen) {
		canvas_offscreen = new evg.Canvas(video_width, video_height, 'rgba');
		if (!canvas_offscreen) return false;
		canvas_offscreen._evg_texture = new evg.Texture(canvas_offscreen);
		if (!canvas_offscreen._evg_texture) return false;

		canvas_offscreen._gl_texture = webgl.createTexture(null);
		if (!canvas_offscreen._gl_texture) return false;
		canvas_offscreen._gl_tx_id = webgl.textureName(canvas_offscreen._gl_texture);


	 	canvas_offscreen._gl_texture.upload(canvas_offscreen._evg_texture);

		let path = new evg.Path().rectangle(0, 0, video_width, video_height, true);
		canvas_offscreen._mesh = new evg.Mesh(path);
		if (!canvas_offscreen._mesh) return;
		canvas_offscreen._mesh.update_gl();

  	canvas_offscreen._gl_program = setup_webgl_program(vs_source, fs_source, canvas_offscreen._gl_tx_id);
	}
	canvas_offscreen.clearf(0, 0, 0, 0);
	canvas_offscreen_active = true;
	return true;
}

function canvas_offscreen_deactivate()
{
	if (!canvas_offscreen_active) return;

	canvas_offscreen_active = false;
	flush_offscreen_canvas();
}

let no_signal_text = null;
let no_signal_brush = new evg.SolidBrush();
let no_signal_path = null;
let no_signal_outline = null;

let no_signal_transform = new evg.Matrix2D();

function draw_scene_no_signal()
{
	if (active_scene && (active_scene.nosig == 'no')) return;

	if (!no_signal_path) no_signal_path = new evg.Text();

	let text = no_signal_path;
	let h = active_scene ? active_scene.mod.height : video_height;
	let w = active_scene ? active_scene.mod.width : video_width;
	text.fontsize = h / 20;
  text.font = ['SANS'];
  text.align = GF_TEXT_ALIGN_LEFT;
	text.baseline = GF_TEXT_BASELINE_TOP;

	text.maxWidth = w;
	let d = new Date();
	if (no_signal_text==null) {
		//let version = 'GPAC '+sys.version + ' API '+sys.version_major+'.'+sys.version_minor+'.'+sys.version_micro;
		let version = 'GPAC ' + (sys.test_mode ? 'Test Mode' : sys.version_full);
		no_signal_text = ['No input', '', version, '(c) 2000-2022 Telecom Paris'];
	}
	let s1 = null;
	if (active_scene && active_scene.mod.pids.length) {
		if (sys.test_mode) 
			s1 = 'Signal lost';
		else if (active_scene.mod.pids[0].pid.source.no_signal_state)
			s1 = 'No signal (next scheduled in ' + Math.floor(active_scene.mod.pids[0].pid.source.no_signal_state) + ' s)';
		else
			s1 = 'Signal lost (' + Math.floor(active_scene.mod.pids[0].pid.source.no_signal/1000) + ' s)';
	} else if (active_scene && (active_scene.no_signal_time>0)) {
		let next = new Date(active_scene.no_signal_time);
		s1 = 'No input (next schedule ' + next.toUTCString() + ' s)';
	} else {
		s1 = 'No input';
	}
	let s2 = sys.test_mode ? 'Date' : d.toUTCString();
	if ((s1 !== no_signal_text[0]) || (s2 !== no_signal_text[1])) {
		no_signal_text[0] = s1;
		no_signal_text[1] = s2;
	  text.set_text(no_signal_text);
	  no_signal_outline = null;
	}

	if (active_scene)
		no_signal_transform.copy(active_scene.mx);
	else
		no_signal_transform.identity = true;

  no_signal_transform.translate(-w/2, 0);
  //comment if align to left
  let m = text.measure();
  no_signal_transform.translate(m.width/2, 0);

  if (!no_signal_outline) {
  	no_signal_outline = text.get_path().outline( { width: 6, align: GF_PATH_LINE_CENTER, cap: GF_LINE_CAP_ROUND, join: GF_LINE_JOIN_ROUND } );
  }
  no_signal_brush.set_color('black');

  let scene = active_scene;
  active_scene = null;
	canvas_draw(no_signal_outline, no_signal_brush, true);

/*  no_signal_brush.set_color('black');
	canvas_draw(text, mx, no_signal_brush, true);
  mx.translate(-2, 2);
*/
  no_signal_brush.set_color('white');
	canvas_draw(text, no_signal_brush, true);

  active_scene = scene;
}


function push_texture_uniforms(tx, prog_tx_uni)
{
	let mx2d = tx.mx || null;
	const txmx = mx2d ? new evg.Matrix(mx2d) : new evg.Matrix();
	webgl.uniformMatrix4fv(prog_tx_uni.matrix, false, txmx.m);

	webgl.uniform1f(prog_tx_uni.alpha, tx.get_alphaf() );

	let cmx_val = tx.cmx;
	if (cmx_val.identity) {
		  webgl.uniform1i(prog_tx_uni.cmx_use, 0);
	} else {
		let cmx_mul = [];
		cmx_mul.length = 16;

		cmx_mul[0] = cmx_val.rr; cmx_mul[1] = cmx_val.rg; cmx_mul[2] = cmx_val.rb; cmx_mul[3] = cmx_val.ra;
		cmx_mul[4] = cmx_val.gr; cmx_mul[5] = cmx_val.gg; cmx_mul[6] = cmx_val.gb; cmx_mul[7] = cmx_val.ga;
		cmx_mul[8] = cmx_val.br; cmx_mul[9] = cmx_val.bg; cmx_mul[10] = cmx_val.bb; cmx_mul[11] = cmx_val.ba;
		cmx_mul[12] = cmx_val.ar; cmx_mul[13] = cmx_val.ag; cmx_mul[14] = cmx_val.ab; cmx_mul[15] = cmx_val.aa;

		webgl.uniformMatrix4fv(prog_tx_uni.cmx_mul, false, cmx_mul);
		webgl.uniform4f(prog_tx_uni.cmx_add, cmx_val.tr, cmx_val.tg, cmx_val.tb, cmx_val.ta);
		webgl.uniform1i(prog_tx_uni.cmx_use, 1);
	}

	if (prog_tx_uni.r_s) {
	  webgl.uniform1i(prog_tx_uni.r_s, tx.repeat_s);
	  webgl.uniform1i(prog_tx_uni.r_t, tx.repeat_t);


		let color = tx.get_pad_color();
		let a = sys.color_component(color, 0);
		let r = sys.color_component(color, 1);
		let g = sys.color_component(color, 2);
		let b = sys.color_component(color, 3);
		webgl.uniform4f(prog_tx_uni.pad_color, r, g, b, a);
	}
}

function canvas_set_matrix(cnv, mx, prog_info)
{
	if (!mx.is3D) {
		if (cnv) {
			cnv.matrix = mx;
		} else {
			const modelViewMatrix = new evg.Matrix(mx);
			webgl.uniformMatrix4fv(prog_info.modelViewMatrix, false, modelViewMatrix.m);
		}
		return;
	}
  let fieldOfView = active_camera ? (active_camera.fov*Math.PI / 180) : (Math.PI / 4);
  let aspect =  (active_camera && active_camera.ar) ? active_camera.ar : (video_width / video_height);
  let zNear = (active_camera && active_camera.znear) ? active_camera.znear : 0.1;
  let zFar = (active_camera && active_camera.zfar) ? active_camera.zfar : (10 * (video_width>video_height ? video_width : video_height));

  //pick view distance so that with no transformation, a rectangular scene with size 100%x100% is exactly fullscreen 
	let final_z = video_height/2 / Math.tan(fieldOfView/2);
	let pos = {x:0, y:0, z: final_z};
	let center = {x:0, y:0, z: 0};
	let up = {x:0, y:1, z: 0};

  if (active_camera) {
		if (active_camera.position && (active_camera.position.length==3)) {
			pos.x = active_camera.position[0];
			pos.y = active_camera.position[1];
			pos.z = active_camera.position[2];
		}
		if (active_camera.target && (active_camera.target.length==3)) {
			center.x = active_camera.target[0];
			center.y = active_camera.target[1];
			center.z = active_camera.target[2];
		}
		if (active_camera.up && (active_camera.up.length==3)) {
			up.x = active_camera.up[0];
			up.y = active_camera.up[1];
			up.z = active_camera.up[2];
		}
	}

  let projectionMatrix = new evg.Matrix().perspective(fieldOfView, aspect, zNear, zFar);
  let modelViewMatrix = new evg.Matrix();

  modelViewMatrix.lookat(pos, center, up);

  modelViewMatrix.add(mx);

  if (cnv) {
	  projectionMatrix.add(modelViewMatrix, true);
	  cnv.matrix3d = projectionMatrix;
  }
  //webgl
  else {
		webgl.uniformMatrix4fv(prog_info.projectionMatrix, false, projectionMatrix.m);
	  webgl.uniformMatrix4fv(prog_info.modelViewMatrix, false, modelViewMatrix.m);
  }

  if (active_camera && active_camera.viewport && (active_camera.viewport.length==4)) {
		let vp_x = active_camera.viewport[0];
		let vp_y = active_camera.viewport[1];
		let vp_w = active_camera.viewport[2];
		let vp_h = active_camera.viewport[3];
		if (active_camera.units == UNIT_RELATIVE) {
			if (typeof vp_x == 'number') vp_x = vp_x * video_width / 100;
			if (typeof vp_y == 'number') vp_y = vp_y * video_height / 100;
			if (typeof vp_w == 'number') vp_w = vp_w * video_width / 100;
			if (typeof vp_h == 'number') vp_h = vp_h * video_height / 100;
		}

		if (vp_x == 'y') vp_x = vp_y;
		else if (vp_x == '-y') vp_x = -vp_y;
		if (vp_y == 'x') vp_y = vp_x;
		else if (vp_y == '-x') vp_y = -vp_x;
		if (vp_w == 'height') vp_w = vp_h;
		if (vp_h == 'width') vp_h = vp_w;

		vp_x = video_width/2 + vp_x - vp_w/2;
		vp_y = video_height/2 + vp_y - vp_h/2;

	  if (cnv) {
			vp_y = video_height - vp_y - vp_h;

			if (round_scene_size) {
				if (vp_x % 2) { vp_x--; vp_w++; };
				if (vp_w % 2) vp_w--;
				if (round_scene_size==2) {
					if (vp_y % 2) { vp_y--; vp_h++; };
					if (vp_h % 2) vp_h--;
				}
			}
			cnv.viewport(vp_x, vp_y, vp_w, vp_h);
	  } else {
		  webgl.viewport(vp_x, vp_y, vp_w, vp_h);
	  }
  }

}

function canvas_draw(path, stencil)
{
	let uniform_reload = false;
	//regular call
	if (arguments.length==2) {
		if (!path)
			return;
		if (!stencil) {
			//this only happens if no more input pids for scene
			draw_scene_no_signal();
			return;
		}
		if (active_scene.mod.pids.length && active_scene.mod.pids[0].pid && active_scene.mod.pids[0].pid.source.no_signal) {
			draw_scene_no_signal();
			return;		
		}
		uniform_reload = active_scene.gl_uniforms_reload;
	} else {
		//group or no signal, always reload uniforms
		uniform_reload = true;
	}

	let matrix = active_scene ? active_scene.mx : no_signal_transform;

	if (!webgl) {
	  canvas.path = path;
	  canvas_set_matrix(canvas, matrix);
	  canvas.fill(stencil);
	  return;
	}

	let is_texture = stencil.width || 0;
	let is_text = path.fontsize || 0;
	let use_soft_raster = false;
	if (!is_texture) {
		if (stencil.solid_brush) {
			use_soft_raster = (filter.gpu==2) ? false : true;
		}
		//we don't support gradients on GPU for now
		else use_soft_raster = true;
	}
	//we only draw text through software rasterizer
	else if (is_text) use_soft_raster = true;

	if (mask_mode) {
		use_soft_raster = false;
	}

	if (use_soft_raster) {
		canvas_offscreen_activate();
		if (canvas_offscreen.mask_mode != mask_mode) {
			canvas_offscreen.mask_mode = mask_mode;
		}
	  canvas_offscreen.path = path;
	  canvas_set_matrix(canvas_offscreen, matrix);
	  canvas_offscreen.fill(stencil);
		return;
	}

	canvas_offscreen_deactivate();

	let mesh = path.mesh || null;
	if (!mesh) {
		mesh = path.mesh = new evg.Mesh(path);
		if (!path.mesh) return;
		path.mesh.update_gl();
	}

	if (is_texture) {
		let texture = stencil._gl_texture || null;
		if (!texture) {
			stencil._gl_texture = webgl.createTexture(null);
			if (!stencil._gl_texture) return;
			stencil._gl_tx_id = webgl.textureName(stencil._gl_texture);

			if (stencil._gl_program) {
				stencil._gl_program = null;
				print(GF_LOG_INFO, 'texture format change, reloading GLSL shader');
			}
			stencil._gl_modified = true;
		}

		if (stencil._gl_modified == true) {
			webgl.bindTexture(webgl.TEXTURE_2D, stencil._gl_texture);
	    stencil._gl_texture.upload(stencil);
			stencil._gl_modified = false;
		}
	}

	let prog_info = stencil._gl_program || null;
  if (!stencil._gl_program) {
		if (is_texture) {
			let color_mx = stencil.cmx;
			if (color_mx.identity) color_mx = null;

			let fs_src = fs_source;

			prog_info = stencil._gl_program = setup_webgl_program(vs_source, fs_src, stencil._gl_tx_id);
			if (!stencil._gl_program) {
				filter.abort();
				return;
			}
		} else {
			prog_info = stencil._gl_program = setup_webgl_program(vs_const_col, fs_const_col, null);
			if (!stencil._gl_program) {
				filter.abort();
				return;
			}
		}
		webgl.useProgram(prog_info.program);
		webgl.uniform1f(prog_info.mask_w, video_width);
		webgl.uniform1f(prog_info.mask_h, video_height);
	}

	let use_mask = 0;
	if (mask_mode==GF_EVGMASK_DRAW) {
	  mask_canvas.path = path;
	  canvas_set_matrix(mask_canvas, matrix);
	  mask_canvas.fill(stencil);
		return;
	}
	else if ((mask_mode==GF_EVGMASK_USE) || (mask_mode==GF_EVGMASK_RECORD)) {
	  use_mask = 1;
	} else if (mask_mode==GF_EVGMASK_USE_INV) {
	  use_mask = 2;
	}

  webgl.useProgram(prog_info.program);

  webgl.viewport(0, 0, video_width, video_height);

  if (wgl_clipper) {
		webgl.enable(webgl.SCISSOR_TEST);

		webgl.scissor(video_width/2 + wgl_clipper.x, wgl_clipper.y - wgl_clipper.h + video_height/2, wgl_clipper.w, wgl_clipper.h);
  } else {
		webgl.disable(webgl.SCISSOR_TEST);
  }

  //set transform
	canvas_set_matrix(null, matrix, prog_info);

  //set mask
  webgl.uniform1i(prog_info.mask_mode, use_mask);

	if (use_mask) {
		let tx_slot = is_texture ? stencil._gl_texture.nb_textures : 0;

	  webgl.activeTexture(webgl.TEXTURE0 + tx_slot);
		webgl.bindTexture(webgl.TEXTURE_2D, mask_texture);
		//not a named texture
		webgl.texSubImage2D(webgl.TEXTURE_2D, 0, 0, 0, video_width, video_height,webgl.LUMINANCE, webgl.UNSIGNED_BYTE, mask_canvas_data);
		webgl.uniform1i(prog_info.mask_tx, tx_slot);
	}

  if (is_texture) {
		if (uniform_reload) {
			push_texture_uniforms(stencil, prog_info.textures[0]);
		}

	  //set video texture
	  webgl.activeTexture(webgl.TEXTURE0);
	  webgl.bindTexture(webgl.TEXTURE_2D, stencil._gl_texture);
		webgl.uniform1i(prog_info.textures[0].sampler, 0);

		mesh.draw(webgl, prog_info.attribLocations.vertexPosition, prog_info.attribLocations.textureCoord);
  } else {
		if (uniform_reload) {
			let color = stencil.get_color();
			let alpha = stencil.get_alphaf();
			let a = sys.color_component(color, 0) * alpha;
			let r = sys.color_component(color, 1);
			let g = sys.color_component(color, 2);
			let b = sys.color_component(color, 3);
			webgl.uniform4f(prog_info.color, r, g, b, a);
		}

		mesh.draw(webgl, prog_info.attribLocations.vertexPosition);
  }
  webgl.useProgram(null);
	webgl.disable(webgl.SCISSOR_TEST);

	//record mode, also draw using black stencil on mask canvas
	if (mask_mode==GF_EVGMASK_RECORD) {
	  mask_canvas.path = path;
	  canvas_set_matrix(mask_canvas, matrix);
	  mask_canvas.fill(mask_gl_stencil);
	}
}

const transition_vs_source = `
attribute vec4 aVertexPosition;
attribute vec2 aTextureCoord;
uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
varying vec2 txcoord_from;
varying vec2 txcoord_to;
uniform mat4 textureMatrixFrom;
uniform mat4 textureMatrixTo;
void main() {
  gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
	txcoord_from = vec2(textureMatrixFrom * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
	txcoord_to = vec2(textureMatrixTo * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
}
`;

const single_in_op_vs_source = `
attribute vec4 aVertexPosition;
attribute vec2 aTextureCoord;
uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
varying vec2 txcoord_from;
varying vec2 txcoord_op;
uniform mat4 textureMatrixFrom;
uniform mat4 textureMatrixOp;
void main() {
  gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
	txcoord_from = vec2(textureMatrixFrom * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
	txcoord_op = vec2(textureMatrixOp * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
}
`;


const dual_in_op_vs_source = `
attribute vec4 aVertexPosition;
attribute vec2 aTextureCoord;
uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
varying vec2 txcoord_from;
varying vec2 txcoord_to;
varying vec2 txcoord_op;
uniform mat4 textureMatrixFrom;
uniform mat4 textureMatrixTo;
uniform mat4 textureMatrixOp;
void main() {
  gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
	txcoord_from = vec2(textureMatrixFrom * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
	txcoord_to = vec2(textureMatrixTo * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
	txcoord_op = vec2(textureMatrixOp * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
}
`;

const fs_trans_source_prefix = `
uniform sampler2D mask_tx;
uniform int mask_mode;
uniform float mask_w, mask_h;

uniform vec4 pad_color;
uniform bool r_s, r_t;
uniform float ratio;
uniform float video_ar;

varying vec2 txcoord_from;
uniform sampler2D maintx1;
uniform float _alpha_from;
uniform bool cmx1_use;
uniform mat4 cmx1_mul;
uniform vec4 cmx1_add;

varying vec2 txcoord_to;
uniform sampler2D maintx2;
uniform float _alpha_to;
uniform bool cmx2_use;
uniform mat4 cmx2_mul;
uniform vec4 cmx2_add;

vec4 get_pixel_from(vec2 tx_coord)
{
	vec4 col;
	if (!r_s && ((tx_coord.s<0.0) || (tx_coord.s>1.0))) {
		col = pad_color;
	} else if (!r_t && ((tx_coord.t<0.0) || (tx_coord.t>1.0))) {
		col = pad_color;
	} else {
		vec2 tx = tx_coord;
		while (tx.s<0.0) tx.s+= 1.0;
		tx.s = mod(tx.s, 1.0);
		while (tx.t<0.0) tx.t+= 1.0;
		tx.t = mod(tx.t, 1.0);

		col = texture2D(maintx1, tx);
	}
	if (cmx1_use) {
		col = cmx1_mul * col;
		col += cmx1_add;
		col = clamp(col, 0.0, 1.0);
	}
	col.a *= _alpha_from;
	return col;
}

vec4 get_pixel_to(vec2 tx_coord)
{
	vec4 col;
	if (!r_s && ((tx_coord.s<0.0) || (tx_coord.s>1.0))) {
		col = pad_color;
	} else if (!r_t && ((tx_coord.t<0.0) || (tx_coord.t>1.0))) {
		col = pad_color;
	} else {
		vec2 tx = tx_coord;
		while (tx.s<0.0) tx.s+= 1.0;
		tx.s = mod(tx.s, 1.0);
		while (tx.t<0.0) tx.t+= 1.0;
		tx.t = mod(tx.t, 1.0);

		col = texture2D(maintx2, tx);
	}
	if (cmx2_use) {
		col = cmx2_mul * col;
		col += cmx2_add;
		col = clamp(col, 0.0, 1.0);
	}
	col.a *= _alpha_to;
	return col;
}

`;

const fs_single_in_op_source_prefix = `
uniform sampler2D mask_tx;
uniform int mask_mode;
uniform float mask_w, mask_h;

varying vec2 txcoord_from;
uniform sampler2D maintx1;
uniform float _alpha_from;
uniform bool cmx1_use;
uniform mat4 cmx1_mul;
uniform vec4 cmx1_add;

uniform vec4 pad_color;
uniform bool r_s, r_t;
uniform float ratio;

vec4 get_pixel_from(vec2 tx_coord)
{
	vec4 col;
	if (!r_s && ((tx_coord.s<0.0) || (tx_coord.s>1.0))) {
		col = pad_color;
	} else if (!r_t && ((tx_coord.t<0.0) || (tx_coord.t>1.0))) {
		col = pad_color;
	} else {
		vec2 tx = tx_coord;
		while (tx.s<0.0) tx.s+= 1.0;
		tx.s = mod(tx.s, 1.0);
		while (tx.t<0.0) tx.t+= 1.0;
		tx.t = mod(tx.t, 1.0);

		col = texture2D(maintx1, tx);
	}
	if (cmx1_use) {
		col = cmx1_mul * col;
		col += cmx1_add;
		col = clamp(col, 0.0, 1.0);
	}
	col.a *= _alpha_from;
	return col;
}

`;


function canvas_draw_sources(path)
{
	let scene = active_scene;
	let pids = scene.mod.pids;
	let op_type = 0;
	let op_param = 0;
	let op_tx = null;
	if (arguments.length == 4) {
		op_type = arguments[1];
		op_param = arguments[2];
		op_tx = arguments[3];
	}


	if (pids.length==0) {
		print(GF_LOG_ERROR, 'Broken scene ' + scene.id + ': call to draw_sources without any source pid !');
		scene.active=false;
		return;
	}

	if (pids[0].pid.source.no_signal) {
		draw_scene_no_signal();
		return;
	}
	if ((pids.length==1) && !op_type) {
		let tx = pids[0].texture;
		//should not happen
		if (!tx) return;

		canvas_draw(path, tx);
	  return;  
	}

	if (op_type) {
	  if ((op_type == GF_EVG_OPERAND_MIX_DYN) || (op_type == GF_EVG_OPERAND_MIX_DYN_ALPHA)) {
			if (pids.length<=1) {
				print(GF_LOG_WARNING, 'Canvas multitexture fill with 3 textures but only 2 textures provided');
				canvas_draw(path, pids[0].texture);
				return;
			}
		}

		if (!use_gpu) {
			let mx = op_tx.mx;

			canvas_set_matrix(canvas, active_scene.mx);
		  canvas.path = path;
		  if ((op_type == GF_EVG_OPERAND_MIX_DYN) || (op_type == GF_EVG_OPERAND_MIX_DYN_ALPHA)) {
			  canvas.fill(op_type, [op_param], pids[0].texture, pids[1].texture, op_tx);
		  } else {
			  canvas.fill(op_type, [op_param], pids[0].texture, op_tx);
		  }
			return;
		}
	}

	if (pids[0].pid && pids[0].pid.source.sequence.transition_state==4) {
		print(GF_LOG_DEBUG, 'transition in error, using simple draw');
		canvas_draw(path, pids[0].texture);
		return;
	}

	if (!op_type && pids[1].pid && pids[1].pid.source.no_signal) {
		draw_scene_no_signal();
		return;
	}

	let transition = null;
	let seq = null;
	let ratio = 0;

	if (pids[0].pid.source.sequence.transition_state==3) {
		seq = pids[0].pid.source.sequence;
		transition = seq.transition;

		let time = video_time - seq.transition_start;
		//in sec
		time /= video_timescale;

		//as fraction
		ratio = time / seq.transition_dur;
		if (ratio>1) ratio=1;
	} else if (scene.transition) {
		let skip_tansition = !use_gpu;
		transition = scene.transition;
		ratio = scene.mod.mix_ratio;
		if (ratio<0) ratio = 0;
		else if (ratio>1) ratio = 1;

		if (pids[0].pid) pids[0].pid.source.mix_volume = 1-ratio;
		if (pids[1].pid) pids[1].pid.source.mix_volume = ratio;

		if (scene.transition_state==4) {
			if (ratio>0.5) ratio = 1;
			else ratio = 0;
			skip_tansition = true;
		}

		//for gpu we must setup the program and use it
		if (skip_tansition) {
			if (ratio==0) {
				canvas_draw(path, pids[0].texture);
				return;
			}
			if (ratio==1) {
				canvas_draw(path, pids[1].texture);
				return;
			}
		}
	}

	if (!op_type && !transition) {
		canvas_draw(path, pids[0].texture);
		return;
	}

	if (transition && transition.fun) {
		let old_ratio = ratio;
		try {
			ratio = transition.fun.apply(transition, [ratio]);
			if (typeof ratio != 'number')
				throw 'Ratio function result is not a number !';
		} catch (e) {
			print(GF_LOG_WARNING, 'Error processing transition fun ' + transition.fun + ': ' + e);
			transition.fun = null;
		}
		if (!old_ratio) ratio = 0;
		else if (!old_ratio==1) ratio = 1;
	}

	if (!use_gpu) {
		if (!pids[0].texture || !pids[1].texture) {
			canvas_draw(path, pids[0].texture);
			return;
		}
		canvas_set_matrix(canvas, active_scene.mx);

		transition.mod.apply(canvas, ratio, path, pids);
		return;
	}

	//gpu: create mesh, bind textures, call apply and draw mesh

	let use_mask = 0;
	if ((mask_mode==GF_EVGMASK_DRAW) || (mask_mode==GF_EVGMASK_RECORD)) {
	  mask_canvas.path = path;
	  canvas_set_matrix(mask_canvas, matrix);
	  mask_canvas.fill(pids[0].texture);
	  if (mask_mode==GF_EVGMASK_DRAW) return;
	  use_mask = 1;
	}
	else if (mask_mode==GF_EVGMASK_USE) {
	  use_mask = 1;
	} else if (mask_mode==GF_EVGMASK_USE_INV) {
	  use_mask = 2;
	}

	canvas_offscreen_deactivate();

	let mesh = path.mesh || null;
	if (!mesh) {
		mesh = path.mesh = new evg.Mesh(path);
		if (!path.mesh) return;
		path.mesh.update_gl();
	}

	//setup all textures
	let tx_slot = 0;
	let all_textures = [];
	pids.forEach (pid => {
			all_textures.push(pid.texture);
	});
	if (op_tx)
		all_textures.push(op_tx);

	all_textures.forEach (tx => {
		let texture = tx._gl_texture || null;

		webgl.activeTexture(webgl.TEXTURE0 + tx_slot);

		if (!texture) {
			tx._gl_texture = webgl.createTexture(null);
			if (!tx._gl_texture) return;
			tx._gl_tx_id = webgl.textureName(tx._gl_texture);

			if (transition && transition.gl_program) {
				print(GF_LOG_INFO, 'texture format changed, reloading GLSL program');
				transition.gl_program = null;
			}
			if (active_scene && active_scene.gl_program) {
				print(GF_LOG_INFO, 'texture format changed, reloading GLSL program');
				active_scene.gl_program = null;
			}
			tx._gl_modified = true;
		}
		//upload data if modified
		if (tx._gl_modified == true) {
			webgl.bindTexture(webgl.TEXTURE_2D, tx._gl_texture);
			tx._gl_texture.upload(tx);
			tx._gl_modified = false;
		}

		tx_slot += tx._gl_texture.nb_textures;
	});


	//check program is still OK
	let prog_info;
	if (transition) {
		prog_info = transition.gl_program || null;
	} else {
		prog_info = active_scene.gl_program || null;
	}

	//create program
	if (!prog_info) {
		let frag_source;
		let has_dual_in = false;
		let op_tx_reuse = 0;
		let tx_source = '';
		let vertex_src;

		if (transition) {
			tx_source = transition.mod.get_shader_src( pids[0].texture );
			if (typeof tx_source != 'string') {
				print(GF_LOG_ERROR, 'Aborting transition, invalid shader source ' + tx_source);
				if (seq)
					seq.transition_state = 4;
				else
					scene.transition_state = 4;
				return;
			}

			let first_uni = tx_source.indexOf('uniform');
			if (first_uni>0) {
				let head = tx_source.slice(0, first_uni);
				let tail = tx_source.slice(first_uni);
				frag_source = head + fs_trans_source_prefix + tail;
			} else {
				frag_source = fs_trans_source_prefix + tx_source;
			}
			has_dual_in = true;

			vertex_src = transition_vs_source;
		} else {
			  if ((op_type == GF_EVG_OPERAND_MIX_DYN) || (op_type == GF_EVG_OPERAND_MIX_DYN_ALPHA)) {
					has_dual_in = true;
					frag_source = fs_trans_source_prefix;
					vertex_src = dual_in_op_vs_source;
				} else {
					frag_source = fs_single_in_op_source_prefix;
					vertex_src = single_in_op_vs_source;
				}
		}

		//replace maintx1 by first texture name
		frag_source = frag_source.replaceAll('maintx1', pids[0].texture._gl_tx_id);

		//replace maintx2 by second texture name
		if (has_dual_in)
			frag_source = frag_source.replaceAll('maintx2', pids[1].texture._gl_tx_id);


		//replace op
		if (!transition) {
			if (op_tx._gl_tx_id == pids[0].texture._gl_tx_id) {
				op_tx_reuse = 1;
				frag_source += `
vec4 gf_apply_effect()
{
	vec4 col_1 = get_pixel_from(txcoord_from);
	vec4 col_op = col_1;
`;

			} else if (has_dual_in && (op_tx._gl_tx_id == pids[1].texture._gl_tx_id)) {
				op_tx_reuse = 2;
				frag_source += `
vec4 gf_apply_effect()
{
	vec4 col_1 = get_pixel_from(txcoord_from);
`;

			} else {
				frag_source += `
varying vec2 txcoord_op;
uniform sampler2D optx;
uniform float _alpha_op;
uniform bool r_s_op, r_t_op;
uniform vec4 pad_color_op;

uniform bool cmx_op_use;
uniform mat4 cmx_op_mul;
uniform vec4 cmx_op_add;

vec4 get_pixel_op(vec2 tx_coord)
{
	vec4 col;
	if (!r_s_op && ((tx_coord.s<0.0) || (tx_coord.s>1.0))) {
		col = pad_color_op;
	} else if (!r_t_op && ((tx_coord.t<0.0) || (tx_coord.t>1.0))) {
		col = pad_color_op;
	} else {
		vec2 tx = tx_coord;
		while (tx.s<0.0) tx.s+= 1.0;
		tx.s = mod(tx.s, 1.0);
		while (tx.t<0.0) tx.t+= 1.0;
		tx.t = mod(tx.t, 1.0);

		col = texture2D(optx, tx);
	}
	if (cmx_op_use) {
		col = cmx_op_mul * col;
		col += cmx_op_add;
		col = clamp(col, 0.0, 1.0);
	}
	col.a *= _alpha_op;
	return col;
}

vec4 gf_apply_effect()
{
	vec4 col_1 = get_pixel_from(txcoord_from);
	vec4 col_op = get_pixel_op(txcoord_op);
`;
				frag_source = frag_source.replaceAll('optx', op_tx._gl_tx_id);
			}

			if (has_dual_in) {
				frag_source += `	vec4 col_2 = get_pixel_to(txcoord_to);
`;
				if (op_tx_reuse == 2) {
					frag_source += `	vec4 col_op = col_2;
`;
				}
			}

			if ((op_type == GF_EVG_OPERAND_REPLACE_ALPHA) || (op_type == GF_EVG_OPERAND_REPLACE_ONE_MINUS_ALPHA)
				|| (op_type == GF_EVG_OPERAND_MIX_DYN) || (op_type == GF_EVG_OPERAND_MIX_DYN_ALPHA)
			) {
				let rep = 'a';
				if (op_param >= 3) rep = 'b';
				else if (op_param >= 2) rep = 'g';
				else if (op_param >= 1) rep = 'r';

				if (op_type == GF_EVG_OPERAND_REPLACE_ALPHA)
					frag_source += `	col_1.a = col_op.`+rep+`;`;
				else if (op_type == GF_EVG_OPERAND_REPLACE_ONE_MINUS_ALPHA)
					frag_source += `	col_1.a = 1.0 - col_op.`+rep+`;`;
				else if (op_type == GF_EVG_OPERAND_MIX_DYN)
					frag_source += `	col_1 = mix(col_1, col_2, col_op.`+rep+`); col_1.a = 1.0;`;
				else //(op_type == GF_EVG_OPERAND_MIX_DYN)
					frag_source += `	col_1 = mix(col_1, col_2, col_op.`+rep+`);`;
			} else if (op_type == GF_EVG_OPERAND_MIX) {
				frag_source += `	col_1 = mix(col_1, col_op, ratio); col_1.a = 1.0;`;
			} else if (op_type == GF_EVG_OPERAND_MIX_ALPHA) {
				frag_source += `	col_1 = mix(col_1, col_op, ratio);`;
			}

			frag_source += `
	return col_1;
}
`;
		}

		frag_source += `
void main() {
	if (mask_mode>0) {
		vec2 mask_uv = vec2(gl_FragCoord.x/mask_w, 1.0 - gl_FragCoord.t/mask_h);
		vec4 mask = texture2D(mask_tx, mask_uv);
		vec4 col = gf_apply_effect();

		if (mask_mode>1)
			col.a *= (1.0-mask.r);
		else
			col.a *= mask.r;

	  gl_FragColor = col;
	} else {
		gl_FragColor = gf_apply_effect();
	}
}
`

		const shaderProgram = webgl_init_shaders(vertex_src, frag_source, null);
		if (!shaderProgram) {
			print(GF_LOG_ERROR, 'shader creation failed, aborting transition');
			if (seq)
				seq.transition_state = 4;
			else
				scene.transition_state = 4;
			return;
		}

		prog_info = {
			program: shaderProgram,
			vertexPosition: webgl.getAttribLocation(shaderProgram, 'aVertexPosition'),
			textureCoord: webgl.getAttribLocation(shaderProgram, 'aTextureCoord'),
			projectionMatrix: webgl.getUniformLocation(shaderProgram, 'uProjectionMatrix'),
			modelViewMatrix: webgl.getUniformLocation(shaderProgram, 'uModelViewMatrix'),
			mask_mode: webgl.getUniformLocation(shaderProgram, 'mask_mode'),
			mask_tx: webgl.getUniformLocation(shaderProgram, 'mask_tx'),
			mask_w: webgl.getUniformLocation(shaderProgram, 'mask_w'),
			mask_h: webgl.getUniformLocation(shaderProgram, 'mask_h'),
			ratio: webgl.getUniformLocation(shaderProgram, 'ratio'),
			//used by gl-transitions
			video_ar: webgl.getUniformLocation(shaderProgram, 'video_ar'),
			textures: [
				{
					sampler: webgl.getUniformLocation(shaderProgram, pids[0].texture._gl_tx_id),
			    matrix: webgl.getUniformLocation(shaderProgram, 'textureMatrixFrom'),
			    alpha: webgl.getUniformLocation(shaderProgram, '_alpha_from'),
					cmx_use: webgl.getUniformLocation(shaderProgram, 'cmx1_use'),
					cmx_mul: webgl.getUniformLocation(shaderProgram, 'cmx1_mul'),
					cmx_add: webgl.getUniformLocation(shaderProgram, 'cmx1_add'),
					r_s: webgl.getUniformLocation(shaderProgram, 'r_s'),
					r_t: webgl.getUniformLocation(shaderProgram, 'r_t'),
					pad_color: webgl.getUniformLocation(shaderProgram, 'pad_color'),
				}
				]
		};

		if (has_dual_in) {
			prog_info.textures.push(
				{
					sampler: webgl.getUniformLocation(shaderProgram, pids[1].texture._gl_tx_id),
			    matrix: webgl.getUniformLocation(shaderProgram, 'textureMatrixTo'),
			    alpha: webgl.getUniformLocation(shaderProgram, '_alpha_to'),
					cmx_use: webgl.getUniformLocation(shaderProgram, 'cmx2_use'),
					cmx_mul: webgl.getUniformLocation(shaderProgram, 'cmx2_mul'),
					cmx_add: webgl.getUniformLocation(shaderProgram, 'cmx2_add'),
					r_s: null,
					r_t: null,
					pad_color: null,
				} );
		}

		prog_info.op_texture = null;
		if (!transition && op_tx) {
				if (op_tx_reuse) {
					prog_info.op_texture = null;
				} else {
					prog_info.op_texture = {
						sampler: webgl.getUniformLocation(shaderProgram, op_tx._gl_tx_id),
				    matrix: webgl.getUniformLocation(shaderProgram, 'textureMatrixOp'),
				    alpha: webgl.getUniformLocation(shaderProgram, '_alpha_op'),
						cmx_use: webgl.getUniformLocation(shaderProgram, 'cmx_op_use'),
						cmx_mul: webgl.getUniformLocation(shaderProgram, 'cmx_op_mul'),
						cmx_add: webgl.getUniformLocation(shaderProgram, 'cmx_op_add'),
				    r_s: webgl.getUniformLocation(shaderProgram, 'r_s_op'),
				    r_t: webgl.getUniformLocation(shaderProgram, 'r_t_op'),
				    pad_color: webgl.getUniformLocation(shaderProgram, 'pad_color_op'),
					};
				}
		}


		if (transition)
			transition.gl_program = prog_info;
		else
			active_scene.gl_program = prog_info;

		webgl.useProgram(prog_info.program);
		webgl.uniformMatrix4fv(prog_info.projectionMatrix, false, defaultOrthoProjectionMatrix.m);
	  webgl.uniform1f(prog_info.mask_w, video_width);
	  webgl.uniform1f(prog_info.mask_h, video_height);

		if (transition) {
			let nb_in_tx = has_dual_in ? 2 : 1;
			tx_slot = 0;
			for (let i=0; i<nb_in_tx; i++) {
				let pid = pids[i];
				tx_slot += pid.texture._gl_texture.nb_textures;
			}
			transition.mod.setup_gl(webgl, shaderProgram, prog_info.ratio, tx_slot);
		}

		if (prog_info.video_ar)
			webgl.uniform1f(prog_info.video_ar, pids[0].texture.width / pids[0].texture.height);

		active_scene.gl_uniforms_reload = true;
	}

	webgl.useProgram(prog_info.program);
	webgl.viewport(0, 0, video_width, video_height);

	let nb_in_tx = (transition || (prog_info.textures.length==2)) ? 2 : 1;

  if (wgl_clipper) {
		webgl.enable(webgl.SCISSOR_TEST);
		webgl.scissor(video_width/2 + wgl_clipper.x, wgl_clipper.y - wgl_clipper.h + video_height/2, wgl_clipper.w, wgl_clipper.h);
  } else {
		webgl.disable(webgl.SCISSOR_TEST);
  }

	//set transform
	canvas_set_matrix(null, active_scene.mx, prog_info);

	if (!transition) ratio = active_scene.mod.mix_ratio;

	webgl.uniform1f(prog_info.ratio, ratio);

	tx_slot = 0;
	//set alpha and texture transforms per pid - 2 max
	for (let i=0; i<nb_in_tx; i++) {
		let pid = pids[i];

		if (active_scene.gl_uniforms_reload) {
			push_texture_uniforms(pid.texture, prog_info.textures[i]);
	  }

		webgl.activeTexture(webgl.TEXTURE0 + tx_slot);
		//and bind our named texture (this will setup active texture slots)
		webgl.bindTexture(webgl.TEXTURE_2D, pid.texture._gl_texture);
		webgl.uniform1i(prog_info.textures[i].sampler, tx_slot);

		tx_slot += pid.texture._gl_texture.nb_textures;
	}

	//set op texture
	if (op_tx && prog_info.op_texture) {
		if (active_scene.gl_uniforms_reload) {
			push_texture_uniforms(op_tx, prog_info.op_texture);
		}

		webgl.activeTexture(webgl.TEXTURE0 + tx_slot);
		//and bind our named texture (this will setup active texture slots)
		webgl.bindTexture(webgl.TEXTURE_2D, op_tx._gl_texture);
		webgl.uniform1i(prog_info.op_texture.sampler, tx_slot);

		tx_slot += op_tx._gl_texture.nb_textures;
	}

  //set mask
  webgl.uniform1i(prog_info.mask_mode, use_mask);
	if (use_mask) {
	  webgl.activeTexture(webgl.TEXTURE0 + tx_slot);
		webgl.bindTexture(webgl.TEXTURE_2D, mask_texture);
		//not a named texture
		webgl.texSubImage2D(webgl.TEXTURE_2D, 0, 0, 0, video_width, video_height,webgl.LUMINANCE, webgl.UNSIGNED_BYTE, mask_canvas_data);
		webgl.uniform1i(prog_info.mask_tx, tx_slot);
	}

	//apply transition, ie set uniforms
	if (transition) {
		transition.mod.apply(webgl, ratio, path, pids);
	}

	mesh.draw(webgl, prog_info.vertexPosition, prog_info.textureCoord);
	webgl.useProgram(null);
}


function canvas_blit(stencil, dst_rc)
{
	if (webgl) {
			print(GF_LOG_ERROR, 'Broken scene ' + active_scene.id + ': call to blit in GPU context');
			active_scene.active=false;
			return;
	}
  canvas.blit(stencil, dst_rc, null);
}



const vs_source = `
attribute vec4 aVertexPosition;
attribute vec2 aTextureCoord;
uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;
uniform mat4 textureMatrix;
varying vec2 vTextureCoord;

void main() {
  gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
	vTextureCoord = vec2(textureMatrix * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
}
`;

const fs_source = `
uniform sampler2D mask_tx;
uniform int mask_mode;
uniform float mask_w, mask_h;

uniform bool r_s, r_t;
uniform vec4 pad_color;
varying vec2 vTextureCoord;
uniform sampler2D maintx;
uniform float alpha;
uniform bool cmx_use;
uniform mat4 cmx_mul;
uniform vec4 cmx_add;
void main(void) {

	vec4 col;
	if (!r_s && ((vTextureCoord.s<0.0) || (vTextureCoord.s>1.0))) {
		col = pad_color;
	} else if (!r_t && ((vTextureCoord.t<0.0) || (vTextureCoord.t>1.0))) {
		col = pad_color;
	} else {
		vec2 tx = vTextureCoord;
		while (tx.s<0.0) tx.s+= 1.0;
		tx.s = mod(tx.s, 1.0);
		while (tx.t<0.0) tx.t+= 1.0;
		tx.t = mod(tx.t, 1.0);

		col = texture2D(maintx, tx);
	}
	if (cmx_use) {
		col = cmx_mul * col;
		col += cmx_add;
		col = clamp(col, 0.0, 1.0);
	}
	col.a *= alpha;

	if (mask_mode>0) {
		vec2 mask_uv = vec2(gl_FragCoord.x/mask_w, 1.0 - gl_FragCoord.t/mask_h);
		vec4 mask = texture2D(mask_tx, mask_uv);
		float m;
		if (mask_mode>1)
			col.a *= (1.0-mask.r);
		else
			col.a *= mask.r;
	}
  gl_FragColor = col;
}
`;


const vs_const_col = `
attribute vec4 aVertexPosition;
uniform mat4 uModelViewMatrix;
uniform mat4 uProjectionMatrix;

void main() {
  gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
}
`;


const fs_const_col = `
uniform sampler2D mask_tx;
uniform int mask_mode;
uniform float mask_w, mask_h;

uniform vec4 color;
void main(void) {

	if (mask_mode>0) {
		vec4 col = color;
		vec2 mask_uv = vec2(gl_FragCoord.x/mask_w, 1.0 - gl_FragCoord.t/mask_h);
		vec4 mask = texture2D(mask_tx, mask_uv);
		if (mask_mode>1)
			col.a *= (1.0-mask.r);
		else
			col.a *= mask.r;

	  gl_FragColor = col;
	} else {
	  gl_FragColor = color;
	}
}
`;


function webgl_load_shader(type, source, tx_name) {
  const shader = webgl.createShader(type);

  if (tx_name) {
  	source = source.replaceAll('maintx', tx_name);
  }
  webgl.shaderSource(shader, source);
  webgl.compileShader(shader);
  if (!webgl.getShaderParameter(shader, webgl.COMPILE_STATUS)) {
    print(GF_LOG_ERROR, 'An error occurred compiling the shaders ' + type + ' : ' + webgl.getShaderInfoLog(shader));
    print(GF_LOG_WARNING, 'Shader was \n' + source);
    webgl.deleteShader(shader);
    return null;
  }
  return shader;
}

function webgl_init_shaders(vsSource, fsSource, tx_name) {
  const vertexShader = webgl_load_shader(webgl.VERTEX_SHADER, vsSource, null);
  const fragmentShader = webgl_load_shader(webgl.FRAGMENT_SHADER, fsSource, tx_name);

  if (!vertexShader || !fragmentShader) return null;

  const shaderProgram = webgl.createProgram();
  webgl.attachShader(shaderProgram, vertexShader);
  webgl.attachShader(shaderProgram, fragmentShader);
  webgl.linkProgram(shaderProgram);

  if (!webgl.getProgramParameter(shaderProgram, webgl.LINK_STATUS)) {
    print(GF_LOG_ERROR, 'Unable to initialize the shader program: ' + webgl.getProgramInfoLog(shaderProgram));
    return null;
  }
  return shaderProgram;
}


function setup_webgl_program(vsSource, fsSource, tx_name)
{
  const shaderProgram = webgl_init_shaders(vsSource, fsSource, tx_name);
  let prog = {
    program: shaderProgram,
    attribLocations: {
      vertexPosition: webgl.getAttribLocation(shaderProgram, 'aVertexPosition'),
    },
    projectionMatrix: webgl.getUniformLocation(shaderProgram, 'uProjectionMatrix'),
    modelViewMatrix: webgl.getUniformLocation(shaderProgram, 'uModelViewMatrix'),
    mask_mode: webgl.getUniformLocation(shaderProgram, 'mask_mode'),
    mask_w: webgl.getUniformLocation(shaderProgram, 'mask_w'),
    mask_h: webgl.getUniformLocation(shaderProgram, 'mask_h'),
		mask_tx: webgl.getUniformLocation(shaderProgram, "mask_tx")
  };

  if (tx_name) {
  	prog.attribLocations.textureCoord = webgl.getAttribLocation(shaderProgram, 'aTextureCoord');
  	prog.textures = [
			{
				sampler: webgl.getUniformLocation(shaderProgram, tx_name),
				matrix: webgl.getUniformLocation(shaderProgram, 'textureMatrix'),
				alpha: webgl.getUniformLocation(shaderProgram, 'alpha'),
				r_s: webgl.getUniformLocation(shaderProgram, 'r_s'),
				r_t: webgl.getUniformLocation(shaderProgram, 'r_t'),
				pad_color: webgl.getUniformLocation(shaderProgram, 'pad_color'),
				cmx_use: webgl.getUniformLocation(shaderProgram, 'cmx_use'),
				cmx_mul: webgl.getUniformLocation(shaderProgram, 'cmx_mul'),
				cmx_add: webgl.getUniformLocation(shaderProgram, 'cmx_add'),
			}
		];
  } else {
    prog.color = webgl.getUniformLocation(shaderProgram, 'color');
  }

  //load default texture matrix (identity)
  webgl.useProgram(shaderProgram);
	let ident = new evg.Matrix();
	webgl.uniformMatrix4fv(prog.modelViewMatrix, false, ident.m);
  webgl.uniformMatrix4fv(prog.projectionMatrix, false, defaultOrthoProjectionMatrix.m);
  webgl.uniform1i(prog.mask_mode, 0);
  if (tx_name) {
	  webgl.uniformMatrix4fv(prog.textures[0].matrix, false, ident.m);
	  webgl.uniform1f(prog.textures[0].alpha, 1.0);
	  webgl.uniform1i(prog.textures[0].cmx_use, 0);
	}
  webgl.useProgram(null);

  return prog;
}


function apply_transition_options(seq, opts, strict)
{
	//parse all params
	opts.forEach(o => {
		seq.transition.mod[o.name] = o.value;
		let typ = typeof seq.transition_effect[o.name];
		if (typ != 'undefined') {
			if (typ == typeof o.value) {
				if (strict && Array.isArray(o.value) ) {
					if (o.value.length == seq.transition_effect[o.name].length) {
						seq.transition.mod[o.name] = seq.transition_effect[o.name];
					} else {
						print(GF_LOG_WARNING, 'Array length mismatch for transition ' + seq.transition.type + '.' + o.name + ': got ' + seq.transition_effect[o.name].length + ', expecting ' + o.value.length + ', ignoring');
					}
				} else {
					seq.transition.mod[o.name] = seq.transition_effect[o.name];
				}
			} else {
				print(GF_LOG_WARNING, 'Type mismatch for transition ' + seq.transition.type + '.' + o.name + ': got ' + typ + ', expecting ' + typeof o.value + ', ignoring');
			}
		}
	});
}

function setup_transition(seq)
{
	/*builtin parameters*/
	seq.transition.fun = (typeof seq.transition_effect.fun == 'string') ? fn_from_script(['ratio'], seq.transition_effect.fun) : null;
	seq.transition.id = (typeof seq.transition_effect.id == 'string') ? seq.transition_effect.id : null;

	seq.transition.mod.update_flag = 0;
	//parse all params
	apply_transition_options(seq, seq.transition.options, false);

	//parse all per-instance params
	if (Array.isArray(seq.transition.mod.options)) {
		apply_transition_options(seq, seq.transition.mod.options, true);
	}

	for (var propertyName in seq.transition_effect) {
		if (propertyName == 'type') continue;
		if (propertyName == 'dur') continue;
		if (propertyName == 'fun') continue;
		if (propertyName == 'id') continue;
		if (propertyName.charAt(0) == '_') continue;
		if (typeof seq.transition.mod[propertyName] != 'undefined') continue;

		print(GF_LOG_WARNING, 'Unrecognized transition option ' + propertyName + ' for ' + seq.transition.type);
	}
	seq.transition.gl_program = null;
	if (!use_gpu)
		seq.transition.mod.setup();
}

function default_transition(canvas, ratio, path, pids)
{
		canvas_draw(path, pids[0].texture);
}

function load_transition(seq)
{
	let type = seq.transition_effect.type || null;
	if (!type) return false;

	let script_src = sys.url_cat(playlist_url, type);
	if (! sys.file_exists(script_src)) {
		script_src = filter.jspath + 'transitions/' + type + '.js';

		if (! sys.file_exists(script_src)) {
			print(GF_LOG_ERROR, 'No such transition effect ' + script_src);
			return false;
		}
	}

	if (seq.transition && (seq.transition.type == type)) {
		setup_transition(seq);
		return true;
	}


	modules_pending ++;
	seq.transition = {};
	seq.transition.type = type;

	import(script_src)
		  .then(obj => {
		  		modules_pending--;
		  		print(GF_LOG_DEBUG, 'Module ' + script_src + ' loaded');
					seq.transition.mod = obj.load(seq.transition_effect);

		  		let has_gl = 0;

		  		//can do gl
		  		if (typeof seq.transition.mod.get_shader_src == 'function') {
		  			has_gl = 1;
			  		if (typeof seq.transition.mod.setup != 'function')
			  			has_gl = 2;
		  		}

		  		if (use_gpu) {
		  			if (!has_gl) {
		  				seq.transition_state = 4;
		  				print(GF_LOG_WARNING, 'Transition ' + type + ' cannot run in GPU mode, disabling');
		  				return;
		  			}
			  		if (typeof seq.transition.mod.setup_gl != 'function')
			  				seq.transition.mod.setup_gl = function () {};
		  		} else {
		  			if (has_gl==2) {
		  				seq.transition_state = 4;
		  				print(GF_LOG_WARNING, 'Transition ' + type + ' cannot run in non-GPU mode, disabling');
		  				return;
			  		} else if (typeof seq.transition.mod.setup != 'function') {
				  		print(GF_LOG_ERROR, "Missing setup function for transition '" + type + "', disabling transition");
				  		seq.transition_state = 4;
				  		return;
			  		}
		  		}

		  		if (typeof seq.transition.mod.apply != 'function')
		  				seq.transition.mod.apply = default_transition;

		  		seq.transition.options = obj.options || null;
		  		setup_transition(seq);
		  })
		  .catch(err => {
		  		modules_pending--;
		  		print(GF_LOG_ERROR, "Failed to load transition '" + type + "': " + err);
		  		seq.transition_state = 4;
		  });

	return true;
}


function get_media_time()
{
	let media_time = -4;
	if ((arguments.length == 1) && arguments[0]) {
		let s = get_source(arguments[0], null, null);
		if (!s) return -4;
		if (!s.playing) return -3;
		if (s.in_prefetch) return -1;
		if (!s.timeline_init) return -2;

		media_time = s.pids[0].pck.cts / s.pids[0].timescale;
	} else if (video_playing) {
		media_time = video_time / video_timescale;
	} else {
		media_time = audio_time / audio_timescale;
	}
	return media_time;
}

function resolve_url(url)
{
	return sys.url_cat(playlist_url, url);
}

function apply_transition_update(transition, prop_name, value)
{
	if ((prop_name=='dur')) {
		if (typeof value == 'number') {
			transition.dur = value;
		} else {
			print(GF_LOG_ERROR, 'Wrong type ' + typeof value + ' for transition.dur');
			return false;
		}
	}
	else if ((prop_name=='type')) {
		if (typeof value == 'string') {
			transition.type = value;
		} else {
			print(GF_LOG_ERROR, 'Wrong type ' + typeof value + ' for transition.type');
			return false;
		}
	} else {
		//blindly updat the transition
		transition[prop_name] = value;
	}
	return true;
}

function check_prop_type(orig, final, allow_string)
{
	if (orig == final) return true;
	if (! allow_string) return false;
	if ((final != 'number') && (final != 'string')) return false;
	if ((orig != 'number') && (orig != 'string')) return false;
	return true;
}

function parse_update_elem(pl, array)
{
	if (pl.skip || false)
		return true;

	if (typeof pl.replace != 'string') {
		print(GF_LOG_WARNING, "Invalid replace command " + JSON.stringify(pl));
		return false;
	}
	if (typeof pl.with == 'undefined') {
		print(GF_LOG_WARNING, "Invalid replace command " + JSON.stringify(pl));
		return false;
	}
	let src = pl.replace.split('@');
	if (src.length != 2) {
		print(GF_LOG_WARNING, "Invalid replace syntax " + src + ', expecting \`ID@name\`');
		return false;
	}
	let prop_name = src[1];
	src = src[0];
	let field_idx=-1;

	if (prop_name.indexOf('[')>0) {
		let vals = prop_name.split('[');
		prop_name = vals[0];
		let idx = vals[1].split(']');
		field_idx = parseInt(idx[0]);
	}

	if (prop_name=='id') {
		print(GF_LOG_WARNING, "ID property cannot be updated");
		return false;
	}

	//locate scene, transition or group
	let scene = get_scene(src);
	let transition = scene ? null : get_transition(src, false);
	let group = (scene || transition) ? null : get_group(src);
	if (scene || transition || group) {
		let mod = scene ? scene.mod : (transition ? transition.mod : group);

		//if transition we must modify the source (transition effect) since this is the object being reloaded
		//do this even when the transition is active
		if (transition) {
			apply_transition_update(get_transition(src, true), prop_name, pl.with);
		}

		let update_type = -2;

		let logname='';
		if (scene) {

			if (prop_name == "sources") {
				update_type = UPDATE_PID;
				mod = scene;
			} else {
				//module options
				update_type = scene_mod_option_update_time(scene, prop_name);
			}

			//transformation options
			if (update_type==-2) {
				update_type = group_get_update_type(prop_name, true);
				mod = scene;
			}

			if (update_type==-2) {
				//scene options, not module ones
				update_type = scene_get_update_type(prop_name);
				if (update_type>=0) {
					if (!scene_is_mod_option(prop_name))
						mod = scene;
				}
			}
			logname = 'scene ' + src;
		} else if (group) {
			update_type = group_get_update_type(prop_name, false);
			logname = 'group ' + src;
		}
		//transition updates use a single invalidate flag
		else {
			//for now we allow all modifications (they are evaluated when reloading the transition)
			update_type = UPDATE_SIZE;
			logname = 'transition ' + src;
		}

		if (update_type==-2) {
			print(GF_LOG_ERROR, 'No property ' + prop_name + ' in ' + logname + ', ignoring');
			return false;
		}
		if (update_type==-2) {
			print(GF_LOG_ERROR, 'Property ' + prop_name + ' in ' + logname + ' cannot be updated');
			return false;
		}

		let prop_type = typeof mod[prop_name];
		if (prop_type == 'undefined') {
			if (!transition) {
				print(GF_LOG_WARNING, 'No property ' + prop_name + ' in scene ' + src);
			}
			return false;
		}

		let res = pl.with;
		let allow_string = (update_type & UPDATE_ALLOW_STRING) ? true : false;
		update_type &= ~UPDATE_ALLOW_STRING;

		if (Array.isArray(mod[prop_name]) && (field_idx>=0)) {
			if (field_idx >= mod[prop_name].length) {
				print(GF_LOG_WARNING, 'Field index ' + field_idx + ' greater than number of elements ' + mod[prop_name].length + ' in property ' + prop_name);
				return;
			} else if (typeof res == typeof mod[prop_name][0]) {
				if (mod[prop_name][field_idx] != res) {
					if (update_type==UPDATE_PID) {
						mod[prop_name][field_idx] = res;
						setup_scene(scene, mod[prop_name], null);
					} else {
						mod.update_flag |= update_type;
						mod[prop_name][field_idx] = res;
						if (update_type && (scene||group))
							invalidate_parent(scene || group);
					}
				}
				trigger_watcher(scene || transition || group , prop_name, mod[prop_name][field_idx]);
			} else {
				print(GF_LOG_WARNING, 'Property ' + prop_name + ' type is ' + (typeof mod[prop_name][0]) + ' but replacement value type is ' + rep_type);
				return false;
			}
		} else {
			let res_type = typeof res;
			if (!check_prop_type(prop_type, res_type, allow_string)) {
				print(GF_LOG_ERROR, 'Property ' + prop_name + ' type is ' + prop_type + ' but replacement value type is ' + res_type + ', ignoring');
				return false;
			}

			if (mod[prop_name] != res) {
				if (update_type==UPDATE_PID) {
					setup_scene(scene, res, null);
				} else {
					mod.update_flag |= update_type;
					mod[prop_name] = res;
					if (update_type && (scene||group))
						invalidate_parent(scene || group);
				}
				trigger_watcher(scene || transition || group, prop_name, res);
			}
		}
		return true;
	}


	//locate timer
	let timer = get_timer(src);
	if (timer) {
		if (prop_name == 'start') {
			if (timer.active_state == 1) {
				print(GF_LOG_WARNING, 'Cannot modify start time of active timer');
				return false;
			}
			timer.start_time = parse_date_time(pl.with, false);
			timer.active_state = 0;
			timer.stop_time = 0;
		}
		else if (prop_name == 'stop') {
			timer.stop_time = parse_date_time(pl.with, false);
		} else if (prop_name == 'loop') {
			if (typeof pl.with == 'boolean') {
				timer.loop = pl.with ? -1 : 0;
			} else if (typeof pl.with == 'number') {
				timer.loop = pl.with;
			} else {
				print(GF_LOG_WARNING, 'Wrong type ' + (typeof pl.with) + ' for timer.loop');
				return false;
			}
		} else if (prop_name == 'dur') {
			if (typeof pl.with == 'number') {
				timer.duration = pl.with;
			} else {
				print(GF_LOG_WARNING, 'Wrong type ' + (typeof pl.with) + ' for timer.dur');
				return false;
			}
		} else if (prop_name == 'pause') {
			if (typeof pl.with == 'boolean') {
				timer.pause = pl.with;
			} else {
				print(GF_LOG_WARNING, 'Wrong type ' + (typeof pl.with) + ' for timer.pause');
				return false;
			}
		} else if (typeof timer[prop_name] != 'undefined') {
			print(GF_LOG_WARNING, 'Property ' + prop_name + ' of timer not updatable');
				return false;
		} else {
			print(GF_LOG_WARNING, 'Unknown timer property ' + prop_name);
				return false;
		}
		trigger_watcher(timer, prop_name, timer[prop_name]);
		return true;
	}

	//locate sequence
	let seq = get_sequence(src);
	if (seq) {
		if (prop_name == 'start') {
			if (seq.active_state==1) {
				print(GF_LOG_WARNING, 'Cannot modify start of active sequence');
				return false;
			} else {
				seq.start_time = parse_date_time(pl.with, true);
				//start is a date, don't skip range adjustment
				if ((typeof pl.with == 'string') && (pl.with.indexOf(':')>0)) {
					seq.skip_start_offset = false;
				}
				//start is not a date, source will start from its start range
				else {
					seq.skip_start_offset = true;
				}
				seq.active_state = 0;
				seq.stop_time = 0;
			}
		}
		else if (prop_name == 'stop') {
			seq.stop_time = parse_date_time(pl.with, false);
			if ((seq.stop_time<0) || (seq.stop_time<=seq.start_time))
				seq.stop_time = -1;
		} else if (prop_name == 'loop') {
			if (typeof pl.with == 'boolean') {
				seq.loop = pl.with ? -1 : 0;
			} else if (typeof pl.with == 'number') {
				seq.loop = pl.with;
			} else {
				print(GF_LOG_WARNING, 'Wrong type ' + (typeof pl.with) + ' for sequence.loop');
				return false;
			}
		} else if (prop_name == 'transition') {
			if (typeof pl.with == 'object') {
				seq.transition_effect = pl.with;
			} else {
				print(GF_LOG_WARNING, 'Wrong type ' + (typeof pl.with) + ' for sequence.transition');
				return false;
			}
		} else if (typeof seq[prop_name] != 'undefined') {
			print(GF_LOG_WARNING, 'Property ' + prop_name + ' of sequence not updatable');
			return false;
		} else {
			print(GF_LOG_WARNING, 'Unknown sequence property ' + prop_name);
			return false;
		}
		trigger_watcher(seq, prop_name, timer[prop_name]);
		return true;
	}
	//try inactive transition
	transition = get_transition(src, true);
	if (transition) {
		return apply_transition_update(transition, prop_name, pl.with);
	}

	//try scripts and watchers, only active property
	let script=get_script(src);
	let log_name='script';
	if (!script) {
		script=get_element_by_id(src, watchers);
		log_name='watcher';
	}
	if (script) {
		if (prop_name == 'active') {
			if (typeof pl.with == 'boolean') {
				script.active = pl.with;
			} else {
				print(GF_LOG_WARNING, 'Wrong type ' + (typeof pl.with) + ' for ' + log_name + '.active');
				return false;
			}
		} else {
			print(GF_LOG_WARNING, 'Property ' + prop_name + ' of ' + log_name + ' not updatable');
			return false;
		}
		trigger_watcher(script, prop_name, timer[prop_name]);
		return true;
	}

	//try styles and watchers, only active property
	let style = get_element_by_id(src, styles);
	if (style) {
		style[prop_name] = pl.with;
		style.updated = true;
		return true;
	}

	print(GF_LOG_WARNING, "No updatable element with id " + src + ' found');
	return false;
}

function load_updates()
{
	if (!filter.updates) return;

	let last_mtime = sys.mod_time(filter.updates);
	if (!last_mtime || (last_mtime == last_updates_modification))
		return;

	last_updates_modification = last_mtime;

	let f = sys.load_file(filter.updates, true);
	if (!f) return;

	let pl;
	try {
		pl = JSON.parse(f);
	} catch (e) {
		print(GF_LOG_WARNING, "Invalid JSON update playlist specified: " + e);
		return false;
	}

	print(GF_LOG_DEBUG, 'Playlist update is ' + JSON.stringify(pl) );

	if (Array.isArray(pl) ) {
		pl.forEach(parse_update_elem);
	} else {
		parse_update_elem(pl);
	}
}


function get_sequence_texture(seq_id)
{
	let seq = get_sequence(seq_id);
	if (!seq) return null;
	let active_src = null;
	seq.sources.forEach(s => {
			if (s.playing) active_src = s;
	});
	if (!active_src) return null;
	let active_pid = null;
	active_src.pids.forEach(p => {
		if ((p.type == TYPE_VIDEO) && p.pck) active_pid = p;
	});
	if (!active_pid) return null;

	if (!active_pid.texture) {
		create_pid_texture(active_pid);
	} else if (active_pid.frame_ts != active_pid.texture.last_frame_ts) {
		active_pid.texture.update(active_pid.pck);
	  active_pid.texture._gl_modified = true;
	  active_pid.texture.last_frame_ts = active_pid.frame_ts;
	 }
	return active_pid.texture;
}

function get_group_texture(group_id)
{
	let group = get_group(group_id);
	if (!group || (group.offscreen==GROUP_OST_NONE)) return null;
	if (!group.canvas_offscreen) {
		group_draw_offscreen(group);
	}
	return group.texture;
}

function get_screen_rect(path)
{
	if (!active_scene || !path) return null;
	if (active_scene.mx.is3D) return null;
	let rc = active_scene.mx.apply(path.bounds);

	rc.x += video_width/2;
	rc.y = video_height/2 - rc.y;

	rc.x = Math.floor(rc.x);
	rc.y = Math.floor(rc.y);
	rc.w = Math.floor(rc.w);
	rc.h = Math.floor(rc.h);

	return round_rect(rc);
}


function reload_playlist(content)
{
	if (!scene_in_update) {
		print(GF_LOG_ERROR, "reload_playlist called outside of module.update() callback, ignoring");
		return;
	}
	load_playlist(content);

	display_list.length = 0;
}

function parse_group_mod(content, parent)
{
	if (!scene_in_update) {
		print(GF_LOG_ERROR, "parse_group called outside of module.update() callback, ignoring");
		return;
	}
	parse_group(content, parent);
}
function parse_scene_mod(content, parent)
{
	if (!scene_in_update) {
		print(GF_LOG_ERROR, "parse_scene called outside of module.update() callback, ignoring");
		return;
	}
	parse_scene(content, parent);
}

function parse_playlist_element_mod(content)
{
	if (!scene_in_update) {
		print(GF_LOG_ERROR, "parse_playlist_element called outside of module.update() callback, ignoring");
		return;
	}
	parse_playlist_element(content);
}

function remove_element_mod(elmt)
{
	if (!scene_in_update) {
		print(GF_LOG_ERROR, "remove_element called outside of module.update() callback, ignoring");
		return;
	}

	if (typeof elmt == 'string') {
		let the_elem = get_group(elmt);
		if (!the_elem)
				the_elem = get_scene(elmt);
		if (!the_elem)
				the_elem = get_sequence(elmt);
		if (!the_elem)
				the_elem = get_timer(elmt);
		if (!the_elem)
				the_elem = get_script(elmt);
		if (!the_elem)
				the_elem = get_element_by_id(elmt, watchers);

		elmt = the_elem;
	}
	if (! elmt) return;

	//group
	if (elmt.scenes) {
		remove_scene_or_group(elmt);
		return;
	}
	//scene
	if (Array.isArray(elmt.sources) && Array.isArray(elmt.sequences)) {
		remove_scene_or_group(elmt);
		return;
	}
	//sequence
	if (Array.isArray(elmt.sources)) {
		let idx = sequences.indexOf(elmt);
		if (idx>=0) sequences.splice(idx, 1);

		sources.forEach(s => {
			if (s.sequence != elmt) return;
			stop_source(s, true);
			s.removed = true;
		});
	}
	//timer
	if (Array.isArray(elmt.keys)) {
		let idx = timers.indexOf(elmt);
		if (idx>=0) timers.splice(idx, 1);
		timer_restore(elmt);
	}
	//scripts
	if (typeof elmt.script == 'function') {
		let idx = user_scripts.indexOf(elmt);
		if (idx>=0) user_scripts.splice(idx, 1);
	}
	//watcher
	if (typeof elmt.target == 'string') {
		remove_watcher(elmt, false);
	}
}


function update_element_mod(id, prop, value)
{
	let update = { replace: id+'@'+prop, with: value};
	return parse_update_elem(update);
}


function query_element_mod(id, prop)
{
	let elt = get_scene(id);
	if (elt) {
		if (elt.mod && typeof elt.mod[prop] != 'undefined') return elt.mod[prop];
		if (typeof elt[prop] != 'undefined') return elt[prop];
		return undefined;
	}
	elt = get_group(id);
	if (elt) {
		if (typeof elt[prop] != 'undefined') return elt[prop];
		return undefined;
	}
	elt = get_timer(id);
	if (elt) {
		if (typeof elt[prop] != 'undefined') return elt[prop];
		return undefined;
	}
	elt = get_script(id);
	if (elt) {
		if (typeof elt[prop] != 'undefined') return elt[prop];
		return undefined;
	}
	elt = get_sequence(id);
	if (elt) {
		if (typeof elt[prop] != 'undefined') return elt[prop];
		return undefined;
	}
	return undefined;
}

function mouse_over_mod()
{
	if (!arguments.length) return null;
	let x, y;

	if (typeof (arguments[0]) == 'object') {
		let evt = arguments[0];
		x = evt.mouse_x;
		y = evt.mouse_y;
		x -= video_width/2;
		y = video_height/2 - y;
	} else if (arguments.length==2) {
		x = arguments[0];
		y = arguments[1];
	}

	let len = prev_display_list.length;
	let pt = {x: x, y: y};

	for (let i=0; i<len; i++) {
		let ctx = prev_display_list[len-i-1];
		if (ctx.mx.is3D) {
			continue;
		}

		let inv_mx = ctx.mx.copy().inverse();
		let a_pt = inv_mx.apply(pt);
		let w = ctx.scene.mod.width;
		let h = ctx.scene.mod.height;

		if (a_pt.x < -w/2) continue;
		if (a_pt.x > w/2) continue;
		if (a_pt.y < -h/2) continue;
		if (a_pt.y > h/2) continue;

		//offscreen group
		if (ctx.group) {
			return ctx.group;
		}
		let over = false;
		if (typeof ctx.scene.mod.point_over == 'function') {
			over = ctx.scene.mod.point_over(a_pt.x, a_pt.y);
		}
		if (over) return ctx.scene;
	}
	return null;
}

