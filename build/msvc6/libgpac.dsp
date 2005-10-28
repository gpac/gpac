# Microsoft Developer Studio Project File - Name="libgpac" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libgpac - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libgpac.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libgpac.mak" CFG="libgpac - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libgpac - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libgpac - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libgpac - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "obj/libgpac_rel"
# PROP Intermediate_Dir "obj/libgpac_rel"
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /I "../../extra_lib/include/zlib" /I "../../extra_lib/include/js" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "GPAC_HAS_SPIDERMONKEY" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../bin/w32_rel/libgpac_static.lib"

!ELSEIF  "$(CFG)" == "libgpac - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "obj/libgpac_deb"
# PROP Intermediate_Dir "obj/libgpac_deb"
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../../extra_lib/include/zlib" /I "../../extra_lib/include/js" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "GPAC_HAS_SPIDERMONKEY" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../bin/w32_deb/libgpac_static.lib"

!ENDIF 

# Begin Target

# Name "libgpac - Win32 Release"
# Name "libgpac - Win32 Debug"
# Begin Group "utils"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\utils\base_encoding.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\bitstream.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\color.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\configfile.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\downloader.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\error.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\list.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\math.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\module.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\module_wrap.h
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\w32\os_divers.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\w32\os_module.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\w32\os_net.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\w32\os_thread.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\path2d.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\path2d_stroker.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\token.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\url.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\utf.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils\xml_parser.c
# End Source File
# End Group
# Begin Group "odf"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\odf\desc_private.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\descriptors.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\ipmpx_code.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\ipmpx_dump.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\ipmpx_parse.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\oci_codec.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\odf_code.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\odf_codec.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\odf_command.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\odf_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\odf_dump.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\odf_parse.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\qos.c
# End Source File
# Begin Source File

SOURCE=..\..\src\odf\slc.c
# End Source File
# End Group
# Begin Group "bifs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\bifs\arith_decoder.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\bifs_codec.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\bifs_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\bifs_node_tables.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\bifs_tables.h
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\com_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\com_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\conditional.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\field_decode.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\field_encode.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\memory_decoder.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\predictive_mffield.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\quant.h
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\quantize.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\script.h
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\script_dec.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\script_enc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bifs\unquantize.c
# End Source File
# End Group
# Begin Group "isomedia"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\isomedia\avc_ext.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\box_code_3gpp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\box_code_base.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\box_code_isma.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\box_code_meta.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\box_dump.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\box_funcs.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\data_map.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\hint_track.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\hinting.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\isma_sample.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\isom_intern.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\isom_read.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\isom_store.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\isom_write.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\isomedia_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\media.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\media_odf.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\meta.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\movie_fragments.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\sample_descs.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\stbl_read.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\stbl_write.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\track.c
# End Source File
# Begin Source File

SOURCE=..\..\src\isomedia\tx3g.c
# End Source File
# End Group
# Begin Group "ietf"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\gpac\internal\ietf_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtcp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtp_packetizer.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtp_pck_3gpp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtp_pck_mpeg12.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtp_pck_mpeg4.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtsp_command.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtsp_common.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtsp_response.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\rtsp_session.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ietf\sdp.c
# End Source File
# End Group
# Begin Group "scenegraph"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\scenegraph\base_scenegraph.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\commands.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\mpeg4_animators.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\mpeg4_nodes.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\mpeg4_valuator.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\scenegraph_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\svg_nodes.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\svg_smjs.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\svg_tools.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\svg_types.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\vrml_interpolators.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\vrml_proto.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\vrml_route.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\vrml_script.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\vrml_smjs.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\vrml_tools.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scenegraph\x3d_nodes.c
# End Source File
# End Group
# Begin Group "media_tools"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\media_tools\av_parsers.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\avilib.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\avilib.h
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\gpac_ogg.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\ismacryp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\isom_hinter.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\isom_tools.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\media_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\media_export.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\media_import.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\mpeg2_ps.c
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\mpeg2_ps.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\ogg.h
# End Source File
# Begin Source File

SOURCE=..\..\src\media_tools\text_import.c
# End Source File
# End Group
# Begin Group "scene_manager"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\scene_manager\encode_cbk.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\encode_isom.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\loader_bt.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\loader_isom.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\loader_qt.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\loader_xmta.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\scene_dump.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\scene_manager.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\scene_stats.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\swf_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\swf_parse.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\swf_shape.c
# End Source File
# Begin Source File

SOURCE=..\..\src\scene_manager\text_to_bifs.c
# End Source File
# End Group
# Begin Group "mcrypt"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\mcrypt\cbc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\cfb.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\crypt_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\ctr.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\des.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\ecb.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\g_crypt.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\ncfb.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\nofb.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\ofb.c
# End Source File
# Begin Source File

SOURCE="..\..\src\mcrypt\rijndael-128.c"
# End Source File
# Begin Source File

SOURCE="..\..\src\mcrypt\rijndael-192.c"
# End Source File
# Begin Source File

SOURCE="..\..\src\mcrypt\rijndael-256.c"
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\stream.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mcrypt\tripledes.c
# End Source File
# End Group
# Begin Group "terminal"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\terminal\channel.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\clock.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\decoder.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\inline.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\input_sensor.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\input_sensor.h
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_control.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_control.h
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_manager.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_memory.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_memory.h
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_object.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\media_sensor.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\network_service.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\object_browser.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\object_manager.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\term_node_init.c
# End Source File
# Begin Source File

SOURCE=..\..\src\terminal\terminal.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\terminal_dev.h
# End Source File
# End Group
# Begin Group "renderer"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\renderer\audio_input.c
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\audio_mixer.c
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\audio_render.c
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\audio_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\base_textures.c
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\common_stacks.c
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\common_stacks.h
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\renderer.c
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\internal\renderer_dev.h
# End Source File
# Begin Source File

SOURCE=..\..\src\renderer\texturing.c
# End Source File
# End Group
# Begin Group "include"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\gpac\modules\audio_out.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\avparse.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\base_coding.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\bifs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\bifsengine.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\bitstream.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\modules\codec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\color.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\config.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\constants.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\crypt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\download.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\modules\font.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\ietf.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\ismacryp.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\isomedia.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\list.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\math.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\media_tools.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\mediaobject.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\module.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\mpeg4_odf.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\network.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\nodes_mpeg4.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\nodes_svg.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\nodes_x3d.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\options.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\path2d.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\modules\raster2d.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\renderer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\scene_manager.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\scenegraph.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\scenegraph_svg.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\scenegraph_vrml.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\modules\service.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\setup.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\sync_layer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\term_info.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\terminal.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\thread.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\token.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\tools.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\user.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\utf.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\modules\video_out.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\xml.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gpac\yuv.h
# End Source File
# End Group
# End Target
# End Project
