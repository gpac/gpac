ase_test()
{

test_begin "ase-$1"

if [ $test_skip  = 1 ] ; then
 return
fi

mp4file="$TEMP_DIR/$1.mp4"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/aac_vbr_51_128k.aac:asemode=$1 -new $mp4file" "import"
do_hash_test $mp4file "import"
$MP4BOX -diso $mp4file

test_end

}


ase_test "v0-bs"
ase_test "v0-2"
ase_test "v1"
ase_test "v1-qt"

