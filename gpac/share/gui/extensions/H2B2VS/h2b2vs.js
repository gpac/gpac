extension = {
    setup: false,
    dialog: null,
    uhd_demo_enabled: false,
    uhd_demo_on: false,
    uhd_state_on: true,
    addon_url: null,
    scene_url: null,
    overlay_position: 0,
    icon_width: 0,
    icon_height: 0,
    movie_width: 0,
    movie_height: 0,
    disable_save_session: false,
    disable_addons: false,

    toggle_uhd_demo: function (val) {
        this.uhd_demo_on = val;
        var notif = null;
        if (this.uhd_demo_on) {
            notif = gw_new_message(null, 'UHD Demo Enbaled', 'Click to toggle quality');
        } else {
            notif = gw_new_message(null, 'UHD Demo Disabled', 'Double-click to re-enable');
            this.logo.children[0].url[0] = '';
        }
        this.do_layout();
        notif.set_size(20 * gwskin.default_text_font_size, gwskin.default_icon_height + 2 * gwskin.default_text_font_size);
        notif.show();
    },

    ext_filter_event: function (evt) {
        switch (evt.type) {
            case GF_EVENT_ADDON_DETECTED:
                this.confirm_addon(evt);
                return true;
            case GF_EVENT_QUIT:
                this.save_session();
                return false;
            case GF_EVENT_DBLCLICK:
                if (this.uhd_demo_enabled) {
                    this.toggle_uhd_demo(!this.uhd_demo_on);
                }
                return false;
            case GF_EVENT_MOUSEUP:
                if (this.uhd_demo_on) {
                    this.uhd_state_on = !this.uhd_state_on;
                    scene.switch_quality(this.uhd_state_on);
                    return true;
                }
                return false;

            case GF_EVENT_MOUSEDOWN:
                if (this.uhd_demo_on) {
                    return true;
                }
                return false;

            case GF_EVENT_SCENE_SIZE:
                if (typeof evt.width != 'undefined') {
                    this.movie_width = evt.width;
                    this.movie_height = evt.height;
                    if (this.movie_height > 1080) this.uhd_state_on = true;

                    if (this.uhd_demo_on) {
                        this.do_layout();
                    }
                }
                return false;
            case GF_EVENT_KEYDOWN:
                //alert('key is '+evt.keycode + ' hw code is ' + evt.hwkey);

                var odm = scene.get_object_manager(this.scene_url);
                if (!odm || odm.main_addon_url) return false;

                if (evt.keycode == 'F7') {
                    this.uhd_state_on = !this.uhd_state_on;
                    scene.switch_quality(this.uhd_state_on);
                    return true;
                }

                if (evt.keycode == 'F6') {
                    this.overlay_position++;
                    if (this.overlay_position == 4) {
                        this.do_deactivate_addon();

                    } else {
                        if (this.overlay_position == 5) {
                            this.do_activate_addon();
                            this.overlay_position = 0;
                        }
                        this.set_option('OverlayPosition', '' + this.overlay_position);
                        this.refresh_addon();
                    }
                    return true;
                }
/*
                if (evt.keycode == 'F12') {
                    scene.exit();
                    return true;
                }
*/
                return false;
			case GF_JS_EVENT_PLAYBACK:
				//whenever paused, store pause time
				if (! evt.is_playing) {
					this.save_session();
				}
				return false;
				
            default:
                return false;
        }
    },

    create_event_filter: function (__anobj) {
        return function (evt) {
            return __anobj.ext_filter_event(evt);
        }
    },

    do_layout: function () {
        if (this.uhd_demo_enabled && this.uhd_demo_on) {
            var url = this.get_option('path');
            if (this.movie_height > 1080) {
                url += 'logo_uhd.png';
                this.logo.scale.x = 1;
                this.logo.scale.y = 1;
            } else {
                url += 'logo_hd.png';
                this.logo.scale.x = 2;
                this.logo.scale.y = 2;
            }
            this.logo.children[0].url[0] = url;
        } else {
            this.logo.children[0].url[0] = '';
        }
    },

    start: function () {
        //first launch - register event filter and exit
        if (!this.setup) {
            gwlib_add_event_filter(this.create_event_filter(this), true);
            this.setup = true;

            this.overlay_position = parseInt(this.get_option('OverlayPosition', '0'));

            /*create media nodes element for playback*/
            this.logo = gw_new_container();
            this.logo.children[0] = new SFNode('Inline');
            this.logo.children[0].extension = this;
            this.logo.children[0].url[0] = '';
            this.logo.children[0].on_scene_size = function (evt) {
                this.extension.icon_width = evt.width;
                this.extension.icon_height = evt.height;
                this.extension.do_layout();
            };

            gw_add_child(null, this.logo);

            this.logo.children[0].addEventListener('gpac_scene_attached', this.logo.children[0].on_scene_size, 0);

            this.restore_session();

            //check our args
            var i;
            for (i = 1; i < Sys.args.length; i++) {
                var arg = Sys.args[i];
                if (arg == '-demo-uhd') {
                    this.uhd_demo_enabled = true;
                    this.toggle_uhd_demo(true);
                    gwlog(l_war, 'UHD Demo enabled');
                }
                else if (arg == '-no-addon') {
                    this.disable_addons = true;
                }
            }
            return;
        }

        gw_hide_dock();
        var wnd = gw_new_window_full(null, true, 'H2B2VS Preferences');

        this.dialog = wnd;
        this.dialog.extension = this;

        wnd.area = gw_new_grid_container(wnd);
        wnd.area.spread_h = true;
        wnd.area.break_at_hidden = true;

        wnd.txt1 = gw_new_text(wnd.area, 'Overlay Position');
        gw_new_separator(wnd.area);


        wnd.check_pos = function (value) {
            this.chk_pos1.set_checked((value == 0) ? true : false);
            this.chk_pos2.set_checked((value == 1) ? true : false);
            this.chk_pos3.set_checked((value == 2) ? true : false);
            this.chk_pos4.set_checked((value == 3) ? true : false);

            this.extension.set_option('OverlayPosition', '' + value);
            this.extension.refresh_addon();
        }
        wnd.chk_pos4 = gw_new_checkbox(wnd.area, 'Top-Left');
        wnd.chk_pos4.on_check = function (value) {
            this.parent.parent.check_pos(3);
        }
        wnd.chk_pos2 = gw_new_checkbox(wnd.area, 'Top-Right');
        wnd.chk_pos2.on_check = function (value) {
            this.parent.parent.check_pos(1);
        }
        wnd.chk_pos3 = gw_new_checkbox(wnd.area, 'Bottom-Left');
        wnd.chk_pos3.on_check = function (value) {
            this.parent.parent.check_pos(2);
        }
        wnd.chk_pos1 = gw_new_checkbox(wnd.area, 'Bottom-Right');
        wnd.chk_pos1.on_check = function (value) {
            this.parent.parent.check_pos(0);
        }

        wnd.txt2 = gw_new_text(wnd.area, 'Overlay Size');
        gw_new_separator(wnd.area);


        wnd.check_size = function (value) {
            this.chk_size1.set_checked((value == 0) ? true : false);
            this.chk_size2.set_checked((value == 1) ? true : false);
            this.chk_size3.set_checked((value == 2) ? true : false);
            this.extension.set_option('OverlaySize', '' + value);
            this.extension.refresh_addon();
        }
        wnd.chk_size1 = gw_new_checkbox(wnd.area, '1/2 Height');
        wnd.chk_size1.on_check = function (value) {
            this.parent.parent.check_size(0);
        }
        wnd.chk_size2 = gw_new_checkbox(wnd.area, '1/3 Height');
        wnd.chk_size2.on_check = function (value) {
            this.parent.parent.check_size(1);
        }
        wnd.chk_size3 = gw_new_checkbox(wnd.area, '1/4 Height');
        wnd.chk_size3.on_check = function (value) {
            this.parent.parent.check_size(2);
        }

        wnd.txt3 = gw_new_text(wnd.area, 'User Identifier');
        gw_new_separator(wnd.area);
        wnd.edit = gw_new_text_edit(wnd.area, this.get_option('UserID', 'H2B2VSUser'));
        scene.set_focus(wnd.edit);
        wnd.edit.on_text = function (val) {
            if (val != '') {
                this.parent.parent.extension.set_option('UserID', val);
            }
        }

        gw_new_separator(wnd.area);
        wnd.chk_addon = gw_new_checkbox(wnd.area, 'Auto-select addon');
        wnd.chk_addon.on_check = function (value) {
            this.parent.parent.extension.set_option('AutoSelect', value ? 'yes' : 'no');
        }
        var do_sel = this.get_option('AutoSelect', 'no');
        wnd.chk_addon.set_checked((do_sel == 'yes') ? true : false);

        wnd.dbg_addon = gw_new_checkbox(wnd.area, 'Debug PVR addon');
        wnd.dbg_addon.on_check = function (value) {
            scene.set_option('Compositor', 'dbgpvr', value ? 'yes' : 'no');
        }
        do_sel = scene.get_option('Compositor', 'dbgpvr');
        wnd.dbg_addon.set_checked((do_sel == 'yes') ? true : false);

        gw_new_separator(wnd.area);
        wnd.uhd_demo = gw_new_checkbox(wnd.area, 'UHD Demo');
        wnd.uhd_demo.on_check = function (value) {
            this.parent.parent.extension.uhd_demo_enabled = value;
            this.parent.parent.extension.set_option('UHDDemo', value ? 'yes' : 'no');
        }
        do_sel = this.get_option('UHDDemo', 'no');
        this.uhd_demo_enabled = (do_sel == 'yes') ? true : false;
        wnd.uhd_demo.set_checked(this.uhd_demo_enabled);
        if (this.uhd_demo_enabled) this.uhd_demo_on = true;

        wnd.on_display_size = function (width, height) {
            w = 0.9 * width;
            if (w > 500) w = 500;
            this.txt1.set_size(w / 3, gwskin.default_icon_height);

            this.chk_pos1.set_size(w / 2, gwskin.default_control_height);
            this.chk_pos2.set_size(w / 2, gwskin.default_control_height);
            this.chk_pos3.set_size(w / 2, gwskin.default_control_height);
            this.chk_pos4.set_size(w / 2, gwskin.default_control_height);

            this.txt2.set_size(w / 3, gwskin.default_icon_height);

            this.chk_size1.set_size(w / 3, gwskin.default_control_height);
            this.chk_size2.set_size(w / 3, gwskin.default_control_height);
            this.chk_size3.set_size(w / 3, gwskin.default_control_height);

            this.txt3.set_size(w / 3, gwskin.default_icon_height);
            this.edit.set_size(w / 2, gwskin.default_icon_height);
            this.chk_addon.set_size(w / 2, gwskin.default_icon_height);
            this.dbg_addon.set_size(w / 2, gwskin.default_icon_height);
            this.uhd_demo.set_size(w / 2, gwskin.default_icon_height);

            this.set_size(w, 13 * gwskin.default_icon_height);
        }

        wnd.check_pos(this.overlay_position);
        wnd.check_size(parseInt(this.get_option('OverlaySize', '0')));


        wnd.on_display_size(gw_display_width, gw_display_height);
        wnd.set_alpha(0.9);
        wnd.show();


        wnd.on_close = function () {
            gw_show_dock();
            wnd.extension.dialog = null;
        };
    },

    refresh_addon: function () {
        if (this.scene_url) {
            var odm = scene.get_object_manager(this.scene_url);
            if (odm) {
                odm.addon_layout(parseInt(this.get_option('OverlayPosition', '0')), parseInt(this.get_option('OverlaySize', '0')));
            }
        }
    },

    do_activate_addon: function () {
        var odm = scene.get_object_manager(this.scene_url);
        if (odm) {
            odm.enable_addon(this.addon_url);
            odm.addon_layout(parseInt(this.get_option('OverlayPosition', '0')), parseInt(this.get_option('OverlaySize', '0')));
        }
    },

    do_deactivate_addon: function () {
        var odm = scene.get_object_manager(this.scene_url);
        if (odm) {
            odm.enable_addon(this.addon_url, true);
        }
    },


    confirm_addon: function (evt) {

        if (this.disable_addons) return;

        if (this.get_option('AutoSelect', 'no') == 'yes') {
            this.scene_url = evt.scene_url;
            this.addon_url = evt.addon_url;
            this.do_activate_addon();
            return;
        }

        var names = ext = evt.addon_url.split('/');
        if (names.length == 0) names = f.url.split('\\');

        var dlg = gw_new_confirm_wnd(null, 'Addon detected (' + names.pop() + '), enable it ?');
        dlg.set_alpha(0.95);
        dlg.show();
        dlg.extension = this;
        dlg.scene_url = evt.scene_url;
        dlg.addon_url = evt.addon_url;

        dlg.on_confirm = function (value) {
            if (!value) return;
            this.extension.scene_url = evt.scene_url;
            this.extension.addon_url = evt.addon_url;
            this.extension.do_activate_addon();
        }
    },

    do_xhr: function (url, cmd) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', url, false);

        xhr.setRequestHeader('Content-Type', 'application/json');
        xhr.setRequestHeader('Content-Length', cmd.length);
        xhr.send(cmd);

        if ((xhr.status != 200) || (xhr.readyState != 4)) {
            if (xhr.status) {
                gwlog(l_err, '[H2B2VS] Failed to query server: ' + xhr.responseText);
            } else {
                gwlog(l_err, '[H2B2VS] Failed to send request');
            }
            return null;
        }
        gwlog(l_deb, 'Command sent is ' + cmd + ' - response is ' + xhr.responseText);

        var obj = gwskin.parse(xhr.responseText);
        if (typeof obj.result == 'undefined') {
            gwlog(l_err, '[H2B2VS] Non conformant response object ' + xhr.responseText);
            return null;
        }
        if (obj.result != 0) {
            gwlog(l_inf, '[H2B2VS] No session found for user - ' + xhr.responseText);
            return null;
        }
        return obj;
    },

    restore_session: function () {

        if (gwskin.media_url) {
            gwlog(l_inf, 'URL was given when opening, skipping session restore');
            return;
        }
        //this.disable_save_session = true;

        var server = this.get_option('SessionServer', null);
        var user = this.get_option('UserID', null);
        if (!server || !user) return;

        var url = server + 'getData';
        var cmd = 'ID=' + user;

        var obj = this.do_xhr(url, cmd);
        if (!obj || !obj.data || !obj.data.url || (obj.data.url== '') ) return;

        var dlg = gw_new_confirm_wnd(null, 'Restore last session ?');
        dlg.set_alpha(0.95);
        dlg.show();
        dlg.sess = obj.data;

        gwlog(l_deb, 'H2B2VS state to restore: ' + gwskin.stringify(obj.data));

        dlg.on_confirm = function (value) {
            if (!value) return;
            gwskin.restore_session(this.sess.url, this.sess.media_time, this.sess.media_clock);
        }
    },

    save_session: function () {

        if (this.disable_save_session) return;

        var obj = {};
        var url = gwskin.pvr_url;
        if (url === '') url = gwskin.media_url;

        obj.url = url.replace(/\\/g, "/");
        obj.media_time = 0;
        obj.media_clock = 0;
        if (typeof gwskin.media_time != 'undefined') obj.media_time = gwskin.media_time;
        if (typeof gwskin.media_clock != 'undefined') obj.media_clock = gwskin.media_clock;

        var str = gwskin.stringify(obj);
        gwlog(l_deb, 'H2B2VS storing state: ' + str);

		var server = this.get_option('SessionServer', null);
		var user = this.get_option('UserID', null);
		if (!server || !user) return;

		var url = server + 'setData';
        var cmd = 'ID=' + user + '&Data=' + str;
        this.do_xhr(url, cmd);

    }
};

