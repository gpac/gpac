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

#include "svg_parse_attributes.h"
#include <gpac/base_coding.h>

/* Basic SVG datatype parsing functions */

#define COLOR_CONVERT(color_name, nred, ngreen, nblue) \
	if (!strcmp(attribute_content, (color_name))) { \
		col->red = INT2FIX(nred) / 255; \
		col->green = INT2FIX(ngreen) / 255; \
		col->blue = INT2FIX(nblue) / 255; \
	}

/* parses an color from a named color HTML or CSS 2 */
static void svg_parse_named_color(SVGParser *parser, SVG_Color *col, xmlChar *attribute_content)
{
	COLOR_CONVERT("aliceblue",240, 248, 255)
	COLOR_CONVERT("antiquewhite",250, 235, 215)
	COLOR_CONVERT("aqua", 0, 255, 255)
	COLOR_CONVERT("aquamarine",127, 255, 212)
	COLOR_CONVERT("azure",240, 255, 255)
	COLOR_CONVERT("beige",245, 245, 220)
	COLOR_CONVERT("bisque",255, 228, 196)
	COLOR_CONVERT("black", 0, 0, 0)
	COLOR_CONVERT("blanchedalmond",255, 235, 205)
	COLOR_CONVERT("blue", 0, 0, 255)
	COLOR_CONVERT("blueviolet",138, 43, 226)
	COLOR_CONVERT("brown",165, 42, 42)
	COLOR_CONVERT("burlywood",222, 184, 135)
	COLOR_CONVERT("cadetblue", 95, 158, 160)
	COLOR_CONVERT("chartreuse",127, 255, 0)
	COLOR_CONVERT("chocolate",210, 105, 30)
	COLOR_CONVERT("coral",255, 127, 80)
	COLOR_CONVERT("lightpink",255, 182, 193)
	COLOR_CONVERT("lightsalmon",255, 160, 122)
	COLOR_CONVERT("lightseagreen", 32, 178, 170)
	COLOR_CONVERT("lightskyblue",135, 206, 250)
	COLOR_CONVERT("lightslategray",119, 136, 153)
	COLOR_CONVERT("lightslategrey",119, 136, 153)
	COLOR_CONVERT("lightsteelblue",176, 196, 222)
	COLOR_CONVERT("lightyellow",255, 255, 224)
	COLOR_CONVERT("lime", 0, 255, 0)
	COLOR_CONVERT("limegreen", 50, 205, 50)
	COLOR_CONVERT("linen",250, 240, 230)
	COLOR_CONVERT("magenta",255, 0, 255)
	COLOR_CONVERT("maroon",128, 0, 0)
	COLOR_CONVERT("mediumaquamarine",102, 205, 170)
	COLOR_CONVERT("mediumblue", 0, 0, 205)
	COLOR_CONVERT("mediumorchid",186, 85, 211)
	COLOR_CONVERT("cornflowerblue",100, 149, 237)
	COLOR_CONVERT("cornsilk",255, 248, 220)
	COLOR_CONVERT("crimson",220, 20, 60)
	COLOR_CONVERT("cyan", 0, 255, 255)
	COLOR_CONVERT("darkblue", 0, 0, 139)
	COLOR_CONVERT("darkcyan", 0, 139, 139)
	COLOR_CONVERT("darkgoldenrod",184, 134, 11)
	COLOR_CONVERT("darkgray",169, 169, 169)
	COLOR_CONVERT("darkgreen", 0, 100, 0)
	COLOR_CONVERT("darkgrey",169, 169, 169)
	COLOR_CONVERT("darkkhaki",189, 183, 107)
	COLOR_CONVERT("darkmagenta",139, 0, 139)
	COLOR_CONVERT("darkolivegreen", 85, 107, 47)
	COLOR_CONVERT("darkorange",255, 140, 0)
	COLOR_CONVERT("darkorchid",153, 50, 204)
	COLOR_CONVERT("darkred",139, 0, 0)
	COLOR_CONVERT("darksalmon",233, 150, 122)
	COLOR_CONVERT("darkseagreen",143, 188, 143)
	COLOR_CONVERT("darkslateblue", 72, 61, 139)
	COLOR_CONVERT("darkslategray", 47, 79, 79)
	COLOR_CONVERT("darkslategrey", 47, 79, 79)
	COLOR_CONVERT("darkturquoise", 0, 206, 209)
	COLOR_CONVERT("darkviolet",148, 0, 211)
	COLOR_CONVERT("deeppink",255, 20, 147)
	COLOR_CONVERT("deepskyblue", 0, 191, 255)
	COLOR_CONVERT("dimgray",105, 105, 105)
	COLOR_CONVERT("dimgrey",105, 105, 105)
	COLOR_CONVERT("dodgerblue", 30, 144, 255)
	COLOR_CONVERT("firebrick",178, 34, 34)
	COLOR_CONVERT("floralwhite",255, 250, 240)
	COLOR_CONVERT("forestgreen", 34, 139, 34)
	COLOR_CONVERT("fuchsia",255, 0, 255)
	COLOR_CONVERT("gainsboro",220, 220, 220)
	COLOR_CONVERT("ghostwhite",248, 248, 255)
	COLOR_CONVERT("gold",255, 215, 0)
	COLOR_CONVERT("goldenrod",218, 165, 32)
	COLOR_CONVERT("gray",128, 128, 128)
	COLOR_CONVERT("grey",128, 128, 128)
	COLOR_CONVERT("green", 0, 128, 0)
	COLOR_CONVERT("greenyellow",173, 255, 47)
	COLOR_CONVERT("honeydew",240, 255, 240)
	COLOR_CONVERT("hotpink",255, 105, 180)
	COLOR_CONVERT("indianred",205, 92, 92)
	COLOR_CONVERT("indigo", 75, 0, 130)
	COLOR_CONVERT("ivory",255, 255, 240)
	COLOR_CONVERT("khaki",240, 230, 140)
	COLOR_CONVERT("lavender",230, 230, 250)
	COLOR_CONVERT("lavenderblush",255, 240, 245)
	COLOR_CONVERT("mediumpurple",147, 112, 219)
	COLOR_CONVERT("mediumseagreen", 60, 179, 113)
	COLOR_CONVERT("mediumslateblue",123, 104, 238)
	COLOR_CONVERT("mediumspringgreen", 0, 250, 154)
	COLOR_CONVERT("mediumturquoise", 72, 209, 204)
	COLOR_CONVERT("mediumvioletred",199, 21, 133)
	COLOR_CONVERT("midnightblue", 25, 25, 112)
	COLOR_CONVERT("mintcream",245, 255, 250)
	COLOR_CONVERT("mistyrose",255, 228, 225)
	COLOR_CONVERT("moccasin",255, 228, 181)
	COLOR_CONVERT("navajowhite",255, 222, 173)
	COLOR_CONVERT("navy", 0, 0, 128)
	COLOR_CONVERT("oldlace",253, 245, 230)
	COLOR_CONVERT("olive",128, 128, 0)
	COLOR_CONVERT("olivedrab",107, 142, 35)
	COLOR_CONVERT("orange",255, 165, 0)
	COLOR_CONVERT("orangered",255, 69, 0)
	COLOR_CONVERT("orchid",218, 112, 214)
	COLOR_CONVERT("palegoldenrod",238, 232, 170)
	COLOR_CONVERT("palegreen",152, 251, 152)
	COLOR_CONVERT("paleturquoise",175, 238, 238)
	COLOR_CONVERT("palevioletred",219, 112, 147)
	COLOR_CONVERT("papayawhip",255, 239, 213)
	COLOR_CONVERT("peachpuff",255, 218, 185)
	COLOR_CONVERT("peru",205, 133, 63)
	COLOR_CONVERT("pink",255, 192, 203)
	COLOR_CONVERT("plum",221, 160, 221)
	COLOR_CONVERT("powderblue",176, 224, 230)
	COLOR_CONVERT("purple",128, 0, 128)
	COLOR_CONVERT("red",255, 0, 0)
	COLOR_CONVERT("rosybrown",188, 143, 143)
	COLOR_CONVERT("royalblue", 65, 105, 225)
	COLOR_CONVERT("saddlebrown",139, 69, 19)
	COLOR_CONVERT("salmon",250, 128, 114)
	COLOR_CONVERT("sandybrown",244, 164, 96)
	COLOR_CONVERT("seagreen", 46, 139, 87)
	COLOR_CONVERT("seashell",255, 245, 238)
	COLOR_CONVERT("sienna",160, 82, 45)
	COLOR_CONVERT("silver",192, 192, 192)
	COLOR_CONVERT("skyblue",135, 206, 235)
	COLOR_CONVERT("slateblue",106, 90, 205)
	COLOR_CONVERT("slategray",112, 128, 144)
	COLOR_CONVERT("slategrey",112, 128, 144)
	COLOR_CONVERT("snow",255, 250, 250)
	COLOR_CONVERT("springgreen", 0, 255, 127)
	COLOR_CONVERT("steelblue", 70, 130, 180)
	COLOR_CONVERT("tan",210, 180, 140)
	COLOR_CONVERT("teal", 0, 128, 128)
	COLOR_CONVERT("lawngreen",124, 252, 0)
	COLOR_CONVERT("lemonchiffon",255, 250, 205)
	COLOR_CONVERT("lightblue",173, 216, 230)
	COLOR_CONVERT("lightcoral",240, 128, 128)
	COLOR_CONVERT("lightcyan",224, 255, 255)
	COLOR_CONVERT("lightgoldenrodyellow",250, 250, 210)
	COLOR_CONVERT("lightgray",211, 211, 211)
	COLOR_CONVERT("lightgreen",144, 238, 144)
	COLOR_CONVERT("lightgrey",211, 211, 211)
	COLOR_CONVERT("thistle",216, 191, 216)
	COLOR_CONVERT("tomato",255, 99, 71)
	COLOR_CONVERT("turquoise", 64, 224, 208)
	COLOR_CONVERT("violet",238, 130, 238)
	COLOR_CONVERT("wheat",245, 222, 179)
	COLOR_CONVERT("white",255, 255, 255)
	COLOR_CONVERT("whitesmoke",245, 245, 245)
	COLOR_CONVERT("yellow",255, 255, 0)
	COLOR_CONVERT("yellowgreen",154, 205, 50)
}

/* Reads an SVG Color 
   either #RRGGBB, #RGB, rgb(r,g,b) in [0,255] , colorname, or 'r g b' in [0,1]
   ignores any space, comma, semi-column before and any space after
   TODO: 
	transform the xmlChar into char and duplicate the input, instead of modifying it
*/
void svg_parse_color(SVGParser *parser, SVG_Color *col, char *attribute_content)
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
	} else if ((str[0] >= 'a' && str[0] <= 'z')
		|| (str[0] >= 'A' && str[0] <= 'Z')) {
		svg_parse_named_color(parser, col, str);
	} else {
		Float _r, _g, _b;
		sscanf(str, "%f %f %f", &_r, &_g, &_b);
		col->red = FLT2FIX(_r);
		col->green = FLT2FIX(_g);
		col->blue = FLT2FIX(_b);
	}
	col->type = SVG_COLOR_RGBCOLOR;

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
	Float _val;
	u32 i = 0;
	while (d[i] != 0 && (d[i] == ' ' || d[i] == ',' || d[i] == ';')) i++;
	if (!d[i]) goto end;
	if (d[i] == '+' || d[i] == '-') i++;
	while (d[i] >= '0' && d[i] <= '9' && d[i] != 0) i++;
	if (!d[i]) goto end;
	if (d[i] == '.') {
		i++;
		while (d[i] >= '0' && d[i] <= '9' && d[i] != 0) i++;
		if (!d[i]) goto end;
	}
	if (d[i] == 'e' || d[i] == 'E') {
		i++;
		if (d[i] == '+' || d[i] == '-') i++;
		while (d[i] >= '0' && d[i] <= '9' && d[i] != 0) i++;
	}
end:
	sscanf(d,"%f", &_val);
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
void smil_parse_time(SVGParser *parser, SVGElement *e, SMIL_Time *v, char *d) 
{
	u32 len;
	char *tmp;
	
	/* Offset Values */
	if ((d[0] >= '0' && d[0] <= '9') || d[0] == '+' || d[0] == '-'){
		v->type = SMIL_TIME_CLOCK;
		svg_parse_clock_value(d, &(v->clock));
		return;
	} 
	
	/* Indefinite Values */
	else if (!strcmp(d, "indefinite")) {
		v->type = SMIL_TIME_INDEFINITE;
		return;
	} 

	/* Wallclock Values */
	else if ((tmp = strstr(d, "wallclock("))) {
		u32 year, month, day;
		u32 hours, minutes;
		u32 nhours, nminutes;
		Float seconds;
		char *tmp1, *tmp2;

		v->type = SMIL_TIME_WALLCLOCK;
		tmp += 10;
		if (tmp1 = strchr(tmp, 'T')) {
			/* From tmp to wallStartTime, we parse a date */
			sscanf(tmp, "%d-%d-%dT", &year, &month, &day);
			tmp1++;
			tmp = tmp1;
		} 	
		if (tmp1 = strchr(tmp, ':')) {
			if (tmp2 = strchr(tmp1, ':')) {
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
			if (tmp1 = strchr(tmp, '+')) {
				sscanf(tmp1, "%d:%d", &nhours, &nminutes);		
			} else if (tmp1 = strchr(tmp, '-')) {
				sscanf(tmp1, "%d:%d", &nhours, &nminutes);		
			}
		}
		return;
	} 

	/* AccessKey Values */
	else if ((tmp = strstr(d, "accessKey("))) {
		v->type = SMIL_TIME_EVENT;
		sscanf(tmp, "accessKey(%c)", &v->parameter);
		tmp = strchr(d, ')');
		tmp++;
		while (*tmp == ' ') tmp++;
		if (*tmp != 0) svg_parse_clock_value(tmp, &(v->clock));
		v->event = SVG_DOM_EVT_KEYPRESS;
		v->element = parser->graph->RootNode;
		v->owner_animation = e;
		return;
	} 

	else {
		char token[500];
		v->type = SMIL_TIME_EVENT;
		v->owner_animation = e;

		if ((tmp = strchr(d, '+')) || (tmp = strchr(d, '-'))) {
			len = tmp - d;
			while (d[len-1] == ' ' && len > 0) len--;
			memcpy(token, d, len);
			token[len] = 0;
			svg_parse_clock_value(tmp, &(v->clock));
		} else {
			strcpy(token, d);
			len = strlen(d);
		}
		if (!strchr(token, '.')) {
			/* animation event name only */
			v->event = svg_get_animation_event_by_name(token);
			v->element = e;
		} else {
			u32 i;
			for (i = 0; i < len; i++) {
				if (token[i] == '\\' && (i+1 < len) && token[i+1] == '.') i++;
				else if (token[i] == '.') {
					/* 0 -> i: element_name
					   i -> len - 1: animation event name */
					GF_SAFEALLOC(v->element_id, i+1);
					memcpy(v->element_id, token, i);
					gf_list_add(parser->unresolved_timing_elements, v);
					v->event = svg_get_animation_event_by_name(token+i+1);
				}
			}
		}

		if (v->event == 3) { //repeat
			tmp = strchr(token, '(');
			if (tmp) {
				tmp++;
				v->parameter = atoi(tmp);
			} else {
				v->parameter = 1;
			}
		}
	}
}

/* Parses an SVG transform attribute and collapses all in the given matrix */
void svg_parse_transform(SVGParser *parser, SVG_Matrix *mat, char *attribute_content) 
{
	SVG_Matrix tmp;

	char *str;
	u32 i;
	
	gf_mx2d_init(*mat);
	
	str = attribute_content;
	i = 0;
	while (str[i] != 0) {
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
		}
	}
}

/* TODO: Change the function to handle elliptical arcs, requires changing data structure */
void svg_parse_path(SVGParser *parser, SVG_PathData *d_attribute, char *attribute_content) 
{
	GF_List *d_commands = d_attribute->commands;
	GF_List *d_points = d_attribute->points;
	char *d = attribute_content;
	Bool first_command = 1;
	if (strlen(d)) {
		Bool pt0_inited, subpath_closed;
		SVG_Point *pt, pt0, cur_pt;
		u8 *command;
		u32 i, k;
		char c, prev_c = 'M';
		i = 0;
		cur_pt.x = cur_pt.y = 0;
		pt0_inited = 0;
		subpath_closed = 0;
		while(1) {
			while ( (d[i]==' ') || (d[i] =='\t') ) i++;			
			c = d[i];
			if (c == 0) break;
next_command:
			switch (c) {
			case 'm':
				if (first_command) cur_pt.x = cur_pt.y = 0;
			case 'M':
			case 'L':
			case 'l':
				i++;
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				if (c == 'M' || c == 'm') *command = 0;
				if (c == 'L' || c == 'l') {
					*command = 1;
					subpath_closed = 0;	
				}
				
				GF_SAFEALLOC(pt, sizeof(SVG_Point))
				gf_list_add(d_points, pt);
				i += svg_parse_float(&(d[i]), &(pt->x), 0);
				i += svg_parse_float(&(d[i]), &(pt->y), 0);				
				if (c == 'm') {
					if (!subpath_closed) {
						pt->x += cur_pt.x;
						pt->y += cur_pt.y;
					} else {
						pt->x += pt0.x;
						pt->y += pt0.y;
					}
				} else if (c == 'l') {
					pt->x += cur_pt.x;
					pt->y += cur_pt.y;
				}
				cur_pt.x = pt->x;
				cur_pt.y = pt->y;
				if (!pt0_inited) {
					pt0.x = cur_pt.x;
					pt0.y = cur_pt.y;
					pt0_inited = 1;
				}
				prev_c = c;
				break;
			case 'H':
			case 'h':
				i++;				
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 1;
				subpath_closed = 0;

				GF_SAFEALLOC(pt, sizeof(SVG_Point))
				gf_list_add(d_points, pt);
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
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 1;
				subpath_closed = 0;

				GF_SAFEALLOC(pt, sizeof(SVG_Point))
				gf_list_add(d_points, pt);
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
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 2;
				subpath_closed = 0;
				
				for (k=0; k<3; k++) {
					GF_SAFEALLOC(pt, sizeof(SVG_Point))
					gf_list_add(d_points, pt);
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
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 3;
				subpath_closed = 0;
				
				for (k=0; k<2; k++) {
					GF_SAFEALLOC(pt, sizeof(SVG_Point))
					gf_list_add(d_points, pt);
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
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 4;
				subpath_closed = 0;
				
				for (k=0; k<2; k++) {
					GF_SAFEALLOC(pt, sizeof(SVG_Point))
					gf_list_add(d_points, pt);
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
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 5;
				subpath_closed = 0;
				
				GF_SAFEALLOC(pt, sizeof(SVG_Point))
				gf_list_add(d_points, pt);
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
					GF_SAFEALLOC(command, sizeof(u8))
					gf_list_add(d_commands, command);
					*command = 7;
					subpath_closed = 0;
	
					GF_SAFEALLOC(pt, sizeof(SVG_Point))
					gf_list_add(d_points, pt);
					i += svg_parse_float(&(d[i]), &(pt->x), 0);	
					i += svg_parse_float(&(d[i]), &(pt->y), 0);				

					i += svg_parse_float(&(d[i]), &(tmp), 0);	
					i += svg_parse_float(&(d[i]), &(tmp), 0);				
					i += svg_parse_float(&(d[i]), &(tmp), 0);	
					
					GF_SAFEALLOC(pt, sizeof(SVG_Point))
					gf_list_add(d_points, pt);
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
				GF_SAFEALLOC(command, sizeof(u8))
				gf_list_add(d_commands, command);
				*command = 6;
				subpath_closed = 1;
				prev_c = c;
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
			first_command = 0;
		}
	}
}

/* Parses a paint attribute: none, inherit or color */
void svg_parse_paint(SVGParser *parser, SVG_Paint *paint, char *attribute_content)
{
	if (!strcmp(attribute_content, "none")) {
		paint->type = SVG_PAINT_NONE;
	} else if (!strcmp(attribute_content, "inherit")) {
		paint->type = SVG_PAINT_INHERIT;
	} else if (!strnicmp(attribute_content, "url(", 4) ) {
		paint->type = SVG_PAINT_URI;
		paint->uri = strdup(attribute_content+4);
		paint->uri[strlen(paint->uri)-1] = 0;
	} else {
		paint->type = SVG_PAINT_COLOR;
		svg_parse_color(parser, paint->color, attribute_content);
	}
}

void svg_convert_length_unit_to_user_unit(SVGParser *parser, SVG_Length *length)
{
	// Assuming the environment is 90dpi
	switch (length->type) {
	case SVG_LENGTH_PERCENTAGE:
		break;
	case SVG_LENGTH_EMS:
		break;
	case SVG_LENGTH_EXS:
		break;
	case SVG_LENGTH_NUMBER:
		break;
	case SVG_LENGTH_PX:
		length->type = SVG_LENGTH_NUMBER;
		break;
	case SVG_LENGTH_CM:
		length->type = SVG_LENGTH_NUMBER;
		length->number = gf_mulfix(length->number, FLT2FIX(35.43307f));
		break;
	case SVG_LENGTH_MM:
		length->type = SVG_LENGTH_NUMBER;
		length->number = gf_mulfix(length->number, FLT2FIX(3.543307f));
		break;
	case SVG_LENGTH_IN:
		length->type = SVG_LENGTH_NUMBER;
		length->number *= 90;
		break;
	case SVG_LENGTH_PT:
		length->type = SVG_LENGTH_NUMBER;
		length->number *= 5; length->number /= 4;
		break;
	case SVG_LENGTH_PC:
		length->type = SVG_LENGTH_NUMBER;
		length->number *= 15;
		break;
	case SVG_LENGTH_INHERIT:
		break;
	}
}

u32 svg_parse_length(SVGParser *parser, SVG_Length *length, char *value_string)
{
	char *unit = NULL;
	u32 len = 0;
	if (!strcmp(value_string, "inherit")) {
		length->type = SVG_LENGTH_INHERIT;
		return 7;
	} else if (unit = strstr(value_string, "%")) {
		length->type = SVG_LENGTH_PERCENTAGE;
	} else if (unit = strstr(value_string, "em")) {
		length->type = SVG_LENGTH_EMS;
	} else if (unit = strstr(value_string, "ex")) {
		length->type = SVG_LENGTH_EXS;
	} else if (unit = strstr(value_string, "px")) {
		length->type = SVG_LENGTH_PX;
	} else if (unit = strstr(value_string, "cm")) {
		length->type = SVG_LENGTH_CM;
	} else if (unit = strstr(value_string, "mm")) {
		length->type = SVG_LENGTH_MM;
	} else if (unit = strstr(value_string, "in")) {
		length->type = SVG_LENGTH_IN;
	} else if (unit = strstr(value_string, "pt")) {
		length->type = SVG_LENGTH_PT;
	} else if (unit = strstr(value_string, "pc")) {
		length->type = SVG_LENGTH_PC;
	} else {
		length->type = SVG_LENGTH_NUMBER;
	}
	if (unit) len = unit - value_string; 
	len+=svg_parse_float(value_string, &(length->number), 0);

	return len;
}

void svg_parse_visibility(SVGParser *parser, SVG_Visibility *value, char *value_string)
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

void svg_parse_display(SVGParser *parser, SVG_Display *value, char *value_string)
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

void svg_parse_displayalign(SVGParser *parser, SVG_DisplayAlign *value, char *value_string)
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

void svg_parse_pointerevents(SVGParser *parser, SVG_PointerEvents *value, char *value_string)
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
	} else if (!strcmp(value_string, "none")) {
		*value = SVG_POINTEREVENTS_NONE;
	}
}

void svg_parse_renderinghint(SVGParser *parser, SVG_RenderingHint *value, char *value_string)
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

void svg_parse_vectoreffect(SVGParser *parser, SVG_VectorEffect *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_VECTOREFFECT_INHERIT;
	} else if (!strcmp(value_string, "none")) {
		*value = SVG_VECTOREFFECT_NONE;
	} else if (!strcmp(value_string, "non-scaling-stroke")) {
		*value = SVG_VECTOREFFECT_NONSCALINGSTROKE;
	}
}

void svg_parse_playbackorder(SVGParser *parser, SVG_VectorEffect *value, char *value_string)
{
	if (!strcmp(value_string, "forwardOnly")) {
		*value = SVG_PLAYBACKORDER_FORWARDONLY;
	} else if (!strcmp(value_string, "all")) {
		*value = SVG_PLAYBACKORDER_ALL;
	}
}

void svg_parse_timelinebegin(SVGParser *parser, SVG_TimelineBegin *value, char *value_string)
{
	if (!strcmp(value_string, "onStart")) {
		*value = SVG_TIMELINEBEGIN_ONSTART;
	} else if (!strcmp(value_string, "onLoad")) {
		*value = SVG_TIMELINEBEGIN_ONLOAD;
	}
}

void svg_parse_xmlspace(SVGParser *parser, XML_Space *value, char *value_string)
{
	if (!strcmp(value_string, "default")) {
		*value = XML_SPACE_DEFAULT;
	} else if (!strcmp(value_string, "preserve")) {
		*value = XML_SPACE_PRESERVE;
	}
}

void svg_parse_xmlev_propagate(SVGParser *parser, XMLEV_Propagate *value, char *value_string)
{
	if (!strcmp(value_string, "continue")) {
		*value = XMLEVENT_PROPAGATE_CONTINUE;
	} else if (!strcmp(value_string, "stop")) {
		*value = XMLEVENT_PROPAGATE_STOP;
	}
}

void svg_parse_xmlev_defaultAction(SVGParser *parser, XMLEV_DefaultAction *value, char *value_string)
{
	if (!strcmp(value_string, "cancel")) {
		*value = XMLEVENT_DEFAULTACTION_CANCEL;
	} else if (!strcmp(value_string, "perform")) {
		*value = XMLEVENT_DEFAULTACTION_PERFORM;
	}
}

void svg_parse_xmlev_phase(SVGParser *parser, XMLEV_Phase *value, char *value_string)
{
	if (!strcmp(value_string, "default")) {
		*value = XMLEVENT_PHASE_DEFAULT;
	} else if (!strcmp(value_string, "capture")) {
		*value = XMLEVENT_PHASE_CAPTURE;
	}
}

void svg_parse_overflow(SVGParser *parser, SVG_Overflow *value, char *value_string)
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

void svg_parse_textanchor(SVGParser *parser, SVG_TextAnchor *value, char *value_string)
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

void svg_parse_clipfillrule(SVGParser *parser, SVG_FillRule *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_FILLRULE_INHERIT;
	} else if (!strcmp(value_string, "nonzero")) {
		*value = SVG_FILLRULE_NONZERO;
	} else if (!strcmp(value_string, "evenodd")) {
		*value = SVG_FILLRULE_EVENODD;
	} 
}

void svg_parse_strokelinejoin(SVGParser *parser, SVG_StrokeLineJoin *value, char *value_string)
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

void svg_parse_strokelinecap(SVGParser *parser, SVG_StrokeLineCap *value, char *value_string)
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

void svg_parse_fontfamily(SVGParser *parser, SVG_FontFamily *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		value->type = SVG_FONTFAMILY_INHERIT;
	} else {
		value->type = SVG_FONTFAMILY_VALUE;
		value->value = strdup(value_string);
	}
}

void svg_parse_fontstyle(SVGParser *parser, SVG_FontStyle *value, char *value_string)
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

void svg_parse_fontweight(SVGParser *parser, SVG_FontWeight *value, char *value_string)
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

void svg_parse_fontvariant(SVGParser *parser, SVG_FontVariant *value, char *value_string)
{
	if (!strcmp(value_string, "inherit")) {
		*value = SVG_FONTVARIANT_INHERIT;
	} else if (!strcmp(value_string, "normal")) {
		*value = SVG_FONTVARIANT_NORMAL;
	} else if (!strcmp(value_string, "small-caps")) {
		*value = SVG_FONTVARIANT_SMALLCAPS;
	} 
}

void svg_parse_inheritablefloat(SVGParser *parser, SVGInheritableFloat *value, char *value_string, u8 clamp0to1)
{
	if (!strcmp(value_string, "inherit")) {
		value->type = SVG_FLOAT_INHERIT;
	} else {
		Float _val;
		value->type = SVG_FLOAT_VALUE;
		sscanf(value_string, "%f", &_val);
		value->value = FLT2FIX(_val); 
	}
	if (clamp0to1) value->value = MAX(0, MIN(1, value->value));
}

void svg_parse_boolean(SVGParser *parser, SVG_Boolean *value, char *value_string)
{
	if (!stricmp(value_string, "1") || !stricmp(value_string, "true"))
		*value = 1;
	else
		*value = 0;
}

/* Should be called after xlink:href of the animation element has been resolved */
void smil_parse_attributename(SVGParser *parser, SVGElement *animation_element, char *value_string)
{
	GF_FieldInfo xlink_href;
	GF_FieldInfo attribute_name;
	SVGElement *targetElement = NULL;
	GF_FieldInfo targetAttribute;

	gf_node_get_field_by_name((GF_Node *)animation_element, "xlink:href", &xlink_href);
	gf_node_get_field_by_name((GF_Node *)animation_element, "attributeName", &attribute_name);

	targetElement = ((SVG_IRI *)xlink_href.far_ptr)->target;

	if (!targetElement) {
		fprintf(stderr, "Warning: element only forward references supported.\n");
		return;
	}
	if (!gf_node_get_field_by_name((GF_Node *)targetElement, value_string, &targetAttribute)) {
		SMIL_AttributeName *attributename_value = (SMIL_AttributeName *)attribute_name.far_ptr;
		attributename_value->type = targetAttribute.fieldType;
		attributename_value->field_ptr = targetAttribute.far_ptr;
	} else 
		fprintf(stderr, "Error: Attribute %s does not belong to target element %s of type %s.\n", value_string, gf_node_get_name((GF_Node *)targetElement), SVG_GetElementName(gf_node_get_tag((GF_Node *)targetElement)));
}

void smil_parse_time_list(SVGParser *parser, SVGElement *e, GF_List *values, char *begin_or_end_list)
{
	SMIL_Time *value;
	char value_string[500];
	char *str = begin_or_end_list, *tmp;
	u32 len;

	/* get rid of leading spaces */
	while (*str == ' ') str++;

	while (tmp = strchr(str, ';')) {
		len = tmp-str;
		memcpy(value_string, str, len);
		while (value_string[len - 1] == ' ' && len > 0) len--;
		value_string[len] = 0;

		GF_SAFEALLOC(value, sizeof(SMIL_Time))
		smil_parse_time(parser, e, value, value_string);
 		gf_list_add(values, value);

		str = tmp + 1;
		while (*str == ' ') str++;
	}

	len = strlen(str);
	memcpy(value_string, str, len);
	while (value_string[len - 1] == ' ' && len > 0) len--;
	value_string[len] = 0;

	GF_SAFEALLOC(value, sizeof(SMIL_Time))
	smil_parse_time(parser, e, value, value_string);
 	gf_list_add(values, value);

	/* sorting timing values */
	if (gf_list_count(values) > 1) {
		SMIL_Time *v, *sv;
		GF_List *sorted = gf_list_new();
		u32 i;
		u8 added = 0;
		do {
			v = gf_list_get(values, 0);
			gf_list_rem(values, 0);
			added = 0;
			for (i=0; i<gf_list_count(sorted); i++) {
				sv = gf_list_get(sorted, i);
				if (v->type >= SMIL_TIME_EVENT) {
					/* unresolved or indefinite so add at the end of the sorted list */
					gf_list_add(sorted, v);
					added = 1;
					break;
				} else {
					if (sv->type >= SMIL_TIME_EVENT) {
						gf_list_insert(sorted, v, i);
						added = 1;
					} else {
						if (v->clock <= sv->clock) {
							gf_list_insert(sorted, v, i);
							added = 1;
						}
					}
				}
			}
			if (!added) gf_list_add(sorted, v);
		} while (gf_list_count(values) > 0);

		for (i = 0; i < gf_list_count(sorted); i++) {
			gf_list_add(values, gf_list_get(sorted, i));
		}
	}
}

void smil_parse_attributeType(SVGParser *parser, SMIL_AttributeType *value, char *value_string)
{
	if (!strcmp(value_string, "auto")) {
		*value = SMIL_ATTRIBUTETYPE_AUTO;
	} else if (!strcmp(value_string, "XML")) {
		*value = SMIL_ATTRIBUTETYPE_XML;
	} else if (!strcmp(value_string, "CSS")) {
		*value = SMIL_ATTRIBUTETYPE_CSS;
	}
}

void smil_parse_min_max_dur_repeatdur(SVGParser *parser, SMIL_Duration *value, char *value_string)
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

void smil_parse_repeatcount(SVGParser *parser, SMIL_RepeatCount *value, char *value_string)
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

void smil_parse_fill(SVGParser *parser, SMIL_Fill *value, char *value_string)
{
	if (!strcmp(value_string, "freeze")) {
		*value = SMIL_FILL_FREEZE;
	} else if (!strcmp(value_string, "remove")) {
		*value = SMIL_FILL_REMOVE;
	}
}

void smil_parse_restart(SVGParser *parser, SMIL_Restart *value, char *value_string)
{
	if (!strcmp(value_string, "always")) {
		*value = SMIL_RESTART_ALWAYS;
	} else if (!strcmp(value_string, "whenNotActive")) {
		*value = SMIL_RESTART_WHENNOTACTIVE;
	} else if (!strcmp(value_string, "never")) {
		*value = SMIL_RESTART_NEVER;
	}
}

void smil_parse_calcmode(SVGParser *parser, SMIL_CalcMode *value, char *value_string)
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

void smil_parse_additive(SVGParser *parser, SMIL_Additive *value, char *value_string)
{
	if (!strcmp(value_string, "replace")) {
		*value = SMIL_ADDITIVE_REPLACE;
	} else if (!strcmp(value_string, "sum")) {
		*value = SMIL_ADDITIVE_SUM;
	} 
}

void smil_parse_accumulate(SVGParser *parser, SMIL_Accumulate *value, char *value_string)
{
	if (!strcmp(value_string, "none")) {
		*value = SMIL_ACCUMULATE_NONE;
	} else if (!strcmp(value_string, "sum")) {
		*value = SMIL_ACCUMULATE_SUM;
	} 
}

void smil_parse_syncBehaviorOrDefault(SVGParser *parser, SMIL_SyncBehavior *value, char *value_string)
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

void smil_parse_syncToleranceOrDefault(SVGParser *parser, SMIL_SyncTolerance *value, char *value_string)
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

void svg_parse_viewbox(SVGParser *parser, SVG_ViewBox *value, char *value_string)
{
	char *str = value_string;
	u32 i = 0;
	i+=svg_parse_float(&(str[i]), &(value->x), 0);
	i+=svg_parse_float(&(str[i]), &(value->y), 0);
	i+=svg_parse_float(&(str[i]), &(value->width), 0);
	i+=svg_parse_float(&(str[i]), &(value->height), 0);
}

void svg_parse_coordinates(SVGParser *parser, GF_List *values, char *value_string)
{
	u32 i = 0;
	char *str = value_string;
	u32 len = strlen(str);
	while (i < len) {
		SVG_Coordinate *c;
		GF_SAFEALLOC(c, sizeof(SVG_Coordinate));
		i+=svg_parse_length(parser, c, &(str[i]));
		gf_list_add(values, c);
	}
}

void svg_parse_points(SVGParser *parser, GF_List *values, char *value_string)
{
	u32 i = 0;
	char *str = value_string;
	u32 len = strlen(str);
	while (i < len) {
		SVG_Point *p;
		GF_SAFEALLOC(p, sizeof(SVG_Point))
		i+=svg_parse_float(&(str[i]), &(p->x), 0);
		i+=svg_parse_float(&(str[i]), &(p->y), 0);
		gf_list_add(values, p);
	}
}

void svg_parse_floats(SVGParser *parser, GF_List *values, char *value_string, Bool is_angle)
{
	u32 i = 0;
	char *str = value_string;
	u32 len = strlen(str);
	while (i < len) {
		Fixed *f;
		GF_SAFEALLOC(f, sizeof(Fixed));
		i+=svg_parse_float(&(str[i]), f, is_angle);
		gf_list_add(values, f);
	}
}

void svg_parse_strokedasharray(SVGParser *parser, SVG_StrokeDashArray *value, char *value_string)
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
			GF_SAFEALLOC(f, sizeof(Fixed));
			i+=svg_parse_float(&(str[i]), f, 0);
			gf_list_add(values, f);
		}
		vals->count = gf_list_count(values);
		GF_SAFEALLOC(vals->vals, (sizeof(Fixed)*vals->count));
		for (i = 0; i < vals->count; i++) {
			Fixed *f = gf_list_get(values, i);
			vals->vals[i] = *f;
			free(f);
		}
		gf_list_del(values);
		value->type = SVG_STROKEDASHARRAY_ARRAY;
	}
}

void svg_parse_transformlist(SVGParser *parser, GF_List *list, char *value_string)
{
	/* To be fixed to parse all the transform values */
	SVG_Transform *tr;
	GF_SAFEALLOC(tr, sizeof(SVG_Transform));
	svg_parse_transform(parser, &(tr->matrix), value_string);
	gf_list_add(list, tr);
}

void svg_parse_iri(SVGParser *parser, SVGElement *elt, SVG_IRI *iri, char *attribute_content)
{
	if (strstr(attribute_content, "data:")) {
#ifdef USE_GPAC_CACHE_MECHANISM
		char cache_name[MAX_URI_LENGTH];
		char mimeType[100], file_extension[4] = "";
		char encoding[100];
		char *mimeType_end = NULL, *encoding_end = NULL;
		char *data;
		u32 data_size;
		char tmp[MAX_URI_LENGTH];

		{ // initializes the path of the file to be cached
			u32 i, last_sep;
			const char *cache_dir = parser->temp_dir;
			if (!cache_dir || !strlen(cache_dir) ) return;

			if (cache_dir[strlen(cache_dir)-1] != GF_PATH_SEPARATOR) {
				sprintf(cache_name, "%s%c", cache_dir, GF_PATH_SEPARATOR);
			} else {
				strcpy(cache_name, cache_dir);
			}
			strcpy(tmp, parser->file_name);
			last_sep = 0;
			for (i=0; i<strlen(tmp); i++) {
				if (tmp[i] == GF_PATH_SEPARATOR) tmp[i] = '_';
				else if (tmp[i] == '.') {
					tmp[i] = '_';
					last_sep = i;
				} else if (tmp[i] == ':') {
					tmp[i] = '_';
					last_sep = i;
				}
			}
			if (last_sep) tmp[last_sep] = '.';
			strcat(cache_name, tmp);
		}

		mimeType_end = strchr(attribute_content+5, ';');
		if (mimeType_end) {
			memcpy(&mimeType, &(attribute_content[5]), (mimeType_end - attribute_content) - 5);
			mimeType[(mimeType_end - attribute_content) - 5] = 0;
		}
		encoding_end = strchr(mimeType_end, ',');
		if (encoding_end) {
			memcpy(&encoding, mimeType_end+1, (encoding_end - mimeType_end)-1);
			encoding[(encoding_end - mimeType_end)-1] = 0;
		}

		GF_SAFEALLOC(data, strlen(attribute_content)*2)
		if (!strcmp(encoding, "base64")) {
			/* decoding is done in place */
			u32 input_len = strlen(encoding_end+1);
			u32 output_len = strlen(attribute_content)*2;
			data_size = gf_base64_decode(encoding_end+1, input_len, data, output_len);
		} else {
			// TODO: Handle other encoding schemes
		}

		/* determine the name of the file based on the mime type 
		   TODO: handle other mime types */
		if (!strcmp(mimeType,"image/jpeg") || !strcmp(mimeType, "image/jpg") ) {
			strcpy(file_extension,"jpg");
		} else if (!strcmp(mimeType,"image/png")) {
			strcpy(file_extension,"png");
		}
		if (file_extension[0]) {
			FILE *cache_image;
			if (parser->cacheID<10) {
				sprintf(tmp, "image0%i.%s", parser->cacheID, file_extension);
			} else {
				sprintf(tmp, "image%2i.%s", parser->cacheID, file_extension);
			}
			parser->cacheID++;
			strcat(cache_name, tmp);
			cache_image = fopen(cache_name, "wb");
			fwrite(data, data_size, 1, cache_image);
			fclose(cache_image);
		} 
		free(data);

		/* Changes the type of the URI and point it to the cache file */
		iri->type = SVG_IRI_IRI;
		iri->iri = strdup(cache_name);
#endif
	} else {
		/* TODO: Handle xpointer(id()) syntax */
		if (attribute_content[0] == '#') {
			iri->type = SVG_IRI_ELEMENTID;
			iri->target = (SVGElement *)gf_sg_find_node_by_name(parser->graph, &(attribute_content[1]));
			iri->iri_owner = elt;
			if (!iri->target) {
				href_instance *hi = malloc(sizeof(href_instance));
				hi->elt = elt;
				hi->iri = iri;
				iri->iri = strdup(attribute_content);
				gf_list_add(parser->unresolved_hrefs, hi);
			} else {
				/* reference is resolved we can register it 
				   the target_element is referenced by the current element */
				//gf_node_register((GF_Node *)iri->target, (GF_Node *)elt);
			}
		} else {
			iri->type = SVG_IRI_IRI;
			iri->iri = strdup(attribute_content);
		}
	}
}

void svg_parse_zoomandpan(SVGParser *parser, SVG_ZoomAndPan *value, char *value_string)
{
	if (!strcmp(value_string, "disable")) {
		*value = SVG_ZOOMANDPAN_DISABLE;
	} else if (!strcmp(value_string, "magnify")) {
		*value = SVG_ZOOMANDPAN_MAGNIFY;
	} 
}

void svg_parse_preserveaspectratio(SVGParser *parser, SVG_PreserveAspectRatio *par, char *attribute_content)
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

void svg_parse_animatetransform_type(SVGParser *parser, SVG_TransformType *anim_transform_type, char *attribute_content)
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
/* end of Basic SVG datatype parsing functions */

void svg_parse_one_anim_value(SVGParser *parser, SVGElement *elt, SMIL_AnimateValue *anim_value, char *attribute_content, u8 anim_value_type, u8 transform_type)
{
	GF_FieldInfo info;
	info.fieldType = anim_value_type;
	info.name = "animation value";
	info.far_ptr = svg_create_value_from_attributetype(anim_value_type, transform_type);
	if (info.far_ptr) svg_parse_attribute(parser, elt, &info, attribute_content, anim_value_type, transform_type);
	anim_value->value = info.far_ptr;
	anim_value->datatype = anim_value_type;
}

void svg_parse_anim_values(SVGParser *parser, SVGElement *elt, SMIL_AnimateValues *anim_values, char *anim_values_string, u8 anim_value_type, u8 transform_type)
{
	u32 len, i = 0;
	char *str;
	char *value_string;
	s32 psemi = -1;
	GF_FieldInfo info;
	info.fieldType = anim_value_type;
	info.name = "animation values";

	anim_values->datatype = anim_value_type;
	
	len = strlen(anim_values_string);
	str = anim_values_string;
	for (i = 0; i < len+1; i++) {
		if (str[i] == ';' || str[i] == 0) {
			u32 single_value_len = 0;
			single_value_len = i - (psemi+1);
			GF_SAFEALLOC(value_string, single_value_len+1);
			memcpy(value_string, str + (psemi+1), single_value_len);
			value_string[single_value_len] = 0;
			psemi = i;

			info.far_ptr = svg_create_value_from_attributetype(anim_value_type, transform_type);
			if (info.far_ptr) {
				svg_parse_attribute(parser, elt, &info, value_string, anim_value_type, transform_type);
				gf_list_add(anim_values->values, info.far_ptr);
			}

			free(value_string);
		}
	}
}

void svg_parse_transformtype(SVGParser *parser, SVGElement *elt, void *transform, char *value_string, u8 transform_type) 
{
	u32 i = 0;
	switch(transform_type) {
	case SVG_TRANSFORM_TRANSLATE:
		{
			SVG_Point *p = (SVG_Point *)transform;
			i+=svg_parse_float(&(value_string[i]), &(p->x), 0);
			if (value_string[i] == 0) {
				p->y = 0;
			} else {
				i+=svg_parse_float(&(value_string[i]), &(p->y), 0);
			}
		}
		break;
	case SVG_TRANSFORM_SCALE:
		{
			SVG_Point *p = (SVG_Point *)transform;;
			i+=svg_parse_float(&(value_string[i]), &(p->x), 0);
			if (value_string[i] == 0) {
				p->y = p->x;
			} else {
				i+=svg_parse_float(&(value_string[i]), &(p->y), 0);
			}
		}
		break;
	case SVG_TRANSFORM_ROTATE:
		{
			SVG_Point_Angle *p = (SVG_Point_Angle *)transform;;
			i+=svg_parse_float(&(value_string[i]), &(p->angle), 1);
			if (value_string[i] == 0) {
				p->y = p->x = 0;
			} else {
				i+=svg_parse_float(&(value_string[i]), &(p->x), 0);
				i+=svg_parse_float(&(value_string[i]), &(p->y), 0);
			}					
		}
		break;
	case SVG_TRANSFORM_SKEWX:
	case SVG_TRANSFORM_SKEWY:
		{
			Fixed *p = (Fixed *)transform;
			i+=svg_parse_float(&(value_string[i]), p, 1);
		}
		break;
	default:
		{
			SVG_Matrix *m = (SVG_Matrix *)transform;
			svg_parse_transform(parser, m, value_string);
		}
	}	
}

void svg_parse_motionvalue(SVGParser *parser, SVG_Point *p, char *value_string)
{
	u32 i = 0;
	i+=svg_parse_float(&(value_string[i]), &(p->x), 0);
	i+=svg_parse_float(&(value_string[i]), &(p->y), 0);
}

/* Parse an SVG attribute */
void svg_parse_attribute(SVGParser *parser, SVGElement *elt, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type, u8 transform_type)
{
	switch (info->fieldType) {
	case SVG_Boolean_datatype:
		svg_parse_boolean(parser, (SVG_Boolean *)info->far_ptr, attribute_content);
	    break;
	case SVG_Color_datatype:
		svg_parse_color(parser, (SVG_Color *)info->far_ptr, attribute_content);
	    break;
	case SVG_Paint_datatype:
		svg_parse_paint(parser, (SVG_Paint *)info->far_ptr, attribute_content);
		break;

/* beginning of keyword type parsing */
	case SVG_FillRule_datatype:
		svg_parse_clipfillrule(parser, (SVG_FillRule *)info->far_ptr, attribute_content);
		break;
	case SVG_StrokeLineJoin_datatype:
		svg_parse_strokelinejoin(parser, (SVG_StrokeLineJoin *)info->far_ptr, attribute_content);
		break;
	case SVG_StrokeLineCap_datatype:
		svg_parse_strokelinecap(parser, (SVG_StrokeLineCap *)info->far_ptr, attribute_content);
		break;
	case SVG_FontStyle_datatype:
		svg_parse_fontstyle(parser, (SVG_FontStyle *)info->far_ptr, attribute_content);
		break;
	case SVG_FontWeight_datatype:
		svg_parse_fontweight(parser, (SVG_FontWeight *)info->far_ptr, attribute_content);
		break;
	case SVG_FontVariant_datatype:
		svg_parse_fontvariant(parser, (SVG_FontVariant *)info->far_ptr, attribute_content);
		break;
	case SVG_TextAnchor_datatype:
		svg_parse_textanchor(parser, (SVG_TextAnchor *)info->far_ptr, attribute_content);
		break;
	case SVG_Display_datatype:
		svg_parse_display(parser, (SVG_Display *)info->far_ptr, attribute_content);
		break;
	case SVG_Visibility_datatype:
		svg_parse_visibility(parser, (SVG_Visibility *)info->far_ptr, attribute_content);
		break;
	case SVG_Overflow_datatype:
		svg_parse_overflow(parser, (SVG_Overflow *)info->far_ptr, attribute_content);
		break;
	case SVG_ZoomAndPan_datatype:
		svg_parse_zoomandpan(parser, (SVG_ZoomAndPan *)info->far_ptr, attribute_content);
		break;
	case SVG_DisplayAlign_datatype:
		svg_parse_displayalign(parser, (SVG_DisplayAlign *)info->far_ptr, attribute_content);
		break;
	case SVG_PointerEvents_datatype:
		svg_parse_pointerevents(parser, (SVG_PointerEvents *)info->far_ptr, attribute_content);
		break;
	case SVG_RenderingHint_datatype:
		svg_parse_renderinghint(parser, (SVG_RenderingHint *)info->far_ptr, attribute_content);
		break;
	case SVG_VectorEffect_datatype:
		svg_parse_vectoreffect(parser, (SVG_VectorEffect *)info->far_ptr, attribute_content);
		break;
	case SVG_PlaybackOrder_datatype:
		svg_parse_playbackorder(parser, (SVG_PlaybackOrder *)info->far_ptr, attribute_content);
		break;
	case SVG_TimelineBegin_datatype:
		svg_parse_timelinebegin(parser, (SVG_TimelineBegin *)info->far_ptr, attribute_content);
		break;
	case XML_Space_datatype:
		svg_parse_xmlspace(parser, (XML_Space *)info->far_ptr, attribute_content);
		break;
	case XMLEV_Propagate_datatype:
		svg_parse_xmlev_propagate(parser, (XMLEV_Propagate *)info->far_ptr, attribute_content);
		break;
	case XMLEV_DefaultAction_datatype:
		svg_parse_xmlev_defaultAction(parser, (XMLEV_DefaultAction *)info->far_ptr, attribute_content);
		break;
	case XMLEV_Phase_datatype:
		svg_parse_xmlev_phase(parser, (XMLEV_Phase *)info->far_ptr, attribute_content);
		break;
	case SMIL_SyncBehavior_datatype:
		smil_parse_syncBehaviorOrDefault(parser, (SMIL_SyncBehavior *)info->far_ptr, attribute_content);
		break;
	case SMIL_SyncTolerance_datatype:
		smil_parse_syncToleranceOrDefault(parser, (SMIL_SyncTolerance *)info->far_ptr, attribute_content);
		break;
	case SMIL_AttributeType_datatype:
		smil_parse_attributeType(parser, (SMIL_AttributeType *)info->far_ptr, attribute_content);
		break;	
	case SMIL_CalcMode_datatype:
		smil_parse_calcmode(parser, (SMIL_CalcMode *)info->far_ptr, attribute_content);
		break;
	case SMIL_Additive_datatype:
		smil_parse_additive(parser, (SMIL_CalcMode *)info->far_ptr, attribute_content);
		break;
	case SMIL_Accumulate_datatype:
		smil_parse_accumulate(parser, (SMIL_Accumulate *)info->far_ptr, attribute_content);
		break;
	case SMIL_Restart_datatype:
		smil_parse_restart(parser, (SMIL_Restart *)info->far_ptr, attribute_content);
		break;
	case SMIL_Fill_datatype:
		smil_parse_fill(parser, (SMIL_Fill *)info->far_ptr, attribute_content);
		break;

	case SVG_GradientUnit_datatype:
		*((SVG_GradientUnit *)info->far_ptr) = !stricmp(attribute_content, "userSpaceOnUse") ? SVG_GRADIENTUNITS_USER : SVG_GRADIENTUNITS_OBJECT;
		break;
/* end of keyword type parsing */

	/* inheritable floats */
	case SVG_Opacity_datatype:
		svg_parse_inheritablefloat(parser, (SVGInheritableFloat *)info->far_ptr, attribute_content, 1);
		break;
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
		svg_parse_inheritablefloat(parser, (SVGInheritableFloat *)info->far_ptr, attribute_content, 0);
		break;

	/* inheritable float and unit */
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		svg_parse_length(parser, (SVG_Length *)info->far_ptr, attribute_content);
		break;

	case SMIL_AnimateValue_datatype:
		svg_parse_one_anim_value(parser, elt, (SMIL_AnimateValue *)info->far_ptr, attribute_content, anim_value_type, transform_type);
		break;
	case SMIL_AnimateValues_datatype:
		svg_parse_anim_values(parser, elt, (SMIL_AnimateValues *)info->far_ptr, attribute_content, anim_value_type, transform_type);
		break;
	case SVG_IRI_datatype:
		svg_parse_iri(parser, elt, (SVG_IRI*)info->far_ptr, attribute_content);
		break;
	case SMIL_AttributeName_datatype:
		/* Should be not called here but directly */
		break;
	case SMIL_Times_datatype:
		smil_parse_time_list(parser, elt, *(GF_List **)info->far_ptr, attribute_content);
		break;
	case SMIL_Duration_datatype:
		smil_parse_min_max_dur_repeatdur(parser, (SMIL_Duration *)info->far_ptr, attribute_content);
		break;
	case SMIL_RepeatCount_datatype:
		smil_parse_repeatcount(parser, (SMIL_RepeatCount *)info->far_ptr, attribute_content);
		break;

	case SVG_PathData_datatype:
		svg_parse_path(parser, (SVG_PathData *)info->far_ptr, attribute_content);
		break;
	case SVG_Points_datatype:
		svg_parse_points(parser, *(GF_List **)(info->far_ptr), attribute_content);
		break;
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
		svg_parse_floats(parser, *(GF_List **)(info->far_ptr), attribute_content, 0);
		break;
	case SVG_Coordinates_datatype:
		svg_parse_coordinates(parser, *(GF_List **)(info->far_ptr), attribute_content);
		break;
	case SVG_ViewBox_datatype:
		svg_parse_viewbox(parser, info->far_ptr, attribute_content);
		break;
	case SVG_StrokeDashArray_datatype:
		svg_parse_strokedasharray(parser, (SVG_StrokeDashArray *)info->far_ptr, attribute_content);
		break;
	case SVG_FontFamily_datatype:
		svg_parse_fontfamily(parser, (SVG_FontFamily *)info->far_ptr, attribute_content);
		break;
	case SVG_TransformList_datatype:
		svg_parse_transformlist(parser, *(GF_List **)info->far_ptr, attribute_content);
		break;
	case SVG_PreserveAspectRatio_datatype:
		svg_parse_preserveaspectratio(parser, (SVG_PreserveAspectRatio *)info->far_ptr, attribute_content);
		break;
	case SVG_Number_datatype:
		svg_parse_float(attribute_content, (SVG_Number *)info->far_ptr, 0);
		break;

	/* required for animateMotion */
	case SVG_Motion_datatype:
		svg_parse_motionvalue(parser, (SVG_Point *)info->far_ptr, attribute_content);
		break;
	/* required for animateTransform */
	case SVG_TransformType_datatype:
		svg_parse_transformtype(parser, elt, info->far_ptr, attribute_content, transform_type);
		break;

	case SVG_String_datatype:
//		*(SVG_String *)info->far_ptr = strdup(attribute_content);
		break;
	case SVG_ContentType_datatype:
		*(SVG_String *)info->far_ptr = strdup(attribute_content);
		break;
	default:
		fprintf(stdout, "Warning: skipping unsupported attribute %s\n", info->name);
		parser->last_error = GF_OK;
	}
}

void svg_parse_one_style(SVGParser *parser, SVGElement *elt, char *one_style) 
{
	GF_FieldInfo info;
	char *c;
	u32 attributeNameLen;
	char *attributeName;

	while (*one_style == ' ') one_style++;
	c = strchr(one_style, ':');
	if (!c) return;
	attributeNameLen = (c - one_style);
	attributeName = malloc(attributeNameLen+1);
	memcpy(attributeName, one_style, attributeNameLen);
	attributeName[attributeNameLen] = 0;
	if (!gf_node_get_field_by_name((GF_Node *)elt, attributeName, &info)) {
		c++;
		svg_parse_attribute(parser, elt, &info, c, 0, 0);
	} else {
#ifndef _WIN32_WCE
		fprintf(stderr, "Error: Attribute %s does not belong to element %s.\n", attributeName, SVG_GetElementName(gf_node_get_tag((GF_Node*)elt)));
#endif
	}
}

void svg_parse_style(SVGParser *parser, SVGElement *elt, char *style) 
{
	u32 i = 0;
	u32 len = strlen(style);
	char *str = style;
	s32 psemi = -1;
	
	for (i = 0; i < len+1; i++) {
		if (str[i] == ';' || str[i] == 0) {
			char *value_string;
			u32 single_value_len = 0;
			single_value_len = i - (psemi+1);
			if (single_value_len) {
				GF_SAFEALLOC(value_string, single_value_len+1);
				memcpy(value_string, str + (psemi+1), single_value_len);
				value_string[single_value_len] = 0;
				psemi = i;
				svg_parse_one_style(parser, elt, value_string);
				free(value_string);
			}
		}
	}

}

void *svg_create_value_from_attributetype(u8 attribute_type, u8 transform_type)
{
	switch (attribute_type) {
	case SVG_Boolean_datatype:
		{
			SVG_Boolean *b;
			GF_SAFEALLOC(b, sizeof(SVG_Boolean_datatype))
			return b;
		}
		break;
	case SVG_Color_datatype:
		{
			SVG_Color *color;
			GF_SAFEALLOC(color, sizeof(SVG_Color))
			return color;
		}
		break;
	case SVG_Paint_datatype:
		{
			SVG_Paint *paint;				
			GF_SAFEALLOC(paint, sizeof(SVG_Paint));
			GF_SAFEALLOC(paint->color, sizeof(SVG_Color));
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
		{
			u8 *keyword;
			GF_SAFEALLOC(keyword, sizeof(u8))
			return keyword;
		}
		break;

	/* inheritable floats */
	case SVG_Opacity_datatype:
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
		{
			SVGInheritableFloat *p;
			GF_SAFEALLOC(p, sizeof(SVGInheritableFloat));
			return p;
		}
		break;

	/* inheritable float and unit */
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
		{
			SVG_Length *length;
			GF_SAFEALLOC(length, sizeof(SVG_Length))
			return length;
		}
		break;	
	
	case SVG_StrokeDashArray_datatype:
		{
			SVG_StrokeDashArray *array;
			GF_SAFEALLOC(array, sizeof(SVG_StrokeDashArray))
			return array;
		}
		break;

	case SVG_TransformList_datatype:
		{
			SVG_Matrix *m;
			GF_SAFEALLOC(m, sizeof(SVG_Matrix))
			return m;
		}
		break;
	case SVG_TransformType_datatype:
		{
			switch(transform_type) {
			case SVG_TRANSFORM_TRANSLATE:
			case SVG_TRANSFORM_SCALE:
				{
					SVG_Point *p;
					GF_SAFEALLOC(p, sizeof(SVG_Point));
					return p;
				}
				break;
			case SVG_TRANSFORM_ROTATE:
				{
					SVG_Point_Angle *p;
					GF_SAFEALLOC(p, sizeof(SVG_Point_Angle));
					return p;
				}
				break;
			case SVG_TRANSFORM_SKEWX:
			case SVG_TRANSFORM_SKEWY:
				{
					Fixed *p;
					GF_SAFEALLOC(p, sizeof(Fixed));
					return p;
				}
				break;
			default:
				{
					SVG_Matrix *m;
					GF_SAFEALLOC(m, sizeof(SVG_Matrix))
					return m;
				}
			}
		}
		break;
	case SVG_Motion_datatype:
		{
			SVG_Point *p;
			GF_SAFEALLOC(p, sizeof(SVG_Point));
			return p;
		}
		break;
	case SVG_ViewBox_datatype:
		{
			SVG_ViewBox *viewbox;
			GF_SAFEALLOC(viewbox, sizeof(SVG_ViewBox))
			return viewbox;
		}
		break;
	case SVG_IRI_datatype:
		{
			SVG_IRI *iri;
			GF_SAFEALLOC(iri, sizeof(SVG_IRI))
			return iri;
		}
		break;
	case SVG_FontFamily_datatype:
		{
			SVG_FontFamily *fontfamily;
			GF_SAFEALLOC(fontfamily, sizeof(SVG_FontFamily))
			return fontfamily;
		}
		break;
	case SVG_String_datatype:
		{
			SVG_String *string;
			GF_SAFEALLOC(string, sizeof(SVG_String))
			return string;
		}
		break;
	case SVG_Points_datatype:
		{
			SVG_Points *points;
			GF_SAFEALLOC(points, sizeof(SVG_Points))
			*points = gf_list_new();
			return points;
		}
		break;
	case SVG_Coordinates_datatype:
		{
			SVG_Coordinates *coords;
			GF_SAFEALLOC(coords, sizeof(SVG_Coordinates))
			*coords = gf_list_new();
			return coords;
		}
		break;
	case SVG_PathData_datatype:
		{
			SVG_PathData *path = malloc(sizeof(SVG_PathData));
			path->commands = gf_list_new();
			path->points = gf_list_new();
			return path;
		}
		break;
	default:
		fprintf(stdout, "Error: Type not supported: %d.\n", attribute_type);
	} 
	return NULL;
}