import { Bitstream as BS } from 'gpaccore'
import { Sys as sys } from 'gpaccore'
import * as evg from 'evg'

filter.pids = [];

const SAMPLING_NONE=0;
const SAMPLING_422=1;
const SAMPLING_420=2;
const SAMPLING_411=3;

const INTERLEAVE_COMPONENT=0;
const INTERLEAVE_PIXEL=1;
const INTERLEAVE_MIXED=2;
const INTERLEAVE_ROW=3;
const INTERLEAVE_TILE=4;
const INTERLEAVE_MULTIY=5;

//metadata
filter.set_name("uncvg");
filter.set_desc("Uncompressed Video File Format Generator Utility");
filter.set_version("1.0");
filter.set_author("GPAC team - (c) Telecom ParisTech 2023 - license LGPL v2");
filter.set_help("This filter provides generation of test images for ISO/IEC 23001-17\n"
+"Generated pixels can be:\n"
+"- a pattern of colors in columns, or in rectangles if [-sq]() is set, using the specified palette.\n"
+"- an image source if [-img]() is used.\n"
+"\n"
+"Colors specified in the palette can be default GPAC colors (see gpac -h colors), 0xRRGGBB or 0xAARRGGBB\n"
+"\n"
+"When generating video, the pixels are shifted to the left at every frame\n"
+"\n"
+"Components are described as N[bpc][+k] with\n"
+"- N: component type, one of M(mono), Y, U, V, R, G, B, A, d(depth), disp, p(palette), f(filterArray), x (pad)\n"
+"- bpc: bits per component value, default is 8. Non-integer values must be one of\n"
+"  - sft: floating-point value on 16 bits\n"
+"  - flt: floating-point value on 32 bits\n"
+"  - dbl: floating-point value on 64 bits\n"
+"  - dblx: floating-point value on 128 bits\n"
+"  - cps: complex value as two floats on 16 bits each\n"
+"  - cpf: complex value as two floats on 32 bits each\n"
+"  - cpd: complex value as two floats on 64 bits each\n"
+"  - cpx: complex value as two floats on 128 bits each\n"
+"- k: force component alignment on k bytes, default is 0 (no alignment)\n"
);

filter.set_arg({ name: "vsize", desc: "width and height of output image", type: GF_PROP_VEC2, def: "128x128"} );
filter.set_arg({ name: "c", desc: "image components", type: GF_PROP_STRING_LIST, def: ["R8", "G8", "B8"]} );
filter.set_arg({ name: "tiles", desc: "number of horizontal and vertical tiles", type: GF_PROP_VEC2, def: "1x1"} );
filter.set_arg({ name: "interleave", desc: `interleave type
- comp: component-based interleaving (planar modes)
- pix: pixel-based interleaving (packed modes)
- mix: pixel-based interleaving for UV (semi-planar modes)
- row: row-based interleaving
- tile: tile-component interleaving
- multi: pixel-based interleaving (packed modes) for sub-sampled modes`, type: GF_PROP_UINT, def: "pix", minmax_enum: "comp|pix|mix|row|tile|multi"} );
filter.set_arg({ name: "sampling", desc: `sampling types
- none: no sub-sampling
- 422: YUV 4:2:2 sub-sampling
- 420: YUV 4:2:0 sub-sampling
- 411: YUV 4:1:1 sub-sampling`, type: GF_PROP_UINT, def: "none", minmax_enum: "none|422|420|411"} );
filter.set_arg({ name: "block_size", desc: "block size in bytes", type: GF_PROP_UINT, def: "0"} );
filter.set_arg({ name: "pad_lsb", desc: "padded bits are at LSB in the block", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "ble", desc: "block is little endian", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "br", desc: "block has reversed components", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "pixel_size", desc: "size of pixel in bytes", type: GF_PROP_UINT, def: "0"} );
filter.set_arg({ name: "row_align", desc: "row alignment in bytes", type: GF_PROP_UINT, def: "0"} );
filter.set_arg({ name: "tile_align", desc: "tile alignment in bytes", type: GF_PROP_UINT, def: "0"} );
filter.set_arg({ name: "cle", desc: "byte-aligned components are little endian", type: GF_PROP_BOOL, def: "false"} );

filter.set_arg({ name: "img", desc: "use specified image as input instead of RGB generation", type: GF_PROP_STRING, def: ""} );
filter.set_arg({ name: "asize", desc: "use input image size", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "pal", desc: "default palette for color generation", type: GF_PROP_STRING_LIST, def: ["red", "green", "blue", "white", "black", "yellow", "cyan", "grey", "orange", "violet"]} );
filter.set_arg({ name: "fa", desc: "bayer-like filter - only 2x2 on R,G,B components is supported", type: GF_PROP_STRING_LIST, def: ["B", "G", "G", "R"]} );
filter.set_arg({ name: "bpm", desc: "set sensor bad pixel map as a list of cN (broken column), rM (broken row) or NxM (single pixel)", type: GF_PROP_STRING_LIST, def: []} );
filter.set_arg({ name: "fps", desc: "frame rate to generate - using 0 will trigger item muxing", type: GF_PROP_FRACTION, def: "25/1"} );
filter.set_arg({ name: "dur", desc: "duration to generate - using 0 will trigger item muxing", type: GF_PROP_FRACTION, def: "1/1"} );
filter.set_arg({ name: "cloc", desc: "set chroma location type", type: GF_PROP_UINT, def: "-1", minmax_enum: "-1,6"} );
filter.set_arg({ name: "stereo", desc: "dump a stereo image", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "sq", desc: "generate square patterns instead of columns", type: GF_PROP_FLOAT, def: "0"} );
filter.set_arg({ name: "shade", desc: "shade pixels from black at bottom to full intensity at top", type: GF_PROP_BOOL, def: "false"} );
filter.set_arg({ name: "scpt", desc: "use single color per tile, first tile using the first color in palette", type: GF_PROP_BOOL, def: "false"} );

let components = [];

let tx = null;
let tx_width, tx_height;
let col_width;

let out_size=0;
let cts=0;
let max_cts=0;
let pid;

let pixel = [];
let min_bits=0;
let pix_len;

let r_y_idx = -1;
let g_u_idx = -1;
let b_v_idx = -1;
let a_idx = -1;
let d_idx = -1;
let is_playing=false;
let use_palette=false;
let use_fa=false;
let fa_width=2;
let fa_height=2;
let fa=[];
let palette_alpha=false;

filter.set_cap({id: "StreamType", value: "Video", output: true} );
filter.set_cap({id: "CodecID", value: "raw", output: true} );

filter.initialize = function()
{
	/*create a texture from a local file*/
	if (this.img != "") {
		tx = new evg.Texture(this.img);
		tx_width = tx.width;
		tx_height = tx.height;
		if (this.asize) {
			this.vsize.x = tx.width;
			this.vsize.y = tx.height;
		}
	}
	col_width = Math.ceil(filter.vsize.x / filter.pal.length);
	while (col_width%4) col_width++;

	if (filter.stereo && (filter.vsize.x%2))
		filter.vsize.x++;

	setup_uncv();

	pid = this.new_pid();
	pid.set_prop('StreamType', 'Visual');
	pid.set_prop('CodecID', 'uncv');
	pid.set_prop('Width', this.vsize.x);
	pid.set_prop('Height', this.vsize.y);
	pid.set_prop('Timescale', filter.fps.n ? filter.fps.n : 1000);
	pid.set_prop('PixelFormat', 'uncv');

	if (!filter.fps.n || !filter.fps.d || !filter.dur.n || !filter.dur.d) {
		max_cts = 1;
		pid.set_prop('ItemID', 1);
	} else {
		if (filter.fps.n && filter.fps.d)
			pid.set_prop('FPS', filter.fps);
		max_cts = filter.dur.n * filter.fps.n / filter.dur.d / filter.fps.d;
	}

	//write dec config as a box array
	let bs = new BS();
	let size, pos = 0;

	//component definition
	bs.put_u32(0);
	bs.put_4cc("cmpd");
	if (use_palette || use_fa) {
		if (palette_alpha)
			bs.put_u32(components.length+4);
		else
			bs.put_u32(components.length+3);
	}
	else
		bs.put_u32(components.length);
	for (let i=0; i<components.length; i++) {
		let c = components[i];
		bs.put_u16(c.type);
	}
	if (use_palette || use_fa) {
		bs.put_u16(4); //R
		bs.put_u16(5); //G
		bs.put_u16(6); //B	
		if (palette_alpha)
			bs.put_u16(7); //A	
	}

	size = bs.pos;
	bs.pos = 0;
	bs.put_u32(size);
	bs.pos = size;

	//uncC
	pos = bs.pos;
	bs.put_u32(0);
	bs.put_4cc("uncC");
	bs.put_u32(0); //version+flags
	bs.put_u32(0); //profile
	bs.put_u32(components.length);
	for (let i=0; i<components.length; i++) {
		let c = components[i];
		bs.put_u16(i); //index
		bs.put_u8(c.bits-1); //nb_bits_minus_one
		bs.put_u8(c.mode); //component_format
		bs.put_u8(c.align); //component_align_size
	}
	bs.put_u8(filter.sampling);
	bs.put_u8(filter.interleave);
	bs.put_u8(filter.block_size);
	bs.put_bits(filter.cle, 1); //components_little_endian
	if (filter.block_size) {
		bs.put_bits(filter.pad_lsb ? 1 : 0, 1); //pad_lsb
		bs.put_bits(filter.ble ? 1 : 0, 1); //block_little_endian
		bs.put_bits(filter.br ? 1 : 0, 1); //block_reversed
	} else {
		bs.put_bits(0, 3);
	}
	bs.put_bits(0, 1); //pad_unknown
	bs.put_bits(0, 3); //reserved

	bs.put_u32(filter.pixel_size);
	bs.put_u32(filter.row_align);
	bs.put_u32(filter.tile_align);
	bs.put_u32(filter.tiles.x-1);
	bs.put_u32(filter.tiles.y-1);
	//write size
	size = bs.pos - pos;
	bs.pos = pos;
	bs.put_u32(size);
	bs.pos = size + pos;

	if (use_palette) {
		pos = bs.pos;
		bs.put_u32(0);
		bs.put_4cc("cpal");
		bs.put_u32(0); //version 0 no flags
		bs.put_u16(palette_alpha ? 4 : 3); //nb_comp, only (A)RGB palette 8 bits int for now
		bs.put_u32(components.length); //R idx
		bs.put_u8(7); //bits minus 1
		bs.put_u8(0); //format
		bs.put_u32(components.length+1); //G idx
		bs.put_u8(7); //bits minus 1
		bs.put_u8(0); //format
		bs.put_u32(components.length+2); //B idx
		bs.put_u8(7); //bits minus 1
		bs.put_u8(0); //format
		if (palette_alpha) {
			bs.put_u32(components.length+3); //A idx
			bs.put_u8(7); //bits minus 1
			bs.put_u8(0); //format
		}

		bs.put_u32(filter.pal.length);
		for (let i=0; i<filter.pal.length; i++) {
			let col = filter.pal[i];
			bs.put_u8( sys.color_component(col, 1) * 255);
			bs.put_u8( sys.color_component(col, 2) * 255 );
			bs.put_u8( sys.color_component(col, 3) * 255 );
			if (palette_alpha)
				bs.put_u8( sys.color_component(col, 0) * 255 );
		}
		//write size
		let size = bs.pos - pos;
		bs.pos = pos;
		bs.put_u32(size);
		bs.pos = size + pos;
	}
	if (use_fa) {
		pos = bs.pos;
		bs.put_u32(0);
		bs.put_4cc("cpat");
		bs.put_u32(0); //version 0 no flags
		bs.put_u16(fa_width); //pattern width
		bs.put_u16(fa_height); //pattern height

		for (let i=0; i<fa_height; i++) {
			for (let j=0; j<fa_width; j++) {
				let ctype = fa[i*fa_height + j];
				//ctype is 4->6 (R->B), so index us ctype-4 + components.length
				bs.put_u32(ctype-4 + components.length);
				bs.put_float(1.0);
			}
		}

		//write size
		let size = bs.pos - pos;
		bs.pos = pos;
		bs.put_u32(size);
		bs.pos = size + pos;
	}

	let bad_rows=[];
	let bad_cols=[];
	let bad_pix=[];
	let use_sbpm=false;
	for (let i=0; i<filter.bpm.length; i++) {
		let c = filter.bpm[i];
		if (c.indexOf('r')==0) {
			bad_rows.push( parseInt(c.slice(1) ) );
			use_sbpm=true;
		}
		else if (c.indexOf('c')==0) {
			bad_cols.push( parseInt(c.slice(1) ) );
			use_sbpm=true;
		}
		else {
			let s = c.indexOf('x');
			if (s>0) {
				let x = parseInt(c.slice(0, s) );
				let y = parseInt(c.slice(s+1) );
				bad_pix.push({x: x, y: y});
				use_sbpm=true;
			}
		}
	}
	if (use_sbpm) {
		pos = bs.pos;
		bs.put_u32(0);
		bs.put_4cc("sbpm");
		bs.put_u32(0); //version 0 no flags
		bs.put_u32(0); //component count, applies to all
		bs.put_u8(0); //correction applied + reserved7=0
		bs.put_u32(bad_rows.length);
		bs.put_u32(bad_cols.length);
		bs.put_u32(bad_pix.length);

		for (let i=0; i<bad_rows.length; i++) {
			bs.put_u32(bad_rows[i]);
		}
		for (let i=0; i<bad_cols.length; i++) {
			bs.put_u32(bad_cols[i]);
		}
		for (let i=0; i<bad_pix.length; i++) {
			bs.put_u32(bad_pix[i].y); //bad row
			bs.put_u32(bad_pix[i].x); //bad col
		}

		//write size
		let size = bs.pos - pos;
		bs.pos = pos;
		bs.put_u32(size);
		bs.pos = size + pos;
	}
	if ((filter.cloc>=0) && gen_yuv) {
		bs.put_u32(13);
		bs.put_4cc("cloc");
		bs.put_u32(0); //version 0 no flags
		bs.put_u8(filter.cloc); 
	}

	if (filter.stereo) {
		bs.put_u32(14);
		bs.put_4cc("fpac");
		bs.put_u32(0); //version 0 no flags
		bs.put_bits(3, 4); //video_frame_packing side by side
		bs.put_bits(1, 4); //PackedContentInterpretationType left is left view
		bs.put_bits(0, 1); //QuincunxSamplingFlag
		bs.put_bits(0, 7); //reserved		
	}

	/* todo - write if needed
		NUC
		polarization
		reference level
		disparity info
		depth mapping info
	*/

	pid.set_prop('DecoderConfig', bs.get_content());

	/*todo - set field interlace sample group if needed*/
}

let blocksize_bits;
let gen_yuv;

function line_size(comp_bits)
{
	let clen = comp_bits.length;
	let size=0;

	if (!filter.block_size) {		
		for (let j=0; j<tile_width; j++) {
			for (let c=0; c<clen; c++) {
				let cbits;
				if (comp_bits[c].align) {
					cbits = 8*comp_bits[c].align; 
					while (size%8) size++;
				} else {
					cbits = comp_bits[c].bits; 
				}
				size += cbits;
			}
		}
		while (size % 8) size++;
		return size / 8;
	}
	//block size, figure out how many blocks
	let nb_bits = 0;
	for (let i=0; i<tile_width; i++) {
		for (let c=0; c<clen; c++) {
			let cbits;
			if (comp_bits[c].align) {
				cbits = 8*comp_bits[c].align; 
				while (size%8) size++;
			} else {
				cbits = comp_bits[c].bits; 
			}
			if (nb_bits + cbits > blocksize_bits) {
				nb_bits=0;
				size += filter.block_size;
				//if first comp in block is first comp, pattern is done
				if (c==0) {
					let nb_pix = i;
					let nb_blocks = 1;
					while ((nb_blocks+1) * nb_pix < tile_width)	nb_blocks++;
					size *= nb_blocks;
					//jump to last non-full pattern
					i += nb_pix * (nb_blocks-1);
				} 
			}
			nb_bits += cbits;
		}
	}
	if (nb_bits)
		size += filter.block_size;
	return size;
}

let tile_width, tile_height;
let row_align_bytes, tile_align_bytes, subsample_x;

const comp_types = [
	{ type: 8, name: "depth", alias: "d"},
	{ type: 9, name: "disp"},
	{ type: 10, name: "pal", alias: "p"},
	{ type: 11, name: "fa", alias: "f"},
	{ type: 12, name: "pad", alias: "x"},

	//do not change order of these types otherwise we conflict with 'u', 'v' and 'g'
	{ type: 0, name: "mono", alias: "m"},
	{ type: 1, name: "y"},
	{ type: 2, name: "u"},
	{ type: 3, name: "v"},
	{ type: 4, name: "r"},
	{ type: 5, name: "g"},
	{ type: 6, name: "b"},
	{ type: 7, name: "a"},
];

let comp_le_buf = null;
let comp_le_bs = null;
let comp_le_view = null;

let has_a=false;
let has_depth=false;
let has_disp=false;


function setup_uncv()
{
	components.length=0;
	if (filter.c.length==0)
		throw "At least one component must be specified";

	let max_align_size=0;
	let has_rgb=false;

	for (let i=0; i<filter.pal.length; i++) {
		if (sys.color_component(filter.pal[i], 0) < 1.0) {
			palette_alpha = true;
			break;
		}
	}

	for (let i=0; i<filter.c.length; i++) {
		let depth=0;
		let fmt=0;
		let c_str = filter.c[i].toLowerCase();
		let type=0;
		for (let j=0; j<comp_types.length; j++) {
			let match=0;
			if (c_str.startsWith(comp_types[j].name)) {
				match = comp_types[j].name.length;
			}
			else if (!match && (typeof comp_types[j].alias == 'string') && c_str.startsWith(comp_types[j].alias)) {
				match = comp_types[j].alias.length;
			}
			if (match) {
				type = comp_types[j].type + 1;
				if (typeof comp_types[j].fmt != 'undefined')
					fmt = comp_types[j].fmt;
				c_str = c_str.slice(match);
				break;
			}
		}
		if (!type) {
			throw "Invalid component declaration "+c_str;			
		}
		type--;
		let align_size=0;
		let ws = c_str.indexOf('+');
		if (ws>0) {
			let a_str = c_str.slice(ws + 1);
			align_size = parseInt(a_str);
			c_str = c_str.slice(0, ws);
			if (align_size>max_align_size) max_align_size = align_size; 
		}

		if (c_str=='') { fmt=0; depth = 8; }
		else if (c_str=='flt') {fmt = 1; depth=32; }
		else if (c_str=='sft') {fmt = 1; depth=16; }
		else if (c_str=='dbl') {fmt = 1; depth=64; }
		else if (c_str=='dbx') {fmt = 1; depth=128; }
		else if (c_str=='cps') {fmt = 2; depth=32; }
		else if (c_str=='cpf') {fmt = 2; depth=64; }
		else if (c_str=='cpd') {fmt = 2; depth=128; }
		else if (c_str=='cpx') {fmt = 2; depth=256; }
		else if (!isNaN(c_str)) { fmt=0; depth = parseInt(c_str); }

		if (!depth) {
			throw "Invalid component declaration "+c_str;
		}
		if (fmt==2) {
			throw "Complex types not supported yet";
		}
		if (align_size && (align_size*8<depth)) {
			throw "Align size must be greater than component bit depth";			
		}
		components.push({type: type, bits: depth, mode: fmt, align: align_size});

		if (type==10) use_palette=true;
		else if (type==11) use_fa=true;
		else if ((type>=4) && (type<=6)) {
			has_rgb = true;
		}
		else if (type==7) {
			palette_alpha = false;
			has_a=true;
		}
		else if (type==8) has_depth = true;
		else if (type==9) has_disp = true;
	}

	if (use_palette && has_rgb) {
		throw "Cannot use R,G or B component with palette (already RGB)"+c_str;
	}
	if (use_fa) {
		if (has_rgb) {
			throw "Cannot use R,G or B component with filter array (already RGB)"+c_str;
		}
		if (use_palette) {
			throw "Cannot use palette with filter array"+c_str;
		}
		if (filter.fa.length != 4) {
			throw "Filter array must be 2x2 size, got "+ JSON.stringify(filter.fa);		
		}

		fa=[];
		for (let i=0; i<filter.fa.length; i++) {
			let c_str = filter.fa[i].toLowerCase();
			let type=0;
			if (c_str=='r') type=4;
			else if (c_str=='g') type=5;
			else if (c_str=='b') type=6;
			else {
				throw "Only RGB component types supported in filter array";
			}
			fa.push(type);
		}
	}


	pixel = [];
	for (let i=0; i<components.length; i++) {
		pixel.push(0);
	}
	pix_len = pixel.length;

	blocksize_bits = 8 * filter.block_size;
	if (!filter.vsize.x || !filter.vsize.y)
		throw "Image size shall not be 0";
	if (!filter.tiles.x || !filter.tiles.y)
		throw "Image tiling shall not be 0";

	if (filter.vsize.x % filter.tiles.x)
		throw "Image width not a multiple of num horizontal tiles";

	if (filter.vsize.y % filter.tiles.y)
		throw "Image height not a multiple of num vertical tiles";

	if (filter.interleave==INTERLEAVE_MULTIY) {
		let nb_y=0, nb_u=0, nb_v=0;
		for (let i=0; i<components.length; i++) {
			let comp = components[i];
			if (comp.type==1) nb_y++;
			if (comp.type==2) nb_u++;
			if (comp.type==3) nb_v++;
		}
		if ((nb_u!=1) && (nb_v!=1))
			throw "Component pattern must have 1 U/Cb and 1 V/Cr";
		if (filter.sampling==SAMPLING_422) {
			if (nb_y != 2)
				throw "Component pattern must have 2 Y, not " + nb_y;
		}
		else if (filter.sampling==SAMPLING_411) {
			if (nb_y != 4)
				throw "Component pattern must have 4 Y, not " + nb_y;
		}
		else {
			throw "Unsupported interleave type for sampling type " +filter.sampling;
		}
		if (components.length % 2)
			throw "Component pattern must have an even number of components";
	}

	tile_width = filter.vsize.x / filter.tiles.x;
	tile_height = filter.vsize.y / filter.tiles.y;

	if (filter.sampling) {
		switch (filter.interleave) {
		case INTERLEAVE_COMPONENT:
		case INTERLEAVE_MIXED:
		case INTERLEAVE_MULTIY:
			break;
		default:
			if (filter.sampling==SAMPLING_420)
				throw "Sub-sampling must use interleave mode 0 or 2";
			else
				throw "Sub-sampling must use interleave mode 0, 2 or 5";
			break;
		}
		if (filter.sampling==SAMPLING_420) {
			if (tile_width%2)
				throw "Tile width must be a multiple of 2 for YUV 420 sampling";
			if (tile_height%2)
				throw "Tile height must be a multiple of 2 for YUV 420 sampling";
		}
		else if (filter.sampling==SAMPLING_422) {
			if (tile_width%2)
				throw "Tile width must be a multiple of 2 for YUV 422 sampling";
		}
		else if (filter.sampling==SAMPLING_411) {
			if (tile_width%4)
				throw "Tile width must be a multiple of 4 for YUV 411 sampling";
		}
	}


	let num_tiles = filter.tiles.x * filter.tiles.y;
	row_align_bytes = tile_align_bytes = 0;


	if ((filter.interleave==INTERLEAVE_TILE) && (num_tiles==1)) {
		throw "Tile-Component interleaving requires more than one tile";
	}

	gen_yuv = false;
	let nb_y=0;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		if (comp.type==1) { nb_y++; gen_yuv = true; }
		else if ((comp.type==2) || (comp.type==3)) gen_yuv = true;

		comp.row_align_size = 0;
		comp.tile_align_size = 0;
	}
	if ((nb_y>1) && (filter.interleave!=INTERLEAVE_MULTIY))
		print(GF_LOG_WARNING, 'Multiple Y component but interleaving is not 5, undefined results');

	if ((filter.interleave==INTERLEAVE_COMPONENT) || (filter.interleave==INTERLEAVE_ROW) || (filter.interleave==INTERLEAVE_TILE)) {
		out_size = 0;
		let tile_size=0;
		filter.write_pixel = writepix_interleave_0;
		//write component-interleave (planar), get the size of each component
		for (let i=0; i<components.length; i++) {
			let comp = components[i];
			comp.line_size = line_size([ {bits: comp.bits, align: comp.align} ] );
			if ((comp.type==2) || (comp.type==3)) {
				if ((filter.sampling==SAMPLING_422) || (filter.sampling==SAMPLING_420))
					comp.line_size /= 2;
				else if (filter.sampling==SAMPLING_411)
					comp.line_size /= 4;
			}
			if (filter.row_align) {
				while (comp.line_size % filter.row_align) {
					comp.line_size++;
				}
				comp.row_align_size = comp.line_size;
			}

			comp.plane_size = comp.line_size * tile_height;

			//for tile-component mode, tile align is on the plane size
			if (filter.tile_align && (filter.interleave==INTERLEAVE_TILE)) {
				while (comp.plane_size % filter.tile_align) {
					comp.plane_size++;
				}
				comp.tile_align_size = comp.plane_size;
			}

			if ((comp.type==2) || (comp.type==3)) {
				if (filter.sampling==SAMPLING_420)
					comp.plane_size /= 2;
			}
			tile_size += comp.plane_size;
		}

		//except for tile-component mode, tile align is on cumulated size of all components in tile
		if (filter.interleave!=INTERLEAVE_TILE) {
			if (filter.tile_align) {
				while (tile_size % filter.tile_align) {
					tile_size++;
				}
				tile_align_bytes = tile_size;
			} else {
				tile_align_bytes = tile_size;
			}
		}
		out_size += tile_size * num_tiles;

		if (gen_yuv) {
			if (filter.sampling==SAMPLING_420)
				filter.write_pixel = writepix_interleave_0_420;
			else if (filter.sampling==SAMPLING_422)
				filter.write_pixel = writepix_interleave_0_422;
			else if (filter.sampling==SAMPLING_411)
				filter.write_pixel = writepix_interleave_0_411;
		}
	} else if (filter.interleave==INTERLEAVE_MIXED) {
		out_size = 0;

		if (!filter.sampling || (filter.sampling>SAMPLING_411))
			throw "bad sampling type for mixed interleave mode";

		if (filter.row_align)
			throw "row align not supported in interleave=2 mode";
		if (filter.tile_align)
			throw "tile align not supported in interleave=2 mode";

		let uv_found=false;
		//write component-interleave (planar), get the size of each component
		for (let i=0; i<components.length; i++) {
			let comp = components[i];
			comp.line_size = line_size([ {bits: comp.bits, align: comp.align} ] );
			if ((comp.type==2) || (comp.type==3)) {
				if (uv_found) {
					comp.line_size = 0;
					comp.plane_size = 0;
					continue;
				}
				uv_found = true;
			}

			comp.plane_size = comp.line_size * tile_height;
			if ((comp.type==2) || (comp.type==3)) {
				if (filter.sampling==SAMPLING_420)
					comp.plane_size /= 2;
			}
			out_size += comp.plane_size * num_tiles;
		}

		if (filter.sampling==SAMPLING_420)
			filter.write_pixel = writepix_interleave_2_420;
		else if (filter.sampling==SAMPLING_422)
			filter.write_pixel = writepix_interleave_2_422;
		else if (filter.sampling==SAMPLING_411)
			filter.write_pixel = writepix_interleave_2_411;

	} else if ((filter.interleave==INTERLEAVE_PIXEL) || (filter.interleave==INTERLEAVE_MULTIY)) {
		let comp_bits=[];
		for (let i=0; i<components.length; i++) {
			let comp = components[i];
			comp_bits.push( {bits: comp.bits, align: comp.align} );			
			comp.line_size = 0;
			comp.plane_size = 0;
		}
		let lsize = line_size( comp_bits );
		if (filter.pixel_size) {
			let lsize2 = tile_width * filter.pixel_size;
			if (lsize2<lsize) {
				throw "Invalid pixel size, smaller than required bits";
			}
			lsize = lsize2;
		}

		if (filter.row_align) {
			while (lsize % filter.row_align) {
				lsize++;
			}
			row_align_bytes = lsize;
		}
		let psize = lsize * tile_height;

		tile_align_bytes = psize;
		if (filter.tile_align) {
			while (psize % filter.tile_align) {
				psize++;
			}
			tile_align_bytes = psize;
		}

		out_size = psize * num_tiles;
		filter.write_pixel = writepix_interleave_1;
		if (filter.interleave==INTERLEAVE_MULTIY) {
			if (filter.sampling==SAMPLING_422) subsample_x = 2;
			else if (filter.sampling==SAMPLING_411) subsample_x = 4;
			filter.write_pixel = writepix_interleave_multiy;
		}
	} else {
		throw "Unsupported interleave type";
	}

	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let nbb = comp.bits;
		comp.max_val = 1;
		//palette is an index - for other types we normalize between 0 and 1
		if (comp.type != 10) {
			while (nbb) {
				comp.max_val *= 2;
				nbb--;
			}
			comp.max_val--;
		}

		if (!min_bits || (min_bits>comp.bits))
			min_bits = comp.bits;

		//map monochrome, Y or R to first comp
		if ((comp.type==0) || (comp.type==1) || (comp.type==4)) {
			if (r_y_idx<0) r_y_idx = i;
			comp.base_idx = r_y_idx;
		}
		else if ((comp.type==2) || (comp.type==5)) {
			if (g_u_idx<0) g_u_idx = i;
			comp.base_idx = g_u_idx;
		}
		else if ((comp.type==3) || (comp.type==6)) {
			if (b_v_idx<0) b_v_idx = i;
			comp.base_idx = b_v_idx;
		}
		//alpha
		else if (comp.type==7) {
			if (a_idx<0) a_idx = i;
			comp.base_idx = a_idx;
		}
		//depth or disp
		else if ((comp.type==8) || (comp.type==9)) {
			if (d_idx<0) d_idx = i;
			comp.base_idx = d_idx;
		}
		//palette
		else if (comp.type==10) {
			if (r_y_idx<0) r_y_idx = i;
			comp.base_idx = r_y_idx;
		}
		//filter array
		else if (comp.type==11) {
			if (r_y_idx<0) r_y_idx = i;
			comp.base_idx = r_y_idx;
		}
		else {
			comp.base_idx = -1;
		}
	}
	//print('Components setup: ' + JSON.stringify(components));

	if (filter.cle && max_align_size) {
		comp_le_buf = new ArrayBuffer(max_align_size);
		comp_le_bs = new BS(comp_le_buf, true); 
		comp_le_view = new Uint8Array(comp_le_buf); 
	}
}

function get_pixel(i, j)
{
	i -= cts * filter.vsize.x / max_cts;
	if (i<0) i += filter.vsize.x;

	if (filter.stereo && (i>=filter.vsize.x/2))
		i-=filter.vsize.x/2;

	let r=0, g=0, b=0;
	let a=1;
	let d=0;
	if (tx) {
		let t_x = Math.floor(i * tx.width / filter.vsize.x);
		if (filter.stereo) t_x *= 2;
		let t_y = Math.floor(j * tx.height / filter.vsize.y);
		let tx_p = tx.get_pixel(t_x, t_y);
		r = tx_p.r;
		g = tx_p.g;
		b = tx_p.b;
		if (d_idx>=0)
			d = tx_p.a;
		else
			a = tx_p.a;
	} else {
		let tidx;
		if (filter.scpt) {
			let tile_x = Math.floor(i / tile_width);
			let tile_y = Math.floor(j / tile_height);
			tidx = (tile_x * filter.tiles.x + tile_y) % filter.pal.length;
		} else {
			tidx = Math.floor(i / col_width);
			if (filter.sq!=0) {
				let ridx = Math.floor(filter.sq*j / col_width);
				tidx += ridx;
				
				while (tidx>=filter.pal.length)
					tidx -= filter.pal.length;
				while (tidx<0)
					tidx += filter.pal.length;
			}
		}

		if (use_palette) {
			pixel[r_y_idx] = tidx;
			return;
		}
		let scale = 1 - j/filter.vsize.y;
		let col = filter.pal[tidx];
		r = sys.color_component(col, 1);
		g = sys.color_component(col, 2);
		b = sys.color_component(col, 3);
		if (a_idx>=0) {
			if ((d_idx<0) && palette_alpha)
				a = sys.color_component(col, 0);
			else
				a = scale;
		} else if (filter.shade) {
			r *= scale;
			g *= scale;
			b *= scale;
		}
		if (d_idx>=0) {
			if (palette_alpha)
				d = sys.color_component(col, 0);
			else
				d = scale;
		}
	}

	if (gen_yuv) {
		let y = 0.299*r + 0.587 * g + 0.114 * b;
		let cb = -0.169*r - 0.331*g + 0.499*b + 0.5;
		let cr = 0.499*r - 0.418*g - 0.0813*b + 0.5;
		r = y;
		g = cb;
		b = cr;
	}
	else if (use_fa) {
		let p_x = i % fa_width;
		let p_y = j % fa_height;
		let type = fa[p_x + p_y*fa_height];
		if (type==5) r = g;
		else if (type==6) r = b;
	}
	if (r_y_idx>=0) pixel[r_y_idx] = r;
	if (g_u_idx>=0) pixel[g_u_idx] = g;
	if (b_v_idx>=0) pixel[b_v_idx] = b;
	if (a_idx>=0) pixel[a_idx] = a;
	if (d_idx>=0) pixel[d_idx] = d;
}


//array of bitstream wrapper objects, one if pixel-interleave, one per comp if component-interleave (planar)
let bs_array = [];

function setup_block(bsw)
{
	if (!blocksize_bits) return;

	bsw.next_block_idx = 0;
	bsw.nb_bits_block = 0;
	bsw.block_comps = [];
	if (filter.ble) {
		bsw.le_buf = new ArrayBuffer(filter.block_size);
		bsw.le_bs = new BS(bsw.le_buf, true); 
		bsw.le_view = new Uint8Array(bsw.le_buf); 
	}
	let max_comp = Math.ceil(blocksize_bits / min_bits);
	for (let i=0; i<max_comp; i++) {
		bsw.block_comps.push({val: 0, component: null});
	}
}

let tile_start_offset = 0;
function start_frame(data)
{
	bs_array.length=0;
	tile_start_offset=0;

	//component interleave
	if ((filter.interleave==INTERLEAVE_PIXEL) || (filter.interleave==INTERLEAVE_MULTIY)) {
		let bsw = {};
		bsw.bs = new BS(data, true);
		setup_block(bsw);
		bs_array.push(bsw);
		bsw.init_offset = 0;
		bsw.line_start_pos = 0;
		bsw.row_align_size = row_align_bytes;
		bsw.tile_align_size = 0;
		bsw.tile_size = tile_align_bytes;
		return;
	}

	//all other are planar modes
	let offset = 0;
	let tile_size=0;
	let comp_row_size=0;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let bsw = {};
		bsw.bs = new BS(data, true);
		bsw.bs.pos = offset;
		bsw.init_offset = offset;
		bsw.tile_start_pos = offset;
		bsw.line_start_pos = offset;
		bsw.row_align_size = comp.row_align_size;

		bsw.tile_align_size = 0;

		setup_block(bsw);
		//row-interleave, first row of next comp starts after first row of this comp
		if (filter.interleave==INTERLEAVE_ROW)
			offset += comp.line_size;
		//tile-interleave, first row of next comp starts after last value of last tile of this comp
		else if (filter.interleave==INTERLEAVE_TILE) {
			offset += comp.plane_size * filter.tiles.x * filter.tiles.y;
			bsw.tile_align_size = comp.tile_align_size;
		}
		//otherwise next comp starts after last value for this comp
		else	
			offset += comp.plane_size;

		comp_row_size += comp.line_size;
		tile_size += comp.plane_size;
		bsw.plane_size = comp.plane_size;
		bs_array.push(bsw);

		//tile size is per comp in this mode, not for the cumulated size
		if (filter.interleave==INTERLEAVE_TILE)
			bsw.tile_size = bsw.plane_size;
	}
	if (tile_align_bytes)
		tile_size = tile_align_bytes;

	bs_array.forEach(bsw => {
		bsw.comp_row_size = comp_row_size;
		if (filter.interleave!=INTERLEAVE_TILE)
			bsw.tile_size = tile_size;
	});

}

function start_tile(tile_x, tile_y)
{
	if ((filter.tiles.x==1) && (filter.tiles.y==1)) return;

	tile_start_offset = tile_align_bytes * (tile_x + filter.tiles.x * tile_y);

	bs_array.forEach(bsw  => {
		let offs = tile_align_bytes ? tile_start_offset : (bsw.tile_size * (tile_x + filter.tiles.x * tile_y));
		offs += bsw.init_offset;
		bsw.bs.pos = offs;
		bsw.tile_start_pos = offs;
		bsw.line_start_pos = offs;
	});
}

function end_tile()
{
	let max_pos=0;
	let max_bs=null;
	bs_array.forEach (bsw => {
		//per comp tile alignment, only in INTERLEAVE_TILE mode
		if (bsw.tile_align_size) {
			let remain = bsw.bs.pos - bsw.tile_start_pos;
			while (remain<bsw.tile_align_size) {
				bsw.bs.put_u8(0);
				remain++;
			}
		}
		if (max_pos<bsw.bs.pos) {
			max_pos = bsw.bs.pos;
			max_bs = bsw.bs;
		}
	});

	if (tile_align_bytes) {		
		let remain = max_pos - tile_start_offset;
		if (remain<0) throw "Internal error in tile padding: remainder is negative " + remain;
		if (remain>tile_align_bytes) throw "Internal error in tile padding: remainder " + remain + " is greater than tile size " + tile_align_bytes + ' tile start ' + tile_start_offset;

		while (remain<tile_align_bytes) {
			max_bs.put_u8(0);
			remain++;
		}
	}
}

function end_line(last_line_in_tile)
{
	bs_array.forEach (bsw => {
		if (filter.block_size && bsw.next_block_idx) {
			flush_block(bsw); 
		}
		//rows of tiles shall be byte-aligned at the end of the row
		bsw.bs.align();
		if (bsw.row_align_size) {
			let remain = bsw.bs.pos - bsw.line_start_pos;
			while (remain<bsw.row_align_size) {
				bsw.bs.put_u8(0);
				remain++;
			}
		}
		//not last line of tile, update offsets
		//for last line we don't update, so that padding can be properly computed. We update the line offset at the beginning of next tile 
		if (!last_line_in_tile) {
			//row interleave, seek to next row start
			if (filter.interleave==INTERLEAVE_ROW) {
				bsw.bs.pos = bsw.line_start_pos + bsw.comp_row_size;
			}
			bsw.line_start_pos = bsw.bs.pos;
		}
	});
}

function write_val(bs, val, comp)
{

	if (comp.align) bs.align();

	if (comp.mode==0) {
		if (comp.align) {
			if (comp_le_buf) {
				comp_le_bs.pos = 0;
				comp_le_bs.put_bits(val * comp.max_val, 8*comp.align);
				for (let i=comp.align; i>0; i--) {
					bs.put_u8(comp_le_view[i-1]);
				}
			} else {
				bs.put_bits(val * comp.max_val, 8*comp.align);
			}
		}
		else
			bs.put_bits(val * comp.max_val, comp.bits);
	}
	else if (comp.mode==1) {
		if (comp.bits==32)
			bs.put_float(val);
		else if (comp.bits==64)
			bs.put_double(val);
	}
}

function flush_block(bsw)
{
	let i, bits=0;
	for (i=0; i<bsw.next_block_idx; i++) {
		let comp = bsw.block_comps[i].component;
		if (!comp) break;
		if (comp.align) {
			while (bits % 8) bits++;
			bits += comp.align*8;
		} else {
			bits += comp.bits;
		}
	}
	let pad = blocksize_bits - bits;
	if (pad<0)
		throw "Wrong block size " + blocksize_bits + " vs " + bits + " nb comps " + bsw.next_block_idx;

	if (filter.ble) {
		let bs_le = bsw.le_bs;
		bs_le.pos = 0;

		if (!filter.pad_lsb)
			bs_le.put_bits(0, pad);

		for (i=0; i<bsw.next_block_idx; i++) {
			//check block reverse
			let b_idx = filter.br ? (bsw.next_block_idx - i - 1) : i;
			let cval = bsw.block_comps[b_idx];
			write_val(bs_le, cval.val, cval.component);
			cval.component = null;
		}

		if (filter.pad_lsb)
			bs_le.put_bits(0, pad);

		for (i=filter.block_size; i>0; i--) {
			bsw.bs.put_u8(bsw.le_view[i-1]);
		}
	} else {
		if (!filter.pad_lsb)
			bsw.bs.put_bits(0, pad);

		//big endian, no block reverse
		for (i=0; i<bsw.next_block_idx; i++) {
			let cval = bsw.block_comps[i];
			if (!cval.component) break;
			write_val(bsw.bs, cval.val, cval.component);
			cval.component = null;
		}

		if (filter.pad_lsb)
			bsw.bs.put_bits(0, pad);
	}

	bsw.nb_bits_block=0;
	bsw.next_block_idx = 0;
}



function push_val(bsw, c, comp)
{
	if (filter.block_size) {
		if (comp.align) {
			while (bsw.nb_bits_block % 8)
				bsw.nb_bits_block++;
		}
		let cbits = comp.align ? (comp.align*8) : comp.bits;
		if (bsw.nb_bits_block + cbits > blocksize_bits) {
			flush_block(bsw);
		}

		bsw.block_comps[bsw.next_block_idx].val = c;
		bsw.block_comps[bsw.next_block_idx].component = comp;
		bsw.next_block_idx++;
		bsw.nb_bits_block += cbits;
	} else {
		write_val(bsw.bs, c, comp);
	}
}

function writepix_interleave_0(x, y)
{
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		push_val(bs_array[i], c, comp);
	}
}

function writepix_interleave_0_420(x, y)
{
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		if ((comp.type==2) || (comp.type==3)) {
			if (x%2) continue;
			if (y%2) continue;
		}
		push_val(bs_array[i], c, comp);
	}
}

function writepix_interleave_0_422(x, y)
{
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		if ((comp.type==2) || (comp.type==3)) {
			if (x%2) continue;
		}
		push_val(bs_array[i], c, comp);
	}
}

function writepix_interleave_0_411(x, y)
{
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		if ((comp.type==2) || (comp.type==3)) {
			if (x%4) continue;
		}
		push_val(bs_array[i], c, comp);
	}
}

function writepix_interleave_1(x, y)
{
	let bsw = bs_array[0];
	let psize = bsw.bs.pos;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;

		push_val(bsw, c, comp);
	}
	if (filter.pixel_size) {
		if (filter.block_size) {
			if (bsw.next_block_idx) {
				flush_block(bsw);
			}
		}
		else
			bsw.bs.align();

		psize = bsw.bs.pos - psize;
		if (psize>filter.pixel_size) {
			throw "Invalid pixel_size " + filter.pixel_size + ", less than total size of components " + psize;
		}
		while (psize<filter.pixel_size) {
			bsw.bs.put_u8(0);
			psize++;
		}
	}
}

function writepix_interleave_2_420(x, y)
{
	let bs_uv=null;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		let bs = bs_array[i];
		if ((comp.type==2) || (comp.type==3)) {
			if (x%2) continue;
			if (y%2) continue;
			if (!bs_uv) bs_uv = bs;
			bs = bs_uv;
		}
		push_val(bs, c, comp);
	}
}

function writepix_interleave_2_422(x, y)
{
	let bs_uv=null;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		let bs = bs_array[i];
		if ((comp.type==2) || (comp.type==3)) {
			if (x%2) continue;
			if (!bs_uv) bs_uv = bs;
			bs = bs_uv;
		}
		push_val(bs, c, comp);
	}
}

function writepix_interleave_2_411(x, y)
{
	let bs_uv=null;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];
		let c = (pix_len>i) ? pixel[i] : 0;
		let bs = bs_array[i];
		if ((comp.type==2) || (comp.type==3)) {
			if (x%4) continue;
			if (!bs_uv) bs_uv = bs;
			bs = bs_uv;
		}
		push_val(bs, c, comp);
	}
}

function writepix_interleave_multiy(x, y)
{
	if (! (x % subsample_x)) {
		for (let i=0; i<components.length; i++) {
			let comp = components[i];
			comp.stored = false;

			if (comp.type==2) comp.store_val = pixel[g_u_idx];
			else if (comp.type==3) comp.store_val = pixel[b_v_idx];
		}
	}
	let types=[];
	let bsw = bs_array[0];
	let psize = bsw.bs.pos;
	for (let i=0; i<components.length; i++) {
		let comp = components[i];

		if (comp.stored) continue;
		//we already wrote a component of that type, we need next pixel
		if (types.includes(comp.type) ) break;

		let c;
		if ((comp.type==2) || (comp.type==3)) { 
			c = comp.store_val;
		} else if (comp.base_idx>=0) {
			c = pixel[comp.base_idx];
		} else {
			c = 0;
		}
		push_val(bsw, c, comp);
		comp.stored = true;
		types.push(comp.type);
	}


	if (filter.pixel_size) {
		if (filter.block_size) {
			if (bsw.next_block_idx)
				flush_block(bws)
		}
		else
			bsw.bs.align();
		psize = bsw.bs.pos - psize;
		if (psize>filter.pixel_size) {
			throw "Invalid pixel_size " + filter.pixel_size + ", less than total size of components " + psize;
		}
		while (psize<filter.pixel_size) {
			bsw.bs.put_u8(0);
			psize++;
		}
	}
}


filter.process = function()
{
	if (!is_playing)
		return GF_EOS;
	if (cts >= max_cts) {
		return GF_EOS;
	}
//	try {

	let pck = pid.new_packet(out_size);

	start_frame(pck.data);

	for (let t_j=0; t_j<filter.tiles.y; t_j++) {	
		for (let t_i=0; t_i<filter.tiles.x; t_i++) {
			let t_y = t_j * tile_height;
			let t_x = t_i * tile_width;
			start_tile(t_i, t_j);

			for (let j=0; j<tile_height; j++) {	
				for (let i=0; i<tile_width; i++) {
					get_pixel(t_x + i, t_y + j);
					this.write_pixel(t_x + i, t_y + j);
				}
				end_line((j+1==tile_height) ? true : false);
			}
			end_tile();
		}
	}

	/*set packet properties and send it*/
	pck.cts = cts;
	pck.sap = GF_FILTER_SAP_1;
	pck.send();
	cts ++;
	if (cts == max_cts) {
		pid.eos = true;
		return GF_EOS;
	}
	print('Frame done ' + cts + '/' + max_cts + '\r');
	return GF_OK;

/*	} catch (e) {
		print('Error writing frame ' + e);
		is_playing=false;
		return GF_EOS;
	}
*/
}

filter.process_event = function(pid, evt)
{
	if (evt.type == GF_FEVT_STOP) {
		is_playing = false;
	} 
	else if (evt.type == GF_FEVT_PLAY) {
		is_playing = true;
	} 
}



