#!/bin/sh

first_run=1

test_swf()
{

 name=$(basename $1)
 name=${name%.*}

test_begin "swf-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

mp4file=$TEMP_DIR/$name-bifs.mp4
#has tests are currently commented since we have rounding issues in quant floaty (swf) to float (bufs or svf) resulting in slightly different encodings/files
do_test "$MP4BOX -mp4 $1 -out $mp4file" "swftobifs"

mp4file=$TEMP_DIR/$name-svg.mp4
do_test "$MP4BOX -add $1 -new $mp4file" "swftosvg-import"

if [ $first_run = 1 ] ; then

mp4file=$TEMP_DIR/$name-bifs-global.mp4
#has tests are currently commented since we have rounding issues in quant floaty (swf) to float (bufs or svf) resulting in slightly different encodings/files
do_test "$MP4BOX -global -no-ctrl -same-app -quad -mp4 $1 -out $mp4file" "swftobifs-global"

#test indexedCurve2D hardcoded proto import and playback
mp4file=$TEMP_DIR/$name-bifs-ic2d.mp4
do_test "$MP4BOX -global -no-ctrl -same-app -ic2d -mp4 $1 -out $mp4file" "swftobifs-ic2d"
dumpfile=$TEMP_DIR/$name-bifs-ic2d.rgb
do_test "$GPAC -i $mp4file compositor:osize=128x128 -o $dumpfile" "swftobifs-ic2d"

svgfile=$TEMP_DIR/$name.svg
#has tests are currently commented since we have rounding issues in quant floaty (swf) to float (bufs or svf) resulting in slightly different encodings/files
do_test "$MP4BOX -svg $1 -out $svgfile" "swftosvg"


fi


test_end
}

for i in $MEDIA_DIR/swf/*.swf ; do
test_swf $i
first_run=0
done

rm $MEDIA_DIR/swf/*.mp3 2>1 > /dev/null
rm $MEDIA_DIR/swf/*.jpg 2>1 > /dev/null
