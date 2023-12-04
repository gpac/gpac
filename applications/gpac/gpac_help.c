/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
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


#include <gpac/filters.h>
#include "gpac.h"

extern FILE *sidebar_md;
extern FILE *helpout;
extern u32 gen_doc;
extern u32 help_flags;
extern u32 list_filters;
extern GF_FilterSession *session;
extern char separator_set[7];
extern const char *auto_gen_md_warning;
extern u32 print_filter_info;
extern int alias_argc;
extern char **alias_argv;
extern GF_List *args_used;
extern GF_List *args_alloc;
extern u32 compositor_mode;
#define SEP_LIST	3


#ifndef GPAC_DISABLE_DOC
const char *gpac_doc =
"# Overview\n"
"Filters are configurable processing units consuming and producing data packets. These packets are carried "
"between filters through a data channel called __PID__. A PID is in charge of allocating/tracking data packets, "
"and passing the packets to the destination filter(s). A filter output PID may be connected to zero or more filters. "
"This fan-out is handled internally by GPAC (no such thing as a tee filter in GPAC).\n"
"Note: When a PID cannot be connected to any filter, a warning is thrown and all packets dispatched on "
"this PID will be destroyed. The session may however still run, unless [-full-link](CORE) is set.\n"
"  \nEach output PID carries a set of properties describing the data it delivers (e.g. __width__, __height__, __codec__, ...). Properties "
"can be built-in (see [gpac -h props](filters_properties) ), or user-defined. Each PID tracks "
"its properties changes and triggers filter reconfiguration during packet processing. This allows the filter chain to be "
"reconfigured at run time, potentially reloading part of the chain (e.g. unload a video decoder when switching from compressed "
"to uncompressed sources).\n"
"  \nEach filter exposes a set of argument to configure itself, using property types and values described as strings formatted with "
"separators. This help is given with default separator sets `:=#,@` to specify filters, properties and options. Use [-seps](GPAC) to change them.\n"
"# Property and filter option format\n"
"- boolean: formatted as `yes`,`true`,`1` or `no`,`false`,`0`\n"
"- enumeration (for filter arguments only): must use the syntax given in the argument description, otherwise value `0` (first in enum) is assumed.\n"
"- 1-dimension (numbers, floats, ints...): formatted as `value[unit]`, where `unit` can be `k`,`K` (x 1000) or `m`,`M` (x 1000000) or `g`,`G` (x 1000000000) or `sec` (x 1000) or `min` (x 60000). `+I` means max float/int/uint value, `-I` min float/int/uint value.\n"
"- fraction: formatted as `num/den` or `num-den` or `num`, in which case the denominator is 1 if `num` is an integer, or 1000000 if `num` is a floating-point value.\n"
"- unsigned 32 bit integer: formatted as number or hexadecimal using the format `0xAABBCCDD`.\n"
"- N-dimension (vectors): formatted as `DIM1xDIM2[xDIM3[xDIM4]]` values, without unit multiplier.\n"
"  - For 2D integer vectors, the following resolution names can be used: `360`, `480`, `576`, `720`, `1080`, `hd`, `2k`, `2160`, `4k`, `4320`, `8k`\n"
"- string: formatted as:\n"
"  - `value`: copies value to string.\n"
"  - `file@FILE`: load string from local `FILE` (opened in binary mode).\n"
"  - `bxml@FILE`: binarize XML from local `FILE` and set property type to data - see https://wiki.gpac.io/NHML-Format.\n"
"- data: formatted as:\n"
"  - `size@address`: constant data block, not internally copied; `size` gives the size of the block, `address` the data pointer.\n"
"  - `0xBYTESTRING`: data block specified in hexadecimal, internally copied.\n"
"  - `file@FILE`: load data from local `FILE` (opened in binary mode).\n"
"  - `bxml@FILE`: binarize XML from local `FILE` - see https://wiki.gpac.io/NHML-Format.\n"
"  - `b64@DATA`: load data from base-64 encoded `DATA`.\n"
"- pointer: pointer address as formatted by `%p` in C.\n"
"- string lists: formatted as `val1,val2[,...]`. Each value can also use `file@FILE` syntax.\n"
"- integer lists: formatted as `val1,val2[,...]`\n"
"Note: The special characters in property formats (0x,/,-,+I,-I,x) cannot be configured.\n"
"# Filter declaration [__FILTER__]\n"
"## Generic declaration\n"
"Each filter is declared by its name, with optional filter arguments appended as a list of colon-separated `name=value` pairs. Additional syntax is provided for:\n"
"- boolean: `value` can be omitted, defaulting to `true` (e.g. `:allt`). Using `!` before the name negates the result (e.g. `:!moof_first`)\n"
"- enumerations: name can be omitted, e.g. `:disp=pbo` is equivalent to `:pbo`.\n"
"\n  \n"
"When string parameters are used (e.g. URLs), it is recommended to escape the string using the keyword `gpac`.  \n"
"EX filter:ARG=http://foo/bar?yes:gpac:opt=VAL\n"
"This will properly extract the URL.\n"
"EX filter:ARG=http://foo/bar?yes:opt=VAL\n"
"This will fail to extract it and keep `:opt=VAL` as part of the URL.\n"
"The escape mechanism is not needed for local source, for which file existence is probed during argument parsing. "
"It is also not needed for builtin protocol handlers (`avin://`, `video://`, `audio://`, `pipe://`)\n"
"For `tcp://` and `udp://` protocols, the escape is not needed if a trailing `/` is appended after the port number.\n"
"EX -i tcp://127.0.0.1:1234:OPT\n"
"This will fail to extract the URL and options.\n"
"EX -i tcp://127.0.0.1:1234/:OPT\n"
"This will extract the URL and options.\n"
"Note: one trick to avoid the escape sequence is to declare the URLs option at the end, e.g. `f1:opt1=foo:url=http://bar`, provided you have only one URL parameter to specify on the filter.\n"
"\n"
"It is possible to disable option parsing (for string options) by duplicating the separator.\n"
"EX filter::opt1=UDP://IP:PORT/:someopt=VAL::opt2=VAL2\n"
"This will pass `UDP://IP:PORT/:someopt=VAL` to `opt1` without inspecting it, and `VAL2` to `opt2`.\n"
"  \n"
"## Source and Sink filters\n"
"Source and sink filters do not need to be addressed by the filter name, specifying `src=` or `dst=` instead is enough. "
"You can also use the syntax `-src URL` or `-i URL` for sources and `-dst URL` or `-o URL` for destination, this allows prompt completion in shells.\n"
"EX \"src=file.mp4\" or \"-src file.mp4\" or  \"-i file.mp4\"\n"
"This will find a filter (for example `fin`) able to load `file.mp4`. The same result can be achieved by using `fin:src=file.mp4`.\n"
"EX \"dst=dump.yuv\" or \"-dst dump.yuv\" or \"-o dump.yuv\"\n"
"This will dump the video content in `dump.yuv`. The same result can be achieved by using `fout:dst=dump.yuv`.\n"
"\n"
"Specific source or sink filters may also be specified using `filterName:src=URL` or `filterName:dst=URL`.\n"
"\n"
"The `src=` and `dst=` syntaxes can also be used in alias for dynamic argument cloning (see `gpac -hx alias`).\n"
"\n"
"## Forcing specific filters\n"
"There is a special option called `gfreg` which allows specifying preferred filters to use when handling URLs.\n"
"EX src=file.mp4:gfreg=ffdmx,ffdec\n"
"This will use __ffdmx__ to read `file.mp4` and __ffdec__ to decode it.\n"
"This can be used to test a specific filter when alternate filter chains are possible.\n"
"## Specifying encoders and decoders\n"
"By default filters chain will be resolved without any decoding/encoding if the destination accepts the desired format. "
"Otherwise, decoders/encoders will be dynamically loaded to perform the conversion, unless dynamic resolution is disabled. "
"There is a special shortcut filter name for encoders `enc` allowing to match a filter providing the desired encoding. "
"The parameters for `enc` are:\n"
"- c=NAME: identifies the desired codec. `NAME` can be the GPAC codec name or the encoder instance for ffmpeg/others\n"
"- b=UINT, rate=UINT, bitrate=UINT: indicates the bitrate in bits per second\n"
"- g=UINT, gop=UINT: indicates the GOP size in frames\n"
"- pfmt=NAME: indicates the target pixel format name (see [properties (-h props)](filters_properties) ) of the source, if supported by codec\n"
"- all_intra=BOOL: indicates all frames should be intra frames, if supported by codec\n"
"\n"
"Other options will be passed to the filter if it accepts generic argument parsing (as is the case for ffmpeg).\n"
"The shortcut syntax `c=TYPE` (e.g. `c=aac:opts`) is also supported.\n"
"\n"
"EX gpac -i dump.yuv:size=320x240:fps=25 enc:c=avc:b=150000:g=50:cgop=true:fast=true -o raw.264\n"
"This creates a 25 fps AVC at 175kbps with a gop duration of 2 seconds, using closed gop and fast encoding settings for ffmpeg.\n"
"\n"
"The inverse operation (forcing a decode to happen) is possible using the __reframer__ filter.\n"
"EX gpac -i file.mp4 reframer:raw=av -o null\n"
"This will force decoding media from `file.mp4` and trash (send to `null`) the result (doing a decoder benchmark for example).\n"
"## Escaping option separators\n"
"When a filter uses an option defined as a string using the same separator character as gpac, you can either "
"modify the set of separators, or escape the separator by duplicating it. The options enclosed by duplicated "
"separator are not parsed. This is mostly used for meta filters, such as ffmpeg, to pass options to sub-filters "
"such as libx264 (cf `x264opts` parameter).\n"
"EX f:a=foo:b=bar\n"
"This will set option `a` to `foo` and option `b` to `bar` on the filter.\n"
"EX f::a=foo:b=bar\n"
"This will set option `a` to `foo:b=bar` on the filter.\n"
"EX f:a=foo::b=bar:c::d=fun\n"
"This will set option `a` to `foo`, `b` to `bar:c` and the option `d` to `fun` on the filter.\n"
"\n"
"# Filter linking [__LINK__]\n"
"\n"
"Each filter exposes one or more sets of capabilities, called __capability bundle__, which are property type and values "
"that must be matched or excluded by connecting PIDs.\n"
"To check the possible sources and destination for a filter `FNAME`, use `gpac -h links FNAME`\n"
"\n"
"The filter graph resolver uses this information together with the PID properties to link the different filters.\n"
"\n"
"Link directives, when provided, specify which source a filter can accept connections from.\n"
"__They do not specify which destination a filter can connect to.__\n"
"\n"
"## Default filter linking\n"
"When no link instructions are given (see below), the default linking strategy used is either __implicit mode__ (default in `gpac`) or __complete mode__ (if [-cl](GPAC) is set).\n"
"Each PID is checked for possible connection to all defined filters, in their declaration order.\n"
"For each filter `DST` accepting a connection from the PID, directly or with intermediate filters:\n"
"- if `DST` filter has link directives, use them to allow or reject PID connection.\n"
"- otherwise, if __complete mode__ is enabled, allow connection..\n"
"- otherwise (__implicit mode__):\n"
" - if `DST` is not a sink and is the first matching filter with no link directive, allow connection.\n"
" - otherwise, if `DST` is not a sink and is not the first matching filter with no link directive, reject connection.\n"
" - otherwise (`DST` is a sink) and no previous connections to a non-sink filter, allow connection.\n"
"\n"
"In all linking modes, a filter can prevent being linked to a filter with no link directives by setting `RSID` option on the filter.\n"
"This is typically needed when dynamically inserting/removing filters in an existing session where some filters have no ID defined and are not desired for the inserted chain.\n"
"\n"
"EX gpac -i file.mp4 c=avc -o output\n"
"With this setup in __implicit mode__:\n"
"- if the file has a video PID, it will connect to `enc` but not to `output`. The output PID of `enc` will connect to `output`.\n"
"- if the file has other PIDs than video, they will connect to `output`, since this `enc` filter accepts only video.\n"
"\n"
"EX gpac -cl -i file.mp4 c=avc -o output\n"
"With this setup in __complete mode__:\n"
"- if the file has a video PID, it will connect both to `enc` and to `output`, and the output PID of `enc` will connect to `output`.\n"
"- if the file has other PIDs than video, they will connect to `output`.\n"
"\n"
"Furthermore in __implicit mode__, filter connections are restricted to filters defined between the last source and the sink(s).\n"
"EX gpac -i video1 reframer:saps=1 -i video2 ffsws:osize=128x72 -o output\n"
"This will connect:\n"
"- `video1` to `reframer` then `reframer` to `output` but will prevent `reframer` to `ffsws` connection.\n"
"- `video2` to `ffsws` then `ffsws` to `output` but will prevent `video2` to `reframer` connection.\n"
"\n"
"EX gpac -i video1 -i video2 reframer:saps=1 ffsws:osize=128x72 -o output\n"
"This will connect `video1` AND `video2` to `reframer->ffsws->output`\n"
"\n"
"The __implicit mode__ allows specifying linear processing chains (no PID fan-out except for final output(s)) without link directives, simplifying command lines for common cases.\n"
"Warning: Argument order really matters in implicit mode!\n"
"\n"
"EX gpac -i file.mp4 c=avc c=aac -o output\n"
"If the file has a video PID, it will connect to `c=avc` but not to `output`. The output PID of `c=avc` will connect to `output`.\n"
"If the file has an audio PID, it will connect to `c=aac` but not to `output`. The output PID of `c=aac` will connect to `output`.\n"
"If the file has other PIDs than audio or video, they will connect to `output`.\n"
"\n"
"EX gpac -i file.mp4 ffswf=osize:128x72 c=avc resample=osr=48k c=aac -o output\n"
"This will force:\n"
"- `SRC(video)->ffsws->enc(video)->output` and prevent `SRC(video)->output`, `SRC(video)->enc(video)` and `ffsws->output` connections which would happen in __complete mode__.\n"
"- `SRC(audio)->resample->enc(audio)->output` and prevent `SRC(audio)->output`, `SRC(audio)->enc(audio)` and `resample->output` connections which would happen in __complete mode__.\n"
"\n"
"## Quick links\n"
"Link between filters may be manually specified. The syntax is an `@` character optionally followed by an integer (0 if omitted).\n"
"This indicates that the following filter specified at prompt should be linked only to a previous listed filter.\n"
"The optional integer is a 0-based index to the previous filter declarations, 0 indicating the previous filter declaration, 1 the one before the previous declaration, ...).\n"
"If `@@` is used instead of `@`, the optional integer gives the filter index starting from the first filter (index 0) specified in command line.\n"
"Several link directives can be given for a filter.\n"
"EX fA fB @1 fC\n"
"This indicates that `fC` only accepts inputs from `fA`.\n"
"EX fA fB fC @1 @0 fD\n"
"This indicates that `fD` only accepts inputs from `fB` and `fC`.\n"
"EX fA fB fC ... @@1 fZ\n"
"This indicates that `fZ` only accepts inputs from `fB`.\n"
"\n"
"## Complex links\n"
"The `@` link directive is just a quick shortcut to set the following filter arguments:\n"
"- FID=name: assigns an identifier to the filter\n"
"- SID=name1[,name2...]: sets a list of filter identifiers, or __sourceIDs__, restricting the list of possible inputs for a filter.\n"
"\n"
"EX fA fB @1 fC\n"
"This is equivalent to `fA:FID=1 fB fC:SID=1`.\n"
"EX fA:FID=1 fB fC:SID=1\n"
"This indicates that `fC` only accepts input from `fA`, but `fB` might accept inputs from `fA`.\n"
"EX fA:FID=1 fB:FID=2 fC:SID=1 fD:SID=1,2\n"
"This indicates that `fD` only accepts input from `fA` and `fB` and `fC` only from `fA`\n"
"Note: A filter with sourceID set cannot get input from filters with no IDs.\n"
"\n"
"A sourceID name can be further extended using fragment identifier (`#` by default):\n"
"- name#PIDNAME: accepts only PID(s) with name `PIDNAME`\n"
"- name#TYPE: accepts only PIDs of matching media type. TYPE can be `audio`, `video`, `scene`, `text`, `font`, `meta`\n"
"- name#TYPEN: accepts only `N` (1-based index) PID of matching type from source (e.g. `video2` to only accept second video PID)\n"
"- name#TAG=VAL: accepts the PID if its parent filter has no tag or a tag matching `VAL`\n"
"- name#ITAG=VAL: accepts the PID if its parent filter has no inherited tag or an inherited tag matching `VAL`\n"
"- name#P4CC=VAL: accepts only PIDs with builtin property of type `P4CC` and value `VAL`.\n"
"- name#PName=VAL: same as above, using the builtin name corresponding to the property.\n"
"- name#AnyName=VAL: same as above, using the name of a non built-in property.\n"
"- name#Name=OtherPropName: compares the value with the value of another property of the PID. The matching will fail if the value to compare to is not present or different from the value to check. The property to compare with shall be a built-in property.\n"
"If the property is not defined on the PID, the property is matched. Otherwise, its value is checked against the given value.\n"
"\n"
"The following modifiers for comparisons are allowed (for any fragment format using `=`):\n"
"- name#P4CC=!VAL: accepts only PIDs with property NOT matching `VAL`.\n"
"- name#P4CC-VAL: accepts only PIDs with property strictly less than `VAL` (only for 1-dimension number properties).\n"
"- name#P4CC+VAL: accepts only PIDs with property strictly greater than `VAL` (only for 1-dimension number properties).\n"
"\n"
"A sourceID name can also use wildcard or be empty to match a property regardless of the source filter.\n"
"EX fA fB:SID=*#ServiceID=2\n"
"EX fA fB:SID=#ServiceID=2\n"
"This indicates to match connection between `fA` and `fB` only for PIDs with a `ServiceID` property of `2`.\n"
"These extensions also work with the __LINK__ `@` shortcut.\n"
"EX fA fB @1#video fC\n"
"This indicates that `fC` only accepts inputs from `fA`, and of type video.\n"
"EX gpac -i img.heif @#ItemID=200 vout\n"
"This indicates to connect to `vout` only PIDs with `ItemID` property equal to `200`.\n"
"EX gpac -i vid.mp4 @#PID=1 vout\n"
"This indicates to connect to `vout` only PIDs with `ID` property equal to `1`.\n"
"EX gpac -i vid.mp4 @#Width=640 vout\n"
"This indicates to connect to `vout` only PIDs with `Width` property equal to `640`.\n"
"EX gpac -i vid.mp4 @#Width-640 vout\n"
"This indicates to connect to `vout` only PIDs with `Width` property less than `640`\n"
"EX gpac -i vid.mp4 @#ID=ItemID#ItemNumber=1 vout\n"
"This will connect to `vout` only PID with an ID property equal to ItemID property (keep items, discard tracks) and an Item number of 1 (first item).\n"
"\n"
"Multiple fragment can be specified to check for multiple PID properties.\n"
"EX gpac -i vid.mp4 @#Width=640#Height+380 vout\n"
"This indicates to connect to `vout` only PIDs with `Width` property equal to `640` and `Height` greater than `380`.\n"
"\n"
"Warning: If a PID directly connects to one or more explicitly loaded filters, no further dynamic link resolution will "
"be done to connect it to other filters with no sourceID set. Link directives should be carefully setup.\n"
"EX fA @ reframer fB\n"
"If `fB` accepts inputs provided by `fA` but `reframer` does not, this will link `fA` PID to `fB` filter since `fB` has no sourceID.\n"
"Since the PID is connected, the filter engine will not try to solve a link between `fA` and `reframer`.\n"
"\n"
"An exception is made for local files: by default, a local file destination will force a remultiplex of input PIDs from a local file.\n"
"EX gpac -i file.mp4 -o dump.mp4\n"
"This will prevent direct connection of PID of type `file` to dst `file.mp4`, remultiplexing the file.\n"
"\n"
"The special option `nomux` is used to allow direct connections (ignored for non-sink filters).\n"
"EX gpac -i file.mp4 -o dump.mp4:nomux\n"
"This will result in a direct file copy.\n"
"\n"
"This only applies to local files destination. For pipes, sockets or other file outputs (HTTP, ROUTE):\n"
"- direct copy is enabled by default\n"
"- `nomux=0` can be used to force remultiplex\n"
"\n"
"## Sub-session tagging\n"
"Filters may be assigned to a sub-session using `:FS=N`, with `N` a positive integer.\n"
"Filters belonging to different sub-sessions may only link to each-other:\n"
"- if explicitly allowed through sourceID directives (`@` or `SID`)\n"
"- or if they have the same sub-session identifier\n"
"\n"
"This is mostly used for __implicit mode__ in `gpac`: each first source filter specified after a sink filter will trigger a new sub-session.\n"
"EX gpac -i in1.mp4 -i in2.mp4 -o out1.mp4 -o out2.mp4\n"
"This will result in both inputs multiplexed in both outputs.\n"
"EX gpac -i in1.mp4 -o out1.mp4 -i in2.mp4 -o out2.mp4\n"
"This will result in in1 mixed to out1 and in2 mixed to out2, these last two filters belonging to a different sub-session.\n"
"\n"
"# Arguments inheriting\n"
"Unless explicitly disabled (see [-max-chain](CORE)), the filter engine will resolve implicit or explicit (__LINK__) connections "
"between filters and will allocate any filter chain required to connect the filters. "
"In doing so, it loads new filters with arguments inherited from both the source and the destination.\n"
"EX gpac -i file.mp4:OPT -o file.aac -o file.264\n"
"This will pass the `:OPT` to all filters loaded between the source and the two destinations.\n"
"EX gpac -i file.mp4 -o file.aac:OPT -o file.264\n"
"This will pass the `:OPT` to all filters loaded between the source and the file.aac destination.\n"
"Note: the destination arguments inherited are the arguments placed **AFTER** the `dst=` option.\n"
"EX gpac -i file.mp4 fout:OPTFOO:dst=file.aac:OPTBAR\n"
"This will pass the `:OPTBAR` to all filters loaded between `file.mp4` source and `file.aac` destination, but not `OPTFOO`.\n"
"Arguments inheriting can be stopped by using the keyword `gfloc`: arguments after the keyword will not be inherited.\n"
"EX gpac -i file.mp4 -o file.aac:OPTFOO:gfloc:OPTBAR -o file.264\n"
"This will pass `:OPTFOO` to all filters loaded between `file.mp4` source and `file.aac` destination, but not `OPTBAR`\n"
"Arguments are by default tracked to check if they were used by the filter chain, and a warning is thrown if this is not the case.\n"
"It may be useful to specify arguments which may not be consumed depending on the graph resolution; the specific keyword `gfopt` indicates that arguments after the keyword will not be tracked.\n"
"EX gpac -i file.mp4 -o file.aac:OPTFOO:gfopt:OPTBAR -o file.264\n"
"This will warn if `OPTFOO` is not consumed, but will not track `OPTBAR`.\n"
"  \n"
"A filter may be assigned a name (for inspection purposes, not inherited) using `:N=name` option. This name is not used in link resolution and may be changed at runtime by the filter instance.\n"
"  \n"
"A filter may be assigned a tag (any string) using `:TAG=name` option. This tag does not need to be unique, and can be used to exclude filter in link resolution. Tags are not inherited, therefore dynamically loaded filters never have a tag.\n"
"  \n"
"A filter may also be assigned an inherited tag (any string) using `:ITAG=name` option. Such tags are inherited, and are typically used to track dynamically loaded filters.\n"
"  \n"
"# URL templating\n"
"Destination URLs can be dynamically constructed using templates. Pattern `$KEYWORD$` is replaced in the template with the "
"resolved value and `$KEYWORD%%0Nd$` is replaced in the template with the resolved integer, padded with up to N zeros if needed.\n"
"`KEYWORD` is **case sensitive**, and may be present multiple times in the string. Supported `KEYWORD`:\n"
"- num: replaced by file number if defined, 0 otherwise\n"
"- PID: ID of the source PID\n"
"- URL: URL of source file\n"
"- File: path on disk for source file; if not found, use URL if set, or PID name otherwise\n"
"- Type: name of stream type of PID (`video`, `audio` ...)\n"
"- p4cc=ABCD: uses PID property with 4CC value `ABCD`\n"
"- pname=VAL: uses PID property with name `VAL`\n"
"- cts, dts, dur, sap: uses properties of first packet in PID at template resolution time\n"
"- OTHER: locates property 4CC for the given name, or property name if no 4CC matches.\n"
"  \n"
"`$$` is an escape for $\n"
"\n"
"Templating can be useful when encoding several qualities in one pass.\n"
"EX gpac -i dump.yuv:size=640x360 vcrop:wnd=0x0x320x180 c=avc:b=1M @2 c=avc:b=750k -o dump_$CropOrigin$x$Width$x$Height$.264\n"
"This will create a cropped version of the source, encoded in AVC at 1M, and a full version of the content in AVC at 750k. "
"Outputs will be `dump_0x0x320x180.264` for the cropped version and `dump_0x0x640x360.264` for the non-cropped one.\n"
"# Cloning filters\n"
"When a filter accepts a single connection and has a connected input, it is no longer available for dynamic resolution. "
"There may be cases where this behavior is undesired. Take a HEIF file with N items and do:\n"
"EX gpac -i img.heif -o dump_$ItemID$.jpg\n"
"In this case, only one item (likely the first declared in the file) will connect to the destination.\n"
"Other items will not be connected since the destination only accepts one input PID.\n"
"EX gpac -i img.heif -o dump_$ItemID$.jpg\n"
"In this case, the destination will be cloned for each item, and all will be exported to different JPEGs thanks to URL templating.\n"
"EX gpac -i vid.mpd c=avc:FID=1 -o transcode.mpd:SID=1\n"
"In this case, the encoder will be cloned for each video PIDs in the source, and the destination will only use PIDs coming from the encoders.\n"
"\n"
"When implicit linking is enabled, all filters are by default clonable. This allows duplicating the processing for each PIDs of the same type.\n"
"EX gpac -i dual_audio resample:osr=48k c=aac -o dst\n"
"The `resampler` filter will be cloned for each audio PID, and the encoder will be cloned for each resampler output.\n"
"You can explicitly deactivate the cloning instructions:\n"
"EX gpac -i dual_audio resample:osr=48k:clone=0 c=aac -o dst\n"
"The first audio will connect to the `resample` filter, the second to the `enc` filter and the `resample` output will connect to a clone of the `enc` filter.\n"
"\n"
"# Templating filter chains\n"
"There can be cases where the number of desired outputs depends on the source content, for example dumping a multiplex of N services into N files. When the destination involves multiplexing the input PIDs, the `:clone` option is not enough since the multiplexer will always accept the input PIDs.\n"
"To handle this, it is possible to use a PID property name in the sourceID of a filter with the value `*` or an empty value. In this case, whenever a new PID with a new value for the property is found, the filter with such sourceID will be dynamically cloned.\n"
"Warning: This feature should only be called with a single property set to `*` (or empty) per source ID, results are undefined otherwise.\n"
"EX gpac -i source.ts -o file_$ServiceID$.mp4:SID=*#ServiceID=*\n"
"EX gpac -i source.ts -o file_$ServiceID$.mp4:SID=#ServiceID=\n"
"In this case, each new `ServiceID` value found when connecting PIDs to the destination will create a new destination file.\n"
"\n"
"Cloning in implicit linking mode applies to output as well:\n"
"EX gpac -i dual_audio -o dst_$PID$.aac\n"
"Each audio track will be dumped to aac (potentially reencoding if needed).\n"
"\n"
"# Assigning PID properties\n"
"It is possible to define properties on output PIDs that will be declared by a filter. This allows tagging parts of the "
"graph with different properties than other parts (for example `ServiceID`). "
"The syntax is the same as filter option, and uses the fragment separator to identify properties, e.g. `#Name=Value`.\n"
"This sets output PIDs property (4cc, built-in name or any name) to the given value. Value can be omitted for boolean "
"(defaults to true, e.g. `:#Alpha`).\n"
"Non built-in properties are parsed as follows:\n"
"- `file@FOO` will be declared as string with a value set to the content of `FOO`.\n"
"- `bxml@FOO` will be declared as data with a value set to the binarized content of `FOO`.\n"
"- `FOO` will be declared as string with a value set to `FOO`.\n"
"- `TYPE@FOO` will be parsed according to `TYPE`. If the type is not recognized, the entire value is copied as string. See `gpac -h props` for defined types.\n"
"\n"
"User-assigned PID properties on filter `fA` will be inherited by all filters dynamically loaded to solve `fA -> fB` connection.\n"
"If `fB` also has user-assigned PID properties, these only apply starting from `fB` in the chain and are not inherited by filters between `fA` and `fB`.\n"
"\n"
"Warning: Properties are not filtered and override the properties of the filter's output PIDs, be careful not to break "
"the session by overriding core properties such as width/height/samplerate/... !\n"
"EX gpac -i v1.mp4:#ServiceID=4 -i v2.mp4:#ServiceID=2 -o dump.ts\n"
"This will multiplex the streams in `dump.ts`, using `ServiceID` 4 for PIDs from `v1.mp4` and `ServiceID` 2 for PIDs from `v2.mp4`.\n"
"\n"
"PID properties may be conditionally assigned by checking other PID properties. The syntax uses parenthesis (not configurable) after the property assignment sign:\n"
"`#Prop=(CP=CV)VAL`\n"
"This will assign PID property `Prop` to `VAL` for PIDs with property `CP` equal to `CV`.\n"
"`#Prop=(CP=CV)VAL,(CP2=CV2)VAL2`\n"
"This will assign PID property `Prop` to `VAL` for PIDs with property `CP` equal to `CV`, and to `VAL2` for PIDs with property `CP2` equal to `CV2`.\n"
"`#Prop=(CP=CV)(CP2=CV2)VAL`\n"
"This will assign PID property `Prop` to `VAL` for PIDs with property `CP` equal to `CV` and property `CP2` equal to `CV2`.\n"
"`#Prop=(CP=CV)VAL,()DEFAULT`\n"
"This will assign PID property `Prop` to `VAL` for PIDs with property `CP` equal to `CV`, or to `DEFAULT` for other PIDs.\n"
"The condition syntax is the same as source ID fragment syntax.\n"
"Note: When set, the default value (empty condition) always matches the PID, therefore it should be placed last in the list of conditions.\n"
"EX gpac -i source.mp4:#MyProp=(audio)\"Super Audio\",(video)\"Super Video\"\n"
"This will assign property `MyProp` to `Super Audio` for audio PIDs and to `Super Video` for video PIDs.\n"
"EX gpac -i source.mp4:#MyProp=(audio1)\"Super Audio\"\n"
"This will assign property `MyProp` to `Super Audio` for first audio PID declared.\n"
"EX gpac -i source.mp4:#MyProp=(Width+1280)HD\n"
"This will assign property `MyProp` to `HD` for PIDs with property `Width` greater than 1280.\n"
"\n"
"The property value can use templates with the following keywords:\n"
"- $GINC(init[,inc]) or @GINC(...): replaced by integer for each new output PID of the filter (see specific filter options for details on syntax)\n"
"- PROP (enclosed between `$` or `@`): replaced by serialized value of property `PROP` (name or 4CC) of the PID or with empty string if no such property\n"
"EX gpac -i source.ts:#ASID=$PID$\n"
"This will assign DASH AdaptationSet ID to the PID ID value.\n"
"EX gpac -i source.ts:#RepresentationID=$ServiceID$\n"
"This will assign DASH Representation ID to the PID ServiceID value.\n"
"\n"
"# Using option files\n"
"It is possible to use a file to define options of a filter, by specifying the target file name as an option without value, i.e. `:myopts.txt`.\n"
"Warning: Only local files are allowed.\n"
"An option file is a simple text file containing one or more options or PID properties on one or more lines.\n"
"- A line beginning with \"//\" is a comment and is ignored (not configurable).\n"
"- A line beginning with \":\" indicates an escaped option (the entire line is parsed as a single option).\n"
"Options in an option file may point to other option files, with a maximum redirection level of 5.\n"
"An option file declaration (`filter:myopts.txt`) follows the same inheritance rules as regular options.\n"
"EX gpac -i source.mp4:myopts.txt:foo=bar -o dst\n"
"Any filter loaded between `source.mp4` and `dst` will inherit both `myopts.txt` and `foo` options and will resolve options and PID properties given in `myopts.txt`.\n"
"\n"
"# Ignoring filters at run-time\n"
"The special option `ccp` can be used to replace filters with an identity filter at run-time based on the input codec ID.\n"
"The option is a list of codec IDs to check. For encoder filters, an empty list reuses the encoder codec type.\n"
"When the PID codec ID matches one of the specified codec, the filter is replaced with a reframer filter with single PID input and same name and ID.\n"
"EX -i src c=avc:b=1m:ccp -o mux\n"
"This will replace the encoder filter with a reframer if the input PID is in AVC|H264 format, or uses the encoder for other visual PIDs.\n"
"EX -i src c=avc:b=1m:ccp=avc,hevc -o mux\n"
"This will replace the encoder filter with a reframer if the input PID is in AVC|H264 or HEVC format, or uses the encoder for other visual PIDs.\n"
"EX -i src cecrypt:cfile=drm.xml:ccp=aac -o mux\n"
"This will replace the encryptor filter with a reframer if the input PID is in AAC format, or uses the encryptor for other PIDs.\n"
"\n"
"# Specific filter options\n"
"Some specific keywords are replaced when processing filter options.\n"
"Warning: These keywords do not apply to PID properties. Multiple keywords cannot be defined for a single option.\n"
"Defined keywords:\n"
"- $GSHARE: replaced by system path to GPAC shared directory (e.g. /usr/share/gpac)\n"
"- $GJS: replaced by the first path from global share directory and paths set through [-js-dirs](CORE) that contains the file name following the macro, e.g. $GJS/source.js\n"
"- $GDOCS: replaced by system path to:\n"
"  - application document directory for iOS\n"
"  - `EXTERNAL_STORAGE` environment variable if present or `/sdcard` otherwise for Android\n"
"  - user home directory for other platforms\n"
"- $GLANG: replaced by the global config language option [-lang](CORE)\n"
"- $GUA: replaced by the global config user agent option [-user-agent](CORE)\n"
"- $GINC(init_val[,inc]): replaced by `init_val` and increment `init_val` by `inc` (positive or negative number, 1 if not specified) each time a new filter using this string is created.\n"
"\n"
"The `$GINC` construct can be used to dynamically assign numbers in filter chains:\n"
"EX gpac -i source.ts tssplit @#ServiceID= -o dump_$GINC(10,2).ts\n"
"This will dump first service in dump_10.ts, second service in dump_12.ts, etc...\n"
"\n"
"As seen previously, the following options may be set on any filter, but are not visible in individual filter help:\n"
"- FID: filter identifier\n"
"- SID: filter source(s) (string value)\n"
"- N=NAME: filter name (string value)\n"
"- FS: sub-session identifier (unsigned int value)\n"
"- RSID: require sourceID to be present on target filters (no value)\n"
"- TAG: filter tag (string value)\n"
"- ITAG: filter inherited tag (string value)\n"
"- FBT: buffer time in microseconds (unsigned int value)\n"
"- FBU: buffer units (unsigned int value)\n"
"- FBD: decode buffer time in microseconds (unsigned int value)\n"
"- clone: explicitly enable/disable filter cloning flag (no value)\n"
"- nomux: enable/disable direct file copy (no value)\n"
"- gfreg: preferred filter registry names for link solving (string value)\n"
"- gfloc: following options are local to filter declaration, not inherited (no value)\n"
"- gfopt: following options are not tracked (no value)\n"
"- gpac: argument separator for URLs (no value)\n"
"- ccp: filter replacement control (string list value)\n"
"\n"
"The buffer control options are used to change the default buffering of PIDs of a filter:\n"
"- `FBT` controls the maximum buffer time of output PIDs of a filter\n"
"- `FBU` controls the maximum number of packets in buffer of output PIDs of a filter when timing is not available\n"
"- `FBD` controls the maximum buffer time of input PIDs of a decoder filter, ignored for other filters\n"
"\n"
"If another filter sends a buffer requirement messages, the maximum value of `FBT` (resp. `FBD`) and the user requested buffer time will be used for output buffer time (resp. decoding buffer time).\n"
"\n"
"These options can be set:\n"
"- per filter instance: `fA reframer:FBU=2`\n"
"- per filter class for the run: `--reframer@FBU=2`\n"
"- in the GPAC config file in a per-filter section: `[filter@reframer]FBU=2`\n"
"\n"
"The default values are defined by the session default parameters `-buffer-gen`, `buffer-units` and `-buffer-dec`.\n"
"\n"
"# External filters\n"
"GPAC comes with a set of built-in filters in libgpac. It may also load external filters in dynamic libraries, located in "
"default module folder or folders listed in [-mod-dirs](CORE) option. The files shall be named `gf_*` and shall export"
" a single function `RegisterFilter` returning a filter register - see [libgpac documentation](https://doxygen.gpac.io/) for more details.\n"
"\n";
#endif

void gpac_filter_help(void)
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
#include <gpac/modules/font.h>

void gpac_modules_help(char *mod_name)
{
	u32 i;
	if (mod_name) {
		gf_opts_set_key("temp", "gendoc", "yes");
		GF_BaseInterface *ifce = gf_modules_load_by_name(mod_name, GF_VIDEO_OUTPUT_INTERFACE, GF_TRUE);
		if (!ifce) ifce = gf_modules_load_by_name(mod_name, GF_AUDIO_OUTPUT_INTERFACE, GF_TRUE);
		if (!ifce) ifce = gf_modules_load_by_name(mod_name, GF_FONT_READER_INTERFACE, GF_TRUE);
		if (!ifce) ifce = gf_modules_load_by_name(mod_name, GF_COMPOSITOR_EXT_INTERFACE, GF_TRUE);
		if (!ifce) ifce = gf_modules_load_by_name(mod_name, GF_HARDCODED_PROTO_INTERFACE, GF_TRUE);

		if (!ifce) {
			fprintf(stderr, "No such module %s\n", mod_name);
			return;
		}
		gf_sys_format_help(helpout, help_flags, "%s\n", ifce->description ? ifce->description : "No description available");
		if (ifce->args) {
			gf_sys_format_help(helpout, help_flags, "Options:\n");
			i=0;
			while (ifce->args && ifce->args[i].name) {
				gf_sys_print_arg(helpout, help_flags, &ifce->args[i], ifce->module_name);
				i++;
			}
		} else {
			gf_sys_format_help(helpout, help_flags, "No Option\n");
		}
		gf_modules_close_interface(ifce);
		return;
	}
	gf_sys_format_help(helpout, help_flags, "Available modules:\n");
	for (i=0; i<gf_modules_count(); i++) {
		char *str = (char *) gf_modules_get_file_name(i);
		if (!str) continue;
		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s: implements ", str);
		if (!strncmp(str, "gm_", 3) || !strncmp(str, "gsm_", 4)) {
			GF_BaseInterface *ifce = gf_modules_load_by_name(str, GF_VIDEO_OUTPUT_INTERFACE, GF_FALSE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "VideoOutput (%s) ", ifce->module_name);
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_AUDIO_OUTPUT_INTERFACE, GF_FALSE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "AudioOutput (%s) ", ifce->module_name);
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_FONT_READER_INTERFACE, GF_FALSE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "FontReader (%s) ", ifce->module_name);
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_COMPOSITOR_EXT_INTERFACE, GF_FALSE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "CompositorExtension (%s) ", ifce->module_name);
				gf_modules_close_interface(ifce);
			}
			ifce = gf_modules_load_by_name(str, GF_HARDCODED_PROTO_INTERFACE, GF_FALSE);
			if (ifce) {
				gf_sys_format_help(helpout, help_flags, "HardcodedProto (%s) ", ifce->module_name);
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
"This allows later audio and video playback using `gpac -i src.mp4 output`\n"
"\n"
"Aliases can use arguments from the command line. The allowed syntaxes are:\n"
"- `@{a}`: replaced by the value of the argument with index `a` after the alias\n"
"- `@{a,b}`: replaced by the value of the arguments with index `a` and `b`\n"
"- `@{a:b}`: replaced by the value of the arguments between index `a` and `b`\n"
"- `@{-a,b}`: replaced by the value of the arguments with index `a` and `b`, inserting a list separator (comma by default) between them\n"
"- `@{-a:b}`: replaced by the value of the arguments between index `a` and `b`, inserting a list separator (comma by default) between them\n"
"- `@{+a,b}`: clones the parent word in the alias for `a` and `b`, replacing this pattern in each clone by the corresponding argument\n"
"- `@{+a:b}`: clones the parent word in the alias for each argument between index `a` and `b`, replacing this pattern in each clone by the corresponding argument\n"
"\n"
"The specified index can be:\n"
"- forward index: a strictly positive integer, 1 being the first argument after the alias\n"
"- backward index: the value 'n' (or 'N') to indicate the last argument on the command line. This can be followed by `-x` to rewind arguments (e.g. `@{n-1}` is the before last argument)\n"
"\n"
"Before solving aliases, all option arguments are moved at the beginning of the command line. This implies that alias arguments cannot be options.\n"
"Arguments not used by any aliases are kept on the command line, other ones are removed\n"
"\n"
"EX -alias=\"foo src=@{N} dst=test.mp4\"\n"
"The command `gpac foo f1 f2` expands to `gpac src=f2 dst=test.mp4 f1`\n"
"EX -alias=\"list: inspect src=@{+:N}\"\n"
"The command `gpac list f1 f2 f3` expands to `gpac inspect src=f1 src=f2 src=f3`\n"
"EX -alias=\"list inspect src=@{+2:N}\"\n"
"The command `gpac list f1 f2 f3` expands to `gpac inspect src=f2 src=f3 f1`\n"
"EX -alias=\"plist aout vout flist:srcs=@{-,N}\"\n"
"The command `gpac plist f1 f2 f3` expands to `gpac aout vout flist:srcs=\"f1,f2,f3\"`  \n"
"\n"
"Alias documentation can be set using `gpac -aliasdoc=\"NAME VALUE\"`, with `NAME` the alias name and `VALUE` the documentation.\n"
"Alias documentation will then appear in gpac help.\n"
"\n";
#endif

void gpac_alias_help(GF_SysArgMode argmode)
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


void gpac_core_help(GF_SysArgMode mode, Bool for_logs)
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

static GF_GPACArg gpac_args[] =
{
#ifdef GPAC_MEMORY_TRACKING
 	GF_DEF_ARG("mem-track", NULL, "enable memory tracker", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
 	GF_DEF_ARG("mem-track-stack", NULL, "enable memory tracker with stack dumping", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#endif
	GF_DEF_ARG("ltf", NULL, "load test-unit filters (used for for unit tests only)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("sloop", NULL, "loop execution of session, creating a session at each loop, mainly used for testing. If no value is given, loops forever", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("runfor", NULL, "run for the given amount of milliseconds, exit with full session flush", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("runforf", NULL, "run for the given amount of milliseconds, exit with fast session flush", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("runforx", NULL, "run for the given amount of milliseconds and exit with no cleanup", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("runfors", NULL, "run for the given amount of milliseconds and exit with segfault (tests)", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("runforl", NULL, "run for the given amount of milliseconds and wait forever at end (tests)", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT),

	GF_DEF_ARG("stats", NULL, "print stats after execution", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("graph", NULL, "print graph after execution", NULL, NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("qe", NULL, "enable quick exit (no mem cleanup)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("k", NULL, "enable keyboard interaction from command line", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("r", NULL, "enable reporting\n"
			"- r: runtime reporting\n"
			"- r=FA[,FB]: runtime reporting but only print given filters, e.g. `r=mp4mx` for ISOBMFF multiplexer only\n"
			"- r=: only print final report"
			, NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("seps", NULL, "set the default character sets used to separate various arguments\n"
		"- the first char is used to separate argument names\n"
		"- the second char, if present, is used to separate names and values\n"
		"- the third char, if present, is used to separate fragments for PID sources\n"
		"- the fourth char, if present, is used for list separators (__sourceIDs__, __gfreg__, ...)\n"
		"- the fifth char, if present, is used for boolean negation\n"
		"- the sixth char, if present, is used for LINK directives (see [filters help (-h doc)](filters_general))", GF_FS_DEFAULT_SEPS, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),

	GF_DEF_ARG("i", "src", "specify an input file - see [filters help (-h doc)](filters_general)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("o", "dst", "specify an output file - see [filters help (-h doc)](filters_general)", NULL, NULL, GF_ARG_STRING, 0),
	GF_DEF_ARG("ib", NULL, "specify an input file to wrap as GF_FileIO object (testing of GF_FileIO)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("ibx", NULL, "specify an input file to wrap as GF_FileIO object without caching (testing of GF_FileIO)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("ob", NULL, "specify an output file to wrap as GF_FileIO object (testing of GF_FileIO)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("cl", NULL, "force complete mode when no link directive are set - see [filters help (-h doc)](filters_general)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),

#ifdef GPAC_CONFIG_EMSCRIPTEN
	GF_DEF_ARG("step[=FPS[:STEPS]", NULL, "configure step mode in non-blocking session (enabled by default if no worker). Step mode is driven by requestAnimationFrame\n"
		"- if `FPS` is 0, let the browser decide (default if video output is used)\n"
		"- if `FPS` is negative, disable step mode\n"
		"- if `FPS` is positive, forces the time interval to 1/FPS (default FPS=100)\n"
		"- if STEPS is set, runs at most STEPS times before returning to the main thread (default is 100)"
	, NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#else
	GF_DEF_ARG("step", NULL, "test step mode in non-blocking session", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#endif

	GF_DEF_ARG("h", "help,-ha,-hx,-hh", "print help. Use `-help` or `-h` for basic options, `-ha` for advanced options, `-hx` for expert options and `-hh` for all.  \nNote: The `@` character can be used in place of the `*` character. String parameter can be:\n"
			"- empty: print command line options help\n"
			"- doc: print the general filter info\n"
			"- alias: print the gpac alias syntax\n"
			"- log: print the log system help\n"
			"- core: print the supported libgpac core options. Use -ha/-hx/-hh for advanced/expert options\n"
			"- cfg: print the GPAC configuration help\n"
			"- net: print network interfaces\n"
			"- prompt: print the GPAC prompt help when running in interactive mode (see [-k](GPAC) )\n"
			"- modules: print available modules\n"
			"- module NAME: print info and options of module `NAME`\n"
			"- creds: print credential help\n"
			"- filters: print name of all available filters\n"
			"- filters:*: print name of all available filters, including meta filters\n"
			"- codecs: print the supported builtin codecs - use `-hx` to include unmapped codecs (ffmpeg, ...)\n"
			"- formats: print the supported formats (`-ha`: print filter names, `-hx`: include meta filters (ffmpeg,...), `-hh`: print mime types)\n"
			"- protocols: print the supported protocol schemes (`-ha`: print filter names, `-hx`: include meta filters (ffmpeg,...), `-hh`: print all)\n"
			"- props: print the supported builtin PID and packet properties\n"
			"- props PNAME: print the supported builtin PID and packet properties mentioning `PNAME`\n"
			"- colors: print the builtin color names and their values\n"
			"- layouts: print the builtin CICP audio channel layout names and their values\n"
			"- links: print possible connections between each supported filters (use -hx to view src->dst cap bundle detail)\n"
			"- links FNAME: print sources and sinks for filter `FNAME` (either builtin or JS filter)\n"
			"- FNAME: print filter `FNAME` info (multiple FNAME can be given)\n"
			"  - For meta-filters, use `FNAME:INST`, e.g. `ffavin:avfoundation`\n"
			"  - Use `*` to print info on all filters (__big output!__), `*:*` to print info on all filters including meta filter instances (__really big output!__)\n"
			"  - By default only basic filter options and description are shown. Use `-ha` to show advanced options capabilities, `-hx` for expert options, `-hh` for all options and filter capabilities including on filters disabled in this build\n"
			"- FNAME.OPT: print option `OPT` in filter `FNAME`\n"
			"- OPT: look in filter names and options for `OPT` and suggest possible matches if none found. Use `-hx` to look for keyword in all option descriptions\n"
		, NULL, NULL, GF_ARG_STRING, 0),

	GF_DEF_ARG("p", NULL, "use indicated profile for the global GPAC config. If not found, config file is created. If a file path is indicated, this will load profile from that file. Otherwise, this will create a directory of the specified name and store new config there. The following reserved names create a temporary profile (not stored on disk):\n"
		"- 0: full profile\n"
		"- n: null profile disabling shared modules/filters and system paths in config (may break GUI and other filters)\n"
		"Appending `:reload` to the profile name will force recreating a new configuration file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("alias", NULL, "assign a new alias or remove an alias. Can be specified several times. See [alias usage (-h alias)](#using-aliases)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),
 	GF_DEF_ARG("aliasdoc", NULL, "assign documentation for a given alias (optional). Can be specified several times", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED),

 	GF_DEF_ARG("uncache", NULL, "revert all items in GPAC cache directory to their original name and server path", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED),
	GF_DEF_ARG("js", NULL, "specify javascript file to use as controller of filter session", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),

	GF_DEF_ARG("wc", NULL, "write all core options in the config file unless already set", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("we", NULL, "write all file extensions in the config file unless already set (useful to change some default file extensions)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("wf", NULL, "write all filter options in the config file unless already set", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("wfx", NULL, "write all filter options and all meta filter arguments in the config file unless already set (__large config file !__)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("unit-tests", NULL, "enable unit tests of some functions otherwise not covered by gpac test suite", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE),
	GF_DEF_ARG("genmd", NULL, "generate markdown doc", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE),
	GF_DEF_ARG("xopt", NULL, "unrecognized options and filters declaration following this option are ignored - used to pass arguments to GUI", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
	GF_DEF_ARG("creds", NULL, "setup credentials as used by servers", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT),

#ifdef GPAC_CONFIG_IOS
	GF_DEF_ARG("req-gl", NULL, "forces loading SDL - iOS only", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT),
#endif
	{0}
};


void gpac_usage(GF_SysArgMode argmode)
{
	u32 i=0;
	if ((argmode != GF_ARGMODE_ADVANCED) && (argmode != GF_ARGMODE_EXPERT) ) {
		if (gen_doc!=2)
			gf_sys_format_help(helpout, help_flags, "Usage: gpac [options] FILTER [LINK] FILTER [...] \n");

		gf_sys_format_help(helpout, help_flags, "gpac is GPAC's command line tool for setting up and running filter chains.\n\n"
		"__FILTER__: a single filter declaration (e.g., `-i file`, `-o dump`, `inspect`, ...), see %s.\n"
		"__[LINK]__: a link instruction (e.g., `@`, `@2`, `@2#StreamType=Visual`, ...), see %s.\n"
		"__[options]__: one or more option strings, each starting with a `-` character.\n"
		"  - an option using a single `-` indicates an option of gpac (see %s) or of libgpac (see %s)\n"
		"  - an option using `--` indicates a global filter or meta-filter (e.g. FFMPEG) option, e.g. `--block_size=1000` or `--profile=Baseline` (see %s)\n"
		"  \n"
		"Filter declaration order may impact the link resolver which will try linking in declaration order. Most of the time for simple graphs, this has no impact. However, for complex graphs with no link declarations, this can lead to different results.  \n"
		"Options do not require any specific order, and may be present anywhere, including between link statements or filter declarations.  \n"
		"Boolean values do not need any value specified. Other types shall be formatted as `opt=val`, except [-i](), `-src`, [-o](), `-dst` and [-h]() options.\n\n"
		"\n"
		"The session can be interrupted at any time using `ctrl+c`, which can also be used to toggle global reporting.\n"
		"\n"
		"The possible options for gpac are:\n\n",
			(gen_doc==1) ? "[gpac -h doc](filters_general#filter-declaration-filter)" : "`gpac -h doc`",
			(gen_doc==1) ? "[gpac -h doc](filters_general#explicit-links-between-filters-link)" : "`gpac -h doc`",
			(gen_doc==1) ? "[gpac -hx](gpac_general#h)" : "`gpac -hx`",
			(gen_doc==1) ? "[gpac -hx core](core_options)" : "`gpac -hx core`",
			(gen_doc==1) ? "[gpac -h doc](core_config#global-filter-options)" : "`gpac -h doc`",
			(gen_doc==1) ? "[gpac -h doc](core_config#global-filter-options)" : "`gpac -h doc`"
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
		gf_sys_format_help(helpout, help_flags, "\n  \nThe following libgpac core options allow customizing the filter session:\n  \n" );
		gf_sys_print_core_help(helpout, help_flags, argmode, GF_ARG_SUBSYS_FILTERS);
	}

	if (argmode==GF_ARGMODE_BASE) {
		if ( gf_opts_get_key_count("gpac.aliasdoc")) {
			gf_sys_format_help(helpout, help_flags, "\n");
			gpac_alias_help(GF_ARGMODE_BASE);
		}
		gf_sys_format_help(helpout, help_flags, "\nReturn codes are 0 for no error, 1 for error"
#ifdef GPAC_MEMORY_TRACKING
			" and 2 for memory leak detection when -mem-track is used"
#endif
			"\n");

		gf_sys_format_help(helpout, help_flags, "\ngpac - GPAC command line filter engine - version %s\n%s\n", gf_gpac_version(), gf_gpac_copyright_cite() );
	}
}

#ifndef GPAC_DISABLE_DOC
static const char *gpac_config =
{
"# Configuration file\n"
"GPAC uses a configuration file to modify default options of libgpac and filters. This file is called `GPAC.cfg` and is located:\n"
"- on Windows platforms, in `C:\\Users\\FOO\\AppData\\Roaming\\GPAC` or in `C:\\Program Files\\GPAC`.\n"
"- on iOS platforms, in a .gpac folder in the app storage directory.\n"
"- on Android platforms, in `/sdcard/GPAC/` if this directory exists, otherwise in `/data/data/io.gpac.gpac/GPAC`.\n"
"- on other platforms, in a `$HOME/.gpac/`.\n"
"\n"
"Applications in GPAC can also specify a different configuration file through the [-p](GPAC) profile option. "
"EX gpac -p=foo []\n"
"This will load configuration from $HOME/.gpac/foo/GPAC.cfg, creating it if needed.\n"
"The reserved name `0` is used to disable configuration file writing.\n"
"\n"
"The configuration file is structured in sections, each made of one or more keys:\n"
"- section `foo` is declared as `[foo]\\n`\n"
"- key `bar` with value `N` is declared as `bar=N\\n`. The key value `N` is not interpreted and always handled as ASCII text.\n"
"\n"
"By default the configuration file only holds a few system specific options and directories. It is possible to serialize "
"the entire set of options to the configuration file, using [-wc](GPAC) [-wf](GPAC).\n"
"This should be avoided as the resulting configuration file size will be quite large, hence larger memory usage for the applications.\n"
"The options specified in the configuration file may be overridden by the values in __restrict.cfg__ file located in GPAC share system directory (e.g. `/usr/share/gpac` or `C:\\Program Files\\GPAC`), "
"if present; this allows enforcing system-wide configuration values.\n"
"Note: The methods describe in this section apply to any application in GPAC transferring their arguments "
"to libgpac. This is the case for __gpac__ and __MP4Box__.\n"
"\n"
"# Core options\n"
"The options from libgpac core can also be assigned though the config file from section __core__ using "
"option name **without initial dash** as key name.\n"
"EX [core]\n"
"EX threads=2\n"
"Setting this in the config file is equivalent to using `-threads=2`.\n"
"The options specified at prompt overrides the value of the config file.\n"
"# Filter options in configuration\n"
"It is possible to alter the default value of a filter option by modifying the configuration file. Filter __foo__ options are stored in section "
"`[filter@foo]`, using option name and value as key-value pair. Options specified through the configuration file do not take precedence over "
"options specified at prompt or through alias.\n"
"EX [filter@rtpin]\n"
"EX interleave=yes\n"
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
"Meta-filter options can be set in the same way using the syntax `--OPT_NAME=VAL`.\n"
"EX --profile=Baseline -i file.cmp -o dump.264\n"
"This is equivalent to specifying `-o dump.264:profile=Baseline`.\n"
"  \n"
"For both syntaxes, it is possible to specify the filter registry name of the option, using `--FNAME:OPTNAME=VAL` or `--FNAME@OPTNAME=VAL`.\n"
"In this case the option will only be set for filters which are instances of registry FNAME. This is used when several registries use same option names.\n"
"EX --flist@timescale=100 -i plist1 -i plist2 -o live.mpd\n"
"This will set the timescale option on the playlists filters but not on the dasher filter.\n"
};
#endif



void gpac_config_help()
{
#ifndef GPAC_DISABLE_DOC
		gf_sys_format_help(helpout, help_flags, "%s", gf_sys_localized("gpac", "config", gpac_config) );
#else
		gf_sys_format_help(helpout, help_flags, "%s", "GPAC compiled without built-in doc.\n");
#endif

	if (gen_doc) return;
	gf_sys_format_help(helpout, help_flags, "\nCurrent configuration file is `%s`\n\n", gf_opts_get_filename());
}

#ifndef GPAC_DISABLE_DOC
static const char *gpac_credentials =
{
"# User Credentials\n"
"Some servers in GPAC can use user-based and group-based authentication.\n"
"The information is stored by default in the file `users.cfg` located in the GPAC profile directory.\n"
"The file can be overwritten using the [-users](CORE) option.\n"
"\n"
"By default, this file does not exist until at least one user has been configured.\n"
"\n"
"The [-creds]() option allows inspecting or modifying the users and groups information. The syntax for the option value is:\n"
"- `show` or no value: prints the `users.cfg` file\n"
"- `reset`: deletes the `users.cfg` file (i.e. deletes all users and groups)\n"
"- `NAME`: show information of user `NAME`\n"
"- `+NAME`: adds user `NAME`\n"
"- `+NAME:I1=V1[,I2=V2]`: sets info `I1` with value `V1` to user `NAME`. The info name `password` resets password without prompt.\n"
"- `-NAME`: removes user `NAME`\n"
"- `_NAME`: force password change of user `NAME`\n"
"- `@NAME`: show information of group `NAME`\n"
"- `@+NAME[:u1[,u2]]`: adds group `NAME` if not existing and adds specified users to group\n"
"- `@-NAME:u1[,u2]`: removes specified users from group `NAME`\n"
"- `@-NAME`: removes group `NAME`\n"
"\n"
"By default all added users are members of the group `users`.\n"
"Passwords are not stored, only a SHA256 hash is stored.\n"
"\n"
"Servers using authentication rules can use a configuration file instead of a directory name.\n"
"This configuration file is organized in sections, each section name describing a directory.\n"
"EX [somedir]\n"
"EX ru=foo\n"
"EX rg=bar\n"
"\n"
"The following keys are defined per directory, but may be ignored by the server depending on its operation mode:\n"
"- ru: comma-separated list of user names with read access to the directory\n"
"- rg: comma-separated list of group names with read access to the directory\n"
"- wu: comma-separated list of user names with write access to the directory\n"
"- wg: comma-separated list of group names with write access to the directory\n"
"- mcast: comma-separated list of user names with multicast creation rights (RTSP server only)\n"
"- filters: comma-separated list of filter names for which the directory is valid. If not found or `all`, applies to all filters\n"
"\n"
"Rights can be configured on sub-directories by adding sections for the desired directories.\n"
"EX [d1]\n"
"EX rg=bar\n"
"EX [d1/d2]\n"
"EX ru=foo\n"
"With this configuration:\n"
"- the directory `d1` will be readable by all members of group `bar`\n"
"- the directory `d1/d2` will be readable by user `foo` only\n"
"\n"
"Servers in GPAC currently only support the `Basic` HTTP authentication scheme, and should preferably be run over TLS.\n"
};
#endif

void gpac_credentials_help(GF_SysArgMode argmode)
{
#ifndef GPAC_DISABLE_DOC
		gf_sys_format_help(helpout, help_flags, "%s", gf_sys_localized("gpac", "credentials", gpac_credentials) );
#else
		gf_sys_format_help(helpout, help_flags, "%s", "GPAC compiled without built-in doc.\n");
#endif
}

#include <gpac/network.h>
void gpac_load_suggested_filter_args()
{
	u32 i, count, k;
	GF_Config *opts = NULL;
	GF_FilterSession *fsess;
	const char *version, *cfg_path;
	char *all_opts;

	if (gf_opts_get_bool("core", "no-argchk"))
		return;

	cfg_path = gf_opts_get_filename();
	all_opts = gf_url_concatenate(cfg_path, "all_opts.txt");

	opts = gf_cfg_force_new("probe", all_opts);
	gf_free(all_opts);

	version = gf_cfg_get_key(opts, "version", "version");
	if (version && !strcmp(version, gf_gpac_version()) ) {
		gf_cfg_del(opts);
		return;
	}
	gf_cfg_del_section(opts, "allopts");
	gf_cfg_set_key(opts, "version", "version", gf_gpac_version());

	gf_sys_format_help(stderr, 0, "__Refreshing all options registry, this may take some time ... ");

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
			if ((arg->arg_type==GF_PROP_UINT) && arg->min_max_enum && strchr(arg->min_max_enum, '|')) {
				gf_dynstrcat(&argn, arg->min_max_enum, "@");
			}

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


void gpac_suggest_arg(char *aname)
{
	u32 k;
	const GF_GPACArg *args;
	if (!aname) return;
	Bool found=GF_FALSE;
	for (k=0; k<2; k++) {
		u32 i=0;
		if (k==0) args = gpac_args;
		else args = gf_sys_get_options();

		while (args[i].name) {
			const GF_GPACArg *arg = &args[i];
			i++;
			if (gf_sys_word_match(aname, arg->name)) {
				if (!found) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unrecognized option \"%s\", did you mean:\n", aname));
					found = GF_TRUE;
				}
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("\t-%s (see gpac -hx%s)\n", arg->name, k ? " core" : ""));
			}
		}
	}
	//look in alias
	u32 nb_alias = gf_opts_get_key_count("gpac.alias");
	for (k=0; k<nb_alias; k++) {
		const char *key = gf_opts_get_key_name("gpac.alias", k);
		if (gf_sys_word_match(aname, key)) {
			if (!found) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unrecognized option \"%s\", did you mean:\n", aname));
				found = GF_TRUE;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("\t%s (see gpac -h)\n", key));
		}
	}

	if (!found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unrecognized option \"%s\", check usage \"gpac -h\"\n", aname));
	}
}

Bool gpac_suggest_filter(char *fname, Bool is_help, Bool filter_only)
{
	Bool found = GF_FALSE;
	Bool first = GF_FALSE;
	u32 i, count, pass_exact = GF_TRUE;
	if (!fname) return GF_FALSE;

	if (filter_only || !is_help) pass_exact = GF_FALSE;

redo_pass:

	count = gf_fs_filters_registers_count(session);
	if (!pass_exact) {
		for (i=0; i<count; i++) {
			const GF_FilterRegister *freg = gf_fs_get_filter_register(session, i);

			if (gf_sys_word_match(fname, freg->name)) {
				if (!first) {
					first = GF_TRUE;
					if (!found) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
						found = GF_TRUE;
					}
					GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("Closest filter names: \n"));
				}
				GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("- %s\n", freg->name));
			}
		}
	}

	if (is_help) {
		if (!pass_exact) {
			const char *doc_helps[] = {
				"log", "core", "modules", "doc", "alias", "props", "colors", "layouts", "cfg", "prompt", "codecs", "formats", "exts", "protocols", "links", "bin", "filters", "filters:*", "filters:@", "mp4c", "net", NULL
			};
			first = GF_FALSE;
			i=0;
			while (doc_helps[i]) {
				if (gf_sys_word_match(fname, doc_helps[i])) {
					if (!first) {
						first = GF_TRUE;
						if (!found) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
							found = GF_TRUE;
						}
						GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("Closest help commands: \n"));
					}
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("-h %s\n", doc_helps[i]));
				}
				i++;
			}
		}

		if (!filter_only) {
			u32 nb_alias = gf_opts_get_key_count("gpac.alias");
			first = GF_FALSE;
			for (i=0; i<nb_alias; i++) {
				const char *alias = gf_opts_get_key_name("gpac.alias", i);

				if (pass_exact) {
					if (!strcmp(fname, alias+1)) {
						const char *doc = gf_opts_get_key("gpac.aliasdoc", alias);
						if (!doc) doc = "Not documented";
						GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s: %s\n", alias, doc));
						found = GF_TRUE;
						if (is_help) return GF_TRUE;
					}
				}
				else if (gf_sys_word_match(fname, alias)) {
					if (!first) {
						first = GF_TRUE;
						if (!found) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
							found = GF_TRUE;
						}
						GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("Closest alias: \n"));
					}
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s\n", alias));
				}
			}
			//check in libgpac core opts
			first = GF_FALSE;
			i=0;
			const GF_GPACArg *core_opts = gf_sys_get_options();
			while (core_opts && core_opts[i].name) {
				const GF_GPACArg *arg = &core_opts[i];
				i++;

				if (gf_sys_word_match(fname, arg->name)) {
					if (!first) {
						first = GF_TRUE;
						if (!found) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
							found = GF_TRUE;
						}
						GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("\nClosest core option: \n"));
					}
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("-%s: %s\n", arg->name, arg->description));
				}
			}

			//check filters
			first = GF_FALSE;
			for (i=0; i<count; i++) {
				u32 j=0;
				const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);

				while (reg->args) {
					const GF_FilterArgs *arg = &reg->args[j];
					if (!arg || !arg->arg_name) break;
					j++;

					if (pass_exact) {
						if (!strcmp(fname, arg->arg_name)) {
							gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s.%s %s\n", reg->name, arg->arg_name, arg->arg_desc);
							found = GF_TRUE;
						}
						continue;
					}

					if (!gf_sys_word_match(fname, arg->arg_name)) continue;

					if (!first) {
						first = GF_TRUE;
						if (!found) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
							found = GF_TRUE;
						}
						GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("\nClosest matching filter options:\n"));
					}
					gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s.%s \n", reg->name, arg->arg_name);
				}
			}


			first = GF_FALSE;
			i=0;
			const GF_BuiltInProperty *prop_info;
			while ((prop_info = gf_props_get_description(i))) {
				i++;
				if (!prop_info->name) continue;
				
				if (gf_sys_word_match(fname, prop_info->name)) {
					if (!first) {
						first = GF_TRUE;
						if (!found) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No such filter %s\n", fname));
							found = GF_TRUE;
						}
						GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("\nClosest property name: \n"));
					}
#ifndef GPAC_DISABLE_DOC
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s: %s\n", prop_info->name, prop_info->description));
#else
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s\n", prop_info->name));
#endif
				}
			}

		}
	}
	if (!found) {
		if (pass_exact) {
			pass_exact = GF_FALSE;
			goto redo_pass;
		}

		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("No filter%swith similar name found, see gpac -h filters%s\n",
			is_help ? ", option or help command " : " ",
			is_help ? ", gpac -h or search all help using gpac -hx" : ""
		));
	}
	return GF_FALSE;
}

static void gpac_suggest_filter_arg(GF_Config *opts, const char *argname, u32 atype)
{
	char *keyname;
	const char *keyval;
	u32 i, count, len, nb_filters, j;
	Bool f_found = GF_FALSE;
	char szSep[2];

	if (gf_opts_get_bool("core", "no-argchk"))
		return;

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
		char *enum_vals=NULL;
		const char *arg = gf_cfg_get_key_name(opts, "allopts", i);
		const char *aval = gf_cfg_get_key(opts, "allopts", arg);
		if ((arg[1]=='+') && (atype!=2)) continue;
		if ((arg[1]=='-') && (atype==2)) continue;

		arg += 2;
		char *sep = strchr(arg, '@');
		if (sep) {
			sep[0] = 0;

			if (!strcmp(arg, argname)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Argument \"%s%s\" was set but not used by any filter\n",
				(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
				sep[0] = '@';
				return;
			}

			enum_vals = strstr(sep+1, argname);
			if (enum_vals) {
				if (!f_found) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unknown argument \"%s%s\" set but not used by any filter - possible matches\n",
						(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
					f_found = GF_TRUE;
				}
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("- %s (%s) in filters %s\n", arg, sep+1, aval));
				sep[0] = '@';
				continue;
			}
		}
		u32 alen = (u32) strlen(arg);
		if (alen>2*len) {
			if (sep) sep[0] = '@';
			continue;
		}

		for (j=0; j<nb_filters; j++) {
			GF_FilterStats stats;
			stats.reg_name = NULL;
			gf_fs_get_filter_stats(session, j, &stats);

			if (stats.reg_name && strstr(aval, stats.reg_name)) {
				ffound = GF_TRUE;
				break;
			}
		}
		if (!ffound) {
			if (sep) sep[0] = '@';
			continue;
		}
		if (gf_sys_word_match(argname, arg)) {
			if (!f_found) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unknown argument \"%s%s\" set but not used by any filter - possible matches\n",
					(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
				f_found = GF_TRUE;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("- %s in filters %s\n", arg, aval));
		}
		if (sep) sep[0] = '@';
	}
	if (!f_found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Unknown argument \"%s%s\" set but not used by any filter - no matching argument found\n",
			(atype==2) ? "-+" : (atype ? "--" : szSep), argname));
	}
}

void gpac_check_session_args()
{
	GF_Config *opts = NULL;
	u32 idx = 0;
	const char *argname, *meta_name, *meta_opt;
	u32 argtype;

	while (1) {
		if (gf_fs_enum_unmapped_options(session, &idx, &argname, &argtype, &meta_name, &meta_opt)==GF_FALSE)
			break;

		if (meta_name && (argtype!=1)) {
			if (meta_opt) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Filter %s:%s value \"%s\" set but not used\n", meta_name, meta_opt, argname));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("Filter %s:%s option set but not used\n", meta_name, argname));
			}
			continue;
		}

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
		else szVal = gf_props_dump_val(&cap->val, szDump, GF_PROP_DUMP_DATA_NONE, NULL);

		gf_sys_format_help(helpout, help_flags, " %s=\"%s\"", szName,  szVal);
		if (cap->priority) gf_sys_format_help(helpout, help_flags, ", priority=%d", cap->priority);
		gf_sys_format_help(helpout, help_flags, "\n");
	}
}

static void print_filter_arg(const GF_FilterArgs *a, u32 gen_doc)
{
	Bool is_enum = GF_FALSE;

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
		if (!strcmp(a->arg_default_val, "2147483647"))
			gf_sys_format_help(helpout, help_flags, ", default: __+I__");
		else if (!strcmp(a->arg_default_val, "-2147483648"))
			gf_sys_format_help(helpout, help_flags, ", default: __-I__");
		else
			gf_sys_format_help(helpout, help_flags, ", default: __%s__", a->arg_default_val);
	} else {
//		gf_sys_format_help(helpout, help_flags, ", no default");
	}
	if (a->min_max_enum && !is_enum) {
		if (!strcmp(a->min_max_enum, "-2147483648-I"))
			gf_sys_format_help(helpout, help_flags, ", %s: -I-I", /*strchr(a->min_max_enum, '|') ? "Enum" : */"minmax");
		else
			gf_sys_format_help(helpout, help_flags, ", %s: %s", /*strchr(a->min_max_enum, '|') ? "Enum" : */"minmax", a->min_max_enum);
	}
	if (a->flags & GF_FS_ARG_UPDATE) gf_sys_format_help(helpout, help_flags, ", updatable");
//		if (a->flags & GF_FS_ARG_META) gf_sys_format_help(helpout, help_flags, ", meta");

	if (is_enum && a->arg_desc && !strchr(a->arg_desc, '\n')) {
		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_OPT_DESC, "): %s (%s)\n", a->arg_desc, a->min_max_enum);
	} else {
		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_OPT_DESC, "): %s\n", a->arg_desc);
	}

	//check syntax
	if (gen_doc) {
		GF_GPACArg _a;
		memset(&_a, 0, sizeof(GF_GPACArg));
		_a.name = a->arg_name;
#ifndef GPAC_DISABLE_DOC
		_a.description = a->arg_desc;
#endif
		_a.flags = GF_ARG_HINT_HIDE;
		gf_sys_print_arg(NULL, 0, &_a, "");
	}

	if (a->min_max_enum && strchr(a->min_max_enum, '|'))
		gf_sys_format_help(helpout, help_flags, "\n");
	else if (!a->min_max_enum && a->arg_desc && strstr(a->arg_desc, "\n- "))
		gf_sys_format_help(helpout, help_flags, "\n");
}

static void print_filter_single_opt(const GF_FilterRegister *reg, char *optname, GF_Filter *filter_inst)
{
	u32 idx=0;
	Bool found = GF_FALSE;
	const GF_FilterArgs *args = NULL;
	if (filter_inst)
		args = gf_filter_get_args(filter_inst);
	else if (reg)
		args = reg->args;

	if (!args) return;

	while (1) {
		const GF_FilterArgs *a = & args[idx];
		if (!a || !a->arg_name) break;
		idx++;
		if (strcmp(a->arg_name, optname)) continue;

		print_filter_arg(a, 0);
		found = GF_TRUE;
		break;
	}
	if (found) return;

	idx = 0;
	while (1) {
		const GF_FilterArgs *a = & args[idx];
		if (!a || !a->arg_name) break;
		idx++;
		if (gf_sys_word_match(optname, a->arg_name)
			|| (a->arg_desc && strstr(a->arg_desc, optname))
		) {
			if (!found) {
				found = GF_TRUE;
				fprintf(stderr, "No such option %s for filter %s - did you mean:\n", optname, filter_inst ? gf_filter_get_name(filter_inst) : reg->name);
			}
			fprintf(stderr, " - %s: %s\n", a->arg_name, a->arg_desc);
		}
	}
	if (found) return;

	fprintf(stderr, "No such option %s for filter %s\n", optname, filter_inst ? gf_filter_get_name(filter_inst) : reg->name);
}

static void print_filter(const GF_FilterRegister *reg, GF_SysArgMode argmode, GF_Filter *filter_inst, char *inst_name)
{
	u32 idx=0;
	const GF_FilterArgs *args = NULL;
	const char *reg_name, *reg_desc=NULL;
#ifndef GPAC_DISABLE_DOC
	const char *reg_help;
#endif
	Bool jsmod_help = (argmode>GF_ARGMODE_ALL) ? GF_TRUE : GF_FALSE;

	if (filter_inst) {
		reg_name = inst_name;
		reg_desc = gf_filter_get_description(filter_inst);
#ifndef GPAC_DISABLE_DOC
		reg_help = gf_filter_get_help(filter_inst);
#endif
	} else if (reg) {
		reg_name = reg->name;
#ifndef GPAC_DISABLE_DOC
		reg_desc = reg->description;
		reg_help = reg->help;
#endif
	} else {
		return;
	}

	//happens on some meta filter or JS filters
	if (!reg_desc) {
		reg_desc = "No description available";
	}

	if (gen_doc==1) {
		char szName[1024];
		sprintf(szName, "%s.md", reg_name);

		gf_fclose(helpout);
		helpout = gf_fopen(szName, "w");
		fprintf(helpout, "%s", auto_gen_md_warning);

		if (!sidebar_md) {
			char *sbbuf = NULL;
			if (gf_file_exists("_Sidebar.md")) {
				char szLine[1024];
				u32 end_pos=0;
				sidebar_md = gf_fopen("_Sidebar.md", "r");
				gf_fseek(sidebar_md, 0, SEEK_SET);
				while (!feof(sidebar_md)) {
					char *read = gf_fgets(szLine, 1024, sidebar_md);
					if (!read) break;
					if (!strncmp(szLine, "**Filters Help**", 16)) {
						end_pos = (u32) gf_ftell(sidebar_md);
						break;
					}
				}
				if (!end_pos) end_pos = (u32) gf_ftell(sidebar_md);
				if (end_pos) {
					sbbuf = gf_malloc(end_pos+1);
					gf_fseek(sidebar_md, 0, SEEK_SET);
					end_pos = (u32) gf_fread(sbbuf, end_pos, sidebar_md);
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
		fprintf(sidebar_md, "[[%s (%s)|%s]]  \n", reg_desc, reg_name, reg_name);
#ifndef GPAC_DISABLE_DOC

		if (!reg_help) {
			fprintf(stderr, "filter %s without help, forbidden\n", reg_name);
			exit(1);
		}
#endif

#ifndef GPAC_DISABLE_DOC
		gf_sys_format_help(helpout, help_flags, "# %s\n", reg_desc);
#endif
		gf_sys_format_help(helpout, help_flags, "Register name used to load filter: **%s**\n", reg_name);
		if (filter_inst) {
			gf_sys_format_help(helpout, help_flags, "This is a JavaScript filter, not checked during graph resolution and needs explicit loading.\n");
		} else {
			if (reg->flags & GF_FS_REG_EXPLICIT_ONLY) {
				gf_sys_format_help(helpout, help_flags, "This filter is not checked during graph resolution and needs explicit loading.\n");
			} else {
				gf_sys_format_help(helpout, help_flags, "This filter may be automatically loaded during graph resolution.\n");
			}
			if (reg->flags & GF_FS_REG_REQUIRES_RESOLVER) {
				gf_sys_format_help(helpout, help_flags, "This filter requires the graph resolver to be activated.\n");
			}
			if (reg->flags & GF_FS_REG_ALLOW_CYCLIC) {
				gf_sys_format_help(helpout, help_flags, "Filters of this class can connect to each-other.\n");
			}
		}
	} else if (!jsmod_help) {
		gf_sys_format_help(helpout, help_flags, "# %s\n", reg_name);
		gf_sys_format_help(helpout, help_flags, "Description: %s\n", reg_desc );

		if (filter_inst) {
			const char *version = gf_filter_get_version(filter_inst);
			if (version)
				gf_sys_format_help(helpout, help_flags, "Version: %s\n", version );
		} else {
			if (reg->version) {
				if (!strncmp(reg->version, "! ", 2)) {
					if (!gen_doc)
						gf_sys_format_help(helpout, help_flags, "%s\n", reg->version+2);
				} else {
					gf_sys_format_help(helpout, help_flags, "Version: %s\n", reg->version);
				}
			}
		}
	}

	if (filter_inst) {
		const char *str;
		if (!jsmod_help) {
			str = gf_filter_get_author(filter_inst);
			if (str)
				gf_sys_format_help(helpout, help_flags, "%s: %s\n", (str[0]=='-') ? "Configuration" : "Author", str );
		}
		str = gf_filter_get_help(filter_inst);
		if (str)
			gf_sys_format_help(helpout, help_flags, "\n%s\n\n", str);
	} else if (reg) {
#ifndef GPAC_DISABLE_DOC
		if (reg->author) {
			if (reg->author[0]=='-') {
				if (! (help_flags & (GF_PRINTARG_MD|GF_PRINTARG_MAN))) {
					gf_sys_format_help(helpout, help_flags, "Configuration: %s\n", reg->author);
				}
			} else {
				gf_sys_format_help(helpout, help_flags, "Author: %s\n", reg->author);
			}
		}
		if (reg->help) {
			u32 hf = help_flags;
			if (gen_doc==1) hf |= GF_PRINTARG_ESCAPE_XML;
			gf_sys_format_help(helpout, hf, "\n%s\n\n", reg->help);
		}
#else
		gf_sys_format_help(helpout, help_flags, "GPAC compiled without built-in doc\n");
#endif
	}

	if (reg && (argmode==GF_ARGMODE_EXPERT)) {
		if (reg->max_extra_pids==(u32) -1) gf_sys_format_help(helpout, help_flags, "Max Input PIDs: any\n");
		else gf_sys_format_help(helpout, help_flags, "Max Input PIDs: %d\n", 1 + reg->max_extra_pids);

		gf_sys_format_help(helpout, help_flags, "Flags:");
		if (reg->flags & GF_FS_REG_EXPLICIT_ONLY) gf_sys_format_help(helpout, help_flags, " ExplicitOnly");
		if (reg->flags & GF_FS_REG_MAIN_THREAD) gf_sys_format_help(helpout, help_flags, " MainThread");
		if (reg->flags & GF_FS_REG_CONFIGURE_MAIN_THREAD) gf_sys_format_help(helpout, help_flags, " ConfigureMainThread");
		if (reg->flags & GF_FS_REG_HIDE_WEIGHT) gf_sys_format_help(helpout, help_flags, " HideWeight");
		if (reg->flags & GF_FS_REG_REQUIRES_RESOLVER) gf_sys_format_help(helpout, help_flags, " RequireResolver");
		if (reg->flags & GF_FS_REG_ALLOW_CYCLIC) gf_sys_format_help(helpout, help_flags, " CyclicAllowed");
		if (reg->probe_url) gf_sys_format_help(helpout, help_flags, " URLMimeProber");
		if (reg->probe_data) gf_sys_format_help(helpout, help_flags, " DataProber");
		if (reg->reconfigure_output) gf_sys_format_help(helpout, help_flags, " ReconfigurableOutput");

		gf_sys_format_help(helpout, help_flags, "\nPriority %d", reg->priority);

		gf_sys_format_help(helpout, help_flags, "\n");
	}

	if (filter_inst) args = gf_filter_get_args(filter_inst);
	else args = reg->args;

	u32 nb_opts=0;
	idx=0;
	while (args) {
		const GF_FilterArgs *a = & args[idx];
		if (!a || !a->arg_name) break;
		idx++;
		if (a->flags & GF_FS_ARG_HINT_HIDE) continue;
		nb_opts++;
	}
	if (!nb_opts)
		args = NULL;

	if (args && !jsmod_help) {
		idx=0;
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
			const GF_FilterArgs *a = & args[idx];
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

			print_filter_arg(a, gen_doc);

			if (!gen_doc) continue;

#ifdef CHECK_DOC
			if (a->flags & GF_FS_ARG_META) continue;

			u8 achar;
			char szArg[100];
			sprintf(szArg, " %s ", a->arg_name);
#ifndef GPAC_DISABLE_DOC
			u32 j=0;
			char *quoted = reg_help ? strstr(reg_help, szArg) : NULL;
			if (quoted) {
				fprintf(stderr, "\nWARNING: filter %s bad help, uses arg %s without link: \"... %s\"\n", reg_name, a->arg_name, quoted);
				exit(1);
			}
			while (1) {
				const GF_FilterArgs *anarg = & args[j];
				if (!anarg || !anarg->arg_name) break;
				j++;
				if (a == anarg) continue;
				if (reg_help && strstr(reg_help, szArg)) {
					fprintf(stderr, "\nWARNING: filter %s bad description for argument %s, uses arg %s without link\n", reg_name, anarg->arg_name, a->arg_name);
					exit(1);
				}
			}
#endif

			if (a->min_max_enum) {
				//check format
                if ((a->arg_type!=GF_PROP_UINT_LIST) && !(a->flags&GF_FS_ARG_META) && strchr(a->min_max_enum, '|') && (!a->arg_default_val || strcmp(a->arg_default_val, "-1")) ) {
					const char *a_val = a->min_max_enum;
					while (a_val[0] == '|') a_val++;
					if (strstr(a->arg_desc, "see filter "))
						a_val = NULL;
					while (a_val) {
						char szName[100];
						const char *a_sep = strchr(a_val, '|');
						u32 len = a_sep ? (u32)(a_sep - a_val) : (u32)strlen(a_val);
						strcpy(szName, "- ");
						strncat(szName, a_val, MIN(sizeof(szName)-3,len));
						szName[2+len]=0;
						strcat(szName, ": ");

						if (!strstr(a->arg_desc, szName)) {
							fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, missing list bullet \"%s\"\n", reg_name, a->arg_name, szName);
							exit(1);
						}
						if (!a_sep) break;
						a_val = a_sep+1;
					}
					if (a_val && !strstr(a->arg_desc, "- ")) {
                    	fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, missing list bullet \"- \"\n", reg_name, a->arg_name);
                    	exit(1);
					}
					if (a_val && strstr(a->arg_desc, ":\n")) {
                    	fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should not use \":\\n\"\n", reg_name, a->arg_name);
                    	exit(1);
					}
                } else if (!(a->flags&GF_FS_ARG_META) && strchr(a->arg_desc, '\n')) {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should not contain \"\\n\"\n", reg_name, a->arg_name);
					exit(1);
				}
			}
            if (!(a->flags&GF_FS_ARG_META)) {
				char *sep;

				achar = a->arg_desc[strlen(a->arg_desc)-1];
				if (achar == '\n') {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should not end with \"\\n\"\n", reg_name, a->arg_name);
					exit(1);
				}

				achar = a->arg_desc[0];
				if ((achar >= 'A') && (achar <= 'Z')) {
					achar = a->arg_desc[1];
					if ((achar < 'A') || (achar > 'Z')) {
						fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, should start with lowercase\n", reg_name, a->arg_name);
						exit(1);
					}
				}
				if (a->arg_desc[0] == ' ') {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, first character should not be space\n", reg_name, a->arg_name);
					exit(1);
				}
				sep = strchr(a->arg_desc+1, ' ');
				if (sep) sep--;
				if (sep && (sep[0] == 's') && (sep[-1] != 's')) {
					fprintf(stderr, "\nWARNING: filter %s bad description format for arg %s, first word should be infinitive\n", reg_name, a->arg_name);
					exit(1);
				}
			}
#endif
		}
	} else if (!args) {
		gf_sys_format_help(helpout, help_flags, "No options\n");
	}

	if (!gen_doc && (argmode==GF_ARGMODE_ALL)) {
		if (filter_inst) {
			u32 nb_caps = 0;
			const GF_FilterCapability *caps = gf_filter_get_caps(filter_inst, &nb_caps);
			dump_caps(nb_caps, caps);
		} else if (reg->nb_caps) {
			dump_caps(reg->nb_caps, reg->caps);
		}
	}
	gf_sys_format_help(helpout, help_flags, "\n");
}

static Bool strstr_nocase(const char *text, const char *subtext, u32 subtext_len)
{
	if (!*text || !subtext || !subtext_len)
		return GF_FALSE;

	while (*text) {
		if (tolower(*text) == *subtext) {
			if (!strnicmp(text, subtext, subtext_len))
				return GF_TRUE;

		}
		text++;
	}
	return GF_FALSE;
}


struct __jsenum_info
{
	GF_FilterSession *session;
	GF_SysArgMode argmode;
	Bool print_filter_info;
	const char *path;
	char *js_dir;
};

static Bool jsinfo_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	struct __jsenum_info *jsi = (struct __jsenum_info *)cbck;
	GF_Filter *f;
	if (jsi->js_dir && strcmp(item_name, "init.js")) {
		return GF_FALSE;
	}

	f = gf_fs_load_filter(jsi->session, item_path, NULL);
	if (f) {
		char szPath[GF_MAX_PATH];
		char *ext;
		if (jsi->js_dir) {
			strcpy(szPath, jsi->js_dir);
		} else {
			strcpy(szPath, item_name);
		}
		ext = gf_file_ext_start(szPath);
		if (ext) ext[0] = 0;

		if (jsi->print_filter_info || gen_doc)
			print_filter(NULL, jsi->argmode, f, szPath);
		else
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s: %s\n", szPath, gf_filter_get_description(f));
	}
	return GF_FALSE;
}
static Bool jsinfo_dir_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	char szPath[GF_MAX_PATH];
	struct __jsenum_info *jsi = (struct __jsenum_info *)cbck;
	jsi->js_dir = item_name;

	strcpy(szPath, jsi->path);
	strcat(szPath, item_name);
	strcat(szPath, "/");
	gf_enum_directory(szPath, GF_FALSE, jsinfo_enum, jsi, ".js");
	jsi->js_dir = NULL;
	return GF_FALSE;
}


Bool print_filters(int argc, char **argv, GF_SysArgMode argmode)
{
	Bool found = GF_FALSE;
	Bool print_all = GF_FALSE;
	char *fname = NULL;
	char *l_fname = NULL;
	u32 lf_len = 0;
	u32 i, count = gf_fs_filters_registers_count(session);

	if (!gen_doc && list_filters) gf_sys_format_help(helpout, help_flags, "Listing %d supported filters%s:\n", count, (list_filters==2) ? " including meta-filters" : "");

	if (print_filter_info != 1) {
		for (i=0; i<count; i++) {
			const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);
			if (gen_doc) {
				print_filter(reg, argmode, NULL, NULL);
			} else if (print_filter_info) {
				print_filter(reg, argmode, NULL, NULL);
			} else {
#ifndef GPAC_DISABLE_DOC
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s: %s\n", reg->name, reg->description);
#else
				gf_sys_format_help(helpout, help_flags, "%s (compiled without built-in doc)\n", reg->name);
#endif
			}
		}
		found = GF_TRUE;
	}
	//print a specific filter info, browse args
	else {
		u32 k;
		//all good to go, load filters
		for (k=1; k<(u32) argc; k++) {
			char *arg = argv[k];
			char *sepe;
			Bool found_freg = GF_FALSE;
			char *optname = NULL;
			if (arg[0]=='-') continue;

			sepe = gf_file_basename(arg);
			if (sepe) sepe = strchr(sepe, '.');
			if (sepe) {
				if (!strncmp(sepe, ".js.", 4)) sepe = strchr(sepe+1, '.');
				else if (!strcmp(sepe, ".js")) sepe = NULL;
				if (sepe) {
					sepe[0] = 0;
					optname = sepe+1;
				}
			}
			fname = arg;
			for (i=0; i<count; i++) {
				const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);
				//exact match
				if (!strcmp(arg, reg->name) ) {
					if (optname)
						print_filter_single_opt(reg, optname, NULL);
					else
						print_filter(reg, argmode, NULL, NULL);
					found_freg = GF_TRUE;
				}
				//search for name:*, also accept *:*
				else {
					char *sepo = strchr(arg, ':');

					if (!strcmp(arg, "*:*") || !strcmp(arg, "@:@")
						|| (!sepo && (!strcmp(arg, "*") || !strcmp(arg, "@")) )
						|| (sepo && (!strcmp(sepo, ":*") || !strcmp(sepo, ":@"))  && !strncmp(reg->name, arg, 1+sepo - arg) )
					) {
						if (optname)
							print_filter_single_opt(reg, optname, NULL);
						else
							print_filter(reg, argmode, NULL, NULL);
						found_freg = GF_TRUE;
						if (!strcmp(arg, "*")) print_all = GF_TRUE;
					}
				}
			}
			if (found_freg) {
				found = GF_TRUE;
			} else /*if (!strchr(arg, ':')) */ {
				GF_SysArgMode _argmode = argmode;
				char *js_opt = strchr(arg, ':');
				if (js_opt) {
					js_opt[0] = 0;
					gf_opts_set_key("temp", "gpac-js-help", js_opt+1);
					_argmode = GF_ARGMODE_ALL+1;
				}
				//try to load the filter (JS)
				GF_Filter *f = gf_fs_load_filter(session, arg, NULL);
				const GF_FilterRegister *reg = f ? gf_filter_get_register(f) : NULL;
				if (!reg || !(reg->flags & GF_FS_REG_SCRIPT))
					f = NULL;

				if (f) {
					char *ext;
					char szPath[GF_MAX_PATH];
					strcpy(szPath, gf_file_basename(arg) );
					ext = gf_file_ext_start(szPath);
					if (ext) ext[0] = 0;
					if (optname) {
						print_filter_single_opt(NULL, optname, f);
					} else {
						print_filter(NULL, _argmode, f, szPath);
					}
					found = GF_TRUE;
				}
				if (js_opt) {
					js_opt[0] = ':';
					gf_opts_set_key("temp", "gpac-js-help", NULL);
				}
			}
			if (sepe) sepe[0] = '.';
		}
	}

	if (print_all || !print_filter_info || gen_doc) {
		const char *js_dirs = gf_opts_get_key("core", "js-dirs");
		char szPath[GF_MAX_PATH];
		struct __jsenum_info jsi;
		memset(&jsi, 0, sizeof(struct __jsenum_info));
		jsi.argmode = argmode;
		jsi.session = session;
		jsi.print_filter_info = print_filter_info;

		gf_log_set_tools_levels("console@error", GF_FALSE);

		if (gf_opts_default_shared_directory(szPath)) {
			strcat(szPath, "/scripts/jsf/");
			jsi.path = szPath;
			gf_enum_directory(szPath, GF_FALSE, jsinfo_enum, &jsi, ".js");
			gf_enum_directory(szPath, GF_TRUE, jsinfo_dir_enum, &jsi, NULL);
		}
		while (js_dirs && js_dirs[0]) {
			char *sep = strchr(js_dirs, ',');
			if (sep) {
				u32 cplen = (u32) (sep-js_dirs);
				if (cplen>=GF_MAX_PATH) cplen = GF_MAX_PATH-1;
				strncpy(szPath, js_dirs, cplen);
				szPath[cplen]=0;
				js_dirs = sep+1;
			} else {
				strcpy(szPath, js_dirs);
			}
			//pre 1.1, $GJS was inserted by default
			if (strcmp(szPath, "$GJS")) {
				u32 len = (u32) strlen(szPath);
				if (len && (szPath[len-1]!='/') && (szPath[len-1]!='\\'))
					strcat(szPath, "/");
				gf_enum_directory(szPath, GF_FALSE, jsinfo_enum, &jsi, ".js");
			}
			if (!sep) break;
		}
		return GF_TRUE;
	}


	if (found) return GF_TRUE;
	if (!fname) return GF_FALSE;
	if (!print_filter_info) return GF_FALSE;
	if (gen_doc) return GF_FALSE;

	if (argmode==GF_ARGMODE_EXPERT) {
		l_fname = gf_strdup(fname);
		strlwr(l_fname);
		lf_len = (u32) strlen(l_fname);
	}

	for (i=0; i<count; i++) {
		u32 j=0;
		const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);

		while (reg->args) {
			const GF_FilterArgs *arg = &reg->args[j];
			if (!arg || !arg->arg_name) break;
			j++;
			if (argmode==GF_ARGMODE_EXPERT) {
				if (!arg->arg_desc || !strstr_nocase(arg->arg_desc, l_fname, lf_len)) continue;
				if (!found) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("\"%s\" is mentioned in the following filters options:\n", fname));
					found = GF_TRUE;
				}
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s.%s \n", reg->name, arg->arg_name);
#if 0

			} else {
				if (strcmp(arg->arg_name, fname)) continue;
				if (!found) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_APP, ("No such filter \"%s\" but found filters with matching options:\n", fname));
					found = GF_TRUE;
				}
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s.%s: %s\n", reg->name, arg->arg_name, arg->arg_desc);
#endif
			}
		}
	}
	if (l_fname) gf_free(l_fname);


	if (found) return GF_TRUE;

	if (gpac_suggest_filter(fname, GF_TRUE, GF_FALSE))
		return GF_TRUE;
	return GF_FALSE;
}

void dump_all_props(char *pname)
{
	u32 i=0;
#ifndef GPAC_DISABLE_DOC
	u32 pname_len = pname ? (u32) strlen(pname) : 0;
#endif
	const GF_BuiltInProperty *prop_info;

	if (gen_doc==1) {
		gf_sys_format_help(helpout, help_flags, "## Built-in property types\n"
		"  \n");

		gf_sys_format_help(helpout, help_flags, "Name | Description  \n");
		gf_sys_format_help(helpout, help_flags, "--- | ---  \n");
		for (i=GF_PROP_FORBIDDEN+1; i<GF_PROP_LAST_DEFINED; i++) {
			if (i==GF_PROP_STRING_NO_COPY) continue;
			if (i==GF_PROP_DATA_NO_COPY) continue;
			if (i==GF_PROP_STRING_LIST_COPY) continue;
			if ((i>=GF_PROP_LAST_NON_ENUM) && (i<GF_PROP_FIRST_ENUM)) continue;
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s  \n", gf_props_get_type_name(i), gf_props_get_type_desc(i) );
		}

		gf_sys_format_help(helpout, help_flags, "## Built-in properties for PIDs and packets, pixel formats and audio formats\n"
		"  \n"
		"Flags can be:\n"
		"- D: droppable property, see [GSF multiplexer](gsfmx) filter help for more info\n"
		"- P: property applying to packet\n"
		"  \n");
		gf_sys_format_help(helpout, help_flags, "Name | type | Flags | Description | 4CC  \n");
		gf_sys_format_help(helpout, help_flags, "--- | --- | --- | --- | ---  \n");
	} else if (pname) {
		gf_sys_format_help(helpout, help_flags, "Built-in properties matching `%s` for PIDs and packets listed as `Name (4CC type FLAGS): description`\n`FLAGS` can be D (droppable - see GSF multiplexer filter help), P (packet property)\n", pname);
	} else {
		gf_sys_format_help(helpout, help_flags, "Built-in property types\n");
		for (i=GF_PROP_FORBIDDEN+1; i<GF_PROP_LAST_DEFINED; i++) {
			if (i==GF_PROP_STRING_NO_COPY) continue;
			if (i==GF_PROP_DATA_NO_COPY) continue;
			if (i==GF_PROP_STRING_LIST_COPY) continue;
			if ((i>=GF_PROP_LAST_NON_ENUM) && (i<GF_PROP_FIRST_ENUM)) continue;

			if (gen_doc==2) {
				gf_sys_format_help(helpout, help_flags, ".TP\n.B %s\n%s\n", gf_props_get_type_name(i), gf_props_get_type_desc(i));
			} else {
				gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s : %s\n", gf_props_get_type_name(i), gf_props_get_type_desc(i) );
			}
		}
		gf_sys_format_help(helpout, help_flags, "\n\n");

		gf_sys_format_help(helpout, help_flags, "Built-in properties for PIDs and packets listed as `Name (4CC type FLAGS): description`\n`FLAGS` can be D (droppable - see GSF multiplexer filter help), P (packet property)\n");
	}
	i=0;
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
#ifndef GPAC_DISABLE_DOC
			 	prop_info->description,
#else
				"not available in build",
#endif
			 	gf_4cc_to_str(prop_info->type)
			);
		} else if (gen_doc==2) {
			gf_sys_format_help(helpout, help_flags, ".TP\n.B %s (%s,%s,%s%s)\n%s\n", prop_info->name, gf_4cc_to_str(prop_info->type), gf_props_get_type_name(prop_info->data_type),
			 	(prop_info->flags & GF_PROP_FLAG_GSF_REM) ? "D" : " ",
			 	(prop_info->flags & GF_PROP_FLAG_PCK) ? "P" : " ",
#ifndef GPAC_DISABLE_DOC
				prop_info->description
#else
				"not available in build"
#endif
			);
		} else {
			u32 len;
			const char *ptype;

			if (pname) {
				if (gf_sys_word_match(prop_info->name, pname)) {}
#ifndef GPAC_DISABLE_DOC
				else if (gf_strnistr(prop_info->description, pname, pname_len)) {}
#endif
				else continue;
			}
			szFlags[0]=0;
			if (prop_info->flags & GF_PROP_FLAG_GSF_REM) strcat(szFlags, "D");
			if (prop_info->flags & GF_PROP_FLAG_PCK) strcat(szFlags, "P");

			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s", prop_info->name);
			len = (u32) strlen(prop_info->name);
			while (len<16) {
				gf_sys_format_help(helpout, help_flags, " ");
				len++;
			}
			ptype = gf_props_get_type_name(prop_info->data_type);
			gf_sys_format_help(helpout, help_flags, " (%s %s %s):", gf_4cc_to_str(prop_info->type), ptype, szFlags);
			len += (u32) strlen(ptype) + (u32) strlen(szFlags);
			while (len<24) {
				gf_sys_format_help(helpout, help_flags, " ");
				len++;
			}

			gf_sys_format_help(helpout, help_flags, "%s",
#ifndef GPAC_DISABLE_DOC
				prop_info->description
#else
				"not available in build"
#endif
			);
			if (prop_info->data_type==GF_PROP_PIXFMT) {
				gf_sys_format_help(helpout, help_flags, "\n\tNames: %s\n\tFile extensions: %s", gf_pixel_fmt_all_names(), gf_pixel_fmt_all_shortnames() );
			} else if (prop_info->data_type==GF_PROP_PCMFMT) {
				gf_sys_format_help(helpout, help_flags, "\n\tNames: %s\n\tFile extensions: %s", gf_audio_fmt_all_names(), gf_audio_fmt_all_shortnames() );
			} else if (gf_props_type_is_enum(prop_info->data_type)) {
				gf_sys_format_help(helpout, help_flags, "\n\tNames: %s\n\t", gf_props_enum_all_names(prop_info->data_type) );
			}
			gf_sys_format_help(helpout, help_flags, "\n");
		}
	}
	if (gen_doc==1) {
		u32 idx=0;
		u32 cicp;
		u64 layout;
		GF_PixelFormat pfmt;
		const char *name, *fileext, *desc;
		gf_sys_format_help(helpout, help_flags, "# Pixel formats\n");
		gf_sys_format_help(helpout, help_flags, "Name | File extensions | QT 4CC | Description  \n");
		gf_sys_format_help(helpout, help_flags, " --- | --- |  --- | ---  \n");
		while ( (pfmt = gf_pixel_fmt_enum(&idx, &name, &fileext, &desc) )) {
			const char *qtname = "";
			u32 qt_code = gf_pixel_fmt_to_qt_type(pfmt);
			if (qt_code) qtname = gf_4cc_to_str(qt_code);

			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s | %s | %s  \n", name, fileext, qtname, desc);
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Audio formats\n");
		gf_sys_format_help(helpout, help_flags, " Name | File extensions | Description  \n");
		gf_sys_format_help(helpout, help_flags, " --- | --- | ---  \n");
		while ( gf_audio_fmt_enum(&idx, &name, &fileext, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s | %s  \n", name, fileext, desc);
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Stream types\n");
		gf_sys_format_help(helpout, help_flags, " Name | Description  \n");
		gf_sys_format_help(helpout, help_flags, " --- | ---  \n");
		while ( gf_stream_types_enum(&idx, &name, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %s  \n", name, desc);
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Codecs\n");
		gf_sys_format_help(helpout, help_flags, "The codec name identifies a codec within GPAC. There can be several names for a given codec. The first name is used as a default file extension when dumping a raw media stream.  \n\n"
			"_NOTE: This table does not include meta filters (ffmpeg, ...). Use `gpac -hx codecs` to list them._  \n"
			"  \n");

		gf_sys_format_help(helpout, help_flags, " Name | Description  \n");
		gf_sys_format_help(helpout, help_flags, " --- | ---  \n");
		while ( gf_codecid_enum(idx, &name, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR | GF_PRINTARG_ESCAPE_PIPE, "%s | %s  \n", name, desc);
			idx++;
		}


		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Audio channel layout code points (ISO/IEC 23091-3)\n");
		gf_sys_format_help(helpout, help_flags, " Name | Integer value | ChannelMask  \n");
		gf_sys_format_help(helpout, help_flags, " --- | ---  | ---  \n");
		while ( (cicp = gf_audio_fmt_cicp_enum(idx, &name, &layout)) ) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | %d | 0x%016"LLX_SUF"  \n", name, cicp, layout);
			idx++;
		}

		gf_sys_format_help(helpout, help_flags, "# Color Primaries code points (ISO/IEC 23091-2)\n");
		gf_sys_format_help(helpout, help_flags, " Name | Integer value \n");
		gf_sys_format_help(helpout, help_flags, " --- | ---  \n");
		for (i=0; i<GF_CICP_PRIM_LAST; i++) {
			name = gf_cicp_color_primaries_name(i);
			if (!name || !strcmp(name, "unknown")) continue;
			gf_sys_format_help(helpout, help_flags, " %s | %d \n", name, i);
		}
		gf_sys_format_help(helpout, help_flags, "# Transfer Characteristics code points (ISO/IEC 23091-2)\n");
		gf_sys_format_help(helpout, help_flags, " Name | Integer value \n");
		gf_sys_format_help(helpout, help_flags, " --- | ---  \n");
		for (i=0; i<GF_CICP_TRANSFER_LAST; i++) {
			name = gf_cicp_color_transfer_name(i);
			if (!name) continue;
			gf_sys_format_help(helpout, help_flags, " %s | %d \n", name, i);
		}
		gf_sys_format_help(helpout, help_flags, "# Matrix Coefficients code points (ISO/IEC 23091-2)\n");
		gf_sys_format_help(helpout, help_flags, " Name | Integer value \n");
		gf_sys_format_help(helpout, help_flags, " --- | ---  \n");
		for (i=0; i<GF_CICP_MX_LAST; i++) {
			name = gf_cicp_color_matrix_name(i);
			if (!name) continue;
			gf_sys_format_help(helpout, help_flags, " %s | %d \n", name, i);
		}
	} else if (gen_doc==2) {
		u32 idx=0, cicp;
		u64 layout;
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

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Stream types\n");
		while ( gf_stream_types_enum(&idx, &name, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, ".TP\n.B %s\n%s\n", name, desc);
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Codecs\n");
		while ( gf_codecid_enum(idx, &name, &desc)) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, ".TP\n.B %s\n%s\n", name, desc);
			idx++;
		}

		idx=0;
		gf_sys_format_help(helpout, help_flags, "# Stream types\n");
		while ( (cicp = gf_audio_fmt_cicp_enum(idx, &name, &layout)) ) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, ".TP\n.B %s (int %d)\nLayout 0x%016"LLX_SUF"\n", name, cicp, layout);
			idx++;
		}
	}
}
#include <gpac/color.h>
void dump_all_colors(void)
{
	u32 i=0;
	GF_Color color;
	const char *name;
	while (gf_color_enum(&i, &color, &name)) {
		gf_sys_format_help(helpout, help_flags|GF_PRINTARG_HIGHLIGHT_FIRST, "%s: 0x%08X\n", name, color);
	}
}

void dump_all_audio_cicp(void)
{
	u32 i=0, cicp;
	const char *name;
	u64 layout;

	gf_sys_format_help(helpout, help_flags, "CICP Layouts as \"name (index): flags\"\n");
	while ((cicp = gf_audio_fmt_cicp_enum(i, &name, &layout)) ) {
		gf_sys_format_help(helpout, help_flags|GF_PRINTARG_HIGHLIGHT_FIRST, "%s (%d): 0x%016"LLX_SUF"\n", name, cicp, layout);
		i++;
	}
}

void dump_all_codecs(GF_SysArgMode argmode)
{
#define PAD_LEN	30
	GF_PropertyValue rawp, filep, stp;
	GF_PropertyValue cp, acp;
	GF_List *meta_codecs = (argmode>GF_ARGMODE_BASE) ? gf_list_new() : NULL;
	char szFlags[10], szCap[2];
	u32 i;
	u32 cidx=0;
	u32 count = gf_fs_filters_registers_count(session);

	gf_sys_format_help(helpout, help_flags, "Codec support in filters, listed as `built_in_name[|variant] [FLAGS]: full_name (mime)` with possible FLAGS values:\n");
	gf_sys_format_help(helpout, help_flags, "  - I: Raw format input (elementary stream parser) support\n");
	gf_sys_format_help(helpout, help_flags, "  - O: Raw format output (elementary stream writer) support\n");
	gf_sys_format_help(helpout, help_flags, "  - D: Decoder support\n");
	gf_sys_format_help(helpout, help_flags, "  - E: Encoder support\n");
	gf_sys_format_help(helpout, help_flags, "\nNote: Raw output may still be possible even when no output serializer is given\n\n");

	rawp.type = cp.type = GF_PROP_UINT;
	rawp.value.uint = GF_CODECID_RAW;
	filep.type = cp.type = GF_PROP_UINT;
	filep.value.uint = GF_STREAM_FILE;
	stp.type = GF_PROP_UINT;
	cp.type = GF_PROP_UINT;
	acp.type = GF_PROP_UINT;
	szCap[1] = 0;

	if (meta_codecs) {
		for (i=0; i<count; i++) {
			u32 j;
			const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);
			if (! (reg->flags & GF_FS_REG_META)) continue;
			for (j=0; j<reg->nb_caps; j++) {
				if (!(reg->caps[j].flags & GF_CAPFLAG_IN_BUNDLE)) continue;
				if (reg->caps[j].code != GF_PROP_PID_CODECID) continue;
				if (reg->caps[j].val.type != GF_PROP_NAME) continue;
				if (gf_list_find(meta_codecs, (void *)reg)<0)
					gf_list_add(meta_codecs, (void *)reg);
			}
		}
	}

	while (1) {
		const char *lname;
		const char *sname;
		const char *mime;
		u32 rfc4cc;
		Bool enc_found = GF_FALSE;
		Bool dec_found = GF_FALSE;
		Bool dmx_found = GF_FALSE;
		Bool mx_found = GF_FALSE;
		cp.value.uint = gf_codecid_enum(cidx, &sname, &lname);
		cidx++;
		if (cp.value.uint == GF_CODECID_NONE) break;
//		if (cp.value.uint == GF_CODECID_RAW) continue;
		if (!sname) break;

		stp.value.uint = gf_codecid_type(cp.value.uint);
		acp.value.uint = gf_codecid_alt(cp.value.uint);

		for (i=0; i<count; i++) {
			const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);

			if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_STREAM_TYPE, &stp, GF_PROP_PID_STREAM_TYPE, &stp, GF_TRUE)) {
				if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &rawp, GF_PROP_PID_CODECID, &cp, GF_TRUE)) {
					enc_found = GF_TRUE;
					if (gf_list_find(meta_codecs, (void *)reg)>=0)
						gf_list_del_item(meta_codecs, (void *)reg);
				} else if (acp.value.uint && gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &rawp, GF_PROP_PID_CODECID, &acp, GF_TRUE)) {
					enc_found = GF_TRUE;
					if (gf_list_find(meta_codecs, (void *)reg)>=0)
						gf_list_del_item(meta_codecs, (void *)reg);
				}
				if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &cp, GF_PROP_PID_CODECID, &rawp, GF_TRUE)) {
					dec_found = GF_TRUE;
				} else if (acp.value.uint && gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &acp, GF_PROP_PID_CODECID, &rawp, GF_TRUE)) {
					dec_found = GF_TRUE;
				}
			}

			if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_STREAM_TYPE, &filep, GF_PROP_PID_STREAM_TYPE, &stp, GF_TRUE)) {
				if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_STREAM_TYPE, &filep, GF_PROP_PID_CODECID, &cp, GF_TRUE))
					dmx_found = GF_TRUE;
			}
			if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_STREAM_TYPE, &stp, GF_PROP_PID_STREAM_TYPE, &filep, GF_TRUE)) {
				if ( gf_fs_check_filter_register_cap(reg, GF_PROP_PID_CODECID, &cp, GF_PROP_PID_STREAM_TYPE, &filep, GF_TRUE))
					mx_found = GF_TRUE;
			}
		}

		szFlags[0] = 0;
		if (enc_found || dec_found || dmx_found || mx_found) {
			strcpy(szFlags, " [");
			if (dmx_found) { szCap[0] = 'I'; strcat(szFlags, szCap); }
			if (mx_found) { szCap[0] = 'O'; strcat(szFlags, szCap); }
			if (dec_found) { szCap[0] = 'D'; strcat(szFlags, szCap); }
			if (enc_found) { szCap[0] = 'E'; strcat(szFlags, szCap); }
			strcat(szFlags, "]");
		}

		mime = gf_codecid_mime(cp.value.uint);
		rfc4cc = gf_codecid_4cc_type(cp.value.uint);
		gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s%s: ", sname, szFlags);

		u32 len = (u32) (2 + strlen(sname) + strlen(szFlags));
		while (len<PAD_LEN) {
			len++;
			gf_sys_format_help(helpout, help_flags, " ");
		}
		gf_sys_format_help(helpout, help_flags, "%s", lname);
		if (rfc4cc || mime) {
			gf_sys_format_help(helpout, help_flags, " (");
			if (rfc4cc) gf_sys_format_help(helpout, help_flags, "%s", gf_4cc_to_str(rfc4cc) );
			if (rfc4cc&&mime) gf_sys_format_help(helpout, help_flags, ", ");
			if (mime) gf_sys_format_help(helpout, help_flags, "%s", mime);
			gf_sys_format_help(helpout, help_flags, ")");
		}
		gf_sys_format_help(helpout, help_flags, "\n");
	}

	if (meta_codecs) {
		gf_sys_format_help(helpout, help_flags, "# Meta-filters codec support (not mapped to GPAC codec IDs)\n");
		gf_sys_format_help(helpout, help_flags, "Only encoding and decoding support is listed.\n\n");
		count = gf_list_count(meta_codecs);
		for (i=0; i<count; i++) {
			u32 j, k;
			Bool has_dec = GF_FALSE, has_enc = GF_FALSE;
			const char *codec_desc = "Unknown";
			const GF_FilterRegister *reg = gf_list_get(meta_codecs, i);
			const char *name = strchr(reg->name, ':');
			if (name) name++;
			else name = reg->name;
			for (j=0; j<reg->nb_caps; j++) {
				if (reg->caps[j].code != GF_PROP_PID_CODECID) continue;
				if (reg->caps[j].val.type != GF_PROP_NAME) continue;
				codec_desc = reg->caps[j].val.value.string;
				if (reg->caps[j].flags & GF_CAPFLAG_INPUT) has_dec = GF_TRUE;
				else if (reg->caps[j].flags & GF_CAPFLAG_OUTPUT) has_enc = GF_TRUE;
				break;
			}
			for (k=0; k<count; k++) {
				if (k==i) continue;
				reg = gf_list_get(meta_codecs, k);
				const char *rname = strchr(reg->name, ':');
				if (rname) rname++;
				else rname = reg->name;
				if (strcmp(rname, name)) continue;

				for (j=0; j<reg->nb_caps; j++) {
					if (reg->caps[j].code != GF_PROP_PID_CODECID) continue;
					if (reg->caps[j].val.type != GF_PROP_NAME) continue;
					if (reg->caps[j].flags & GF_CAPFLAG_INPUT) has_dec = GF_TRUE;
					else if (reg->caps[j].flags & GF_CAPFLAG_OUTPUT) has_enc = GF_TRUE;
					gf_list_rem(meta_codecs, k);
					count--;
				}
			}
			strcpy(szFlags, " [");
			if (has_dec) strcat(szFlags, "D");
			if (has_enc) strcat(szFlags, "E");
			strcat(szFlags, "]");

			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s%s: ", name, szFlags);
			u32 len = (u32) (2 + strlen(name) + strlen(szFlags));
			while (len<PAD_LEN) {
				len++;
				gf_sys_format_help(helpout, help_flags, " ");
			}
			gf_sys_format_help(helpout, help_flags, "%s\n", codec_desc);
			gf_list_rem(meta_codecs, i);
			i--;
			count--;
		}
		gf_list_del(meta_codecs);
	}

	gf_sys_format_help(helpout, help_flags, "\n");
}

typedef struct
{
	char *ext;
	char *mime;
	GF_List *demuxers, *muxers;
	Bool meta_only;
} FMTHandler;

//quick hack for a few of our filters to avoid crazy long lists of mimes
static const char *get_best_mime(char *ext)
{
	if (!strcmp(ext, "mp4") || !strcmp(ext, "iso") || !strcmp(ext, "ismv")) return "video/mp4|audio/mp4|application/mp4";
	if (!strcmp(ext, "mpg4")) return "application/mp4";
	if (!strcmp(ext, "m4a")) return "audio/mp4";
	if (!strcmp(ext, "m4i")) return "application/mp4";
	if (!strcmp(ext, "3gp") || !strcmp(ext, "3gpp")) return "video/3gpp|audio/3gpp";
	if (!strcmp(ext, "3g2") || !strcmp(ext, "3gp2")) return "video/3gp2|audio/3gp2";
	if (!strcmp(ext, "m4s")) return "video/iso.segment|audio/iso.segment";
	if (!strcmp(ext, "iff") || !strcmp(ext, "heif")) return "image/heif";
	if (!strcmp(ext, "heic")) return "image/heic";
	if (!strcmp(ext, "avci") || !strcmp(ext, "avif")) return "image/avci";
	if (!strcmp(ext, "mj2")) return "video/jp2";
	if (!strcmp(ext, "mov") || !strcmp(ext, "qt")) return "video/quicktime";
	if (!strcmp(ext, "bt") || !strcmp(ext, "btz") || !strcmp(ext, "bt.gz")) return "application/x-bt";
	if (!strcmp(ext, "xmt") || !strcmp(ext, "xmtz") || !strcmp(ext, "xmt.gz")) return "application/x-xmt";
	if (!strcmp(ext, "wrl") || !strcmp(ext, "wrl.gz")) return "model/vrml";
	if (!strcmp(ext, "x3dv") || !strcmp(ext, "x3dvz") || !strcmp(ext, "x3dv.gz")) return "model/x3d+vrml";
	if (!strcmp(ext, "x3d") || !strcmp(ext, "x3dz") || !strcmp(ext, "x3d.gz")) return "model/x3d+xml";
	if (!strcmp(ext, "swf")) return "application/x-shockwave-flash";
	if (!strcmp(ext, "xsr")) return "application/x-LASeR+xml";
	if (!strcmp(ext, "svg") || !strcmp(ext, "svgz") || !strcmp(ext, "svg.gz")) return "image/svg+xml";
	if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg")) return "image/jpg";
	if (!strcmp(ext, "jp2") || !strcmp(ext, "j2k")) return "image/jp2";
	if (!strcmp(ext, "bmp")) return "image/bmp";
	if (!strcmp(ext, "png")) return "image/png";
	if (!strcmp(ext, "aac") || !strcmp(ext, "adts")) return "audio/aac";
	if (!strcmp(ext, "latm")) return "audio/aac+latm";
	if (!strcmp(ext, "usac") || !strcmp(ext, "xheaac")) return "audio/xheaac+latm";
	if (!strcmp(ext, "amr") || !strcmp(ext, "awb")) return "audio/amr";
	if (!strcmp(ext, "evc")) return "audio/x-evc";
	if (!strcmp(ext, "smv")) return "audio/x-smv";
	if (!strcmp(ext, "oga") || !strcmp(ext, "spx") || !strcmp(ext, "opus")) return "audio/ogg";
	if (!strcmp(ext, "ogg") || !strcmp(ext, "ogv")) return "video/ogg";
	if (!strcmp(ext, "oggm")) return "application/ogg";
	if (!strcmp(ext, "ts") || !strcmp(ext, "m2t") || !strcmp(ext, "mts") || !strcmp(ext, "dmb") || !strcmp(ext, "trp")) return "video/mp2t";
	if (!strcmp(ext, "ac3")) return "audio/ac3";
	if (!strcmp(ext, "eac3")) return "audio/eac3";
	if (!strcmp(ext, "mpd")) return "application/dash+xml";
	if (!strcmp(ext, "m3u8")) return "application/vnd.apple.mpegurl";
	if (!strcmp(ext, "ism")) return "application/vnd.ms-sstr+xml";
	if (!strcmp(ext, "3gm")) return "application/vnd.3gpp.mpd";
	if (!strcmp(ext, "264") || !strcmp(ext, "avc") || !strcmp(ext, "h264") || !strcmp(ext, "26l") || !strcmp(ext, "h26l")) return "video/h264";
	if (!strcmp(ext, "svc")) return "video/svc";
	if (!strcmp(ext, "mvc")) return "video/mvc";
	if (!strcmp(ext, "265") || !strcmp(ext, "hvc") || !strcmp(ext, "h265") || !strcmp(ext, "hevc")) return "video/hevc";
	if (!strcmp(ext, "shvc")) return "video/shvc";
	if (!strcmp(ext, "mhvc")) return "video/mhvc";
	if (!strcmp(ext, "lvc") || !strcmp(ext, "lhvc")) return "video/lhvc";
	if (!strcmp(ext, "266") || !strcmp(ext, "vvc") || !strcmp(ext, "h266") || !strcmp(ext, "lvvc")) return "video/vvc";
	if (!strcmp(ext, "srt")) return "subtitle/srt";
	if (!strcmp(ext, "ttxt")) return "subtitle/x-ttxt";
	if (!strcmp(ext, "vtt")) return "subtitle/vtt";
	if (!strcmp(ext, "sub")) return "subtitle/sub";
	if (!strcmp(ext, "txml")) return "x-quicktime/text";
	if (!strcmp(ext, "ttml")) return "subtitle/ttml";
	if (!strcmp(ext, "ass") || !strcmp(ext, "ssa")) return "subtitle/ssa";
	if (!strcmp(ext, "mkv") || !strcmp(ext, "mks")) return "video/x-matroska";
	if (!strcmp(ext, "mka")) return "audio/x-matroska";
	if (!strcmp(ext, "webm")) return "video/webm";
	return NULL;
}
static void push_ext_mime(GF_List *all_fmts, const char *exts, Bool is_output, const GF_FilterRegister *reg, const char *mime)
{
	FMTHandler *hdl;
	if (!exts) return;
	if (!strcmp(exts, "*")) return;

	char *names = gf_strdup(exts);
	char *name = names;
	while (name) {
		Bool found = GF_FALSE;
		char *sep = strchr(name, '|');
		char *sep2 = strchr(name, ',');
		if (sep2 && sep && (sep2<sep)) sep = sep2;
		else if (sep2 && !sep) sep=sep2;

		if (sep) sep[0] = 0;
		hdl = NULL;
		u32 i, count = gf_list_count(all_fmts);
		for (i=0; i<count; i++) {
			hdl = gf_list_get(all_fmts, i);
			if (!strcmp(hdl->ext, name)) {
				found=GF_TRUE;
				break;
			}
		}
		if (!found || !hdl) {
			GF_SAFEALLOC(hdl, FMTHandler);
			while (name[0]==' ')
				name = name+1;
			hdl->ext = gf_strdup(name);
			hdl->demuxers = gf_list_new();
			hdl->muxers = gf_list_new();
			if (reg->flags & GF_FS_REG_META)
				hdl->meta_only = GF_TRUE;
			gf_list_add(all_fmts, hdl);
		}
		if (mime && !hdl->mime) {
			const char *small_mime = get_best_mime(hdl->ext);
			if (small_mime)
				hdl->mime = gf_strdup(small_mime);
			else {
				hdl->mime = gf_strdup(mime);
			}
		}
		if (!(reg->flags & GF_FS_REG_META))
			hdl->meta_only = GF_TRUE;

		if (is_output) {
			if (gf_list_find(hdl->muxers, (void*) reg)<0) {
				gf_list_add(hdl->muxers, (void*) reg);
			}
		} else {
			if (gf_list_find(hdl->demuxers, (void*) reg)<0) {
				gf_list_add(hdl->demuxers, (void*)reg);
			}
		}
		if (!sep) break;
		name = sep+1;
	}
	gf_free(names);
}

void dump_all_formats(GF_SysArgMode argmode)
{
	u32 i, count = gf_fs_filters_registers_count(session);
	GF_List *all_fmts = gf_list_new();
	GF_List *all_inputs = gf_list_new();

	for (i=0; i<count; i++) {
		u32 j, nb_in=0;
		const char *in_ext=NULL;
		const char *out_ext=NULL;
		const char *in_mime=NULL;
		const char *out_mime=NULL;
		Bool ext_static = GF_FALSE;
		Bool st_static = GF_FALSE;
		u32 in_st_type = GF_STREAM_UNKNOWN;
		u32 out_st_type = GF_STREAM_UNKNOWN;
		const GF_FilterRegister *reg = gf_fs_get_filter_register(session, i);
		if ((argmode<GF_ARGMODE_EXPERT) && (reg->flags&GF_FS_REG_META)) continue;

		for (j=0; j<reg->nb_caps; j++) {
			if (!(reg->caps[j].flags & GF_CAPFLAG_IN_BUNDLE)) {
				//special case for meta filters, declaring only input ext (demux) or output ext (mux)
				if (reg->flags & GF_FS_REG_META) {
					if (in_ext && (out_st_type==GF_STREAM_UNKNOWN)) out_st_type = GF_STREAM_VISUAL;
					if (out_ext && (in_st_type==GF_STREAM_UNKNOWN)) in_st_type = GF_STREAM_VISUAL;
				}

				if (in_ext && (in_st_type==GF_STREAM_FILE) && !out_ext && (out_st_type!=GF_STREAM_UNKNOWN))
					push_ext_mime(all_fmts, in_ext, GF_FALSE, reg, in_mime);
				if (out_ext && (out_st_type==GF_STREAM_FILE) && !in_ext && (in_st_type!=GF_STREAM_UNKNOWN))
					push_ext_mime(all_fmts, out_ext, GF_TRUE, reg, out_mime);

				if (!ext_static) {
					in_ext=NULL;
					out_ext=NULL;
				}
				if (!st_static) {
					in_st_type = GF_STREAM_UNKNOWN;
					out_st_type = GF_STREAM_UNKNOWN;
				}
				continue;
			}
			if (reg->caps[j].flags & GF_CAPFLAG_INPUT) nb_in++;

			if (reg->caps[j].code == GF_PROP_PID_FILE_EXT) {
				if ((reg->caps[j].val.type != GF_PROP_NAME)
					&& (reg->caps[j].val.type != GF_PROP_STRING)
					&& (reg->caps[j].val.type != GF_PROP_STRING_NO_COPY)
				) continue;
				char *ext = reg->caps[j].val.value.string;
				if (!ext || !ext[0]) continue;

				if (reg->caps[j].flags & GF_CAPFLAG_STATIC) ext_static = GF_TRUE;
				if (reg->caps[j].flags & GF_CAPFLAG_INPUT) {
					in_ext = ext;
					in_st_type = GF_STREAM_FILE;
				}
				if (reg->caps[j].flags & GF_CAPFLAG_OUTPUT) {
					out_ext = ext;
					out_st_type = GF_STREAM_FILE;
				}
			} else if (reg->caps[j].code == GF_PROP_PID_MIME) {
				if ((reg->caps[j].val.type != GF_PROP_NAME)
					&& (reg->caps[j].val.type != GF_PROP_STRING)
					&& (reg->caps[j].val.type != GF_PROP_STRING_NO_COPY)
				) continue;
				char *m = reg->caps[j].val.value.string;
				if (!m || !m[0]) continue;
				if (reg->caps[j].flags & GF_CAPFLAG_INPUT) in_mime = m;
				if (reg->caps[j].flags & GF_CAPFLAG_OUTPUT) out_mime = m;
			} else if (reg->caps[j].code == GF_PROP_PID_STREAM_TYPE) {
				if (reg->caps[j].flags & GF_CAPFLAG_STATIC) st_static = GF_TRUE;
				u32 st = reg->caps[j].val.value.uint;
				//special case for filters advertising any stream, consider not file
				if ((st==GF_STREAM_UNKNOWN) && (reg->caps[j].flags&GF_CAPFLAG_EXCLUDED))
					st = GF_STREAM_VISUAL;
				if (reg->caps[j].flags & GF_CAPFLAG_INPUT) in_st_type = st;
				if (reg->caps[j].flags & GF_CAPFLAG_OUTPUT) out_st_type = st;
			}
		}

		if (reg->flags & GF_FS_REG_META) {
			if (in_ext && (out_st_type==GF_STREAM_UNKNOWN)) out_st_type = GF_STREAM_VISUAL;
			if (out_ext && (in_st_type==GF_STREAM_UNKNOWN)) in_st_type = GF_STREAM_VISUAL;
		}
		if (in_ext && (in_st_type==GF_STREAM_FILE) && !out_ext && (out_st_type!=GF_STREAM_UNKNOWN))
			push_ext_mime(all_fmts, in_ext, GF_FALSE, reg, in_mime);
		if (out_ext && (out_st_type==GF_STREAM_FILE) && !in_ext && (in_st_type!=GF_STREAM_UNKNOWN))
			push_ext_mime(all_fmts, out_ext, GF_TRUE, reg, out_mime);


		if (reg->nb_caps && !nb_in && reg->probe_url && !strchr(reg->name, ':')) {
			gf_list_add(all_inputs, (void*)reg);
		}
	}

	if (gen_doc==1) {
		gf_sys_format_help(helpout, help_flags, "# Extensions and mime types\n");
		gf_sys_format_help(helpout, help_flags, "Extension name can be used to force output formats using `ext=` option of sink filters.  \nBy default, GPAC does not rely on file extension for source processing unless `-no-probe` option is set, in which case `ext=` option may be set on source filter.  \n");
		gf_sys_format_help(helpout, help_flags, "_NOTE: This table does not include meta filters (ffmpeg, ...), use `gpac -hx formats` to list them._\n\n");
		gf_sys_format_help(helpout, help_flags, " Extension | Input Filter(s) | Output Filter(s) | Mime(s) \n");
		gf_sys_format_help(helpout, help_flags, " --- | --- | --- | ---  \n");
	} else {
		gf_sys_format_help(helpout, help_flags, "Format support in filters, by file extension\n");
		gf_sys_format_help(helpout, help_flags, "\nNote: Some demuxers may only rely on data probing and not declare file extensions used for formats, resulting in formats not listed as supported for inputs\n\n");
	}

	count = gf_list_count(all_fmts);
	for (i=0; i<count; i++) {
		u32 j, c2;
		FMTHandler *hdl = gf_list_get(all_fmts, i);
		if (gen_doc==1) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "%s | ", hdl->ext);
		} else {
			gf_sys_format_help(helpout, help_flags|GF_PRINTARG_HIGHLIGHT_FIRST, "%s:", hdl->ext);

		}
		c2 = gf_list_count(hdl->demuxers);
		if (!c2 && hdl->meta_only) {
			char szFileName[100];
			sprintf(szFileName, "test.%s", hdl->ext);
			c2 = gf_list_count(all_inputs);
			for (j=0; j<c2; j++) {
				const GF_FilterRegister *reg = gf_list_get(all_inputs, j);
				GF_FilterProbeScore score = reg->probe_url(szFileName, NULL);
				if (score>=GF_FPROBE_MAYBE_SUPPORTED) {
					gf_list_add(hdl->demuxers, (void*)reg);
					break;
				}
			}
			c2 = gf_list_count(hdl->demuxers);
		}
		if (c2 && (gen_doc!=1)) {
			gf_sys_format_help(helpout, help_flags, " Demux");
		}

		if (c2) {
			if ((gen_doc==1) || (argmode>=GF_ARGMODE_ADVANCED)) {
				if (gen_doc!=1)
					gf_sys_format_help(helpout, help_flags, " (");
				for (j=0; j<c2; j++) {
					const GF_FilterRegister *reg = gf_list_get(hdl->demuxers, j);
					if (j) gf_sys_format_help(helpout, help_flags, " ");
					gf_sys_format_help(helpout, help_flags, "%s", reg->name);
				}
				if (gen_doc!=1)
					gf_sys_format_help(helpout, help_flags, ")");
			}
		} else if (gen_doc==1) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "n/a");
		}

		if (gen_doc==1)
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, " | ");

		c2 = gf_list_count(hdl->muxers);
		if (c2) {
			if (gen_doc!=1)
				gf_sys_format_help(helpout, help_flags, " Mux");
			if ((gen_doc==1) || (argmode>=GF_ARGMODE_ADVANCED)) {
				if (gen_doc!=1)
					gf_sys_format_help(helpout, help_flags, " (");
				for (j=0; j<c2; j++) {
					const GF_FilterRegister *reg = gf_list_get(hdl->muxers, j);
					if (j) gf_sys_format_help(helpout, help_flags, " ");
					gf_sys_format_help(helpout, help_flags, "%s", reg->name);
				}
				if (gen_doc!=1)
					gf_sys_format_help(helpout, help_flags, ")");
			}
		} else if (gen_doc==1) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, "n/a");
		}

		if (gen_doc==1) {
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR, " | ");
			gf_sys_format_help(helpout, help_flags | GF_PRINTARG_NL_TO_BR | GF_PRINTARG_ESCAPE_PIPE, "%s \n", hdl->mime ? hdl->mime : "n/a");
		} else {
			if (hdl->mime && (argmode>GF_ARGMODE_EXPERT)) {
				gf_sys_format_help(helpout, help_flags, " MIME %s", hdl->mime);
			}
			gf_sys_format_help(helpout, help_flags, "\n");
		}
	}

	while (gf_list_count(all_fmts)) {
		FMTHandler *hdl = gf_list_pop_back(all_fmts);
		gf_free(hdl->ext);
		if (hdl->mime) gf_free(hdl->mime);
		gf_list_del(hdl->demuxers);
		gf_list_del(hdl->muxers);
		gf_free(hdl);
	}
	gf_list_del(all_fmts);
	gf_list_del(all_inputs);
}

typedef struct
{
	char *proto;
	GF_List *in, *out;
} PROTOHandler;

void dump_all_proto_schemes(GF_SysArgMode argmode)
{
	GF_List *all_protos = gf_list_new();
	u32 k, i, count;
	for (k=0; k<2; k++) {
		const char *sname = k ? "temp_out_proto" : "temp_in_proto";
		count = gf_opts_get_key_count(sname);
		for (i=0; i<count; i++) {
			const char *f = gf_opts_get_key_name(sname, i);
			if (!f) continue;
			if ((argmode<GF_ARGMODE_EXPERT) && strchr(f, ':')) continue;

			char *proto = (char*)gf_opts_get_key(sname, f);
			if (!proto) continue;

			while (proto) {
				PROTOHandler *pe=NULL;
				char *sep = strchr(proto, ',');
				if (sep) sep[0] = 0;
				u32 j=0;
				while ((pe = gf_list_enum(all_protos, &j) )) {
					if (!strcmp(pe->proto, proto)) {
						break;
					}
				}
				if (!pe) {
					GF_SAFEALLOC(pe, PROTOHandler);
					pe->proto = gf_strdup(proto);
					pe->in = gf_list_new();
					pe->out = gf_list_new();
					gf_list_add(all_protos, pe);
				}
				if (k)
					gf_list_add(pe->out, (void *)f);
				else
					gf_list_add(pe->in, (void *)f);

				if (!sep) break;
				proto = sep+1;
			}
		}
	}

	if (gen_doc==1) {
		gf_sys_format_help(helpout, help_flags, "# Protocol Schemes\n");
		gf_sys_format_help(helpout, help_flags, "_NOTE: This table does not include meta filters (ffmpeg, ...), use `gpac -hx protocols` to list them._\n\n");
		gf_sys_format_help(helpout, help_flags, " Scheme | Input Filter(s) | Output Filter(s)\n");
		gf_sys_format_help(helpout, help_flags, " --- | --- | ---  \n");
	} else {
		gf_sys_format_help(helpout, help_flags, "\nSupported protocols schemes (listed as `scheme: in (filters) out (filters)`):\n");
	}
	count = gf_list_count(all_protos);
	for (i=0; i<count; i++) {
		u32 j, c2;
		PROTOHandler *pe = gf_list_get(all_protos, i);

		if (gen_doc==1) {
			gf_sys_format_help(helpout, help_flags|GF_PRINTARG_NL_TO_BR, "%s | ", pe->proto);
		} else {
			gf_sys_format_help(helpout, help_flags|GF_PRINTARG_HIGHLIGHT_FIRST, "%s:", pe->proto);
		}
		for (k=0; k<2; k++) {
			GF_List *list = k ? pe->out : pe->in;
			const char *lab = k ? "in" : "out";
			c2 = gf_list_count(list);
			if (c2) {
				if (gen_doc!=1)
					gf_sys_format_help(helpout, help_flags, " %s", lab);

				if ((gen_doc==1) || (argmode==GF_ARGMODE_ADVANCED) || (argmode==GF_ARGMODE_ALL)) {
					if (gen_doc!=1)
						gf_sys_format_help(helpout, help_flags, " (");
					for (j=0;j<c2; j++) {
						char *f = gf_list_get(list, j);
						if (j) gf_sys_format_help(helpout, help_flags, " ");
						gf_sys_format_help(helpout, help_flags, "%s", f);
					}
					if (gen_doc!=1)
						gf_sys_format_help(helpout, help_flags, ")");
				}
			} else if (gen_doc==1) {
				gf_sys_format_help(helpout, help_flags, " n/a");
			}
			if ((gen_doc==1) && !k)
				gf_sys_format_help(helpout, help_flags, " | ");
		}
		gf_sys_format_help(helpout, help_flags, "\n");
		gf_free(pe->proto);
		gf_list_del(pe->in);
		gf_list_del(pe->out);
		gf_free(pe);
	}
	gf_list_del(all_protos);
}


/*********************************************************
			Config file writing functions
*********************************************************/

void write_core_options()
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

void write_file_extensions()
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

void write_filters_options()
{
	u32 i, count;
	count = gf_fs_filters_registers_count(session);
	for (i=0; i<count; i++) {
		char *meta_sep;
		u32 j=0;
		char szSecName[200];

		const GF_FilterRegister *freg = gf_fs_get_filter_register(session, i);
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

#ifndef GPAC_DISABLE_DOC
static Bool lang_updated = GF_FALSE;
static void gpac_lang_set_key(GF_Config *cfg, const char *sec_name,  const char *key_name, const char *key_val)
{
	char szKeyCRC[1024];
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
		char szKeyCRCVal[100];
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
#endif

int gpac_make_lang(char *filename)
{
#ifndef GPAC_DISABLE_DOC
	GF_Config *cfg;
	u32 i;
	gf_sys_init(GF_MemTrackerNone, NULL);

	session = gf_fs_new_defaults(0);
	if (!session) {
		gf_sys_format_help(helpout, help_flags, "failed to load session, cannot create language file\n");
		return 1;
	}
	if (!gf_file_exists(filename)) {
		FILE *f = gf_fopen(filename, "wt");
		if (!f) {
			gf_sys_format_help(helpout, help_flags, "failed to open %s in write mode\n", filename);
			gf_fs_del(session);
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

#ifndef GPAC_DISABLE_DOC
	//print gpac doc
	gpac_lang_set_key(cfg, "gpac", "doc", gpac_doc);

	//print gpac alias doc
	gpac_lang_set_key(cfg, "gpac", "alias", gpac_alias);
#endif

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

#ifndef GPAC_DISABLE_DOC
		if (reg->description) {
			gpac_lang_set_key(cfg, reg->name, "desc", reg->description);
		}
		if (reg->help) {
			gpac_lang_set_key(cfg, reg->name, "help", reg->help);
		}
#endif
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

	gf_fs_del(session);
	gf_sys_close();
	return 0;
#else
	gf_sys_format_help(helpout, help_flags, "Documentation disabled in build, cannot create language file\n");
	return 1;
#endif
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

static Bool gpac_expand_alias_arg(char *param, char *prefix, char *suffix, int arg_idx, int argc, char **argv, char *alias_name);

static Bool check_param_extension(char *szArg, int arg_idx, int argc, char **argv, char *alias_name)
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

	 	ok = gpac_expand_alias_arg(szPar, szArg, par_end+1, arg_idx, argc, argv, alias_name);
	 	if (!ok) return GF_FALSE;
		par_start[0] = '@';
		par_end[0] = '}';
		return GF_TRUE;
	}
	//done, push arg
	push_arg(szArg, 1);
	return GF_TRUE;
}

static Bool gpac_expand_alias_arg(char *param, char *prefix, char *suffix, int arg_idx, int argc, char **argv, char *alias_name)
{
	char *alias_arg=NULL;
	char szSepList[2];
	char *oparam = param;
	szSepList[0] = separator_set[SEP_LIST];
	szSepList[1] = 0;

	Bool is_list = param[0]=='-';
	Bool is_expand = param[0]=='+';

	if (is_list || is_expand) param++;

	gf_dynstrcat(&alias_arg, prefix, NULL);
	while (param) {
		u32 idx=0;
		u32 last_idx=0;
		char *sep = strchr(param, ',');
		if (sep) sep[0]=0;

		if ((param[0]=='n') || (param[0]=='N')) {
			idx = argc - arg_idx - 1;
			if (param[1]=='-') {
				u32 diff = atoi(param+2);
				if (diff>=idx) {
					if (sep) sep[0]=',';
					fprintf(stderr, "Bad usage for alias %s parameter %s: not enough parameters\n", alias_name, oparam);
					gf_free(alias_arg);
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
							fprintf(stderr, "Bad usage for alias %s parameter %s: not enough parameters\n", alias_name, oparam);
							gf_free(alias_arg);
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
			fprintf(stderr, "Bad format for alias %s parameter %s: cannot extract argument index\n", alias_name, oparam);
			gf_free(alias_arg);
			return GF_FALSE;
		}
		if ((int) idx + arg_idx >= argc) {
			if (sep) sep[0]=',';
			fprintf(stderr, "Bad format for %s alias parameter %s: argument out of bounds (not enough paramteters?)\n", alias_name, oparam);
			gf_free(alias_arg);
			return GF_FALSE;
		}

		if (!last_idx) last_idx=idx;
		for (; idx<=last_idx;idx++) {
			char *an_arg = argv[idx+arg_idx];

			if (!args_used) args_used = gf_list_new();
			gf_list_add(args_used, an_arg);

			if (is_expand) {
				gf_free(alias_arg);
				alias_arg = NULL;
				gf_dynstrcat(&alias_arg, prefix, NULL);
				gf_dynstrcat(&alias_arg, an_arg, NULL);
				gf_dynstrcat(&alias_arg, suffix, NULL);

				Bool ok = check_param_extension(alias_arg, arg_idx, argc, argv, alias_name);
				if (!ok) {
					if (sep) sep[0]=',';
					gf_free(alias_arg);
					return GF_FALSE;
				}
			} else {
				gf_dynstrcat(&alias_arg, an_arg, NULL);
				if (is_list && (idx<last_idx)) {
					gf_dynstrcat(&alias_arg, szSepList, NULL);
				}
			}
		}

		if (!sep) break;
		sep[0]=',';
		param = sep+1;
		if (is_list) {
			gf_dynstrcat(&alias_arg, szSepList, NULL);
		}
	}

	if (is_expand) {
		gf_free(alias_arg);
		return GF_TRUE;
	}

	gf_dynstrcat(&alias_arg, suffix, NULL);

	Bool res = check_param_extension(alias_arg, arg_idx, argc, argv, alias_name);
	gf_free(alias_arg);
	return res;
}

Bool gpac_expand_alias(int o_argc, char **o_argv)
{
	u32 i, a_idx;
	Bool has_xopt=GF_FALSE;
	int argc = o_argc;

	//move all options at the beginning, except anything specified after xopt
	char **argv = gf_malloc(sizeof(char*) * argc);
	if (!argv) return GF_FALSE;

	a_idx = 1;
	argv[0] = o_argv[0];
	for (i=1; i< (u32) argc; i++) {
		//alias, do not push
		const char *alias = gf_opts_get_key("gpac.alias", o_argv[i]);
		if (alias != NULL) {
			if (strstr(alias, "xopt")) break;
			continue;
		}
		//not an option, do not push
		if (o_argv[i][0] != '-') continue;
		argv[a_idx] = o_argv[i];
		a_idx++;
	}
	for (i=1; i< (u32) argc; i++) {
		//option and not an alias, do not push
		if (o_argv[i][0] == '-') {
			if (!has_xopt) {
				const char *alias = gf_opts_get_key("gpac.alias", o_argv[i]);
				if (alias == NULL)
					continue;
				if (strstr(alias, "xopt")) has_xopt = GF_TRUE;
			}
		}
		argv[a_idx] = o_argv[i];
		a_idx++;
	}
	assert(argc==a_idx);

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

#ifndef GPAC_CONFIG_ANDROID
		if (!compositor_mode) {
			if (!strcmp(arg, "-mp4c")) compositor_mode = LOAD_MP4C;
			//on OSX we only activate this mode when launched from finder
			else if (!strcmp(arg, "-gui")) compositor_mode = LOAD_GUI;
		}
#endif

		while (alias) {
			sep = strchr(alias, ' ');
			if (sep) sep[0] = 0;
			Bool ok = check_param_extension(alias, i, argc, argv, arg);
			if (sep) sep[0] = ' ';
			if (!ok) {
				gf_free(argv);
				return GF_FALSE;
			}
			if (!sep) break;
			alias = sep+1;
		}
	}
	gf_free(argv);
	return GF_TRUE;
}


void parse_sep_set(const char *arg, Bool *override_seps)
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
