
#create our test file
mp4file="$TEMP_DIR/source_media.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


crypto_test()
{

test_begin "encryption-$1"

if [ $test_skip  = 1 ] ; then
 return
fi

cryptfile="$TEMP_DIR/$1-crypted.mp4"
decryptfile="$TEMP_DIR/$1-decrypted.mp4"

do_test "$MP4BOX -crypt $2 -out $cryptfile $mp4file" "Encrypt"
do_hash_test $cryptfile "crypt"

do_test "$MP4BOX -decrypt $2 -out $decryptfile $mp4file" "Decrypt"
do_hash_test $decryptfile "decrypt"

#compare hashes of source and decrypted
do_compare_file_hashes $mp4file $decryptfile
rv=$?

if [ $rv != 0 ] ; then
result="Hash is not the same between source content and decrypted content"
fi

#do dash test
do_test "$MP4BOX -dash 4000 -profile live -out $TEMP_DIR/test.mpd $cryptfile" "DASH"

dashfile="$TEMP_DIR/$1-crypted_dashinit.mp4"
do_hash_test $dashfile "crypt-dash-init"

dashfile="$TEMP_DIR/$1-crypted_dash1.m4s"
do_hash_test $dashfile "crypt-dash-seg"

#playback test of files for which we can retrieve the key
if [ $1 != "adobe" ] ; then
do_playback_test "$cryptfile" "play"
fi

test_end

}

for drm in $MEDIA_DIR/encryption/*.xml ; do

name=$(basename $drm)
name=${name%%.*}

crypto_test "$name" $drm

done

rm -f $mp4file 2> /dev/null
