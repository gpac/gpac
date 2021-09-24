import {WebGLContext} from 'webgl'
import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'
import { AudioMixer as amix } from 'gpaccore'
import * as os from 'os'


const UPDATE_PID = 1;
const UPDATE_SIZE = 1<<1;
const UPDATE_POS = 1<<2;

/*
	globals modules imported for our imported modules
*/
globalThis.sys = sys;
globalThis.evg = evg;
globalThis.os = os;


/*
	globals variables for our imported modules
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
//scene update flag indicating the scene size position has
globalThis.UPDATE_POS = UPDATE_POS;
//indicates if GPU (WebGL) is used
globalThis.use_gpu = false;
//indicates if EVG blit is enabled
globalThis.blit_enabled = evg.BlitEnabled;

/*
	globals functions for our imported modules
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
	globals functions for canvas access

Each scene module is associated an array of PidLink objects.
Each transition module is passed an array of PidLink objects.

A PidLink object has the following properties:
- pid: associated input visual pid
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
- draw(canvas): draw scene on canvas. The canvas can be EVG canvas or WebGL.

Note: scenes setup to skip drawing and forward input (fullscreen, opaque, identity) must check this.force_draw is not set to true, indicating
that the scene shall be drawn (due to canvas pixel format change for example)


*/

/*clears cnavas
\param color: color to use
\param clip clipper to use (type IRect) , or null/not specified
*/
globalThis.canvas_clear = canvas_clear;
/*sets axis-aligned clipper on canvas
\param clip clipper to use (type IRect), or null to reset
*/
globalThis.canvas_set_clipper = canvas_set_clipper;
/*draw path on canvas
\param path path (type Path) to draw
\param matrix matrix (type Matrix2D) to use
\param stencil stencil to use (type Texture or Stencil)
*/
globalThis.canvas_draw = canvas_draw;
/*blits image on canvas
\param texture texture to blit (type Texture)
\param dst_rc destination window (type IRect)
*/
globalThis.canvas_blit = canvas_blit;
/* texture path on canvas cf  canvas.fill
The input textures are fteched from the sequences associated with the scene 
\param path path (type Path) to draw
\param matrix matrix (type Matrix2D) to use
\param op_type (=0) multitexture operand type
\param op_param (=0) multitexture operand param
\param op_param (=null) multitexture second texture
*/
globalThis.canvas_draw_sources = canvas_draw_sources;




//metadata
filter.set_name("AVMix");
filter.set_desc("Audio Video Mixer");
filter.set_author("GPAC team");

//global options
filter.set_arg({ name: "pl", desc: "local playlist file to load", type: GF_PROP_STRING, def: "avmix.json" } );
filter.set_arg({ name: "live", desc: "live mode (see filter help)", type: GF_PROP_BOOL, def: "true"} );
filter.set_arg({ name: "gpu", desc: `enable GPU usage
  - off: no GPU
  - mix: only render textured path to GPU, use software rasterizer for the outlines, solid fills and gradients
  - all: try to use GPU for everything`, type: GF_PROP_UINT, def: "off", minmax_enum: 'off|mix|all', hint:"advanced"} );
filter.set_arg({ name: "thread", desc: "use threads for software rasterizer (-1 for all available cores)", type: GF_PROP_SINT, def: "-1", hint:"expert"} );
filter.set_arg({ name: "lwait", desc: "timeout in ms before considering no signal is present", type: GF_PROP_UINT, def: "1000", hint:"expert"} );
filter.set_arg({ name: "ltimeout", desc: "timeout in ms before restarting child processes (see filter help)", type: GF_PROP_UINT, def: "4000", hint:"expert"} );
filter.set_arg({ name: "maxdur", desc: "run for given seconds and exit, will not abort if 0 (used for live mode tests)", type: GF_PROP_DOUBLE, def: "0", hint:"expert"} );

//video output options
filter.set_arg({ name: "vsize", desc: "output video size", type: GF_PROP_VEC2, def: "1920x1080"} );
filter.set_arg({ name: "fps", desc: "output video frame rate", type: GF_PROP_FRACTION, def: "25"} );
filter.set_arg({ name: "pfmt", desc: "output pixel format. Use \`rgba\` in GPU mode to force alpha channel", type: GF_PROP_PIXFMT, def: "yuv"} );
filter.set_arg({ name: "dynpfmt", desc: `allow dynamic change of output pixel format in software mode
  - off: pixel format is forced to desired value
  - init: pixel format is forced to format of fullscreen input in first generated frame
  - all: pixel format changes each time a full-screen input pid at same resolution is used`, type: GF_PROP_UINT, def: "init", minmax_enum: 'off|init|all', hint:"expert"} );

//audio output options
filter.set_arg({ name: "sr", desc: "output audio sample rate", type: GF_PROP_UINT, def: "44100"} );
filter.set_arg({ name: "ch", desc: "number of output audio channels", type: GF_PROP_UINT, def: "2"} );
filter.set_arg({ name: "afmt", desc: "output audio format (only s16, s32, flt and dbl are supported)", type: GF_PROP_PCMFMT, def: "s16"} );
filter.set_arg({ name: "alen", desc: "default number of samples per frame", type: GF_PROP_UINT, def: "1024", hint:"expert"} );

filter.max_pids = -1;
filter.make_sticky();
filter.maxdur=0;

let sources = [];
let reload_tests=null;

let last_modification = 0;
let init_utc = 0;
let init_clock_us = 0;

const TYPE_UNHANDLED = 0;
const TYPE_VIDEO = 1;
const TYPE_AUDIO = 2;

let playlist_url = null;

let active_scene = null;

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

function build_help_mod(obj, name, mod_type, index)
{
	name = name.split('.')[0];
	let rad = '';
	if (mod_type==0) {
		if (!index) {
			filter._help += (mod_help_short ? '\n' : '# ' ) + '`scene` modules\n';
		}
		rad = 'Scene';
	}
	else if (mod_type==1) {
		if (!index) {
			filter._help += (mod_help_short ? '\n' : '# ' ) + '`transition` modules\n';
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
			} else if (opt.value.length) {
				opts+='\''+opt.value+'\'';
			}
		}
		if (mod_type==0) {
			if ((typeof opt.dirty == 'undefined') || !opt.dirty) {
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
		filter._help += 'For global variables available to modules, use `gpac -h avmix:globals`\n';
		filter._help += '# Available modules\n';
	} else {
		filter._help = obj.help_playlist + '\n';
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
	if (gpac_help || (sys.get_opt("temp", "gendoc") == "yes")) {
		let args = sys.args;
		let help_mod = sys.get_opt("temp", "gpac-js-help");

		if (help_mod) {
			let name;
			let path = filter.jspath;
			let	script = path+'scenes/'+help_mod+'.js';
			filter._help = '';
			single_mod_help = true;
			if (help_mod == 'globals') {
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
			mod_help_short = true;

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

	load_playlist(); 
	playlist_loaded = true;

	if (filter.live) {
		init_utc = sys.get_utc();
		init_clock_us = sys.clock_us();
	}


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
				globalThis.blit_enabled = false;
			}
	}

	configure_vout();
	configure_aout();

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
let scenes = [];

let wait_for_pids = 0;
let wait_pid_play = 0;
let live_forced_play = false;

let use_canvas_3d = false;

let defaultOrthoProjectionMatrix = null;

let audio_mix = null;



function configure_vout()
{
	if (!filter.vsize.x || !filter.vsize.y) {
		if (vout) vout.remove();
		vout = null;	
		return;
	}
	if (filter.gpu) {
		print(GF_LOG_INFO, (filter.live ? 'Live ' : '' ) + 'Video output ' + filter.vsize.x + 'x' + filter.vsize.y + ' FPS ' + filter.fps.n + '/' + filter.fps.d + ' OpenGL output');
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
	scenes.forEach(scene => {
		scene.sequences.forEach(seq => {
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

	scenes.forEach(scene => {
		for (let i=0; i<scene.mod.pids.length; i++) {
			let pid_link = scene.mod.pids[i];
			if (pid_link.pid === pid) {
				scene.mod.pids.splice(i, 1);
				scene.resetup_pids = true;
				return;
			}
		} 
	});
}

filter.update_arg = function(name, val)
{
}

filter.process_event = function(pid, evt)
{
//	if (evt.type != GF_FEVT_USER) { } 

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
	//cancel all other events events	
	return true;
}

let last_forward_pixfmt = 0;
let video_inputs_ready, audio_inputs_ready;

let blocking_ref_pids = [];
let purge_sources = [];

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
	scenes.length = 0;
	sources.forEach(stop_source);
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

	if (filter.maxdur) {
		if ((video_time > filter.maxdur*video_timescale) || (audio_time > filter.maxdur*audio_timescale)) {
			print(GF_LOG_INFO, 'maxdur reached, quit');
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
		if (!do_video && audio_playing && (audio_time * video_timescale > (video_time+video_time_inc) * audio_timescale)) {
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

	if (reload_tests && reload_tests.length) {
		if (last_pl_test_switch + video_timescale < video_time) {
			last_pl_test_switch = video_time;
			filter.pl = sys.url_cat(filter.pl, reload_tests[0]);
			reload_tests = reload_tests.splice(1);
		}
	}
	load_playlist();
	//modules are pending, wait for fail/success - async cannot wait here because QuickJS will only try to load the modules once out of JS functions
	if (modules_pending)
		return GF_OK;

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
			//incative, reschedule in 2 ms. A better way would be to compute next activation time
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

	if (do_audio) {
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

	let prefetching = [];
	//check all pids from sources
	scene.sequences.forEach(seq => {
		seq.sources.forEach(src => {
			if (src.in_prefetch) return;
			src.pids.forEach(pid => {	
				//only check for video pids
		    if (pid.type != TYPE_VIDEO)
					return;

				if (scene.mod.pids.indexOf(pid)>=0) {
					return;
				}
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
		print(GF_LOG_DEBUG, 'created PID link for ' + pid_link.pid.source.logname + '.' + pid_link.pid.name);
	});

}

function scene_update_visual_pids(scene)
{
	let ready = true;
	scene.mod.pids.forEach(pidlink => {
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
		if (!pidlink.texture) {
			if (!pidlink.pid.pck) {
				ready = false;
				return;				
			}
			pidlink.texture = new evg.Texture(pidlink.pid.pck);
			scene.mod.update_flag |= UPDATE_SIZE;
		} else {
			pidlink.texture.update(pidlink.pid.pck);
		}
	});
	return ready;
}


let round_scene_size = false;

function set_canvas_yuv(pfmt)
{
	globalThis.canvas_yuv = canvas.is_yuv;
	round_scene_size = false;
	if (canvas_yuv) {
		if (video_pfmt == 'yuv4') {}
		else if (video_pfmt =='yp4l') {}
		else if (video_pfmt =='yp4a') {}
		else if (video_pfmt =='yv4p') {}
		else if (video_pfmt =='y4ap') {}
		else if (video_pfmt =='y4lp') {}
		else {
			round_scene_size = true;
		}
	}

}
let display_list = [];

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
			filter.reschedule(next);
			print(GF_LOG_DEBUG, 'video output frame due in ' + (vtime - now) + ' us, waiting ' + next);
			return;
		}
	}

  if (!scenes.length) return;

	let has_opaque = false;
	let pid_background = null;
	let pid_background_forward = false;

  display_list.length = 0;
  let has_clip = false;
  scenes.forEach(scene => {
  	if (!scene.mod.active) return;

  	//recreate our set of inputs for the scene if any
  	if (scene.resetup_pids) {
  		scene_resetup_pids(scene);
  		scene.resetup_pids = false;
  		scene.mod.update_flag |= UPDATE_PID;
  	}
  	//not ready
  	if (!scene_update_visual_pids(scene)) return;

  	//update the scene
			scene.mod.force_draw = false;
  	let res = scene.mod.update();
  	if (!res) return;
  	if (res==2) has_clip = true;

  	scene.opaque_pid = null;
  	scene.is_opaque = false;
  	if (has_clip || (scene.mod.rotation != 0) || (scene.mod.x > 0) || (scene.mod.x+scene.mod.width < video_width)
  		|| (scene.mod.y > 0) || (scene.mod.y+scene.mod.height < video_height)
  	) {
  	} else {
  		let opaque_pid_idx = scene.mod.fullscreen();
  		if (scene.sequences.length && !scene.mod.pids.length) {

  		} else {
	  		if ((opaque_pid_idx>=0) && (opaque_pid_idx<scene.mod.pids.length)) {
		  		scene.opaque_pid = scene.mod.pids[opaque_pid_idx].pid;
	  		}

	  		if (!scene.opaque_pid) {
		  		scene.is_opaque = scene.mod.is_opaque() || false;
	  		}
  		}
  	}

  	if (scene.opaque_pid) {
	  	has_opaque = true;
	  	if (pids.indexOf(scene.opaque_pid) < 0) {
	  		print(GF_LOG_WARNING, 'Broken scene ' + scene.id + ' returned fullscreen pid not in PID list');
	  		scene.active = false;
	  		return;
	  	}
	  } else if (scene.is_opaque) {
	  	has_opaque = true;
	  }
  	display_list.push(scene);
  });
  //sort display list
  display_list.sort( (e1, e2) => { return e1.mod.zorder - e2.mod.zorder ; } );

	let frame = null;
	let pfmt = video_pfmt;
	let restore_pfmt = false;

  //reverse browse
	if (has_opaque) {
		for (let i=display_list.length; i>0; i--) {
			let scene = display_list[i-1];
			if (scene.opaque_pid) {
				display_list = display_list.splice(i-1);
				pid_background = scene.opaque_pid;
				pid_background_forward = scene.mod.identity() || false;

				if (!pid_background.pck) {
					pid_background_forward = false;
					pid_background = null;
					has_opaque = false;
					scene.mod.force_draw = true;
				}
				break;
			}
			if (scene.is_opaque) {
				display_list = display_list.splice(i-1);
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
				display_list[0].mod.force_draw = true;
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

		display_list.forEach(scene => {
			active_scene = scene;
			if (scene.gl_type == SCENE_GL_NONE) {
				scene.mod.draw(); 
			} else {
				scene.mod.draw_gl(webgl); 
			}
		});

		canvas_offscreen_deactivate();

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
				canvas = new evg.Canvas(video_width, video_height, pfmt, frame.data);
				if (filter.thread) {
					canvas.enable_threading(filter.thread);
				}
				if (use_canvas_3d)
					canvas.enable_3d();

				if (typeof canvas.blit != 'function')
					globalThis.blit_enabled = false;
				else if (!globalThis.use_gpu)
					globalThis.blit_enabled = true;

				canvas.pix_fmt = pfmt;
				set_canvas_yuv(pfmt);
			} else if (canvas_reconfig) {
				canvas_reconfig = false;	
				canvas.reassign(video_width, video_height, pfmt, frame.data);
				canvas.pix_fmt = pfmt;
				set_canvas_yuv(pfmt);
			} else {		
				canvas.reassign(frame.data);
			}

			if (!has_opaque) {
				print(GF_LOG_DEBUG, 'canvas clear to ' + back_color);
				canvas.clear(back_color);
			}
			//reset clipper
			canvas.clipper = null;
			display_list.forEach(scene => {
					active_scene = scene;
					scene.mod.draw(canvas);
				  canvas.matrix = null;
				  canvas.path = null;
				  //do not reset clipper
			});
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

	pids.forEach(pid => {
		if (pid.type != TYPE_AUDIO) return;
		if (!pid.source.playing) return;
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
		if (pid.source.mix)
			pid.volume = pid.source.volume * pid.source.mix_volume;
		else
			pid.volume = pid.source.volume;
	});

	if (nb_samples > filter.alen)
		nb_samples = filter.alen;


	let aframe = null;
	let empty=false;

	if (!nb_samples) {
		//todo - check if we need to move forward after some time (and drop input) ??
		if (nb_active_audio) {
			if (!filter.live) {
				print(GF_LOG_DEBUG, 'waiting for audio frame');
				return;
			}
			let atime = init_clock_us + (audio_time + 2*filter.alen)*1000000/filter.sr;
			let now = sys.clock_us();
			if (atime >= now) {
				print(GF_LOG_DEBUG, 'waiting for audio frame');
				return;
			}
			print(GF_LOG_WARNING, 'empty audio input but audio frame due since ' + Math.ceil(now-atime) + ' us');
		}

		//send an empty packet of 100 samples
		aframe = aout.new_packet(channels * filter.alen);
		let output = new Uint16Array(aframe.data);
		output.fill(0);
		empty=true;
		nb_samples = filter.alen;
	} else {

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
			s.sequence.active = false;
		}
	}
	return next_src;
}

function sequence_over(s)
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
		scenes.forEach(scene => {
			if (scene.sequences.indexOf(s.sequence) >= 0) {
				scene.resetup_pids = true;
			}
		});
		s.sequence.sources.forEach(src => { src.next_in_transition = false;});
		print(GF_LOG_INFO, 'Stoping source ' + s.logname);
  	s.video_time_at_init = 0;
		stop_source(s);
		return;
	}

	s.video_time_at_init = 0;

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

	if (!is_transition_start) {
		print(GF_LOG_DEBUG, 'source ' + s.logname + ' is over, sequence start offset ' + s.sequence.start_offset);
		stop_source(s);

		//transition has been started so next source is loaded, only reset scene pids
		if (s.sequence.transition_state==3) {
			scenes.forEach(scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
						scene.resetup_pids = true;
						print(GF_LOG_DEBUG, 'end of transition, scene PID resetup');
				}
			});
			s.sequence.transition_state = 0;
			return;
		}
	}

	let next_src = next_source(s, false);
	if (next_src) {
		if (next_src.in_prefetch) {
			print(GF_LOG_INFO, 'End of prefecth for ' + next_src.logname);
			next_src.in_prefetch = 0;
			next_src.pids.forEach(pid => {
				if (pid.type==TYPE_VIDEO) {
					let pck = pid.get_packet();
					print(GF_LOG_DEBUG,'got packet during prefetch: ' + ((pck==null) ? 'no' : 'yes'));
				}
			});

			scenes.forEach(scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
						scene.resetup_pids = true;
						print(GF_LOG_DEBUG, 'end of prefecth, scene PID resetup');
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
				continue;	
			}
			open_source(next_src);
			play_source(next_src);
		}
	} else if (! is_transition_start) {
		inactive_sources++;
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
		if (!s.sequence.active && (s.sequence.start_time>0) && (s.sequence.start_time <= current_utc_clock + s.prefetch_ms)) {
			s.sequence.start_offset = (current_utc_clock + s.prefetch_ms - s.sequence.start_time ) / 1000.0;
			if (s.sequence.start_offset<0.1) s.sequence.start_offset = 0;
			s.sequence.active = true;
			print(GF_LOG_DEBUG, 'Starting sequence start offset ' + s.sequence.start_offset + ' - ' + s.sequence.start_time + ' - UTC ' + current_utc_clock);

			if (s.media_start + s.media_stop < s.sequence.start_offset) {
					print(GF_LOG_DEBUG, 'source ' + s.logname + ' will end before sequence start_offset, skipping source ');
					s.sequence.start_offset -= s.media_start + s.media_stop;
					inactive_sources++;
					sequence_over(s);
					return;
			} else {
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
			print(GF_LOG_DEBUG, 'end of prefecth for ' + s.logname + ' , scene PID resetup');
			scenes.forEach(scene => {
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
	if (s.sequence.active && (s.sequence.stop_time>0) && (s.sequence.stop_time <= current_utc_clock)) {
	  	s.video_time_at_init = 0;
			stop_source(s);
			s.sequence.transition_state = 0;
			scenes.forEach(scene => {
				if (scene.sequences.indexOf(s.sequence) >= 0) {
					scene.resetup_pids = true;
				}
			});
			s.sequence.sources.forEach(src => { src.next_in_transition = false;});
			inactive_sources++;
			print(GF_LOG_INFO, 'End of sequence ' + s.sequence.id + ' at ' + (current_utc_clock-s.sequence.stop_time));
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
			if (!min_timescale || (min_cts * pid.timescale < pid.cts * min_timescale)) {
				min_cts = pck.cts;
				min_timescale = pid.timescale;
			}
			return;
		}

		//source is active (has packets)
		nb_active++;

		if (!s.sequence.prefetch_check) {
			let check_end = s.duration;
			if (s.media_stop>0 && s.media_stop<s.duration) check_end = s.media_stop;

			if ((!s.sequence.transition_state<=1) && s.sequence.transition_effect) {
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

		if (s.media_stop>0) {
			if ((pck.cts - pid.init_ts) > (s.transition_offset + s.media_stop - s.media_start) * pid.timescale) {
				pid.drop_packet();
				pid.done = true;
				pid.send_event( new FilterEvent(GF_FEVT_STOP) );
				nb_over++;
				return;
			}
		}

		if (s.sequence.transition_state==1) {
			let check_end = s.duration;
			if (s.media_stop>0 && s.media_stop<s.duration) check_end = s.media_stop;

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

			if ( (s.media_stop>0) && ((pid.pck.cts + pid.pck.dur) >= s.media_stop * pid.timescale)) {
				let diff_samples = Math.floor(s.media_stop * audio_timescale);
				if (audio_timescale == pid.timescale) {
					diff_samples -= Math.floor(pid.pck.cts);
				} else {
					diff_samples -= Math.floor(pid.pck.cts * audio_timescale / pid.timescale);
				}
				if (diff_samples>0) {
					pid.nb_samples = diff_samples;
					if ((pid.source.audio_fade=='out') || (pid.source.audio_fade=='inout'))
						pid.fade = 2; //fade to 0
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
		print(GF_LOG_DEBUG, 'Video from ' + s.logname + ' will draw pck CTS ' + pid.pck.cts + ' translated ' + pid.frame_ts + ' video time ' + video_time);
		return;
	}
	});
	//done fetching pids

	if (s.pids.length && (nb_over == s.pids.length)) {
		print(GF_LOG_INFO, 'source ' + s.logname + ' is over - transition state: ' + s.sequence.transition_state);
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
	  } else {
	  	nb_over = 0;
	  }
	} else {		
		nb_over = 0;
	}

	if (!nb_active && source_restart) {
		nb_over=0;
		s.timeline_init=0;
		stop_source(s);
		open_source(s);
		play_source(s);
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
				if ((pid.source.audio_fade=='in') || (pid.source.audio_fade=='inout'))
					pid.fade = 1; //fade from 0
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
			sequence_over(s);
		}
		//active sources and waiting for inputs
		else if (s.pids.length) {
			if (wait_video) video_inputs_ready = false;
			if (wait_audio) audio_inputs_ready = false;			
		}
		return;
	}

	sequence_over(s);

	//we are done
	if (s.removed) {
		print(GF_LOG_INFO, 'schedule for remove source ' + s.logname);
		purge_sources.push(s);
	}
}


function get_source(id, src)
{
	for (let i=0; i<sources.length; i++) {
		let elem = sources[i]; 
		if (id && (elem.id == id)) return elem;
		if (src && (elem.src.length == src.length)) {
			let diff = false;
			elem.src.forEach( (v, index) => {
				if (!(v.in === src[index].in)) diff = true;
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
	s.sequence.active = true;
	s.playing = true;
	s.timeline_init = false;
	s.pids.forEach(pid => { play_pid(pid, s); });
}

function stop_source(s)
{
	if (!s.playing) return;
	s.playing = false;
	s.sequence.prefetch_check = false;

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
	print(GF_LOG_DEBUG, 'Stoping source ' + s.logname);

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
	s.sequence.active = false;
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

function get_sequence_by_id(seq_id)
{
	for (let i=0; i<sequences.length; i++) {
		if (sequences[i].id === seq_id) return sequences[i];
	}
	return null;
}

function get_sequence(seq_pl)
{
	let id = (seq_pl==null) ? "_seq_default" : seq_pl.id;
	for (let i=0; i<sequences.length; i++) {
		if (sequences[i].id === id) return sequences[i];
	}
	let seq = {};
	seq.id = id;
	seq.pl_update = true;
	seq.active = false;
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
	if (typeof pl_el.volume == 'number')
		src.volume = pl_el.volume;
	else
		src.volume = 1.0;
	if (typeof pl_el.mix == 'boolean')
		src.mix = pl_el.mix;
	else
		src.mix = true;

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
	s.volume = 1.0;
	s.mix_volume = 1.0;
	s.keep_alive = el.keep_alive || false;
	s.video_time_at_init = 0;

	sources.push(s);
	if (s.id==="") {
		s.logname = el.src[0].in.split('\\').pop().split('/').pop(); 
	} else {
		s.logname = s.id; 
	}
	let parent_seq = get_sequence(seq);

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
	let id = src_pl.id || 0;

	let src = get_source(id, src_pl.src);
	if (!src) {
		push_source(src_pl, id, seq_pl);
	} else {
		let par_seq = get_sequence(seq_pl);
		if (src.sequence != par_seq) {
			print(GF_LOG_ERROR, 'source update cannot change parent sequence, ignoring');
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
	let seq = get_sequence(pl);
	//got new seg
	seq.pl_update = true;

	let crc = sys.crc32(JSON.stringify(pl));
	if (crc != seq.crc) {
		seq.crc = crc;
	} else {
		seq.sources.forEach(s => {
			s.pl_update = true;
		});
		return;
	}

  //parse UTC start time
	if (seq.start_time<=0) {
		seq.start_time = parse_date_time(pl.start, true);
		if (seq.start_time<0) 
			seq.start_time = -1;
	}

	seq.stop_time = parse_date_time(pl.stop, true);
	if ((seq.stop_time<0) || (seq.stop_time<=seq.start_time)) 
		seq.stop_time = -1;

	seq.transition_effect = pl.transition || null;

	seq.loop = pl.loop || 0;
	if (seq.loop && (typeof pl.loop == 'boolean'))
		seq.loop = -1;
	pl.seq.forEach( p => { parse_url(p, pl); });
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
			if (!get_sequence_by_id(s_id)) {
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
		scenes.forEach(s => {
			if (s.id === scene_id) nb_scenes++;
		});
		if (nb_scenes>1) {
			print(GF_LOG_WARNING, 'More than one scene defined with the same id ' + scene_id + ', ignoring element ' + JSON.stringify(pl) );
			return false;
		}
	}
	return true;
}

function get_scene(scene_id, js_name)
{
	for (let i=0; i<scenes.length; i++) {
		let scene = scenes[i];
		let a_scene_id = scene.id || null;
		if (scene_id && a_scene_id && (scene_id==a_scene_id)) {
			return scene;
		}
		if ((scene_id && !a_scene_id) || (!scene_id && a_scene_id)) {
			continue;
		}
		if (!js_name) continue;
		if (scene.js == js_name) return scene;
	}
	return null;
}

function parse_scene(pl)
{
	let scene_id = pl.id || null;	
	let scene = get_scene(scene_id, pl.js);
	if (!scene) {
		if (typeof pl.sources == 'undefined') pl.sources = [];

		create_scene(pl.sources, pl);
	} else {
		scene.pl_update = true;
		set_scene_options(scene, pl);
		check_scene_coords(scene);
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
			if (typeof pl.fps == string) {
				var fps = pl.fps.split('/');
				filter.fps.num = parseInt(fps[0]);
				filter.fps.den = (fps.length>2) ? parseInt(fps[1]) : 1;
			} else {
				filter.fps.num = pl.fps;
				filter.fps.den = 1;
			}	
		}
		else if (propertyName == 'dynpfmt') {
			if (pl.dynpfmt=='off') filter.dynpfmt=0;
			else if (pl.dynpfmt=='init') filter.dynpfmt=1;
			else if (pl.dynpfmt=='all') filter.dynpfmt=2;
			else print(GF_LOG_WARNING, "Wrong syntax for option \`dynpfmt\` in playlist, ignoring");
		}
		else if (propertyName == 'pfmt') filter.pfmt = pl.pfmt;
		else if (propertyName == 'afmt') filter.afmt = pl.afmt;
		else if ((propertyName == 'live') || (propertyName == 'thread') || (propertyName == 'lwait') || (propertyName == 'ltimeout')
			 || (propertyName == 'sr') || (propertyName == 'ch') || (propertyName == 'alen') || (propertyName == 'maxdur')
		) {
			if (typeof pl[propertyName]  != 'number') print(GF_LOG_WARNING, "Expecting number for option \`" + propertyName + "\` in playlist config, ignoring");
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
		else if (propertyName == 'type' ) {}
		else if (propertyName.charAt(0) != '_') {
			print(GF_LOG_WARNING, 'Unrecognized option ' + propertyName + ' in playlist config ' + JSON.stringify(pl) );
		}
	}
}


function parse_pl_elem(pl, array)
{
	let type = pl.type || 'url';

	if (pl.skip || false)
		return;

	if (Array.isArray(pl.src))
		type = 'url';

	if (Array.isArray(pl.seq))
		type = 'seq';

	if (Array.isArray(pl.sources) 
		|| (typeof pl.js == 'string')
		|| (typeof pl.x == 'number')
		|| (typeof pl.y == 'number')
		|| (typeof pl.width == 'number')
		|| (typeof pl.height == 'number')
	)
		type = 'scene';

	if (Array.isArray(pl.keys)
		|| (typeof pl.start_time != 'undefined')
		|| (typeof pl.dur == 'number')
	)
		type = 'timer';

	if (type==='url') {
		if (validate_source(pl)) {
			parse_url(pl, null);
		}	
	} else if (type==='seq') {
		if (validate_seq(pl)) {
			parse_seq(pl);
		}	
	} else if (type==='scene') {
		if (validate_scene(pl)) {
			parse_scene(pl);
		}	
	} else if (type==='timer') {
		if (validate_timer(pl)) {
			parse_timer(pl);
		}	
	} else if (type==='config') {
		if (!playlist_loaded) {
			parse_config(pl);
		}	
	} else {
		print(GF_LOG_WARNING, 'type ' + type + ' not defined, ignoring element ' + JSON.stringify(pl) );
	}
}

function load_playlist()
{
	let last_mtime = sys.mod_time(filter.pl);
	if (!last_mtime || (last_mtime == last_modification))
		return true;

	print(GF_LOG_DEBUG, last_modification ? 'refreshing JSON config' : 'loading JSON config')

	let f = sys.load_file(filter.pl, true);
	if (!f) return  true;

	current_utc_clock = Date.now();

	let pl;
	try {
		pl = JSON.parse(f);
	} catch (e) {
		print(GF_LOG_ERROR, "Invalid JSON playlist specified: " + e);
		last_modification = last_mtime;
		return false;
	}
	last_modification = last_mtime;
	print(GF_LOG_DEBUG, 'Playlist is ' + JSON.stringify(pl) );

	//mark all our objects present
	sequences.forEach(seq => {
		seq.pl_update = false;
		seq.sources.forEach(src => {
			src.pl_update = false;
		});
	});
	scenes.forEach(scene => {
		scene.pl_update = false;
	});

	if (Array.isArray(pl) ) {
		pl.forEach(parse_pl_elem);
	} else {
		parse_pl_elem(pl);
	}

	//cleanup sources no longer in use
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
	for (let i=0; i<scenes.length; i++) {
		let s = scenes[i];
		if (s.pl_update) continue;

		print(GF_LOG_INFO, 'Scene ' + s.id + ' removed');
		scenes.splice(i, 1);
		i--;
	}


	//create default scene if needed
	if (!scenes.length && sources.length) {
		print(GF_LOG_INFO, 'No scenes defined, generating default one');
		create_scene(null, { "js": "shape"});
	}
	return true;
}

function parse_val(params, name, scaler, scene_obj, def_val, update_type)
{
	let new_val;
	let prop_set = (typeof scene_obj[name] == 'undefined') ? false : true;

	if (typeof params[name] == 'undefined') {
		//first setup, use default value
		if (! prop_set)
			new_val = def_val;
		else
			new_val = scene_obj[name];
	}
	else if (typeof params[name] == 'string') {
			if (params[name] == "width") new_val = scene_obj.width;
			else if (params[name] == "height") new_val = scene_obj.height;
			else new_val =  parseInt(params[name]);
	} else if (typeof params[name] == 'object') {
			return;
	} else if (scaler) {
		new_val = (params[name] * scaler / 100);
	} else {		
		new_val = params[name];
	}
	//any change to these properties will require a PID reconfig of the scene
	if (prop_set && (new_val != scene_obj[name])) {
		scene_obj.update_flag |= update_type;
	}
	scene_obj[name] = new_val;
}

function set_scene_options(scene, params)
{
	//setup defaults
	parse_val(params, "x", video_width, scene.mod, 0, UPDATE_POS);
	parse_val(params, "y", video_height, scene.mod, 0, UPDATE_POS);
	parse_val(params, "width", video_width, scene.mod, video_width, UPDATE_SIZE);
	parse_val(params, "height", video_height, scene.mod, video_height, UPDATE_SIZE);
	parse_val(params, "zorder", 0, scene.mod, 0, 0);
	parse_val(params, "active", 0, scene.mod, true, 0);
	parse_val(params, "rotation", 0, scene.mod, 0, UPDATE_POS);
	parse_val(params, "hskew", 0, scene.mod, 0, UPDATE_POS);
	parse_val(params, "vskew", 0, scene.mod, 0, UPDATE_POS);

	if (scene.options) {
		scene.options.forEach( o => {
				if (typeof o.name == 'undefined') return;
				if (typeof o.value == 'undefined') return;
				let prop_set = (typeof scene.mod[o.name] == 'undefined') ? false : true;

				if (params && (typeof params[o.name] == typeof o.value) ) {
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
		if (propertyName == 'x') continue;
		if (propertyName == 'y') continue;
		if (propertyName == 'width') continue;
		if (propertyName == 'height') continue;
		if (propertyName == 'zorder') continue;
		if (propertyName == 'active') continue;
		if (propertyName == 'rotation') continue;
		if (propertyName == 'hskew') continue;
		if (propertyName == 'vskew') continue;
		if (propertyName == 'id') continue;
		if (propertyName == 'js') continue;
		if (propertyName == 'skip') continue;
		if (propertyName == 'sources') continue;
		if (propertyName == 'mix') continue;
		if (propertyName == 'mix_ratio') continue;

		if (propertyName.charAt(0) == '_') continue;
		if (scene.options && (typeof scene.mod[propertyName] != 'undefined')) continue;

		print(GF_LOG_WARNING, 'Unrecognized scene option ' + propertyName + ' for ' + scene.id + ' ');
	}

	//check if we have a mix instruction
	if (!scene.sequences || (scene.sequences.length<=1)) {
		scene.transition_effect = null;
		scene.mod.mix_ratio = -1;
		scene.transition = null;
	} else {
		let old_fx = scene.transition_effect;
		scene.transition_effect = params.mix || null;
		scene.mod.mix_ratio = params.mix_ratio || 0;

		if (old_fx && scene.transition_effect && (scene.transition_effect.type != old_fx.type))
			scene.transition = null;

		if (!scene.transition) {
			scene.transition_state = 0;
			load_transition(scene);
		}
	}
}

function check_scene_coords(scene)
{
	if (round_scene_size && scene.sequences.length && canvas_yuv) {
		if (scene.mod.x % 2) scene.mod.x--; 
		if (scene.mod.y % 2) scene.mod.y--;
		if (scene.mod.width % 2) scene.mod.width--;
		if (scene.mod.height % 2) scene.mod.height--;
	}
}

function setup_scene(scene, seq_ids, params)
{

	scene.sequences = [];
	scene.resetup_pids = true;
	scene.mod.pids = [];
	scene.transition_effect = null;
	scene.mod.mix_ratio = -1;
	scene.transition = null;

	//default scene
	if (!seq_ids) {
		scene.sequences.push( sequences[0] );
	} else if (typeof seq_ids != 'undefined') {
		seq_ids.forEach(sid =>{
			let s = get_sequence_by_id(sid);
			scene.sequences.push(s);
		}); 
	}

	scene.mod.update_flag = UPDATE_PID;
	set_scene_options(scene, params);

	check_scene_coords(scene);

}

function create_scene(seq_ids, params)
{
	let script_src = sys.url_cat(playlist_url, params.js);


	if (! sys.file_exists(script_src)) {
		script_src = filter.jspath + 'scenes/' + params.js + '.js';

		if (! sys.file_exists(script_src)) {
			print(GF_LOG_ERROR, 'No such scene file ' + script_src);
			return;
		}
	}

	modules_pending ++;
	let scene = {};
	scene.id = params.id || "_gpac_scene_default";
	scene.pl_update = true;
	scene.mod = null;
	scene.sequences = null;
	scenes.push(scene);
	scene.gl_type = SCENE_GL_NONE;

	import(script_src)
		  .then(obj => {
		  		modules_pending--;
		  		print(GF_LOG_DEBUG, 'Module ' + script_src + ' loaded');
		  		scene.mod = obj.load();
		  		scene.mod.id = scene.id;

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
								let idx = scenes.indexOf(scene);
								scenes.splice(idx, 1);
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
		  		print(GF_LOG_ERROR, "Failed to load scene '" + params.js + "': " + err);
			    let index = scenes.indexOf(scene);
			    if (index > -1) scenes.splice(index, 1);

		  });
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
			if (typeof val != 'string' && typeof val != 'number' ) {
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
		if (propertyName == 'start_time') continue;
		if (propertyName == 'stop_time') continue;
		if (propertyName == 'keys') continue;
		if (propertyName == 'anims') continue;
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
		if (filter.live) {
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
	let eval_start_time = false;
	let timer = null;
	let timer_id = pl.id || null;
	timers.forEach(t => {
		let tid = t.id || null;
		if (tid && timer_id && (tid==timer_id)) timer = t;
	});

	if (!timer) {
		timer = {};
		timers.push(timer);
		eval_start_time = true;
		timer.active_state = 0;
		timer.id = timer_id;
		timer.crc = 0;
	} 

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
				a.fun = anim.mode;
			}
		} 
		
		a.postfun = (typeof anim.postfun == 'string') ? anim.postfun : null;

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
			tar.scene = get_scene(vals[0]);
			tar.field = vals[1];
			if ((tar.field=="x") || (tar.field=="width")) tar.atype = 1; 
			else if ((tar.field=="y") || (tar.field=="height")) tar.atype = 2; 
			else tar.atype = 0;

			tar.update_type = UPDATE_SIZE;
			if ((tar.field=="width") || (tar.field=="height"))
					tar.update_type = UPDATE_SIZE; 
			else if ((tar.field=="x") || (tar.field=="y") || (tar.field=="rotation") || (tar.field=="hskew") || (tar.field=="vskew"))
					tar.update_type = UPDATE_POS; 
			else if (tar.scene && tar.scene.options && (typeof tar.scene.options[tar.field] != 'undefined'))
				tar.update_type = tar.scene.options[tar.field].dirty || UPDATE_SIZE;

			if (!tar.update_type) {
				print(GF_LOG_ERROR, 'Field ' + tar.field + ' of scene ' + tar.scene.id + ' is not updatable, ignoring target');
				return;
			}

			if (tar.field=="mix_ratio")
				tar.update_type = 0; 

			//indexed anim
			if (tar.field.indexOf('[')>0) {
				vals = tar.field.split('[');
				tar.field = vals[0];
				let idx = vals[1].split(']');
				tar.idx = parseInt(idx[0]);
			} else {
				tar.idx = -1;
			}
			a.targets.push(tar);
		});
		timer.anims.push(a);
	}); 

	timer.loop = pl.loop || false;

	if (!timer.active_state || (timer.active_state==2)) {
		eval_start_time = true;
	}

	if (eval_start_time) {
		timer.duration = pl.dur || 0;
		timer.start_time = parse_date_time(pl.start_time, false);
	}

	timer.stop_time = parse_date_time(pl.stop_time, false);
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

function update_timer(timer)
{
	let do_store=false;

	if (timer.active_state == 2) return;

	let now = init_utc + video_time * 1000 / video_timescale;
	if ((timer.start_time<0) || (timer.start_time > now)) return;

	if (!timer.active_state) {
		timer.active_state = 1;
		timer.activation_time = video_time;
		do_store = true;
	}
	let frac = (video_time - timer.activation_time) * video_time_inc / video_timescale;
	
	if (frac > timer.duration) {
		if (!timer.loop && (timer.stop_time<0) ) {
			timer.active_state = 2;
		} else {
			while (frac > timer.duration) frac -= timer.duration;
		}
	}
	if ((timer.stop_time > 0) && (timer.stop_time <= now)) {
		timer.active_state = 2;
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
				eval(anim.fun);
				if (interp<0) interp=0;
				else if (interp>1) interp=1;
		}

		if (anim.mode==1) {
			res = value1;
			if (anim.postfun) {
				eval(anim.postfun);
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
				eval(anim.postfun);
			}
		}

		anim.targets.forEach(target => {
			if (!target.scene) return;
			let scene = target.scene.mod;

			if (typeof scene[target.field] == 'undefined' ) return;
			let update_type = target.update_type;

			let final = res;
			if (target.atype==1) {
				if (res == 'height') res = scene.height;
				else {
					final = res * video_width / 100; 
				}
			}
			else if (target.atype==2) {
				if (res == 'width') res = scene.width;
				else {
					final = res * video_height / 100; 
				}
			}

			if (target.idx>=0) {
				if (Array.isArray(scene[target.field])
					&& (target.idx<scene[target.field].length)
					&& (typeof scene[target.field][target.idx] == typeof final)
				) {
					if (do_store) {
						target.orig_value = scene[target.field][target.idx];
					}
					if ((scene[target.field][target.idx] != final)) {
						scene.update_flag |= update_type;
						scene[target.field][target.idx] = final;
					}
				}
				return;
			}

			if ( (typeof scene[target.field] == typeof final) ) {
				if (do_store) {
					target.orig_value = scene[target.field];
				}
				if ((scene[target.field] != final)) {
					scene.update_flag |= update_type;
					scene[target.field] = final;
					if ((target.atype==1) || (target.atype==2)) check_scene_coords(target.scene)
					print(GF_LOG_DEBUG, 'update ' + target.scene.id + '.' + target.field + ' to ' + final);
				}
				return;
			}
			else if ((typeof res == 'string') && (typeof scene[target.field] == 'number') ) {
				 let val = globalThis[res];
				 if (typeof val == typeof scene[target.field]) {
					if (do_store) {
						target.orig_value = scene[target.field];
					}
					if (scene[target.field] != val) {
						scene.update_flag |= update_type;
 						scene[target.field] = val;
					}
					return;
				 }
			}
			print(GF_LOG_WARNING, 'Anim type mismatch for ' + target.field + ' got ' + typeof final + ' expecting ' + typeof scene[target.field] + ', ignoring');
		});

	});
}



function canvas_clear(color, clip)
{
		if (webgl) {
	    let a = sys.color_component(color, 0);
    	let r = sys.color_component(color, 1);
    	let g = sys.color_component(color, 2);
    	let b = sys.color_component(color, 3);

		  webgl.viewport(clip.x, clip.y, clip.w, clip.h);
		  webgl.clearColor(r, g, b, a);
  		webgl.clear(webgl.COLOR_BUFFER_BIT);
		  webgl.viewport(0, 0, video_width, video_height);
		  return;
		}
		canvas.clipper = clip;
		canvas.clear(color);
		canvas.clipper = null;
}

function canvas_set_clipper(clip)
{
		if (webgl) {
			if (clip)
			  webgl.viewport(clip.x, clip.y, clip.w, clip.h);
			else
			  webgl.viewport(0, 0, video_width, video_height);
		  return;
		}
		canvas.clipper = clip;
}


let canvas_offscreen_active = false;
let last_named_tx_id = 0;

function flush_offscreen_canvas()
{
 	canvas_offscreen._gl_texture.upload(canvas_offscreen._evg_texture);

  let glprog = canvas_offscreen._gl_program;
  webgl.useProgram(glprog.program);

  webgl.viewport(0, 0, video_width, video_height);

  //set video texture
  webgl.activeTexture(webgl.TEXTURE0);
  webgl.bindTexture(webgl.TEXTURE_2D, canvas_offscreen._gl_texture);
	webgl.uniform1i(glprog.uniformLocations.mainTx, 0);

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

		canvas_offscreen._gl_tx_id = '_gf_internal_sampler' + last_named_tx_id;
		last_named_tx_id++;

		canvas_offscreen._gl_texture = webgl.createTexture(canvas_offscreen._gl_tx_id);
		if (!canvas_offscreen._gl_texture) return false;

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

function gl_set_color_matric(cmx_val, cmx_mul_uni, cmx_add_uni)
{
	if (!cmx_mul_uni) return;

	let cmx_mul = [];
	for (let i=0; i<4; i++) {
  	for (let j=0; j<4; j++) {
			cmx_mul.push(cmx_val[i*5 + j]);
		}
	}
  webgl.uniformMatrix4fv(cmx_mul_uni, false, cmx_mul);
  webgl.uniform4f(cmx_add_uni, cmx_val[4], cmx_val[9], cmx_val[14], cmx_val[19]);

}

let no_signal_text = null;
let no_signal_brush = new evg.SolidBrush();
let no_signal_path = null;
let no_signal_outline = null;

function draw_scene_no_signal(matrix)
{
	if (!no_signal_path) no_signal_path = new evg.Text();

	let text = no_signal_path;
	text.fontsize = active_scene.mod.height / 20;  
  text.font = ['SANS'];
  text.align = GF_TEXT_ALIGN_LEFT;
	text.baseline = GF_TEXT_BASELINE_TOP;

	text.maxWidth = active_scene.mod.width;
	let d = new Date();
	if (no_signal_text==null) {
		//let version = 'GPAC '+sys.version + ' API '+sys.version_major+'.'+sys.version_minor+'.'+sys.version_micro;
		let version = 'GPAC ' + (sys.test_mode ? 'Test Mode' : sys.version_full);
		no_signal_text = ['No input', '', version, '(c) 2000-2021 Telecom Paris'];
	}
	let s1 = null;
	if (active_scene.mod.pids.length) {
		if (sys.test_mode) 
			s1 = 'Signal lost';
		else
			s1 = 'Signal lost (' + Math.floor(active_scene.mod.pids[0].pid.source.no_signal/1000) + ' s)';
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

  let mx = new evg.Matrix2D(matrix);
  mx.translate(-active_scene.mod.width/2, 0);

  if (!no_signal_outline) {
  	no_signal_outline = text.get_path().outline( { width: 6, align: GF_PATH_LINE_CENTER, cap: GF_LINE_CAP_ROUND, join: GF_LINE_JOIN_ROUND } );
  }
  no_signal_brush.set_color('black');
  no_signal_brush._gl_color = 'black';
	canvas_draw(no_signal_outline, mx, no_signal_brush, true);

/*  no_signal_brush.set_color('black');
  no_signal_brush._gl_color = 'black';
	canvas_draw(text, mx, no_signal_brush, true);
  mx.translate(-2, 2);
*/
  no_signal_brush.set_color('white');
  no_signal_brush._gl_alpha = 1;
  no_signal_brush._gl_color = 'white';
	canvas_draw(text, mx, no_signal_brush, true);
}

function canvas_draw(path, matrix, stencil)
{
	if (arguments.length==3) {
		if (!path)
			return;
		if (!stencil) {
			//this only happens if no more input pids for scene
			draw_scene_no_signal(matrix);
			return;
		}
		if (active_scene.mod.pids.length && active_scene.mod.pids[0].pid.source.no_signal) {
			draw_scene_no_signal(matrix);
			return;		
		}
	}

	if (webgl) {
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

			if (use_soft_raster) {
				canvas_offscreen_activate();
			  canvas_offscreen.path = path;
			  canvas_offscreen.matrix = matrix;
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
					stencil._gl_tx_id = 'internal_sampler' + last_named_tx_id;
					last_named_tx_id++;
					stencil._gl_texture = webgl.createTexture(stencil._gl_tx_id);
					if (!stencil._gl_texture) return;
				}
				webgl.bindTexture(webgl.TEXTURE_2D, stencil._gl_texture);
		    stencil._gl_texture.upload(stencil);
			}

			let prog_info = stencil._gl_program || null;
	    if (!stencil._gl_program) {
	    		if (is_texture) {
	    			let fs_src = (stencil._gl_cmx==null) ? fs_source : fs_source_cmx; 
	  	      prog_info = stencil._gl_program = setup_webgl_program(vs_source, fs_src, stencil._gl_tx_id);
	  	      if (!stencil._gl_program) return;

	  	      if (stencil._gl_cmx) {
	  	      	prog_info.uniformLocations.cmx_mul = webgl.getUniformLocation(prog_info.program, 'cmx_mul');
	  	      	prog_info.uniformLocations.cmx_add = webgl.getUniformLocation(prog_info.program, 'cmx_add');
	  	      } else {
							prog_info.uniformLocations.cmx_mul = null;	  	      	
							prog_info.uniformLocations.cmx_add = null;	  	      	
	  	      }
	    		} else {
	  	      prog_info = stencil._gl_program = setup_webgl_program(vs_const_col, fs_const_col, null);
	  	      if (!stencil._gl_program) return;

				  	let color = stencil._gl_color || "0xFFFFFFFF";
				  	let alpha = stencil._gl_alpha || 1;
				    let a = sys.color_component(color, 0) * alpha;
			    	let r = sys.color_component(color, 1);
			    	let g = sys.color_component(color, 2);
			    	let b = sys.color_component(color, 3);
					  webgl.useProgram(prog_info.program);
					  webgl.uniform4f(prog_info.uniformLocations.color, r, g, b, a);
	    		}
    	}

		  webgl.useProgram(prog_info.program);

		  webgl.viewport(0, 0, video_width, video_height);

		  //set transform
	  	const modelViewMatrix = new evg.Matrix(matrix);
		  webgl.uniformMatrix4fv(prog_info.uniformLocations.modelViewMatrix, false, modelViewMatrix.m);

		  if (is_texture) {
			  let mx2d = stencil._gl_mx || null;
		  	const txmx = mx2d ? new evg.Matrix(mx2d) : new evg.Matrix();
			  webgl.uniformMatrix4fv(prog_info.uniformLocations.txMatrix, false, txmx.m);

	      let alpha = stencil._gl_alpha || 1;
			  webgl.uniform1f(prog_info.uniformLocations.alpha, alpha);

	      if (stencil._gl_cmx) {
					gl_set_color_matric(stencil._gl_cmx, prog_info.uniformLocations.cmx_mul, prog_info.uniformLocations.cmx_add);
	      }


			  //set video texture
			  webgl.activeTexture(webgl.TEXTURE0);
			  webgl.bindTexture(webgl.TEXTURE_2D, stencil._gl_texture);
				webgl.uniform1i(prog_info.uniformLocations.mainTx, 0);

				mesh.draw(webgl, prog_info.attribLocations.vertexPosition, prog_info.attribLocations.textureCoord);
		  } else {
				mesh.draw(webgl, prog_info.attribLocations.vertexPosition);
		  }
		  webgl.useProgram(null);
			return;
	}
  canvas.path = path;
  canvas.matrix = matrix;
  canvas.fill(stencil);      
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

const fs_trans_source_prefix = `
varying vec2 txcoord_from;
uniform sampler2D maintx1;
uniform float _alpha_from;
varying vec2 txcoord_to;
replace_uni_cmx1

uniform sampler2D maintx2;
uniform float _alpha_to;
uniform float ratio;
replace_uni_cmx2

vec4 get_pixel_from(vec2 tx_coord)
{
	vec4 col = texture2D(maintx1, tx_coord);
	replace_cmx1
	col.a *= _alpha_from;
	return col;
}

vec4 get_pixel_to(vec2 tx_coord)
{
	vec4 col = texture2D(maintx2, tx_coord);
	replace_cmx2
	col.a *= _alpha_to;
	return col;
}

`;

function canvas_draw_sources(path, matrix)
{
	let scene = active_scene;
	let pids = scene.mod.pids;

	let op_type = 0;
	let op_param = 0;
	let op_tx = null;
	if (arguments.length == 5) {
		op_type = arguments[2];
		op_param = arguments[3];
		op_tx = arguments[4];
	}

	if (pids.length==0) {
		print(GF_LOG_ERROR, 'Broken scene ' + scene.id + ': call to draw_sources without any source pid !');
		scene.active=false;
		return;
	}
	if ((pids.length==1) && !op_type) {
		let tx = pids[0].texture;
		//should not happen
		if (!tx) return;

		canvas_draw(path, matrix, tx);
	  return;  
	}

	if (pids[0].pid.source.no_signal) {
		draw_scene_no_signal(matrix);
		return;
	}

	if (op_type) {
		if (use_gpu) {
			print(GF_LOG_WARNING, 'Multitexture canvas fill without transitions not implemented for GPU, disabling multitexture !');
			canvas_draw(path, matrix, pids[0].texture);
			return;
		}
	  canvas.path = path;
	  canvas.matrix = matrix;
	  canvas.fill(op_type, [op_param], pids[0].texture, op_tx);
		return;		
	}

	if (pids[0].pid.source.sequence.transition_state==4) {
		print(GF_LOG_DEBUG, 'transition in error, using simple draw');
		canvas_draw(path, matrix, pids[0].texture);
		return;
	}

	if (pids[1].pid.source.no_signal) {
		draw_scene_no_signal(matrix);
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
	} else if (scene.transition) {
		transition = scene.transition;
		ratio = scene.mod.mix_ratio;
		if (ratio<0) ratio = 0;
		else if (ratio>1) ratio = 1;
		pids[0].pid.source.mix_volume = 1-ratio;
		pids[1].pid.source.mix_volume = ratio;

		if (scene.transition_state==4) {
			if (ratio>0.5) ratio = 1;
			else ratio = 0;
		}

		if (ratio==0) {
			canvas_draw(path, matrix, pids[0].texture);
			return;
		}
		if (ratio==1) {
			canvas_draw(path, matrix, pids[1].texture);
			return;
		}
	}

	if (transition) {
		if (typeof transition.fun == 'string') {
			let old_ratio = ratio;
			eval(transition.fun);
			if (!old_ratio) ratio = 0; 
			else if (!old_ratio==1) ratio = 1; 
		}
		if (!use_gpu) {
			transition.mod.apply(canvas, ratio, path, matrix, pids);
			return;
		}

		//gpu: create mesh, bind textures, call apply and draw mesh

		canvas_offscreen_deactivate();

		let mesh = path.mesh || null;
		if (!mesh) {
			mesh = path.mesh = new evg.Mesh(path);
			if (!path.mesh) return;
			path.mesh.update_gl();
		}

		//setup all textures
		let tx_slot = 0;
		pids.forEach (pid => {
			let texture = pid.texture._gl_texture || null;

		  webgl.activeTexture(webgl.TEXTURE0 + tx_slot);

			if (!texture) {
				pid.texture._gl_tx_id = 'internal_sampler' + last_named_tx_id;
				last_named_tx_id++;
				pid.texture._gl_texture = webgl.createTexture(pid.texture._gl_tx_id);
				if (!pid.texture._gl_texture) return;
			}
			//bind and update
			webgl.bindTexture(webgl.TEXTURE_2D, pid.texture._gl_texture);
		    pid.texture._gl_texture.upload(pid.texture);
			tx_slot += pid.texture._gl_texture.nb_textures;
		});

		//create program
		let prog_info = transition.gl_program || null;
	    if (!prog_info) {
	  		let frag_source = fs_trans_source_prefix;
	  		let tx_source = transition.mod.get_shader_src();
	  		if (typeof tx_source != 'string') {
			    print(GF_LOG_ERROR, 'invalid shader source ' + tx_source);
			  	print(GF_LOG_ERROR, 'aborting transition');
			  	if (seq)
			  		seq.transition_state = 4;
	  			return;
	  		}
	  		frag_source += tx_source;

	  		//replace maintx1 by first texture name
			frag_source = frag_source.replaceAll('maintx1', pids[0].texture._gl_tx_id);
  			//replace maintx2 by second texture name
		  	frag_source = frag_source.replaceAll('maintx2', pids[1].texture._gl_tx_id);

			if (pids[0].texture._gl_cmx) {
				frag_source = frag_source.replaceAll('replace_uni_cmx1', `
uniform mat4 cmx1_mul;
uniform vec4 cmx1_add;
`);

				frag_source = frag_source.replaceAll('replace_cmx1', `
col = cmx1_mul * col;
col += cmx1_add;
col = clamp(col, 0.0, 1.0);
`);
		  	} else {
				frag_source = frag_source.replaceAll('replace_uni_cmx1', '');
				frag_source = frag_source.replaceAll('replace_cmx1', '');
		  	}

		  	if (pids[1].texture._gl_cmx) {
				frag_source = frag_source.replaceAll('replace_uni_cmx2', `
uniform mat4 cmx2_mul;
uniform vec4 cmx2_add;
`);

				frag_source = frag_source.replaceAll('replace_cmx2', `
col = cmx2_mul * col;
col += cmx2_add;
col = clamp(col, 0.0, 1.0);
`);
		  	} else {
				frag_source = frag_source.replaceAll('replace_uni_cmx2', '');
				frag_source = frag_source.replaceAll('replace_cmx2', '');
		  	}


		 	const shaderProgram = webgl_init_shaders(transition_vs_source, frag_source, null);
		 	if (!shaderProgram) {
		  		print(GF_LOG_ERROR, 'shader creation failed, aborting transition');
		  		if (seq) 
		  			seq.transition_state = 4;
		  		return;
		 	}
			prog_info = {
				program: shaderProgram,
  				vertexPosition: webgl.getAttribLocation(shaderProgram, 'aVertexPosition'),
				textureCoord: webgl.getAttribLocation(shaderProgram, 'aTextureCoord'),
	      		projectionMatrix: webgl.getUniformLocation(shaderProgram, 'uProjectionMatrix'),
  				modelViewMatrix: webgl.getUniformLocation(shaderProgram, 'uModelViewMatrix'),
  				ratio: webgl.getUniformLocation(shaderProgram, 'ratio'),
  				textures: [
  					{
				    matrix: webgl.getUniformLocation(shaderProgram, 'textureMatrixFrom'),
				    alpha: webgl.getUniformLocation(shaderProgram, '_alpha_from'),
						sampler: webgl.getUniformLocation(shaderProgram, pids[0].texture._gl_tx_id),
						cmx_mul: null,
						cmx_add: null
  					},
  					{
				    matrix: webgl.getUniformLocation(shaderProgram, 'textureMatrixTo'),
				    alpha: webgl.getUniformLocation(shaderProgram, '_alpha_to'),
						sampler: webgl.getUniformLocation(shaderProgram, pids[1].texture._gl_tx_id),
						cmx_mul: null,
						cmx_add: null
  					}
  				]
			};
			transition.gl_program = prog_info;

			webgl.useProgram(prog_info.program);
		  	webgl.uniformMatrix4fv(prog_info.projectionMatrix, false, defaultOrthoProjectionMatrix.m);

		 	if (pids[0].texture._gl_cmx) {
		  		prog_info.textures[0].cmx_mul = webgl.getUniformLocation(shaderProgram, 'cmx1_mul');
		  		prog_info.textures[0].cmx_add = webgl.getUniformLocation(shaderProgram, 'cmx1_add');
		 	}
		 	if (pids[1].texture._gl_cmx) {
		  		prog_info.textures[1].cmx_mul = webgl.getUniformLocation(shaderProgram, 'cmx2_mul');
		  		prog_info.textures[1].cmx_add = webgl.getUniformLocation(shaderProgram, 'cmx2_add');
		 	}

			tx_slot = 0;
		 	for (let i=0; i<2; i++) {
	  			let pid = pids[i];
			 	tx_slot += pid.texture._gl_texture.nb_textures;
	  		}
			transition.mod.setup_gl(webgl, shaderProgram, prog_info.ratio, tx_slot);
  		}

		webgl.useProgram(prog_info.program);
		webgl.viewport(0, 0, video_width, video_height);

	 	//set transform
  		const modelViewMatrix = new evg.Matrix(matrix);
		webgl.uniformMatrix4fv(prog_info.modelViewMatrix, false, modelViewMatrix.m);

		webgl.uniform1f(prog_info.ratio, ratio);

		tx_slot = 0;
		//set alpha and texture transforms per pid - 2 max
		for (let i=0; i<2; i++) {
	  		let pid = pids[i];

		 	let mx2d = pid.texture._gl_mx || null;
	  		const txmx = mx2d ? new evg.Matrix(mx2d) : new evg.Matrix();
			webgl.uniformMatrix4fv(prog_info.textures[i].matrix, false, txmx.m);

	    	let alpha = pid.texture._gl_alpha || 1;
		 	webgl.uniform1f(prog_info.textures[i].alpha, alpha);

      		if (pid.texture._gl_cmx) {
				gl_set_color_matric(pid.texture._gl_cmx, prog_info.textures[i].cmx_mul, prog_info.textures[i].cmx_add);
      		}

			webgl.activeTexture(webgl.TEXTURE0 + tx_slot);
			//and bind our named texture (this will setup active texture slots)
			webgl.bindTexture(webgl.TEXTURE_2D, pid.texture._gl_texture);

			tx_slot += pid.texture._gl_texture.nb_textures;
		}

	  	//apply transition, ie set uniforms
		transition.mod.apply(webgl, ratio, path, matrix, pids);

		mesh.draw(webgl, prog_info.vertexPosition, prog_info.textureCoord);
	  	webgl.useProgram(null);
	  	return;
	}

	return;
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
varying vec2 vTextureCoord;
uniform mat4 textureMatrix;
void main() {
  gl_Position = uProjectionMatrix * uModelViewMatrix * aVertexPosition;
	vTextureCoord = vec2(textureMatrix * vec4(aTextureCoord.x, aTextureCoord.y, 0, 1) );
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


const fs_source = `
varying vec2 vTextureCoord;
uniform sampler2D maintx;
uniform float alpha;
void main(void) {
	vec4 col = texture2D(maintx, vTextureCoord);
	col.a *= alpha;
  gl_FragColor = col;
}
`;


const fs_source_cmx = `
varying vec2 vTextureCoord;
uniform sampler2D maintx;
uniform float alpha;
uniform mat4 cmx_mul;
uniform vec4 cmx_add;
void main(void) {
	vec4 col = texture2D(maintx, vTextureCoord);
	col = cmx_mul * col;
	col += cmx_add;
	col = clamp(col, 0.0, 1.0);
	col.a *= alpha;
  gl_FragColor = col;
}
`;

const fs_const_col = `
uniform vec4 color;
void main(void) {
  gl_FragColor = color;
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
    webgl.deleteShader(shader);
    return null;
  }
  return shader;
}

function webgl_init_shaders(vsSource, fsSource, tx_name) {
  const vertexShader = webgl_load_shader(webgl.VERTEX_SHADER, vsSource, null);
  const fragmentShader = webgl_load_shader(webgl.FRAGMENT_SHADER, fsSource, tx_name);

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
    uniformLocations: {
      projectionMatrix: webgl.getUniformLocation(shaderProgram, 'uProjectionMatrix'),
      modelViewMatrix: webgl.getUniformLocation(shaderProgram, 'uModelViewMatrix'),
    },
  };

  if (tx_name) {
  	prog.attribLocations.textureCoord = webgl.getAttribLocation(shaderProgram, 'aTextureCoord');
  	prog.uniformLocations.mainTx = webgl.getUniformLocation(shaderProgram, tx_name);
    prog.uniformLocations.txMatrix = webgl.getUniformLocation(shaderProgram, 'textureMatrix');
    prog.uniformLocations.alpha = webgl.getUniformLocation(shaderProgram, 'alpha');
  } else {
    prog.uniformLocations.color = webgl.getUniformLocation(shaderProgram, 'color');
  }

  //load default texture matrix (identity)
  webgl.useProgram(shaderProgram);
	let ident = new evg.Matrix();
	webgl.uniformMatrix4fv(prog.uniformLocations.modelViewMatrix, false, ident.m);
  webgl.uniformMatrix4fv(prog.uniformLocations.projectionMatrix, false, defaultOrthoProjectionMatrix.m);
  if (tx_name) {
	  webgl.uniformMatrix4fv(prog.uniformLocations.txMatrix, false, ident.m);
	  webgl.uniform1f(prog.uniformLocations.alpha, 1.0);
	}
  webgl.useProgram(null);

  return prog;
}


function setup_transition(seq)
{
	/*builtin parameters*/
	seq.transition.fun = (typeof seq.transition_effect.fun == 'string') ? seq.transition_effect.fun : null;

	//parse all params
	seq.transition.options.forEach(o => {
		seq.transition.mod[o.name] = o.value;
		let typ = typeof seq.transition_effect[o.name];
		if (typ != 'undefined') {
			if (typ == typeof o.value) {
				seq.transition.mod[o.name] = seq.transition_effect[o.name];
			} else {
				print(GF_LOG_WARNING, 'Type mismatch for transition ' + seq.transition.type + '.' + o.name + ': got ' + typ + ', expecting ' + typeof o.value + ', ignoring');
			}
		}
	});


	for (var propertyName in seq.transition_effect) {
		if (propertyName == 'type') continue;
		if (propertyName == 'dur') continue;
		if (propertyName == 'fun') continue;
		if (propertyName.charAt(0) == '_') continue;
		if (typeof seq.transition.mod[propertyName] != 'undefined') continue;

		print(GF_LOG_WARNING, 'Unrecognized transition option ' + propertyName + ' for ' + seq.transition.type);
	}
	seq.transition.gl_program = null;
	if (!use_gpu)
		seq.transition.mod.setup();
}

function default_transition(canvas, ratio, path, matrix, pids)
{
		canvas_draw(path, matrix, pids[0].texture);
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
		  		seq.transition.mod = obj.load();

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
		let s = get_source(arguments[0], null);
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

