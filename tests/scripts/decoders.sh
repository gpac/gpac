#!/bin/sh

test_decoder()
{

test_begin "decoder-$1"

if [ $test_skip  = 1 ] ; then
return
fi

mp4_file=$TEMP_DIR/in.mp4
src_file=$2
dst_file=$TEMP_DIR/$3

#only test for 1 sec
$MP4BOX -add $src_file:dur=0.4 -new $mp4_file 2> /dev/null

do_test "$GPAC -i $mp4_file -o $dst_file -graph -stats $4"  "decoder"

#some decoder output tests are disabled due to non uniform output on different platform
#this will need further investigations
if [ $5  = 0 ] ; then
do_hash_test "$dst_file" "decoder"
fi

if [ ! -f $dst_file ] ; then
result="Decoding output not present"
fi

test_end

}

nvdec=`$GPAC -h filters 2>&1 | grep nvdec`
vtbdec=`$GPAC -h filters 2>&1 | grep vtbdec`
ohevcdec=`$GPAC -h filters 2>&1 | grep ohevc`
xviddec=`$GPAC -h filters 2>&1 | grep xvid`
libaom=`gpac -hh ffdec:* 2>&1 | grep ffdec:libaom-av1`
j2koj2k=`$GPAC -h filters 2>&1 | grep j2kdec`
j2kff=`gpac -hh ffdec:* 2>&1 | grep ffdec:jpeg2000`


#test png+alpha decode to raw
test_decoder "png-imgdec" $MEDIA_DIR/auxiliary_files/logo.png "test.rgb" "-blacklist=ffdec" 0
test_decoder "png-ffdec" $MEDIA_DIR/auxiliary_files/logo.png "test.rgb" "-blacklist=imgdec" 0

#test jpg decode to raw
test_decoder "jpg-imgdec" $MEDIA_DIR/auxiliary_files/logo.jpg "test.rgb" "-blacklist=ffdec" 0
test_decoder "jpg-ffdec" $MEDIA_DIR/auxiliary_files/logo.jpg "test.rgb" "-blacklist=imgdec" 0
test_decoder "jpg-bmp" $MEDIA_DIR/auxiliary_files/logo.jpg "test.bmp" "-blacklist=ffdec" 0

#test aac decode to raw
test_decoder "aac-faad" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.pcm" "-blacklist=ffdec" 1
test_decoder "aac-ffdec" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.pcm" "-blacklist=faad" 1

#test mp3 decode to raw
test_decoder "mp3-maddec" $MEDIA_DIR/auxiliary_files/count_english.mp3 "test.pcm" "-blacklist=ffdec" 1
test_decoder "mp3-ffdec" $MEDIA_DIR/auxiliary_files/count_english.mp3 "test.pcm" "-blacklist=maddec" 0

#test mp3 decode to wav
test_decoder "mp3-wav" $MEDIA_DIR/auxiliary_files/count_english.mp3 "test.wav" "-blacklist=maddec" 0

#test h264 decode to raw using ffmpeg
test_decoder "avc-ffdec" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.yuv" "-blacklist=vtbdec,nvdec,ohevcdec" 0

#test h264 decode to raw using openhevc
if [ -n "$ohevcdec" ] ; then
test_decoder "avc-ohevc" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.yuv" "-blacklist=vtbdec,nvdec,ffdec" 1
fi

if [ -n "$vtbdec" ] ; then
test_decoder "avc-vtb" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.yuv" "-blacklist=ohevcdec,nvdec,ffdec" 0
fi

if [ -n "$nvdec" ] ; then
test_decoder "avc-nvdec" $MEDIA_DIR/auxiliary_files/enst_video.h264 "test.yuv" "-blacklist=ohevcdec,vtbdec,ffdec" 0
fi

#test hevc decode to raw using ffmpeg
test_decoder "hevc-ffdec" $MEDIA_DIR/auxiliary_files/counter.hvc "test.yuv" "-blacklist=vtbdec,nvdec,ohevcdec" 0

#test hevc decode to raw using openhevc
if [ -n "$ohevcdec" ] ; then
test_decoder "hevc-ohevc" $MEDIA_DIR/auxiliary_files/counter.hvc "test.yuv" "-blacklist=vtbdec,nvdec,ffdec" 0
fi

#latest OSX releases breaks decoding of our counter sequence !! Commented for now until we find a fix
#if [ -n "$vtbdec" ] ; then
#test_decoder "hevc-vtb" $MEDIA_DIR/auxiliary_files/counter.hvc "test.yuv" "-blacklist=ohevcdec,nvdec,ffdec" 0
#fi

#if [ -n "$nvdec" ] ; then
#test_decoder "hevc-nvdec" $MEDIA_DIR/auxiliary_files/counter.hvc "test.yuv" "-blacklist=ohevcdec,vtbdec,ffdec" 0
#fi

#test av1
if [ -n "$libaom" ] ; then
test_decoder "av1" $MEDIA_DIR/auxiliary_files/video.av1 "test.yuv" "" 0
fi

 if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
 fi

test_decoder "amr-ffdec" $EXTERNAL_MEDIA_DIR/import/bear_audio.amr "test.pcm" "" 1

test_decoder "amrwb-ffdec" $EXTERNAL_MEDIA_DIR/import/obrother_wideband.amr "test.pcm" "" 1

test_decoder "h263-ffdec" $EXTERNAL_MEDIA_DIR/import/bear_video.263 "test.yuv" "-blacklist=vtbdec" 0
if [ -n "$vtbdec" ] ; then
test_decoder "h263-vtb" $EXTERNAL_MEDIA_DIR/import/bear_video.263 "test.yuv" "-blacklist=ffdec" 0
fi

test_decoder "qcp-ffdec" $EXTERNAL_MEDIA_DIR/import/count_english.qcp "test.pcm" "" 1

test_decoder "m1v-ffdec" $EXTERNAL_MEDIA_DIR/import/dead.m1v "test.yuv" "" 1

if [ -n "$j2koj2k" ] ; then
test_decoder "j2k-oj2k" $EXTERNAL_MEDIA_DIR/import/logo.jp2 "test.yuv" "-blacklist=ffdec" 0
test_decoder "mjp2-oj2k" $EXTERNAL_MEDIA_DIR/import/speedway.mj2 "test.yuv" "-blacklist=ffdec" 0
fi

if [ -n "$j2kff" ] ; then
test_decoder "j2k-ff" $EXTERNAL_MEDIA_DIR/import/logo.jp2 "test.yuv" "-blacklist=j2kdec" 0
test_decoder "mjp2-ff" $EXTERNAL_MEDIA_DIR/import/speedway.mj2 "test.yuv" "-blacklist=j2kdec" 0
fi

test_decoder "ac3-a52" $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.ac3 "test.pcm" "-blacklist=ffdec" 1
test_decoder "ac3-ff" $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.ac3 "test.pcm" "-blacklist=a52dec" 0

test_decoder "vorbis" $EXTERNAL_MEDIA_DIR/import/dead_ogg.ogg "test.pcm" "-blacklist=ffdec" 0

test_decoder "theora" $EXTERNAL_MEDIA_DIR/import/dead_ogg.ogg "test.yuv" "-blacklist=ffdec" 0

#test aac multichannel decode to raw
test_decoder "aac-faad-mc" $EXTERNAL_MEDIA_DIR/import/aac_vbr_51_128k.aac "test.pcm" "-blacklist=ffdec" 1

test_decoder "flac-ffdec" $EXTERNAL_MEDIA_DIR/import/enst_audio.flac "test.pcm" "" 0
