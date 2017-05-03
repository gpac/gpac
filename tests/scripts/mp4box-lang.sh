#!/bin/sh

output=$TEMP_DIR/output.mp4

my_test_with_hash ()
{
	do_test "$1" $2
	do_hash_test $output $2

	do_test "$MP4BOX -info $output" "$2-info"
	do_test "$MP4BOX -diso $output -out $TEMP_DIR/out_info.xml" "$2-diso"
	do_hash_test $TEMP_DIR/out_info.xml "$2-diso"
}


test_begin "mp4box-lang"

 if [ $test_skip  = 1 ] ; then
  return
 fi

$MP4BOX -add $MEDIA_DIR/auxiliary_files/subtitle.srt:dur=1 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $TEMP_DIR/file.mp4 2> /dev/null

my_test_with_hash "$MP4BOX -lang all=en $TEMP_DIR/file.mp4 -out $output" "all-2-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang all=eng $TEMP_DIR/file.mp4 -out $output" "all-3-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang all=en-US $TEMP_DIR/file.mp4 -out $output" "all-2-2-char"
my_test_with_hash "$MP4BOX -lang all=fr-FR $TEMP_DIR/file.mp4 -out $output" "all-twice"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang en $TEMP_DIR/file.mp4 -out $output" "2-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang eng $TEMP_DIR/file.mp4 -out $output" "3-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang en-US $TEMP_DIR/file.mp4 -out $output" "2-2-char"
my_test_with_hash "$MP4BOX -add $output -new $TEMP_DIR/out2.mp4" "copy"
rm $TEMP_DIR/out2.mp4 2> /dev/null

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang 1=en $TEMP_DIR/file.mp4 -out $output" "track-2-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang 1=eng $TEMP_DIR/file.mp4 -out $output" "track-3-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -lang 1=en-US $TEMP_DIR/file.mp4 -out $output" "track-2-2-char"
my_test_with_hash "$MP4BOX -lang 2=en-US $output" "second-track"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=en -new $output" "param-2-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=eng -new $output" "param-3-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=en-US -new $output" "param-2-2-char"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:lang=fr-FR:lang=en-US -new $output" "param-twice"

#### TODO
## concat of same language, of different language
## fragmentation/segmentation preserves language
## dash uses new language

rm $TEMP_DIR/file.mp4 2> /dev/null
rm $output 2> /dev/null
rm $TEMP_DIR/out_info.xml 2> /dev/null

test_end