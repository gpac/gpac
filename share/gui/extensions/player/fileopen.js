extension.open_local_file = function () {
    var filebrowse = gw_new_file_dialog(null, 'Open file');
    filebrowse.filter = '*';
    filebrowse.browse(Sys.last_wdir);

    filebrowse.extension = this;
    this.file_open_dlg = true;

    if (!gwskin.mobile_device)
        gw_object_set_dragable(filebrowse);

    filebrowse.on_display_size = function (width, height) {
        if (gwskin.mobile_device) {
			dh = 0;
			if (gwskin.display_cutout) {
				dh = gwskin.default_control_height;
			}
            this.set_size(width, height-dh);
            this.move(0, -dh/2);
        } else {
            var w = width / 2;
            if (w < 200) w = width - 20;
            if (w > 500) w = 500;
            this.set_size(w, 0.8 * height);
            this.move((width - w) / 2, 0);
        }
    }

    filebrowse.on_browse = function (value, directory) {
        if (value == null) {
            this.extension.controler.show();
        } else {
            if (directory) Sys.last_wdir = directory;
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
                if (names.length == 0) {
                     o.name = names.pop();
                } else {
                    o.name = f.url;
                }
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
                fb_dlg.extension.set_option('PlaybackHistory', gwskin.stringify(fb_dlg.extension.history));
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
                fb_dlg.extension.set_option('PlaybackHistory', gwskin.stringify(fb_dlg.extension.history));
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
                fb_dlg.extension.set_option('Bookmarks', gwskin.stringify(fb_dlg.extension.bookmarks));
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

        var item = gw_new_icon_button(popup.area, 'view360', 'View as 360', true, 'listitem');
        item.dlg = popup;
        item.set_size(0.9 * this.width, gwskin.default_control_height);
        item.index = hist_idx;
        item.on_click = function () {
            var fb_dlg = this.dlg.dlg;
            path += '#VR';
            fb_dlg.extension.set_movie_url(path);
            if (directory) Sys.last_wdir = directory;
            fb_dlg.close();
            this.dlg.close();
        }


        popup.set_size(0.9 * this.width, 0.9 * this.height);
        popup.set_alpha(1);
        popup.move(this.translation.x, this.translation.y);

        popup.on_close = function () {
            this.dlg.enable();
        }
        popup.show();
    }


    filebrowse.go_net = filebrowse.add_tool('remote_location');
    filebrowse.go_net.on_click = function () {
        var popup = gw_new_window_full(null, true, 'Enter Address');
        popup.dlg = this.dlg;
        this.dlg.disable();
        popup.area = gw_new_grid_container(popup);
        popup.area.dlg = popup;
        popup.area.spread_h = true;

        popup.edit = gw_new_text_edit(popup.area, '');
        popup.edit.dlg = popup;
        scene.set_focus(popup.edit);
        popup.edit.on_text = function (val) {
            if (val != '') {
                this.dlg.dlg.on_browse(val, null);
                this.dlg.dlg.close();
                this.dlg.dlg = null;
            }
            this.dlg.close();
        }

        popup.on_display_size = function (w, h) {
			let ed_h = 2 * gwskin.default_text_font_size;
			if (gwskin.mobile_device) ed_h *= 1.5;
            this.edit.set_size(w, ed_h);
            this.set_size(w, ed_h + gwskin.default_icon_height);
//            this.move(0, h / 2 - this.height / 2);
			this.move(0, 0);
        }

        popup.on_display_size(gw_display_width, gw_display_height);
        popup.set_alpha(1.0);

        popup.on_close = function () {
            if (this.dlg) {
                this.dlg.enable();
            }
        }
        popup.show();
    }

    filebrowse.on_display_size(gw_display_width, gw_display_height);
    if (scene.hardware_rgba) filebrowse.set_alpha(0.8);

    this.controler.hide();
    filebrowse.show();

    return filebrowse;
};
    
    
extension.is_in_history = function (url, is_history) {
        var search_in = is_history ? this.history : this.bookmarks;
        var hist = search_in.filter(function (obj) { return obj.url == url; });
        if (hist.length) {
            var index = search_in.indexOf(hist[0]);
            return index;
        }
        return -1;
    };

extension.add_bookmark = function (url, is_history) {
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

        this.set_option(is_history ? 'PlaybackHistory' : 'Bookmarks', gwskin.stringify(store));
    };
