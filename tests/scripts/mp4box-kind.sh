#!/bin/sh

output=$TEMP_DIR/output.mp4

my_test_with_hash ()
{
	do_test "$1" $2
	do_hash_test $output $2
}


test_begin "mp4box-kind"

 if [ $test_skip  = 1 ] ; then
  return
 fi

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $TEMP_DIR/file.mp4 2> /dev/null

my_test_with_hash "$MP4BOX -kind all=myKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-all-scheme" $output
$MP4BOX -info $output 2> /dev/null
$MP4BOX -diso $output 2> /dev/null

my_test_with_hash "$MP4BOX -kind-rem all=myKindScheme $output" "kind-all-scheme-rem"

my_test_with_hash "$MP4BOX -kind-rem all=myKindScheme $output" "kind-all-scheme-rem-twice"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind all=myKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output" "kind-all-scheme-value"

my_test_with_hash "$MP4BOX -kind-rem all=myKindScheme=myKindValue $output" "kind-all-scheme-value-rem"

rm $output 2> /dev/null
do_test "$MP4BOX -kind all=myKindScheme=myKindValue2 $TEMP_DIR/file.mp4 -out $output" "kind-all-scheme-value-multiple-1"
do_test "$MP4BOX -kind all=myKindScheme=myKindValue $output" "kind-all-scheme-value-multiple-2"
my_test_with_hash "$MP4BOX -kind all=myKindScheme $output" "kind-all-scheme-value-multiple-3"

my_test_with_hash "$MP4BOX -kind-rem all=myKindScheme=myKindValue $output" "kind-all-multiple-rem-1"

my_test_with_hash "$MP4BOX -kind-rem all=myKindScheme $output" "kind-all-rem-scheme"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind all=urn:gpac:kindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output" "kind-all-scheme-with-column"

my_test_with_hash "$MP4BOX -kind-rem all=urn:gpac:kindScheme=myKindValue  $output" "kind-all-scheme-with-column-rem"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind all=myFirstKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-all-2-schemes-1"
my_test_with_hash "$MP4BOX -kind all=mySecondKindScheme $output" "kind-all-2-schemes-2"

my_test_with_hash "$MP4BOX -kind-rem all=myFirstKindScheme $output" "kind-all-2-schemes-rem-1"
my_test_with_hash "$MP4BOX -kind-rem all=mySecondKindScheme $output" "kind-all-2-schemes-rem-2"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind all=myFirstKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-all-same-scheme-1"
my_test_with_hash "$MP4BOX -kind all=myFirstKindScheme $output" "kind-all-same-scheme-2"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind all=myFirstKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output" "kind-all-same-scheme-value-1"
my_test_with_hash "$MP4BOX -kind all=myFirstKindScheme=myKindValue $output" "kind-all-same-scheme-value-2"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind myKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-scheme"

my_test_with_hash "$MP4BOX -kind-rem myKindScheme $output" "kind-scheme-rem"

my_test_with_hash "$MP4BOX -kind-rem myKindScheme $output" "kind-scheme-rem-twice"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind myKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output" "kind-scheme-value"

my_test_with_hash "$MP4BOX -kind-rem myKindScheme=myKindValue $output" "kind-scheme-value-rem"

rm $output 2> /dev/null
do_test "$MP4BOX -kind myKindScheme=myKindValue2 $TEMP_DIR/file.mp4 -out $output" "kind-scheme-value-multiple-1"
do_test "$MP4BOX -kind myKindScheme=myKindValue $output" "kind-scheme-value-multiple-2"
my_test_with_hash "$MP4BOX -kind myKindScheme $output" "kind-scheme-value-multiple-3"

my_test_with_hash "$MP4BOX -kind-rem myKindScheme=myKindValue $output" "kind-multiple-rem-1"

my_test_with_hash "$MP4BOX -kind-rem myKindScheme $output" "kind-rem-scheme"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind urn:gpac:kindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output"	

my_test_with_hash "$MP4BOX -kind-rem urn:gpac:kindScheme=myKindValue  $output" "kind-scheme-with-column-rem"

rm $output 2> /dev/null
do_test "$MP4BOX -kind myFirstKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-2-schemes-1"
my_test_with_hash "$MP4BOX -kind mySecondKindScheme $output" "kind-2-schemes-2"

do_test "$MP4BOX -kind-rem myFirstKindScheme $output" "kind-2-schemes-rem-1"
my_test_with_hash "$MP4BOX -kind-rem mySecondKindScheme $output" "kind-2-schemes-rem-2"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind myFirstKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-same-scheme-1"
my_test_with_hash "$MP4BOX -kind myFirstKindScheme $output" "kind-same-scheme-1"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind myFirstKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output" "kind-same-scheme-value-1"
my_test_with_hash "$MP4BOX -kind myFirstKindScheme=myKindValue $output" "kind-same-scheme-value-2"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind 1=myKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-track-scheme"
my_test_with_hash "$MP4BOX -kind-rem 1=myKindScheme $output" "kind-track-scheme-rem"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind 1=myKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $output" "kind-track-scheme-value"
my_test_with_hash "$MP4BOX -kind-rem 1=myKindScheme=myKindValue $output" "kind-track-scheme-value-rem"

rm $output 2> /dev/null
do_test "$MP4BOX -kind 1=myKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-track-2-schemes-1"
my_test_with_hash "$MP4BOX -kind 1=mySecondKindScheme $output" "kind-track-2-schemes-2"
my_test_with_hash "$MP4BOX -kind-rem 1=myKindScheme $output" "kind-track-2-schemes-rem-1"
my_test_with_hash "$MP4BOX -kind-rem 1=mySecondKindScheme $output" "kind-track-2-schemes-rem-2"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -kind 1=mySecondKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-track-2-schemes-2-tracks-1"
my_test_with_hash "$MP4BOX -kind 2=mySecondKindScheme $output" "kind-track-2-schemes-2-tracks-2"
my_test_with_hash "$MP4BOX -kind-rem 1=mySecondKindScheme $output" "kind-track-2-schemes-2-tracks-rem-1"
my_test_with_hash "$MP4BOX -kind-rem 2=mySecondKindScheme $output" "kind-track-2-schemes-2-tracks-rem-2"

rm $output 2> /dev/null
rm $TEMP_DIR/out2.mp4 2> /dev/null
my_test_with_hash "$MP4BOX -kind mySecondKindScheme $TEMP_DIR/file.mp4 -out $output" "kind-new-output"

my_test_with_hash "$MP4BOX -add $output $TEMP_DIR/out2.mp4" "kind-import-mp4"
rm $TEMP_DIR/out2.mp4 2> /dev/null

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme -new $output" "kind-import-track-scheme"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue -new $output" "kind-import-track-scheme-value"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=urn:gpac:kindScheme=myKindValue -new $output" "kind-import-track-scheme-value-column"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue:kind=myKindScheme=myKindValue2 -new $output" "kind-import-track-2-schemes-same-kind"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue:kind=myKindScheme2=myKindValue2 -new $output" "kind-import-track-2-schemes-same-kind"

rm $output 2> /dev/null
my_test_with_hash "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue:kind=myKindScheme=myKindValue -new $output" "kind-import-track-2-kinds-same"

#### TODO
## concat of same kinds, of different kinds
## fragmentation/segmentation preserves kinds
## dash uses kinds

rm $TEMP_DIR/file.mp4 2> /dev/null
rm $output 2> /dev/null

test_end

