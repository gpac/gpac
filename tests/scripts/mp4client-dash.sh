
DASH_REF_TPT="http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech"

# MP4Client live profile dash playback
single_playback_test "$DASH_REF_TPT/mp4-live/mp4-live-mpd-AV-BS.mpd" "mp4client-dash-isobmf-live-bs"
single_playback_test "$DASH_REF_TPT/mp4-live-mux/mp4-live-mux-mpd-AV-NBS.mpd" "mp4client-dash-isobmf-live-bs-mux"

single_playback_test "$DASH_REF_TPT//mp4-live-periods/mp4-live-periods-mpd.mpd" "mp4client-dash-isobmf-live-periods"
single_playback_test "$DASH_REF_TPT//mp4-live-periods/mp4-live-periods-reversed-mpd.mpd " "mp4client-dash-isobmf-live-periods-reversed"

# MP4Client onDemand profile dash playback
single_playback_test "$DASH_REF_TPT/mp4-onDemand/mp4-onDemand-mpd-AV.mpd" "mp4client-dash-isobmf-ondemand"

# MP4Client main profile dash playback
single_playback_test "$DASH_REF_TPT/mp4-main-single/mp4-main-single-mpd-AV-NBS.mpd" "mp4client-dash-isobmf-main-single"
single_playback_test "$DASH_REF_TPT/mp4-main-multi/mp4-main-multi-mpd-AV-NBS.mpd" "mp4client-dash-isobmf-main-multi"

#SRD tests
single_playback_test "$DASH_REF_TPT/SRD/mp4-live/srd-3x3.mpd" "mp4client-dash-isobmf-live-srd"

#todo: add other SRD tests ...

#todo: add dash-if sequences


#baseURL tests

single_playback_test "http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/resources/media/dash/base_url_switch/src_baseurl.mpd" "mp4client-dash-baseurl-http"

if [ $EXTERNAL_MEDIA_AVAILABLE != 0 ] ; then
single_playback_test "$EXTERNAL_MEDIA_DIR/dash/base_url_switch/src_baseurl.mpd" "mp4client-dash-baseurl-local"
fi

