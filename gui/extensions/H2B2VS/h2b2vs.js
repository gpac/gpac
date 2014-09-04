extension = {
    setup: false,
    dialog: null,
    active_addon: null,

    ext_filter_event: function (evt) {
        switch (evt.type) {
            case GF_EVENT_ADDON_DETECTED:
                this.confirm_addon(evt);
                return true;
            default:
                return false;
        }
    },

    create_event_filter: function (__anobj) {
        return function (evt) {
            __anobj.ext_filter_event(evt);
        }
    },

    start: function () {
        //first launch - register event filter and exit
        if (!this.setup) {
            gwlib_add_event_filter(this.create_event_filter(this));
            this.setup = true;
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
        gpac.set_focus(wnd.edit);
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
            gpac.set_option('Systems', 'DebugPVRScene', value ? 'yes' : 'no');
        }
        do_sel = gpac.get_option('Systems', 'DebugPVRScene');
        wnd.dbg_addon.set_checked((do_sel == 'yes') ? true : false);


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

            this.set_size(w, 12 * gwskin.default_icon_height);
        }

        wnd.check_pos(parseInt(this.get_option('OverlayPosition', '0')));
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
        if (this.active_url) {
            var odm = gpac.get_object_manager(this.active_url);
            if (odm) {
                odm.addon_layout(parseInt(this.get_option('OverlayPosition', '0')), parseInt(this.get_option('OverlaySize', '0')));
            }
        }
    },

    do_activate_addon: function (obj) {
        var odm = gpac.get_object_manager(obj.scene_url);
        if (odm) {
            odm.enable_addon(obj.addon_url);
            odm.addon_layout(parseInt(this.get_option('OverlayPosition', '0')), parseInt(this.get_option('OverlaySize', '0')));
        }
        this.active_url = obj.scene_url;
    },

    confirm_addon: function (evt) {

        if (this.get_option('AutoSelect', 'no') == 'yes') {
            this.do_activate_addon(evt);
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
            this.extension.do_activate_addon(this);
        }
    }
};

