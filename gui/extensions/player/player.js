
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

    ext_filter_event: function (evt) {
        switch (evt.type) {
            case GF_EVENT_MOUSEUP:
                if (this.file_open_dlg) return false;
                gw_hide_dock();
                if (this.controler.visible) {
                    this.controler.hide();
                } else {
                    this.controler.show();
                }
                return 1;
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
                if (this.movie_control.mediaSpeed != 1) {
                    this.set_speed(1);
                    var msg = gw_new_message(null, 'Timeshift Underrun', 'Resuming to normal speed');
                    msg.set_size(380, gwskin.default_icon_height + 2 * gwskin.default_text_font_size);
                    msg.show();
                }
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

                alert('Movie setup ' + this.extension.movie_connected + ' to URL ' + ext.movie.children[0].url[0]);

                //success !
                ext.current_url = this.url[0];
                ext.set_speed(1);
                ext.movie_control.mediaStartTime = 0;
                ext.movie_control.url[0] = ext.current_url;
                ext.movie_sensor.url[0] = ext.current_url;
                ext.timeshift_depth = 0;

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

                if (ext.default_addon && (ext.default_addon != '')) {
                    var addon_url = ext.default_addon;
                    if (addon_url.indexOf('://') < 0) addon_url = 'gpac://' + addon_url;
                    ext.root_odm.declare_addon(addon_url);
                }

                if (ext.initial_service_id) {
                    var odm = gpac.get_object_manager(ext.current_url);
                    if (odm) odm.select_service(ext.initial_service_id);
                    ext.initial_service_id = 0;

                }
            }

            if (!this.extension.movie_connected) {
                alert('movie connected');
                this.extension.movie_connected = true;
                gpac.set_3d(evt.type3d ? 1 : 0);
                this.extension.controler.play.switch_icon(this.extension.icon_pause);
                this.extension.dynamic_scene = evt.dynamic_scene;
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
        }
        this.movie.children[0].on_addon_found = function (evt) {
            alert('Addon found!');
            var e = {};
            e.type = GF_EVENT_ADDON_DETECTED;
            e.addon_url = evt.url;
            e.scene_url = this.url[0];
            e.discarded = false;
            gwlib_filter_event(e);
        }

        this.movie.children[0].addEventListener('gpac_scene_attached', this.movie.children[0].on_scene_size, 0);
        this.movie.children[0].addEventListener('gpac_addon_found', this.movie.children[0].on_addon_found, 0);

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
            this.extension.controler.play.switch_icon(this.extension.icon_pause);

            if (!this.extension.root_odm) {
                this.extension.root_odm = gpac.get_object_manager(this.extension.current_url);
            }
            if (this.extension.root_odm) {
                this.extension.set_timeshift_depth(this.extension.root_odm.timeshift_depth);
            }
        }

        this.movie.children[0].on_media_end = function (evt) {
            alert('ended ');
            alert('ended ' + this.extension.duration);

            if (this.extension.duration) {
                if (this.extension.movie_control.loop) {
                    this.extension.movie_control.mediaStartTime = 0;
                    this.extension.current_time = 0;
                } else {
                    this.extension.set_state(this.extension.GF_STATE_STOP);
                }
            }
        }

        this.movie.children[0].on_media_waiting = function (evt) {
            alert('URL is now buffering');
        }
        this.movie.children[0].on_main_addon = function (evt) {
            alert('Main addon is ' + evt.detail);
            this.extension.controler.layout();
        }

        this.movie.children[0].addEventListener('progress', this.movie.children[0].on_media_progress, 0);
        this.movie.children[0].addEventListener('playing', this.movie.children[0].on_media_playing, 0);
        this.movie.children[0].addEventListener('canplay', this.movie.children[0].on_media_playing, 0);
        this.movie.children[0].addEventListener('waiting', this.movie.children[0].on_media_waiting, 0);
        this.movie.children[0].addEventListener('ended', this.movie.children[0].on_media_end, 0);
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
            this.history = JSON.parse(hist);
        }

        var bmarks = gpac.get_option('GUI', 'Bookmarks');
        if ((bmarks != null) && (bmarks != '')) {
            this.bookmarks = JSON.parse(bmarks);
        }

        /*create player control UI*/
        var wnd = gw_new_window(null, true, true);
        //remember it !
        this.controler = wnd;
        wnd.extension = this;

        this.stoped_url = null;
        wnd.set_corners(true, true, false, false);

        /*first set of controls*/
        wnd.infobar = gw_new_grid_container(wnd);
        wnd.infobar.spread_h = true;

        /*add our controls in order*/
        wnd.snd_low = gw_new_icon(wnd.infobar, 'audio');
        wnd.snd_low.extension = this;
        wnd.snd_ctrl = gw_new_slider(wnd.infobar);
        wnd.snd_ctrl.extension = this;
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

        wnd.back_live = gw_new_icon(wnd.infobar, 'live');
        wnd.back_live.extension = this;
        wnd.back_live.on_click = function () {
            if (this.extension.root_odm && this.extension.root_odm.main_addon_on) {
                this.extension.root_odm.disable_main_addon();
            }
            else if (this.extension.movie_control.mediaStopTime < 0) {
                this.extension.movie_control.mediaStopTime = 0;
                this.extension.controler.layout();
            }
        }


        if (!gwskin.browser_mode) {
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
        } else {
            wnd.forward = null;
        }

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
                alert('Going ' + time + ' ms in timeshift buffer');

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

        if (1) {
            wnd.loop = gw_new_icon(wnd.infobar, 'play_once');
            wnd.loop.extension = this;
            wnd.loop.add_icon(gwskin.images.play_loop);
            //            wnd.loop.add_icon(gwskin.images.play_shuffle);
            wnd.loop.on_click = function () {
                this.extension.movie_control.loop = this.extension.movie_control.loop ? FALSE : TRUE;
                this.switch_icon(this.extension.movie_control.loop ? 1 : 0);
            }
        } else {
            wnd.loop = null;
        }

        wnd.view = gw_new_icon(wnd.infobar, 'navigation');
        wnd.view.on_click = function () {
            select_navigation_type();
        }

        wnd.stats = gw_new_icon(wnd.infobar, 'statistics');
        wnd.stats.extension = this;
        wnd.stats.on_click = function () {
            this.extension.view_stats();
        }


        if (!gwskin.browser_mode) {
            wnd.open = gw_new_icon(wnd.infobar, 'file_open');
            wnd.open.extension = this;
            wnd.open.on_click = function () {
                this.extension.open_local_file();
            }
        } else {
            wnd.open = null;
        }


        if (!gwskin.browser_mode) {
            wnd.home = gw_new_icon(wnd.infobar, 'home');
            wnd.home.extension = this;
            wnd.home.on_click = function () {
                this.extension.controler.hide();
                if (this.extension._evt_filter != null) {
                    gwlib_remove_event_filter(this.extension._evt_filter);
                    this.extension._evt_filter = null;
                }
                gw_show_dock();
            }
        } else {
            wnd.home = null;
        }


        if (this.UPnP_Enabled) {
            wnd.remote = gw_new_icon(wnd.infobar, 'remote_display');
            wnd.remote.on_click = function () { select_remote_display('push'); }
        } else {
            wnd.remote = null;
        }

        if (1) {
            wnd.fullscreen = gw_new_icon(wnd.infobar, 'fullscreen');
            wnd.fullscreen.on_click = function () { gpac.fullscreen = !gpac.fullscreen; }
        } else {
            wnd.fullscreen = null;
        }

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
                this.snd_ctrl.set_size(2 * control_icon_size, 2, control_icon_size / 3, control_icon_size / 3);
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

            if (arguments.length == 0) {
                width = this.width;
                height = this.height;
            }
            this.move(0, Math.floor((height - gw_display_height) / 2));

            this.on_size(width, height);

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


            if (this.view) {
                this.view.hide();
                if (!this.extension.dynamic_scene && this.extension.movie_connected && (gpac.navigation_type != GF_NAVIGATE_TYPE_NONE)) {
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
                full_w += control_icon_size;
                this.stop.show();
                full_w += control_icon_size;
                this.play.show();
            } else {
                this.stats.hide();
                this.stop.hide();
                this.play.hide();
            }

            if (this.extension.duration) {
                if (this.rewind) full_w += control_icon_size;
                if (this.forward) full_w += control_icon_size;
                if (this.loop) full_w += control_icon_size;
            }

            if (this.extension.movie_control.mediaStopTime < 0) {
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

            if (min_w + full_w + time_w < width) {
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

                if (this.extension.movie_control.mediaStopTime < 0) {
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

                if (this.view && !this.extension.dynamic_scene && this.extension.movie_connected && (gpac.navigation_type != GF_NAVIGATE_TYPE_NONE)) {
                    this.view.show();
                }
            } else {

                if (this.snd_low) this.snd_low.hide();
                if (this.snd_ctrl) this.snd_ctrl.hide();
                if (this.rewind) this.rewind.hide();
                if (this.forward) this.forward.hide();
                if (this.loop) this.loop.hide();
                if (this.fullscreen) this.fullscreen.hide();
                if (this.remote) this.remote.hide();

                if (this.view && !this.extension.dynamic_scene && this.extension.movie_connected && (gpac.navigation_type != GF_NAVIGATE_TYPE_NONE)) {
                    if (min_w + time_w + this.view.width < width) {
                        min_w += this.view.width;
                        this.view.show();
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

            this.infobar.set_size(width, control_icon_size);
            this.infobar.move(0, -0.1 * control_icon_size);
        }

        wnd.on_display_size = function (w, h) {
            this.set_size(w, 1.2 * gwskin.default_icon_height);
            this.layout();
        }

        wnd.on_event = function (evt) {
            if (this.infobar.on_event(evt)) return true;
            return false;
        }
        //make the bar hotable so that clicking on it does not make it disappear
        gw_object_set_hitable(wnd);
        wnd.set_size(gw_display_width, 1.2 * gwskin.default_icon_height);
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

                //MP4Client options ...
                else if ((arg == '-rti') || (arg == '-rtix') || (arg == '-c') || (arg == '-cfg') || (arg == '-size') || (arg == '-lf') || (arg == '-log-file') || (arg == '-logs')
                    || (arg == '-opt') || (arg == '-ifce') || (arg == '-play-from') || (arg == '-speed') || (arg == '-views') || (arg == '-run-for')
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
                }
            }

            if (url_arg != null) {
                var def_addon = this.default_addon;
                this.set_movie_url(url_arg);
                gw_hide_dock();
                this.default_addon = def_addon;
                return;
            }
        }

        this.controler.show();
        gw_hide_dock();
    },

    create_event_filter: function (__anobj) {
        return function (evt) {
            __anobj.ext_filter_event(evt);
        }
    },

    set_time: function (value) {
        var h, m, s, str;

        str = '';
        if (this.timeshift_depth) {
            var time_in_tsb = this.root_odm.timeshift_time;
            var pos = 0;
            if (this.timeshift_depth > time_in_tsb) {
                pos = 100 * (this.timeshift_depth - time_in_tsb) / this.timeshift_depth;
/*                if (pos > 95) {
                    pos = 100;
                    time_in_tsb = 0;
                }
*/
            }
            this.controler.media_line.set_value(pos);
            if (!time_in_tsb) {
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
            if (this.timeshift_depth > 0) return;
            dlg.time.hide();
            dlg.media_line.hide();
            if (dlg.rewind) dlg.rewind.hide();
            if (dlg.stop) dlg.stop.hide();
            if (dlg.forward) dlg.forward.hide();
            if (dlg.loop) dlg.loop.hide();
            dlg.time.set_size(0, gwskin.default_icon_height);
            dlg.time.set_width(0);
        } else {
            dlg.time.show();
            dlg.media_line.show();
            if (dlg.rewind) dlg.rewind.show();
            if (dlg.stop) dlg.stop.show();
            if (dlg.forward) dlg.forward.show();
            if (dlg.loop) dlg.loop.show();
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

    open_local_file: function () {
        var filebrowse = gw_new_file_dialog(null, 'Open file');
        filebrowse.filter = '*';
        filebrowse.browse(gpac.last_working_directory);

        filebrowse.extension = this;
        this.file_open_dlg = true;

        filebrowse.on_display_size = function (width, height) {
            var w = width / 2;
            if (w < 200) w = width - 20;
            if (w > 500) w = 500;
            this.set_size(w, 0.8 * height);
            this.move((width - w) / 2, 0);
        }

        filebrowse.on_browse = function (value, directory) {
            if (value == null) {
                this.extension.controler.show();
            } else {
                if (directory) gpac.last_working_directory = directory;
                this.extension.set_movie_url(value);
            }
            this.file_open_dlg = false;
        }

        filebrowse.on_close = function () {
            this.extension.controler.show();
            this.extension.file_open_dlg = false;
        }

        if (this.history.length) {
            filebrowse.go_hist = filebrowse.add_tool('history');
            filebrowse.go_hist.on_click = function () {
                var hist_items = [];
                var history = this.dlg.extension.history;
                for (var i = 0; i < history.length; i++) {
                    var o = {};
                    var f = history[history.length - i - 1];
                    var names = f.url.split('/');
                    if (names.length == 0) names = f.url.split('\\');

                    o.name = names.pop();
                    o.directory = false;
                    o.path = f.url;
                    var delim = f.url.slice(-1);
                    hist_items.push(o);
                }
                this.dlg.set_label('History');
                this.dlg.browse(hist_items);
            }
        }

        filebrowse.go_fav = filebrowse.add_tool('favorite');
        filebrowse.go_fav.on_click = function () {
            var fav_items = [];
            var bookmarks = this.dlg.extension.bookmarks;
            for (var i = 0; i < bookmarks.length; i++) {
                var o = {};
                var f = bookmarks[bookmarks.length - i - 1];
                var names = f.url.split('/');
                if (names.length == 0) names = f.url.split('\\');

                o.name = names.pop();
                if (o.name == '') {
                    o.directory = true;
                    o.name = names.pop();
                } else {
                    o.directory = false;
                }
                o.path = f.url;
                var delim = f.url.slice(-1);
                fav_items.push(o);
            }
            this.dlg.set_label('Bookmarks');
            this.dlg.browse(fav_items);
        }
        if (this.bookmarks.length) filebrowse.go_fav.show();
        else filebrowse.go_fav.hide();

        filebrowse.on_long_click = function (filename, path, directory) {
            var popup = gw_new_window_full(null, true, '');
            popup.dlg = this;
            this.disable();
            popup.area = gw_new_grid_container(popup);
            popup.area.dlg = popup;
            popup.area.break_at_line = true;
            var hist_idx = this.extension.is_in_history(path, true);

            if (hist_idx > -1) {
                var item = gw_new_icon_button(popup.area, 'history', 'Remove from history', true, 'listitem');
                item.dlg = popup;
                item.set_size(0.9 * this.width, gwskin.default_control_height);
                item.index = hist_idx;
                item.on_click = function () {
                    var fb_dlg = this.dlg.dlg;
                    fb_dlg.extension.history.splice(this.index, 1);
                    gpac.set_option('GUI', 'PlaybackHistory', JSON.stringify(fb_dlg.extension.history));
                    fb_dlg.go_hist.on_click();
                    this.dlg.close();
                }
            }

            if (this.extension.history.length) {
                var item = gw_new_icon_button(popup.area, 'trash', 'Clear all history', true, 'listitem');
                item.dlg = popup;
                item.set_size(0.9 * this.width, gwskin.default_control_height);
                item.index = hist_idx;
                item.on_click = function () {
                    var fb_dlg = this.dlg.dlg;
                    fb_dlg.extension.history = [];
                    gpac.set_option('GUI', 'PlaybackHistory', JSON.stringify(fb_dlg.extension.history));
                    fb_dlg.close();
                    this.dlg.close();
                }
            }

            var hist_idx = this.extension.is_in_history(path, false);
            item = gw_new_icon_button(popup.area, 'favorite', hist_idx > -1 ? 'Remove from favorites' : 'Add to favorites', true, 'listitem');
            item.dlg = popup;
            item.set_size(0.9 * this.width, gwskin.default_control_height);
            item.index = hist_idx;
            item.on_click = function () {
                var fb_dlg = this.dlg.dlg;
                if (this.index > -1) {
                    fb_dlg.extension.bookmarks.splice(this.index, 1);
                    gpac.set_option('GUI', 'Bookmarks', JSON.stringify(fb_dlg.extension.bookmarks));
                    if (fb_dlg.extension.bookmarks.length == 0) {
                        fb_dlg.go_fav.hide();
                        fb_dlg.browse(fb_dlg.directory);
                    }
                } else {
                    fb_dlg.extension.add_bookmark(path, false, filename);
                    fb_dlg.go_fav.show();
                }
                fb_dlg.layout(this.dlg.dlg.width, this.dlg.dlg.height);
                this.dlg.close();
            }


            popup.set_size(0.9 * this.width, 0.9 * this.height);
            popup.set_alpha(this.get_alpha());

            popup.on_close = function () {
                this.dlg.enable();
            }
            popup.show();
        }


        filebrowse.go_net = filebrowse.add_tool('remote_location');
        filebrowse.go_net.on_click = function () {
            var popup = gw_new_window_full(null, true, 'Enter Adress');
            popup.dlg = this.dlg;
            this.dlg.disable();
            popup.area = gw_new_grid_container(popup);
            popup.area.dlg = popup;
            popup.area.spread_h = true;

            popup.edit = gw_new_text_edit(popup.area, '');
            popup.edit.dlg = popup;
            gpac.set_focus(popup.edit);
            popup.edit.on_text = function (val) {
                if (val != '') {
                    this.dlg.dlg.on_browse(val, null);
                    this.dlg.dlg.close();
                }
                this.dlg.close();
            }

            popup.on_display_size = function (w, h) {
                this.edit.set_size(0.8 * w, 2 * gwskin.default_text_font_size);
                this.set_size(0.8 * w, 2 * gwskin.default_text_font_size + gwskin.default_icon_height);
            }

            popup.on_display_size(gw_display_width, gw_display_height);
            popup.set_alpha(1.0);

            popup.on_close = function () {
                this.dlg.enable();
            }
            popup.show();
        }


        filebrowse.on_display_size(gw_display_width, gw_display_height);
        if (gpac.hardware_rgba) filebrowse.set_alpha(0.8);

        this.controler.hide();
        filebrowse.show();
    },


    set_speed: function (value) {
        this.movie_control.mediaSpeed = value;
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

    is_in_history: function (url, is_history) {
        var search_in = is_history ? this.history : this.bookmarks;
        var hist = search_in.filter(function (obj) { return obj.url == url; });
        if (hist.length) {
            var index = search_in.indexOf(hist[0]);
            return index;
        }
        return -1;
    },

    add_bookmark: function (url, is_history) {
        var store = is_history ? this.history : this.bookmarks;
        var hist = store.filter(function (obj) { return obj.url == url; });
        if (hist.length) {
            var index = store.indexOf(hist[0]);
            if (index > -1) store.splice(index, 1);
            hist = hist[0];
        } else {
            hist = {};
            hist.url = url;
            hist.nb_access = 0;
        }
        hist.nb_access++;
        var t = new Date()
        hist.time = t.getTime();
        store.push(hist);

        if (is_history) {
            var nb_max = 3;
            if (this.history.length > nb_max) {
                var rem = this.history.length - nb_max;
                this.history.splice(-nb_max, rem);
            }
        }

        gpac.set_option('GUI', is_history ? 'PlaybackHistory' : 'Bookmarks', JSON.stringify(store));
    },

    view_stats: function () {
        if (this.stat_wnd) {
            this.stat_wnd.close();
            this.stat_wnd = null;
            return;
        }

        var wnd = gw_new_window_full(null, true, 'Stats');
        gw_object_set_dragable(wnd);
        this.stat_wnd = wnd;
        wnd.extension = this;

        wnd.area = gw_new_grid_container(wnd);
        wnd.area.break_at_hidden = true;
        wnd.area.spread_h = false;
        wnd.area.spread_v = true;

        wnd.root_odm = gpac.get_object_manager(wnd.extension.current_url);
        var nb_http = wnd.root_odm.nb_http;
        var nb_buffering = 0;

        wnd.has_select = false;
        wnd.objs = [];
        for (var res_i = 0; res_i < wnd.root_odm.nb_resources; res_i++) {
            var m = wnd.root_odm.get_resource(res_i);
            if (!m) continue;
            wnd.objs.push(m);

            m.gui = {};

            var label = '' + m.type;
            label += ' ID ' + m.ID;
            if (m.width) label += ' (' + m.width + 'x' + m.height + ')';
            else if (m.samplerate) label += ' (' + m.samplerate + ' Hz ' + m.channels + ' channels)';

            m.gui.txt = gw_new_text(wnd.area, label, 'lefttext');


            m.gui.info = gw_new_icon(wnd.area, 'information');

            m.gui.info.odm = m;
            m.gui.info.on_click = function () {
                if (this.odm.gui.info_wnd) {
                    this.odm.gui.info_wnd.close();
                    return;
                }
                var odm = this.odm;
                var iw = gw_new_window_full(null, true, '' + odm.type + ' ' + odm.ID + ' statistics:');
                odm.gui.info_wnd = iw;
                gw_object_set_dragable(iw);
                iw.odm = odm;

                iw.on_close = function (width, height) {
                    this.timer.stop();
                    this.odm.gui.info_wnd = null;
                }

                iw.area = gw_new_text_area(iw, '');

                iw.on_display_size = function (width, height) {
                    var w = 30 * gwskin.default_text_font_size;
                    var h = 2 * gwskin.default_icon_height + 11 * gwskin.default_text_font_size;
                    this.set_size(w, h);
                }

                iw.timer = gw_new_timer(false);
                iw.timer.wnd = iw;
                iw.timer.set_timeout(0.1, true);
                iw.timer.on_event = function (val) {
                    var bw;
                    var m = this.wnd.odm;
                    if (!m) return;
                    var label = '';

                    this.wnd.children[0].spacing = 1.2;

                    if (m.width) {
                        var fps = m.frame_duration;
                        label += '' + m.width + 'x' + m.height + ' (' + m.par + ' ' + m.pixelformt + ')';
                    } else {
                        label += '' + m.samplerate + 'Hz ' + m.channels + ' channels';
                    }
                    label += '\n'
                    label += 'Status: ' + m.status + ' - clock time: ' + m.clock_time + ' (drift ' + m.clock_drift + ')';

                    label += '\n'
                    label += 'Composition Memory: ' + m.cb_unit_count + '/' + m.cb_capacity;

                    label += '\n'
                    label += 'Buffer: ' + m.buffer + ' ms (min ' + m.min_buffer + ' - max ' + m.max_buffer + ') ' + m.db_unit_count + ' AUs in DB';

                    label += '\n'
                    var dec_time = m.total_dec_time / m.dec_frames / 1000;
                    var max_time = m.max_dec_time / 1000;
                    label += '' + m.dec_frames + ' frames (' + m.drop_frames + ' dropped) - ' + Math.round(100 * dec_time) / 100 + ' ms/frame (max ' + Math.round(m.max_dec_time / 10) / 100 + ') ';

                    if (m.irap_frames && (m.dec_frames != m.irap_frames)) {
                        label += '\n'
                        dec_time = m.irap_dec_time / m.irap_frames / 1000;
                        label += 'Average GOP size: ' + Math.round(m.dec_frames / m.irap_frames) + ' - ' + Math.round(100 * dec_time) / 100 + ' ms/irap (max ' + Math.round(m.irap_max_time / 10) / 100 + ') ';
                    }
                    label += '\n'
                    label += 'Average bitrate: ';
                    bw = m.avg_bitrate;
                    if (bw < 8000000) label += '' + Math.round(bw / 1024) + ' kbps';
                    else label += '' + Math.round(bw / 1024 / 1024) + ' mbps';

                    label += ' - Maximum ';
                    bw = m.max_bitrate;
                    if (bw < 8000000) label += '' + Math.round(bw / 1024) + ' kbps';
                    else label += '' + Math.round(bw / 1024 / 1024) + ' mbps';

                    label += '\n'
                    label += 'Download bandwidth: ';
                    var bw = m.bandwidth_down;
                    if (bw < 8000000) label += '' + Math.round(bw / 1024) + ' kbps';
                    else label += '' + Math.round(bw / 1024 / 1024) + ' mbps';

                    label += '\n'
                    label += 'Codec: ' + m.codec;

                    label += '\n'

                    var myurl;
                    if (m.service_url.indexOf('\\') >= 0) {
                        myurl = m.service_url.split('\\');
                    } else {
                        myurl = m.service_url.split('/');
                    }

                    label += 'Service: ' + myurl.pop();
                    if (m.service_id)
                        label += ' - ID ' + m.service_id;

                    this.wnd.area.set_content(label);
                }

                odm.gui.info_wnd.on_display_size(gw_display_width, gw_display_height);
                odm.gui.info_wnd.set_alpha(0.9);
                odm.gui.info_wnd.show();

            }

            m.gui.buffer = null;
            m.gui.play = null;
            if (m.max_buffer) {
                nb_buffering++;
                if (m.selected_service == m.service_id) {
                    m.gui.buffer = gw_new_gauge(wnd.area, 'Buffer');
                } else if (!m.is_addon) {
                    m.gui.play = gw_new_icon(wnd.area, 'play');
                    m.gui.play.odm = m;
                    m.gui.play.wnd = wnd;
                    m.gui.play.on_click = function () {
                        this.odm.select_service(this.odm.service_id);
                        this.wnd.close_all();
                    }
                }
            }

            if (m.nb_qualities > 1) {
                wnd.has_select = true;
                m.gui.select_label = gw_new_button(wnd.area, 'Quality');
                m.gui.select_label.odm = m;
                m.gui.select_label.on_click = function () {
                    var idx = this.odm.gui.select.value;
                    if (this.odm.gui.selected_idx == idx) {
                        this.on_long_click();
                        return;
                    }

                    if (idx) {
                        if (this.odm.gui.qualities[idx - 1].disabled) {
                            var notif = gw_new_message(null, 'Cannot select representation', 'Representation has been disabled');
                            notif.set_size(20 * gwskin.default_text_font_size, (2 * gwskin.default_icon_height + gwskin.default_text_font_size));
                            notif.show();
                            return;
                        }
                        this.odm.select_quality(this.odm.gui.qualities[idx - 1].ID);
                    } else {
                        this.odm.select_quality('auto');

                        this.odm.update_qualities();
                    }
                }
                m.gui.select_label.on_long_click = function () {
                    var idx = this.odm.gui.select.value;
                    if (!idx) return;
                    var q = this.odm.gui.qualities[idx - 1];

                    var iw = gw_new_window_full(null, true, 'Representation ' + q.ID + ' info');
                    gw_object_set_dragable(iw);
                    iw.area = gw_new_text_area(iw, '');

                    var label = 'Mime ' + q.mime + ' - codecs ' + q.codec;
                    label += '\n';
                    label += 'Bandwidth: ' + q.bandwidth;
                    label += '\n';
                    label += 'Enabled: ' + !q.disabled;
                    label += '\n';
                    if (q.width) {
                        label += 'Size ' + q.width + 'x' + q.height + (q.interlaced ? ' interlaced @' : ' progressive @') + q.fps + ' FPS - SAR ' + q.par_num + '/' + q.par_den;
                    } else {
                        label += 'Samplerate ' + q.samplerate + ' Hz - ' + q.channels + ' channels';
                    }

                    iw.area.set_content(label);

                    iw.on_display_size = function (width, height) {
                        var w = 25 * gwskin.default_text_font_size;
                        var h = 2 * gwskin.default_icon_height + 5 * gwskin.default_text_font_size;
                        this.set_size(w, h);
                    }

                    iw.on_display_size(gw_display_width, gw_display_height);
                    iw.set_alpha(0.9);
                    iw.show();
                }

                m.gui.select = gw_new_spincontrol(wnd.area, true);
                m.gui.select.odm = m;
                m.gui.select.on_click = function (val) {
                    this.odm.show_select(val);
                }


                m.update_qualities = function () {
                    var sel_idx = 0;
                    this.gui.qualities = [];
                    for (i = 0; i < this.nb_qualities; i++) {
                        var q = this.get_quality(i);
                        if (q) {
                            this.gui.qualities.push(q);
                        }
                    }
                    this.gui.qualities.sort(function (a, b) { return a.bandwidth - b.bandwidth });

                    for (i = 0; i < this.gui.qualities.length; i++) {
                        if (this.gui.qualities[i].is_selected)
                            sel_idx = i + 1;
                    }

                    this.gui.select.min = 0;
                    this.gui.select.max = m.nb_qualities;
                    if (this.gui.selected_idx != sel_idx) {
                        this.show_select(sel_idx);
                        this.gui.selected_idx = sel_idx;
                        this.gui.select.value = sel_idx;
                    }
                }

                m.show_select = function (idx) {
                    if (!idx) {
                        this.gui.select_label.set_label('Auto');
                    } else {
                        var label = '';
                        var q = this.gui.qualities[idx - 1];
                        if (!q) return;

                        if (q.is_selected) label += '*';

                        if (q.width) {
                            label += q.height + (q.interlaced ? 'i' : 'p');
                        } else if (q.samplerate) {
                            label += q.samplerate;
                            if (q.channels) label += '/' + q.channels;
                        }
                        if (q.bandwidth < 1000000) label += '@' + Math.round(q.bandwidth / 1000) + 'K';
                        else label += '@' + Math.round(q.bandwidth / 10000) / 100 + 'M';

                        this.gui.select_label.set_label(label);
                    }
                }
                m.update_qualities();

            } else {
                m.gui.select = null;
                m.gui.select_label = null;
            }

            nb_http += m.nb_http;

            gw_new_separator(wnd.area);
        }

        wnd.http_control = null;
        if (nb_http) {
            wnd.http_text = gw_new_text(wnd.area, 'HTTP rate', 'lefttext');
            wnd.http_control = gw_new_slider(wnd.area);

            if (gpac.http_max_bitrate) {
                var br = Math.round(100 * gpac.http_max_bitrate / 1024 / 1024) / 100;
                if (br > 200) bt = 200;
                if (br <= 1) {
                    v = 30 * br;
                } else if (br <= 10) {
                    v = 30 + 30 * br / 10;
                } else if (br < 50) {
                    v = 60 + 20 * (br - 10) / 40;
                } else {
                    v = 80 + 20 * (br - 50) / 150;
                }
                wnd.http_control.set_value(v);

                wnd.http_text.set_label('HTTP cap ' + Math.round(100 * gpac.http_max_bitrate / 1024 / 1024) / 100 + ' Mbps');

            } else {
                wnd.http_control.set_value(0);
                wnd.http_text.set_label('HTTP cap off');
            }

            wnd.http_control.text = wnd.http_text;
            wnd.http_control.on_slide = function (val) {
                var br = 0;
                if (val <= 30) {
                    br = val / 30;
                } else if (val <= 60) {
                    br = 1 + 10 * (val - 30) / 30;
                } else if (val < 80) {
                    br = 10 + 40 * (val - 60) / 20;
                } else {
                    br = 50 + 150 * (val - 80) / 20;
                }
                if (br) {
                    this.text.set_label('HTTP cap ' + Math.round(100 * br) / 100 + ' Mbps');
                } else {
                    this.text.set_label('HTTP cap off');
                }
                gpac.http_max_bitrate = Math.round(br * 1024 * 1024);
            }

            gw_new_separator(wnd.area);
        }

        wnd.plot = gw_new_plotter(wnd.area);


        wnd.on_display_size = function (width, height) {
            var w;
            var h = 2 * gwskin.default_icon_height;

            w = 20 * gwskin.default_text_font_size;
            w += 4 * gwskin.default_icon_height;
            if (this.has_select)
                w += 4 * gwskin.default_icon_height;

            for (var i = 0; i < this.objs.length; i++) {
                var aw = w;

                this.objs[i].gui.info.set_size(1.5 * gwskin.default_icon_height, gwskin.default_icon_height);
                aw -= this.objs[i].gui.info.width;

                if (this.objs[i].gui.buffer) {
                    this.objs[i].gui.buffer.set_size(3 * gwskin.default_icon_height, 0.75 * gwskin.default_icon_height);
                }
                if (this.objs[i].gui.play) {
                    this.objs[i].gui.play.set_size(gwskin.default_icon_height, gwskin.default_icon_height);
                }
                aw -= 4 * gwskin.default_icon_height;

                if (this.objs[i].gui.select) {
                    this.objs[i].gui.select.set_size(gwskin.default_icon_height, gwskin.default_icon_height);
                    aw -= gwskin.default_icon_height;
                    this.objs[i].gui.select_label.set_size(4 * gwskin.default_icon_height, 0.75 * gwskin.default_icon_height);
                    aw -= 4 * gwskin.default_icon_height;
                }


                this.objs[i].gui.txt.set_size(aw, gwskin.default_icon_height);
                this.objs[i].gui.txt.set_width(aw);
                h += gwskin.default_icon_height;
            }

            if (this.plot) {
                this.plot.set_size(w, 8 * gwskin.default_icon_height);
                h += 8 * gwskin.default_icon_height;
            }

            if (wnd.http_control) {
                wnd.http_text.set_size(11 * gwskin.default_text_font_size, gwskin.default_icon_height);
                wnd.http_control.set_size(w - 11 * gwskin.default_text_font_size, 0.2 * gwskin.default_icon_height);
                h += gwskin.default_icon_height;
            }


            this.set_size(w, h);
            this.move((w - width) / 2, (height - h) / 2);
        }
        wnd.on_close = function () {
            this.timer.stop();
            this.objs = [];
            this.extension.stat_wnd = null;
        }

        wnd.timer = gw_new_timer(false);
        wnd.timer.wnd = wnd;
        wnd.timer.set_timeout(0.25, true);
        wnd.timer.cores = gpac.nb_cores;
        if (gpac.system_memory > 1000000000) wnd.timer.mem = '' + Math.round(gpac.system_memory / 1024 / 1024 / 1024) + ' GB';
        else wnd.timer.mem = '' + Math.round(gpac.system_memory / 1024 / 1024) + ' MB';

        wnd.stats = [];
        wnd.stats_window = 32;

        if (wnd.plot) {
            wnd.s_fps = wnd.plot.add_serie('FPS', 'Hz', 0.8, 0, 0);
            if (nb_http) {
                wnd.s_bw = wnd.plot.add_serie('BW', 'Mbps', 0.8, 0, 0.8);
            } else {
                wnd.s_bw = null;
            }
            wnd.s_bitrate = wnd.plot.add_serie('Rate', 'Mbps', 0, 0.8, 0);
            if (nb_buffering)
                wnd.s_buf = wnd.plot.add_serie('Buffer', 's', 0, 0, 0.8);
            else
                wnd.s_buf = null;
        }

        wnd.timer.on_event = function (val) {
            var wnd = this.wnd;

            wnd.set_label('Stats FPS ' + Math.round(100 * gpac.fps) / 100 + ' CPU ' + gpac.cpu + '% RAM ' + Math.round(gpac.memory / 1024 / 1024) + ' MB  ' + this.cores + ' cores ' + this.mem + ' RAM');

            var stat_obj = null;
            //stats every second
            if (!(val % 4)) {
                if (wnd.stats.length >= wnd.stats_window) {
                    wnd.stats.splice(0, 1);
                }
                stat_obj = {};
                wnd.stats.push(stat_obj);

                var t = new Date()
                stat_obj.time = Math.round(t.getTime() / 1000);
                stat_obj.fps = Math.round(100 * gpac.fps) / 100;
                stat_obj.cpu = gpac.cpu;
                stat_obj.memory = gpac.memory;
                stat_obj.bitrate = 0;
                if (wnd.s_bw) {
                    var b = gpac.http_bitrate / 1024 / 1024;
                    stat_obj.http_bandwidth = Math.round(100 * b) / 100;
                }
                stat_obj.buffer = 0;
                stat_obj.cumulated_bandwidth = 0;
            }

            for (var i = 0; i < wnd.objs.length; i++) {
                var m = wnd.objs[i];

                if (m.gui.buffer) {
                    var label = ' ' + m.type;
                    label += ' ID ' + m.ID;
                    if (m.width) label += ' (' + m.width + 'x' + m.height + ')';
                    else if (m.samplerate) label += ' (' + m.samplerate + ' Hz ' + m.channels + ' channels)';
                    m.gui.txt.set_label(label);

                    var bl;
                    if (m.max_buffer) {
                        var speed = wnd.extension.movie_control.mediaSpeed;
                        if (speed < 0) speed = 1;
                        else if (speed == 0) speed = 1;
                        var buf = m.buffer / speed;
                        bl = 100 * buf / m.max_buffer;

                        if (stat_obj) {
                            if (stat_obj.buffer < buf) {
                                stat_obj.buffer = buf;
                            }
                        }
                    }
                    else bl = 100;
                    m.gui.buffer.set_value(bl);

                    m.gui.buffer.set_label('' + Math.round(m.buffer / 10) / 100 + ' s');
                }

                if (stat_obj) {
                    stat_obj.bitrate += Math.round(100 * m.avg_bitrate / 1024 / 1024) / 100;
                    stat_obj.cumulated_bandwidth += m.bandwidth_down;
                }
            }

            if (stat_obj && wnd.stats.length) {
                wnd.s_fps.refresh_serie(wnd.stats, 'time', 'fps', wnd.stats_window, 8);
                if (wnd.s_bw) {
                    wnd.s_bw.refresh_serie(this.wnd.stats, 'time', 'http_bandwidth', wnd.stats_window, 1);
                }
                if (wnd.s_buf) {
                    wnd.s_buf.refresh_serie(this.wnd.stats, 'time', 'buffer', wnd.stats_window, 2);
                }
                wnd.s_bitrate.refresh_serie(this.wnd.stats, 'time', 'bitrate', wnd.stats_window, 4);
            }
        }

        wnd.quality_changed = function () {
            for (var i = 0; i < this.objs.length; i++) {
                var m = this.objs[i];
                m.update_qualities();
            }
        }
        wnd.close_all = function () {
            for (var i = 0; i < this.objs.length; i++) {
                if (this.objs[i].gui.info_wnd) {
                    this.objs[i].gui.info_wnd.odm = null;
                    this.objs[i].gui.info_wnd.close();
                }
            }
            this.objs.length = 0;
            this.close();
        }

        wnd.on_display_size(gw_display_width, gw_display_height);
        wnd.set_alpha(0.9);
        wnd.show();

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
        }

        this.buffer_wnd.txt.set_label('Buffering ' + level + ' %');
    }

};

