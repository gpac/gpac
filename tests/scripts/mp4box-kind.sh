#!/bin/sh

test_begin "mp4box-kind"

$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=1 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $TEMP_DIR/file.mp4

do_test "$MP4BOX -kind all=myKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-scheme"
$MP4BOX -info $TEMP_DIR/out.mp4
$MP4BOX -diso $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=myKindScheme $TEMP_DIR/out.mp4" "kind-all-scheme-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=myKindScheme $TEMP_DIR/out.mp4" "kind-all-scheme-rem-twice"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-scheme-value"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-all-scheme-value-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myKindScheme=myKindValue2 $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-scheme-value-multiple-1"
do_test "$MP4BOX -kind all=myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-all-scheme-value-multiple-2"
do_test "$MP4BOX -kind all=myKindScheme $TEMP_DIR/out.mp4" "kind-all-scheme-value-multiple-3"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-all-multiple-rem-1"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=myKindScheme $TEMP_DIR/out.mp4" "kind-all-rem-scheme"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=urn:gpac:kindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-scheme-with-column"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=urn:gpac:kindScheme=myKindValue  $TEMP_DIR/out.mp4" "kind-all-scheme-with-column-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myFirstKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-2-schemes-1"
do_test "$MP4BOX -kind all=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-all-2-schemes-2"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem all=myFirstKindScheme $TEMP_DIR/out.mp4" "kind-all-2-schemes-rem-1"
do_test "$MP4BOX -kind-rem all=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-all-2-schemes-rem-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myFirstKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-same-scheme-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myFirstKindScheme $TEMP_DIR/out.mp4" "kind-all-same-scheme-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myFirstKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-all-same-scheme-value-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind all=myFirstKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-all-same-scheme-value-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-scheme"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem myKindScheme $TEMP_DIR/out.mp4" "kind-scheme-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem myKindScheme $TEMP_DIR/out.mp4" "kind-scheme-rem-twice"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-scheme-value"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-scheme-value-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myKindScheme=myKindValue2 $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-scheme-value-multiple-1"
do_test "$MP4BOX -kind myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-scheme-value-multiple-2"
do_test "$MP4BOX -kind myKindScheme $TEMP_DIR/out.mp4" "kind-scheme-value-multiple-3"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-multiple-rem-1"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem myKindScheme $TEMP_DIR/out.mp4" "kind-rem-scheme"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind urn:gpac:kindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-scheme-with-column"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem urn:gpac:kindScheme=myKindValue  $TEMP_DIR/out.mp4" "kind-scheme-with-column-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myFirstKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-2-schemes-1"
do_test "$MP4BOX -kind mySecondKindScheme $TEMP_DIR/out.mp4" "kind-2-schemes-2"
$MP4BOX -info $TEMP_DIR/out.mp4

do_test "$MP4BOX -kind-rem myFirstKindScheme $TEMP_DIR/out.mp4" "kind-2-schemes-rem-1"
do_test "$MP4BOX -kind-rem mySecondKindScheme $TEMP_DIR/out.mp4" "kind-2-schemes-rem-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myFirstKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-same-scheme-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myFirstKindScheme $TEMP_DIR/out.mp4" "kind-same-scheme-1"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myFirstKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-same-scheme-value-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind myFirstKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-same-scheme-value-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind 1=myKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-track-scheme"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind-rem 1=myKindScheme $TEMP_DIR/out.mp4" "kind-track-scheme-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind 1=myKindScheme=myKindValue $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-track-scheme-value"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind-rem 1=myKindScheme=myKindValue $TEMP_DIR/out.mp4" "kind-track-scheme-value-rem"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind 1=myKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-track-2-schemes-1"
do_test "$MP4BOX -kind 1=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-track-2-schemes-2"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind-rem 1=myKindScheme $TEMP_DIR/out.mp4" "kind-track-2-schemes-rem-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind-rem 1=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-track-2-schemes-rem-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind 1=mySecondKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4" "kind-track-2-schemes-2-tracks-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind 2=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-track-2-schemes-2-tracks-2"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind-rem 1=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-track-2-schemes-2-tracks-rem-1"
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -kind-rem 2=mySecondKindScheme $TEMP_DIR/out.mp4" "kind-track-2-schemes-2-tracks-rem-2"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
rm $TEMP_DIR/out2.mp4
$MP4BOX -kind mySecondKindScheme $TEMP_DIR/file.mp4 -out $TEMP_DIR/out.mp4 
$MP4BOX -info $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/out.mp4 $TEMP_DIR/out2.mp4" "kind-import-mp4"
$MP4BOX -info $TEMP_DIR/out2.mp4
rm $TEMP_DIR/out2.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme -new $TEMP_DIR/out.mp4" "kind-import-track-scheme"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue -new $TEMP_DIR/out.mp4" "kind-import-track-scheme-value"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=urn:gpac:kindScheme=myKindValue -new $TEMP_DIR/out.mp4" "kind-import-track-scheme-value-column"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue:kind=myKindScheme=myKindValue2 -new $TEMP_DIR/out.mp4" "kind-import-track-2-schemes-same-kind"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue:kind=myKindScheme2=myKindValue2 -new $TEMP_DIR/out.mp4" "kind-import-track-2-schemes-same-kind"
$MP4BOX -info $TEMP_DIR/out.mp4

rm $TEMP_DIR/out.mp4
do_test "$MP4BOX -add $TEMP_DIR/file.mp4#1:kind=myKindScheme=myKindValue:kind=myKindScheme=myKindValue -new $TEMP_DIR/out.mp4" "kind-import-track-2-kinds-same"
$MP4BOX -info $TEMP_DIR/out.mp4

#### TODO
## concat of same kinds, of different kinds
## fragmentation/segmentation preserves kinds
## dash uses kinds

rm $TEMP_DIR/file.mp4
rm $TEMP_DIR/out.mp4

test_end