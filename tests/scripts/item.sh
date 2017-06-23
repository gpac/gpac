#!/bin/sh

mp4file=$TEMP_DIR/test.mp4

test_begin "meta"

do_test "$MP4BOX -set-meta Metadata:id=0 $EXTERNAL_MEDIA_DIR/item/counter_noItems1.mp4 -out $mp4file" "set0"
do_hash_test $mp4file "set0"
do_test "$MP4BOX -set-meta Metadata:id=1 $EXTERNAL_MEDIA_DIR/item/counter_noItems2.mp4 -out $mp4file" "set1"
do_hash_test $mp4file "set1"
do_test "$MP4BOX -set-meta Metadata $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -out $mp4file" "set2"
do_hash_test $mp4file "set2"


do_test "$MP4BOX -add-item $EXTERNAL_MEDIA_DIR/item/file.html:mime=html:id=1 -add-item $EXTERNAL_MEDIA_DIR/item/file.css:mime=css:id=2 -add-item $EXTERNAL_MEDIA_DIR/item/file.js:mime=javascript:id=3 -add-item $EXTERNAL_MEDIA_DIR/item/file.svg:mime=svg:id=4 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -out $mp4file" "add-items"
do_hash_test $mp4file "add-items"

do_test "$MP4BOX -set-primary id=2 $mp4file" "set-primary"
do_hash_test $mp4file "mp4box-set-primary"

do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 1:path=$TEMP_DIR/file1.html" "dump-item1"
do_hash_test $TEMP_DIR/file1.html "dump-item1"
do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 2:path=$TEMP_DIR/file1.css" "dump-item2"
do_hash_test $TEMP_DIR/file1.css "dump-item2"
do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 3:path=$TEMP_DIR/file1.js" "dump-item3"
do_hash_test $TEMP_DIR/file1.js "dump-item3"
do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 4:path=$TEMP_DIR/file1.svg" "dump-item4"
do_hash_test $TEMP_DIR/file1.svg "dump-item4"


do_test "$MP4BOX -rem-item 1 $mp4file" "remove-item1"
do_hash_test $mp4file "remove-item1"
do_test "$MP4BOX -rem-item 2 $mp4file" "remove-item2"
do_hash_test $mp4file "remove-item2"
do_test "$MP4BOX -rem-item 3 $mp4file" "remove-item3"
do_hash_test $mp4file "remove-item3"
do_test "$MP4BOX -rem-item 4 $mp4file" "remove-item4"
do_hash_test $mp4file "remove-item4"

do_test "$MP4BOX -set-meta XML -set-xml $EXTERNAL_MEDIA_DIR/item/file.xml:id=1 $mp4file" "set-meta-XML-data"
do_hash_test $mp4file "set-meta-XML-data"

do_test "$MP4BOX -dump-xml $TEMP_DIR/file1.xml:id=1 $mp4file" "dump-meta-XML-data"
do_hash_test $mp4file "dump-meta-XML-data"

do_test "$MP4BOX -rem-xml id=1 $mp4file" "remove-meta-XML-data"
do_hash_test $mp4file "remove-meta-XML-data"

test_end
