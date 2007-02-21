include ../config.mak

vpath %.c $(SRC_PATH)/src

CFLAGS= $(OPTFLAGS) -Wall -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

## libgpac objects gathering: src/utils
LIBGPAC_UTILS=utils/os_divers.o utils/os_net.o utils/os_module.o utils/os_thread.o utils/list.o utils/base_encoding.o utils/bitstream.o utils/color.o utils/configfile.o utils/downloader.o utils/error.o utils/math.o utils/path2d.o utils/path2d_stroker.o utils/module.o utils/token.o utils/url.o utils/utf.o utils/xml_parser.o

## libgpac objects gathering: src/ietf
LIBGPAC_IETF=ietf/rtcp.o ietf/rtp.o ietf/rtp_packetizer.o ietf/rtp_pck_3gpp.o ietf/rtp_pck_mpeg12.o ietf/rtp_pck_mpeg4.o ietf/rtsp_command.o ietf/rtsp_common.o ietf/rtsp_response.o ietf/rtsp_session.o ietf/sdp.o

## libgpac objects gathering: src/bifs
LIBGPAC_BIFS=bifs/arith_decoder.o bifs/bifs_codec.o bifs/bifs_node_tables.o bifs/com_dec.o bifs/com_enc.o bifs/conditional.o bifs/field_decode.o bifs/field_encode.o bifs/memory_decoder.o bifs/predictive_mffield.o bifs/quantize.o bifs/script_dec.o bifs/script_enc.o bifs/unquantize.o

## libgpac objects gathering: src/isomedia
LIBGPAC_ISOM=isomedia/avc_ext.o isomedia/box_code_3gpp.o isomedia/box_code_apple.o isomedia/box_code_base.o isomedia/box_code_isma.o isomedia/box_code_meta.o isomedia/box_dump.o isomedia/box_funcs.o isomedia/data_map.o isomedia/hint_track.o isomedia/hinting.o isomedia/isma_sample.o isomedia/isom_intern.o isomedia/isom_read.o isomedia/isom_store.o isomedia/isom_write.o isomedia/media.o isomedia/media_odf.o isomedia/meta.o isomedia/movie_fragments.o isomedia/sample_descs.o isomedia/stbl_read.o isomedia/stbl_write.o isomedia/track.o isomedia/tx3g.o 

## libgpac objects gathering: src/odf
LIBGPAC_ODF=odf/desc_private.o odf/descriptors.o odf/ipmpx_code.o odf/ipmpx_dump.o odf/ipmpx_parse.o odf/oci_codec.o odf/odf_code.o odf/odf_codec.o odf/odf_command.o odf/odf_dump.o odf/odf_parse.o odf/qos.o odf/slc.o 

## libgpac objects gathering: src/scenegraph
LIBGPAC_SCENE=scenegraph/base_scenegraph.o scenegraph/mpeg4_animators.o scenegraph/commands.o scenegraph/mpeg4_nodes.o scenegraph/mpeg4_valuator.o scenegraph/vrml_interpolators.o scenegraph/vrml_proto.o scenegraph/vrml_route.o scenegraph/vrml_script.o scenegraph/vrml_smjs.o scenegraph/vrml_tools.o scenegraph/x3d_nodes.o 

ifeq ($(DISABLE_SVG), no)
LIBGPAC_SCENE+=scenegraph/svg_attributes.o scenegraph/svg_nodes.o scenegraph/svg_tools.o scenegraph/svg_types.o scenegraph/svg_smjs.o scenegraph/smil_anim.o scenegraph/smil_timing.o scenegraph/svg_properties.o scenegraph/dom_events.o
endif


## libgpac objects gathering: src/mcrypt
LIBGPAC_MCRYPT=mcrypt/cbc.o mcrypt/cfb.o mcrypt/ctr.o mcrypt/des.o mcrypt/ecb.o mcrypt/g_crypt.o mcrypt/ncfb.o mcrypt/nofb.o mcrypt/ofb.o mcrypt/rijndael-128.o mcrypt/rijndael-192.o mcrypt/rijndael-256.o mcrypt/stream.o mcrypt/tripledes.o 

## libgpac objects gathering: src/media tools
LIBGPAC_MEDIATOOLS=media_tools/av_parsers.o media_tools/avilib.o media_tools/gpac_ogg.o media_tools/ismacryp.o media_tools/isom_hinter.o media_tools/isom_tools.o media_tools/media_export.o media_tools/media_import.o media_tools/mpeg2_ps.o media_tools/text_import.o media_tools/saf.o media_tools/mpegts.o media_tools/vobsub.o

## libgpac objects gathering: src/scene_manager
LIBGPAC_SCENEMANAGER=scene_manager/loader_bt.o scene_manager/loader_isom.o scene_manager/loader_qt.o scene_manager/loader_xmt.o scene_manager/scene_dump.o scene_manager/scene_manager.o scene_manager/scene_stats.o scene_manager/swf_parse.o scene_manager/swf_shape.o scene_manager/text_to_bifs.o scene_manager/encode_cbk.o scene_manager/encode_isom.o 
ifeq ($(DISABLE_SVG), no)
LIBGPAC_SCENEMANAGER+=scene_manager/loader_svg.o
endif

## libgpac objects gathering: src/terminal
LIBGPAC_TERMINAL=terminal/channel.o terminal/clock.o terminal/decoder.o terminal/term_node_init.o terminal/inline.o terminal/input_sensor.o terminal/media_control.o terminal/media_manager.o terminal/media_memory.o terminal/media_object.o terminal/media_sensor.o terminal/network_service.o terminal/object_browser.o terminal/object_manager.o terminal/terminal.o 
ifeq ($(DISABLE_SVG), no)
LIBGPAC_TERMINAL+=terminal/svg_external.o
endif

## libgpac objects gathering: src/renderer
LIBGPAC_RENDERER=renderer/audio_input.o renderer/audio_mixer.o renderer/audio_render.o renderer/audio_stacks.o renderer/base_textures.o renderer/common_stacks.o renderer/renderer.o renderer/texturing.o 

## libgpac objects gathering: src/laser
LIBGPAC_LASER=
ifeq ($(DISABLE_SVG), no)
LIBGPAC_LASER=laser/lsr_enc.o laser/lsr_dec.o laser/lsr_tables.o
endif

## libgpac objects gathering: la totale ...
OBJS=$(LIBGPAC_UTILS) $(LIBGPAC_MCRYPT) $(LIBGPAC_SCENE) $(LIBGPAC_IETF) $(LIBGPAC_BIFS) $(LIBGPAC_ISOM) $(LIBGPAC_ODF) $(LIBGPAC_MEDIATOOLS) $(LIBGPAC_SCENEMANAGER) $(LIBGPAC_TERMINAL) $(LIBGPAC_RENDERER) $(LIBGPAC_LASER)


## libgpac global compilation and linking options
#1- zlib support
ifeq ($(CONFIG_ZLIB), local)
CFLAGS+= -I$(LOCAL_INC_PATH)/zlib
EXTRALIBS+=-L../extra_lib/lib/gcc
endif
EXTRALIBS+=-lz

#2- ssl support
ifeq ($(HAS_OPENSSL), yes)
EXTRALIBS+=$(SSL_LIBS)
endif

#3- big-endian config
ifeq ($(IS_BIGENDIAN), yes)
CFLAGS+=-DGPAC_BIG_ENDIAN
endif


## libgpac scenegraph compilation and linking options
SCENEGRAPH_CFLAGS=
ifeq ($(CONFIG_JS),no)
else
SCENEGRAPH_CFLAGS+=-DGPAC_HAS_SPIDERMONKEY $(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
EXTRALIBS+= -L../extra_lib/lib/gcc
endif
EXTRALIBS+= -l$(JS_LIBS)
endif

ifeq ($(DISABLE_SVG), yes)
SCENEGRAPH_CFLAGS+=-DGPAC_DISABLE_SVG
endif

## libgpac isomedia compilation options
ISOMEDIA_CFLAGS=
ifeq ($(GPACREADONLY), yes)
ISOMEDIA_CFLAGS=-DGPAC_READ_ONLY
endif
ifeq ($(ISOFRAGMENTS), no)
ISOMEDIA_CFLAGS+= -DGF_ISOM_NO_FRAGMENTS
endif

## libgpac terminal compilation options
TERMINAL_CFLAGS=
ifeq ($(GPACREADONLY), yes)
TERMINAL_CFLAGS+=-DGPAC_READ_ONLY
endif
ifeq ($(DISABLE_SVG), yes)
TERMINAL_CFLAGS+=-DGPAC_DISABLE_SVG
endif

## libgpac renderer compilation options
RENDERER_CFLAGS=
ifeq ($(DISABLE_SVG), yes)
RENDERER_CFLAGS+=-DGPAC_DISABLE_SVG
endif

## libgpac media tools compilation options
MEDIATOOLS_CFLAGS=
ifeq ($(GPACREADONLY), yes)
MEDIATOOLS_CFLAGS=-DGPAC_READ_ONLY
endif

## libgpac scene manager compilation options
SCENEMANAGER_CFLAGS=
ifeq ($(GPACREADONLY), yes)
SCENEMANAGER_CFLAGS=-DGPAC_READ_ONLY
endif
ifeq ($(DISABLE_SVG), yes)
SCENEMANAGER_CFLAGS+=-DGPAC_DISABLE_SVG
endif


##libgpac library output
LIB=libgpac.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
#LDFLAGS+=-export-symbols libgpac.def
else
ifeq ($(CONFIG_DARWIN),yes)
LDFLAGS+=-install_name $(prefix)/lib/$(LIB)
endif
EXTRALIBS+=$(GPAC_SH_FLAGS)
endif

SRCS := $(OBJS:.o=.c) 

all: lib

lib: $(LIB)

#there's a bunch of warnings in there, get rid of them
mcrypt: CFLAGS= $(OPTFLAGS) -w -I$(SRC_PATH)/include
mcrypt: $(LIBGPAC_MCRYPT)

scenegraph: CFLAGS+=$(SCENEGRAPH_CFLAGS)
scenegraph: $(LIBGPAC_SCENE)

isomedia: CFLAGS+=$(ISOMEDIA_CFLAGS)
isomedia: $(LIBGPAC_ISOM)

mediatools: CFLAGS+=$(MEDIATOOLS_CFLAGS)
mediatools: $(LIBGPAC_MEDIATOOLS)

sceneman: CFLAGS+=$(SCENEMANAGER_CFLAGS)
sceneman: $(LIBGPAC_SCENEMANAGER)

term: CFLAGS+=$(TERMINAL_CFLAGS)
term: $(LIBGPAC_TERMINAL)

render: CFLAGS+=$(RENDERER_CFLAGS)
render: $(LIBGPAC_RENDERER)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 

#IPV6
utils/os_net.o: utils/os_net.c
ifeq ($(CONFIG_IPV6), yes)
	$(CC) $(CFLAGS) -DGPAC_IPV6 -c -o $@ $< 
else
	$(CC) $(CFLAGS) -c -o $@ $< 
endif

#OpenSSL
utils/downloader.o: utils/downloader.c
ifeq ($(HAS_OPENSSL), yes)
	$(CC) $(CFLAGS) -DGPAC_HAS_SSL -c -o $@ $< 
else
	$(CC) $(CFLAGS) -c -o $@ $< 
endif

$(LIB): $(LIBGPAC_UTILS) $(LIBGPAC_IETF) $(LIBGPAC_BIFS) $(LIBGPAC_ODF) $(LIBGPAC_LASER) isomedia scenegraph mcrypt mediatools sceneman term render
	ar cr ../bin/gcc/libgpac_static.a $(OBJS)
	ranlib ../bin/gcc/libgpac_static.a
	$(CC) $(SHFLAGS) $(LDFLAGS) -o ../bin/gcc/$@ $(OBJS) $(EXTRALIBS)

dep: depend

depend:
	rm -f .depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

clean: 
	rm -f $(OBJS) $(LIB)
	rm -rf ../bin/gcc/libgpac_static.a

distclean: clean
	rm -f Makefile.bak .depend


# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
