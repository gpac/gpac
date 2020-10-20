
/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020
 *					All rights reserved
 *
 *  This file is part of GPAC / vout default ui
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

/*very simple controller for vout and vout+aout allowing stats display and playback control */

import * as evg from 'evg'
import { Sys as sys } from 'gpaccore'

_gpac_log_name="";

print("Type 'h' in window for command list");

let aout = null
let vout = null

let i;
for (i=0; i< session.nb_filters; i++) {
 	let f = session.get_filter(i);
 	if (f.type==="aout") aout = f;
 	else if (f.type==="vout") vout = f;
}

let speed=0;
let drop=0;
let paused=0;
let duration=-1
let fullscreen = false;

let audio_only = false;
let ol_buffer = null;
let ol_canvas = null;
let ol_width = 200;
let ol_height = 20;

let text = new evg.Text();
text.font = 'SERIF';
text.fontsize = 20;
text.baseline = GF_TEXT_BASELINE_HANGING;
text.align = GF_TEXT_ALIGN_LEFT;
text.lineSpacing = 20;

const OL_NONE = 0;
const OL_HELP = 1;
const OL_STATS = 2;
const OL_PLAY = 3;
let overlay_type = OL_NONE;

let brush = new evg.SolidBrush();
brush.set_color('white');

let disp_size = null;
let task_reschedule = 500;

function setup_overlay()
{
	let target_width=ol_width;
	let target_height=ol_height;

	disp_size = vout.get_arg('owsize');
	if (disp_size==null) return false;
	if (audio_only && !disp_size.x) {
		disp_size = {"x": 320, "y": 80};
	}

	task_reschedule = 500;
	switch (overlay_type) {
	case OL_HELP:
		target_width = Math.floor(disp_size.x/2);
		target_height = Math.floor( (shortcuts.length+2) * 30);
		break;
	case OL_PLAY:
		target_width = Math.floor(disp_size.x/2);
		target_height = Math.floor( 60);
		task_reschedule = 500;
		break;
	case OL_STATS:
		target_width = Math.floor(disp_size.x/2);
		target_height = Math.floor(disp_size.y/2);
		task_reschedule = 50;
		break;
	default:
		if (!ol_width)
				ol_width = Math.floor(disp_size.x/3);
		if (!ol_height)
			ol_height = Math.floor(disp_size.y/4);
		break;
	}
	init_wnd=true;

	if (audio_only) {
		target_width = ol_width = disp_size.x;
		target_height = ol_height = disp_size.y;
	}
	while (target_width%2) target_width--;
	while (target_height%2) target_height--;

	if ((ol_width!=target_width) || (ol_height!=target_height)) {
		ol_buffer = null;
		ol_width = target_width;
		ol_height = target_height;
	}
	if (ol_buffer) return true;
	ol_buffer = new ArrayBuffer(ol_width*ol_height*4);
	ol_canvas = new evg.Canvas(ol_width, ol_height, 'rgba', ol_buffer);
	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);
	return true;
}

if (vout) {
	speed = vout.get_arg('speed');
	drop = vout.get_arg('drop');
	fullscreen = vout.get_arg('fullscreen');

	session.post_task( () => {
		if (session.last_task) {
			return false;
		}
		if (vout.nb_ipid) {
			audio_only=false;
			return false;
		}
		if (aout) {
			if (!aout.nb_ipid || !session.connected) {
				return 100;
			}

			if (!audio_only) {
				overlay_type=OL_PLAY;
				audio_only=true;
				toggle_overlay();
			}
		}
		return false;
	}, "check_vout_pids");

}

session.set_event_fun( (evt)=> {
	if (evt.type != GF_FEVT_USER) return 0;

	switch (evt.ui_type) {
	case GF_EVENT_MOUSEDOWN:
		break;

	case GF_EVENT_MOUSEUP:
		if (overlay_type==OL_PLAY) 
			if (prog_interact(evt)) return 0;
		if (ol_visible) {
			toggle_overlay();
		} else if (!audio_only) {
			overlay_type=OL_PLAY;
			toggle_overlay();
		}
		break;

	case GF_EVENT_KEYDOWN:
		process_keyboard(evt);
		break;
	case GF_EVENT_SIZE:
		if (ol_visible) {
			let old_type = overlay_type;
			toggle_overlay();
			overlay_type = old_type;
			toggle_overlay();
			vout.update('oldata', ol_buffer);
		}
		break;
	default:
	}
	return 0;
}
);


function check_duration()
{
	if (duration!=-1) return (duration>=0) ? true : false;
	let dur;
	if (audio_only)
		dur = aout.ipid_props(0, 'Duration');
	else
		dur = vout.ipid_props(0, 'Duration');
	if (dur==null) return;
	duration = dur.n;
	duration /= dur.d;
	return true;
}

function do_seek(val, mods, absolute)
{
	if (! check_duration())
		return;

	let seek_to=-1;
	if (!absolute) {
		if (mods & GF_KEY_MOD_CTRL) val *= 30; 
		else if (mods & GF_KEY_MOD_SHIFT) val *= 10;
		seek_to = duration * val / 100;
	}
	//in seconds
	else {
		//30smin
		if (mods & GF_KEY_MOD_CTRL) val *= 1800;
		//10min
		else if (mods & GF_KEY_MOD_SHIFT) val *= 600;
		//30s
		else val *= 30;
		seek_to = val;
	}


	let last_ts_f = vout.last_ts_drop;
	if (last_ts_f==null) return;

	let last_ts = last_ts_f.n;
	last_ts /= last_ts_f.d;
	last_ts += seek_to;	
	if (last_ts<0) last_ts = 0;
	else if (last_ts>duration) last_ts = duration-10;
	
	vout.update('start', ''+last_ts);
}

let init_wnd=false;
function update_help()
{
	let args = ['Shortcuts for vout', '  '];

	shortcuts.forEach( (key) => {
		args.push( '' + sys.keyname(key.code) + ': ' + key.desc);
	});
	if (audio_only)
		text.fontsize = 20;
	else
		text.fontsize = Math.floor(0.6*ol_height/args.length);
	text.set_text(args);

	if (init_wnd && audio_only) {
		let txtdim = text.measure();

		ol_width = Math.floor(txtdim.width*1.2);
		while (ol_width%2) ol_width--;
		ol_height = Math.floor(txtdim.height*1.2);
		while (ol_height%2) ol_height--;
		ol_buffer = new ArrayBuffer(ol_width*ol_height*4);
		ol_canvas = new evg.Canvas(ol_width, ol_height, 'rgba', ol_buffer);
		vout.update('olsize', ''+ol_width+'x'+ol_height);
		vout.update('olwnd', '0x0x'+ol_width+'x'+ol_height);
		init_wnd=false;
	}

	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);
	let mx = new evg.Matrix2D();
	mx.translate(-ol_width/2, ol_height/3);
	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	ol_canvas.fill(brush);


	overlay_type=OL_NONE;
	vout.update('oldata', ol_buffer);
}

let progress_bar_path=null;
let prog_ox=0;
let prog_length=0;
let reverse=false;

function prog_interact(evt)
{
	let ret = audio_only ? true : false;

	let x = evt.mouse_x - disp_size.x/2;
	let y = disp_size.y - evt.mouse_y - ol_height/2;

	if (x<-ol_width/2) return ret;
	if (x>ol_width/2) return ret;
	if (y>ol_height/2) return ret;

	if (!progress_bar_path.point_over(x, y)) {
		x -= prog_ox;
		x /= prog_length;
		x += 0.5;
		if (x>1) reverse = !reverse;
		return true;
	}

	x -= prog_ox;
	x /= prog_length;
	x += 0.5;
	x *= duration;
	if (audio_only) {
		aout.update('start', '' + x);
	} else {
		vout.update('start', '' + x);
		if (aout) aout.update('start', '' + x);
	}
	return true;
}

function update_play()
{
	if (init_wnd) {
		let pos = '0x' + Math.floor(ol_height/2 - disp_size.y/2) + 'x'+ol_width+'x'+ol_height;
		vout.update('olwnd', pos);
		init_wnd=false;
		progress_bar_path=null;
	}

	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);
	let length = ol_width - 20;
	let last_ts_f = vout.last_ts_drop;
	if (!last_ts_f && aout)
		last_ts_f = aout.last_ts_drop;
	if (!last_ts_f) {
		return;
	}

	let h, m, s;
	let last_ts = last_ts_f.n;
	last_ts /= last_ts_f.d;

	if (reverse) last_ts = duration - last_ts;

	h = Math.floor(last_ts/3600);
	m = Math.floor(last_ts/60 - h*60);
	s = Math.floor(last_ts - h*360) - m*60;
	let str = reverse ? '-' : ' ';

	if (duration>=3600) {
		if (h<10) str+='0';
		str+=h+':';
	}
	if (m<10) str+='0';
	str+=m+':';
	if (s<10) str+='0';
	str+=s;
	let fs = Math.floor(0.5*ol_height);
	if (fs > 60) fs=60;
	text.fontsize = fs; 
	text.set_text(str);
	let mx = new evg.Matrix2D();
	mx.translate(ol_width/2 - str.length * text.fontsize/2, -10);
	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	ol_canvas.fill(brush);
	length -= str.length * text.fontsize/2;
	

	if (check_duration()) {
		if (!progress_bar_path) {
			prog_length = length;
			prog_ox = 10 + (length - ol_width) / 2;
			progress_bar_path = new evg.Path();
			progress_bar_path.rectangle(prog_ox, 0, prog_length, 20, true);
		}
		ol_canvas.matrix = null;
		ol_canvas.path = progress_bar_path;
		brush.set_color('grey');
		ol_canvas.fill(brush);

		let	progress_path = new evg.Path();
		let progress = (last_ts_f.n/last_ts_f.d) / duration;
		brush.set_color('white');
		progress_path.rectangle(prog_ox - prog_length/2, 9, prog_length*progress, 18, false);
		ol_canvas.path = progress_path;
		ol_canvas.fill(brush);
	}

	if (paused || (speed != 1)) {
		text.fontsize = 16; 
		if (paused)
			text.set_text((paused==2) ? 'Step mode' : 'Paused');
		else
			text.set_text('Speed: ' + speed);
		let mx = new evg.Matrix2D();
		mx.translate(0, ol_height/4);
		ol_canvas.matrix = mx;
		ol_canvas.path = text;
		brush.set_color('cyan');
		ol_canvas.fill(brush);
		brush.set_color('white');
	}
	vout.update('oldata', ol_buffer);
}

function build_graph(f, str, str_list, fdone)
{
	let i, j;
	if (!f.nb_opid) {
		str += '->' + f.name;
		str_list.push(str);
		str = '';
		fdone.push(f);
		return;
	}
	if (fdone.indexOf(f)>=0) {
		str += '->' + f.name;
		str_list.push(str);
		str = '';
		return;
	}

	let len=0;
	if (!str.length) str = f.name;
	else str += '->' + f.name;
	text.set_text(str);
	len = text.measure().width;

	for (i=0; i<f.nb_opid; i++) {
		let sinks = f.opid_sinks(i);
		let sub_len=0;
		let sub_str;

		if (i) {
			str=' ';
			let k=0;
			while (1) { 
				text.set_text(str);
				let slen = text.measure().width;
				if (slen >= len) break;
				str += ' ';
			}
		}
		sub_str = str;
		if (f.nb_opid>1) sub_str += '#' + f.opid_props(i, 'name');

		if (!sinks.length) {
			str_list.push(sub_str + ': Not connected');
			continue;
		}

		for (j=0; j<sinks.length; j++) {
			let sf = sinks[j];

			if (!j) {
				text.set_text(sub_str);
				sub_len = text.measure().width;
			} else {
				sub_str=' ';
				let k=0;
				while (1) { 
					text.set_text(sub_str);
					let slen = text.measure().width;
					if (slen >= sub_len) break;
					sub_str += ' ';
				}
			}
			build_graph(sinks[j], sub_str, str_list, fdone);
		}
	}
	fdone.push(f);
}

let stats_translate_y=0;
let stats_graph = [];

function update_stats()
{
	let stats = [];

	let sys_info='CPU: '+sys.process_cpu_usage + ' Mem: ' + Math.floor(sys.process_memory/1000000) + ' MB';
	stats.push(sys_info);
	if (session.http_bitrate) {
		let r = session.http_bitrate;
		sys_info = 'HTTP rate: '; 
		if (r>1000000) sys_info += '' + Math.floor(r/10000) / 100 + ' mbps';
		else if (r>1000) sys_info += '' + Math.floor(r/10) / 100 + ' kbps';
		else sys_info += '' + r + ' bps';
		stats.push(sys_info);
	}
	stats.push('  ');

	let i;
	for (i=0; i< session.nb_filters; i++) {
	 	let f = session.get_filter(i);
	 	//only output filters (no out, single in) 
	 	if ((f.nb_ipid!=1) || f.nb_opid) continue;

		let str = f.streamtype;
		let src = f.ipid_source(0);
		str += ' ' + src.name;
		let decrate = src.pck_done;
		if (!decrate) decrate = src.pck_sent + src.pck_ifce_sent;
		decrate /= src.time/1000000;
		decrate = Math.floor(decrate);
		str += ' (' + decrate + ' f/s)';
		let p = f.ipid_props(0, 'Width');
		if (p) {
			str += ' ' + p;
			p = f.ipid_props(0, 'Height');
			str += 'x' + p;
			p = f.ipid_props(0, 'FPS');
			if (p) {
				let fps = Math.floor(p.n/p.d);
				if (fps * p.d == p.n)
					str += '@' + fps;
				else
					str += '@' + p.n + '/'+p.d;
			}
			p = f.ipid_props(0, 'SAR');
			if (p) str += ' SAR ' + p.n + '/' + p.d;			
		} else {
			p = f.ipid_props(0, 'SampleRate');
			if (p) {
				str += ' ' + p;
				p = f.ipid_props(0, 'Channels');
				if (p) str += ' ' + p;
				p = f.ipid_props(0, 'ChannelLayout');
				if (p) str += ' ' + p;
			} 
		}

		str += ' buffer ' + Math.floor(f.ipid_props(0, 'buffer')/1000) + ' ms';
		stats.push(str);
	}

	stats.push(' ');

	//recompute graph only when initializing the window
	if (init_wnd)
		stats_graph = [];

	if (! stats_graph.length) {
		let fdone = [];
		//browse all sources, and build graph
		for (i=0; i< session.nb_filters; i++) {
		 	let f = session.get_filter(i);
		 	//source filter: no input and outputs
		 	if (f.nb_ipid || !f.nb_opid) continue;

		 	build_graph(f, '', stats_graph, fdone);
		}
		let not_done = [];
		for (i=0; i< session.nb_filters; i++) {
		 	let f = session.get_filter(i);
		 	if (f.alias) continue;

		 	if (fdone.indexOf(f)<0)
		 		not_done.push(f);
		 }
		 if (not_done.length) {
			stats.push(' ');
		 	let str = "Filters not connected:";
		 	not_done.forEach(f => { str += ' '+f.name;});
			stats.push(str);
		 }
	 }

	 //push graph to stats string array
	 stats_graph.forEach(str => { stats.push(str);});


	text.fontsize = 20; 
	text.set_text(stats);

	if (init_wnd) {
		let txtdim = text.measure();

		ol_width = Math.floor(txtdim.width*1.2);
		while (ol_width%2) ol_width--;
		ol_height = Math.floor(txtdim.height*1.4);
		while (ol_height%2) ol_height--;
		ol_buffer = new ArrayBuffer(ol_width*ol_height*4);
		ol_canvas = new evg.Canvas(ol_width, ol_height, 'rgba', ol_buffer);
		vout.update('olsize', ''+ol_width+'x'+ol_height);
		stats_translate_y = txtdim.height/2;

		let pos;
		if (audio_only)
			pos = '0x0x'+ol_width+'x'+ol_height;
		else
			pos = ''+ Math.floor(ol_width/2 - disp_size.x/2) + 'x' + Math.floor(ol_height/2 - disp_size.y/2) + 'x'+ol_width+'x'+ol_height;
		vout.update('olwnd', pos);
		init_wnd=false;
	}

	ol_canvas.clearf(0.2, 0.2, 0.2, 0.6);

	let mx = new evg.Matrix2D();
	mx.translate(-ol_width/2, stats_translate_y);
	ol_canvas.matrix = mx;
	ol_canvas.path = text;
	ol_canvas.fill(brush);
	vout.update('oldata', ol_buffer);
}


function update_overlay()
{
	if (overlay_type==OL_NONE) return;

	//help screen
	if (overlay_type==OL_HELP) {
		update_help();
		return;
	}
	//play screen
	if (overlay_type==OL_PLAY) {
		update_play();
	}
	//play screen
	if (overlay_type==OL_STATS) {
		update_stats();
	}
}

let ol_visible=false;
let oltask_scheduled=false;

function toggle_overlay()
{
	ol_visible = !ol_visible;
	if (!ol_visible) {
		vout.update('oldata', null);
		overlay_type=OL_NONE;
		return;
	}
	if (!setup_overlay())
		return;

	vout.update('olwnd', '0x0x'+ol_width+'x'+ol_height);
	vout.update('olsize', ''+ol_width+'x'+ol_height);
	vout.update('oldata', ol_buffer);
	if (!oltask_scheduled) {
		session.post_task( () => {
			oltask_scheduled=false;
			if (audio_only && aout.ipid_props(0, 'eos') ) {
				vout.update('oldata', null);
				return false;
			}
			if (session.last_task) {
				return false;
			}
			update_overlay();
			if (!ol_visible) return false;
			if (overlay_type==OL_NONE) return false;
			oltask_scheduled=true;
			return task_reschedule;
		}, "overlay_update");
	}
}

let shortcuts = [
	{ "code": GF_KEY_B, "desc": "speeds up by 2"},
	{ "code": GF_KEY_V, "desc": "speeds down by 2"},
	{ "code": GF_KEY_N, "desc": "normal play speed"},
	{ "code": GF_KEY_SPACE, "desc": "pause/resume"},
	{ "code": GF_KEY_S, "desc": "step one frame"},
	{ "code": GF_KEY_RIGHT, "desc": "seek forward by 1%, 10% if alt down, 30% if ctrl down"},
	{ "code": GF_KEY_LEFT, "desc": "seek backward "},
	{ "code": GF_KEY_UP, "desc": "seek forward by 30s, 10m if alt down, 30m if ctrl down"},
	{ "code": GF_KEY_DOWN, "desc": "seek backward"},
	{ "code": GF_KEY_F, "desc": "fullscreen mode"},
	{ "code": GF_KEY_I, "desc": "show info and statistics"},
	{ "code": GF_KEY_P, "desc": "show playback info"},
];

function set_speed(speed)
{
	if (audio_only) aout.update('speed', ''+speed);
	else {
		vout.update('speed', ''+speed);
		if (aout) aout.update('speed', ''+speed);

		if (speed>2) vout.update('drop', '1');
		else if (speed<=1) vout.update('drop', ''+drop);

	}
}

function process_keyboard(evt)
{
	switch (evt.keycode) {
	case GF_KEY_B:
		if (evt.keymods & GF_KEY_MOD_SHIFT) speed *= 1.2;
		else speed *= 2;
		set_speed(speed);
		return;
	case GF_KEY_V:
		if (evt.keymods & GF_KEY_MOD_SHIFT) speed /= 1.2;
		else speed /= 2;
		set_speed(speed);
		return;
	case GF_KEY_N:
		speed = 1;
		set_speed(speed);
		return;
	case GF_KEY_SPACE:
		paused = !paused;
		set_speed(paused ? 0 : speed);
		return;
	case GF_KEY_S:
		paused = 2;
		vout.update('step', '1');
		if (aout) aout.update('speed', '0');
		check_duration();
		return;
	case GF_KEY_RIGHT:
		do_seek(1, evt.keymods, false);
		return;
	case GF_KEY_LEFT:
		do_seek(-1, evt.keymods, false);
		return;
	case GF_KEY_UP:
		do_seek(1, evt.keymods, true);
		return;
	case GF_KEY_DOWN:
		do_seek(-1, evt.keymods, true);
		return;

	case GF_KEY_F:
		if (audio_only) return;
		fullscreen = !fullscreen;
		vout.update('fullscreen', ''+fullscreen);
		return;

	case GF_KEY_H:
		//hide player
		if (audio_only && (overlay_type==OL_PLAY)) {
			toggle_overlay();
		}
		overlay_type=OL_HELP;
		toggle_overlay();
		//show player player
		if (!ol_visible && audio_only) {
			overlay_type=OL_PLAY;
			toggle_overlay();
		}
		return;
	case GF_KEY_I:
		//hide player
		if (audio_only && (overlay_type==OL_PLAY)) {
			toggle_overlay();
		}
		overlay_type=OL_STATS;
		toggle_overlay();
		//show player player
		if (!ol_visible && audio_only) {
			overlay_type=OL_PLAY;
			toggle_overlay();
		}
		return;
	case GF_KEY_P:
		//do not untoggle for audio only
		if (audio_only) return;
		overlay_type=OL_PLAY;
		if (!ol_visible) init_wnd=true;
		toggle_overlay();
		return;
	default:
		break;
	}
}
