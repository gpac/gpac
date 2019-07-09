/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac application
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

#include <gpac/main.h>
#include <gpac/filters.h>

static GF_SystemRTInfo rti;
static GF_FilterSession *session=NULL;
static u32 list_filters = 0;
static Bool dump_stats = GF_FALSE;
static Bool dump_graph = GF_FALSE;
static Bool print_filter_info = GF_FALSE;
static Bool print_meta_filters = GF_FALSE;
static Bool load_test_filters = GF_FALSE;
static s32 nb_loops = 0;
static s32 runfor = 0;
Bool enable_prompt = GF_FALSE;
u32 enable_reports = 0;
char *report_filter = NULL;
Bool do_unit_tests = GF_FALSE;
static int alias_argc = 0;
static char **alias_argv = NULL;
static GF_List *args_used = NULL;
static GF_List *args_alloc = NULL;
static u32 gen_doc = 0;
static u32 help_flags = 0;

FILE *sidebar_md=NULL;
static FILE *helpout = NULL;


static const char *auto_gen_md_warning = "<!-- automatically generated - do not edit, patch gpac/applications/gpac/main.c -->\n";

//uncomment to check argument description matches our conventions - see filter.h
#define CHECK_DOC

//the default set of separators
static char separator_set[7] = GF_FS_DEFAULT_SEPS;
#define SEP_LINK	5
#define SEP_FRAG	2
#define SEP_LIST	3

static Bool print_filters(int argc, char **argv, GF_FilterSession *session, GF_SysArgMode argmode);
static void dump_all_props(void);
static void dump_all_codec(GF_FilterSession *session);
static void write_filters_options(GF_FilterSession *fsess);
static void write_core_options();
static void write_file_extensions();
static int gpac_make_lang(char *filename);
static Bool gpac_expand_alias(int argc, char **argv);
static u32 gpac_unit_tests(GF_MemTrackerType mem_track);

static Bool revert_cache_file(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info);



#ifndef GPAC_DISABLE_DOC
const char *gpac_doc =
"# General\n"
"Filters are configurable processing units consuming and producing data packets. These packets are carried "
"between filters through a data channel called __pid__. A PID is in charge of allocating/tracking data packets, "
"and passing the packets to the destination filter(s). A filter output PID may be connected to zero or more filters. "
"This fan-out is handled internally by GPAC (no such thing as a tee filter in GPAC).\n"
"Note: When a PID cannot be connected to any filter, a warning is thrown and all packets dispatched on "
"this PID will be destroyed. The session may however still run.\n"
"Each output PID carries a set of properties describing the data it delivers (eg __width__, __height__, __codec__, ...). Properties "
"can be built-in (identified by a 4 character code **abcd**, see [properties (-h props)](filters_properties) ), or user-defined (identified by a string). Each PID tracks "
"its properties changes and triggers filter reconfiguration during packet processing. This allows the filter chain to be "
"reconfigured at run time, potentially reloading part of the chain (eg unload a video decoder when switching from compressed "
"to uncompressed sources).\n"
"Each filter exposes one or more sets of capabilities, called __capability bundle__, which are property type and values "
"that must be matched or excluded by connecting PIDs.\n"
"Each filter exposes a set of argument to configure itself, using property types and values described as strings formated with "
"separators. This help is given with default separator sets `:=#,@` to specify filters, properties and options. Use [-seps](GPAC) to change them.\n"
"# Property format\n"
"- boolean: formatted as `yes`|`true`|`1` or `no`|`false`|`0`\n"
"- enumeration (for filter arguments only): must use the syntax given in the argument description, otherwise value `0` (first in enum) is assumed.\n"
"-  1-dimension (numbers, floats, ints...): formatted as `value[unit]`, where `unit` can be `k`|`K` (x1000) or `m`|`M` (x1000000) or `g`|`G` (x1000000000). "
"For such properties, value `+I` means maximum possible value, `-I` minimum possible value.\n"
"- fraction: formatted as `num/den` or `num-den` or `num`, in which case the denominator is 1 if `num` is an integer, or 1000000 if `num` is a floating-point value.\n"
"- unsigned 32 bit integer: formated as number or hexadecimal using the format `0xAABBCCDD`.\n"
"- N-dimension (vectors): formatted as `DIM1xDIM2[xDIM3[xDIM4]]` values, without unit multiplier.\n"
"- data: formatted as:\n"
" - `size@address`: constant data block, not internally copied; `size` gives the size of the block, `address` the data pointer.\n"
" - `0xBYTESTRING`: data block specified in hexadecimal, internally copied.\n"
" - `file@FILE`: load data from local `FILE`(opened in binary mode).\n"
" - `bxml@FILE`: binarize XML from local `FILE`, see https://github.com/gpac/gpac/wiki/NHML-Format.\n"
"- pointer: are formatted as `address` giving the pointer address (32 or 64 bit depending on platforms).\n"
"- string lists: formatted as `val1,val2[,...]\n"
"- interget lists: formatted as `val1,val2[,...]`\n"
"Note: The special characters in property formats (0x,/,-,+I,-I,x) cannot be configured.\n"
"# Filter declaration [__FILTER__]\n"
"## Generic declaration\n"
"Filters are listed by name, with options appended as a list of colon-separated `name=value` pairs. Additionnal syntax is provided for:\n"
"- boolean: `value` can be omitted, defaulting to `true` (eg `:noedit`). Using `!` before the name negates the result (eg `:!moof_first`)\n"
"- enumerations: name can be omitted (eg `:disp=pbo` is equivalent to `:pbo`), provided that filter developers pay attention to not reuse enumeration names in the same filter.\n"
"When string parameters are used (eg URLs), it is recommended to escape the string using the keyword `gpac`.  \n"
"EX filter:ARG=http://foo/bar?yes:gpac:opt=VAL\n"
"This will properly extract the URL.\n"
"EX filter:ARG=http://foo/bar?yes:opt=VAL\n"
"This will fail to extract it and keep `:opt=VAL` as part of the URL.\n"
"The escape mechanism is not needed for local source, for which file existence is probed during argument parsing. "
"It is also not needed for builtin procotol handlers (`avin://`, `video://`, `audio://`, `pipe://`)\n"
"For `tcp://` and `udp://` protocols, the escape is not needed if a trailing `/` is appended after the port number.\n"
"EX -i tcp://127.0.0.1:1234:OPT\n"
"This will fail to extract the URL and options/\n"
"EX -i tcp://127.0.0.1:1234/:OPT\n"
"This will extract the URL and options.\n"
"Note: one trick to avoid the escape sequence is to declare the URLs option at the end, eg `f1:opt1=foo:url=http://bar`, provided you have only one URL parameter to specify on the filter.\n"
"## Source and Sink filters\n"
"Source and sink filters do not need to be addressed by the filter name, specifying `src=` or `dst=` instead is enough. "
"You can also use the syntax `-src URL` or `-i URL` for sources and `-dst URL` or `-o URL` for destination, this allows prompt completion in shells\n"
"EX src=file.mp4 -src file.mp4 -i file.mp4\n"
"This will find a filter (for example `fin`) able to load `file.mp4`. The same result can be achieved by using `fin:src=file.mp4`\n"
"EX src=file.mp4 dst=dump.yuv\n"
"This will dump the video content of `file.mp4` in `dump.yuv`.\n"
"Specific source or sink filters may also be specified using `filterName:src=URL` or `filterName:dst=URL`.\n"
"## Forcing specific filters\n"
"There is a special option called `gfreg` which allows specifying preferred filters to use when handling URLs\n"
"EX src=file.mp4:gfreg=ffdmx,ffdec\n"
"This will use __ffdmx__ to read `file.mp4` and __ffdec__ to decode it.\n"
"This can be used to test a specific filter when alternate filter chains are possible.\n"
"## Specifying encoders and decoders\n"
"By default filters chain will be resolved without any decoding/encoding if the destination accepts the desired format. "
"Otherwise, decoders/encoders will be dynamically loaded to perform the conversion, unless dynamic resolution is disabled. "
"There is a special shorcut filter name for encoders `enc` allowing to match a filter providing the desired encoding. "
"The parameters for `enc` are:\n"
"- c=NAME: identifes the desired codec. `NAME` can be the gpac codec name or the encoder instance for ffmpeg/others\n"
"- b=UINT, rate=UINT, bitrate=UINT: indicates the bitrate in bits per second\n"
"- g=UINT, gop=UINT: indicates the GOP size in frames\n"
"- pfmt=NAME: indicates the target pixel format name (see [properties (-h props)](filters_properties) ) of the source, if supported by codec\n"
"- all_intra=BOOL: indicates all frames should be intra frames, if supported by codec\n"
"Other options will be passed to the filter if it accepts generic argument parsing (as is the case for ffmpeg)\n"
"EX src=dump.yuv:size=320x240:fps=25 enc:c=avc:b=150000:g=50:cgop=true:fast=true dst=raw.264\n"
"This creates a 25 fps AVC at 175kbps with a gop duration of 2 seconds, using closed gop and fast encoding settings for ffmpeg\n"
"\n"
"The inverse operation (forcing a decode to happen) is possible using the __reframer__ filter.\n"
"EX src=file.mp4 reframer:raw @ -o null\n"
"This will force decoding media from `file.mp4` and trash (send to `null`) the result (doing a decoder benchmark for example).\n"
"\n"
"When a filter uses an option defined as a string using the same separator character as gpac, you can either "
"modify the set of separators, or escape the seperator by duplicating it. The options enclosed by duplicated "
"separator are not parsed. This is mostly used for meta filters, such as ffmpeg, to pass options to subfilters "
"such as libx264 (cf `x264opts` parameter).\n"
"EX f:a=foo:b=bar\n"
"This will set option `a` to `foo` and option `b` to `bar` on the filter\n"
"EX f::a=foo:b=bar\n"
"This will set option `a` to `foo:b=bar` on the filter\n"
"EX f:a=foo::b=bar:c::d=fun\n"
"This will set option `a` to `foo`, `b` to `bar:c` and the option `d` to `fun` on the filter\n"
"# Expliciting links between filters [__LINK__]\n"
"## Quick links\n"
"Link between filters may be manually specified. The syntax is an `@` character optionnaly followed by an integer (0 if omitted). "
"This indicates which previous (0-based) filters should be link to the next filter listed. "
"Only the last link directive occuring before a filter is used to setup links for that filter.\n"
"EX fA fB @1 fC\n"
"This indicates to direct `fA` outputs to `fC`\n"
"EX fA fB @1 @0 fC\n"
"This indicates to direct `fB` outputs to `fC`, `@1` is ignored\n"
"If no link directives are given, the links will be dynamically solved to fullfill as many connections as possible (__see below__).\n"
"\n"
"## Complex links\n"
"The link directive is just a quick shortcut to set the following filter arguments:\n"
"- FID=name, which assigns an identifier to the filter\n"
"- SID=name1[,name2...], which set a list of filter identifiers , or __sourceIDs__, restricting the list of possible inputs for a filter.\n"
"\n"
"EX fA fB @1 fC\n"
"This is equivalent to `fA:FID=1 fB fC:SID=1` \n"
"Link directives specify which source a filter can accept connections from. They do not specifiy which destination a filter can connect to.\n"
"EX fA:FID=1 fB fC:SID=1\n"
"This indicates that `fC` only accpets input from `fA`, but `fB` might accept inputs from `fA`.\n"
"EX fA:FID=1 fB:FID=2 fC:SID=1 fD:SID=1,2\n"
"This indicates that `fD` only accepts input from `fA` and `fB` and `fC` only from `fA`\n"
"Note: A filter with sourceID set cannot get input from filters with no IDs.\n"
"A sourceID name can be further extended using fragment identifier (`#` by default):\n"
"- name#PIDNAME: accepts only PID(s) with name `PIDNAME`\n"
"- name#TYPE: accepts only PIDs of matching media type. TYPE can be `audio`, `video`, `scene`, `text`, `font`, `meta`\n"
"- name#TYPEN: accepts only `N`th PID of matching type from source\n"
"- name#P4CC=VAL: accepts only PIDs with property matching `VAL`.\n"
"- name#PName=VAL: same as above, using the builtin name corresponding to the property.\n"
"- name#AnyName=VAL: same as above, using the name of a non built-in property.\n"
"If the property is not defined on the PID, the property is matched. Otherwise, its value is checked against the given value.\n"
"\n"
"The following modifiers for comparisons are allowed (for both `P4CC=`, `PName=` and `AnyName=`)\n"
"- name#P4CC=!VAL: accepts only PIDs with property NOT matching `VAL`.\n"
"- name#P4CC-VAL: accepts only PIDs with property strictly less than `VAL` (only for 1-dimension number properties).\n"
"- name#P4CC+VAL: accepts only PIDs with property strictly greater than `VAL` (only for 1-dimension number properties).\n"
"\n"
"A sourceID name can also use wildcard or be empty to match a property regardless of the source filter:\n"
"EX fA fB:SID=*#ServiceID=2\nfA fB:SID=#ServiceID=2\n"
"This indicates to match connection between `fA` and `fB` only for PIDs with a `ServiceID` property of `2`.\n"
"These extensions also work with the __LINK__ `@` shortcut\n"
"EX fA fB @1#video fC\n"
"This indicates to direct `fA` video outputs to `fC`\n"
"EX src=img.heif @#ItemID=200 vout\n"
"This indicates to connect to `vout` only PIDs with `ItemID` property equal to `200`\n"
"EX src=vid.mp4 @#PID=1 vout\n"
"This indicates to connect to `vout` only PIDs with `ID` property equal to `1`\n"
"EX src=vid.mp4 @#Width=640 vout\n"
"This indicates to connect to `vout` only PIDs with `Width` property equal to `640`\n"
"EX src=vid.mp4 @#Width-640 vout\n"
"This indicates to connect to `vout` only PIDs with `Width` property less than `640`\n"
"\n"
"Multiple fragment can be specified to check for multiple PID properties\n"
"EX src=vid.mp4 @#Width=640#Height+380 vout\n"
"This indicates to connect to `vout` only PIDs with `Width` property equal to `640` and `Height` greater than `380`\n"
"\n"
"Warning: If a filter PID gets connected to a loaded filter, no further dynamic link resolution will "
"be done to connect it to other filters, unless sourceIDs are set. Link directives should be carfully setup.\n"
"EX src=file.mp4 @ reframer dst=dump.mp4\n"
"This will link src `file.mp4` PID (type __file__) to dst `dump.mp4`filter (type `file`) because dst has no sourceID and therefore will "
"accept input from src. Since the PID is connected, the filter engine will not try to solve "
"a link between src and `reframer`. The result is a direct copy of the source file, `reframer` being unused.\n"
"EX src=file.mp4 reframer @ dst=dump.mp4\n"
"This will force dst to accept only from reframer, a muxer will be loaded to solve this link, and "
"src PID will be linked to `reframer` (no source ID), loading a demuxer to solve the link. The result is a complete remux of the source file.\n"
"# Arguments inheriting\n"
"Unless explicitly disabled (see [-max-chain](CORE)), the filter engine will resolve implicit or explicit (__LINK__) connections "
"between filters and will allocate any filter chain required to connect the filters. "
"In doing so, it loads new filters with arguments inherited from both the source and the destination.\n"
"EX src=file.mp4:OPT dst=file.aac dst=file.264\n"
"This will pass the `:OPT` to all filters loaded between the source and the two destinations\n"
"EX src=file.mp4 dst=file.aac:OPT dst=file.264\n"
"This will pass the `:OPT` to all filters loaded between the source and the file.aac destination\n"
"Note: the destination arguments inherited are the arguments placed **AFTER** the `dst=` option.\n"
"EX src=file.mp4 fout:OPTFOO:dst=file.aac:OPTBAR\n"
"This will pass the `:OPTBAR` to all filters loaded between `file.mp4` source and `file.aac` destination, but not `OPTFOO`\n"
"Arguments inheriting can be stopped by using the keyword `locarg`: arguments after the keyword will not be inherited\n"
"EX src=file.mp4 dst=file.aac:OPTFOO:locarg:OPTBAR dst=file.264\n"
"This will pass the `:OPTFOO` to all filters loaded between `file.mp4`source and `file.aac` destination, but not `OPTBAR`\n"
"# URL templating\n"
"Destination URLs can be templated using the same mechanism as MPEG-DASH, where `$KEYWORD$` is replaced in the template with the "
"resolved value and `$KEYWORD%%0Nd$` is replaced in the template with the resolved integer, padded with N zeros if needed. "
"`$$` is an escape for $\n"
"`KEYWORD` is **case sensitive**, and may be present multiple times in the string. Supported `KEYWORD` are:\n"
"- num: replaced by file number if defined, 0 otherwise\n"
"- PID: ID of the source PID\n"
"- URL: URL of source file\n"
"- File: path on disk for source file\n"
"- p4cc=ABCD: uses PID property with 4CC value `ABCD`\n"
"- pname=VAL: uses PID property with name `VAL`\n"
"- OTHER: locates property 4CC for the given name, or property name if no 4CC matches.\n"
"Templating can be usefull when encoding several qualities in one pass\n"
"EX src=dump.yuv:size=640x360 vcrop:wnd=0x0x320x180 enc:c=avc:b=1M @2 enc:c=avc:b=750k dst=dump_$CropOrigin$x$Width$x$Height$.264:clone\n"
"This will create a croped version of the source, encoded in AVC at 1M, and a full version of the content in AVC at 750k. "
"Outputs will be `dump_0x0x320x180.264` for the croped version and `dump_0x0x640x360.264` for the non-croped one.\n"
"# Cloning filters\n"
"When a filter accepts a single connection and has a connected input, it is no longer available for dynamic resolution. "
"There may be cases where this behaviour is undesired. Take a HEIF file with N items and do:\n"
"EX src=img.heif dst=dump_$ItemID$.jpg\n"
"In this case, only one item (likely the first declared in the file) will connect to the destination.\n"
"Other items will not be connected since the destination only accepts one input PID.\n"
"There is a special option `clone` allowing destination filters (**and only them**) to be cloned with the same arguments:\n"
"EX src=img.heif dst=dump_$ItemID$.jpg:clone\n"
"In this case, the destination will be cloned for each item, and all will be exported to different JPEGs thanks to URL templating.\n"
"# Templating filter chains\n"
"There can be cases where the number of desired outputs depends on the source content, for example dumping a multiplex of N services into N files. When the destination involves multiplexing the input PIDs, the `:clone`option is not enough since the muxer will always accetp the input PIDs.\n"
"To handle this, it is possible to use a PID property name in the sourceID of a filter with the value `*` or an empty value. In this case, whenever a new PID with a new value for the property is found, the filter with such sourceID will be dynamically cloned\n"
"Warning: This feature should only be called with a single property set to `*` per source ID, results are undefined otherwise.\n"
"EX src=source.ts dst=file_$ServiceID$.mp4:SID=*#ServiceID=*\n"
"EX src=source.ts dst=file_$ServiceID$.mp4:SID=#ServiceID=\n"
"In this case, each new `ServiceID` value found when connecting PIDs to the destination will create a new destination file.\n"
"# Assigning PID properties\n"
"It is possible to define properties on output PIDs that will be declared by a filter. This allows tagging parts of the "
"graph with different properties than other parts (for example `ServiceID`). "
"The syntax is the same as filter option, and uses the fragment separator to identify properties, eg `#Name=Value`. "
"This sets output PIDs property (4cc, built-in name or any name) to the given value. Value can be omitted for booleans "
"(defaults to true, eg `:#Alpha`). If a non built-in property is used, the value will be declared as string.\n"
"Warning: Properties are not filtered and override the properties of the filter's output PIDs, be carefull not to break "
"the session by overriding core properties such as width/height/samplerate/... !\n"
"EX -i v1.mp4:#ServiceID=4 -i v2.mp4:#ServiceID=2 -o dump.ts\n"
"This will mux the streams in `dump.ts`, using `ServiceID` 4 for PIDs from `v1.mp4` and `ServiceID` 2 for PIDs from `v2.mp4`\n"
"# External filters\n"
"GPAC comes with a set of built-in filters in libgpac. It may also load external filters in dynamic libraries, located in "
"folders listed in GPAC config file section `core` key `mod-dirs`. The files shall be named __gf_*__ and shall export a single function "
"returning a filter register - see [libgpac documentation](https://doxygen.gpac.io/) for more details.\n"
"\n";
#endif

static void gpac_filter_help(void)
{
	gf_sys_format_help(helpout, help_flags,
"Usage: gpac [options] FILTER [LINK] FILTER [...] \n"
#ifndef GPAC_DISABLE_DOC
	"%s", gf_sys_localized("gpac", "doc", gpac_doc)
#else
	"GPAC compiled without built-in doc.\n"
#endif
	);

}

#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include <gpac/modules/compositor_ext.h>
#include <gpac/modules/hardcoded_proto.h>
#include <gpac/modules/js_usr.h>
#include <gpac/modules/font.h>

static void gpac_modules_help(void)
{
	u32 i;
	gf_sys_format_help(helpout, help_flags, "Available modules:\n");
	for (i=0; i<gf_modules_count(); i++) {
		char *str = (char *) gf_modules_get_file_name(i);
		if (!str) continue;
		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s: implements ", str);
		if (!strncmp(str, "gm_", 3) || !strncmp(str, "gsm_", 4)) {
			GF_BaseInterface *ifce = gf_modules_load_by_name(str, GF_VIDEO_OUTPUT_INTERFACE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "VideoOutput ");
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_AUDIO_OUTPUT_INTERFACE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "AudioOutput ");
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_FONT_READER_INTERFACE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "FontReader ");
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_COMPOSITOR_EXT_INTERFACE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "CompositorExtension ");
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_JS_USER_EXT_INTERFACE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "javaScriptExtension ");
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_HARDCODED_PROTO_INTERFACE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "HardcodedProto ");
				gf_modules_close_interface(ifce);
			}
		} else {
			gf_sys_format_help(helpout, help_flags, "Filter ");
		}
		gf_sys_format_help(helpout, help_flags, "\n");
	}
	gf_sys_format_help(helpout, help_flags, "\n");
}

#ifndef GPAC_DISABLE_DOC
const char *gpac_alias =
"The gpac command line can become quite complex when many sources or filters are used. In order to simplify this, an alias system is provided.\n"
"\n"
"To assign an alias, use the syntax `gpac -alias=\"NAME VALUE\"`.\n"
"- `NAME`: shall be a single string, with no space.\n"
"- `VALUE`: the list of argument this alias replaces. If not set, the alias is destroyed\n"
"\n"
"When parsing arguments, the alias will be replace by its value.\n"
"EX gpac -alias=\"output aout vout\"\n"
"This allows later playback using `gpac -i src.mp4 output`\n"
"\n"
"Aliases can use arguments from the command line. The allowed syntaxes are:\n"
"- `@{a}`: replaced by the value of the argument with index `a` after the alias\n"
"- `@{a,b}`: replaced by the value of the arguments with index `a` and `b`\n"
"- `@{a:b}`: replaced by the value of the arguments between index `a` and `b`\n"
"- `@{-a,b}`: replaced by the value of the arguments with index `a` and `b`, inserting a list separator (comma by default) between them\n"
"- `@{-a:b}`: replaced by the value of the arguments between index `a` and `b`, inserting a list separator (comma by default) between them\n"
"- `@{+a,b}`: clones the alias for each value listed, replacing in each clone with the corresponding argument\n"
"- `@{+a:b}`: clones the alias for each value listed, replacing in each clone with the corresponding argument\n"
"\n"
"The specified index can be:\n"
"- forward index: a strictly positive integer, 1 being the first argument after the alias\n"
"- backward index: the value 'n' (or 'N') to indicate the last argument on the command line. This can be followed by `-x` to rewind arguments (eg `@{n-1}` is the before last argument)\n"
"\n"
"Arguments not used by any aliases are kept on the command line, other ones are removed\n"
"\n"
"EX -alias=\"foo src=@{N} dst=test.mp4\"\n"
"The command `gpac foo f1 f2` expands to `gpac src=f2 dst=test.mp4 f1`\n"
"EX -alias=\"list: inspect src=@{+:N}\"\n"
"The command `gpac list f1 f2 f3` expands to `gpac inspect src=f1 src=f2 src=f3`\n"
"EX -alias=\"list inspect src=@{+2:N}\"\n"
"The command `gpac list f1 f2 f3` expands to `gpac inspect src=f2 src=f3 f1`\n"
"EX -alias=\"plist aout vout flist:srcs=@{-:N}\"\n"
"The command `gpac plist f1 f2 f3` expands to `gpac aout vout plist:srcs=\"f1,f2,f3\"`  \n"
"\n"
"Alias documentation can be set using `gpac -aliasdoc=\"NAME VALUE\"`, with `NAME` the alias name and `VALUE` the documentation.\n"
"Alias documentation will then appear in gpac help.\n"
"\n";
#endif

static void gpac_alias_help(GF_SysArgMode argmode)
{
	u32 i, count;

	if (argmode >= GF_ARGMODE_EXPERT) {

#ifndef GPAC_DISABLE_DOC
		gf_sys_format_help(helpout, help_flags, "%s", gf_sys_localized("gpac", "alias", gpac_alias) );
#else
		gf_sys_format_help(helpout, help_flags, "%s", "GPAC compiled without built-in doc.\n");
#endif
		if (argmode == GF_ARGMODE_EXPERT) {
			return;
		}
	}

	count = gf_opts_get_key_count("gpac.alias");
	if (count) {
		if (argmode < GF_ARGMODE_EXPERT) {
			gf_sys_format_help(helpout, help_flags, "Available aliases (use 'gpac -hx alias' for more info on aliases):\n");
		} else {
			gf_sys_format_help(helpout, help_flags, "Available aliases:\n");
		}
		for (i=0; i<count; i++) {
			const char *alias = gf_opts_get_key_name("gpac.alias", i);
			const char *alias_doc = gf_opts_get_key("gpac.aliasdoc", alias);
			const char *alias_value = gf_opts_get_key("gpac.alias", alias);

			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s", alias);
			if (argmode>=GF_ARGMODE_ADVANCED)
				gf_sys_format_help(helpout, help_flags, " (%s)", alias_value);

			if (alias_doc)
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_OPT_DESC, ": %s", gf_sys_localized("gpac", "aliasdoc", alias_doc));
			else if  (argmode<GF_ARGMODE_ADVANCED) {
				gf_sys_format_help(helpout, help_flags, " (%s)", alias_value);
			}

			gf_sys_format_help(helpout, help_flags, "\n");
		}
	} else {
		gf_sys_format_help(helpout, help_flags, "No aliases defined - use 'gpac -hx alias' for more info on aliases\n");
	}
}


static void gpac_core_help(GF_SysArgMode mode, Bool for_logs)
{
	u32 mask;
	gf_sys_format_help(helpout, help_flags, "# libgpac %s options:\n", for_logs ? "logs" : "core");
	if (for_logs) {
		mask = GF_ARG_SUBSYS_LOG;
	} else {
		mask = 0xFFFFFFFF;
		mask &= ~GF_ARG_SUBSYS_LOG;
		mask &= ~GF_ARG_SUBSYS_FILTERS;
	}
	gf_sys_print_core_help(helpout, help_flags, mode, mask);
}

GF_GPACArg gpac_args[] =
{
#ifdef GPAC_MEMORY_TRACKING
 	GF_DEF_ARG("mem-track", NULL, "enable memory tracker", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("mem-track-stack", NULL, "enable memory tracker with stack dumping", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#endif
	GF_DEF_ARG("ltf", NULL, "load test-unit filters (used for for unit tests only)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("loop", NULL, "loop execution of session, creating a session at each loop, mainly used for testing. If no value is given, loops forever", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("runfor", NULL, "run for the given amount of milliseconds", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),

	GF_DEF_ARG("stats", NULL, "print stats after execution", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("graph", NULL, "print graph after execution", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("k", NULL, "enable keyboard interaction from command line", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("r", NULL, "enable reporting\n"
			"- r: runtime reporting\n"
			"- r=FA[,FB]: runtime reporting but only print given filters, eg `r=mp4mx`for ISOBMFF muxer only\n"
			"- r=: only print final report"
			, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("seps", NULL, "set the default character sets used to seperate various arguments\n"\
		"- the first char is used to seperate argument names\n"\
		"- the second char, if present, is used to seperate names and values\n"\
		"- the third char, if present, is used to seperate fragments for PID sources\n"\
		"- the fourth char, if present, is used for list separators (__sourceIDs__, __gfreg__, ...)\n"\
		"- the fifth char, if present, is used for boolean negation\n"\
		"- the sixth char, if present, is used for LINK directives (see [filters help (-h doc)](filters_general))", GF_FS_DEFAULT_SEPS, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),

	GF_DEF_ARG("i", "src", "specify an input file - see [filters help (-h doc)](filters_general)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("o", "dst", "specify an output file - see [filters help (-h doc)](filters_general)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("h", "help -ha -hx -hh", "print help. Use ``help`/`-h` for basic options, `-ha` for advanced options, `-hx` for expert options and `-hh` for all. String parameter can be:\n"\
			"- empty: print command line options help\n"\
			"- doc: print the general filter info\n"\
			"- alias: print the gpac alias syntax\n"\
			"- log: print the log system help\n"\
			"- core: print the supported libgpac core options. Use -ha/-hx/-hh for advanced/expert options\n"\
			"- cfg: print the GPAC configuration help\n"\
			"- modules: print available modules\n"\
			"- filters: print name of all available filters\n"\
			"- filters:*: print name of all available filters, including meta filters\n"\
			"- codecs: print the supported builtin codecs\n"\
			"- props: print the supported builtin PID properties\n"\
			"- links: print possible connections between each supported filters.\n"\
			"- links FNAME: print sources and sinks for filter `FNAME`\n"\
			"- FNAME: print filter `FNAME` info (multiple FNAME can be given). For meta-filters, use `FNAME:INST`, eg `ffavin:avfoundation`. Use `*` to print info on all filters (_big output!_), `*:*` to print info on all filters including meta filter instances (__really big output!__). By default only basic filter options and description are shown. Use `-ha` to show advanced options and filter IO capabilities, `-hx` for expert options, `-hh` for all options and filter capbilities"\
		, NULL, NULL, GF_ARG_STRING, 0),

 	GF_DEF_ARG("p", NULL, "use indicated profile for the global GPAC config. If not found, config file is created. If a file path is indicated, this will load profile from that file. Otherwise, this will create a directory of the specified name and store new config there", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("alias", NULL, "assign a new alias or remove an alias. Can be specified several times. See [alias usage (-h alias)](#using-aliases)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("aliasdoc", NULL, "assign documentation for a given alias (optionnal). Can be specified several times", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),

 	GF_DEF_ARG("uncache", NULL, "revert all items in GPAC cache directory to their original name and server path", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),

	GF_DEF_ARG("wc", NULL, "write all core options in the config file unless already set", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("we", NULL, "write all file extensions in the config file unless already set (usefull to change some default file extensions)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("wf", NULL, "write all filter options in the config file unless already set", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("wfx", NULL, "write all filter options and all meta filter arguments in the config file unless already set (__large config file !__)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("unit-tests", NULL, "enable unit tests of some functions otherwise not covered by gpac test suite", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE),
	GF_DEF_ARG("genmd", NULL, "generate markdown doc", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE),
	{0}
};


static void gpac_usage(GF_SysArgMode argmode)
{
	u32 i=0;
	if ((argmode != GF_ARGMODE_ADVANCED) && (argmode != GF_ARGMODE_EXPERT) ) {
		if (gen_doc!=2)
			gf_sys_format_help(helpout, help_flags, "Usage: gpac [options] FILTER [LINK] FILTER [...] \n");

		gf_sys_format_help(helpout, help_flags, "gpac is GPAC's command line tool for setting up and running filter chains. Options do not require any specific order, and may be present anywhere, including between link statements or filter declarations.\n"
			"boolean values do not need any value specified. Other types shall be formatted as `opt=val`, except [-i](), `src`, [-o](), `dst` and [-h]() options.\n\n"
		);
	}

	while (gpac_args[i].name) {
		GF_GPACArg *arg = &gpac_args[i];
		i++;

		if (argmode!=GF_ARGMODE_ALL) {
			if ((argmode==GF_ARGMODE_EXPERT) && !(arg->flags & GF_ARG_HINT_EXPERT)) continue;
			if ((argmode==GF_ARGMODE_ADVANCED) && !(arg->flags & GF_ARG_HINT_ADVANCED)) continue;
			else if ((argmode==GF_ARGMODE_BASE) && (arg->flags & (GF_ARG_HINT_ADVANCED| GF_ARG_HINT_EXPERT)) ) continue;
		}

		gf_sys_print_arg(helpout, help_flags, arg, "gpac");
	}

	if (argmode>=GF_ARGMODE_ADVANCED) {
		gf_sys_print_core_help(helpout, help_flags, argmode, GF_ARG_SUBSYS_FILTERS);
	}

	if (argmode==GF_ARGMODE_BASE) {
		if ( gf_opts_get_key_count("gpac.aliasdoc")) {
			gf_sys_format_help(helpout, help_flags, "\n");
			gpac_alias_help(GF_ARGMODE_BASE);
		}

		gf_sys_format_help(helpout, help_flags, "\ngpac - GPAC command line filter engine - version %s\n%s\n", gf_gpac_version(), gf_gpac_copyright() );
	}
}

#ifndef GPAC_DISABLE_DOC
const char *gpac_config =
{
"# Configuration file\n"
"GPAC uses a configuration file to modify default options of libgpac and filters. This configuration file is located in `$HOME/.gpac/GPAC.cfg`.\n"
"Applications in GPAC can also specify a different configuration file through the [-p](GPAC) option to indicate a profile. "
"This allows different configurations for different usages and simplifies command line typing.\n"
"EX gpac -p=foo []\n"
"This will load configuration from $HOME/.gpac/foo/GPAC.cfg, creating it if needed.\n"
"By default the configuration file only holds a few system specific options and directories. It is possbible to serialize "
"the entire set of options to the configuration file, using [-wc](GPAC) [-wf](GPAC). This should be avoided as the resulting "
"configuration file size will be quite large, hence larger memory usage for the applications.\n"
"The options specified in the configuration file may be overriden by the values in __restrict.cfg__ file located in GPAC share system directory (e.g. /usr/share/gpac), "
"if present; this allows enforcing system-wide configuration values.\n"
"Note: The methods describe in this section apply to any application in GPAC transferring their arguments "
"to libgpac. This is the case for __gpac__, __MP4Box__, __MP4Client/Osmo4__.\n"
"# Core options\n"
"The options from libgpac core can also be assigned though the config file from section __core__ using "
"option name **without initial dash** as key name.\n"
"EX [core]threads=2\n"
"Setting this in the config file is equivalent to using `-threads=2`.\n"
"The options specified at prompt overrides the value of the config file.\n"
"# Filter options in configuration\n"
"It is possible to alter the default value of a filter option by modifing the configuration file. Filter __foo__ options are stored in section "
"`[filter@foo]`, using option name and value as key-value pair. Options specified through the configuration file do not take precedence over "
"options specified at prompt or through alias.\n"
"EX [filter@rtpin]interleave=yes\n"
"This will force the rtp input filter to always request RTP over RTSP by default.\n"
"To generate a configuration file with all filters options serialized, use [-wf](GPAC).\n"
"# Global filter options\n"
"It is possible to specify options global to multiple filters using `--OPTNAME=VAL`. Global options do not override filter options but "
"take precedence over options loaded from configuration file.\n"
"This will set option `OPTNAME`, when present, to `VAL` in any loaded filter.\n"
"EX --buffer=100 -i file vout aout\n"
"This is equivalent to specifying `vout:buffer=100 aout:buffer=100`.\n"
"EX --buffer=100 -i file vout aout:buffer=10\n"
"This is equivalent to specifying `vout:buffer=100 aout:buffer=10`.\n"
"Warning: This syntax only applies to regular filter options. It cannot be used with builtin shortcuts (gfreg, enc, ...).\n"
"Meta-filter options can be set in the same way using the syntax `-+OPT_NAME=VAL`.\n"
"EX -+profile=Baseline -i file.cmp -o dump.264\n"
"This is equivalent to specifying `-o dump.264:profile=Baseline`.\n"

};
#endif

static void gpac_config_help()
{

#ifndef GPAC_DISABLE_DOC
		gf_sys_format_help(helpout, help_flags, "%s", gf_sys_localized("gpac", "config", gpac_config) );
#else
		gf_sys_format_help(helpout, help_flags, "%s", "GPAC compiled without built-in doc.\n");
#endif
}

static void gpac_fsess_task_help()
{
	fprintf(stderr, "Available runtime options/keys:\n"
		"q: flushes all streams and exit\n"
		"x: exit with no flush (may break output files)\n"
		"s: prints statistics\n"
		"g: prints filter graph\n"
		"h: prints this screen\n"
	);
}


static Bool logs_to_file=GF_FALSE;

#define DEF_LOG_ENTRIES	10

struct _logentry
{
	u32 tool, level;
	u32 nb_repeat;
	u64 clock;
	char *szMsg;
} *static_logs;
static u32 nb_log_entries = DEF_LOG_ENTRIES;

static u32 log_write=0;

static char *log_buf = NULL;
static u32 log_buf_size=0;
static void gpac_on_logs(void *cbck, GF_LOG_Level log_level, GF_LOG_Tool log_tool, const char* fmt, va_list vlist)
{
	va_list vlist_tmp;
	va_copy(vlist_tmp, vlist);
	u32 len = vsnprintf(NULL, 0, fmt, vlist_tmp);
	if (log_buf_size < len+2) {
		log_buf_size = len+2;
		log_buf = gf_realloc(log_buf, log_buf_size);
	}
	vsprintf(log_buf, fmt, vlist);

	if (log_write && static_logs[log_write-1].szMsg) {
		if (!strcmp(static_logs[log_write-1].szMsg, log_buf)) {
			static_logs[log_write-1].nb_repeat++;
			return;
		}
	}

	static_logs[log_write].level = log_level;
	static_logs[log_write].tool = log_tool;
	if (static_logs[log_write].szMsg) gf_free(static_logs[log_write].szMsg);
	static_logs[log_write].szMsg = gf_strdup(log_buf);
	static_logs[log_write].clock = gf_net_get_utc();

	log_write++;
	if (log_write==nb_log_entries) {
		log_write = nb_log_entries - 1;
		gf_free(static_logs[0].szMsg);
		memmove(&static_logs[0], &static_logs[1], sizeof (struct _logentry) * (nb_log_entries-1) );
		memset(&static_logs[log_write], 0, sizeof(struct _logentry));
	}
}

u64 last_report_clock_us = 0;
static void print_date(u64 time)
{
	time_t gtime;
	struct tm *t;
	u32 sec;
	u32 ms;
	gtime = time / 1000;
	sec = (u32)(time / 1000);
	ms = (u32)(time - ((u64)sec) * 1000);
	t = gf_gmtime(&gtime);
//	fprintf(stderr, "[%d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, ms);
	fprintf(stderr, "[%02d:%02d:%02d.%03dZ] ", t->tm_hour, t->tm_min, t->tm_sec, ms);
}

static void gpac_print_report(GF_FilterSession *fsess, Bool is_init, Bool is_final)
{
	u32 i, count, nb_active;
	u64 now;
	GF_SystemRTInfo rti;

	if (is_init) {
		if (enable_reports==2)
			gf_sys_set_console_code(stderr, GF_CONSOLE_SAVE);

		logs_to_file = gf_sys_logs_to_file();
		if (!logs_to_file && (enable_reports==2) ) {
			if (!nb_log_entries) nb_log_entries = 1;
			static_logs = gf_malloc(sizeof(struct _logentry) * nb_log_entries);
			memset(static_logs, 0, sizeof(struct _logentry) * nb_log_entries);
			gf_log_set_callback(fsess, gpac_on_logs);
		}
		last_report_clock_us = gf_sys_clock_high_res();
		return;
	}

	now = gf_sys_clock_high_res();
	if ( (now - last_report_clock_us < 200000) && !is_final)
		return;

	last_report_clock_us = now;
	if (!is_final)
		gf_sys_set_console_code(stderr, GF_CONSOLE_CLEAR);

	gf_sys_get_rti(100, &rti, 0);
	gf_sys_set_console_code(stderr, GF_CONSOLE_CYAN);
	print_date(gf_net_get_utc());
	fprintf(stderr, "GPAC Session Status: ");
	gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
	fprintf(stderr, "mem % 10"LLD_SUF" kb CPU % 2d", rti.gpac_memory/1000, rti.process_cpu_time);
	fprintf(stderr, "\n");

	gf_fs_lock_filters(fsess, GF_TRUE);
	nb_active = count = gf_fs_get_filters_count(fsess);
	for (i=0; i<count; i++) {
		GF_FilterStats stats;
		gf_fs_get_filter_stats(fsess, i, &stats);
		if (stats.done) {
			nb_active--;
			continue;
		}
		if (report_filter && (!strstr(report_filter, stats.reg_name)))
			continue;
		if (stats.status) {
			gf_sys_set_console_code(stderr, GF_CONSOLE_GREEN);
			fprintf(stderr, "%s", stats.name ? stats.name : stats.reg_name);
			gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
			fprintf(stderr, ": %s\n", stats.status);
		} else {
			gf_sys_set_console_code(stderr, GF_CONSOLE_GREEN);
			fprintf(stderr, "%s", stats.name ? stats.name : stats.reg_name);
			gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
			if (stats.name && strcmp(stats.name, stats.reg_name))
				fprintf(stderr, " (%s)", stats.reg_name);
			fprintf(stderr, ": %s ", gf_stream_type_name(stats.stream_type));
			if (stats.codecid)
			 	fprintf(stderr, "(%s) ", gf_codecid_name(stats.codecid) );

			if ((stats.nb_pid_in == stats.nb_pid_out) && (stats.nb_pid_in==1)) {
				Double pck_per_sec = (Double) stats.nb_pck_sent;
				pck_per_sec *= 1000000;
				pck_per_sec /= (stats.time_process+1);

				fprintf(stderr, "% 10"LLD_SUF" pck %02.02f FPS ", (s64) stats.nb_out_pck, pck_per_sec);
			} else {
				if (stats.nb_pid_in)
					fprintf(stderr, "in %d PIDs % 10"LLD_SUF" pck ", stats.nb_pid_in, (s64)stats.nb_in_pck);
				if (stats.nb_pid_out)
					fprintf(stderr, "out %d PIDs % 10"LLD_SUF" pck ", stats.nb_pid_out, (s64) stats.nb_out_pck);
			}
			if (stats.in_eos)
				fprintf(stderr, "- EOS");
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "Active filters: %d\n", nb_active);

	if (static_logs) {
		if (is_final && (!log_write || !static_logs[log_write-1].szMsg))
			return;

		fprintf(stderr, "\nLogs:\n");
		for (i=0; i<log_write; i++) {
			if (static_logs[i].level==GF_LOG_ERROR) gf_sys_set_console_code(stderr, GF_CONSOLE_RED);
			else if (static_logs[i].level==GF_LOG_WARNING) gf_sys_set_console_code(stderr, GF_CONSOLE_YELLOW);
			else if (static_logs[i].level==GF_LOG_INFO) gf_sys_set_console_code(stderr, GF_CONSOLE_GREEN);
			else gf_sys_set_console_code(stderr, GF_CONSOLE_CYAN);

			print_date(static_logs[i].clock);

			if (static_logs[i].nb_repeat)
				fprintf(stderr, "[repeated %d] ", static_logs[i].nb_repeat);

			fprintf(stderr, "%s", static_logs[i].szMsg);
			gf_sys_set_console_code(stderr, GF_CONSOLE_RESET);
		}
		fprintf(stderr, "\n");
	}
	gf_fs_lock_filters(fsess, GF_FALSE);
	fflush(stderr);
}

static Bool gpac_event_proc(void *opaque, GF_Event *event)
{
	GF_FilterSession *fsess = (GF_FilterSession *)opaque;
	if (event->type==GF_EVENT_PROGRESS) {
		if (event->progress.progress_type==3) {
			gpac_print_report(fsess, GF_FALSE, GF_FALSE);
		}
	}
	else if (event->type==GF_EVENT_QUIT) {
		gf_fs_abort(fsess, GF_TRUE);
	}
	return GF_FALSE;
}

static u64 run_start_time = 0;
static Bool gpac_fsess_task(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{

	if (enable_prompt && gf_prompt_has_input()) {
		char c = gf_prompt_get_char();
		switch (c) {
		case 'q':
			gf_fs_abort(fsess, GF_TRUE);
			nb_loops = 0;
			return GF_FALSE;
		case 'x':
			gf_fs_abort(fsess, GF_FALSE);
			nb_loops = 0;
			return GF_FALSE;
		case 's':
			gf_fs_print_stats(fsess);
			break;
		case 'g':
			gf_fs_print_connections(fsess);
			break;
		case 'h':
			gpac_fsess_task_help();
			break;
		default:
			break;
		}
	}
	if (runfor>0) {
		u64 now = gf_sys_clock_high_res();
		if (!run_start_time) run_start_time = now;
		else if (now - run_start_time > runfor) {
			gf_fs_abort(fsess, GF_TRUE);
			nb_loops = 0;
			return GF_FALSE;
		}
	}

	if (gf_fs_is_last_task(fsess))
		return GF_FALSE;
	*reschedule_ms = 500;
	return GF_TRUE;
}

static Bool sigint_catched=GF_FALSE;
static Bool sigint_processed=GF_FALSE;
#ifdef WIN32
#include <windows.h>
static BOOL WINAPI gpac_sig_handler(DWORD sig)
{
	if (sig == CTRL_C_EVENT) {
#else
#include <signal.h>
static void gpac_sig_handler(int sig)
{
	if (sig == SIGINT) {
#endif
		nb_loops = 0;
		if (session) {
			char input=0;
			int res;
			if (sigint_catched) {
				if (sigint_processed) {
					fprintf(stderr, "catched SIGINT twice and session not responding, forcing exit. Please report to GPAC devs https://github.com/gpac/gpac\n");
				}
				exit(1);
			}
			sigint_catched = GF_TRUE;
			fprintf(stderr, "catched SIGINT - flush session before exit ? (Y/n):\n");
			res = scanf("%c", &input);
			sigint_processed = GF_TRUE;
			if (res!=1) input=0;
			switch (input) {
			case 'Y':
			case 'y':
			case '\n':
				gf_fs_abort(session, GF_TRUE);
				break;
			default:
				gf_fs_abort(session, GF_FALSE);
				break;
			}
		}
	}
#ifdef WIN32
	return TRUE;
#endif
}

static void parse_sep_set(const char *arg, Bool *override_seps)
{
	if (!arg) return;
	u32 len = (u32) strlen(arg);
	if (!len) return;
	*override_seps = GF_TRUE;
	if (len>=1) separator_set[0] = arg[0];
	if (len>=2) separator_set[1] = arg[1];
	if (len>=3) separator_set[2] = arg[2];
	if (len>=4) separator_set[3] = arg[3];
	if (len>=5) separator_set[4] = arg[4];
	if (len>=6) separator_set[5] = arg[5];
}


static int gpac_exit_fun(int code, char **alias_argv, int alias_argc)
{
	if (alias_argv) {
		while (gf_list_count(args_alloc)) {
			gf_free(gf_list_pop_back(args_alloc));
		}
		gf_list_del(args_alloc);
		gf_list_del(args_used);
		gf_free(alias_argv);
	}
	if (log_buf) gf_free(log_buf);
	if ((helpout != stdout) && (helpout != stderr)) {
		if (gen_doc==2) {
			fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://github.com/gpac/gpac/wiki/Filters\n");
			fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
			".br\nFor bug reports, feature requests, more information and source code, visit http://github.com/gpac/gpac\n"
			".br\nbuild: %s\n"
			".br\nCopyright: %s\n.br\n"
			".SH SEE ALSO\n"
			".LP\ngpac(1), MP4Client(1), MP4Box(1)\n", gf_gpac_version(), gf_gpac_copyright());
		}
		gf_fclose(helpout);
	}

	if (sidebar_md)
		gf_fclose(sidebar_md);

	if (static_logs) {
		u32 i;
		for (i=0; i<nb_log_entries; i++) {
			if (static_logs[i].szMsg)
				gf_free(static_logs[i].szMsg);
		}
		gf_free(static_logs);
	}

	gf_sys_close();
	if (code) return code;

#ifdef GPAC_MEMORY_TRACKING
	if (gf_memory_size() || gf_file_handles_count() ) {
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		return 2;
	}
#endif
	return 0;
}

#define gpac_exit(_code) return gpac_exit_fun(_code, alias_argv, alias_argc)

#include <gpac/network.h>
static void gpac_load_suggested_filter_args()
{
	u32 i, count, k;
	GF_Config *opts = NULL;
	GF_FilterSession *fsess;

	const char *version;
	const char *cfg_path = gf_opts_get_filename();
	char *all_opts = gf_url_concatenate(cfg_path, "all_opts.txt");

	opts = gf_cfg_force_new(NULL, all_opts);
	gf_free(all_opts);

	version = gf_cfg_get_key(opts, "version", "version");
	if (version && !strcmp(version, gf_gpac_version()) ) {
		gf_cfg_del(opts);
		return;
	}
	gf_cfg_del_section(opts, "allopts");
	gf_cfg_set_key(opts, "version", "version", gf_gpac_version());

	gf_sys_format_help(stderr, 0, "__Constructing all options registry, this may take some time ... ");

	fsess = gf_fs_new(0, GF_FS_SCHEDULER_LOCK_FREE, GF_FS_FLAG_LOAD_META, NULL);
	if (!fsess) {
		gf_sys_format_help(stderr, 0, "\nWarning: Error creating session\n");
		gf_cfg_del(opts);
		return;
	}

	count = gf_fs_filters_registers_count(fsess);
	for (i=0; i<count; i++) {
		const GF_FilterRegister *freg = gf_fs_get_filter_register(fsess, i);

		k=0;
		while (freg->args) {
			const char *old_val;
			char *argn=NULL;
			const GF_FilterArgs *arg = &freg->args[k];
			if (!arg || !arg->arg_name) {
				break;
			}
			k++;
			if (arg->flags & GF_FS_ARG_HINT_HIDE) continue;

			if (arg->flags & GF_FS_ARG_META) {
				gf_dynstrcat(&argn, "-+", NULL);
			} else {
				gf_dynstrcat(&argn, "--", NULL);
			}
			gf_dynstrcat(&argn, arg->arg_name, NULL);

			old_val = gf_cfg_get_key(opts, "allopts", argn);
			if (old_val) {
				char *newval = gf_strdup(freg->name);
				gf_dynstrcat(&newval, old_val, " ");
				gf_cfg_set_key(opts, "allopts", argn, newval);
				gf_free(newval);
			} else {
				gf_cfg_set_key(opts, "allopts", argn, freg->name);
			}
			gf_free(argn);
		}
	}
	gf_fs_del(fsess);
	gf_cfg_del(opts);
	gf_sys_format_help(stderr, 0, "done\n");
}

//very basic word match, check the number of source characters in order in dest
static Bool word_match(const char *orig, const char *dst)
{
	s32 dist = 0;
	u32 match = 0;
	u32 i;
	u32 olen = (u32) strlen(orig);
	u32 dlen = (u32) strlen(dst);
	u32 *run;
	if (olen*2 < dlen) return GF_FALSE;

	run = gf_malloc(sizeof(u32) * olen);
	memset(run, 0, sizeof(u32) * olen);

	for (i=0; i<dlen; i++) {
		u32 dist_char;
		u32 offset=0;
		char *pos;

retry_char:
		pos = strchr(orig+offset, dst[i]);
		if (!pos) continue;
		dist_char = (u32) (pos - orig);
		if (!run[dist_char]) {
			run[dist_char] = i+1;
			match++;
		} else if (run[dist_char] > i) {
			run[dist_char] = i+1;
			match++;
		} else {
			//this can be a repeated character
			offset++;
			goto retry_char;

		}
	}
	if (match*2<olen) {
		gf_free(run);
		return 0;
	}
/*	if ((olen<=4) && (match>=3) && (dlen*2<olen*3) ) {
		gf_free(run);
		return GF_TRUE;
	}
*/
	for (i=0; i<olen; i++) {
		if (!i) {
			if (run[0]==1)
				dist++;
		} else if (run[i-1] + 1 == run[i]) {
			dist++;
		}
	}
	gf_free(run);
	//if half the characters are in order, consider a match
	//if arg is small only check dst
	if ((olen<=4) && (dist >= 2))
		return GF_TRUE;
	if ((dist*2 >= (s32) olen) && (dist*2 >= (s32) dlen))
		return GF_TRUE;
	return GF_FALSE;
}

static void gpac_suggest_arg(char *aname)
{
	u32 k;
	const GF_GPACArg *args = gf_sys_get_options();
	Bool found=GF_FALSE;
	for (k=0; k<2; k++) {
		u32 i=0;
		if (k==0) args = gpac_args;
		else args = gf_sys_get_options();

		while (args[i].name) {
			const GF_GPACArg *arg = &args[i];
			i++;
			if (word_match(aname, arg->name)) {
				if (!found) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unrecognized option \"%s\", did you mean:\n", aname));
					found = GF_TRUE;
				}
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("\t-%s (see gpac -hx%s)\n", arg->name, k ? " core" : ""));
			}
		}
	}
	if (!found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unrecognized option \"%s\", check usage \"gpac -h\"\n", aname));
	}
}

static void gpac_suggest_filter(char *fname, Bool is_help)
{
	char *doc_helps[] = {
		"log", "core", "modules", "doc", "alias", "props", "cfg", "codecs", "links", "bin", "filters", "filters:*", NULL
	};

	Bool found = GF_FALSE;
	u32 i, count = gf_fs_filters_registers_count(session);
	for (i=0; i<count; i++) {
		const GF_FilterRegister *freg = gf_fs_get_filter_register(session, i);

		if (word_match(fname, freg->name)) {
			if (!found) {
				found = GF_TRUE;
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Closest filter names: \n"));
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("- %s\n", freg->name));
		}
	}
	if (!found && is_help) {
		i=0;
		while (doc_helps[i]) {
			if (word_match(fname, doc_helps[i])) {
				if (!found) {
					found = GF_TRUE;
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Closest help command: \n"));
				}
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("-h %s\n", doc_helps[i]));
			}
			i++;
		}
	}
	if (!found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No filter %swith similar name found, see gpac -h filters%s\n",
			is_help ? "or help command " : "",
			is_help ? " or gpac -h" : ""
		));
	}
}

static void gpac_suggest_filter_arg(GF_Config *opts, char *argname, u32 atype)
{
	char *keyname;
	const char *keyval;
	u32 i, count, len, nb_filters, j;
	Bool f_found = GF_FALSE;
	char szSep[2];

	szSep[0] = separator_set[0];
	szSep[1] = 0;

	len = (u32) strlen(argname);
	keyname = gf_malloc(sizeof(char)*(len+3));
	sprintf(keyname, "-%c%s", (atype==2) ? '+' : '-', argname);
	keyval = gf_cfg_get_key(opts, "allopts", keyname);
	gf_free(keyname);
	if (keyval) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Argument \"%s%s\" was set but not used by any filter\n",
		(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
		return;
	}

	nb_filters = gf_fs_get_filters_count(session);
	count = gf_cfg_get_key_count(opts, "allopts");
	for (i=0; i<count; i++) {
		Bool ffound = GF_FALSE;
		const char *arg = gf_cfg_get_key_name(opts, "allopts", i);
		const char *aval = gf_cfg_get_key(opts, "allopts", arg);
		if ((arg[1]=='+') && (atype!=2)) continue;
		if ((arg[1]=='-') && (atype==2)) continue;

		arg += 2;
		u32 alen = (u32) strlen(arg);
		if (alen>2*len) continue;

		for (j=0; j<nb_filters; j++) {
			GF_FilterStats stats;
			stats.reg_name = NULL;
			gf_fs_get_filter_stats(session, j, &stats);

			if (stats.reg_name && strstr(aval, stats.reg_name)) {
				ffound = GF_TRUE;
				break;
			}
		}
		if (!ffound) continue;

		if (word_match(argname, arg)) {
			if (!f_found) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unknown argument \"%s%s\" set but not used by any filter - possible matches\n",
					(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
				f_found = GF_TRUE;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("- %s in filters %s\n", arg, aval));
		}
	}
	if (!f_found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unknown argument \"%s%s\" set but not used by any filter - no matching argument found\n",
			(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
	}
}

static void gpac_check_session_args()
{
	GF_Config *opts = NULL;
	u32 idx = 0;
	char *argname;
	u32 argtype;

	while (1) {
		if (gf_fs_enum_unmapped_options(session, &idx, &argname, &argtype)==GF_FALSE)
			break;

		if (!opts) {
			const char *cfg_path = gf_opts_get_filename();
			char *all_opts = gf_url_concatenate(cfg_path, "all_opts.txt");
			opts = gf_cfg_new(NULL, all_opts);
			gf_free(all_opts);
		}
		if (!opts) {
			break;
		}
		gpac_suggest_filter_arg(opts, argname, argtype);
	}
	if (opts) gf_cfg_del(opts);
}

static int gpac_main(int argc, char **argv)
{
	GF_Err e=GF_OK;
	int i;
	const char *profile=NULL;
	u32 sflags=0;
	Bool override_seps=GF_FALSE;
	Bool write_profile=GF_FALSE;
	Bool write_core_opts=GF_FALSE;
	Bool write_extensions=GF_FALSE;
	s32 link_prev_filter = -1;
	char *link_prev_filter_ext=NULL;
	GF_List *loaded_filters=NULL;
	GF_SysArgMode argmode = GF_ARGMODE_BASE;
	u32 nb_filters = 0;
	Bool nothing_to_do = GF_TRUE;
	GF_MemTrackerType mem_track=GF_MemTrackerNone;
	char *view_conn_for_filter = NULL;
	Bool view_filter_conn = GF_FALSE;
	Bool dump_codecs = GF_FALSE;
	Bool has_alias = GF_FALSE;
	Bool alias_set = GF_FALSE;
	GF_FilterSession *tmp_sess;
	u32 loops_done = 0;

	helpout = stdout;

	//look for mem track and profile, and also process all helpers
	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {
#ifdef GPAC_MEMORY_TRACKING
            mem_track = !strcmp(arg, "-mem-track-stack") ? GF_MemTrackerBackTrace : GF_MemTrackerSimple;
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"%s\"\n", arg);
#endif
		} else if (!strncmp(arg, "-p=", 3)) {
			profile = arg+3;
		} else if (!strncmp(arg, "-mkl=", 5)) {
			return gpac_make_lang(arg+5);
		}
	}

	gf_sys_init(mem_track, profile);

	gpac_load_suggested_filter_args();

//	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
	gf_log_set_tool_level(GF_LOG_APP, GF_LOG_INFO);

	if (gf_opts_get_key_count("gpac.alias")) {
		for (i=1; i<argc; i++) {
			char *arg = argv[i];
			if (gf_opts_get_key("gpac.alias", arg) != NULL) {
				has_alias = GF_TRUE;
				break;
			}
		}
		if (has_alias) {
			if (! gpac_expand_alias(argc, argv) ) {
				gpac_exit(1);
			}
			argc = alias_argc;
			argv = alias_argv;
		}
	}


	//this will parse default args
	e = gf_sys_set_args(argc, (const char **) argv);
	if (e) {
		fprintf(stderr, "Error assigning libgpac arguments: %s\n", gf_error_to_string(e) );
		gpac_exit(1);
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_APP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("GPAC args: "));
		for (i=1; i<argc; i++) {
			char *arg = argv[i];
			GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("%s ", arg));
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_APP, ("\n"));
	}
#endif

	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		char *arg_val = strchr(arg, '=');
		if (arg_val) {
			arg_val[0]=0;
			arg_val++;
		}

		if (!strcmp(arg, "-h") || !strcmp(arg, "-help") || !strcmp(arg, "-ha") || !strcmp(arg, "-hx") || !strcmp(arg, "-hh")) {
			if (!strcmp(arg, "-ha")) argmode = GF_ARGMODE_ADVANCED;
			else if (!strcmp(arg, "-hx")) argmode = GF_ARGMODE_EXPERT;
			else if (!strcmp(arg, "-hh")) argmode = GF_ARGMODE_ALL;

			if ((i+1==argc) || (argv[i+1][0]=='-')) {
				gpac_usage(argmode);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "log")) {
				gpac_core_help(GF_ARGMODE_ALL, GF_TRUE);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "core")) {
				gpac_core_help(argmode, GF_FALSE);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "modules")) {
				gpac_modules_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "doc")) {
				gpac_filter_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "alias")) {
				gpac_alias_help(argmode);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "props")) {
				dump_all_props();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "cfg")) {
				gpac_config_help();
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "codecs")) {
				dump_codecs = GF_TRUE;
				i++;
			} else if (!strcmp(argv[i+1], "links")) {
				view_filter_conn = GF_TRUE;
				if ((i+2<argc)	&& (argv[i+2][0] != '-')) {
					view_conn_for_filter = argv[i+2];
					i++;
				}

				i++;
			} else if (!strcmp(argv[i+1], "bin")) {
				gf_sys_format_help(helpout, help_flags, "GPAC binary information:\n"\
				 	"Version: %s\n"\
	        		"Compilation configuration: " GPAC_CONFIGURATION "\n"\
	        		"Enabled features: %s\n" \
	        		"Disabled features: %s\n", gf_gpac_version(), gf_enabled_features(), gf_disabled_features()
				);
				gpac_exit(0);
			} else if (!strcmp(argv[i+1], "filters")) {
				list_filters = 1;
				i++;
			} else if (!strcmp(argv[i+1], "filters:*")) {
				list_filters = 2;
				i++;
			} else {
				print_filter_info = 1;
			}
		}
		else if (!strcmp(arg, "-genmd") || !strcmp(arg, "-genman")) {
			argmode = GF_ARGMODE_ALL;
			if (!strcmp(arg, "-genmd")) {
				gen_doc = 1;
				help_flags = GF_PRINTARG_MD;
				helpout = gf_fopen("gpac_general.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Usage\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
				fprintf(helpout, "# General Usage of gpac\n");
			} else {
				gen_doc = 2;
				help_flags = GF_PRINTARG_MAN;
				helpout = gf_fopen("gpac.1", "w");
	 			fprintf(helpout, ".TH gpac 1 2019 gpac GPAC\n");
				fprintf(helpout, ".\n.SH NAME\n.LP\ngpac \\- GPAC command-line filter session manager\n"
				".SH SYNOPSIS\n.LP\n.B gpac\n"
				".RI [options] FILTER [LINK] FILTER [...]\n.br\n.\n");
			}
			gpac_usage(GF_ARGMODE_ALL);

			if (gen_doc==1) {
				fprintf(helpout, "# Using Aliases\n");
			} else {
				fprintf(helpout, ".SH Using Aliases\n.PL\n");
			}
			gpac_alias_help(GF_ARGMODE_EXPERT);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("core_config.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Configuration\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
			}
			gpac_config_help();

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("core_options.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Core Options\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
				fprintf(helpout, "# GPAC Core Options\n");
			}
			gpac_core_help(argmode, GF_FALSE);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("core_logs.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Logging System\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
				fprintf(helpout, "# GPAC Log System\n");
			}
			gpac_core_help(argmode, GF_TRUE);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("filters_general.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » General Concepts\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
			}
			gf_sys_format_help(helpout, help_flags, "%s", gpac_doc);

			if (gen_doc==1) {
				gf_fclose(helpout);
				helpout = gf_fopen("filters_properties.md", "w");
				fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » Built-in Properties\n");
				fprintf(helpout, "%s", auto_gen_md_warning);
			}
			gf_sys_format_help(helpout, help_flags, "# GPAC Built-in properties\n");
			dump_all_props();
//			dump_codecs = GF_TRUE;

			if (gen_doc==2) {
				fprintf(helpout, ".SH EXAMPLES\n.TP\nBasic and advanced examples are available at https://github.com/gpac/gpac/wiki/Filters\n");
				fprintf(helpout, ".SH MORE\n.LP\nAuthors: GPAC developers, see git repo history (-log)\n"
				".br\nFor bug reports, feature requests, more information and source code, visit http://github.com/gpac/gpac\n"
				".br\nbuild: %s\n"
				".br\nCopyright: %s\n.br\n"
				".SH SEE ALSO\n"
				".LP\ngpac-filters(1), MP4Client(1), MP4Box(1)\n", gf_gpac_version(), gf_gpac_copyright());
				gf_fclose(helpout);

				helpout = gf_fopen("gpac-filters.1", "w");
	 			fprintf(helpout, ".TH gpac 1 2019 gpac GPAC\n");
				fprintf(helpout, ".\n.SH NAME\n.LP\ngpac \\- GPAC command-line filter session manager\n"
				".SH SYNOPSIS\n.LP\n.B gpac\n"
				".RI [options] FILTER [LINK] FILTER [...]\n.br\n.\n"
				".SH DESCRIPTION\n.LP"
				"\nThis page describes all filters usually present in GPAC\n"
				"\nTo check for help on a filter not listed here, use gpac -h myfilter\n"
				"\n"
				);
			}

			list_filters = 1;
		}
		else if (!strcmp(arg, "-ltf")) {
			load_test_filters = GF_TRUE;
		} else if (!strcmp(arg, "-stats")) {
			dump_stats = GF_TRUE;
		} else if (!strcmp(arg, "-graph")) {
			dump_graph = GF_TRUE;
		} else if (strstr(arg, ":*") && list_filters) {
			list_filters = 3;
		} else if (strstr(arg, ":*")) {
			print_meta_filters = GF_TRUE;
		} else if (!strcmp(arg, "-wc")) {
			write_core_opts = GF_TRUE;
		} else if (!strcmp(arg, "-we")) {
			write_extensions = GF_TRUE;
		} else if (!strcmp(arg, "-wf")) {
			write_profile = GF_TRUE;
		} else if (!strcmp(arg, "-wfx")) {
			write_profile = GF_TRUE;
			sflags |= GF_FS_FLAG_LOAD_META;
		} else if (!strcmp(arg, "-loop")) {
			nb_loops = -1;
			if (arg_val) nb_loops = atoi(arg_val);
		} else if (!strcmp(arg, "-runfor")) {
			if (arg_val) runfor = 1000*atoi(arg_val);
		} else if (!strcmp(arg, "-uncache")) {
			const char *cache_dir = gf_opts_get_key("core", "cache");
			gf_enum_directory(cache_dir, GF_FALSE, revert_cache_file, NULL, "*.txt");
			fprintf(stderr, "GPAC Cache dir %s flattened\n", cache_dir);
			gpac_exit(0);
		} else if (!strcmp(arg, "-cfg")) {
			nothing_to_do = GF_FALSE;
		}

		else if (!strcmp(arg, "-alias") || !strcmp(arg, "-aliasdoc")) {
			char *alias_val;
			Bool exists;
			if (!arg_val) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("-alias does not have any argument, check usage \"gpac -h\"\n"));
				gpac_exit(1);
			}
			alias_val = arg_val ? strchr(arg_val, ' ') : NULL;
			if (alias_val) alias_val[0] = 0;

			session = gf_fs_new_defaults(sflags);
			exists = gf_fs_filter_exists(session, arg_val);
			gf_fs_del(session);
			if (exists) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("alias %s has the same name as an existing filter, not allowed\n", arg_val));
				if (alias_val) alias_val[0] = ' ';
				gpac_exit(1);
			}

			gf_opts_set_key(!strcmp(arg, "-alias") ? "gpac.alias" : "gpac.aliasdoc", arg_val, alias_val ? alias_val+1 : NULL);
			fprintf(stderr, "Set %s for %s to %s\n", arg, arg_val, alias_val ? alias_val+1 : "NULL");
			if (alias_val) alias_val[0] = ' ';
			alias_set = GF_TRUE;
		}
		else if (!strncmp(arg, "-seps=", 3)) {
			parse_sep_set(arg_val, &override_seps);
		} else if (!strcmp(arg, "-mem-track") || !strcmp(arg, "-mem-track-stack")) {

		} else if (!strcmp(arg, "-k")) {
			enable_prompt = GF_TRUE;
		} else if (!strcmp(arg, "-r")) {
			enable_reports = 2;
			if (arg_val && !strlen(arg_val)) {
				enable_reports = 1;
			} else {
				report_filter = arg_val;
			}
		} else if (!strcmp(arg, "-unit-tests")) {
			do_unit_tests = GF_TRUE;
		} else if (arg[0]=='-') {
			if (!strcmp(arg, "-i") || !strcmp(arg, "-src") || !strcmp(arg, "-o") || !strcmp(arg, "-dst") ) {
			} else if (!gf_sys_is_gpac_arg(arg) ) {
				gpac_suggest_arg(arg);
				gpac_exit(1);
			}
		}

		if (arg_val) {
			arg_val--;
			arg_val[0]='=';
		}
	}

	if (do_unit_tests) {
		gpac_exit( gpac_unit_tests(mem_track) );
	}

	if (alias_set) {
		gpac_exit(0);
	}

	if (dump_stats && gf_sys_get_rti(0, &rti, 0) ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("System info: %d MB RAM - %d cores\n", (u32) (rti.physical_memory/1024/1024), rti.nb_cores));
	}
	if ((list_filters>=2) || print_meta_filters || dump_codecs || print_filter_info) sflags |= GF_FS_FLAG_LOAD_META;

restart:

	session = gf_fs_new_defaults(sflags);

	if (!session) {
		gpac_exit(1);
	}
	if (override_seps) gf_fs_set_separators(session, separator_set);
	if (load_test_filters) gf_fs_register_test_filters(session);

	if (gf_fs_get_max_resolution_chain_length(session) <= 1 ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("\nDynamic resolution of filter connections disabled\n\n"));
	}

	if (list_filters || print_filter_info) {
		if (print_filters(argc, argv, session, argmode)==GF_FALSE)
			e = GF_FILTER_NOT_FOUND;
		goto exit;
	}
	if (view_filter_conn) {
		gf_fs_print_all_connections(session, view_conn_for_filter, gf_sys_format_help);
		goto exit;
	}
	if (dump_codecs) {
		dump_all_codec(session);
		goto exit;
	}
	if (write_profile || write_extensions || write_core_opts) {
		if (write_core_opts)
			write_core_options();
		if (write_extensions)
			write_file_extensions();
		if (write_profile)
			write_filters_options(session);
		goto exit;
	}

	//all good to go, load filters
	loaded_filters = gf_list_new();
	for (i=1; i<argc; i++) {
		GF_Filter *filter=NULL;
		Bool is_simple=GF_FALSE;
		Bool f_loaded = GF_FALSE;
		char *arg = argv[i];

		if (!strcmp(arg, "-src") || !strcmp(arg, "-i") ) {
			filter = gf_fs_load_source(session, argv[i+1], NULL, NULL, &e);
			arg = argv[i+1];
			i++;
			f_loaded = GF_TRUE;
		} else if (!strcmp(arg, "-dst") || !strcmp(arg, "-o") ) {
			filter = gf_fs_load_destination(session, argv[i+1], NULL, NULL, &e);
			arg = argv[i+1];
			i++;
			f_loaded = GF_TRUE;
		}
		//appart from the above src/dst, other args starting with - are not filters
		else if (arg[0]=='-') {
			continue;
		}
		if (!f_loaded) {
			if (arg[0]== separator_set[SEP_LINK] ) {
				char *ext = strchr(arg, separator_set[SEP_FRAG]);
				if (ext) {
					ext[0] = 0;
					link_prev_filter_ext = ext+1;
				}
				link_prev_filter = 0;
				if (strlen(arg)>1) {
					link_prev_filter = atoi(arg+1);
					if (link_prev_filter<0) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Wrong filter index %d, must be positive\n", link_prev_filter));
						e = GF_BAD_PARAM;
						goto exit;
					}
				}

				if (ext) ext[0] = separator_set[SEP_FRAG];
				continue;
			}

			if (!strncmp(arg, "src=", 4) ) {
				filter = gf_fs_load_source(session, arg+4, NULL, NULL, &e);
			} else if (!strncmp(arg, "dst=", 4) ) {
				filter = gf_fs_load_destination(session, arg+4, NULL, NULL, &e);
			} else {
				filter = gf_fs_load_filter(session, arg);
				is_simple=GF_TRUE;
			}
		}

		if (!filter) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Failed to load filter%s %s\n", is_simple ? "" : " for",  arg));
			if (!e) e = GF_NOT_SUPPORTED;
			nb_filters=0;
			if (is_simple) gpac_suggest_filter(arg, GF_FALSE);

			goto exit;
		}
		nb_filters++;

		if (link_prev_filter>=0) {
			GF_Filter *link_from = gf_list_get(loaded_filters, gf_list_count(loaded_filters)-1-link_prev_filter);
			if (!link_from) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Wrong filter index @%d\n", link_prev_filter));
				e = GF_BAD_PARAM;
				goto exit;
			}
			link_prev_filter = -1;
			gf_filter_set_source(filter, link_from, link_prev_filter_ext);
			link_prev_filter_ext = NULL;
		}

		gf_list_add(loaded_filters, filter);
	}
	if (!gf_list_count(loaded_filters)) {
		if (nothing_to_do && !gen_doc) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Nothing to do, check usage \"gpac -h\"\ngpac - GPAC command line filter engine - version %s\n", gf_gpac_version()));
			e = GF_BAD_PARAM;
		} else {
			e = GF_EOS;
		}
		goto exit;
	}
	if (enable_reports) {
		if (enable_reports==2)
			gf_fs_set_ui_callback(session, gpac_event_proc, session);

		gf_fs_enable_reporting(session, GF_TRUE);
	}

	if (enable_prompt || (runfor>0)) {
		if (enable_prompt) {
			GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("Running session, press 'h' for help\n"));
		}
		gf_fs_post_user_task(session, gpac_fsess_task, NULL, "gpac_fsess_task");
	}
	if (!enable_prompt) {
#ifdef WIN32
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)gpac_sig_handler, TRUE);
#else
		signal(SIGINT, gpac_sig_handler);
#endif
	}

	if (enable_reports) {
		gpac_print_report(session, GF_TRUE, GF_FALSE);
	}

	e = gf_fs_run(session);
	if (e>0) e = GF_OK;
	if (!e) {
		e = gf_fs_get_last_connect_error(session);
		if (e<0) fprintf(stderr, "session last connect error %s\n", gf_error_to_string(e) );
	}
	if (!e) {
		e = gf_fs_get_last_process_error(session);
		if (e<0) fprintf(stderr, "session last process error %s\n", gf_error_to_string(e) );
	}
	gpac_check_session_args();

	if (enable_reports) {
		if (enable_reports==2) {
			gf_sys_set_console_code(stderr, GF_CONSOLE_RESTORE);
		}
		gpac_print_report(session, GF_FALSE, GF_TRUE);
	}

exit:
	if (enable_reports==2) {
		gf_log_set_callback(session, NULL);
	}

	if (e && nb_filters) {
		gf_fs_run(session);
	}
	if (dump_stats)
		gf_fs_print_stats(session);
	if (dump_graph)
		gf_fs_print_connections(session);


	tmp_sess = session;
	session = NULL;
	gf_fs_del(tmp_sess);
	if (loaded_filters) gf_list_del(loaded_filters);

	if (!e && nb_loops) {
		if (nb_loops>0) nb_loops--;
		loops_done++;
		fprintf(stderr, "session done, restarting (loop %d)\n", loops_done);
		gf_log_reset_file();
		goto restart;
	}

	gpac_exit(e<0 ? 1 : 0);
}

GF_MAIN_FUNC(gpac_main)


/*********************************************************
		Filter and property info/dump functions
*********************************************************/

static void dump_caps(u32 nb_caps, const GF_FilterCapability *caps)
{
	u32 i;
	for (i=0;i<nb_caps; i++) {
		const char *szName;
		const char *szVal;
		char szDump[GF_PROP_DUMP_ARG_SIZE];
		const GF_FilterCapability *cap = &caps[i];
		if (!(cap->flags & GF_CAPFLAG_IN_BUNDLE) && i+1==nb_caps) break;
		if (!i) gf_sys_format_help(helpout, help_flags, "Capabilities Bundle:\n");
		else if (!(cap->flags & GF_CAPFLAG_IN_BUNDLE) ) {
			gf_sys_format_help(helpout, help_flags, "Capabilities Bundle:\n");
			continue;
		}

		szName = cap->name ? cap->name : gf_props_4cc_get_name(cap->code);
		if (!szName) szName = gf_4cc_to_str(cap->code);
		gf_sys_format_help(helpout, help_flags, "\t Flags:");
		if (cap->flags & GF_CAPFLAG_INPUT) gf_sys_format_help(helpout, help_flags, " Input");
		if (cap->flags & GF_CAPFLAG_OUTPUT) gf_sys_format_help(helpout, help_flags, " Output");
		if (cap->flags & GF_CAPFLAG_EXCLUDED) gf_sys_format_help(helpout, help_flags, " Exclude");
		if (cap->flags & GF_CAPFLAG_LOADED_FILTER) gf_sys_format_help(helpout, help_flags, " LoadedFilterOnly");

		//dump some interesting predefined ones which are not mapped to types
		if (cap->code==GF_PROP_PID_STREAM_TYPE) szVal = gf_stream_type_name(cap->val.value.uint);
		else if (cap->code==GF_PROP_PID_CODECID) szVal = (const char *) gf_codecid_name(cap->val.value.uint);
		else szVal = gf_prop_dump_val(&cap->val, szDump, GF_FALSE, NULL);

		gf_sys_format_help(helpout, help_flags, " %s=\"%s\"", szName,  szVal);
		if (cap->priority) gf_sys_format_help(helpout, help_flags, ", priority=%d", cap->priority);
		gf_sys_format_help(helpout, help_flags, "\n");
	}
}

static void print_filter(const GF_FilterRegister *reg, GF_SysArgMode argmode)
{

	if (gen_doc==1) {
		char szName[1024];
		sprintf(szName, "%s.md", reg->name);
		if (gen_doc==1) {
			gf_fclose(helpout);
			helpout = gf_fopen(szName, "w");
			fprintf(helpout, "[**HOME**](Home) » [**Filters**](Filters) » %s\n", reg->description);
			fprintf(helpout, "%s", auto_gen_md_warning);

			if (!sidebar_md) {
				char *sbbuf = NULL;
				if (gf_file_exists("_Sidebar.md")) {
					char szLine[1024];
					u32 end_pos=0;
					sidebar_md = gf_fopen("_Sidebar.md", "r");
					gf_fseek(sidebar_md, 0, SEEK_SET);
					while (!feof(sidebar_md)) {
						char *read = fgets(szLine, 1024, sidebar_md);
						if (!read) break;
						if (!strncmp(szLine, "**Filters Help**", 16)) {
							end_pos = ftell(sidebar_md);
							break;
						}
					}
					if (!end_pos) end_pos = ftell(sidebar_md);
					if (end_pos) {
						sbbuf = gf_malloc(end_pos+1);
						gf_fseek(sidebar_md, 0, SEEK_SET);
						end_pos = (u32) fread(sbbuf, 1, end_pos, sidebar_md);
						sbbuf[end_pos]=0;
						gf_fclose(sidebar_md);
					}
				}
				sidebar_md = gf_fopen("_Sidebar.md", "w");
				if (sbbuf) {
					fprintf(sidebar_md, "%s\n  \n", sbbuf);
					gf_free(sbbuf);
				}
			}
#ifndef GPAC_DISABLE_DOC
			if (reg->description) {
				fprintf(sidebar_md, "[[%s (%s)|%s]]  \n", reg->description, reg->name, reg->name);
			} else {
				fprintf(sidebar_md, "[[%s|%s]]  \n", reg->name, reg->name);
			}
			if (!reg->help) {
				fprintf(stderr, "filter %s without help, forbidden\n", reg->name);
				exit(1);
			}
			if (!reg->description) {
				fprintf(stderr, "filter %s without description, forbidden\n", reg->name);
				exit(1);
			}
#endif
		}

		gf_sys_format_help(helpout, help_flags, "# %s\n", reg->description);
#ifndef GPAC_DISABLE_DOC
		gf_sys_format_help(helpout, help_flags, "Register name used to load filter: **%s**\n", reg->name);
#endif

	} else {
		gf_sys_format_help(helpout, help_flags, "# %s\n", reg->name);
#ifndef GPAC_DISABLE_DOC
		if (reg->description) gf_sys_format_help(helpout, help_flags, "Description: %s\n", reg->description);
		if (reg->version) gf_sys_format_help(helpout, help_flags, "Version: %s\n", reg->version);
#endif
	}

#ifndef GPAC_DISABLE_DOC
	if (reg->author) gf_sys_format_help(helpout, help_flags, "Author: %s\n", reg->author);
	if (reg->help) gf_sys_format_help(helpout, help_flags, "\n%s\n\n", reg->help);
#else
	gf_sys_format_help(helpout, help_flags, "GPAC compiled without built-in doc\n");
#endif

	if (argmode==GF_ARGMODE_EXPERT) {
		if (reg->max_extra_pids==(u32) -1) gf_sys_format_help(helpout, help_flags, "Max Input PIDs: any\n");
		else gf_sys_format_help(helpout, help_flags, "Max Input PIDs: %d\n", 1 + reg->max_extra_pids);

		gf_sys_format_help(helpout, help_flags, "Flags:");
		if (reg->flags & GF_FS_REG_EXPLICIT_ONLY) gf_sys_format_help(helpout, help_flags, " ExplicitOnly");
		if (reg->flags & GF_FS_REG_MAIN_THREAD) gf_sys_format_help(helpout, help_flags, " MainThread");
		if (reg->flags & GF_FS_REG_CONFIGURE_MAIN_THREAD) gf_sys_format_help(helpout, help_flags, " ConfigureMainThread");
		if (reg->flags & GF_FS_REG_HIDE_WEIGHT) gf_sys_format_help(helpout, help_flags, " HideWeight");
		if (reg->flags & GF_FS_REG_DYNLIB) gf_sys_format_help(helpout, help_flags, " DynamicLib");
		if (reg->probe_url) gf_sys_format_help(helpout, help_flags, " URLMimeProber");
		if (reg->probe_data) gf_sys_format_help(helpout, help_flags, " DataProber");
		if (reg->reconfigure_output) gf_sys_format_help(helpout, help_flags, " ReconfigurableOutput");

		gf_sys_format_help(helpout, help_flags, "\nPriority %d", reg->priority);

		gf_sys_format_help(helpout, help_flags, "\n");
	}


	if (reg->args) {
		u32 idx=0;
		if (gen_doc==1) {
			gf_sys_format_help(helpout, help_flags, "# Options  \n");
		} else {
			switch (argmode) {
			case GF_ARGMODE_ALL:
			case GF_ARGMODE_EXPERT:
				gf_sys_format_help(helpout, help_flags, "# Options (expert):\n");
				break;
			case GF_ARGMODE_ADVANCED:
				gf_sys_format_help(helpout, help_flags, "# Options (advanced):\n");
				break;
			case GF_ARGMODE_BASE:
				gf_sys_format_help(helpout, help_flags, "# Options (basic):\n");
				break;
			}
		}

		while (1) {
			Bool is_enum = GF_FALSE;
			const GF_FilterArgs *a = & reg->args[idx];
			if (!a || !a->arg_name) break;
			idx++;

			if (a->flags & GF_FS_ARG_HINT_HIDE) continue;

			switch (argmode) {
			case GF_ARGMODE_ALL:
			case GF_ARGMODE_EXPERT:
			 	break;
			case GF_ARGMODE_ADVANCED:
			 	if (a->flags & GF_FS_ARG_HINT_EXPERT) continue;
			 	break;
			case GF_ARGMODE_BASE:
			 	if (a->flags & (GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_HINT_ADVANCED)) continue;
			 	break;
			}

			if ((a->arg_type==GF_PROP_UINT) && a->min_max_enum && strchr(a->min_max_enum, '|')) {
				is_enum = GF_TRUE;
			}

			if (gen_doc==1) {
				gf_sys_format_help(helpout, help_flags, "<a id=\"%s\">", a->arg_name);
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s", a->arg_name);
				gf_sys_format_help(helpout, help_flags, "</a> (%s", is_enum ? "enum" : gf_props_get_type_name(a->arg_type));
			} else {
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s (%s", a->arg_name, is_enum ? "enum" : gf_props_get_type_name(a->arg_type));
			}
			if (a->arg_default_val) {
				gf_sys_format_help(helpout, help_flags, ", default: __%s__", a->arg_default_val);
			} else {
				gf_sys_format_help(helpout, help_flags, ", no default");
			}
			if (a->min_max_enum && !is_enum) {
				gf_sys_format_help(helpout, help_flags, ", %s: %s", /*strchr(a->min_max_enum, '|') ? "Enum" : */"minmax", a->min_max_enum);
			}
			if (a->flags & GF_FS_ARG_UPDATE) gf_sys_format_help(helpout, help_flags, ", updatable");
			if (a->flags & GF_FS_ARG_META) gf_sys_format_help(helpout, help_flags, ", meta");
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_OPT_DESC, "): %s\n", a->arg_desc);

			if (a->min_max_enum && strchr(a->min_max_enum, '|'))
				gf_sys_format_help(helpout, help_flags, "\n");

			if (!gen_doc) continue;

#ifdef CHECK_DOC
			if (a->flags & GF_FS_ARG_META) continue;

			u8 achar;
			u32 j=0;
			char szArg[100];
			sprintf(szArg, " %s ", a->arg_name);
			if (reg->help && strstr(reg->help, szArg)) {
				fprintf(stderr, "\nWARNING: filter %s bad help, uses arg %s without link\n", reg->name, a->arg_name);
				exit(1);
			}
			while (1) {
				const GF_FilterArgs *anarg = & reg->args[j];
				if (!anarg || !anarg->arg_name) break;
				j++;
				if (a == anarg) continue;
				if (reg->help && strstr(reg->help, szArg)) {
					fprintf(stderr, "\nWARNING: filter %s bad description for argument %s, uses arg %s without link\n", reg->name, anarg->arg_name, a->arg_name);
					exit(1);
				}
			}
			
			if (a->min_max_enum) {
				//check format
                if ((a->arg_type!=GF_PROP_UINT_LIST) && !(a->flags&GF_FS_ARG_META) && strchr(a->min_max_enum, '|') ) {
                	if (!strstr(a->arg_desc, "- ")) {
                    	fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, missing list bullet \"- \"\n", reg->name, a->arg_name);
                    	exit(1);
					}
                	if (strstr(a->arg_desc, ":\n")) {
                    	fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should not use \":\\n\"\n", reg->name, a->arg_name);
                    	exit(1);
					}
                } else if (!(a->flags&GF_FS_ARG_META) && strchr(a->arg_desc, '\n')) {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should not contain \"\\n\"\n", reg->name, a->arg_name);
					exit(1);
				}
			}
            if (!(a->flags&GF_FS_ARG_META)) {
				char *sep;

				achar = a->arg_desc[strlen(a->arg_desc)-1];
				if (achar == '\n') {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should not end with \"\\n\"\n", reg->name, a->arg_name);
					exit(1);
				}

				achar = a->arg_desc[0];
				if ((achar >= 'A') && (achar <= 'Z')) {
					achar = a->arg_desc[1];
					if ((achar < 'A') || (achar > 'Z')) {
						fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should start with lowercase\n", reg->name, a->arg_name);
						exit(1);
					}
				}
				if (a->arg_desc[0] == ' ') {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, first character should not be space\n", reg->name, a->arg_name);
					exit(1);
				}
				sep = strchr(a->arg_desc+1, ' ');
				if (sep) sep--;
				if (sep && (sep[0] == 's')) {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, first word should be infinitive\n", reg->name, a->arg_name);
					exit(1);
				}
			}
#endif
		}
	} else {
		gf_sys_format_help(helpout, help_flags, "No options\n");
	}

	if (reg->nb_caps) {
		if (!gen_doc && (argmode==GF_ARGMODE_ALL)) {
			dump_caps(reg->nb_caps, reg->caps);
		}
	}
	gf_sys_format_help(helpout, help_flags, "\n");
}

static Bool print_filters(int argc, char **argv, GF_FilterSession *session, GF_SysArgMode argmode)
{
	Bool found = GF_FALSE;
	char *fname = NULL;
	u32 i, count = gf_fs_filters_registers_count(session);
	if (!gen_doc && list_filters) gf_sys_format_help(helpout, help_flags, "Listing %d supported filters%s:\n", count, (list_filters==2) ? " including meta-filters" : "");
	for (i=0; i<count; i++) {
		const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);
		if (gen_doc) {
			print_filter(reg, argmode);
			found = GF_TRUE;
		} else if (print_filter_info) {
			u32 k;
			//all good to go, load filters
			for (k=1; k<(u32) argc; k++) {
				char *arg = argv[k];
				if (arg[0]=='-') continue;
				fname = arg;

				if (!strcmp(arg, reg->name) ) {
					print_filter(reg, argmode);
					found = GF_TRUE;
				} else {
					char *sep = strchr(arg, ':');
					if (!strcmp(arg, "*:*")
						|| (!sep && !strcmp(arg, "*"))
					 	|| (sep && !strcmp(sep, ":*") && !strncmp(reg->name, arg, 1+sep - arg) )
					) {
						print_filter(reg, argmode);
						found = GF_TRUE;
						break;
					}
				}
			}
		} else {
#ifndef GPAC_DISABLE_DOC
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s: %s\n", reg->name, reg->description);
#else
			gf_sys_format_help(helpout, help_flags, "%s (compiled without built-in doc)\n", reg->name);
#endif
			found = GF_TRUE;

		}
	}
	if (!found && fname) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
		gpac_suggest_filter(fname, GF_TRUE);
	}
	return found;
}

static void dump_all_props(void)
{
	u32 i=0;
	const GF_BuiltInProperty *prop_info;

	if (gen_doc==1) {
		gf_sys_format_help(helpout, help_flags, "Built-in properties for PIDs and packets, pixel formats and audio formats.\n"
		"  \n"
		"Flags can be:\n"
		"- D: dropable property, see [GSF mux](gsfmx) filter help for more info\n"
		"- P: property applying to packet\n"
		"  \n");
		gf_sys_format_help(helpout, help_flags, "Name | type | Flags | Description | 4CC  \n");
		gf_sys_format_help(helpout, help_flags, "--- | --- | --- | --- | ---  \n");
	} else {
		gf_sys_format_help(helpout, help_flags, "Built-in properties for PIDs and packets listed as `Name (4CC type FLAGS): description`\n`FLAGS` can be D (dropable - see GSF mux filter help), P (packet property)\n");
	}
	while ((prop_info = gf_props_get_description(i))) {
		i++;
		char szFlags[10];
		if (! prop_info->name) continue;

		if (gen_doc==1) {
			strcpy(szFlags, "");
			if (prop_info->flags & GF_PROP_FLAG_GSF_REM) strcat(szFlags, "D");
			if (prop_info->flags & GF_PROP_FLAG_PCK) strcat(szFlags, "P");

			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s | %s | %s | %s  \n", prop_info->name,  gf_props_get_type_name(prop_info->data_type),
				szFlags,
			 	prop_info->description,
			 	gf_4cc_to_str(prop_info->type)
			);
		} else if (gen_doc==2) {
			gf_sys_format_help(helpout, help_flags, ".TP\n.B %s (%s,%s,%s%s)\n%s\n", prop_info->name, gf_4cc_to_str(prop_info->type), gf_props_get_type_name(prop_info->data_type),
			 	(prop_info->flags & GF_PROP_FLAG_GSF_REM) ? "D" : " ",
			 	(prop_info->flags & GF_PROP_FLAG_PCK) ? "P" : " ",
				prop_info->description);
		} else {
			szFlags[0]=0;
			if (prop_info->flags & GF_PROP_FLAG_GSF_REM) strcat(szFlags, "D");
			if (prop_info->flags & GF_PROP_FLAG_PCK) strcat(szFlags, "P");

			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s (%s %s %s):", prop_info->name, gf_4cc_to_str(prop_info->type), gf_props_get_type_name(prop_info->data_type), szFlags);

			gf_sys_format_help(helpout, help_flags, "%s", prop_info->description);

			if (prop_info->data_type==GF_PROP_PIXFMT) {
				gf_sys_format_help(helpout, help_flags, "\n\tNames: %s\n\tFile extensions: %s", gf_pixel_fmt_all_names(), gf_pixel_fmt_all_shortnames() );
			} else if (prop_info->data_type==GF_PROP_PCMFMT) {
				gf_sys_format_help(helpout, help_flags, "\n\tNames: %s\n\tFile extensions: %s", gf_audio_fmt_all_names(), gf_audio_fmt_all_shortnames() );
			}
			gf_sys_format_help(helpout, help_flags, "\n");
		}
	}
	if (gen_doc==1) {
		u32 idx=0;
		const char *name, *fileext, *desc;
		gf_sys_format_help(helpout, help_flags, "# Pixel formats\n");
		gf_sys_format_help(helpout, help_flags, "Name | File extensions | Description  \n");
		gf_sys_format_help(helpout, help_flags, " --- | --- | ---  \n");
		while ( gf_pixel_fmt_enum(&idx, &name, &fileext, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s | %s  \n", name, fileext, desc);
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Audio formats\n");
		gf_sys_format_help(helpout, help_flags, " Name | File extensions | Description  \n");
		gf_sys_format_help(helpout, help_flags, " --- | --- | ---  \n");
		while ( gf_audio_fmt_enum(&idx, &name, &fileext, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s | %s  \n", name, fileext, desc);
		}
	} else if (gen_doc==2) {
		u32 idx=0;
		const char *name, *fileext, *desc;
		gf_sys_format_help(helpout, help_flags, "# Pixel formats\n");
		while ( gf_pixel_fmt_enum(&idx, &name, &fileext, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, ".TP\n.B %s (ext *.%s)\n%s\n", name, fileext, desc);
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Audio formats\n");
		while ( gf_audio_fmt_enum(&idx, &name, &fileext, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, ".TP\n.B %s (ext *.%s)\n%s\n", name, fileext, desc);
		}
	}}

static void dump_all_codec(GF_FilterSession *session)
{
	GF_PropertyValue rawp;
	GF_PropertyValue cp;
	u32 cidx=0;
	u32 count = gf_fs_filters_registers_count(session);
	gf_sys_format_help(helpout, help_flags, "Codec names listed as built_in_name[|variant](I: Filter Input support, O: Filter Output support): full_name\n");
	rawp.type = cp.type = GF_PROP_UINT;
	rawp.value.uint = GF_CODECID_RAW;
	while (1) {
		u32 i;
		const char *lname;
		const char *sname;
		Bool enc_found = GF_FALSE;
		Bool dec_found = GF_FALSE;
		cp.value.uint = gf_codecid_enum(cidx, &sname, &lname);
		cidx++;
		if (cp.value.uint == GF_CODECID_NONE) break;
		if (cp.value.uint == GF_CODECID_RAW) continue;
		if (!sname) break;

		for (i=0; i<count; i++) {
			const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);
			if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &rawp, GF_PROP_PID_CODECID, &cp)) enc_found = GF_TRUE;
			if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &cp, GF_PROP_PID_CODECID, &rawp)) dec_found = GF_TRUE;
		}

		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s (%c%c)", sname, dec_found ? 'I' : '-', enc_found ? 'O' : '-');
		gf_sys_format_help(helpout, help_flags, "%s\n", lname);
	}
	gf_sys_format_help(helpout, help_flags, "\n");
}

/*********************************************************
			Config file writing functions
*********************************************************/

static void write_core_options()
{
	//print libgpac core help
	const GF_GPACArg *args = gf_sys_get_options();
	u32 i = 0;

	while (args[i].name) {
		if (! gf_opts_get_key("core", args[i].name) && args[i].val) {
			gf_opts_set_key("core", args[i].name, args[i].val);
		}
		i++;
	}
}

static void write_file_extensions()
{
	u32 i = 0;

	while (1) {
		const char *short_name, *long_name, *mime ;
		u32 cid = gf_codecid_enum(i, &short_name, &long_name);
		if (cid==GF_CODECID_NONE) break;
		mime = gf_codecid_mime(cid);
		if (mime && ! gf_opts_get_key("file_extensions", mime) ) {
			gf_opts_set_key("file_extensions", mime, short_name);
		}
		i++;
	}
}

static void write_filters_options(GF_FilterSession *fsess)
{
	u32 i, count;
	count = gf_fs_filters_registers_count(fsess);
	for (i=0; i<count; i++) {
		char *meta_sep;
		u32 j=0;
		char szSecName[200];

		const GF_FilterRegister *freg = gf_fs_get_filter_register(fsess, i);
		sprintf(szSecName, "filter@%s", freg->name);
		meta_sep = strchr(szSecName + 7, ':');
		if (meta_sep) meta_sep[0] = 0;

		while (freg->args) {
			const GF_FilterArgs *arg = &freg->args[j];
			if (!arg || !arg->arg_name) break;
			j++;

			if (arg->arg_default_val && !gf_opts_get_key(szSecName, arg->arg_name)) {
				gf_opts_set_key(szSecName, arg->arg_name, arg->arg_default_val);
			}
		}
	}
}

/*********************************************************
		Language file creation / update functions
*********************************************************/

static Bool lang_updated = GF_FALSE;
static void gpac_lang_set_key(GF_Config *cfg, const char *sec_name,  const char *key_name, const char *key_val)
{
	char szKeyCRC[1024];
	char szKeyCRCVal[100];
	const char *opt = gf_cfg_get_key(cfg, sec_name, key_name);
	u32 crc_key = 0;
	if (opt) {
		sprintf(szKeyCRC, "%s_crc", key_name);
		const char *crc_opt = gf_cfg_get_key(cfg, sec_name, szKeyCRC);
		if (crc_opt) {
			u32 old_crc = atoi(crc_opt);
			crc_key = gf_crc_32((u8*)key_val, (u32) strlen(key_val));
			if (old_crc != crc_key) {
				gf_sys_format_help(helpout, help_flags, "Warning: description has changed for %s:%s (crc %d - crc in file %d) - please check translation\n", sec_name, key_name, crc_key, old_crc);
			}
			return;
		}
	}
	if (!opt) {
		if (!crc_key) {
			sprintf(szKeyCRC, "%s_crc", key_name);
			crc_key = gf_crc_32((u8*)key_val, (u32) strlen(key_val));
		}
		sprintf(szKeyCRCVal, "%u", crc_key);
		gf_cfg_set_key(cfg, sec_name, key_name, key_val);
		gf_cfg_set_key(cfg, sec_name, szKeyCRC, szKeyCRCVal);
		lang_updated = GF_TRUE;
	}
}
static int gpac_make_lang(char *filename)
{
	GF_Config *cfg;
	u32 i;
	gf_sys_init(GF_MemTrackerNone, NULL);
	GF_FilterSession *session = gf_fs_new_defaults(0);
	if (!session) {
		gf_sys_format_help(helpout, help_flags, "failed to load session, cannot create language file\n");
		return 1;
	}
	if (!gf_file_exists(filename)) {
		FILE *f = gf_fopen(filename, "wt");
		if (!f) {
			gf_sys_format_help(helpout, help_flags, "failed to open %s in write mode\n", filename);
			return 1;
		}
		gf_fclose(f);
	}

	cfg = gf_cfg_new(NULL, filename);

	//print gpac help
	i = 0;
	while (gpac_args[i].name) {
		gpac_lang_set_key(cfg, "gpac", gpac_args[i].name, gpac_args[i].description);
		i++;
	}

	//print gpac doc
	gpac_lang_set_key(cfg, "gpac", "doc", gpac_doc);

	//print gpac alias doc
	gpac_lang_set_key(cfg, "gpac", "alias", gpac_alias);

	//print libgpac core help
	const GF_GPACArg *args = gf_sys_get_options();
	i = 0;

	while (args[i].name) {
		gpac_lang_set_key(cfg, "core", args[i].name, args[i].description);
		i++;
	}

	//print properties
	i=0;
	const GF_BuiltInProperty *prop_info;
	while ((prop_info = gf_props_get_description(i))) {
		i++;
		if (! prop_info->name || !prop_info->description) continue;
		gpac_lang_set_key(cfg, "properties", prop_info->name, prop_info->description);
	}

	//print filters
	u32 count = gf_fs_filters_registers_count(session);
	for (i=0; i<count; i++) {
		u32 j=0;
		const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);

		if (reg->description) {
			gpac_lang_set_key(cfg, reg->name, "desc", reg->description);
		}
		if (reg->help) {
			gpac_lang_set_key(cfg, reg->name, "help", reg->help);
		}
		while (reg->args && reg->args[j].arg_name) {
			gpac_lang_set_key(cfg, reg->name, reg->args[j].arg_name, reg->args[j].arg_desc);
			j++;
		}
	}

	if (!lang_updated) {
		gf_cfg_discard_changes(cfg);
		fprintf(stderr, "lang file %s has not been modified\n", filename);
	} else {
		fprintf(stderr, "lang file generated in %s\n", filename);
	}
	gf_cfg_del(cfg);

	gf_sys_close();
	return 0;
}


/*********************************************************
			Alias functions
*********************************************************/

static GFINLINE void push_arg(char *_arg, Bool _dup)
{
	alias_argv = gf_realloc(alias_argv, sizeof(char**) * (alias_argc+1));\
	alias_argv[alias_argc] = _dup ? gf_strdup(_arg) : _arg; \
	if (_dup) {
		if (!args_alloc) args_alloc = gf_list_new();
		gf_list_add(args_alloc, alias_argv[alias_argc]);
	}
	alias_argc++;
}

static Bool gpac_expand_alias_arg(char *param, char *prefix, char *suffix, int arg_idx, int argc, char **argv);

static Bool check_param_extension(char *szArg, int arg_idx, int argc, char **argv)
{
	char *par_start = strstr(szArg, "@{");
	if (par_start) {
		char szPar[100];
		Bool ok;
		char *par_end = strchr(par_start, '}');
		if (!par_end) {
			fprintf(stderr, "Bad format %s for alias parameter, expecting @{N}\n", szArg);
			return GF_FALSE;
		}
		par_end[0] = 0;
		strcpy(szPar, par_start+2);
		par_start[0] = 0;

	 	ok = gpac_expand_alias_arg(szPar, szArg, par_end+1, arg_idx, argc, argv);
	 	if (!ok) return GF_FALSE;
		par_start[0] = '@';
		par_end[0] = '}';
		return GF_TRUE;
	}
	//done, push arg
	push_arg(szArg, 1);
	return GF_TRUE;
}

static Bool gpac_expand_alias_arg(char *param, char *prefix, char *suffix, int arg_idx, int argc, char **argv)
{
	char szArg[1024];
	char szSepList[2];
	char *oparam = param;
	szSepList[0] = separator_set[SEP_LIST];
	szSepList[1] = 0;

	Bool is_list = param[0]=='-';
	Bool is_expand = param[0]=='+';

	if (is_list || is_expand) param++;
	strcpy(szArg, prefix);
	while (param) {
		u32 idx=0;
		u32 last_idx=0;
		char *an_arg;
		char *sep = strchr(param, ',');
		if (sep) sep[0]=0;

		if ((param[0]=='n') || (param[0]=='N')) {
			idx = argc - arg_idx - 1;
			if (param[1]=='-') {
				u32 diff = atoi(param+2);
				if (diff>=idx) {
					if (sep) sep[0]=',';
					fprintf(stderr, "Bad usage for alias parameter %s: not enough parameters\n", oparam);
					return GF_FALSE;
				}
				idx -= diff;
			}
		} else {
			char *lsep = strchr(param, ':');
			if (lsep) {
				lsep[0] = 0;
				if (strlen(param))
					idx = atoi(param);
				else
					idx=1;

				lsep[0] = ':';
				if ((lsep[1]=='n') || (lsep[1]=='N')) {

					last_idx = argc - arg_idx - 1;
					if (lsep[2]=='-') {
						u32 diff = atoi(lsep+3);
						if (diff>=last_idx) {
							fprintf(stderr, "Bad usage for alias parameter %s: not enough parameters\n", oparam);
							return GF_FALSE;
						}
						last_idx -= diff;
					}
				} else {
					last_idx = atoi(lsep+1);
				}
			} else {
				idx = atoi(param);
			}
		}
		if (!idx) {
			if (sep) sep[0]=',';
			fprintf(stderr, "Bad format for alias parameter %s: cannot extract argument index\n", oparam);
			return GF_FALSE;
		}
		if ((int) idx + arg_idx >= argc) {
			if (sep) sep[0]=',';
			fprintf(stderr, "Bad format for alias parameter %s: argment out of bounds (not enough paramteters?)\n", oparam);
			return GF_FALSE;
		}

		if (!last_idx) last_idx=idx;
		for (; idx<=last_idx;idx++) {
			an_arg = argv[idx+arg_idx];

			if (!args_used) args_used = gf_list_new();
			gf_list_add(args_used, an_arg);

			if (is_expand) {
				strcpy(szArg, prefix);
				strcat(szArg, an_arg);
				strcat(szArg, suffix);

				Bool ok = check_param_extension(szArg, arg_idx, argc, argv);
				if (!ok) {
					if (sep) sep[0]=',';
					return GF_FALSE;
				}
			} else {
				strcat(szArg, an_arg);
				if (is_list && (idx<last_idx)) {
					strcat(szArg, szSepList);
				}
			}
		}

		if (!sep) break;
		sep[0]=',';
		param = sep+1;
		if (is_list) {
			strcat(szArg, szSepList);
		}
	}

	if (is_expand)
		return GF_TRUE;

	strcat(szArg, suffix);

	return check_param_extension(szArg, arg_idx, argc, argv);
}

static Bool gpac_expand_alias(int argc, char **argv)
{
	u32 i;

	for (i=0; i< (u32) argc; i++) {
		char *arg = argv[i];
		char *alias = (char *) gf_opts_get_key("gpac.alias", arg);
		if (alias == NULL) {
			if (gf_list_find(args_used, arg)<0) {
				push_arg(arg, 0);
			}
			continue;
		}
		char *sep;
		while (alias) {
			sep = strchr(alias, ' ');
			if (sep) sep[0] = 0;
			Bool ok = check_param_extension(alias, i, argc, argv);
			if (sep) sep[0] = ' ';
			if (!ok) return GF_FALSE;
			if (!sep) break;
			alias = sep+1;
		}

	}
	return GF_TRUE;
}

#include <gpac/unicode.h>
#include <gpac/base_coding.h>
#include <gpac/network.h>
#include <gpac/iso639.h>
#include <gpac/token.h>
#include <gpac/xml.h>
#include <gpac/thread.h>
#include <gpac/avparse.h>
#include <gpac/mpegts.h>
#include <gpac/rtp_streamer.h>
static u32 gpac_unit_tests(GF_MemTrackerType mem_track)
{
#ifdef GPAC_ENABLE_COVERAGE
	u32 ucs4_buf[4];
	u8 utf8_buf[7];

	void *mem = gf_calloc(4, sizeof(u32));
	gf_free(mem);

	if (mem_track == GF_MemTrackerNone) return 0;

	gpac_fsess_task_help(); //for coverage
	gf_dm_sess_last_error(NULL);

	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[CoreUnitTests] performing tests\n"));

	utf8_buf[0] = 'a';
	utf8_buf[1] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 1, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for single char\n"));
		return 1;
	}
	utf8_buf[0] = 0xc2;
	utf8_buf[1] = 0xa3;
	utf8_buf[2] = 'a';
	utf8_buf[3] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 3, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 2-byte + 1-byte char\n"));
		return 1;
	}
	utf8_buf[0] = 0xe0;
	utf8_buf[1] = 0xa4;
	utf8_buf[2] = 0xb9;
	utf8_buf[3] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 3, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 3-byte char\n"));
		return 1;
	}
	utf8_buf[0] = 0xf0;
	utf8_buf[1] = 0x90;
	utf8_buf[2] = 0x8d;
	utf8_buf[3] = 0x88;
	utf8_buf[4] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 4, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 4-byte char\n"));
		return 1;
	}

	utf8_buf[0] = 0xf8;
	utf8_buf[1] = 0x80;
	utf8_buf[2] = 0x80;
	utf8_buf[3] = 0x80;
	utf8_buf[4] = 0xaf;
	utf8_buf[5] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 5, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 5-byte char\n"));
		return 1;
	}
	utf8_buf[0] = 0xfc;
	utf8_buf[1] = 0x80;
	utf8_buf[2] = 0x80;
	utf8_buf[3] = 0x80;
	utf8_buf[4] = 0x80;
	utf8_buf[5] = 0xaf;
	utf8_buf[6] = 0;
	if (! utf8_to_ucs4 (ucs4_buf, 6, (unsigned char *) utf8_buf)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] UCS-4 translation failed for 6-byte char\n"));
		return 1;
	}
	//test error case
	utf8_buf[0] = 0xf8;
	utf8_to_ucs4 (ucs4_buf, 6, (unsigned char *) utf8_buf);

	char buf[5], obuf[3];
	obuf[0] = 1;
	obuf[1] = 2;
	u32 res = gf_base16_encode(obuf, 2, buf, 5);
	if (res != 4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] base16 encode fail\n"));
		return 1;
	}
	u32 res2 = gf_base16_decode(buf, res, obuf, 3);
	if (res2 != 2) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] base16 decode fail\n"));
		return 1;
	}

	char *zbuf;
	u32 osize;
	GF_Err e;
	char *ozbuf;

#ifndef GPAC_DISABLE_ZLIB
	zbuf = gf_strdup("123451234512345123451234512345");
	osize=0;
	e = gf_gz_compress_payload(&zbuf, 1 + (u32) strlen(zbuf), &osize);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] zlib compress fail\n"));
		gf_free(zbuf);
		return 1;
	}
	ozbuf=NULL;
	res=0;
	e = gf_gz_decompress_payload(zbuf, osize, &ozbuf, &res);
	gf_free(zbuf);
	if (ozbuf) gf_free(ozbuf);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] zlib decompress fail\n"));
		return 1;
	}
#endif

	zbuf = gf_strdup("123451234512345123451234512345");
	osize=0;
	e = gf_lz_compress_payload(&zbuf, 1+(u32) strlen(zbuf), &osize);
	if (e && (e!= GF_NOT_SUPPORTED)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] lzma compress fail\n"));
		gf_free(zbuf);
		return 1;
	}
	ozbuf=NULL;
	res=0;
	e = gf_lz_decompress_payload(zbuf, osize, &ozbuf, &res);
	gf_free(zbuf);
	if (ozbuf) gf_free(ozbuf);
	if (e && (e!= GF_NOT_SUPPORTED)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] lzma decompress fail\n"));
		return 1;
	}

	gf_htonl(0xAABBCCDD);
	gf_ntohl(0xAABBCCDD);
	gf_htons(0xAABB);
	gf_tohs(0xAABB);
	gf_errno_str(-1);

	/* these two lock the bash shell in test mode
	gf_prompt_set_echo_off(GF_TRUE);
	gf_prompt_set_echo_off(GF_FALSE);
	*/

	gf_net_set_ntp_shift(-1000);
	gf_net_get_ntp_diff_ms(gf_net_get_ntp_ts() );
	gf_net_get_timezone();
	gf_net_get_utc_ts(70, 1, 0, 0, 0, 0);

	gf_lang_get_count();
	gf_lang_get_2cc(2);
	GF_Blob b;
	b.data = (u8 *) "test";
	b.size = 5;
	char url[100];
	u8 *data;
	u32 size;
	sprintf(url, "gmem://%p", &b);

	gf_blob_get_data(url, &data, &size);
	if (!data || strcmp((char *)data, "test")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreUnitTests] blob url parsing fail\n"));
		return 1;
	}
	gf_sys_get_battery_state(NULL, NULL, NULL, NULL, NULL);
	gf_sys_get_process_id();
	data = (u8 *) gf_log_get_tools_levels();
	if (data) gf_free(data);

	sigint_catched = GF_TRUE;

#ifdef WIN32
	gpac_sig_handler(CTRL_C_EVENT);
#else
	gpac_sig_handler(SIGINT);
#endif

	gf_mkdir("testdir");
	gf_mkdir("testdir/somedir");
	strcpy(url, "testdir/somedir/test.bin");
	FILE *f=gf_fopen(url, "wb");
	fprintf(f, "some test\n");
#ifdef GPAC_MEMORY_TRACKING
	gf_memory_print();
#endif
	gf_fclose(f);
	gf_file_modification_time(url);
	gf_m2ts_probe_file(url);

	gf_cleanup_dir("testdir");
	gf_rmdir("testdir");

	//math.c not covered yet by our sample files
	GF_Matrix2D mx;
	gf_mx2d_init(mx);
	gf_mx2d_add_skew(&mx, FIX_ONE, FIX_ONE);
	gf_mx2d_add_skew_x(&mx, GF_PI/4);
	gf_mx2d_add_skew_y(&mx, GF_PI/4);
	GF_Point2D scale, translate;
	Fixed rotate;
	gf_mx2d_decompose(&mx, &scale, &rotate, &translate);
	GF_Rect rc1, rc2;
	memset(&rc1, 0, sizeof(GF_Rect));
	memset(&rc2, 0, sizeof(GF_Rect));
	gf_rect_equal(&rc1, &rc2);

	GF_Matrix mat;
	gf_mx_init(mat);
	Fixed yaw, pitch, roll;
	gf_mx_get_yaw_pitch_roll(&mat, &yaw, &pitch, &roll);

	GF_Ray ray;
	GF_Vec center, outPoint;
	memset(&ray, 0, sizeof(GF_Ray));
	ray.dir.z = FIX_ONE;
	memset(&center, 0, sizeof(GF_Vec));
	gf_ray_hit_sphere(&ray, &center, FIX_ONE, &outPoint);

	gf_closest_point_to_line(center, ray.dir, center);

	GF_Plane plane;
	plane.d = FIX_ONE;
	plane.normal = center;
	gf_plane_intersect_line(&plane, &center, &ray.dir, &outPoint);

	GF_Vec4 rot, quat;
	rot.x = rot.y = 0;
	rot.z = FIX_ONE;
	rot.q = GF_PI/4;
	quat = gf_quat_from_rotation(rot);
	gf_quat_get_inv(&quat);
	gf_quat_rotate(&quat, &ray.dir);
	gf_quat_slerp(quat, quat, FIX_ONE/2);
	GF_BBox bbox;
	memset(&bbox, 0, sizeof(GF_BBox));
	gf_bbox_equal(&bbox, &bbox);

	//token.c
	char container[1024];
	gf_token_get_strip("12 34{ 56 : }", 0, "{:", " ", container, 1024);

	//netwok.c
	char name[GF_MAX_IP_NAME_LEN];
	gf_sk_get_host_name(name);
	gf_sk_set_usec_wait(NULL, 1000);
	u32 fam;
	u16 port;
	//to remove once we have rtsp server back
	gf_sk_get_local_info(NULL, &port, &fam);
	gf_sk_receive_wait(NULL, NULL, 0, &fam, 1);
	gf_sk_send_wait(NULL, NULL, 0, 1);

	//path2D
	GF_Path *path = gf_path_new();
	gf_path_add_move_to(path, 0, 0);
	gf_path_add_quadratic_to(path, 5, 5, 10, 0);
	gf_path_point_over(path, 4, 0);
	gf_path_del(path);
	
	//xml dom - to update once we find a way to integrate atsc demux in tests
	GF_DOMParser *dom = gf_xml_dom_new();
	gf_xml_dom_parse_string(dom, "<Dummy>test</Dummy>");
	gf_xml_dom_get_error(dom);
	gf_xml_dom_get_line(dom);
	gf_xml_dom_get_root_nodes_count(dom);
	gf_xml_dom_del(dom);

	//downloader - to update once we find a way to integrate atsc demux in tests
	GF_DownloadManager *dm = gf_dm_new(NULL);
	gf_dm_set_auth_callback(dm, NULL, NULL);

	gf_dm_set_data_rate(dm, 0);
	gf_dm_get_data_rate(dm);
	gf_dm_set_localcache_provider(dm, NULL, NULL);

	const DownloadedCacheEntry ent = gf_dm_add_cache_entry(dm, "http://localhost/test.dummy", "test", 4, 0, 0, "application/octet-string", GF_FALSE, 1);

	gf_dm_force_headers(dm, ent, "x-GPAC: test\r\n");
	gf_dm_sess_enum_headers(NULL, NULL, NULL, NULL);//this one is deactivated in test mode in httpin because of Date: header
	gf_dm_sess_abort(NULL);
	gf_dm_del(dm);

	//constants
	gf_stream_type_afx_name(GPAC_AFX_3DMC);
	//thread
	gf_th_stop(NULL);
	gf_list_swap(NULL, NULL);
	//bitstream
	GF_BitStream *bs = gf_bs_new("test", 4, GF_BITSTREAM_READ);
	gf_bs_bits_available(bs);
	gf_bs_get_bit_offset(bs);
	gf_bs_read_vluimsbf5(bs);
	gf_bs_del(bs);
	//module
	gf_module_load_static(NULL);

	gf_mp3_version_name(0);
	char tsbuf[188];
	u8 is_pes=GF_TRUE;
	memset(tsbuf, 0, 188);
	tsbuf[0] = 0x47;
	tsbuf[1] = 0x40;
	tsbuf[4]=0x00;
	tsbuf[5]=0x00;
	tsbuf[6]=0x01;
	tsbuf[10] = 0x80;
	tsbuf[11] = 0xc0;
	tsbuf[13] = 0x2 << 4;
	gf_m2ts_restamp(tsbuf, 188, 1000, &is_pes);


	gf_filter_post_task(NULL,NULL,NULL,NULL);
	gf_filter_get_num_events_queued(NULL);
	gf_filter_get_arg(NULL, NULL, NULL);
	gf_filter_all_sinks_done(NULL);

	gf_opts_discard_changes();

	gf_rtp_reset_ssrc(NULL);
	gf_rtp_enable_nat_keepalive(NULL, 0);
//	gf_rtp_stop(NULL);
//	gf_rtp_get_current_time(NULL);
//	gf_rtp_is_unicast(NULL);
	gf_rtp_streamer_get_payload_type(NULL);
	gf_rtsp_unregister_interleave(NULL, 0);

#endif
	return 0;
}

static Bool revert_cache_file(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	const char *url;
	char *sep;
	GF_Config *cached;
	if (strncmp(item_name, "gpac_cache_", 11)) return GF_FALSE;
	cached = gf_cfg_new(NULL, item_path);
	url = gf_cfg_get_key(cached, "cache", "url");
	if (url) url = strstr(url, "://");
	if (url) {
		u32 i, len, dir_len=0, k=0;
		char *dst_name;
		sep = strstr(item_path, "gpac_cache_");
		if (sep) {
			sep[0] = 0;
			dir_len = (u32) strlen(item_path);
			sep[0] = 'g';
		}
		url+=3;
		len = (u32) strlen(url);
		dst_name = gf_malloc(len+dir_len+1);
		memset(dst_name, 0, len+dir_len+1);

		strncpy(dst_name, item_path, dir_len);
		k=dir_len;
		for (i=0; i<len; i++) {
			dst_name[k] = url[i];
			if (dst_name[k]==':') dst_name[k]='_';
			else if (dst_name[k]=='/') {
				if (!gf_dir_exists(dst_name))
					gf_mkdir(dst_name);
			}
			k++;
		}
		sep = strrchr(item_path, '.');
		if (sep) {
			sep[0]=0;
			if (gf_file_exists(item_path)) {
				gf_move_file(item_path, dst_name);
			}
			sep[0]='.';
		}
		gf_free(dst_name);
	}
	gf_cfg_del(cached);
	gf_delete_file(item_path);
	return GF_FALSE;
}

