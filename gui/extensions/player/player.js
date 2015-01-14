
extension = {
    GF_STATE_PLAY: 0,
    GF_STATE_PAUSE: 1,
    GF_STATE_STOP: 2,
    GF_STATE_TRICK: 3,


    movie: null,
    movie_control: null,
    movie_sensor: null,
    UPnP_Enabled: false,
    dynamic_scene: false,
    movie_connected: false,
    controlled_renderer: null,
    current_url: null,
    local_url: false,
    initial_play_done: false,
    dictionary: null,
    icon_pause: 1,
    icon_play: 0,
    controler: null,
    stat_wnd: null,
    buffer_wnd: null,
    current_time: 0,
    duration: 0,
    timeshift_depth: 0,
    muted: 0,
    file_open_dlg: false,
    stoped_url: null,
    initial_service_id: 0,
    default_addon: null,
    history: [],
    bookmarks: [],
    root_odm: null,
    navigation_wnd: null,
    initial_loop: false,
    initial_speed: 1,
    initial_start: 0,
    show_stats_init: 0,
    services: [],
    channels_wnd: null,

    do_show_controler: function () {
        if (this.file_open_dlg) {
            alert('Cannot show - File dialog is open');
            return false;
        }
        gw_hide_dock();
        if (this.controler.visible) {
            this.controler.hide();
        } else {
            this.controler.show();
        }
    },

    ext_filter_event: function (evt) {
        switch (evt.type) {
            case GF_EVENT_MOUSEUP:
                this.do_show_controler();
                return 1;
            case GF_EVENT_KEYUP:
                if (evt.keycode == gwskin.keys.close) {
                    this.do_show_controler();
                    return 1;
                }
                return 0;
            case GF_EVENT_QUALITY_SWITCHED:
                if (this.stat_wnd) {
                    this.stat_wnd.quality_changed();
                }
                return 1;
            case GF_EVENT_TIMESHIFT_UPDATE:
                if (this.timeshift_depth) {
                    this.set_time(0);
                }
                return 1;
            case GF_EVENT_TIMESHIFT_OVERFLOW:
                if (this.state == this.GF_STATE_PAUSE) {
                    this.set_state(this.GF_STATE_PLAY);
                    var msg = gw_new_message(null, 'Timeshift Overflow', 'Falling outside of timeshift buffer: resuming playback');
                    msg.set_size(380, gwskin.default_icon_height + 2 * gwskin.default_text_font_size);
                    msg.show();
                }
                return 1;
            case GF_EVENT_TIMESHIFT_UNDERRUN:
                if (this.movie_control.mediaSpeed > 1) {
                    this.set_speed(1);
                    var msg = gw_new_message(null, 'Timeshift Underrun', 'Resuming to normal speed');
                    msg.set_size(380, gwskin.default_icon_height + 2 * gwskin.default_text_font_size);
                    msg.show();
                }
                return 1;
            case GF_EVENT_NAVIGATE:
                this.set_movie_url(evt.target_url);
                return 1;
        }
    },

    init_extension: function () {

        /*create media nodes element for playback*/
        var a_movie = new SFNode('OrderedGroup');
        this.movie = new SFNode('Transform2D');
        a_movie.children[0] = this.movie;
        a_movie.children[0].children[0] = new SFNode('Inline');
        a_movie.children[0].children[0].url[0] = '';
        this.movie_control = new SFNode('MediaControl')
        a_movie.children[0].children[1] = this.movie_control;
        this.movie_sensor = new SFNode('MediaSensor')
        a_movie.children[0].children[2] = this.movie_sensor;

        gw_insert_media_node(a_movie);
        //dictionary of resources to load 
        this.dictionary = gw_new_container(null);
        this.dictionary.hide();

        this.movie.children[0].extension = this;

        this.movie.children[0].on_scene_size = function (evt) {
            var ext = this.extension;

            if (!this.callback_done) {
                this.callback_done = true;

                //process the error or connect service
                if (evt.error) {
                    var notif = gw_new_message(null, 'Error opening file!', 'Failed to open ' + this.url[0] + '\n\nReason: ' + gpac.error_string(evt.error));
                    notif.show();

                    var url = ext.movie.children[0].url.length ? ext.movie.children[0].url[0] : '';
                    //reassign old inline URL
                    this.url[0] = url;
                    if (url == '') {
                        ext.controler.show();
                    } else {
                        this.movie_connected = true;
                    }
                }

                //switch back inline nodes and remove from dictionary
                gw_detach_child(this);
                //force detach, we don't know when GC will be done
                ext.movie.children[0].url[0] = '';
                ext.movie.children[0] = this;

                if (evt.error) return;

                //success !
                ext.current_url = this.url[0];
                ext.set_speed(ext.initial_speed);
                if (ext.initial_start >= 0) {
                    ext.movie_control.mediaStartTime = ext.initial_start;
                } else {
                    ext.movie_control.mediaStartTime = 0;
                    ext.movie_control.mediaStopTime = ext.initial_start;
                    alert('MC stopTime is ' + ext.movie_control.mediaStopTime + ' TS buffer is ' + ext.timeshift_depth);
                }
                ext.movie_control.loop = ext.initial_loop;

                ext.movie_control.url[0] = ext.current_url;
                ext.movie_sensor.url[0] = ext.current_url;

                ext.timeshift_depth = 0;
                ext.initial_loop = false;
                ext.initial_speed = 1;
                ext.initial_start = 0;


                if ((ext.current_url.indexOf('gpac://') == 0) && ((ext.current_url.indexOf('://') < 0) || (ext.current_url.indexOf('file://') == 0))) {
                    ext.local_url = true;
                } else {
                    ext.local_url = false;
                }

                ext.root_odm = gpac.get_object_manager(ext.current_url);


                gw_background_control(false);

                ext.controler.hide();
                gpac.caption = ext.current_url;
                ext.add_bookmark(ext.current_url, true);

                ext.declare_addons();

                if (ext.initial_service_id) {
                    var odm = gpac.get_object_manager(ext.current_url);
                    if (odm) odm.select_service(ext.initial_service_id);
                    ext.initial_service_id = 0;
                }
                if (ext.show_stats_init) {
                    ext.view_stats();
                }
            }

            if (!ext.movie_connected) {
                ext.movie_connected = true;
                gpac.set_3d(evt.type3d ? 1 : 0);
                ext.controler.play.switch_icon(ext.icon_pause);
                ext.dynamic_scene = evt.dynamic_scene;
                //force display size notif on controler to trigger resize of the window
                ext.controler.on_display_size(ext.controler.width, ext.controler.height);
            }
            if (!gpac.fullscreen && evt.width && evt.height) {
                var w, h, r_w, r_h;
                w = evt.width;
                h = evt.height;

                if (w > gpac.screen_width) w = gpac.screen_width;
                if (h > gpac.screen_height) h = gpac.screen_height;

                r_w = r_h = 1;
                //                if (w < min_width) r_w = Math.ceil(min_width / w);
                //                if (h < min_height) r_h = Math.ceil(min_height / h);
                if (r_w < r_h) r_w = r_h;
                w = r_w * w;
                h = r_w * h;
                gpac.set_size(w, h);
            }
            ext.streamlist_changed();
        }
        this.movie.children[0].on_addon_found = function (evt) {
            var e = {};
            e.type = GF_EVENT_ADDON_DETECTED;
            e.addon_url = evt.url;
            e.scene_url = this.url[0];
            e.discarded = false;
            gwlib_filter_event(e);
        }

        this.movie.children[0].on_streamlist_changed = function (evt) {
            this.extension.streamlist_changed();
        }

        this.movie.children[0].addEventListener('gpac_scene_attached', this.movie.children[0].on_scene_size, 0);
        this.movie.children[0].addEventListener('gpac_addon_found', this.movie.children[0].on_addon_found, 0);
        this.movie.children[0].addEventListener('gpac_streamlist_changed', this.movie.children[0].on_streamlist_changed, 0);


        this.movie.children[0].on_media_progress = function (evt) {
            if (evt.buffering) {
                this.extension.show_buffer(evt.bufferLevel);
                this.extension.controler.layout();

            }
            if (evt.total) {
                var percent_dload = 100 * evt.loaded / evt.total;
                if (percent_dload >= 100) percent_dload = 0;
                this.extension.controler.media_line.set_progress(percent_dload);
            }
        }

        this.movie.children[0].on_media_playing = function (evt) {
            this.extension.show_buffer(-1);

            if (!this.extension.initial_play_done) {
                this.extension.initial_play_done = true;

                this.extension.controler.play.switch_icon(this.extension.icon_pause);

                if (!this.extension.root_odm) {
                    this.extension.root_odm = gpac.get_object_manager(this.extension.current_url);
                }
                if (this.extension.root_odm) {
                    this.extension.declare_addons();
                }
            }

            if (this.extension.root_odm) {
                this.extension.set_timeshift_depth(this.extension.root_odm.timeshift_depth);
            }
        }

        this.movie.children[0].on_media_end = function (evt) {
            if (this.extension.duration) {
                if (this.extension.movie_control.loop) {
                    this.extension.movie_control.mediaStartTime = 0;
                    this.extension.current_time = 0;
                } else if (this.extension.playlist_mode && (this.extension.playlist_idx + 1 < this.extension.playlist.length)) {
                    this.extension.playlist_next();
                } else {
                    this.extension.set_state(this.extension.GF_STATE_STOP);
                }
            }
        }

        this.movie.children[0].on_media_waiting = function (evt) {
            //            alert('URL is now buffering');
        }
        this.movie.children[0].on_tsb_change = function (evt) {
            if (this.extension.root_odm) {
                this.extension.set_timeshift_depth(this.extension.root_odm.timeshift_depth);

                var main_addon_url = this.extension.root_odm.main_addon_url;
                if (main_addon_url != null) gwskin.pvr_url = main_addon_url;

            }
        }
        this.movie.children[0].on_main_addon = function (evt) {
            this.extension.controler.layout();
        }


        this.movie.children[0].addEventListener('progress', this.movie.children[0].on_media_progress, 0);
        this.movie.children[0].addEventListener('playing', this.movie.children[0].on_media_playing, 0);
        this.movie.children[0].addEventListener('canplay', this.movie.children[0].on_media_playing, 0);
        this.movie.children[0].addEventListener('waiting', this.movie.children[0].on_media_waiting, 0);
        this.movie.children[0].addEventListener('ended', this.movie.children[0].on_media_end, 0);
        this.movie.children[0].addEventListener('gpac_timeshift_depth_changed', this.movie.children[0].on_tsb_change, 0);
        this.movie.children[0].addEventListener('gpac_main_addon_state', this.movie.children[0].on_main_addon, 0);


        this.on_movie_duration = function (value) {
            if (value < 0) value = 0;
            this.current_duration = value;
            this.set_duration(value);
            if (this.UPnP_Enabled) UPnP.MovieDuration = value;
        }

        this.on_movie_active = function (value) {
            if (!value) {
                this.movie_control.mediaStartTime = -1;
            }
        }

        this.on_movie_time = function (value) {
            var diff = this.current_time - value;
            if (diff < 0) diff = -diff;
            /*filter out every 1/2 seconds*/
            if (diff < 0.5) return;
            this.current_time = value;
            this.set_time(value);
            if (this.UPnP_Enabled) UPnP.MovieTime = value;
        }

        Browser.addRoute(this.movie_sensor, 'mediaDuration', this, this.on_movie_duration);
        Browser.addRoute(this.movie_sensor, 'mediaCurrentTime', this, this.on_movie_time);
        Browser.addRoute(this.movie_sensor, 'isActive', this, this.on_movie_active);

        var hist = gpac.get_option('GUI', 'PlaybackHistory');
        if ((hist != null) && (hist != '')) {
            this.history = gwskin.parse(hist);
        }

        var bmarks = gpac.get_option('GUI', 'Bookmarks');
        if ((bmarks != null) && (bmarks != '')) {
            this.bookmarks = gwskin.parse(bmarks);
        }

        /*create player control UI*/
        var wnd = gw_new_window(null, true, true);
        //remember it !
        this.controler = wnd;
        wnd.extension = this;

        wnd.set_corners(true, true, false, false);
        wnd.set_alpha(0.95);

        /*first set of controls*/
        wnd.infobar = gw_new_grid_container(wnd);
        wnd.infobar.spread_h = true;

        /*add our controls in order*/

        wnd.media_line = gw_new_progress_bar(wnd.infobar, false, true);
        wnd.media_line.extension = this;
        wnd.media_line.on_slide = function (value, type) {
            if (!this.extension.movie_connected && !this.extension.controlled_renderer) {
                this.set_value(0);
                return;
            }

            var duration = this.extension.duration;
            var time;
            if (duration) {
                time = value * duration / 100;
            } else if (this.extension.timeshift_depth) {
                time = (100 - value) * this.extension.timeshift_depth / 100;

                if (this.extension.controlled_renderer) {
                    return;
                }
                if (type != 2) return;

                this.extension.movie_control.mediaStopTime = -time;
                this.extension.controler.layout();
                return;
            }

            if (this.extension.controlled_renderer) {
                this.extension.controlled_renderer.Seek(time);
                return;
            }
            gw_background_control(false);
            switch (type) {
                //start sliding                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
                case 1:
                    this.extension.set_state(this.extension.GF_STATE_PAUSE);
                    this.extension.set_speed(0);
                    break;
                //done sliding                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
                case 2:
                    this.extension.set_state(this.extension.GF_STATE_PLAY);
                    this.extension.movie_control.mediaStartTime = time;
                    this.extension.set_speed(1);
                    break;
                //init slide, go in play mode                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
                default:
                    if (this.extension.state == this.extension.GF_STATE_STOP)
                        this.extension.set_state(this.extension.GF_STATE_PLAY);

                    this.extension.set_state(this.extension.GF_STATE_PAUSE);
                    break;
            }
        }

        wnd.time = gw_new_text(wnd.infobar, '00:00:00');
        gw_object_set_hitable(wnd.time);
        wnd.time.extension = this;
        wnd.time.reversed = false;
        wnd.time.on_down = function (val) {
            if (!val) return;

            if (this.extension.timeshift_depth) {
                return;
                //                if (this.extension.movie_control.mediaStopTime < 0)
                //                    this.extension.movie_control.mediaStopTime = 0;
            } else {
                this.reversed = !this.reversed;
                this.extension.set_time(this.extension.current_time);
            }
        }

        wnd.back_live = gw_new_icon(wnd.infobar, 'live');
        wnd.back_live.extension = this;
        wnd.back_live.on_click = function () {
            if (0 && this.extension.root_odm && this.extension.root_odm.main_addon_on) {
                this.extension.root_odm.disable_main_addon();
            }
            else if (this.extension.movie_control.mediaStopTime < 0) {
                this.extension.set_time(0);
                this.extension.movie_control.mediaStopTime = 0;
                this.extension.controler.layout();
            }
        }

        gw_new_separator(wnd.infobar);


        wnd.stop = gw_new_icon(wnd.infobar, 'stop');
        wnd.stop.extension = this;
        wnd.stop.on_click = function () {
            this.extension.set_state(this.extension.GF_STATE_STOP);
        }

        if (0) {
            wnd.rewind = gw_new_icon(wnd.infobar, 'rewind');
        } else {
            wnd.rewind = null;
        }

        wnd.play = gw_new_icon(wnd.infobar, 'play');
        wnd.play.extension = this;
        this.state = this.GF_STATE_PLAY;
        wnd.play.add_icon(gwskin.images.pause);
        wnd.play.on_click = function () {
            this.extension.set_state((this.extension.state == this.extension.GF_STATE_PLAY) ? this.extension.GF_STATE_PAUSE : this.extension.GF_STATE_PLAY);
        }


        wnd.forward = gw_new_icon_button(wnd.infobar, 'seek_forward', '', true, 'icon_label');
        wnd.forward.extension = this;
        wnd.forward.on_click = function () {
            if (this.extension.movie_control.mediaSpeed) {
                this.extension.set_state(this.extension.GF_STATE_TRICK);
                this.extension.set_speed(2 * this.extension.movie_control.mediaSpeed);
            }
        }
        wnd.forward.on_long_click = function () {
            if (this.extension.movie_control.mediaSpeed) {
                this.extension.set_state(this.extension.GF_STATE_TRICK);
                this.extension.set_speed(this.extension.movie_control.mediaSpeed / 2);
            }
        }

        wnd.loop = gw_new_icon(wnd.infobar, 'play_once');
        wnd.loop.extension = this;
        wnd.loop.add_icon(gwskin.images.play_loop);
        //            wnd.loop.add_icon(gwskin.images.play_shuffle);
        wnd.loop.on_click = function () {
            this.extension.movie_control.loop = this.extension.movie_control.loop ? FALSE : TRUE;
            this.switch_icon(this.extension.movie_control.loop ? 1 : 0);
        }

        wnd.media_list = gw_new_icon(wnd.infobar, 'media');
        wnd.media_list.extension = this;
        wnd.media_list.hide();

        wnd.channels = gw_new_icon(wnd.infobar, 'channels');
        wnd.channels.extension = this;
        wnd.channels.hide();


        wnd.navigate = gw_new_icon(wnd.infobar, 'navigation');
        wnd.navigate.extension = this;
        wnd.navigate.on_click = function () {
            this.extension.select_navigation_type();
        }

        wnd.stats = gw_new_icon(wnd.infobar, 'statistics');
        wnd.stats.extension = this;
        wnd.stats.on_click = function () {
            this.extension.view_stats();
        }


        gw_new_separator(wnd.infobar);


        wnd.snd_low = gw_new_icon(wnd.infobar, 'audio');
        wnd.snd_low.extension = this;
        wnd.snd_ctrl = gw_new_slider(wnd.infobar);
        wnd.snd_ctrl.extension = this;
        wnd.snd_ctrl.stick_to_previous = true;
        wnd.snd_low.add_icon('audio_mute');
        wnd.snd_low.on_click = function () {
            if (this.extension.muted) {
                gpac.volume = this.extension.muted;
                this.extension.muted = 0;
                this.switch_icon(0);
            } else {
                this.extension.muted = gpac.volume ? gpac.volume : 1;
                gpac.volume = 0;
                this.switch_icon(1);
            }
        }
        wnd.snd_ctrl.on_slide = function (value, type) {
            if (this.extension.muted) this.extension.controler.snd_low.on_click();
            gpac.volume = value;
        }
        wnd.snd_ctrl.set_value(gpac.volume);

        wnd.open = null;
        if (!gwskin.browser_mode) {
            wnd.open = gw_new_icon(wnd.infobar, 'file_open');
            wnd.open.extension = this;
            wnd.open.on_click = function () {
                this.extension.open_local_file();
            }
        }

        wnd.playlist = gw_new_icon(wnd.infobar, 'playlist');
        wnd.playlist.extension = this;
        wnd.playlist.on_click = function () {
            this.extension.view_playlist();
        }

        wnd.playlist_prev = gw_new_icon(wnd.infobar, 'playlist_prev');
        wnd.playlist_prev.extension = this;
        wnd.playlist_prev.on_click = function () {
            this.extension.playlist_prev();
        }

        wnd.playlist_next = gw_new_icon(wnd.infobar, 'playlist_next');
        wnd.playlist_next.extension = this;
        wnd.playlist_next.on_click = function () {
            this.extension.playlist_next();
        }


        wnd.home = null;
        if (!gwskin.browser_mode) {
            wnd.home = gw_new_icon(wnd.infobar, 'osmo');
            wnd.home.extension = this;
            wnd.home.on_click = function () {
                this.extension.controler.hide();
                if (this.extension._evt_filter != null) {
                    gwlib_remove_event_filter(this.extension._evt_filter);
                    this.extension._evt_filter = null;
                }
                gw_show_dock();
            }
        }


        if (this.UPnP_Enabled) {
            wnd.remote = gw_new_icon(wnd.infobar, 'remote_display');
            wnd.remote.on_click = function () { select_remote_display('push'); }
        } else {
            wnd.remote = null;
        }

        wnd.fullscreen = gw_new_icon(wnd.infobar, 'fullscreen');
        wnd.fullscreen.add_icon(gwskin.images.fullscreen_back);
        wnd.fullscreen.on_click = function () {
            gpac.fullscreen = !gpac.fullscreen;
        }
        wnd.fullscreen.switch_icon(gpac.fullscreen ? 1 : 0);

        if (!gwskin.browser_mode) {
            wnd.exit = gw_new_icon(wnd.infobar, 'exit');
            wnd.exit.on_click = function () { gpac.exit(); }
        } else {
            wnd.exit = null;
        }

        wnd.on_size = function (width, height) {
            var control_icon_size = gwskin.default_icon_height;
            var children = this.infobar.get_children();
            for (i = 0; i < children.length; i++) {
                children[i].set_size(control_icon_size, control_icon_size);
            }
            if (this.time) this.time.set_size(4 * wnd.time.font_size(), control_icon_size);
            if (this.snd_ctrl) {
                this.snd_ctrl.set_size(2 * control_icon_size, control_icon_size / 2, control_icon_size / 3, control_icon_size / 2);
                this.snd_ctrl.set_value(gpac.volume);
            }

            var speed = this.extension.movie_control.mediaSpeed;
            if (speed && (speed != 1)) {
                if ((speed > 100) || (speed < 1))
                    this.forward.set_size(3 * control_icon_size, control_icon_size);
                else
                    this.forward.set_size(2 * control_icon_size, control_icon_size);
            } else {
                this.forward.set_size(control_icon_size, control_icon_size);
            }
        }

        wnd.layout = function (width, height) {
            var min_w, full_w, time_w;
            var control_icon_size = gwskin.default_icon_height;
            var is_over = true;
            var show_navigate = false;
            if (arguments.length == 0) {
                width = this.width;
                height = this.height;
            }
            this.move(0, Math.floor((height - gw_display_height) / 2));

            this.on_size(width, height);

            this.fullscreen.switch_icon(gpac.fullscreen ? 1 : 0);


            width -= control_icon_size / 2;
            min_w = control_icon_size;
            if (this.time.visible) min_w += this.time.width;
            if (this.open) min_w += control_icon_size;
            if (this.home) min_w += control_icon_size;
            if (this.exit && gpac.fullscreen) min_w += control_icon_size;
            full_w = 0;
            if (this.snd_low) full_w += control_icon_size;
            if (this.snd_ctrl) full_w += this.snd_ctrl.width;
            if (this.fullscreen) full_w += control_icon_size;


            if (this.navigate) {
                this.navigate.hide();
                if (!this.extension.dynamic_scene && this.extension.movie_connected && (gpac.navigation_type != GF_NAVIGATE_TYPE_NONE)) {
                    show_navigate = true;
                    full_w += control_icon_size;
                }
            }

            if (this.extension.movie_connected) {
                if (this.extension.state == this.extension.GF_STATE_STOP) {
                    this.stats.hide();
                } else {
                    full_w += control_icon_size;
                    this.stats.show();
                }
                if (this.extension.duration || this.extension.timeshift_depth) {
                    full_w += control_icon_size;
                    this.stop.show();
                    full_w += control_icon_size;
                    this.play.show();
                } else {
                    full_w += control_icon_size;
                    this.play.show();
                    if (this.extension.state == this.extension.GF_STATE_STOP) {
                        this.stop.hide();
                        this.play.show();
                    } else {
                        //                        this.play.hide();
                        //                        this.stop.show();
                    }
                }

                if (this.extension.root_odm && !this.extension.dynamic_scene && !this.extension.root_odm.is_over)
                    is_over = false;

                //                this.media_list.show();

            } else {
                this.stats.hide();
                this.stop.hide();
                this.play.hide();
                this.media_list.hide();
            }

            if (this.extension.duration) {
                if (this.rewind) full_w += control_icon_size;
                if (this.forward) full_w += control_icon_size;
                if (this.loop) full_w += control_icon_size;
            }
            else if (this.extension.movie_control.mediaStopTime < 0) {
                if (this.forward) full_w += control_icon_size;
            } else if (!is_over) {
                if (this.forward) full_w += control_icon_size;
            }

            if ((this.extension.root_odm && this.extension.root_odm.main_addon_on) || this.extension.movie_control.mediaStopTime < 0) {
                full_w += control_icon_size;
                this.back_live.show();
            } else {
                this.back_live.hide();
            }


            if (this.remote && UPnP.MediaRenderersCount && (this.extension.current_url != '')) {
                full_w += control_icon_size;
            }
            time_w = this.media_line.visible ? 2 * control_icon_size : 0;

            if (this.exit) {
                if (gpac.fullscreen) {
                    this.exit.show();
                } else {
                    this.exit.hide();
                }
            }

            //            if (min_w + full_w + time_w < width) {
            if (1) {
                if (this.media_line.visible)
                    this.media_line.set_size(width - min_w - full_w, control_icon_size / 3);

                if (this.snd_low) this.snd_low.show();
                if (this.snd_ctrl) this.snd_ctrl.show();

                if (this.extension.duration) {
                    if (this.rewind) this.rewind.show();
                    if (this.forward) this.forward.show();
                    if (this.loop) this.loop.show();
                    if (this.stop) this.stop.show();
                }
                else if (this.extension.movie_control.mediaStopTime < 0) {
                    if (this.forward) this.forward.show();
                } else if (!is_over) {
                    if (this.forward) this.forward.show();
                } else {
                    if (this.forward) this.forward.hide();
                }
                if (wnd.fullscreen) wnd.fullscreen.show();

                if (this.remote) {
                    if (UPnP.MediaRenderersCount && (this.extension.current_url != '')) {
                        this.remote.show();
                    } else {
                        this.remote.hide();
                    }
                }

                if (show_navigate) {
                    this.navigate.show();
                }
            } else {

                if (this.snd_low) this.snd_low.hide();
                if (this.snd_ctrl) this.snd_ctrl.hide();
                if (this.rewind) this.rewind.hide();
                if (this.forward) this.forward.hide();
                if (this.loop) this.loop.hide();
                if (this.fullscreen) this.fullscreen.hide();
                if (this.remote) this.remote.hide();

                if (show_navigate) {
                    if (min_w + time_w + this.navigate.width < width) {
                        min_w += this.navigate.width;
                        this.navigate.show();
                    }
                }

                if (this.remote) {
                    if (UPnP.MediaRenderersCount && (this.extension.current_url != '') && (min_w + time_w + this.remote.width < width)) {
                        min_w += this.remote.width;
                        this.remote.show();
                    } else {
                        this.remote.hide();
                    }
                }

                if (this.media_line.visible)
                    this.media_line.set_size(width - min_w - 5, control_icon_size / 3);

            }

            if (this.extension.duration || this.extension.timeshift_depth) {
                this.media_line.show();
                this.time.show();
            } else {
                this.media_line.hide();
                this.time.hide();
            }
            //this.time.set_size(width - 6*control_icon_size, control_icon_size / 3);
            this.media_line.set_size(width - this.time.width - 2 * control_icon_size, control_icon_size / 3);

            //            this.infobar.set_size(width, control_icon_size);
            this.infobar.set_size(width, height);
            this.infobar.move(0, -0.1 * control_icon_size);
        }

        wnd.on_display_size = function (w, h) {
            var def_width = 640;
            if (!gpac.fullscreen) {
                if (w < def_width) {
                    gpac.set_size(480, h);
                    return;
                }
            } else {
                if (w < def_width) {
                    def_width = w;
                }
            }

            //            this.set_size(w, 1.2 * gwskin.default_icon_height);

            if (this.extension.movie_connected) h = 3.3 * gwskin.default_icon_height;
            else h = 1.1 * gwskin.default_icon_height;

            this.set_size(def_width, h);

            this.layout();
        }

        wnd.on_event = function (evt) {
            if (this.infobar.on_event(evt)) return true;
            return false;
        }
        //make the bar hitable so that clicking on it does not make it disappear
        gw_object_set_hitable(wnd);
        wnd.set_size(gw_display_width, 2.2 * gwskin.default_icon_height);
        this.set_duration(0);
        this.set_time(0);


        this._evt_filter = null;
    },

    start: function () {
        if (!this._evt_filter) {
            this._evt_filter = this.create_event_filter(this);
            gwlib_add_event_filter(this._evt_filter);
        }

        if (!this.movie) {
            this.init_extension();

            //check our args
            var i, argc = gpac.argc;
            var url_arg = null;

            for (i = 1; i < argc; i++) {
                var arg = gpac.get_arg(i);
                if (!arg) break;

                //that's our file
                if (arg.charAt(0) != '-') {
                    //do not reopen ourselves !
                    if (arg.indexOf('gui.bt') >= 0) continue;

                    url_arg
                    if (arg.indexOf('://') < 0) url_arg = 'gpac://' + arg;
                    else url_arg = arg;
                }

                //MP4Client options taking 2 args
                else if ((arg == '-rti') || (arg == '-rtix') || (arg == '-c') || (arg == '-cfg') || (arg == '-size') || (arg == '-lf') || (arg == '-log-file') || (arg == '-logs')
                    || (arg == '-opt') || (arg == '-ifce') || (arg == '-views') || (arg == '-run-for')
                ) {
                    i++;
                } else if (arg == '-service') {
                    this.initial_service_id = parseInt(gpac.get_arg(i + 1));
                    i++;
                } else if (arg == '-com') {
                    arg = gpac.get_arg(i + 1);
                    var idx = arg.indexOf('gpac add ');

                    if (idx == 0) {
                        this.default_addon = arg.substring(9);
                    }

                    i++;
                } else if (arg == '-addon') {
                    this.default_addon = gpac.get_arg(i + 1);
                    i++;
                } else if (arg == '-loop') {
                    this.initial_loop = true;
                } else if (arg == '-stats') {
                    this.show_stats_init = true;
                } else if (arg == '-speed') {
                    this.initial_speed = Number(gpac.get_arg(i + 1));
                    i++;
                } else if (arg == '-play-from') {
                    this.initial_start = Number(gpac.get_arg(i + 1));
                    i++;
                }
            }

            gwskin.media_url = '';
            gwskin.pvr_url = '';
            gwskin.media_time = 0;
            gwskin.media_clock = 0;
            gwskin.restore_session = this.create_restore_session(this);

            if (url_arg != null) {
                this.set_playlist_mode(false);
                var def_addon = this.default_addon;
                this.set_movie_url(url_arg);
                gw_hide_dock();
                this.default_addon = def_addon;
                return;
            }
            this.set_playlist_mode(true);

            /*
            var label = '';
            for (i = 1; i < argc; i++) {
            label += '#'+i + ': ' + gpac.get_arg(i) + '\n';
            }
            var notif = gw_new_message(null, 'GPAC Arguments', label);
            notif.show();
            */

        }

        this.controler.show();
        gw_hide_dock();
    },

    create_event_filter: function (__anobj) {
        return function (evt) {
            __anobj.ext_filter_event(evt);
        }
    },

    create_restore_session: function (__anobj) {
        return function (url, media_time, media_clock) {
            __anobj.set_movie_url(url);
            if (media_clock) {
                var time_in_tsb = (new Date()).getTime() - media_clock;
                if (time_in_tsb >= 0) {
                    __anobj.initial_start = -time_in_tsb / 1000;
                }

            } else {
                __anobj.initial_start = media_time;
            }
        }
    },

    declare_addons: function () {
        if (this.default_addon && (this.default_addon != '')) {
            var addon_url = this.default_addon;
            if (addon_url.indexOf('://') < 0) addon_url = 'gpac://' + addon_url;
            this.root_odm.declare_addon(addon_url);
        }
    },

    set_time: function (value) {
        var h, m, s, str;
        str = '';
        if (this.timeshift_depth) {
            var pos = 100;
            var time_in_tsb = 0;

            if (this.movie_control.mediaStopTime < 0) {
                time_in_tsb = this.root_odm.timeshift_time;
                if (this.timeshift_depth > time_in_tsb) {
                    pos = 100 * (this.timeshift_depth - time_in_tsb) / this.timeshift_depth;
                }
            }
            this.controler.media_line.set_value(pos);

            gwskin.media_clock = (new Date()).getTime() - time_in_tsb * 1000;

            if (!time_in_tsb) {
                this.controler.time.set_label('live');
                this.controler.time.hide();
                this.controler.layout();
                return;
            }

            if (!this.controler.time.visible) {
                this.controler.time.show();
                this.controler.layout();
            }
            str = '-';
            value = time_in_tsb;

        } else {
            if (!this.duration) return;

            this.current_time = value;
            gwskin.media_time = value;

            if (this.duration) {
                this.controler.media_line.set_value(100 * value / this.duration);
            }
            if (this.controler.time.reversed) {
                value = this.duration - value;
                str = '-';
            }
        }

        h = Math.floor(value / 3600);
        value -= h * 3600;
        m = Math.floor(value / 60);
        value -= m * 60;
        s = Math.floor(value);
        if (h) {
            if (h < 10) str += '0';
            str += h + ':';
        }
        if (m < 10) str += '0';
        str += m + ':';
        if (s < 10) str += '0';
        str += s;
        this.controler.time.set_label(str);
    },

    set_duration: function (value) {
        this.duration = value;
        var dlg = this.controler;
        if (!value) {
            if (this.timeshift_depth > 0) {
                return;
            }
            dlg.time.hide();
            dlg.media_line.hide();
            if (dlg.rewind) dlg.rewind.hide();
            dlg.play.hide();
            dlg.forward.hide();
            dlg.loop.hide();
            dlg.time.set_size(0, gwskin.default_icon_height);
            dlg.time.set_width(0);
        } else {
            dlg.time.show();
            dlg.media_line.show();
            if (dlg.rewind) dlg.rewind.show();
            dlg.stop.show();
            dlg.forward.show();
            dlg.loop.show();
            if (value < 3600) {
                dlg.time.set_size(gwskin.default_icon_height / 2, gwskin.default_icon_height);
                dlg.time.set_width(3 * dlg.time.font_size());
            } else {
                dlg.time.set_size(gwskin.default_icon_height, gwskin.default_icon_height);
                dlg.time.set_width(4 * dlg.time.font_size());
            }
        }
        dlg.layout();
    },
    set_timeshift_depth: function (value) {
        if (this.timeshift_depth == value) return;

        this.timeshift_depth = value;
        var dlg = this.controler;

        if (!value) {
            dlg.time.hide();
            dlg.media_line.hide();
            if (dlg.rewind) dlg.rewind.hide();
            if (dlg.forward) dlg.forward.hide();
            dlg.time.set_size(0, gwskin.default_icon_height);
            dlg.time.set_width(0);
        } else {
            dlg.time.show();
            dlg.media_line.show();
            /*
            if (dlg.rewind) dlg.rewind.show();
            if (dlg.stop) dlg.stop.show();
            if (dlg.forward) dlg.forward.show();
            if (dlg.loop) dlg.loop.show();
            */
            var w = 3 * dlg.time.font_size();
            if (value > 3600) w += dlg.time.font_size();

            dlg.time.set_size(w, gwskin.default_icon_height);
            dlg.time.set_width(w);
        }
        dlg.layout();
    },

    set_state: function (state) {
        if (!this.movie_connected && !this.controlled_renderer) return;

        if (state == this.state) return;

        if (state == this.GF_STATE_STOP) {
            if (this.stat_wnd) this.stat_wnd.close_all();
            this.stoped_url = '' + this.current_url;
            if (this.controlled_renderer) this.controlled_renderer.Stop();
            else {
                this.set_movie_url('');
                /*override movie_connected to avoid auto-resizing*/
                this.movie_connected = true;
            }
            this.movie_control.mediaStartTime = 0;
            this.controler.media_line.set_value(0);
            this.controler.play.switch_icon(this.icon_play);
            this.state = this.GF_STATE_STOP;
            this.set_speed(1);
            this.root_odm = null;
            return;
        }
        if (state == this.GF_STATE_PAUSE) {
            if (this.state == this.GF_STATE_STOP) return;
            if (this.controlled_renderer) this.controlled_renderer.Pause();
            this.state = this.GF_STATE_PAUSE;
            this.controler.play.switch_icon(this.icon_play);
            this.set_speed(0);
            return;
        }
        //we are playing, resume from stop if needed
        if (this.stoped_url) {
            if (this.controlled_renderer) {
                this.controlled_renderer.Play();
            } else {
                this.set_movie_url(this.stoped_url, true);
            }
            this.stoped_url = null;
            //not in trick mode, next pause/play will restart from current time
            if (state != this.GF_STATE_TRICK)
                this.movie_control.mediaStartTime = -1;
        }


        if (state == this.GF_STATE_PLAY) {
            if (this.controlled_renderer) this.controlled_renderer.Play();
            this.state = state;
            this.controler.play.switch_icon(this.icon_pause);
            if (this.movie_control.mediaSpeed != 1);
            this.movie_control.mediaStartTime = -1;
            this.set_speed(1);
            return;
        }
        if (state == this.GF_STATE_TRICK) {
            this.state = state;
            this.controler.play.switch_icon(this.icon_play);
            this.movie_control.mediaStartTime = -1;
            return;
        }
    },


    set_speed: function (value) {
        this.movie_control.mediaSpeed = value;

        if (!this.duration && this.timeshift_depth && !value) {
            this.movie_control.mediaStopTime = -0.1;
            //
            this.movie_control.mediaStartTime = -1;
        }
        if (value && value != 1) {
            if (value > 1)
                this.controler.forward.set_label('x' + value);
            else
                this.controler.forward.set_label('x' + Math.round(100 * value) / 100);
        } else {
            this.controler.forward.set_label('');
        }
        this.controler.layout();
    },

    set_movie_url: function (url) {
        this.initial_play_done = false;

        gwskin.media_url = url;
        gwskin.pvr_url = '';

        if ((url == '') || (url == this.current_url)) {
            this.movie.children[0].url[0] = url;
            this.movie_control.url[0] = url;
            this.movie_sensor.url[0] = url;
            if (this.UPnP_Enabled) UPnP.MovieURL = url;
            this.movie_connected = (url == '') ? false : true;
            gw_background_control(this.movie_connected ? false : true);
            gpac.caption = url;

        } else if (this.controlled_renderer == null) {
            //resume from stop  
            if ((arguments.length == 2) && (arguments[1] == true)) {
                this.current_url = url;
                this.set_speed(1);
                this.movie_control.mediaStartTime = 0;
                this.movie_control.url[0] = url;
                this.movie_sensor.url[0] = url;
                this.movie.children[0].url[0] = url;
                gw_background_control(false);

                return;
            }

            this.default_addon = null;
            this.root_odm = null;

            /*create a temp inline holding the previous scene, use it as the main movie and use the old inline to test the resource. 
            This avoids messing up with event targets already setup*/
            var movie_inline = this.movie.children[0];
            var temp_inline = new SFNode('Inline');
            if (typeof movie_inline.url[0] != 'undefined')
                temp_inline.url[0] = movie_inline.url[0];

            this.movie.children[0] = temp_inline;

            movie_inline.callback_done = false;
            this.movie_connected = false;

            /*handle navigation between local files*/
            if ((this.current_url && this.current_url.indexOf('gpac://') >= 0) && (url.indexOf('://') < 0)) {
                movie_inline.url[0] = "gpac://" + url;
            } else {
                movie_inline.url[0] = url;
            }

            if (!movie_inline.callback_done) {
                gw_add_child(this.dictionary, movie_inline);
            }

            return;

        } else {
            var uri = UPnP.ShareResource(url);
            this.controlled_renderer.Open(uri);
        }
        this.current_url = url;

        if (url == '') this.controler.show();
        else this.controler.hide();

    },


    show_buffer: function (level) {
        if ((level < 0) || (level >= 98)) {
            if (this.buffer_wnd) {
                this.buffer_wnd.close();
                this.buffer_wnd = null;
            }
            return;
        }

        if (!this.buffer_wnd) {
            this.buffer_wnd = gw_new_window(null, true, true);
            this.buffer_wnd.txt = gw_new_text(this.buffer_wnd, '');
            this.buffer_wnd.on_display_size = function (w, h) {
                this.set_size(20 * gwskin.default_text_font_size, 2 * gwskin.default_text_font_size);
                this.txt.set_width(20 * gwskin.default_text_font_size);
                this.move(0, h / 2 - gwskin.default_text_font_size);
            }
            this.buffer_wnd.on_display_size(gw_display_width, gw_display_height);
            this.buffer_wnd.set_alpha(0.9);
            this.buffer_wnd.show();
            this.buffer_wnd.show();

            this.buffer_wnd.timer = gw_new_timer(false);
            this.buffer_wnd.timer.wnd = this.buffer_wnd;
            this.buffer_wnd.timer.set_timeout(gwskin.default_message_timeout, false);
            this.buffer_wnd.timer.start(0);
            this.buffer_wnd.timer.on_active = function (val) {
                if (!val) this.wnd.close();
            }

        }

        this.buffer_wnd.timer.stop(gwskin.default_message_timeout);
        this.buffer_wnd.txt.set_label('Buffering ' + level + ' %');
    },

    select_navigation_type: function () {
        var nb_items = 0;
        var type = gpac.navigation;
        if (this.navigation_wnd) {
            this.navigation_wnd.close();
            return;
        }
        var wnd = gw_new_popup(this.controler.navigate, 'up');
        this.navigation_wnd = wnd;
        wnd.extension = this;

        wnd.on_close = function () {
            this.extension.navigation_wnd = null;
        }

        wnd.select = function (type) {
            this.extension.navigation_wnd = null;
            if (type == 'reset') {
                gpac.navigation_type = 0;
            } else {
                gpac.navigation = type;
            }
        }
        wnd.make_select_item = function (text, type, current_type) {
            if (current_type == type) text = '* ' + text;
            wnd.add_menu_item(text, function () { this.select(type); });
        }
        wnd.add_menu_item('Reset', function () { this.select('reset'); });

        wnd.make_select_item('None', GF_NAVIGATE_NONE, type);
        wnd.make_select_item('Slide', GF_NAVIGATE_SLIDE, type);
        wnd.make_select_item('Examine', GF_NAVIGATE_EXAMINE, type);

        if (gpac.navigation_type == GF_NAVIGATE_TYPE_3D) {
            wnd.make_select_item('Walk', GF_NAVIGATE_WALK, type);
            wnd.make_select_item('Fly', GF_NAVIGATE_FLY, type);
            wnd.make_select_item('Pan', GF_NAVIGATE_PAN, type);
            wnd.make_select_item('Game', GF_NAVIGATE_GAME, type);
            wnd.make_select_item('Orbit', GF_NAVIGATE_ORBIT, type);
            wnd.make_select_item('VR', GF_NAVIGATE_VR, type);
        }
        wnd.on_display_size(gw_display_width, gw_display_height);
        wnd.show();
    },

    streamlist_changed: function () {
        this.services = [];

        var root_odm = this.root_odm;

        if (root_odm) {
            for (var res_i = 0; res_i < root_odm.nb_resources; res_i++) {
                var m = root_odm.get_resource(res_i);
                if (!m || !m.service_id) continue;
                var res = null;
                for (var k = 0; k < this.services.length; k++) {
                    if (this.services[k].service_id == m.service_id) {
                        res = this.services[k];
                        break;
                    }
                }
                if (res == null) {
                    this.services.push(m);
                }
            }
        }

        if (this.services.length <= 1) {
            this.controler.channels.hide();
            this.controler.channels.on_click = function () { }
            this.controler.layout();
            return;
        }
        if (!this.controler.channels.visible) {
            this.controler.channels.show();
            this.controler.layout();
        }

        this.controler.channels.on_click = function () {

            if (this.extension.channels_wnd) {
                this.extension.channels_wnd.close();
                return;
            }
            var wnd = gw_new_popup(this.extension.controler.channels, 'up');
            this.extension.channels_wnd = wnd;
            wnd.extension = this.extension;

            wnd.on_close = function () {
                this.extension.channels_wnd = null;
            }

            wnd.make_item = function (text, i) {
                wnd.add_menu_item(text, function () {
                    this.extension.root_odm.select_service(this.extension.services[i].service_id);
                });
            }
            for (var i = 0; i < this.extension.services.length; i++) {
                var text = '';
                var n = this.extension.services[i].service_name;
                if (this.extension.root_odm.selected_service == this.extension.services[i].service_id) text += '* ';
                if (n == null) n = 'Service ' + this.extension.services[i].service_id;
                text += n;
                wnd.make_item(text, i);
            }
            wnd.on_display_size(gw_display_width, gw_display_height);
            wnd.show();
        }

    }


};

