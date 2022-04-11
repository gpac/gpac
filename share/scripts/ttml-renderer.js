/* JavaScript Renderer for TTML tracks
   Generates GPAC Scene Graph (SVG) to display TTML docs

The renderer is currently extremely basic:
- only displays text samples in a single view area
- no backcolor, no highlight, limited font support
- no image attachments

The SVG scenegraph uses uDOM for scripting

The native code will look for the following functions:

function on_ttml_sample(ttmldoc, scene_time, pck_dur)
 @ttmldoc: DOM document representing the TTML doc
 @scene_time: scene time in milliseconds as int
 @pck_dur: packet duration in seconds as double, if known, or 0 otherwise

 The function may return a positive value in milliseconds indicating to call back the on_ttml_clock in the given amount of time (for animations and text off toggle)
If the return value is <=0, on_ttml_clock will not be called

The ttmldoc may be kept alive in the script for later usage (it is not destroyed by the native code after the on_ttml_sample function is called).

A TTML sample will contain a valid TTML document containing typically a single TTML <p>

function on_ttml_clock(scene_time)
 @scene_time: scene time in milliseconds as int

The function may return a positive value in milliseconds indicating to call back the on_ttml_clock in the given amount of time (for animations and text off toggle)
If the return value is <=0, on_ttml_clock will not be called
 */

var ttml_dbg = false;
var ttml_view_area = false;

function ttml_dbg_msg(msg) {
	if (ttml_dbg) {
		alert(msg);
	}
}

var rect = document.documentElement.getRectPresentationTrait("viewBox");
var width = rect.width;
var height = rect.height;

ttml_dbg_msg("TTML Rendered Loaded size: " + width + "x" + height);

//filter-assignable variables - if changing names, also do it in filters/dec_ttml.c
var xOffset = 0;
var yOffset = 0;
var fontSize = 20;
var textColor = "white";
var fontFamily = "SANS";
var lineSpaceFactor = 1.0;
var v_align = 0;
//end of filter-assignable variables

//for now only one
var display_area = document.createElement("g");
document.documentElement.appendChild(display_area);

function create_text_area(settings) {
	var t = document.createElement("textArea");
	t.setAttribute("x", settings.x_offset);
	t.setAttribute("y", -5);
	var w, h;
	if (settings.align_top==2) {
		t.setAttribute("display-align", "before");
		h=height ;
	} else if (settings.align_top==1) {
		t.setAttribute("display-align", "center");
		h=height ;//- 2*yOffset;
	} else {
		t.setAttribute("display-align", "after");
		h = height - settings.linePosition ;//- 2*yOffset;
	}
	w = settings.size - 2*( settings.x_offset);
	t.setAttribute("height", h);
	t.setAttribute("width", w);
	ttml_dbg_msg("Creating textArea size " + w + " x " + h + " - offset: " + xOffset + " x " + yOffset);
	
	t.setAttribute("fill", textColor);

	if (height>=720) {
		fontSize = height * 0.05;
	}

	t.setAttribute("font-size", fontSize);
	t.setAttribute("font-family", fontFamily);
	t.setAttribute("text-align", settings.align);
	t.setAttribute("line-increment", lineSpaceFactor*fontSize);


	let g = document.createElement("g");
	let mx = g.getMatrixTrait('transform');
   mx.mTranslate(xOffset, -yOffset);
	g.setMatrixTrait('transform', mx);
	display_area.appendChild(g);

	if (ttml_view_area) {
		var r = document.createElement("rect");
		r.setAttribute("x", settings.x_offset);
		r.setAttribute("y", -5);
		r.setAttribute("fill", "cyan");
		r.setAttribute("width", w);
		r.setAttribute("height", h);
		g.appendChild(r);
	}

	g.appendChild(t);
	ttml_dbg_msg("textArea created: "+t);
	return t;
}

function add_text_span(parent, text, styles) {
	var i;
	var span = document.createElement('tspan');

	if (styles != undefined) {
		if (styles.italic) span.setAttribute("font-style", "italic");
		if (styles.underlined) span.setAttribute("text-decoration", "underline");
		if (styles.bold) span.setAttribute("font-weight", "bold");
		if (styles.colors) span.setAttribute("fill", ""+styles.color);
	}
	span.textContent = text;
	parent.appendChild(span);
	ttml_dbg_msg("tspan created "+span + " in " + parent.tagName);
	return span;
}

function parse_sample_settings(ttmldoc, tsample, reg_id)
{
	ttml_dbg_msg("Parsing sample settings");
	var obj = {};
	obj.linePosition = lineSpaceFactor*fontSize;
	obj.align_top = v_align;
	obj.x_offset = 0;
	obj.size = 100;
	obj.size *= width/100;
	
	obj.align = "center";

	//todo, parse style and region attributes...
/*
	if (reg_id==="")
		return obj;

	var region = ttmldoc.getElementById(reg_id);
	if (region==null) {
		ttml_dbg_msg("region not found: "+reg_id);
		return obj;
	}
	var styles = region.getElementsByTagName("style");
	if (styles.length==0) {
		ttml_dbg_msg("region found but no styles");
		return obj;
	}
	var style = styles.item(0);
	var offset = style.getAttribute('offset');
	var extend = style.getAttribute('extent');
	var disp_align = style.getAttribute('displayAlign');
*/

	return obj;
}

function parse_time(timestamp)
{
	var result = 0;
	if (timestamp.indexOf(':')>0) {
		var nums = timestamp.split(':');
		result = parseInt(nums[0]) * 3600; 
		result += parseInt(nums[1]) * 60;
		result*= 1000;
		result += 1000 * Number.parseFloat(nums[2]); 
	}
	else if (timestamp.indexOf('s')>0) {
		result = 1000 * Number.parseFloat(timestamp); 
	}
	return result;
}

function push_ttml_text_node(textArea, node, styles)
{
	var children = node.childNodes;
		ttml_dbg_msg("child " + node.nodeName + " children " + children.length);

	let nb_italic = styles.italic;
	let nb_bold = styles.bold;
	let nb_underlined = styles.underlined;
	let nb_colors = styles.colors;
	let color = styles.color;
	let att = node.getAttributeNS('*', 'fontStyle');
	if ((att == 'italic') || (att == 'oblique')) styles.italic++;

	att = node.getAttributeNS('*', 'fontWeight');
	if (att == 'bold') styles.bold++;

	att = node.getAttributeNS('*', 'textDecoration');
	if (att == 'underlined') styles.underlined++;

	att = node.getAttributeNS('*', 'color');
	if ((att != null) && att.length) {
		styles.colors++;
		styles.color = att;
	}
	for (var i=0; i<children.length; i++) {
		var t = children.item(i);
		if (t.nodeName ==='br') {
			ttml_dbg_msg("line break");
			textArea.appendChild(document.createElement('tbreak'));
			continue;
		}
		if (t.nodeName ==='i') {
			styles.italic++;
			push_ttml_text_node(textArea, t, styles);
			styles.italic--;
			continue;
		}
		if (t.nodeName ==='b') {
			styles.bold++;
			push_ttml_text_node(textArea, t, styles);
			styles.bold--;
			continue;
		}
		if (t.nodeName ==='u') {
			styles.underlined++;
			push_ttml_text_node(textArea, t, styles);
			styles.underlined--;
			continue;
		}
		if (t.nodeName ==='span') {
			push_ttml_text_node(textArea, t, styles);
			continue;
		}
		ttml_dbg_msg("child " + t.nodeName + " txt " + t.textContent);
		add_text_span(textArea, t.textContent, styles);
	}
	styles.italic = nb_italic;
	styles.bold = nb_bold;
	styles.underlined = nb_underlined;
	styles.colors = nb_colors;
	styles.color = color;

}

let sample_end = 0;

function on_ttml_sample(ttmldoc, scene_time, pck_dur)
{
	ttml_dbg_msg("TTML sample at " + scene_time + " pck dur " + pck_dur);

	sample_end = 0;
	display_area.textContent = '';
	var body = ttmldoc.getElementsByTagName("body");
	var samples = ttmldoc.getElementsByTagName("p");
	var styles = {"italic": 0, "underlined": 0, "bold": 0, "colors": 0, color: null};

	if (samples.length>1) {
		alert("TTML sample has " + samples.length + " defined, only the last sample will be displayed");
	}
	ttml_dbg_msg(samples.length + " text samples");
	var body_region = "";
	if (body.length) {
		var b = body.item(0);
		body_region = b.getAttribute('region');
	}
	for (var i=0; i<samples.length; i++) {
		var sample_start = 0;
		var s = samples.item(i);
		var begin = s.getAttribute('begin');
		var end = s.getAttribute('end');
		var region = s.getAttribute('region');
		var id = s.getAttribute('id');
		if (region==="") region = body_region;

		sample_end = parse_time(end);
		if (begin)
			sample_start = parse_time(begin);

		ttml_dbg_msg("sample start: " + sample_start + " end: " + sample_end + " scenetime: " + scene_time +  " region: " + region + " id: " + id);
		//we're late but check if sample not over
		if (sample_start < scene_time) {
//			if (sample_end < scene_time) 
//				break;
		}

		if (sample_end < scene_time) 
			continue;

		var settings = parse_sample_settings(ttmldoc, s, region);
		var tA = create_text_area(settings);
		push_ttml_text_node(tA, s, styles);
	}
	if (sample_end > scene_time) return sample_end - scene_time;
	return 0;
}

function on_ttml_clock(scene_time)
{
	if (!sample_end) return 0;

	if (sample_end > scene_time) {
		//ttml_dbg_msg("not over yet: " + scene_time + " vs last end " + sample_end);
		return sample_end - scene_time;
	}
	ttml_dbg_msg("sample is over (end: " + sample_end + " - now: " + scene_time + ")" );
	display_area.textContent = '';
	sample_end = 0;
	return 0;
}
