import * as evg from 'evg'

export const description = `Draw video, shape, text, shape outline...`;

export const help = `The default scene handler can be used to setup a shape, its outline and specify the fill and strike modes.
Supported shapes include:
- a variety of rectangles, ellipse and other polygons
- custom paths specified from JS
- text

The color modes for shapes and outlines include:
- texturing using data from input media streams (shape fill only)
- texturing using local JPEG and PNG files (shape fill only)
- solid color
- linear and radial gradients

The default scene is optimized to fallback to fast blit when no transformations are used on a straight rectangle shape.

All options can be updated at run time.

Color replacement operations can be specified for base scenes using source videos by specifying the \`replace\` option. The replacement source is:
- the image data if \`img\` is set, potentially altered using \`keep_ar_rep\`, \`txmx_rep\` and \`cmx_rep\` options
- otherwise a linear gradient if \`fill=linear\` or a radial gradient if \`fill=radial\`

## Text options 

Text can be loaded from file if \`text[0]\` is an existing local file.
By default all lines are loaded. The number of loaded lines can be specified using \`text[1]\` as follows:
- 0 or not present: all lines are loaded
- N > 0: only keep the last N lines
- N < 0: only keep the first N lines

Text loaded from file will be refreshed whenever the file is modified.

Predefined keywords can be used in input text, identified as \`$KEYWORD$\`. The following keywords (case insensitive) are defined:
- time: replaced by UTC date
- ltime: replaced by locale date
- date: replaced by date (Y/M/D)
- ldate: replaced by locale date (Y/M/D)
- mtime: replaced by output media time 
- mtime_SRC: replaced by media time of input source \`SRC\` 
- cpu: replaced by current CPU usage of process
- mem: replaced by current memory usage of process
- version: replaced by GPAC version
- fversion: replaced by GPAC full version

## Custom paths

Custom paths (shapes) can be created through JS code indicated in \'shape\', either inline or through a file.
The following GPAC JS modules are imported:
 - \`Sys\` as \`sys\`
 - All EVG as \`evg\`
 - \`os\` form QuickJS

See [https://doxygen.gpac.io]() for more information on EVG and Sys JS APIs.

The code is exposed the scene as \`this\`. The variable \`this.path\` is created, representing an empty path.
EX "shape": "this.path.add_rectangle(0, 0, this.width, this.height); let el = new evg.Path().ellipse(0, 0, this.width, this.height/3); this.path.add_path(el);"

The following global can be used:
 - get_media_time(): return media time in seconds (float) of output
 - get_media_time(SRC): get time of source with id \`SRC\`, return -4 if not found, -3 if not playing, -2 if in pre-fetch, -1 if timing not yet known, media time in seconds (float) otherwise
 - current_utc_clock: current UTC time in ms
 - video_time: output video time
 - video_timescale: output video timescale
 - video_width: output video width
 - video_height: output video height

If your path needs to be reevaluated on regular basis, set the value \`this.reload\` to the timeout to next reload, in milliseconds.
`;

const UPDATE_LINE = 1<<3;
const UPDATE_COLOR = 1<<4;

export const options = [
 {name:"rx", value: 0, desc: "horizontal radius for rounded rect in percent of width", dirty: UPDATE_SIZE},
 {name:"ry", value: 0, desc: "vertical radius for rounded rect in percent of height", dirty: UPDATE_SIZE},
 {name:"tl", value: true, desc: "top-left corner rounded", dirty: UPDATE_SIZE},
 {name:"bl", value: true, desc: "bottom-left corner rounded", dirty: UPDATE_SIZE},
 {name:"tr", value: true, desc: "top-right corner rounded", dirty: UPDATE_SIZE},
 {name:"br", value: true, desc: "bottom-right corner rounded", dirty: UPDATE_SIZE},
 {name:"rs", value: false, desc: "repeat texture horizontally", dirty: UPDATE_SIZE},
 {name:"rt", value: false, desc: "repeat texture vertically", dirty: UPDATE_SIZE},
 {name:"keep_ar", value: true, desc: "keep aspect ratio", dirty: UPDATE_POS},
 {name:"pad_color", value: "black", desc: "color to use for texture padding, use 'none' for transparent (always enforced if source is transparent). Warning: Not supported in GPU mode", dirty: UPDATE_POS},
 {name:"txmx", value: [], desc: "texture matrix", dirty: UPDATE_POS},
 {name:"cmx", value: [], desc: "color transform - all 20 coefficients must be set in order, i.e [Mrr, Mrg, Mrb, Mra, Tr, Mgr, Mgg ...]", dirty: UPDATE_POS},
 {name:"line_width", value: 0, desc: "line width in percent of width if positive, or absolute value if negative", dirty: UPDATE_LINE},
 {name:"line_color", value: "white", desc: "line color, `linear` for linear gradient and `radial` for radial gradient", dirty: UPDATE_LINE},
 {name:"line_pos", value: 'center', desc: `line/shape positioning. Possible values are:
  - center: line is centered around shape
  - outside: line is outside the shape
  - inside: line is inside the shape`, dirty: UPDATE_LINE},
 {name:"line_dash", value: 'plain', desc: `line dashing mode. Possible values are:
  - plain: no dash
  - dash: predefined dash pattern is used
  - dot:  predefined dot pattern is used
  - dashdot:  predefined dash-dot pattern is used
  - dashdashdot:  predefined dash-dash-dot pattern is used
  - dashdotdot:  predefined dash-dot-dot pattern is used`, dirty: UPDATE_LINE},
 {name:"dashes", value:[], desc: "dash/dot pattern lengths for custom dashes (these will be multiplied by line size)", dirty: UPDATE_LINE},
 {name:"cap", value: 'flat', desc: `line end style. Possible values are:
  - flat: flat end
  - round: round end
  - square: square end (extends limit compared to flat)
  - triangle: triangle end`, dirty: UPDATE_LINE},
 {name:"join", value: 'miter', desc: `line joint style. Possible values are:
  - miter: miter join (straight lines)
  - round: round join
  - bevel: bevel join
  - bevelmiter: bevel+miter join`, dirty: UPDATE_LINE},
 {name:"miter_limit", value: 2, desc: "miter limit for joint styles", dirty: UPDATE_LINE},
 {name:"dash_length", value: -1.0, desc: "length of path to outline, negative values mean full path", dirty: UPDATE_LINE},
 {name:"dash_offset", value: 0.0, desc: "offset in path at which the outline starts", dirty: UPDATE_LINE},
 {name:"blit", value: true, desc: "use blit if possible, otherwise EVG texturing. If disabled, always use texturing", dirty: UPDATE_SIZE},
 {name:"fill", value: "none", desc: "fill color if used without sources, `linear` for linear gradient and `radial` for radial gradient", dirty: UPDATE_COLOR},
 {name:"img", value: "", desc: "path to local image (JPG or PNG) to use as texture if without sources", dirty: UPDATE_SIZE},
 {name:"alpha", value: 1, desc: "global texture transparency", dirty: UPDATE_COLOR},
 {name:"replace", value: "", desc: `if \`img\` or \`fill\` is set and shape is using source, replace source component by \`img\` component (\`a\`, \`r\`, \`g\` or \`b\`). If prefix \`-\` is set, replace by one minus the source component`, dirty: UPDATE_SIZE},
 {name:"shape", value: "rect", desc: `shape type. Possible values are:
  - rect: rounded rectangle
  - square: square using smaller width/height value
  - ellipse: ellipse
  - circle: circle using smaller width/height value
  - rhombus: axis-aligned rhombus
  - text: force text mode even if text field is empty
  - rects: same as rounded rectangle but use straight lines for corners
  - other value: JS code for custom path creation, either string or local file name (dynamic reload possible)`, dirty: UPDATE_SIZE},
 {name:"grad_p", value: [], desc: "gradient positions between 0 and 1", dirty: UPDATE_COLOR},
 {name:"grad_c", value: [], desc: "gradient colors for each position, as strings", dirty: UPDATE_COLOR},
 {name:"grad_start", value: [0, 0], desc: "start point for linear gradient or center point for radial gradient", dirty: UPDATE_COLOR},
 {name:"grad_end", value: [0, 0], desc: "end point for linear gradient or radius value for radial gradient", dirty: UPDATE_COLOR},
 {name:"grad_focal", value: [0, 0], desc: "focal point for radial gradient", dirty: UPDATE_COLOR},
 {name:"grad_mode", value: 'pad', desc: `gradient mode. Possible values are:
  - pad: color padding outside of gradient bounds
  - spread: mirror gradient outside of bounds
  - repeat: repeat gradient outside of bounds`, dirty: UPDATE_COLOR},
 {name:"text", value: [], desc: "text lines (UTF-8 only). If not empty, force \`shape=text\`", dirty: UPDATE_SIZE},
 {name:"font", value: ["SANS"], desc: "font name(s)", dirty: UPDATE_SIZE},
 {name:"size", value: 20, desc: "font size in percent of height (horizontal text), width (vertical text) or absolute value if negative", dirty: UPDATE_SIZE},
 {name:"baseline", value: 'alphabetic', desc: `baseline position. Possible values are:
  - alphabetic: alphabetic position of baseline
  - top: baseline at top of EM Box
  - hanging: reserved, __not implemented__
  - middle: baseline at middle of EM Box
  - ideograph: reserved, __not implemented__
  - bottom: baseline at bottom of EM Box`, dirty: UPDATE_SIZE},
 {name:"align", value: 'center', desc: `horizontal text alignment. Possible values are:
  - center: center of shape
  - start: start of shape (left or right depending on text direction)
  - end: end of shape (right or left depending on text direction)
  - left: left of shape
  - right: right of shape`, dirty: UPDATE_SIZE},
 {name:"spacing", value: 0, desc: "line spacing in percent of height (horizontal text), width (vertical text) or absolute value if negative", dirty: UPDATE_SIZE},
 {name:"bold", value: false, desc: "use bold version of font", dirty: UPDATE_SIZE},
 {name:"italic", value: false, desc: "use italic version of font", dirty: UPDATE_SIZE},
 {name:"underline", value: false, desc: "underline text", dirty: UPDATE_SIZE},
 {name:"vertical", value: false, desc: "draw text vertically", dirty: UPDATE_SIZE},
 {name:"flip", value: false, desc: "flip text vertically", dirty: UPDATE_SIZE},
 {name:"extend", value: 0, desc: "maximum text width in percent of width (for horizontal) or height (for vertical)", dirty: UPDATE_SIZE},

 {name:"keep_ar_rep", value: true, desc: "same as `keep_ar`, applies to local image in replace mode", dirty: UPDATE_POS},
 {name:"txmx_rep", value: [], desc: "same as `txmx`, applies to local image in replace mode", dirty: UPDATE_POS},
 {name:"cmx_rep", value: [], desc: "same as `cmx`, applies to local image in replace mode", dirty: UPDATE_POS},

 {}
];


function make_rounded_rect(is_straight)
{
  var hw, hh, rx_bl, ry_bl, rx_br, ry_br, rx_tl, ry_tl, rx_tr, ry_tr, rx, ry;
 
  hw = this.width / 2;
  hh = this.height / 2;

  /*compute default rx/ry*/
  rx = this.rx * this.width / 100;
  ry = this.ry * this.height / 100;

  if (rx >= hw) rx = hw;
  if (ry >= hh) ry = hh;
  rx_bl = rx_br = rx_tl = rx_tr = rx;
  ry_bl = ry_br = ry_tl = ry_tr = ry;
  if (!this.tl) rx_tl = ry_tl = 0;
  if (!this.tr) rx_tr = ry_tr = 0;
  if (!this.bl) rx_bl = ry_bl = 0;
  if (!this.br) rx_br = ry_br = 0;

  this.path.move_to(hw - rx_tr, hh);

  if (is_straight)
    this.path.line_to(hw, hh - ry_tr);
  else
    this.path.quadratic_to(hw, hh, hw, hh - ry_tr);

  this.path.line_to(hw, -hh + ry_br);

  if (is_straight)
    this.path.line_to(hw - rx_br, -hh);
 else
    this.path.quadratic_to(hw, -hh, hw - rx_br, -hh);

  this.path.line_to(-hw + rx_bl, -hh);

  if (is_straight)
    this.path.line_to(-hw, -hh + ry_bl);
  else
    this.path.quadratic_to(-hw, -hh, -hw, -hh + ry_bl);

  this.path.line_to(-hw, hh - ry_tl);

  if (is_straight)
    this.path.line_to(-hw + rx_tl, hh);
  else
    this.path.quadratic_to(-hw, hh, -hw + rx_tl, hh);

  this.path.close();
}

function create_brush(color)
{
   let stencil = null;

   if (color=='none') return null;

   if ((color=='linear') || (color=='radial') ) {
      let s_x, s_y, e_x, e_y, f_x, f_y;
      s_x = this.grad_start[0];
      s_y = this.grad_start[1];
      e_x = this.grad_end[0];
      e_y = this.grad_end[1];
      f_x = this.grad_focal[0];
      f_y = this.grad_focal[1];

      if (color=='linear') {
        if ((s_x==e_x) && (s_y==e_y) && (s_x==0) && (s_y==0)) {
          e_x = 1;
        }
        stencil = new evg.LinearGradient();
        stencil.set_points(s_x, s_y, e_x, e_y);
      } else {
        if ((s_x==e_x) && (s_y==e_y) && (s_x==0) && (s_y==0)) {
          s_x = f_x = 0.5;
          s_y = f_y = 0.5;
          e_x = 0.5;
          e_y = 0.5;
        }

        stencil = new evg.RadialGradient();
        stencil.set_points(s_x, s_y, f_x, f_y, e_x, e_y);
      }
      let maxg = this.grad_p.length;
      if (this.grad_c.length < maxg) maxg = this.grad_c.length;
      for (let i=0; i<maxg; i++) {
        stencil.set_stop(this.grad_p[i], this.grad_c[i]);
      }
      stencil.set_alphaf(this.alpha);
      stencil._gl_alpha = this.alpha;

      if (this.grad_mode == 'pad') stencil.pad = GF_GRADIENT_MODE_PAD;
      else if (this.grad_mode == 'spread') stencil.pad = GF_GRADIENT_MODE_SPREAD;
      else stencil.pad = GF_GRADIENT_MODE_REPEAT;

      let txmx = null;
      if (this.txmx && this.txmx.length==6) {
        txmx = new evg.Matrix2D(this.txmx[0], this.txmx[1], this.txmx[2], this.txmx[3], this.txmx[4], this.txmx[5]);
      } else {
        txmx = new evg.Matrix2D();        
      }
      let mmx = new evg.Matrix2D();
      mmx.add(txmx);
      mmx.scale(this.width, this.height);
      mmx.translate(-this.width/2, -this.height/2);
      mmx.add(this.mx);
      stencil.mx = mmx;

    } else {
      stencil = new evg.SolidBrush();
      stencil.set_color(color);
      stencil.set_alphaf(this.alpha);
      stencil._gl_alpha = this.alpha;
      stencil._gl_color = this.fill;
    }
    return stencil;
}

function setup_texture(pid_link)
{
  let texture = pid_link ? pid_link.texture : this.local_stencil;
  let pid = pid_link ? pid_link.pid : null;
  let txmx_pid = (pid_link || !this.replace_op) ? this.txmx : this.txmx_rep;
  let keep_ar_pid = (pid_link || !this.replace_op) ? this.keep_ar : this.keep_ar_rep;
  let cmx_pid = (pid_link || !this.replace_op) ? this.cmx : this.cmx_rep;

  let txmx;
  if (txmx_pid && Array.isArray(txmx_pid) && (txmx_pid.length==6)) {
    txmx = new evg.Matrix2D(txmx_pid[0], txmx_pid[1], txmx_pid[2], txmx_pid[3], txmx_pid[4], txmx_pid[5]);
  } else {
    txmx = new evg.Matrix2D();        
  }

  let mmx = new evg.Matrix2D();

  let scale_gl_sw = 1;
  let scale_gl_sh = 1;
  let trans_gl_x = 0;
  let trans_gl_y = 0;
  let sw = this.sw;
  let sh = this.sh;
  let sw_o = this.sw;
  let sh_o = this.sh;
  let tx_w = texture.width;
  let tx_h = texture.height;
  let rotated = 0;
  let mirrored = 0;

  if (!txmx.identity && !pid_link && this.replace_op) {    
    texture.repeat_s = true;
    texture.repeat_t = true;
    keep_ar_pid = false;
  } else {
    texture.repeat_s = !txmx.identity ? true : this.rs;
    texture.repeat_t = !txmx.identity ? true : this.rt;
  }

  let mxflip = new evg.Matrix2D();

  if (pid && pid.mirror) {
    mirrored = pid.mirror;
    if (pid.mirror==1) mxflip.scale(-1, 1);
    else if (pid.mirror==2) mxflip.scale(1, -1);
    else if (pid.mirror==3) {
        mxflip.scale(-1, 1);
        mxflip.scale(1, -1);
    }
  }

  if (pid && pid.rotate) {
    rotated = pid.rotate;
    if ((pid.rotate==1) || (pid.rotate==3)) {
      tx_w = texture.height;
      tx_h = texture.width;
    }
    if (pid.rotate==3) mxflip.rotate(0, 0, -Math.PI/2);
    else if (pid.rotate==2) mxflip.rotate(0, 0, Math.PI);
    else if (pid.rotate==1) mxflip.rotate(0, 0, Math.PI/2);
  }

  txmx.add(mxflip);

  if (keep_ar_pid) {
      if (sw * tx_h < sh * tx_w) {
        sh = sw * tx_h / tx_w;
      } else if (sw * tx_h > sh * tx_w) {
        sw = sh * tx_w / tx_h;
      }
  }
  if ((sw != sw_o) || (sh != sh_o)) {
      this.can_reuse = false;
      scale_gl_sw = sw_o / sw;
      trans_gl_x = (sw_o - sw)/sw_o / 2;
      scale_gl_sh = sh_o / sh;
      trans_gl_y = (sh - sh_o)/sh_o / 2;

      if (!this.rs || !this.rt) {
        if (this.pad_color == "none") {
          texture.set_pad_color();
        } else {
          texture.set_pad_color(this.pad_color);
        }
        this.use_blit = false;
      }
  }

  if (!txmx.identity) {
      this.use_blit = false;
      this.can_reuse = false;
  }

  mmx.add(txmx);
  mmx.scale(sw / tx_w, sh / tx_h);
  
  let tx = -sw/2;
  let ty = sh/2;

  if (rotated==1) {
    if (mirrored==1) mmx.translate(tx, ty);
    else if (mirrored==2) mmx.translate(-tx, -ty);
    else if (mirrored==3) mmx.translate(-tx, ty);
    else mmx.translate(tx, -ty);
  }
  else if (rotated==2) {
    if (mirrored==1) mmx.translate(tx, -ty);
    else if (mirrored==2) mmx.translate(-tx, ty);
    else if (mirrored==3) mmx.translate(tx, ty);
    else mmx.translate(-tx, -ty);
  }
  else if (rotated==3) {
    if (mirrored==1) mmx.translate(-tx, -ty);
    else if (mirrored==2) mmx.translate(tx, ty);
    else if (mirrored==3) mmx.translate(tx, -ty);
    else mmx.translate(-tx, ty);
  }
  else {
    if (mirrored==1) mmx.translate(-tx, ty);
    else if (mirrored==2) mmx.translate(tx, -ty);
    else if (mirrored==3) mmx.translate(-tx, -ty);
    else mmx.translate(tx, ty);
  }

  mmx.add(this.mx);
  texture.mx = mmx;


  texture._gl_mx = new evg.Matrix2D();
  texture._gl_mx.add(txmx);
  texture._gl_mx.scale(scale_gl_sw, scale_gl_sh);
  texture._gl_mx.translate(trans_gl_x, trans_gl_y);

  if (pid) {
    if ((tx_w != this.sw) || (tx_h != this.sh)) {
      this.can_reuse=false;
    }
  }
  if (this.alpha < 1) {
    this.use_blit = false;
    this.opaque = false;
    texture.set_alphaf(this.alpha);
  } else {
    texture.set_alphaf(1);
  }
  texture._gl_alpha = this.alpha;

  texture._gl_cmx = null;
  if (cmx_pid && (cmx_pid.length==20)) {
    let cmx = new evg.ColorMatrix(cmx_pid);
    texture.cmx = cmx;
    if (!cmx.identity) {
      this.can_reuse = false;
      this.use_blit = false;
      texture._gl_cmx = cmx_pid;
    }
  }
  else
    texture.cmx = null;
}

function set_text(txt)
{
  let final_text = [];

  txt.forEach( line => {
    if (line.indexOf('$') < 0) {
      final_text.push(line);
      return;
    }
    let first_is_sep = (line.charAt(0)=='$') ? true : false; 
    let last_is_sep = (line.charAt(line.length-1)=='$') ? true : false; 
    let split_text = line.split('$');
    let len = split_text.length;
    let new_line='';

    split_text.forEach( (item, index) => {
      if (!index && !first_is_sep) {
        new_line += item;
        return;
      }
      if (!last_is_sep && (index==len-1)) {
        new_line += item;
        return;
      }
      let timeout=0;
      let kword = item.toLowerCase();
      if (kword=='time') {
        item = new Date().toUTCString();
        timeout = 1000;
      }
      else if (kword=='ltime') {
        item = new Date().toLocaleString();
        timeout = 1000;
      }
      else if (kword=='date') {
        item = new Date().toDateString();
        timeout = 1000 * 3600 * 24;
      }
      else if (kword=='ldate') {
        item = new Date().toLocaleDateString();
        timeout = 1000 * 3600 * 24;
      }
      else if (kword.startsWith('mtime')) {
        let time;
        if (kword.startsWith('mtime_')) {
          let src = item.substring(6);
          time = get_media_time( src );
        } else {
          time = get_media_time();
        }
        if (time == -4) {
          item = 'not found';
        } else if (time==-3) {
          item = 'not playing';
        } else if (time==-2) {
          item = 'initializing';
        } else if (time==-1) {
          item = 'pre-fetching';
        } else {
          time *= 1000;
          let time_s = Math.floor(time/1000);
          let h = Math.floor(time_s/3600);
          let m = Math.floor(time_s/60 - h*60);
          let s = Math.floor((time_s - h*3600 - m*60));
          item = ((h<10) ? '0' : '') + h + ((m<10) ? ':0' : ':') + m + ((s<10) ? ':0' : ':') + s + '.';
          time =  Math.floor(time - time_s*1000);
          if (time < 10) item += '00';
          else if (time < 100) item += '0';
          item += time;
        }
        timeout = 30;
      }
      else if (kword=='cpu') {
        item = '' + sys.process_cpu_usage;
        timeout = 500;
      }
      else if (kword=='mem') {
        item = '' + Math.floor(sys.process_memory/1000000) + ' MB';
        timeout = 500;
      }
      else if (kword=='version') {
        item = sys.version;
      }
      else if (kword=='fversion') {
        item = sys.version_full;
      }
      new_line += item;

      if (timeout) { 
        if (!this.reload) this.reload = timeout;
        else if (timeout < this.reload) this.reload = timeout;
      }
    });

    final_text.push(new_line);      
  });
  this.path.set_text(final_text);
}


export function load() { return {

local_stencil: null,
path: null,
mx: null,
last_img: null,
dyn_text: null,
dyn_path: null,
last_path_time: 0,
last_text_time: 0,
reload: 0,

update: function() {

  if (this.update_flag & UPDATE_PID) {
    if (this.pids.length) {
      this.is_texture=true;
    } else {
      this.update_flag = UPDATE_SIZE;
      this.is_texture=false;
    }
  }

  if (this.reload && (this.reload < current_utc_clock)) {    
      this.update_flag = UPDATE_SIZE;
  }
  else if (this.dyn_text && (this.last_text_time < sys.mod_time(this.dyn_text))) {
      this.update_flag = UPDATE_SIZE;
  }
  else if (this.last_path_time && (this.last_path_time < sys.mod_time(this.dyn_path))) {
      this.update_flag = UPDATE_SIZE;
  }

  if (!this.update_flag) return 1;

  this.replace_op = 0;
  this.replace_op_img = false;
  this.replace_par = 0;
  let rep = this.replace;
  let op = GF_EVG_OPERAND_REPLACE_ALPHA;
  if (rep[0]=='-') {
    op = GF_EVG_OPERAND_REPLACE_ONE_MINUS_ALPHA;
    rep = rep.substring(1);
  }

  if (rep=="a") { this.replace_op = op; }
  else if (rep=="r") { this.replace_op = op; this.replace_par = 1; }
  else if (rep=="g") { this.replace_op = op; this.replace_par = 2; }
  else if (rep=="b") { this.replace_op = op; this.replace_par = 3; }

  //no associated sources, use image if set
  let reset_local_stencil = true;
  if ((!this.pids.length || this.replace_op) && this.img.length) {
    if (this.img != this.last_img) this.local_stencil = null;
    this.last_img = this.img;

    if (!this.local_stencil) {
      try {
        let url = resolve_url(this.img);
        this.local_stencil = new evg.Texture(url, false);
        this.local_stencil.filtering = GF_TEXTURE_FILTER_HIGH_QUALITY;
        this.is_texture = true;
        if (this.replace_op) this.replace_op_img = true;
        reset_local_stencil = false;
      } catch (e) {
        if (this.replace_op) {
          print(GF_LOG_WARNING, 'Failed to load image ' + this.img + ': ' + e + ' - ignoring component replacement');
          this.is_texture = false;
          this.replace_op = false;
        } else {
          print(GF_LOG_WARNING, 'Failed to load image ' + this.img + ': ' + e + ' - will use color fill');
          this.is_texture = false;
        }
      }
    }
  }
  if (this.replace_op && !this.img.length) {
    if ((this.fill == 'linear') || (this.fill == 'radial')) {}
    else this.replace_op = 0;
  }

  if (this.update_flag & UPDATE_COLOR) {
    if (!this.local_stencil || !this.local_stencil.solid_brush)
      this.update_flag |= UPDATE_POS;
  }

  //update our objects

  this.opaque = false;
  this.can_reuse = this.pids.length ? true : false;
  this.sw = this.width;
  this.sh = this.height;
  this.is_text = false;

  if (!this.path || (this.update_flag & UPDATE_SIZE)) {
      let path_loaded = false;
      this.path = new evg.Path();

      this.reload = 0;

      if ((this.shape == 'text') || this.text.length) {
        this.is_text = true;

        if ((this.text.length == 1) || (this.text.length == 2)) {
          let url = resolve_url(this.text[0]);
          if (sys.file_exists(url)) {
            this.dyn_text = url;
          }
        }
        /*create a text*/
        this.path = new evg.Text();

        this.path.font = this.font;
        this._font_size = 0;
        if (this.size<0)
          this.path.fontsize = this._font_size = -this.size;
        else
          this.path.fontsize = this._font_size = this.size * (this.vertical ? this.width : this.height) / 100;
  
        if (this.spacing<0)
          this.path.lineSpacing = -this.spacing;
        else
          this.path.lineSpacing = this.spacing * (this.vertical ? this.width : this.height) / 100;
        

        if (this.baseline=='top') this.path.baseline = GF_TEXT_BASELINE_TOP;
        else if (this.baseline=='hanging') this.path.baseline = GF_TEXT_BASELINE_HANGING;
        else if (this.baseline=='middle') this.path.baseline = GF_TEXT_BASELINE_MIDDLE;
        else if (this.baseline=='ideographic') this.path.baseline = GF_TEXT_BASELINE_IDEOGRAPHIC;
        else if (this.baseline=='bottom') this.path.baseline = GF_TEXT_BASELINE_BOTTOM;
        else this.path.baseline = GF_TEXT_BASELINE_ALPHABETIC;

        if (this.align == 'start') this.path.align = GF_TEXT_ALIGN_START;
        else if (this.align == 'end') this.path.align = GF_TEXT_ALIGN_END;
        else if (this.align == 'left') this.path.align = GF_TEXT_ALIGN_LEFT;
        else if (this.align == 'right') this.path.align = GF_TEXT_ALIGN_RIGHT;
        else this.path.align = GF_TEXT_ALIGN_CENTER;

        this.path.bold = this.bold;
        this.path.italic = this.italic;
        this.path.underline = this.underline;
        this.path.horizontal = !this.vertical;
        this.path.flip = this.flip;
        this.path.maxWidth = 0;
        if (this.extend) {
          this.path.maxWidth = this.extend * (this.vertical ? this.height : this.width);
        }

        if (this.dyn_text) {
          let txt = sys.load_file(this.dyn_text, true);
          if (this.text.length==2) {
            txt = txt.split(/\r?\n/);
            let nb_items = this.text[1];
            if (nb_items>0) {
              var n = txt.length - nb_items;
              txt.splice(0, txt.length - nb_items);
            } else {              
              var n = txt.length + nb_items;
              txt.splice(-nb_items, txt.length + nb_items);
            }
          }
          set_text.apply(this, [txt]);
          this.last_text_time = sys.mod_time(this.dyn_text);
        } else {
          set_text.apply(this, [this.text]);
        }

        path_loaded = true;
      } else if (this.shape == 'rect') {

      } else if (this.shape == 'square') {
        if (this.sh<this.sw) this.sw = this.sh;
        else this.sh = this.sw;
        this.path.rectangle(0, 0, this.sw, this.sh, true);
        path_loaded = true;
      } else if (this.shape == 'circle') {
        if (this.sh<this.sw) this.sw = this.sh;
        else this.sh = this.sw;
        this.path.ellipse(0, 0, this.sw, this.sh);
        path_loaded = true;
      } else if (this.shape == 'ellipse') {
        this.path.ellipse(0, 0, this.sw, this.sh);
        path_loaded = true;
      } else if (this.shape == 'rhombus') {
        let hsw = this.sw/2;
        let hsh = this.sh/2;
        this.path.move_to(this.sw, 0);
        this.path.line_to(0, this.sh);
        this.path.line_to(-this.sw, 0);
        this.path.line_to(0, -this.sh);
        path_loaded = true;
      } else if (this.shape == 'rects') {
        path_loaded = true;
        make_rounded_rect.apply(this, [true]);
      } else {
        try {
          let url = resolve_url(this.shape);
          if (sys.file_exists(url)) {
            let f = sys.load_file(url, true);
            eval(f);
            path_loaded = true;
            this.dyn_path = url;
            this.last_path_time = sys.mod_time(url);
          } else {
            eval(this.shape);
            path_loaded = true;
            this.dyn_path = null;
          }
        } catch (e) {
          print(GF_LOG_ERROR, "Failed to load path: " + e + " - using default");
        }
      }

      if (path_loaded) {
      }
      else if ((!this.rx && !this.ry) || (!this.tl && !this.tr && !this.bl && !this.br)) {
        this.path.rectangle(0, 0, this.sw, this.sh, true);
      }
      else if ((this.rx == 50) && (this.ry == 50) && this.tl && this.tr && this.bl && this.br) {
        this.path.ellipse(0, 0, this.sw, this.sh);
      } else {
        make_rounded_rect.apply(this, [false]);
      }
      if (this.reload) {
        this.reload += current_utc_clock;
      }

      //force loading of matrix below
      this.mx = null;
  }

  if (this.path.is_rectangle)
    this.opaque = true;

  this.use_blit = blit_enabled ? this.blit : null;

  if ((this.x<0) || (this.y<0) || (this.x + this.sw > video_width) || (this.y + this.sh > video_height) || !this.path.is_rectangle) {
    this.use_blit = false;
    this.can_reuse = false;
  }

  if (!this.mx || (this.update_flag & UPDATE_POS)) {
    if (reset_local_stencil)
      this.local_stencil = null;
    this.mx = new evg.Matrix2D();
    if (this.rotation) {
      this.mx.rotate(0, 0, this.rotation * Math.PI / 180);
      this.use_blit = false;
      this.can_reuse = false;
    }
    if (this.hskew || this.vskew) {
      this.mx.skew_x(this.hskew);
      this.mx.skew_y(this.vskew);
      this.use_blit = false;
      this.can_reuse = false;
    }
    this.mx.translate(-video_width/2 + this.sw/2 + this.x, video_height/2 - this.sh/2 - this.y);


    if (this.is_text) {
        let tsize = this.path.measure();

        if (this.vertical) {
            let align_type = 0; //0: top, 1: center, 2: bottom
            if ((this.align == 'end') || (this.align == 'right')) align_type = 2;
            else if (this.align == 'center') align_type = 1;

            if (align_type==0) {
              this.mx.translate(0, this.sh/2 - this._font_size);
            }
            else if (align_type==1) {
              this.mx.translate(0, tsize.height/2);
            }
            else if (align_type==2) {
              this.mx.translate(0, -this.sh/2 + tsize.height);
            }
        } else {
            let align_type = 0; //0: left, 1: center, 2: right
            if ((this.align == 'start') && tsize.right_to_left) align_type = 2;
            else if ((this.align == 'end')  && !tsize.right_to_left) align_type = 2;
            else if (this.align == 'right') align_type = 2;
            else if (this.align == 'center') align_type = 1;

            if (align_type==0) {
              this.mx.translate(-this.sw/2, 0);
            }
            else if (align_type==1) {
              this.mx.translate(-tsize.width/2, 0);
            }
            else if (align_type==2) {
              this.mx.translate(this.sw/2 - tsize.width, 0);
            }
        }
    }


    //setup textures
    if (this.is_texture) {

      if (this.pids.length) {
        this.pids.forEach(p => {
           setup_texture.apply(this, [p]);
        });
        if ((this.pids.length>1) || use_gpu)
          this.can_reuse = false;
      }

      //image case
      if (!this.pids.length || this.replace_op_img) {
         setup_texture.apply(this, [null]);
        if (this.yuv_cache_texture) {
          this.yuv_cache_texture.set_alphaf(this.alpha);
          this.yuv_cache_texture.mx = this.local_stencil.mx;

          if (this.cmx && (this.cmx.length==20))
            this.yuv_cache_texture.cmx = new evg.ColorMatrix(this.cmx);
          else
            this.yuv_cache_texture.cmx = null;
        }
        if (this.replace_op) {
          this.can_reuse = false;
          this.use_blit = false;
        }
      }

      if (!this.replace_op_img && this.replace_op) {
        this.can_reuse = false;
        this.use_blit = false;
        this.local_stencil = create_brush.apply(this, [this.fill]);
      }

    } else {
      this.can_reuse = false;
      this.use_blit = false;
      this.local_stencil = create_brush.apply(this, [this.fill]);
    }
  }

  if (!this.outline || (this.update_flag & UPDATE_LINE)) {
    if (this.line_width) {
      let path = this.is_text ? this.path.get_path() : this.path;
      let lwidth = (this.line_width<0) ? (-this.line_width) : (this.line_width*this.sw / 100);
      let pen_settings = {
          width: lwidth,
          dash: this.line_dash,
          miter: this.miter_limit,
          offset: this.dash_offset,
          length: this.dash_length,
          dashes: this.dashes
      };
      if (this.line_pos=='outside') pen_settings.align = GF_PATH_LINE_OUTSIDE;
      else if (this.line_pos=='inside') pen_settings.align = GF_PATH_LINE_INSIDE;
      else pen_settings.align = GF_PATH_LINE_CENTER;

      if (this.line_dash=='dash') pen_settings.dash = GF_DASH_STYLE_DASH;
      else if (this.line_dash=='dot') pen_settings.dash = GF_DASH_STYLE_DOT;
      else if (this.line_dash=='dashdot') pen_settings.dash = GF_DASH_STYLE_DASH_DOT;
      else if (this.line_dash=='dashdashdot') pen_settings.dash = GF_DASH_STYLE_DASH_DASH_DOT;
      else if (this.line_dash=='dashdotdot') pen_settings.dash = GF_DASH_STYLE_DASH_DOT_DOT;
      else pen_settings.dash = GF_DASH_STYLE_PLAIN;

      if (this.cap=='round') pen_settings.cap = GF_LINE_CAP_ROUND;
      else if (this.cap=='square') pen_settings.cap = GF_LINE_CAP_SQUARE;
      else if (this.cap=='triangle') pen_settings.cap = GF_LINE_CAP_TRIANGLE;
      else pen_settings.cap = GF_LINE_CAP_FLAT;

      if (this.join=='round') pen_settings.join = GF_LINE_JOIN_ROUND;
      else if (this.join=='bevel') pen_settings.join = GF_LINE_JOIN_BEVEL;
      else if (this.join=='bevelmiter') pen_settings.join = GF_LINE_JOIN_MITER_SVG;
      else pen_settings.join = GF_LINE_JOIN_MITER;

      this.outline = path.outline(pen_settings);
      this.outline_brush = create_brush.apply(this, [this.line_color]);
    } else {
      this.outline=null;
      this.outline_brush = null;
    }
  }

  if (this.update_flag & UPDATE_COLOR) {
      if (this.outline_brush) this.outline_brush.set_color(this.line_color);
      if (!this.is_texture && this.local_stencil) this.local_stencil.set_color(this.fill);
  }

  this.update_flag = 0;

  if (this.use_blit ) {
    this.dst_rc = {x:this.x, y: this.y, w: this.sw, h: this.sh};
  } else {
    this.dst_rc = null;
  }

  if (this.pids.length>1) {
      //todo, get info from transition to check if we can blit / reuse source
      if (this.mix_ratio < 0) {
        this.can_reuse = false;
        this.use_blit = false;
      }
  }
  if (this.pids.length) {
    if (this.can_reuse) {
      print(GF_LOG_DEBUG, 'Scene ' + this.id + ' setup done: may reuse src as vout framebuffer');
    } else {
      print(GF_LOG_DEBUG, 'Scene ' + this.id + ' setup done: draw using ' + (this.use_blit ? 'blit' : 'texture') + (this.opaque ? ', opaque' : ', transparent' ) );
    }
  } else {
      print(GF_LOG_DEBUG, 'Scene ' + this.id + ' setup done: ' + (this.is_texture ? 'image fill' : 'solid color fill') );
  }
  this.tmp_blit = false;
  return 1;
},

tmp_blit: false,

fullscreen: function()
{
  this.no_draw = false;
  this.tmp_blit = this.use_blit;

  if (this.opaque && this.can_reuse) {
    if (this.pids.length>1) {
      if (this.mix_ratio == 0) {
        this.no_draw = true;
        return 0;
      }
      else if (this.mix_ratio == 1) {
        this.no_draw = true;
        return 1;
      } else {
        this.tmp_blit = false;        
      }
      return -1;
    } else {
      this.no_draw = true;
      this.tmp_blit = false;        
      return 0;
    }
  }

  if (this.pids.length>1) {
    if (this.mix_ratio == 0) {
    }
    else if (this.mix_ratio == 1) {
    } else {
      this.tmp_blit = false;        
    }
  }
  return -1;
},

identity: function() {
 if (this.no_draw && !this.outline) 
    return true;
 return false;
},
is_opaque: function() {
  return this.opaque; 
},


draw: function(canvas)
{
  if (!this.no_draw || this.force_draw) {
    if (this.tmp_blit && !canvas.has_clipper) {
        if ((this.mix_ratio == 1) && (this.pids.length>1)) {
          canvas_blit(this.pids[1].texture, this.dst_rc);
        } else {
          canvas_blit(this.pids[0].texture, this.dst_rc);
        }
    } else {
      let tx = null;

      if (!this.pids.length || this.replace_op_img) {
        tx = this.local_stencil;

        if (canvas_yuv && this.is_texture && !use_gpu) {
          if (this.yuv_cache_texture == null) {
            this.yuv_cache_texture = this.local_stencil.rgb2yuv(canvas);
            this.yuv_cache_texture.set_alphaf(this.alpha);
            this.yuv_cache_texture.mx = this.local_stencil.mx;
            this.yuv_cache_texture.repeat_s = this.local_stencil.repeat_s;
            this.yuv_cache_texture.repeat_t = this.local_stencil.repeat_t;

            let cmx = (this.replace_op) ? this.cmx_rep : this.cmx;
            if (cmx && (cmx.length==20))
              this.yuv_cache_texture.cmx = new evg.ColorMatrix(cmx);
            else
              this.yuv_cache_texture.cmx = null;
          }
          tx = this.yuv_cache_texture;
        }
      }

      if (this.replace_op && !this.replace_op_img) {
        tx = this.local_stencil;
      }

      if (!this.pids.length) {
        canvas_draw(this.path, this.mx, tx);
      } else if (this.replace_op && tx) {
        canvas_draw_sources(this.path, this.mx, this.replace_op, this.replace_par, tx);
      } else {
        canvas_draw_sources(this.path, this.mx);      
      }
    }
  }

  this.no_draw = false;
  if (this.outline) {
    canvas_draw(this.outline, this.mx, this.outline_brush);      
  }

}


}; }
