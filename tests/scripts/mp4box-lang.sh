#!/bin/sh
test_begin "mp4box-lang"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt:dur=1 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $TEMP_DIR/file.mp4

do_test "$MP4BOX -lang all=en $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "all-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"
do_test "$MP4BOX -diso $TEMP_DIR/out.mp4 -out $TEMP_DIR/out_info.xml"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang all=eng $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "all-3-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang all=en-US $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "all-2-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang all=en-US $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"
do_test "$MP4BOX -lang all=fr-FR $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "all-twice"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang en $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang eng $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "3-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang en-US $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "2-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang 1=en $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "track-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"
do_test "$MP4BOX -diso $TEMP_DIR/out.mp4 -out $TEMP_DIR/out_info.xml"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang 1=eng $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "track-3-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang 1=en-US $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "track-2-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang 1=en-US $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"
do_test "$MP4BOX -lang 2=en-US $TEMP_DIR/out.mp4" "second-track"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -lang en-US $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" 
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"
do_test "$MP4BOX -add $TEMP_DIR/out.mp4 -new $TEMP_DIR/out2.mp4" "copy"
do_test "$MP4BOX -info $TEMP_DIR/out2.mp4"
rm $TEMP_DIR/out2.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=en -new $TEMP_DIR/out.mp4" "param-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=eng -new $TEMP_DIR/out.mp4" "param-3-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=en-US -new $TEMP_DIR/out.mp4" "param-2-2-char"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=fr-FR:lang=en-US -new $TEMP_DIR/out.mp4" "param-twice"
do_test "$MP4BOX -info $TEMP_DIR/out.mp4"

#### TODO
## concat of same language, of different language
## fragmentation/segmentation preserves language
## dash uses new language

rm $TEMP_DIR/file.mp4
rm $TEMP_DIR/out.mp4
rm $TEMP_DIR/out_info.xml

test_end