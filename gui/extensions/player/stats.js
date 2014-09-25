
extension.view_stats = function () {
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

    var root_odm = this.root_odm;
    var nb_http = root_odm.nb_http;
    var nb_buffering = 0;

    wnd.has_select = false;
    wnd.objs = [];
    //if not dynamic scene, add main OD to satts
    if (!this.dynamic_scene) {
        wnd.objs.push(root_odm);
    }


    for (var res_i = 0; res_i < root_odm.nb_resources; res_i++) {
        var m = root_odm.get_resource(res_i);
        if (!m) continue;
        wnd.objs.push(m);
    }

    for (var res_i = 0; res_i < wnd.objs.length; res_i++) {
        var m = wnd.objs[res_i];
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
                    label += 'Size:' + m.width + 'x' + m.height;
                    if (m.pixelformat) label += ' (' + m.par + ' ' + m.pixelformat + ')';
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

        if (m.selected_service == m.service_id) {
            if (m.max_buffer) {
                nb_buffering++;
                m.gui.buffer = gw_new_gauge(wnd.area, 'Buffer');
            }
        } else if (!m.is_addon) {
            m.gui.play = gw_new_icon(wnd.area, 'play');
            m.gui.play.odm = m;
            m.gui.play.wnd = wnd;
            m.gui.play.on_click = function () {
                this.odm.select_service(this.odm.service_id);
                this.wnd.close_all();
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

    var label = 'Statistics (' + gpac.nb_cores + ' cores - ';
    if (gpac.system_memory > 1000000000) label += '' + Math.round(gpac.system_memory / 1024 / 1024 / 1024) + ' GB RAM)';
    else label += '' + Math.round(gpac.system_memory / 1024 / 1024) + ' MB RAM)';

    wnd.set_label(label);

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

        wnd.s_cpu = wnd.plot.add_serie('CPU', '%', 0, 0.5, 0.5);
        wnd.s_mem = wnd.plot.add_serie('MEM', 'MB', 0.5, 0.5, 0);

    }

    wnd.timer.on_event = function (val) {
        var wnd = this.wnd;

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
            stat_obj.memory = Math.round(100 * gpac.memory / 1024 / 1024) / 100;
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
            wnd.s_fps.refresh_serie(wnd.stats, 'time', 'fps', wnd.stats_window, 4);
            if (wnd.s_bw) {
                wnd.s_bw.refresh_serie(this.wnd.stats, 'time', 'http_bandwidth', wnd.stats_window, 1);
            }
            if (wnd.s_buf) {
                wnd.s_buf.refresh_serie(this.wnd.stats, 'time', 'buffer', wnd.stats_window, 1.5);
            }
            if (stat_obj.bitrate) {
                wnd.s_bitrate.refresh_serie(this.wnd.stats, 'time', 'bitrate', wnd.stats_window, 2);
            } else {
                wnd.s_bitrate.hide();
            }
            wnd.s_cpu.refresh_serie(wnd.stats, 'time', 'cpu', wnd.stats_window, 10);
            wnd.s_mem.refresh_serie(wnd.stats, 'time', 'memory', wnd.stats_window, 6);
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
};
    