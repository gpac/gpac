
extension.playlist_wnd = null;

extension.playlist_filter_event = function(evt) {
	switch (evt.type) {
	case GF_JS_EVENT_PLAYLIST_ADD:
		this.add_playlist_item(evt.url);
		return true;
	case GF_JS_EVENT_PLAYLIST_RESET:
		this.reset_playlist();
		return true;
	case GF_JS_EVENT_PLAYLIST_PLAY:
		this.playlist_idx = evt.index - 1;
		this.start();
			this.playlist_next();
		return true;
	default:
		return false;
	}
}

extension.create_playlist_event_filter = function (__anobj) {
	return function (evt) {
		return __anobj.playlist_filter_event(evt);
	}
}

extension.pl_event_filter = null;


extension.playlist = [];
var plist = extension.get_option('Playlist');

if ((plist != null) && (plist != '')) {
    extension.playlist = gwskin.parse(plist);
}

extension.playlist_idx = 0;
var plistidx = extension.get_option('PlaylistIndex');
if (plistidx != null) {
    extension.playlist_idx = parseInt(plistidx);
}

extension.view_playlist = function () {
    if (this.playlist_wnd) {
        this.playlist_wnd.close();
        this.playlist_wnd = null;
        return;
    }

    var plist = gw_new_window_full(null, true, 'Playlist');
    this.playlist_wnd = plist;
    plist.extension = this;
    plist.hide_controler = false;
    
    this.controler.hide();
    
    plist.on_close = function () {
        if (! this.hide_controler)
            this.extension.controler.show();

        this.extension.playlist_wnd = null;
        this.extension.file_open_dlg = false;
    }

    plist.area = gw_new_grid_container(plist);
    plist.area.break_at_line = true;
    plist.area.dlg = plist;

    plist.go_prev = plist.add_tool('previous');
    plist.go_prev.on_click = function () {
        this.dlg.area._move_page(-1);
    }

    plist.go_next = plist.add_tool('next');
    plist.go_next.on_click = function () {
        this.dlg.area._move_page(1);
    }

    plist.add = plist.add_tool('add');
    plist.add.on_click = function () {
        this.dlg.browse_files();
    }
    plist.browse_files = function() {
        this.hide();
        
        var fb = this.extension.open_local_file();
        fb.playlist = this;
        fb.on_browse = function (value, directory) {
            this.playlist.show();
            plist.add_files(value, false, false);
            if (directory) Sys.last_wdir = directory;

        }
        fb.on_close = function () {
            this.playlist.show();
        }


        fb.on_long_click = function (filename, path, directory) {
            var popup = gw_new_window_full(null, true, '');
            popup.fb = fb;
            this.disable();
            popup.area = gw_new_grid_container(popup);
            popup.area.dlg = popup;
            popup.area.break_at_line = true;

            popup.set_size(0.9 * this.width, 0.9 * this.height);
            popup.move(this.translation.x, this.translation.y);
            
            if (directory) {
                var item = gw_new_icon_button(popup.area, 'scan_directory', 'Add directory', true, 'listitem');
                item.set_size(popup.width, gwskin.default_control_height);
                item.dlg = popup;
                item.on_click = function() {
                    var filelist = Sys.enum_directory(path, '*', false);
                    item.dlg.fb.playlist.add_files(filelist, false, false);
                    item.dlg.fb.close();
                    item.dlg.close();
                }
                item = gw_new_icon_button(popup.area, 'scan_directory', 'Insert directory', true, 'listitem');
                item.set_size(popup.width, gwskin.default_control_height);
                item.dlg = popup;
                item.on_click = function() {
                    var filelist = Sys.enum_directory(path, '*', false);
                    item.dlg.fb.playlist.add_files(filelist, true, false);
                    item.dlg.fb.close();
                    item.dlg.close();
                }

            } else {
                var item = gw_new_icon_button(popup.area, 'add', 'Add file', true, 'listitem');
                item.set_size(popup.width, gwskin.default_control_height);
                item.dlg = popup;
                item.on_click = function() {
                    item.dlg.fb.playlist.add_files(path, false, false);
                    item.dlg.fb.close();
                    item.dlg.close();
                }
                item = gw_new_icon_button(popup.area, 'add', 'Insert file', true, 'listitem');
                item.set_size(popup.width, gwskin.default_control_height);
                item.dlg = popup;
                item.on_click = function() {
                    item.dlg.fb.playlist.add_files(path, true, false);
                    item.dlg.fb.close();
                    item.dlg.close();
                }
            }
            popup.area.layout();
            popup.set_alpha(1);
            
            popup.on_close = function () {
                this.fb.enable();
            }
            popup.show();
        }
    }


    plist.add_object = function(pl_item, is_selected) {
        var icon_name = is_selected ? 'play' : gw_guess_mime_icon(pl_item.name);
                
        var item = gw_new_icon_button(this.area, icon_name, pl_item.name, true, 'listitem');
        
        item.set_size(this.width, gwskin.default_control_height);
        item.dlg = this;
        item.pl_item = pl_item;        
        item.on_click = function() {
            this.dlg.extension.playlist_play(this.pl_item);
            this.dlg.hide_controler = true;
            this.dlg.close();
        }
        this.trash.enable();
        if (plist.extension.playlist.length>1)
            this.sort.enable();
    }
    
    plist.add_files = function(list, is_insert, recursive) {
        if (typeof list =='string') {
            var obj = {};
            var names = list.split('/');
            if (names.length == 0) names = list.split('\\');            
            obj.name = names.pop();
            var len = list.length - obj.name.length;
            obj.path = list.slice(0, len);
            
            if (is_insert) {
                this.extension.playlist.splice(this.extension.playlist_idx+1, 0, obj);
            } else {
                this.extension.playlist.push(obj);
                this.add_object(obj, false);
            }
        } else {
            //temp array for sorting, we have issues sorting Arrays creating by native code 
            var ar = [];
            for (var i=0; i<list.length; i++) {
                if (list[i].directory) continue;
                var obj = {};
                obj.path = list[i].path;
                obj.name = list[i].name;
                ar.push(obj);
            }
            if (list.length) Sys.last_wdir = list[0].path;

            //sort
            if (!this._sort_type) {
                ar.sort( function(a, b) { return a.name > b.name; } );
            } else {
                ar.sort( function(a, b) { return a.name < b.name; } );
            }
                        
            for (var i=0; i<ar.length; i++) {
                var obj = ar[i];
                if (is_insert) {
                    this.extension.playlist.splice(this.extension.playlist_idx+1+i, 0, obj);
                } else {
                    this.extension.playlist.push(obj);
                    this.add_object(obj, false);
                }
            }            
        }
        //in insert mode rebuild the UI (we cannot insert SFNode easily in an MFNode field)
        if (is_insert) {
            this.refresh_items();
        }
        this.area.layout();

        this.extension.set_option('Playlist', gwskin.stringify(this.extension.playlist));
    }
    
    plist._sort_type = 0;
    plist.sort = plist.add_tool('sort');
    plist.sort.on_click = function() {
        var wnd = this.dlg;
        wnd.area.reset_children();
        if (!wnd._sort_type) {
            wnd.extension.playlist.sort( function (a, b) { return a.name > b.name;} );
            wnd._sort_type = 1;
        } else {
            wnd.extension.playlist.sort( function (a, b) { return a.name < b.name;} );
            wnd._sort_type = 0;
        }
        wnd.extension.set_option('Playlist', gwskin.stringify(wnd.extension.playlist));
        wnd.refresh_items();
    }

    plist.area.on_page_changed = function () {
        if (this.is_first_page()) this.dlg.go_prev.disable();
        else this.dlg.go_prev.enable();

        if (this.is_last_page()) this.dlg.go_next.disable();
        else this.dlg.go_next.enable();
    }

    plist.trash = plist.add_tool('trash');
    plist.trash.on_click = function () {
        this.dlg.area.reset_children();
        this.dlg.extension.playlist = [];
        this.dlg.extension.set_option('Playlist', gwskin.stringify(this.dlg.extension.playlist));
        this.dlg.extension.set_option('PlaylistIndex', '0');
        this.dlg.extension.set_playlist_mode(true);

        this.disable();
    }
    plist.trash.disable();
    plist.sort.disable();

    plist.refresh_items = function() {
        this.area.reset_children();
        
        for (var i=0; i<this.extension.playlist.length; i++) {
            var is_selected = false;
            if (this.extension.playlist_mode && this.extension.current_url && (this.extension.playlist_idx==i)) {
                is_selected = true;
            }
            this.add_object(this.extension.playlist[i], is_selected);
        }
        this.area.layout();
    }

    //init playlist
    plist.refresh_items();

    plist.on_size = function (width, height) {
        var __children = this.area.get_children();
        for (var i = 0; i < __children.length; i++) {
            __children[i].set_size(width, gwskin.default_control_height);
        }
        this.area.set_size(width, height);
    }

    plist.on_display_size = function (width, height) {
        if (gwskin.mobile_device) {
            this.set_size(width, height);
        } else {
            var w = 2*width / 3;
            this.set_size(w, 0.8 * height);
        }
    }

   plist.on_display_size(gw_display_width, gw_display_height);
   plist.set_alpha(0.9);
   plist.show();
}

extension.playlist_mode = false;
extension.set_playlist_mode = function(value)
{
	
	if (!this.pl_event_filter) {
		this.pl_event_filter = this.create_playlist_event_filter(this);
		gwlib_add_event_filter(this.pl_event_filter);
	}
	
    if (!value || (this.playlist.length<=1)) {
        if (this.controler.playlist_next.visible) {
            this.controler.playlist_next.hide();
            this.controler.playlist_prev.hide();
        }
    } else {
        if (!this.controler.playlist_next.visible) {
            this.controler.playlist_next.show();
            this.controler.playlist_prev.show();
        }
        if (this.playlist_idx == 0) {
            this.controler.playlist_next.enable();
            this.controler.playlist_prev.disable();
        } else if (this.playlist_idx+1 == this.playlist.length) {
            this.controler.playlist_next.disable();
            this.controler.playlist_prev.enable();
        }
    }

    extension.playlist_mode = value;
	this.controler.layout();
}

extension.playlist_play = function (pl_item) {
    this.set_playlist_mode(true);
    this.set_movie_url(pl_item.path + pl_item.name);

    //save current index
    this.playlist_idx = this.playlist.indexOf(pl_item);
    this.set_option('PlaylistIndex', '' + this.playlist_idx);


	var e = {};
	e.type = GF_JS_EVENT_PLAYBACK;
	e.is_playing = true;
	e.index = this.playlist_idx;
	gwlib_filter_event(e);
}

extension.playlist_next = function()
{
    if (this.playlist_idx + 1 == this.playlist.length) return;
    this.playlist_idx++;
    this.playlist_play( this.playlist[this.playlist_idx] );
}

extension.playlist_prev = function()
{
    if (!this.playlist_idx) return;
    this.playlist_idx--;
    this.playlist_play( this.playlist[this.playlist_idx] );
}

extension.add_playlist_item = function(file)
{
	var obj = {};
	var names = file.split('/');
	if (names.length == 0) names = file.split('\\');
	obj.name = names.pop();
	var len = file.length - obj.name.length;
	obj.path = file.slice(0, len);
	this.playlist.push(obj);
	
	this.set_playlist_mode(true);
}
extension.playlist_load = function(items, do_play)
{
    this.reset_playlist();
    items.forEach(it => { this.add_playlist_item(it); });
    this.set_playlist_mode(true);
    if (do_play) this.playlist_next();
}

extension.reset_playlist = function()
{
	this.playlist = [];
	this.playlist_idx = -1;
}

    