

crypto_unit_test()
{

test_begin "encryption-$1"

if [ $test_skip  = 1 ] ; then
 return
fi

cryptfile="$TEMP_DIR/$1-crypted.mp4"
decryptfile="$TEMP_DIR/$1-decrypted.mp4"

do_test "$MP4BOX -crypt $2 -out $cryptfile $mp4file" "Encrypt"
do_hash_test $cryptfile "crypt"

do_test "$MP4BOX -decrypt $2 -out $decryptfile $cryptfile" "Decrypt"
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

crypto_test_file()
{

for drm in $MEDIA_DIR/encryption/*.xml ; do

#vp9 only supports ctr for now
if [ $1 == "vp9" ] ; then
 case $drm in
 *cbc* )
 	continue ;;
 *adobe* )
 	continue ;;
 *isma* )
 	continue ;;
 *cens* )
 	continue ;;
 *clearbytes* )
 	continue ;;
 *forceclear* )
 	continue ;;
 *clear_stsd* )
 	continue ;;
 esac
elif [ $1 != "avc" ] ; then
 case $drm in
 *clearbytes* )
 	continue ;;
 *forceclear* )
 	continue ;;
 *clear_stsd* )
 	continue ;;
 esac
fi

name=$(basename $drm)
name=${name%%.*}

crypto_unit_test "$name-$1" $drm

done

}

mp4file="$TEMP_DIR/source_media.mp4"

#AVC
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -new $mp4file 2> /dev/null
crypto_test_file "avc"

#AAC
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null
crypto_test_file "aac"

#HEVC
$MP4BOX -add $MEDIA_DIR/auxiliary_files/counter.hvc -new $mp4file 2> /dev/null
crypto_test_file "hevc"

#AV1
$MP4BOX -add $MEDIA_DIR/auxiliary_files/video.av1 -new $mp4file 2> /dev/null
crypto_test_file "av1"

rm -f $mp4file 2> /dev/null


if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
 return
fi

#test encryption of AVC with emul prev byte in non encrypted NAL
test_begin "encryption-avc-ebp"

$MP4BOX -crypt $MEDIA_DIR/encryption/cbcs.xml $EXTERNAL_MEDIA_DIR/misc/avc_sei_epb.mp4 -out $mp4file 2> /dev/null
do_hash_test $mp4file "crypt-avc-epb"
rm -f $mp4file 2> /dev/null

test_end

#AV1 with small tiles less than 16 bytes
$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/obu_tiles4x2_grp4.av1 -new $mp4file 2> /dev/null
crypto_test_file "av1small"

#VP9, we only test CTR
$MP4BOX -add $EXTERNAL_MEDIA_DIR/import/counter_1280_720_I_25_500k.ivf -new $mp4file 2> /dev/null
crypto_test_file "vp9"


