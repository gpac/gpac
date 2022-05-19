
extension.reload_stats = function () {
    if (this.stats_wnd) {
        this.stats_wnd.reload_request = true;
        this.stats_wnd.close();
    }
}

extension.view_stats = function () {

    if (this.stats_wnd) {
        this.stats_wnd.close();
        this.stats_wnd = null;
        return;
    }

    var wnd = gw_new_window_full(null, true, 'Stats');
    gw_object_set_dragable(wnd);
    this.stats_wnd = wnd;
    wnd.extension = this;
    wnd.reload_request = false;

    wnd.area = gw_new_grid_container(wnd);
    wnd.area.break_at_hidden = true;
    wnd.area.spread_h = false;
    wnd.area.spread_v = true;

    var root_odm = this.root_odm;
    var nb_http = root_odm.nb_http;
    var nb_buffering = 0;
    var nb_ntp_diff = 0;
    var nb_qualities = 0;
    var srd_obj = null;

    wnd.has_select = false;

    for (var res_i = 0; res_i < wnd.extension.stats_resources.length; res_i++) {
        var m = wnd.extension.stats_resources[res_i];
		var num_qualities;
		var odm_srd = m.get_srd();
        m.gui = {};
		if (!srd_obj && odm_srd) srd_obj = m;
		if (m.dependent_group_id) {
			srd_obj = m;
			odm_srd = m.get_srd(m.dependent_group_id);
		}

        var label = '' + m.type;
		if (m.dependent_group_id) label += '(dep grp#' + m.dependent_group_id + ')';
        else if (m.scalable_enhancement) label += ' (Enh. Layer)';
        else if (m.width) label += ' (' + m.width + 'x' + m.height + ')';
        else if (m.samplerate) label += ' (' + m.samplerate + ' Hz ' + m.channels + ' channels)';
		
		if (odm_srd) {
		 label += ' (SRD ' + odm_srd.x + ',' + odm_srd.y + ',' + odm_srd.w + ',' + odm_srd.h + ')';
		}

        m.gui.txt = gw_new_text(wnd.area, label, 'lefttext');

		num_qualities = m.nb_qualities;
		if (m.dependent_group_id) {
			var q = m.get_quality(1, m.dependent_group_id);
			if (q) num_qualities=2;
		}

        if (num_qualities > 1) {
            nb_qualities++;
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
                    this.odm.select_quality('' + (idx - 1), this.odm.dependent_group_id);
                } else {
                    this.odm.select_quality('auto', this.odm.dependent_group_id);

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
                } else if (q.samplerate) {
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
				if (this.dependent_group_id) {
					var dqidx=0;
					while (1) {
						var q = this.get_quality(dqidx, this.dependent_group_id);
						if (!q) break;
						this.gui.qualities.push(q);
						dqidx++;
					}
				} else {
					for (i = 0; i < this.nb_qualities; i++) {
						var q = this.get_quality(i);
						if (q) {
							this.gui.qualities.push(q);
						}
					}
				}
                this.gui.qualities.sort(function (a, b) { return a.bandwidth - b.bandwidth });

                for (i = 0; i < this.gui.qualities.length; i++) {
                    if (this.gui.qualities[i].is_selected)
                        sel_idx = i + 1;
                }

                this.gui.select.min = 0;
                this.gui.select.max = this.gui.qualities.length;
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
                    label += '@' + Math.round(q.bandwidth / 1000) + 'Kbps';

                    this.gui.select_label.set_label(label);
                }
            }
            m.update_qualities();
        } else {
            m.gui.select = null;
            m.gui.select_label = null;
            m.update_qualities = function () { }
        }

		m.gui.info = null;
		if (!m.dependent_group_id) {
		
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
                if (this.odm) this.odm.gui.info_wnd = null;
            }

            iw.area = gw_new_text_area(iw, '');

            iw.on_display_size = function (width, height) {
                var w;
                if (gwskin.mobile_device) {
                    w = width;
                } else {
                    w = 30 * gwskin.default_text_font_size;
                }
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
                    label += 'Size:' + m.width + 'x' + m.height;
                    if (m.pixelformat) label += ' (' + m.par + ' ' + m.pixelformat + ')';
                } else if (m.samplerate) {
                    label += '' + m.samplerate + 'Hz ' + m.channels + ' channels';
                }
                label += '\n'
                label += 'Status: ' + m.status + ' - clock time: ' + m.clock_time; // + ' (drift ' + m.clock_drift + ')';

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
                if (bw < 8000000) label += '' + Math.round(bw / 1000) + ' Kbps';
                else label += '' + Math.round(bw / 1000 / 1000) + ' Mbps';

                label += ' - Maximum ';
                bw = m.max_bitrate;
                if (bw < 8000000) label += '' + Math.round(bw / 1000) + ' Kbps';
                else label += '' + Math.round(bw / 1000 / 1000) + ' Mbps';

                var bw = m.bandwidth_down;
                if (bw) {
                    label += '\n'
                    label += 'Download bandwidth: ';
                    if (bw < 8000) label += '' + bw + ' Kbps';
                    else label += '' + Math.round(bw / 1000) + ' Mbps';
                }
                var sender_diff = m.ntp_sender_diff;
                if (sender_diff != null) {
                    label += '\n'
                    label += 'NTP transmission diff: ' + sender_diff + ' ms';                    
                }

                label += '\n'
                label += 'Codec: ' + m.codec;
                if (odm.nb_views) {
                    label += '\n'
                    label += 'Nb Views: ' + odm.nb_views;
                }

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
            iw.timer.start(0);
            odm.gui.info_wnd.on_display_size(gw_display_width, gw_display_height);
            odm.gui.info_wnd.set_alpha(0.9);
            odm.gui.info_wnd.show();
        }
		}
		
        m.gui.buffer = null;
        m.gui.play = null;

        if (m.selected_service == m.service_id) {
            if (m.max_buffer && !m.dependent_group_id) {
                nb_buffering++;
                m.gui.buffer = gw_new_gauge(wnd.area, 'Buffer');
            }
            if (m.ntp_diff || this.show_stats_init) nb_ntp_diff++;
        } else if (!m.is_addon) {
            m.gui.play = gw_new_icon(wnd.area, 'play');
            m.gui.play.odm = m;
            m.gui.play.wnd = wnd;
            m.gui.play.on_click = function () {
                this.odm.select_service(this.odm.service_id);
                this.wnd.close_all();
            }
        }

        nb_http += m.nb_http;

        gw_new_separator(wnd.area);
    }

    wnd.http_control = null;
    if (nb_http || wnd.has_select) {
        wnd.http_text = gw_new_text(wnd.area, 'HTTP rate', 'lefttext');

        wnd.http_control = gw_new_slider(wnd.area);

        if (session.http_max_bitrate) {
            var br = Math.round(100 * session.http_max_bitrate / 1000 / 1000) / 100;
            if (br > 200) br = 200;
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

            wnd.http_text.set_label('HTTP cap ' + Math.round(session.http_max_bitrate / 10000)/100 + ' Mbps');
        } else {
            wnd.http_control.set_value(100);
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
            if (br == 200) {
                this.text.set_label('HTTP cap off');
                session.http_max_bitrate = Math.round(0);
            } else if (br == 0) {
                this.text.set_label('HTTP disable');
                session.http_max_bitrate = -1;
            } else {
                this.text.set_label('HTTP cap ' + Math.round(100 * br) / 100 + ' Mbps');
                session.http_max_bitrate = Math.round(br * 1000000);
            }
        }

        gw_new_separator(wnd.area);
    }

    wnd.srd_modes = null;
	if (srd_obj) {
        wnd.srd_text = gw_new_text(wnd.area, 'SRD Control', 'lefttext');
        wnd.srd_modes = gw_new_button(wnd.area, 'Modes');
		wnd.srd_modes.wnd = wnd;
		wnd.srd_modes.srd_obj = srd_obj;
		wnd.srd_modes.on_click = function() {
			this.wnd.srd_selected_mode = this.wnd.srd_current_mode;
			this.wnd.srd_set_mode(this.wnd.srd_current_mode);
			this.srd_obj.select_quality(this.wnd.srd_selected_mode);
		}
		wnd.srd_select = gw_new_spincontrol(wnd.area, true);
		wnd.srd_select.wnd = wnd;
		wnd.srd_select.on_click = function (val) {
			this.wnd.srd_set_mode(val);
		}
        wnd.srd_select.min = 0;
        wnd.srd_select.max = 8;
        wnd.srd_selected_mode = 0;
		if (srd_obj.gui.qualities != null)
			srd_obj.gui.qualities[0].tile_mode;
        wnd.srd_current_mode = 0;
		
		wnd.srd_set_mode = function(mode) {
		 var label = '';
		 this.srd_current_mode = mode;
		 if (this.srd_current_mode == this.srd_selected_mode) label='* ';

		 switch (mode) {
		 case 0: label+='Equal'; break;
		 case 1: label+='Top Rows'; break;
		 case 2: label+='Bottom Rows'; break;
		 case 3: label+='Middle Rows'; break;
		 case 4: label+='Left Column'; break;
		 case 5: label+='Right Column'; break;
		 case 6: label+='Middle Column'; break;
		 case 7: label+='Center'; break;
		 case 8: label+='Edges'; break;
		 }
		 this.srd_modes.set_label(label);
		}
		wnd.srd_set_mode(wnd.srd_selected_mode);
	}

    wnd.plot = gw_new_plotter(wnd.area);


    wnd.on_display_size = function (width, height) {
        var w, w_select;
        var h = 2 * gwskin.default_icon_height;
        var icon_h = gwskin.default_icon_height;

        if (gwskin.mobile_device) {
            w = width;
            icon_h *= 0.66;
			if (this.has_select)
				w_select = 4*icon_h;
        } else {
            w = 20 * gwskin.default_text_font_size;
            w += 4 * gwskin.default_icon_height;
			if (this.has_select) {
				w_select = 6*icon_h;
				w += w_select;
			}
        }

        for (var i = 0; i < this.extension.stats_resources.length; i++) {
            var res = this.extension.stats_resources[i];
            var aw = w;

            if (res.gui.info) {
				res.gui.info.set_size(1.5 * icon_h, icon_h);
				aw -= res.gui.info.width;
			}
            if (res.gui.buffer) {
                res.gui.buffer.set_size(2 * icon_h, 0.75 * icon_h);
            }
            if (res.gui.play) {
                res.gui.play.set_size(icon_h, icon_h);
            }
            aw -= 4 * icon_h;

            if (res.gui.select) {
                res.gui.select.set_size(icon_h, icon_h);
                aw -= icon_h;
                res.gui.select_label.set_size(w_select, 0.75 * icon_h);
                aw -= 4 * icon_h;
            }


            res.gui.txt.set_size(aw, icon_h);
            res.gui.txt.set_width(aw);
            h += icon_h;
        }

        if (this.plot) {
            this.plot.set_size(w, 8 * icon_h);
            h += 8 * icon_h;
        }

        if (this.http_control) {
            this.http_text.set_size(10 * gwskin.default_text_font_size, icon_h);
            var ch = 0.5 * icon_h;
            this.http_control.set_size(w - 10 * gwskin.default_text_font_size, ch, ch, ch);
            h += icon_h;
        }
		
		if (this.srd_modes) {
			var cw = 0.75 * icon_h;
			this.srd_text.set_size(12 * gwskin.default_text_font_size, icon_h);
			this.srd_modes.set_size(w - 12 * gwskin.default_text_font_size - icon_h, cw);
			this.srd_select.set_size(icon_h, cw);
		}

        this.set_size(w, h);
        this.move((w - width) / 2, (height - h) / 2);
    }
    wnd.on_close = function () {
        //this.timer.stop();
        //this.objs = [];
        if (this.reload_request) {
            var translation = this.extension.stats_wnd.translation;
            this.extension.stats_wnd = null;
            this.extension.view_stats();
            this.extension.stats_wnd.translation = translation;
        } else {
            this.extension.stats_wnd = null;
        }
    }

    var label = 'Statistics (' + Sys.nb_cores + ' cores';
    let mem = Sys.physical_memory;
    if (mem) {
		if (mem > 1000000000) label += ' - ' + Math.round(mem / 1000 / 1000 / 1000) + ' GB RAM';
		else label += ' - ' + Math.round(mem / 1000 / 1000) + ' MB RAM';
	}
	label += ')';

    wnd.set_label(label);

    if (wnd.plot) {
        wnd.s_fps = wnd.plot.add_serie('FPS', 'Hz', 0.8, 0, 0);
        if (nb_http) {
            wnd.s_bw = wnd.plot.add_serie('BW', 'Kbps', 0.8, 0, 0.8);
        } else {
            wnd.s_bw = null;
        }
        wnd.s_bitrate = wnd.plot.add_serie('Rate', 'Kbps', 0, 0.8, 0);
        if (nb_qualities)  
            wnd.s_quality = wnd.plot.add_serie('Quality', 'Kbps', 1, 0.65, 0);
        else
            wnd.s_quality = null;

        if (nb_buffering)
            wnd.s_buf = wnd.plot.add_serie('Buffer', 'ms', 0, 0, 0.8);
        else
            wnd.s_buf = null;

        if (nb_ntp_diff)
            wnd.s_ntp = wnd.plot.add_serie('E2E delay', 'ms', 0, 0.3, 0.8);
        else
            wnd.s_ntp = null;

        wnd.s_cpu = wnd.plot.add_serie('CPU', '%', 0, 0.5, 0.5);
        wnd.s_mem = wnd.plot.add_serie('MEM', 'MB', 0.5, 0.5, 0);

    }

    wnd.update_series = function() {
        var ext = this.extension;
        var length = (ext.stats_data.length > ext.stats_window ? stats_window : ext.stats_data.length);

        this.s_fps.refresh_serie(ext.stats_data, 'time', 'fps', length, 4);
        if (this.s_bw) {
            this.s_bw.refresh_serie(ext.stats_data, 'time', 'http_bandwidth', length, 1);
        }
        if (this.s_buf) {
            this.s_buf.refresh_serie(ext.stats_data, 'time', 'buffer', length, 1.5);
        }
        var stat_obj = ext.stats_data[ext.stats_data.length-1];
        if (stat_obj.bitrate) {
            this.s_bitrate.refresh_serie(ext.stats_data, 'time', 'bitrate', length, 2);
        }
        if (this.s_ntp) {
            this.s_ntp.refresh_serie(ext.stats_data, 'time', 'ntp_diff', length, 3);
        }
        this.s_cpu.refresh_serie(ext.stats_data, 'time', 'cpu', length, 10);
        this.s_mem.refresh_serie(ext.stats_data, 'time', 'memory', length, 6);
        if (this.s_quality) {
            this.s_quality.refresh_serie(ext.stats_data, 'time', 'quality', length, 2.5);
        }
    }

    wnd.quality_changed = function () {
        for (var i = 0; i < this.extension.stats_resources.length; i++) {
            var m = this.extension.stats_resources[i];
            m.update_qualities();
        }
    }
    wnd.close_all = function () {
        for (var i = 0; i < this.extension.stats_resources.length; i++) {
            if (this.extension.stats_resources[i].gui.info_wnd) {
                this.extension.stats_resources[i].gui.info_wnd.odm = null;
                this.extension.stats_resources[i].gui.info_wnd.close();
            }
        }
        this.extension.stats_resources.length = 0;
        this.close();
    }

    wnd.update_resource_gui = function(m, bl) {
        if (m.gui && m.gui.buffer) {
			var odm_srd = m.get_srd();

			if (m.dependent_group_id) {
				odm_srd = m.get_srd(m.dependent_group_id);
			}

            var label = ' ' + m.type;

			if (m.dependent_group_id) label += '(Dep. Group)';
			else if (m.scalable_enhancement) label += ' (Enh. Layer)';
            else if (m.width) label += ' (' + m.width + 'x' + m.height + ')';
            else if (m.samplerate) label += ' (' + Math.round(m.samplerate / 10) / 100 + ' kHz ' + m.channels + ' ch)';
            else if (m.scalable_enhancement) label += ' (Enh. Layer)';

			if (odm_srd) {
				label += ' (SRD ' + odm_srd.x + ',' + odm_srd.y + ',' + odm_srd.w + ',' + odm_srd.h + ')';
			}

            var url = m.service_url;
/*
            if ((url.indexOf('udp://') >= 0) || (url.indexOf('rtp://') >= 0) || (url.indexOf('dvb://') >= 0))
                label += ' Broadcast';
            else if ((url.indexOf('file://') >= 0) || (url.indexOf('://') < 0))
                label += ' File';
            else
                label += ' Broadband';
*/
            m.gui.txt.set_label(label);
            m.gui.buffer.set_value(bl);
            m.gui.buffer.set_label('' + Math.round(m.buffer / 10) / 100 + ' s');
        }
    }

    wnd.on_display_size(gw_display_width, gw_display_height);
    wnd.set_alpha(0.9);
    wnd.show();
};
    
