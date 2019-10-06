import * as evg from 'evg'

filter.pids = [];

//metadata
filter.set_name("Super EVG Test !");
filter.set_desc("JS-based vector graphics generator");
filter.set_version("0.1beta");
filter.set_author("GPAC team");
filter.set_help("This filter provides testing of gpac's vector graphics bindings");

//exposed arguments
//filter.set_arg("raw", "if set, accept undemuxed input PIDs", GF_PROP_BOOL, "false");
//filter.max_pids = -1;

let width = 320;
let height = 240;
let fps = {n:25, d:1};
let osize=0;
let cts=0;
let max_cts=100;
let pid;

let path = new evg.Path();
path.ellipse(0, 0, 200, 100);
path.ellipse(0, 0, 100, 50);
path.zero_fill=true;
let dashes=[];

let strikepath = path.outline({width: 5.0, align: GF_PATH_LINE_OUTSIDE, join: GF_LINE_JOIN_BEVEL, dash: GF_DASH_STYLE_DASH_DASH_DOT, dashes: [0.5, 2.5, 1.0, 5.0] });

let brush = new evg.SolidBrush();
brush.set_color('cyan');

let text = new evg.Text();
text.font = 'Times';
text.baseline = GF_TEXT_BASELINE_HANGING;
text.align=GF_TEXT_ALIGN_RIGHT;
text.lineSpacing=0;
text.italic=true;
text.text = ['My awsome text', 'on two lines' ];

let line = new evg.Path().move_to(-200, 0).line_to(200, 0).outline({width: 2} );

let centered=true;

let mx = new evg.Matrix2D();
//mx.skew_x(0.5);
mx.scale(0.5, 0.5);
let mxi = new evg.Matrix2D(mx);
mxi.inverse();

let rad = new evg.RadialGradient();
rad.set_points(0.0, 0.0, 10.0, 10.0, 15.0, 15.0);
rad.set_stop(0.0, 'red');
rad.set_stop(1.0, 'blue');
rad.mode = GF_GRADIENT_MODE_STREAD;
let cmx = new evg.ColorMatrix();

let tx = new evg.Texture();
tx.load("../auxiliary_files/logo.png", true);

let mxtxt = new evg.Matrix2D();
mxtxt.translate(-50, -100);

filter.initialize = function() {
print('ok ');

this.set_cap("StreamType", "Video", true);
this.set_cap("CodecID", "raw", true);

pid = this.new_pid();
pid.set_prop('StreamType', 'Visual');
pid.set_prop('CodecID', 'raw');
pid.set_prop('Width', width);
pid.set_prop('Height', height);
pid.set_prop('FPS', fps);
pid.set_prop('Timescale', fps.n);
pid.set_prop('PixelFormat', 'rgb');

osize = 3 * width * height;
cts=0;
}

filter.update_arg = function(name, val)
{
}


filter.process = function()
{
	if (cts >= max_cts) {
		return GF_EOS;
	}
	let pck = pid.new_packet(osize);
	//basic red formating of buffer
/*	let pixbuf = new Uint8Array(pck.data);
	for (let i=0; i<osize; i+= 3) {
		pixbuf[i] = 255*cts/100;
	}
*/
	let canvas = new evg.Canvas(width, height, 'rgb', pck.data);
	canvas.centered = centered;
//	canvas.clearf(1.0, 1.0, 0.0);
	canvas.clearf('yellow');
//	let clip = {x:2*cts,y:2*cts,w:50,h:20};
//	canvas.clearf(clip, 1.0);

	canvas.on_alpha = function(in_alpha, x, y) {
		//return Math.random() * 255;
		return x > y ? in_alpha : 255 - in_alpha;
		//return in_alpha;
	}
	mx.rotate(0, 0, 0.1);
	mxi = mx;

	cmx.bb = 0;
	cmx.gb = 1;
//	brush.set_color(cmx.apply('blue'));

	canvas.path = path;
//	canvas.clipper = {x:0,y:0,w:100,h:100};
	canvas.matrix = mx;
//	canvas.fill(brush);
//	canvas.fill(rad);

	tx.repeat_s = true;
	tx.repeat_t = true;
	let mmx = new evg.Matrix2D();
	mmx.scale(200/tx.width, 100/tx.height);
	mmx.translate(100, 50);
	mmx.add(mx);
	tx.mx = mmx;
	tx.set_alphaf(0.5);
	canvas.fill(tx);

	canvas.on_alpha = null;
	brush.set_color('green');
	canvas.path = strikepath;
	canvas.fill(brush);

	brush.set_color('black');
	canvas.matrix = mxtxt;
	canvas.path = line;
	canvas.fill(brush);

	canvas.path = text;
	canvas.fill(brush);


	pck.cts = cts;
	pck.sap = GF_FILTER_SAP_1;
	pck.send();
	cts ++;
	if (cts == max_cts) {
		pid.eos = true;
		return GF_EOS;
	}
	return GF_OK;
}

filter.process_event = function(pid, evt)
{
	if (evt.type != GF_FEVT_USER) {
		return;
	} 
	if (evt.ui_type == GF_EVENT_MOUSEMOVE) {
		let m = {x: evt.mouse_x, y: evt.mouse_y};
		if (centered) {
			m.x -= width/2;
			m.y = height/2 - m.y;
		}
		//print('mouse move ' + JSON.stringify(m) );
		m = mxi.apply(m);

		if (path.point_over(m))
			print('mouse over path !');

	}
}


