extension = {
    setup: false,
	dialog: null,
	sequence_index: 0,
	sequences: [],

	showroom_url: 'http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/showroom/',

	ext_filter_event: function (evt) {
		if (!this.dialog ) return;
		
		switch (evt.type) {
		default:
			return false;
		}
	},
	
	create_event_filter: function (__anobj) {
		return function (evt) {
			return __anobj.ext_filter_event(evt);
		}
	},

	load_description: function () {
		if (this.setup) return;

		var xhr = new XMLHttpRequest();
		xhr.open('GET', this.showroom_url + '00_showroom.json', false);
		xhr.send('');

		if ((xhr.status != 200) || (xhr.readyState != 4)) {
			if (xhr.status) {
				gwlog(l_err, '[ShowRoom] Failed to query server: ' + xhr.responseText);
			} else {
				gwlog(l_err, '[ShowRoom] Failed to send request');
			}
			var msg = gw_new_message(null, 'Error loading showroom', 'HTTP status '+xhr.status);
			msg.set_size(380, gwskin.default_icon_height + 2 * gwskin.default_text_font_size);
			msg.show();
			return null;
		}

		var obj = gwskin.parse(xhr.responseText);

		if (typeof obj.length == 'undefined') {
			gwlog(l_err, '[ShowRoom] Non conformant response object ' + xhr.responseText);
			return null;
		}
		this.sequences = obj;
		this.setup = true;
	},
	
	start: function () {
        gw_hide_dock();
		
		if (!this._evt_filter) {
			this._evt_filter = this.create_event_filter(this);
			gwlib_add_event_filter(this._evt_filter);
		}

		this.image = gw_new_image(null, '');
		this.image.extension = this;
		this.image.on_click = function() {
			this.extension.launch_sequence();
		}
		
		var wnd = gw_new_window(null, true, false);
		wnd.show_effect = 'none';

		wnd.area = gw_new_grid_container(wnd);
		wnd.area.spread_h = false;
        wnd.area.break_at_hidden = true;

		this.dialog = wnd;
		this.dialog.extension = this;
		
		wnd.btn_close = gw_new_icon(wnd.area, 'close');
		wnd.btn_close.on_click = function () {
			var ext = this.parent.parent.extension;
			ext.image.close();
			ext.desc.close();
			ext.dialog.close();
			if (arguments.length==0) gw_show_dock();
		};
		
		wnd.prev = gw_new_icon(wnd.area, 'previous');
		wnd.prev.on_click = function() {
			var ext = this.parent.parent.extension;
			if (ext.sequence_index)
				ext.sequence_index--;
			else
				ext.sequence_index = ext.sequences.length-1;
			ext.load_sequence();
		}

		wnd.next = gw_new_icon(wnd.area, 'next');
		wnd.next.on_click = function() {
			var ext = this.parent.parent.extension;
			if (ext.sequence_index<ext.sequences.length-1)
				ext.sequence_index++;
			else
				ext.sequence_index = 0;
			ext.load_sequence();
		}
		this.desc = gw_new_text_area(wnd.area, '', 'float_text');

        wnd.on_display_size = function (width, height) {
			h = height/2 - gwskin.default_control_height;
			this.extension.image.set_size(width, h);
			this.extension.image.move(0, gwskin.default_control_height + h/2 - height/2);

			this.extension.desc.set_size(width, h);
			this.set_size(width, gwskin.default_control_height + h);
			this.move(0, height/2 - this.height/2);
        }
		
		wnd.on_event = function (evt) {
			switch (evt.type) {
			case GF_EVENT_KEYDOWN:
				if (evt.keycode == 'Right') {
					this.next.on_click();
					return true;
				}
				if (evt.keycode == 'Left') {
					this.prev.on_click();
					return true;
				}
				if (evt.keycode == gwskin.keys.close) {
					this.btn_close.on_click();
					return true;
				}
				if (evt.keycode == gwskin.keys.validate) {
					this.extension.image.on_click();
					return true;
				}
				return 0;
			default:
				return 0;
			}
		}

        wnd.on_display_size(gw_display_width, gw_display_height);
		wnd.show();

		this.load_description();

		this.sequence_index = parseInt( this.get_option('SequenceIndex', '0') );
		if (this.sequence_index >= this.sequences.length) this.sequence_index = 0;
		else if (this.sequence_index < 0) this.sequence_index = 0;
		
		this.load_sequence();

    },
	
	load_sequence : function () {
		if (!this.setup) return;
		var o = this.sequences[this.sequence_index];
		var desc = o.name + '\n\n' + o.description;
		this.desc.set_content(desc);
		var res = o.icon.indexOf('://');
		if (res>=0) {
			this.image.set_image(o.icon);
		} else {
			this.image.set_image(this.showroom_url + o.icon);
		}
	},
	launch_sequence: function() {
		var o = this.sequences[this.sequence_index];
		this.set_option('SequenceIndex', '' + this.sequence_index);
		var res = o.path.indexOf('://');

		if (res >= 0) {
			gwskin.restore_session(o.path, 0, 0);
		} else {
			gwskin.restore_session(this.showroom_url + o.path, 0, 0);
		}
		this.dialog.btn_close.on_click(false);
	}

};
