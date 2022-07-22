/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, (c) 2010-2020 Telecom Paris
//
/////////////////////////////////////////////////////////////////////////////////

/*log function*/
function gwlog(lev, str) {
    alert(lev, str);
}

/*log levels*/
let l_err = 1;
let l_war = 2;
let l_inf = 3;
let l_deb = 4;

/*default log level*/
let gw_log_level = l_inf;

let GF_JS_EVENT_BASE = 20000;

//playback event
//@param is_playing boolean giving the playback status;
let GF_JS_EVENT_PLAYBACK = GF_JS_EVENT_BASE;

//playlist insert event
//@param url string giving URL to append
let GF_JS_EVENT_PLAYLIST_ADD = GF_JS_EVENT_BASE + 1;

//playlist reset event
//@param none
let GF_JS_EVENT_PLAYLIST_RESET = GF_JS_EVENT_BASE + 2;


//playlist reset event
//@param index item index in playlist
let GF_JS_EVENT_PLAYLIST_PLAY = GF_JS_EVENT_BASE + 3;

function gw_new_timer(progressive) {
    let obj = new SFNode('TimeSensor');

    obj.set_timeout = function (time, loop) {
        this.cycleInterval = time ? time : 1;
        this.loop = loop;
    }
    obj.startTime = -1;
    obj.start = function (when) {
        if (arguments.length==0) this.startTime = this.getTime();
        else this.startTime = when + this.getTime();
    }
    obj.stop = function (when) {
        if (arguments.length==0) this.stopTime = this.getTime();
        else this.stopTime = when + this.getTime();
    }
    obj.on_event = function (val) { };
    obj._event = function (val) {
        this.on_event(Math.floor(val / this.cycleInterval));
    }
    Browser.addRoute(obj, 'cycleTime', obj, obj._event);

    if (progressive) {
        obj.on_fraction = function (val) { }
        obj._fraction = function (frac) {
            this.on_fraction(frac);
        }
        Browser.addRoute(obj, 'fraction_changed', obj, obj._fraction);
    }
    obj.on_active = function (val) { }
    obj._active = function (val) {
        this.on_active(val);
    }
    Browser.addRoute(obj, 'isActive', obj, obj._active);
    return obj;
}

//static
function setup_gw_object(obj, name) {
    obj._name = name;
    obj.set_color = function () { }
    obj.set_size = function (width, height) {
        this.width = width;
        this.height = height;
    }
    obj.hide = function () {
        this.scale.x = this.scale.y = 0;
        this.visible = false;
        if (arguments.length == 1) {
            arguments[0].apply(this);
        }
    }
    obj.show = function () { this.scale.x = this.scale.y = 1; this.visible = true; }
    obj.visible = true;
    obj.set_size = function (width, height) {
        this.width = width;
        this.height = height;
    }
    obj.move = function (x, y) {
        this.translation.x = x;
        this.translation.y = y;
    }
    obj.width = obj.height = 0;

    obj.close = function () {
        if (typeof this._pre_destroy != 'undefined') this._pre_destroy();
        gw_detach_child(this);
    };
}

function gw_object_set_dragable(obj) {
    if (typeof (obj.on_drag) != 'undefined') return;

    var ps2d = new SFNode('PlaneSensor2D');
    obj._ps2d_idx = obj.children.length;
    obj.children[obj.children.length] = ps2d;
    ps2d.maxPosition.x = ps2d.maxPosition.y = -1;
    ps2d.autoOffset = FALSE;
    obj._drag = false;
    obj.begin_drag = null;
    obj.end_drag = null;
    obj._drag_active = function (val) {
        if (val) {
            this.startx = this.translation.x;
            this.starty = this.translation.y;
            if (this.begin_drag) this.begin_drag();
        } else {
            if (this.end_drag) this.end_drag();
        }
    }
    obj.on_drag = null;
    obj._on_drag = function (val) {
        if (this.on_drag) {
            this.on_drag(this.startx + val.x, this.starty + val.y);
        } else {
            this.translation.x = this.startx + val.x;
            this.translation.y = this.starty + val.y;
        }
        if (!this._drag && (val.x || val.y)) this._drag = true;
    }
    Browser.addRoute(ps2d, 'isActive', obj, obj._drag_active);
    Browser.addRoute(ps2d, 'translation_changed', obj, obj._on_drag);
}

//static
function gw_object_set_hitable(obj) {
    if (typeof (obj._active) != 'undefined') return;

    var ts = new SFNode('TouchSensor');
    obj._ts_idx = obj.children.length;
    obj.children[obj.children.length] = ts;
    obj._drag = false;
    obj._over = false;
    obj.on_over = null;
    obj._on_over = function (val) {
        this._over = val;
        gw_reset_hit(this, val);
        if (this.on_over) this.on_over(val);
    }
    obj.last_click = 0;
    obj._nb_click = 0;
    obj.on_long_click = null;
    obj.on_click = null;
    obj.on_down = null;
    obj._on_active = function (val, timestamp) {
        if (!this._over) return;

        if (this.on_down) this.on_down(val);

        if (!val) {
            if (this._drag) {
                this._drag = false;
                return;
            }
            if (this.on_long_click && (timestamp - this.last_click > gwskin.long_click_delay)) this.on_long_click();
            else if (this.on_click && this._over) this.on_click();
        } else {
            this.last_click = timestamp;
        }
    }
    Browser.addRoute(ts, 'isOver', obj, obj._on_over);
    Browser.addRoute(ts, 'isActive', obj, obj._on_active);

    obj._disable_touch = function () {
        this.children[this._ts_idx].enabled = false;
    }
    obj._enable_touch = function () {
        this.children[this._ts_idx].enabled = true;
    }
    obj.new_over_handler = function(callback) {
        Browser.addRoute(this.children[this._ts_idx], 'isOver', this, callback);
    }
}

//static
function gw_new_appearance(r, g, b) {
    var appearance = new SFNode('Appearance');
    appearance.material = new SFNode('Material2D');
    appearance.material.filled = TRUE;
    appearance.material.emissiveColor = new SFColor(r, g, b);
    return appearance;
}
//static
function gw_new_lineprops(red, green, blue) {
    var lineProps = new SFNode('LineProperties');
    lineProps.lineColor.r = red;
    lineProps.lineColor.g = green;
    lineProps.lineColor.b = blue;
    return lineProps;
}

//static
function gw_make_gradient(type, keys, colors, opacities) {
    var i;
    var obj = new SFNode(type == 'radial' ? 'RadialGradient' : 'LinearGradient');
    for (i = 0; i < keys.length; i++) {
        obj.key[i] = keys[i];
    }
    for (i = 0; i < colors.length / 3; i++) {
        obj.keyValue[i] = new SFColor(colors[3 * i], colors[3 * i + 1], colors[3 * i + 2]);
        if (arguments.length == 4) {
            obj.opacity[i] = opacities[i];
        }
    }
    if (type == 'vertical') {
        obj.endPoint.x = 0;
        obj.endPoint.y = 1;
    }
    return obj;
}

//static
function gw_new_fontstyle(size, align) {
    var fs = new SFNode('FontStyle');
    fs.size = size;
    fs.family[0] = gwskin.default_font_family;
    switch (align) {
        case 0:
            fs.justify[0] = 'BEGIN';
            break;
        case 1:
            fs.justify[0] = 'MIDDLE';
            break;
        case 2:
            fs.justify[0] = 'END';
            break;
        default:
            fs.justify[0] = 'MIDDLE';
            break;
    }
    fs.justify[1] = 'MIDDLE';
    return fs;
}

let gw_resources_bank = [];

//static
function gw_load_resource(url, is_icon) {
    var new_res;

    if (gwskin.use_resource_bank) {
        for (var i in gw_resources_bank) {
            if (gw_resources_bank[i].res_url == url) {
                gw_resources_bank[i].nb_instances++;
                return gw_resources_bank[i];
            }
        }
    }

    if (is_icon) {
        new_res = new SFNode('StyleGroup');
        new_res.children[0] = new SFNode('Inline');
        new_res.children[0].url[0] = url;
        new_res.set_style = function (app) {
            this.appearance = app;
        };
        new_res.set_style(gwskin.styles[0].normal);
    } else {
//        new_res = new SFNode('OffscreenGroup');
//        new_res.offscreen_mode = 2;
//        new_res.children[0] = new SFNode('Inline');
//        new_res.children[0].url[0] = url;
           new_res = new SFNode('Inline');
           new_res.url[0] = url;

        new_res.set_style = function () { };
    }

    new_res.nb_instances = 1;
    if (gwskin.use_resource_bank) {
        new_res.res_url = url;
        gw_resources_bank.push(new_res);
    }
    return new_res;
}

//static
function gw_unload_resource(res) {
    if (!gwskin.use_resource_bank) return;

    if (!res.nb_instances) return;
    res.nb_instances--;
    if (res.nb_instances) return;

    for (var i in gw_resources_bank) {
        if (gw_resources_bank[i] == res) {
            gw_resources_bank.splice(i, 1);
            return;
        }
    }
    gwlog(l_err, 'Unloading resource for url ' + res.children[0].url[0] + ' not found in resource bank');
}

//static
function gw_appy_effect_scale(timer, val) {
    if (!timer.wnd.visible) {
        timer.wnd.scale.x = 1 - val;
        timer.wnd.scale.y = 1 - val;
          timer.wnd.set_alpha((1 - val) * timer.target_alpha);
    } else {
        timer.wnd.scale.x = val;
        timer.wnd.scale.y = val;
          timer.wnd.set_alpha(val * timer.target_alpha);
    }
}

//static
function gw_appy_effect_notif(timer, val) {
    var final_x = gw_display_width / 2 - timer.wnd.width / 2;
    var final_y = gw_display_height / 2 - timer.wnd.height/2;
    if (!timer.wnd.visible) {
        timer.wnd.translation.x = final_x;
        timer.wnd.translation.y = final_y + val * timer.wnd.height;
        timer.wnd.set_alpha((1 - val) * timer.target_alpha);
    } else {
        timer.wnd.translation.x = final_x;
        timer.wnd.translation.y = final_y + (1-val) * timer.wnd.height;
        timer.wnd.set_alpha(val * timer.target_alpha);
    }
}

//static
function gw_window_show_hide() {

	if (typeof this.show_effect == 'string') {
		if (this.show_effect == 'none') {
			this.visible = !this.visible;
			this.scale.x = this.visible ? 1 : 0;
			this.scale.y = this.visible ? 1 : 0;
			return;
		}
	}

    if (typeof this._wnd_timer == 'undefined') {
        this._wnd_timer = gw_new_timer(1);
        this._wnd_timer.wnd = null;

        this._wnd_timer.effect = 0;
        this._wnd_timer.set_timeout(0.25, false);
        this._wnd_timer.on_fraction = function (val) {
            if (!this.wnd) return;

            switch (this.effect) {
            case 0:
                gw_appy_effect_scale(this, val);
                break;
            case 1:
                gw_appy_effect_notif(this, val);
                break;
            }
        }
        this._wnd_timer.on_active = function (val) {
            var fun;
            if (val || !this.wnd) return;
            var wnd = this.wnd;
            this.wnd = null;
            wnd.scale.x = wnd.visible ? 1 : 0;
            wnd.scale.y = wnd.visible ? 1 : 0;
            wnd.set_alpha(this.target_alpha);
            if (wnd.visible) {
                gw_ui_root.set_focus(wnd);
            } else {
                gw_ui_root.remove_focus(wnd);
            }
            fun = this.call_on_end;
            this.call_on_end = null;
            if (fun) {
                fun.apply(wnd);
            }
        }
    }
    /*not done yet! This can happen when the function is called faster than the animation duration*/
    if (this._wnd_timer.wnd) return;

    this.visible = !this.visible;
    if (this.visible) {
        this._wnd_timer.target_alpha = this.get_alpha();
        this._wnd_timer.target_x = this.translation.x;
        this._wnd_timer.target_y = this.translation.y;
        this._wnd_timer.target_w = this.width;
        this._wnd_timer.target_h = this.height;
    }

    if (typeof this.show_effect == 'string') {
        if (this.show_effect == 'notif') this._wnd_timer.effect = 1;
    }
    this._wnd_timer.wnd = this;
    this._wnd_timer.start(0);
    this._wnd_timer.call_on_end = null;
    if (arguments.length) {
        this._wnd_timer.call_on_end = arguments[0];
    }
}

//static
function gw_window_show() {
    if (this.visible) return;
    gw_window_show_hide.call(this);
}

//static
function gw_window_hide() {
    if (!this.visible) return;
    gw_window_show_hide.apply(this, arguments);
}

//Skin definition
var gwskin = new Object();
gwskin.back_color = new SFColor(0.2, 0.2, 0.2);
gwskin.pointing_device = true;
gwskin.long_click_delay = 0.5;
gwskin.use_resource_bank = false;
gwskin.default_window_alpha = 0.5;
gwskin.default_message_timeout = 3.0;
gwskin.default_tooltip_timeout = 0.75;
gwskin.default_tooltip_delay = 1;

gwskin.appearance_transparent = gw_new_appearance(0, 0, 0);
gwskin.appearance_transparent.material.transparency = 1;
gwskin.appearance_transparent.skin = true;

gwskin.no_gl_window_back = new SFNode('Background2D')
gwskin.no_gl_window_back.backColor = new SFColor(0, 0, 0);

gwskin.last_hit_x = 0;
gwskin.last_hit_y = 0;

gwskin.enable_background = function(do_enable) {
 if (do_enable) {
  scene.set_back_color(gwskin.back_color.r, gwskin.back_color.g, gwskin.back_color.b, 1.0);
 } else {
  scene.set_back_color(0, 0, 0, 1.0);
 }
}

gwskin.enable_focus = function(do_enable) {
    this.focus_on = do_enable;
}

//static
function gw_get_abs_pos(child) {
    var pos = new SFVec2f(0, 0);
    while (child != null) {
        if (typeof (child.translation) != 'undefined') {
            pos.x += child.translation.x;
            pos.y += child.translation.y;
        }
        if (typeof (child.parent) == 'undefined') break;
        if (typeof (child._is_window) == 'boolean') break;
        child = child.parent;
    }
    return pos;
}

function gw_get_adjusted_abs_pos(child, width, height, type)
{
    var pos = gw_get_abs_pos(child);
    
    if (type == 0) {
        pos.y += child.height/2 + height/2; 
    } else if (type==1) {
        pos.y -= child.height/2 + height/2; 
    }
    
    
    if (pos.x - width / 2 < - gw_display_width / 2)
        pos.x = - gw_display_width / 2 + width / 2;
    else if (pos.x + width / 2 > gw_display_width / 2)
        pos.x = gw_display_width / 2 - width / 2;
    
    if (pos.y + height/2 > gw_display_height / 2) {
        if (type == 0) {
            pos.y -= child.height + height;
        } else {
            pos.y = gw_display_height / 2 - height/2;
        }
    }
    else if (pos.y - height/2 < -gw_display_height / 2) {
        if (type==1) {
            pos.y += child.height + height;
        } else {
            pos.y = height/2 - gw_display_height / 2;
        }
    }

    return pos;
}


gwskin.tooltip_wnd = null;
gwskin.tooltip_timeout = gw_new_timer(false);

gwskin.tooltip_exec = function (obj, show) {

    if (!show) return;

    if (!gwskin.tooltip_wnd) {
        wnd = gw_new_window(null, true, true, 'tooltip', true);
        wnd.label = '';
        gwskin.tooltip_wnd = wnd;
        wnd.txt = gw_new_text(gwskin.tooltip_wnd, '');
        wnd.on_display_size = function (w, h) {
            if (! this.label) return;

            width = this.label.length * gwskin.default_text_font_size;
            this.set_size(width, 2 * gwskin.default_text_font_size);
            this.txt.set_width(width);
            this.move(-w / 2 + width / 2, h / 2 - gwskin.default_text_font_size);
        }
        wnd.on_close = function () {
            this.timer = null;
            gwskin.tooltip_wnd = null;
        }
        wnd.on_display_size(gw_display_width, gw_display_height);
        wnd.set_alpha(0.9);
        wnd.show();

        wnd.timer = gw_new_timer(false);
        wnd.timer.set_timeout(gwskin.default_tooltip_timeout, false);
        wnd.timer.start(0);
        wnd.timer.on_active = function (val) {
            if (!val) gwskin.tooltip_wnd.close();
        }
    }

    gwskin.tooltip_wnd.label = obj.get_label();
    gwskin.tooltip_wnd.txt.set_label(gwskin.tooltip_wnd.label);
    gwskin.tooltip_wnd.on_display_size(gw_display_width, gw_display_height);


    var tt = gwskin.tooltip_wnd;
    var dy = 1.2 * tt.height;
    var pos = gw_get_adjusted_abs_pos(obj, tt.width, 1.2 * tt.height, 0);
    tt.move(pos.x, pos.y);
}

gwskin.tooltip_callback = function (obj, show) {

    if (! obj._tooltip) return;
    if (!show) {
        gwskin.tooltip_timeout.obj = null;
        gwskin.tooltip_timeout.stop(0);
        return;
    }
    gwskin.tooltip_timeout.set_timeout(gwskin.default_tooltip_delay, false);
    gwskin.tooltip_timeout.obj = obj;
    gwskin.tooltip_timeout.on_active = function (val) {
        if (!val && gwskin.tooltip_timeout.obj) {
            //deactivate tooltip after first run
            obj._tooltip = false;
            gwskin.tooltip_exec(gwskin.tooltip_timeout.obj, true);
        }
    }
    gwskin.tooltip_timeout.start(0);
}
gwskin.default_label_font_size = 18;
gwskin.default_text_font_size = 18;
gwskin.default_font_family = 'SANS';
gwskin.default_icon_text_spacing = 8;
gwskin.default_control_height = 48;
gwskin.default_icon_height = 32;

//create styles
gwskin.styles = [];
let s = { name: 'default' };
gwskin.styles.push(s);

s.normal = gw_new_appearance(0.4, 0.6, 1);
s.normal.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.4, 0.6]);
s.normal.skin = true;
if (gwskin.pointing_device) {
    s.over = gw_new_appearance(0.0, 1.0, 1);
    s.over.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.8, 1]);
    s.over.skin = true;
} else {
    s.over = null;
}
s.down = gw_new_appearance(0, 0, 1);
s.down.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 1, 1]);
s.down.skin = true;
s.disable = gw_new_appearance(0.4, 0.4, 0.4);
s.disable.skin = true;
s.text = gw_new_appearance(1, 1, 1);
s.text.skin = true;
s.font = gw_new_fontstyle(gwskin.default_label_font_size, 1);
s.font.skin = true;

//overrite style for button to have its own text color
s = { name: 'button' };
gwskin.styles.push(s);
s.text = gw_new_appearance(1, 1, 1);
s.text.skin = true;

//override style for checkbox to have text alignment to 'begin'
s = { name: 'checkbox' };
gwskin.styles.push(s);
s.font = gw_new_fontstyle(gwskin.default_label_font_size, 0);
s.font.skin = true;
s.over = gw_new_appearance(0.0, 1.0, 1);
s.over.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.8, 1]);
s.over.material.lineProps = gw_new_lineprops(0.0, 0.8, 1);
s.over.skin = true;

//override style for icon to have no text not fonts
s = { name: 'icon' };
gwskin.styles.push(s);
s.text = null;
s.font = null;

s = { name: 'root_icon' };
gwskin.styles.push(s);
s.text = gw_new_appearance(1, 1, 1);
s.text.skin = true;


//override listitems to have text alignment to 'begin'
s = { name: 'listitem' };
gwskin.styles.push(s);
s.font = gw_new_fontstyle(gwskin.default_label_font_size, 0);
s.font.skin = true;

s = { name: 'edit' };
gwskin.styles.push(s);
s.normal = gw_new_appearance(1, 1, 1);
s.normal.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.8, 1]);
s.over = gw_new_appearance(1, 1, 1);
s.over.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.8, 1]);
s.over.material.lineProps = gw_new_lineprops(0.0, 0.8, 1);
s.text = gw_new_appearance(0, 0, 0);
s.text.skin = true;
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.style += ' SIMPLE_EDIT';
s.font.skin = true;

s = { name: 'editpw' };
gwskin.styles.push(s);
s.normal = gw_new_appearance(1, 1, 1);
s.normal.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.8, 1]);
s.over = gw_new_appearance(1, 1, 1);
s.over.texture = gw_make_gradient('vertical', [0, 1], [0.0, 0.2, 0.4, 0.0, 0.8, 1]);
s.over.material.lineProps = gw_new_lineprops(0.0, 0.8, 1);
s.text = gw_new_appearance(0, 0, 0);
s.text.skin = true;
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.style += ' SIMPLE_EDIT PASSWD';
s.font.skin = true;

s = { name: 'window' };
gwskin.styles.push(s);
s.normal = gw_new_appearance(0.6, 0.6, 0.6);
s.normal.texture = gw_make_gradient('vertical', [0, 0.1, 0.9, 1], [0.6, 0.6, 0.6, 0, 0, 0, 0, 0, 0, 0.6, 0.6, 0.6]);
s.normal.skin = true;
//override font to have middle alignment
s.font = gw_new_fontstyle(gwskin.default_label_font_size, 1);
s.font.skin = true;
s.hide = gw_window_hide;
s.show = gw_window_show;

let tt_s = { name: 'tooltip' };
gwskin.styles.push(tt_s);
tt_s.normal = gw_new_appearance(0.6, 0.6, 0.6);
tt_s.normal.texture = s.normal.texture;
//tt_s.normal.material.lineProps = gw_new_lineprops(0, 0, 0);
//tt_s.normal.material.lineProps.width = 0.5;
tt_s.font = s.font;
//tt_s.hide = gw_window_hide;
//tt_s.show = gw_window_show;

s = { name: 'progress' };
gwskin.styles.push(s);
s.normal = gw_new_appearance(0.6, 0.6, 0.6);
s.normal.texture = gw_make_gradient('vertical', [0, 0.5, 1], [0.0, 0.4, 0.6, 0, 0, 0, 0.0, 0.4, 0.6]);
s.normal.skin = true;
s.over = gw_new_appearance(0.7, 0.7, 0.8);
s.over.texture = gw_make_gradient('vertical', [0, 0.5, 1], [0.0, 1, 1, 0, 0.2, 0.4, 0.0, 1, 1]);
s.over.skin = true;
s.down = gw_new_appearance(0.7, 0.7, 0.8);
s.down.texture = gw_make_gradient('vertical', [0, 0.5, 1], [0.8, 1, 1, 0, 0.4, 0.6, 0.8, 1, 1]);
s.down.skin = true;
//for gauge
s.text = gw_new_appearance(1, 1, 1);
s.text.skin = true;
s.font = gw_new_fontstyle(gwskin.default_label_font_size, 0);
s.font.skin = true;


s = { name: 'plot' };
gwskin.styles.push(s);
s.normal = gw_new_appearance(0.9, 0.9, 0.9);
s.normal.skin = true;
s.text = gw_new_appearance(0, 0, 0);
s.text.skin = true;
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.skin = true;


s = { name: 'text' };
gwskin.styles.push(s);
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.justify[1] = 'END';
s.font.skin = true;

s = { name: 'lefttext' };
gwskin.styles.push(s);
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.skin = true;

s = { name: 'righttext' };
gwskin.styles.push(s);
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.skin = true;

s = { name: 'float_text' };
gwskin.styles.push(s);
s.text = gw_new_appearance(1, 1, 1);
s.text.skin = true;
s.font = gw_new_fontstyle(gwskin.default_text_font_size, 0);
s.font.justify[1] = 'END';
s.font.skin = true;

gwskin.images = new Object();
gwskin.labels = new Object();

gwskin.images.trash = 'icons/trash.svg';
gwskin.labels.trash = 'Delete';
gwskin.images.add = 'icons/add.svg';
gwskin.labels.add = 'Add';
gwskin.images.remove = 'icons/remove.svg';
gwskin.labels.remove = 'Remove';
gwskin.images.remote_display = 'icons/monitor.svg';
gwskin.labels.remote_display = 'Push to remote device';
gwskin.images.information = 'icons/info.svg';
gwskin.labels.information = 'Information';
gwskin.images.resize = 'icons/resize.svg';
gwskin.labels.resize = 'Resize';
gwskin.images.cancel = 'icons/cross.svg';
gwskin.labels.cancel = 'Cancel';
gwskin.images.close = 'icons/cross.svg';
gwskin.labels.close = 'Close';
gwskin.images.previous = 'icons/previous.svg';
gwskin.labels.previous = 'Previous';
gwskin.images.next = 'icons/next.svg';
gwskin.labels.next = 'Next';
gwskin.images.left = 'icons/left.svg';
gwskin.labels.left = 'Left';
gwskin.images.right = 'icons/right.svg';
gwskin.labels.right = 'Right';
gwskin.images.up = 'icons/up.svg';
gwskin.labels.up = 'Up';
gwskin.images.down = 'icons/down.svg';
gwskin.labels.down = 'Down';
gwskin.images.scan_directory = 'icons/overflowing.svg';
gwskin.labels.scan_directory = 'Select directory';
gwskin.images.history = 'icons/overflowing.svg';
gwskin.labels.history = 'History';
gwskin.images.media_next = 'icons/media_next.svg';
gwskin.labels.media_next = 'Next Item';
gwskin.images.media_prev = 'icons/media_prev.svg';
gwskin.labels.media_prev = 'Previous Clip';
gwskin.images.seek_forward = 'icons/seek_forward.svg';
gwskin.labels.seek_forward = 'Fast Forward';
gwskin.images.rewind = 'icons/rewind.svg';
gwskin.labels.rewind = 'Rewind';
gwskin.images.folder = 'icons/folder.svg';
gwskin.labels.folder = 'Directory';
gwskin.images.check = 'icons/check.svg';
gwskin.labels.check = 'Check';
gwskin.images.drive = 'icons/harddrive.svg';
gwskin.labels.drive = 'Storage Drive';
gwskin.images.device = 'icons/laptop.svg';
gwskin.labels.device = 'Device';
gwskin.images.home = 'icons/home.svg';
gwskin.labels.home = 'Home';
gwskin.images.gpac = 'icons/gpac.svg';
gwskin.labels.gpac = 'Home';
gwskin.images.media = 'icons/more.svg';
gwskin.labels.media = 'Media';
gwskin.images.favorite = 'icons/heart.svg';
gwskin.labels.favorite = 'Favorites';
gwskin.images.audio = 'icons/audio.svg';
gwskin.labels.audio = 'Audio On';
gwskin.images.audio_mute = 'icons/audio_mute.svg';
gwskin.labels.audio_mute = 'Audio Mute';
gwskin.images.play = 'icons/play.svg';
gwskin.labels.play = 'Play';
gwskin.images.pause = 'icons/pause.svg';
gwskin.labels.pause = 'Pause';
gwskin.images.stop = 'icons/stop2.svg';
gwskin.labels.stop = 'Stop';
gwskin.images.fullscreen = 'icons/expand.svg';
gwskin.labels.fullscreen = 'Fullscreen';
gwskin.images.fullscreen_back = 'icons/shrink.svg';
gwskin.labels.fullscreen_back = 'Exit Fullscreen';
gwskin.images.play_once = 'icons/play_single.svg';
gwskin.labels.play_once = 'Play once';
gwskin.images.play_loop = 'icons/play_loop.svg';
gwskin.labels.play_loop = 'Loop';
gwskin.images.play_shuffle = 'icons/play_shuffle.svg';
gwskin.labels.play_shuffle = 'Shuffle';
gwskin.images.navigation = 'icons/navigation.svg';
gwskin.labels.navigation = 'Navigation';
gwskin.images.network = 'icons/network.svg';
gwskin.labels.network = 'Network';
gwskin.images.file_open = 'icons/tray.svg';
gwskin.labels.file_open = 'Open';
gwskin.images.exit = 'icons/power.svg';
gwskin.labels.exit = 'Exit';
gwskin.images.remote_location = 'icons/world.svg';
gwskin.labels.remote_location = 'Enter URL';
gwskin.images.statistics = 'icons/speed.svg';
gwskin.labels.statistics = 'Statistics';
gwskin.images.live = 'icons/live.svg';
gwskin.labels.live = 'Back to live';
gwskin.images.sort = 'icons/sort.svg';
gwskin.labels.sort = 'Sort';
gwskin.images.playlist = 'icons/list.svg';
gwskin.labels.playlist = 'Playlist';
gwskin.images.playlist_next = 'icons/pl_next.svg';
gwskin.labels.playlist_next = 'Next';
gwskin.images.playlist_prev = 'icons/pl_prev.svg';
gwskin.labels.playlist_prev = 'Previous';
gwskin.images.channels = 'icons/tv.svg';
gwskin.labels.channels = 'TV Channels';
gwskin.images.view360 = 'icons/image.svg';
gwskin.labels.view360 = 'VR';
gwskin.images.sensors = 'icons/compass.svg';
gwskin.labels.sensors = 'Orientation';
gwskin.images.chapters = 'icons/chapter.svg';
gwskin.labels.chapters = 'Chapters';


gwskin.mime_video_default_ext = " mp4 mp4s m4s 3gp 3gpp m2ts ts trp m3u8 mpd avi mov ";
gwskin.mime_audio_default_ext = " m4a mp3 aac wav ";
gwskin.mime_image_default_ext = " jpg jpeg png ";
gwskin.mime_model_default_ext = " bt xmt svg ";

gwskin.images.mime_video = 'icons/film.svg';
gwskin.images.mime_audio = 'icons/musical.svg';
gwskin.images.mime_model = 'icons/app.svg';
gwskin.images.mime_generic = 'icons/file.svg';
gwskin.images.mime_image = 'icons/image.svg';



gwskin.keys = new Object();
gwskin.keys.close = 'U+0008';
gwskin.keys.validate = 'Enter';

gwskin.last_hitable_obj = null;

//static
function gwskin_set_white_blue()
{
	var s = gwskin.get_class('default');

 	//s.normal.texture.keyValue = [0.0, 0.2, 0.4, 0.0, 0.4, 0.6];
	//if (s.over && s.over.texture) s.over.texture.keyValue = [0.0, 0.2, 0.4, 0.0, 0.8, 1];
	//s.down.texture.keyValue = [0.0, 0.2, 0.4, 0.0, 1, 1];

	s.text.material.emissiveColor = new SFColor(0, 0, 0);

	//override style for checkbox to have text alignment to 'begin'
	s = gwskin.get_class('checkbox');
//	s.over.texture.keyValue = [0.0, 0.2, 0.4, 0.0, 0.8, 1];

	s = gwskin.get_class('edit');
//	s.normal.texture.keyValue = [0.0, 0.2, 0.4, 0.0, 0.8, 1];
//	s.over.texture.keyValue = [0.0, 0.2, 0.4, 0.0, 0.8, 1];
	s.over.material.lineProps.lineColor = new SFColor(0.0, 0.8, 1);
	s.text.material.emissiveColor = new SFColor(0, 0, 0);

	s = gwskin.get_class('window');
	s.normal.material.emissiveColor = new SFColor(1, 1, 1);
	gwskin.no_gl_window_back.backColor = new SFColor(1, 1, 1);
//	s.normal.texture.keyValue = [0.6, 0.6, 0.6, 0, 0, 0, 0, 0, 0, 0.6, 0.6, 0.6];
	if (s.normal.texture) {
		s.normal.texture.keyValue[1] = new SFColor(1, 1, 1);
		s.normal.texture.keyValue[2] = new SFColor(1, 1, 1);
	}

	s = gwskin.get_class('progress');
//	s.normal.texture.keyValue = [0.0, 0.4, 0.6, 0, 0, 0, 0.0, 0.4, 0.6];
//	s.normal.texture.keyValue[1] = new SFColor(1, 1, 1);
//	s.over.texture.keyValue = [0.0, 1, 1, 0, 0.2, 0.4, 0.0, 1, 1];
//	s.over.texture.keyValue[1] = new SFColor(0.4, 0.8, 1);
//	s.down.texture.keyValue = [0.0, 0.4, 0.6, 0, 0.2, 0.4, 0.0, 0.4, 0.6];
//	s.down.texture.keyValue[1] = new SFColor(0.4, 0.8, 1);
	s.text.material.emissiveColor = new SFColor(1, 1, 1);

	s = gwskin.get_class('root_icon');
	s.text.material.emissiveColor = new SFColor(1, 1, 1);

}


function gwskin_set_default_control_height(value) {
    gwskin.default_control_height = value;
}

function gwskin_set_default_icon_height(value) {
    gwskin.default_icon_height = value;
    
    var fsize = 1;
    if (value>50) fsize = 32;
    else if (value>30) fsize = 22;
    else if (value > 10) fsize = value - 10;
             
    gwskin.default_label_font_size = fsize;
    gwskin.default_text_font_size = fsize;
    
    for (var i = 0; i < gwskin.styles.length; i++) {
        if ((typeof gwskin.styles[i].font != 'undefined') && gwskin.styles[i].font) {
            gwskin.styles[i].font.size = fsize;
        }
    }
}


//static
function gwlib_filter_event(evt) {

	if (evt.type == GF_EVENT_MOUSEDOWN) {
		if (gw_ui_root.has_popup) {
			//close all open popups
			var count = gw_ui_root.children.length;
			for (var i = count; i > 0; i--) {
				var c = gw_ui_root.children[i - 1];
				if (typeof c._popup != 'undefined') {
					c.close();
					if (count>gw_ui_root.children.length) i--;
				}
			}
			gw_ui_root.has_popup = false;
		}
		gwskin.last_hit_x = evt.mouse_x;
		gwskin.last_hit_y = evt.mouse_y;
	}

    if (gw_ui_top_wnd && gw_ui_top_wnd.on_event(evt)) return true;

/*
 if ((evt.type == GF_EVENT_KEYDOWN) && ((evt.keycode == 'Up') || (evt.keycode == 'Down') || (evt.keycode == 'Right') || (evt.keycode == 'Left')))
        return false;
*/
	
    for (var i = 0; i < gw_event_filters.length; i++) {
        if (gw_event_filters[i](evt) == true) return true;
    }
    return false;
}

function gwlib_init(root_node) {
    gw_ui_root = gw_new_container();
    gw_ui_root._name = 'Root Display';
    gw_ui_root.has_popup = false;
    gw_add_child(root_node, gw_ui_root);
    gw_ui_root.set_focus = function(wnd) {
        gw_ui_top_wnd = null;
        if (typeof wnd.on_event != 'undefined') {
            gw_ui_top_wnd = wnd;
        }
        if (typeof (wnd._no_focus) == 'boolean') return;
        if (gwskin.focus_on) {
			if (typeof (wnd._init_focus) != 'undefined')
				scene.set_focus(wnd._init_focus);
			else
				scene.set_focus(gw_ui_top_wnd);
		}
    }

    gw_ui_root.remove_focus = function(wnd) {
        gw_ui_top_wnd = null;
        for (var i = this.children.length; i > 0; i--) {
            var c = this.children[i - 1];
            if (c == wnd) continue;
            if (!c.visible) continue;
            
            if (typeof c.on_event != 'undefined') {
                gw_ui_top_wnd = c;
                break;
            }
        }
        if (typeof (wnd._no_focus) == 'boolean') return;
        if (gwskin.focus_on)
            scene.set_focus(gw_ui_top_wnd);
    }
    
    gw_ui_root.add_child = function (child) {
        this.children[this.children.length] = child;
        this.set_focus(child);
    }

    gw_ui_root.remove_child = function (child) {
        this.remove_focus(child);
        this.removeChildren[0] = child;
    }
    gw_ui_top_wnd = null;
    gw_event_filters = [];

    scene.set_event_filter(gwlib_filter_event);
    gwskin.has_opengl = scene.has_opengl;

    gwskin.browser_mode = (scene.get_option('Temp', 'BrowserMode') == 'yes') ? true : false;

    scene.focus_highlight = false;

    gwskin.disable_transparency = false;
    //remove window gradients 
    if (!gwskin.has_opengl && !scene.hardware_rgba) {
        s = gwskin.get_style('window', 'normal');
        s.texture = null;
        gwskin.disable_transparency = true;
        gwskin.default_window_alpha = 1;
    }
    var device = scene.get_option('core', 'devclass');

    if ((device == 'ios') || (device == 'android')) gwskin.mobile_device = true;
    else gwskin.mobile_device = false;

    var cutout = scene.get_option('temp', 'display-cutout');
    gwskin.display_cutout = (cutout == 'yes') ? true : false;

    if (gwskin.mobile_device) {
        var size = scene.screen_width;
        if (size > scene.screen_height) size = scene.screen_height;

        gwskin_set_default_control_height(size/8);
        gwskin_set_default_icon_height(size/10);
    }
    
	gwskin_set_white_blue();

    gwskin._to_string = function (obj) {
            var res = '';
            if (obj instanceof Array) {
                res = '[';
                for (var i = 0; i < obj.length; i++) {
                    if (i) res += ', ';
                    res += gwskin._to_string(obj[i]);
                }
                res += ']';
            } else {
                var first = true;
                res = '{';
                for (var prop in obj) {
                    if (!first) res += ', ';
                    res += '' + prop;
                    res += ': ';
                    if (typeof obj[prop] == 'string') {
                        res += '"';
                        res += obj[prop];
                        res += '"';
                    } else if (typeof obj[prop] == 'function') {
                    } else if (typeof obj[prop] == 'object') {
                        res += gwskin._to_string(obj[prop]);
                    } else {
                        res += obj[prop];
                    }
                    first = false;
                }
                res += '}';
            }
            return res;
        }

        gwskin.stringify = function (obj) {
            return this._to_string(obj);
        }
        gwskin.parse = function (serial_obj) {
            gwlog(l_deb, 'JSON eval ' + serial_obj);
            return eval(serial_obj);
        }
}

function gwlib_add_event_filter(evt_filter) {
	if ((arguments.length==2) && (arguments[1]==true)) {
		gw_event_filters.unshift(evt_filter);
	} else {
		gw_event_filters.push(evt_filter);
	}
}

function gwlib_remove_event_filter(evt_filter) {
    var idx = gw_event_filters.indexOf(evt_filter);
    if (idx > -1) gw_event_filters.splice(idx, 1);
}

function gwlib_notify_display_size(width, height) {
    for (var i = 0; i < gw_ui_root.children.length; i++) {
        if (typeof gw_ui_root.children[i].on_display_size != 'undefined') {
            gw_ui_root.children[i].on_display_size(width, height);
        }
    }
}

function gwlib_refresh_layout() {
    for (var i = 0; i < gw_ui_root.children.length; i++) {
        if (typeof gw_ui_root.children[i].layout != 'undefined') {
            gw_ui_root.children[i].layout(gw_ui_root.children[i].width, gw_ui_root.children[i].height);
        }
    }
}


//static
function gw_reset_hit(obj, is_over) {
    if (is_over) {
        if (gwskin.last_hitable_obj && (gwskin.last_hitable_obj != obj)) {
            //reset isOver value of previous touch sensor
            if (typeof (gwskin.last_hitable_obj._ts_idx) != 'undefined') {
                gwskin.last_hitable_obj.children[gwskin.last_hitable_obj._ts_idx].isOver = false;
            }
            gwskin.last_hitable_obj._on_over(false);
        }
        gwskin.last_hitable_obj = obj;
    } else if (gwskin.last_hitable_obj == obj) {
        gwskin.last_hitable_obj = null;
    }
}

//static
function gw_add_child(container, child) {
    if (container == null) container = gw_ui_root;

    if (typeof (container.add_child) != 'undefined') {
        container.add_child(child);
    } else {
        container.children[container.children.length] = child;
    }
    child.parent = container;
}

//static
function gw_detach_child(child) {
    /*detach all default routes*/
    Browser.deleteRoute(child, 'ALL', null, null);
    if (typeof (child._ps2d_idx) != 'undefined') {
        Browser.deleteRoute(child.children[child._ps2d_idx], 'ALL', null, null);
    }
    if (typeof (child._ts_idx) != 'undefined') {
        Browser.deleteRoute(child.children[child._ts_idx], 'ALL', null, null);
    }
    if (typeof (child.dlg) != 'undefined') child.dlg = null;
    if (typeof (child.parent) == 'undefined') return;

    var p = child.parent;
    child.parent = null;
    if (typeof (p.remove_child) != 'undefined') {
        p.remove_child(child);
    } else {
        p.removeChildren[0] = child;
    }
    if (gwskin.last_hitable_obj == child) gwskin.last_hitable_obj = null;
}

//static
function gw_close_child_list(list) {
    var count = list.length;
    if (!count) return;
    for (var i = 0; i < count; i++) {
        var child = list[i];
        if (typeof child.close != 'undefined') child.close();
        else gw_detach_child(child);

        if (list.length < count) {
            i -= (count - list.length);
            count = list.length;
        }
    }
    //trigger GC whenever we have closed a child list
    scene.trigger_gc();
}


//static
gwskin.get_class = function (class_name) {
    if (arguments.length < 1) class_name = 'default';

    var styles = gwskin.styles.filter(function (obj) { return obj.name == class_name; });
    if (styles.length > 0) {
        return styles[0];
	}
	return null;
}

//static
gwskin.get_style = function (class_name, style_name) {
    if (arguments.length < 1) class_name = 'default';
    if (arguments.length < 2) style_name = 'normal';
    if (style_name == 'invisible') return gwskin.appearance_transparent;

    var style = null;
    var styles = gwskin.styles.filter(function (obj) { return obj.name == class_name; });
    if (styles.length > 0) {
        if (typeof styles[0][style_name] != 'undefined')
            return styles[0][style_name];
    } else {
        gwlog(l_err, 'Non-existing class ' + class_name);
    }

    style = gwskin.styles[0];
    if (typeof style[style_name] != 'undefined')
        return style[style_name];

    gwlog(l_err, 'Non-existing style ' + style_name + ' in default class');
    return null;
}

//static
gwskin.get_font = function (class_name) {
    if (arguments.length < 1) class_name = 'default';

    var styles = gwskin.styles.filter(function (obj) { return obj.name == class_name; });
    if (styles.length > 0) {
        if (typeof styles[0].font != 'undefined') {
            return styles[0].font;
        }
    } else {
        gwlog(l_err, 'Non-existing class ' + class_name);
    }
    return gwskin.styles[0].font;
}

function gw_new_container() {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Container');
    obj.push_to_top = function (wnd) {
        this.removeChildren[0] = wnd;
        this.addChildren[0] = wnd;
    }
    return obj;
}

function gw_new_curve2d(class_name) 
{
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Curve2D');
    var shape = new SFNode('Shape');
    obj.children[0] = shape;
    shape.appearance = gw_new_appearance(0, 0, 0);
    obj.children[0].appearance.material.filled = false;
    obj.children[0].appearance.material.lineProps = new SFNode('LineProperties');

    obj.set_color = function (r, g, b) {
        this.children[0].appearance.material.lineProps.lineColor.r = r;
        this.children[0].appearance.material.lineProps.lineColor.g = g;
        this.children[0].appearance.material.lineProps.lineColor.b = b;
    }
    obj.set_line_width = function (width) {
        if (!this.children[0].appearance.material.lineProps) this.children[0].appearance.material.lineProps = new SFNode('LineProperties');
        this.children[0].appearance.material.lineProps.width = width;
    }
    shape.geometry = new SFNode('Curve2D');
    shape.geometry.point = new SFNode('Coordinate2D');

    obj.reset = function () {
        this.children[0].geometry.point.point.length = 0;
        this.children[0].geometry.type.length = 0;
    }
    obj._add_coord = function (x, y) {
        var pts = this.children[0].geometry.point.point;
        pts[pts.length] = new SFVec2f(x, y);
    }
    obj.add_move_to = function (x, y) {
        this._add_coord(x, y);
        //first moveto is implicit
        if (this.children[0].geometry.type.length)
            this.children[0].geometry.type[this.children[0].geometry.type.length] = 0;
    }
    obj.add_line_to = function (x, y) {
        this._add_coord(x, y);
        this.children[0].geometry.type[this.children[0].geometry.type.length] = 1;
    }
    obj.remove_first = function () {
        var pts = this.children[0].geometry.point.point;
        if (pts.length < 2) pts.length = 0;
        else {
            for (var i = 0; i < pts.length - 1; i++) {
                pts[i] = pts[i + 1];
            }
            pts.length = i;
            
            pts = this.children[0].geometry.type;

            for (var i = 0; i < pts.length - 1; i++) {
                pts[i] = pts[i + 1];
            }
            pts.length = i;
        }
    }
    obj.get_num_points = function() {
        return this.children[0].geometry.point.point.length;
    }
    return obj;
}


function gw_new_rectangle(class_name, style) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Rectangle');
    var shape = new SFNode('Shape');
    obj.children[0] = shape;
    if (arguments.length == 1) style = 'normal';

    shape.appearance = gwskin.get_style(class_name, style);

    if (shape.appearance && (typeof (shape.appearance.skin) == 'undefined')) {
        obj.set_alpha = function (alpha) {
            this.children[0].appearance.material.transparency = 1 - alpha;
        }
        obj.get_alpha = function () {
            return 1 - this.children[0].appearance.material.transparency;
        }
        obj.set_fill_color = function (r, g, b) {
            this.children[0].appearance.material.emissiveColor.r = r;
            this.children[0].appearance.material.emissiveColor.g = g;
            this.children[0].appearance.material.emissiveColor.b = b;
        }
        obj.set_strike_color = function (r, g, b) {
            if (!this.children[0].appearance.material.lineProps) this.children[0].appearance.material.lineProps = new SFNode('LineProperties');
            this.children[0].appearance.material.lineProps.lineColor.r = r;
            this.children[0].appearance.material.lineProps.lineColor.g = g;
            this.children[0].appearance.material.lineProps.lineColor.b = b;
        }
        obj.set_line_width = function (width) {
            if (!this.children[0].appearance.material.lineProps) this.children[0].appearance.material.lineProps = new SFNode('LineProperties');
            this.children[0].appearance.material.lineProps.width = width;
        }
    } else {
        obj.set_alpha = function (alpha) { }
        obj.set_fill_color = function (r, g, b) { }
        obj.set_strike_color = function (r, g, b) { }
        obj.set_line_width = function (width) { }
        obj.get_alpha = function () { return 1; }
    }

    obj.set_style = function (classname, style) {
        var app = gwskin.get_style(classname, style);
        if (app) this.children[0].appearance = app;
    }


    shape.geometry = new SFNode('Curve2D');
    shape.geometry.point = new SFNode('Coordinate2D');

    obj.corner_tl = true;
    obj.corner_tr = true;
    obj.corner_bl = true;
    obj.corner_br = true;
    obj.set_corners = function (tl, tr, br, bl) {
        this.corner_tl = tl;
        this.corner_tr = tr;
        this.corner_bl = bl;
        this.corner_br = br;
    }
    obj.sfv = new SFVec2f(0, 0);

    obj.set_size = function (w, h) {
        var hw, hh, rx_bl, ry_bl, rx_br, ry_br, rx_tl, ry_tl, rx_tr, ry_tr, rx, ry;
        let shape = this.children[0];
        var temp = shape.geometry.type;
        hw = w / 2;
        hh = h / 2;
        this.width = w;
        this.height = h;
        temp[0] = 7;
        temp[1] = 1;
        temp[2] = 7;
        temp[3] = 1;
        temp[4] = 7;
        temp[5] = 1;
        temp[6] = 7;
        temp[7] = 6; /*close*/

        /*compute default rx/ry*/
        ry = rx = 10;
        if (rx >= hw) rx = hw;
        if (ry >= hh) ry = hh;
        rx_bl = rx_br = rx_tl = rx_tr = rx;
        ry_bl = ry_br = ry_tl = ry_tr = ry;
        if (!this.corner_tl) rx_tl = ry_tl = 0;
        if (!this.corner_tr) rx_tr = ry_tr = 0;
        if (!this.corner_bl) rx_bl = ry_bl = 0;
        if (!this.corner_br) rx_br = ry_br = 0;

        if (shape.geometry.point.point.length < 12) {
            shape.geometry.point.point.length = 12;
            for (var i = 0; i < 12; i++) {
                shape.geometry.point.point[i].x = 0;
            }
        }
        temp = this.children[0].geometry.point.point;

        this.sfv.x = hw - rx_tr;
        this.sfv.y = hh;
        temp[0] = this.sfv;
        this.sfv.x = hw; 
        this.sfv.y = hh; /*bezier ctrl point or line-to*/
        temp[1] = this.sfv;
        this.sfv.x = hw;
        this.sfv.y = hh - ry_tr;
        temp[2] = this.sfv;
        this.sfv.x = hw; 
        this.sfv.y = -hh + ry_br;
        temp[3] = this.sfv;
        this.sfv.x = hw;
        this.sfv.y = -hh; /*bezier control point*/
        temp[4] = this.sfv;
        this.sfv.x = hw - rx_br;
        this.sfv.y = -hh;
        temp[5] = this.sfv;
        this.sfv.x = -hw + rx_bl;
        this.sfv.y = -hh;
        temp[6] = this.sfv;
        this.sfv.x = -hw;
        this.sfv.y = -hh; /*bezier control point*/
        temp[7] = this.sfv;
        this.sfv.x = -hw;
        this.sfv.y = -hh + ry_bl;
        temp[8] = this.sfv;
        this.sfv.x = -hw;
        this.sfv.y = hh - ry_tl;
        temp[9] = this.sfv;
        this.sfv.x = -hw;
        this.sfv.y = hh; /*bezier control point*/
        temp[10] = this.sfv;
        this.sfv.x = -hw + rx_tl;
        this.sfv.y = hh;
        temp[11] = this.sfv;
    }
    return obj;
}

function gw_new_text(parent, label, class_name) {
    var obj = new SFNode('Transform2D');
    if (arguments.length < 3) class_name = 'default';

    setup_gw_object(obj, 'Text');
    gw_add_child(parent, obj);
    obj.children[0] = new SFNode('Transform2D');
    obj.children[0].children[0] = new SFNode('Shape');
    obj.children[0].children[0].geometry = new SFNode('Text');

    if (class_name == 'custom') {
        class_name = 'default';
        obj.children[0].children[0].appearance = gw_new_appearance(0, 0, 0);
        obj.children[0].children[0].geometry.fontStyle = gw_new_fontstyle(gwskin.default_label_font_size, 1);
    } else {
        obj.children[0].children[0].appearance = gwskin.get_style(class_name, 'text');
        obj.children[0].children[0].geometry.fontStyle = gwskin.get_font(class_name);
    }



    obj.set_label = function (value) {
        this.children[0].children[0].geometry.string.length = 0;
        this.children[0].children[0].geometry.string[0] = value;
    }

    obj.set_labels = function () {
        this.children[0].children[0].geometry.string.length = 0;
        for (var i = 0; i < arguments.length; i++) {
            this.children[0].children[0].geometry.string[i] = '' + arguments[i];
        }
    }
    obj.get_label = function () {
        var mfs = this.children[0].children[0].geometry.string;
        return mfs.length ? this.children[0].children[0].geometry.string[0] : '';
    }
    obj.set_width = function (value) {
        this.children[0].children[0].geometry.maxExtent = -value;
        this.width = value;
    }
    obj.font_size = function (value) {
        return this.children[0].children[0].geometry.fontStyle ? this.children[0].children[0].geometry.fontStyle.size : 1;
    }

    if (obj.children[0].children[0].appearance && (typeof (obj.children[0].children[0].appearance.skin) == 'undefined')) {
        obj.set_color = function (r, g, b) {
            this.children[0].children[0].appearance.material.emissiveColor = new SFColor(r, g, b);
        }
        obj.children[0].set_alpha = function (a) {
            this.children[0].children[0].appearance.material.transparency = 1 - a;
        }
        obj.set_align = function (val) {
            this.children[0].children[0].geometry.fontStyle.justify[0] = val;
        }
        obj.set_font_size = function (val) {
            this.children[0].children[0].geometry.fontStyle.size = val;
        }
    } else {
        obj.set_color = function (r, g, b) { }
        obj.set_alpha = function (a) { }
        obj.set_align = function (a) { }
        obj.set_font_size = function (a) { }
    }
    obj.set_size = function (width, height) {
        var justify = this.children[0].children[0].geometry.fontStyle ? this.children[0].children[0].geometry.fontStyle.justify[0] : '';
        if (justify == 'BEGIN') {
            this.children[0].translation.x = -width / 2;
        } else if (justify == 'END') {
            this.children[0].translation.x = width / 2;
        } else {
            this.children[0].translation.x = 0;
        }
        this.children[0].children[0].geometry.maxExtent = -width;
        this.width = width;
        this.height = height;
    }
    if (label) obj.set_label(label);
    return obj;
}


function gw_new_window(parent, offscreen, background, class_name, no_focus) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Window');
    obj.background = null;
    obj._is_window = true;

    if (arguments.length < 4) class_name = 'window';
    else if ((arguments.length == 4) && typeof (arguments[3]) == 'boolean') {
        obj._no_focus = true;
        class_name = 'window';
    }
    else if (arguments.length == 5) obj._no_focus = true;

    if (class_name == 'popup') {
        class_name = 'window';
        obj._popup = true;
    }

    if (background)
        obj.background = gw_new_rectangle(class_name);
    obj.visible = false;
    obj.scale.x = obj.scale.y = 0;
    obj._disable_rect = gw_new_rectangle(class_name, 'invisible');

    obj.on_event = function (evt) { return 0; }

    obj.set_style = function (classname, style) {
        if (this.background)
            this.background.set_style(classname, style);
    }
    if (!offscreen) {
        if (obj.background) gw_add_child(obj, obj.background);
        var item = new SFNode('Layer2D');
        setup_gw_object(item, 'Layer');
        gw_add_child(obj, item);
        obj.set_size = function (width, height) {
            this.width = width;
            this.height = height;
            var idx = 0;
            if (this.background) {
                this.children[0].set_size(width, height);
                idx++;
            }
            this.children[idx].size.x = width;
            this.children[idx].size.y = height;
            this.layout(width, height);
        }
        obj.set_alpha = function (alpha) {
            if (this.background) {
                this.children[0].set_alpha(alpha);
            }
        }
        obj.get_alpha = function () {
            if (this.background) {
                return this.children[0].get_alpha();
            }
            return 0.0;
        }
        obj.add_child = function (child) {
            var idx = this.background ? 1 : 0;
            this.children[idx].children[this.children[idx].children.length] = child;
        }
        obj.remove_child = function (child) {
            var idx = this.background ? 1 : 0;
            this.children[idx].removeChildren[0] = child;
        }
        obj._wnd_close = function () {
            obj.background = null;
            obj._disable_rect = null;
            gw_close_child_list(this.children);
        }
    } else {
        var shape = new SFNode('Shape');
        shape.appearance = new SFNode('Appearance');
        shape.appearance.material = new SFNode('Material2D');
        shape.appearance.material.filled = TRUE;
        shape.appearance.texture = new SFNode('CompositeTexture2D');

        if (obj.background) {
            //use background to make sure the underlying texture is RGB, not RGBA
            if (!gwskin.disable_transparency) {
                shape.appearance.texture.children[0] = obj.background;
            } else {
                shape.appearance.texture.background = gwskin.no_gl_window_back;

            }
        }
        shape.geometry = new SFNode('Bitmap');
        obj.children[0] = shape;

        obj.set_size = function (width, height) {
            this.width = width;
            this.height = height;
            this.children[0].appearance.texture.pixelWidth = width;
            this.children[0].appearance.texture.pixelHeight = height;
            this.children[0].appearance.texture.children[0].set_size(width, height);
            this.layout(width, height);
        }

        obj.set_alpha = function (alpha) {
            //disable image blending for offscreens with backgrounds
            if (this.background && gwskin.disable_transparency) alpha = 1;

            this.children[0].appearance.material.transparency = 1 - alpha;
        }
        obj.get_alpha = function () {
            return 1 - this.children[0].appearance.material.transparency;
        }

        obj.add_child = function (child) {
            this.children[0].appearance.texture.children[this.children[0].appearance.texture.children.length] = child;
        }
        obj.remove_child = function (child) {
            this.children[0].appearance.texture.removeChildren[0] = child;
        }
        obj._wnd_close = function () {
            obj.background = null;
            obj._disable_rect = null;
            gw_close_child_list(this.children[0].appearance.texture.children);
        }
    }
    obj._pre_destroy = obj._wnd_close;
    obj.set_corners = function (tl, tr, br, bl) {
        if (this.background)
            this.background.set_corners(tl, tr, br, bl);
    }

    obj._is_disable = false;
    obj.disable = function () {
        if (this._is_disable) return;
        this._is_disable = true;
        this._disable_rect.set_size(this.width, this.height);
        this.add_child(this._disable_rect);
    }
    obj.enable = function () {
        if (!this._is_disable) return;
        this._is_disable = false;
        this.remove_child(this._disable_rect);
    }

    if (typeof obj._popup != 'boolean') {
        var s = gwskin.get_class(class_name);

        if (s) {
            if (typeof s.show != 'undefined') obj.show = s.show;
            if (typeof s.hide != 'undefined') obj.hide = s.hide;
        }
    }

    obj.on_close = null;
    obj._orig_close = obj.close;
    obj._on_wnd_close = function () {
        if (this.on_close != null) this.on_close();
        this._orig_close();
    }
    obj.close = function () {
        this.hide(this._on_wnd_close);
    }
    obj.layout = function (w, h) { }

    obj.set_alpha(gwskin.default_window_alpha);
    gw_add_child(parent, obj);
    return obj;
}

function gw_new_image(parent, src_url) {
	var obj = new SFNode('Transform2D');
	setup_gw_object(obj, 'Image');
	obj.children[0] = new SFNode('Layer2D');
	obj.children[0].children[0] = new SFNode('Inline');
	obj.children[1] = gw_new_rectangle('default', 'invisible');
	gw_object_set_hitable(obj.children[1]);

	obj.on_click = null;
	obj.children[1]._par = obj;
	obj.children[1].on_click = function() {
		if (this._par.on_click) this._par.on_click();
	}
	obj.set_image = function(src_url) {
		var inl = gw_load_resource(src_url, false);
		this.children[0].children[0] = inl;
	}
	obj.set_size = function(width, height) {
		this.children[0].size.x = width;
		this.children[0].size.y = height;
		this.width = width;
		this.height = height;
		this.children[1].set_size(width, height);
	}
	obj._pre_destroy = function () {
		this.children[0].children.length = 0;
	}
	obj.on_event = function () { return false; }

	obj.set_image(src_url);
	
	gw_add_child(parent, obj);
	return obj;
}


function gw_new_icon_button(parent, icon_url, label, horizontal, class_name) {
    var touch;
    
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'IconButton');

    if (arguments.length == 3) {
        horizontal = false;
        class_name = 'default';
    }
    else if (arguments.length == 4) {
        if (typeof arguments[3] == 'string') {
            class_name = arguments[3];
            horizontal = false;
        } else {
            class_name = 'default';
        }
    }

    obj._icon_root = new SFNode('Transform2D');
    obj._icon_root.children[0] = new SFNode('Layer2D');
    obj._touch = new SFNode('TouchSensor');

    obj._show_highlight = false;
    obj._is_icon = false;
    obj._tooltip = true;
    if (class_name == 'image') {
		obj._is_icon = false;
		obj._tooltip = false;
		class_name = 'default';
    }
	else if (class_name == 'icon') {
		obj._is_icon = true;
		obj._highlight = gw_new_rectangle(class_name, 'invisible');
	}
    else if (class_name == 'icon_label') {
        obj._is_icon = true;
        obj._show_highlight = false;
        class_name = 'checkbox';
    }
    else if (class_name == 'listitem') {
        obj._is_icon = true;
        obj._show_highlight = true;
		obj._highlight = gw_new_rectangle(class_name, 'invisible');
        obj._tooltip = false;
    } else {
        obj._highlight = gw_new_rectangle(class_name, 'invisible');
        obj._show_highlight = true;
    }

    if (obj._highlight)
        obj.children[0] = obj._highlight;

    obj.children[obj.children.length] = obj._icon_root;
    obj.children[obj.children.length] = obj._touch;
    obj._label = null;

    var skip_label = (label == null) ? true : false;

    if (!skip_label && gwskin.get_font(class_name) != null) {
        obj._label = gw_new_text(obj, label, class_name);
        if (horizontal) {
            obj.set_size = function (width, height) {
                this._icon_root.children[0].size.x = height;
                this._icon_root.children[0].size.y = height;
                this._icon_root.translation.x = (height - width) / 2;
                if (this._highlight)
                    this._highlight.set_size(width, height);
                this._label.set_width(width - height);
                this._label.translation.x = height - width / 2;
                this.width = width;
                this.height = height;
            };
        } else {
            obj.set_size = function (width, height) {
                var fsize = this._label.font_size();

                var h = height - fsize - gwskin.default_icon_text_spacing;
                this._icon_root.children[0].size.x = h;
                this._icon_root.children[0].size.y = h;
                this._icon_root.translation.y = (height - h) / 2;
                var w = width;
                if (this._highlight)
                    this._highlight.set_size(w, height);
                this._label.set_width(w);
                this._label.translation.y = -height / 2 + fsize / 2;
                this.width = w;
                this.height = height;
            };
        }
        obj.set_label = function (label) {
            this._label.set_label(label);
        }
        obj.get_label = function () {
			return this._label ? this._label.get_label() : null;
        }
    } else {
        obj.set_size = function (width, height) {
            this._icon_root.children[0].size.x = width;
            this._icon_root.children[0].size.y = height;
            if (this._highlight)
                this._highlight.set_size(width, height);
            this.width = width;
            this.height = height;
        };
        obj.set_label = function (label) {
            this.label = label;
        }
        obj.get_label = function () {
            return this.label;
        }
    }

    obj._last_ts = 0;
    obj.on_long_click = NULL;
    obj.on_click = NULL;
    obj.on_over = null;
    obj.down = false;
    obj.over = false;
    obj._on_active = function (value, timestamp) {
        if (value) {
            this.down = true;
            if (this._is_icon) {
                var app = gwskin.get_style(class_name, 'down');
                this._icon_root.children[0].children[0].set_style(app);
            }

            if (this._show_highlight) {
                this._highlight.set_style(class_name, 'down');
            }

            this._last_ts = timestamp;
        } else {
            if (this._is_icon) {
                var app = gwskin.get_style(class_name, (this.over && gwskin.pointing_device) ? 'over' : 'normal');
                this._icon_root.children[0].children[0].set_style(app);
            }

            if (this._show_highlight) {
                this._highlight.set_style(class_name, (this.over && gwskin.pointing_device) ? 'over' : 'invisible');
            }

            if (this.down) {
                if (this.on_long_click && (timestamp - this._last_ts > gwskin.long_click_delay)) this.on_long_click();
                else if (this.on_click) this.on_click();
            }
            this.down = false;
        }
    };
    obj._on_over = function (value) {
        this.over = value;
        gw_reset_hit(this, value);

        if (gwskin.pointing_device) {
            if (this._is_icon) {
                var app = gwskin.get_style(class_name, value ? 'over' : 'normal');
                if (app) {
                    this._icon_root.children[0].children[0].set_style(app);
                }
            }

            if (this._show_highlight) {
                this._highlight.set_style(class_name, value ? 'over' : 'invisible');
            }

            if (this.on_over) this.on_over(value);
            if (gwskin.tooltip_callback) gwskin.tooltip_callback(this, value);
        }
    };
    Browser.addRoute(obj._touch, 'isOver', obj, obj._on_over);
    Browser.addRoute(obj._touch, 'isActive', obj, obj._on_active);

    obj.icons = [];
    obj.add_icon = function (url) {

        if (typeof url == 'string') {
            var inl;
            if (typeof gwskin.images[url] != 'undefined') {
                inl = gw_load_resource(gwskin.images[url], this._is_icon);
            } else {
                inl = gw_load_resource(url, this._is_icon);
            }
        } else {
            var inl = url;
        }
        this.icons[this.icons.length] = inl;

        if (this._icon_root.children[0].children.length == 0) {
            this._icon_root.children[0].children[0] = this.icons[0];
        }
    }
    obj.switch_icon = function (idx) {
        while (idx > this.icons.length) idx -= this.icons.length;
        this._icon_root.children[0].children[0] = this.icons[idx];
    }
		
    obj._pre_destroy = function () {
        for (var i in this.icons) {
            gw_unload_resource(this.icons[i]);
        }
        this.icons.length = 0;
        this._highlight = null;
        this._icons = null;
        this._label = null;
        Browser.deleteRoute(this._touch, 'ALL', null, null);
        this._touch = null;
    }

    if (icon_url) obj.add_icon(icon_url);
    obj.set_label(label);

    obj.enable = function() {
		this._touch.enabled = true;
		this._on_over(false);
	}
    obj.disable = function() {
		this._touch.enabled = false;
		var app = gwskin.get_style(class_name, 'disable');
		if (app) {
			this._icon_root.children[0].children[0].set_style(app);
		}
	}


    obj.on_event = function () { return false; }

    if (0 && (typeof gwskin[class_name] != 'undefined') && (typeof gwskin[class_name].height != 'undefined')) {
        obj.set_size(gwskin[class_name].height, gwskin[class_name].height);
    } else {
        obj.set_size(gwskin.default_control_height, gwskin.default_control_height);
    }
    gw_add_child(parent, obj);
    return obj;
}

function gw_new_icon(parent, icon_name) {
    return gw_new_icon_button(parent, gwskin.images[icon_name], gwskin.labels[icon_name], false, 'icon');
}

function gw_new_checkbox(parent, label) {
    var touch;
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'CheckBox');

    obj._icon_root = new SFNode('Transform2D');
    obj._icon_root.children[0] = new SFNode('Layer2D');
    obj._touch = new SFNode('TouchSensor');

    var class_name = 'checkbox';

    obj.children[0] = gw_new_rectangle(class_name, 'invisible');
    obj.children[1] = obj._icon_root;
    gw_new_text(obj, label, class_name);
    obj.children[3] = obj._touch;

    obj.set_size = function (width, height) {
        this._icon_root.children[0].size.x = height;
        this._icon_root.children[0].size.y = height;
        this._icon_root.translation.x = (height - width) / 2;
        this.children[0].set_size(width, height);
        this.children[2].set_width(width - height);
        this.children[2].translation.x = (height - width) / 2 + height;
        this.width = width;
        this.height = height;
    };

    obj.set_label = function (label) {
        this.children[2].set_label(label);
    }
    obj.get_label = function () {
        return this.children[2].get_label();
    }
    obj.on_check = NULL;
    obj.down = false;
    obj.over = false;
    obj.checked = false;
    obj._on_active = function (value, timestamp) {
        if (value) {
            this.down = true;
        } else {
            if (this.down && this.over) {
                this._set_checked(!this.checked);
                if (this.on_check) this.on_check(this.checked);
            }
            this.down = false;
        }
    };
    obj._on_over = function (value) {
        this.over = value;
        gw_reset_hit(this, value);

        if (gwskin.pointing_device) {
                        var app = gwskin.get_style(class_name, value ? 'over' : (this.checked ? 'down' : 'normal') );
                        if (app) {
                            this._icon_root.children[0].children[0].set_style(app);
                        }
            if (this.on_over) this.on_over(value);
            if (gwskin.tooltip_callback) gwskin.tooltip_callback(this, value);
        }
    };
    Browser.addRoute(obj._touch, 'isOver', obj, obj._on_over);
    Browser.addRoute(obj._touch, 'isActive', obj, obj._on_active);


    obj._set_checked = function (value) {
        this.checked = value;
        var app = gwskin.get_style(class_name, value ? 'down' : 'normal');
        this._icon_root.children[0].children[0].set_style(app);
    }
    obj.set_checked = function (value) {
        this._set_checked(value);
    }
    obj._icon_root.children[0].children[0] = gw_load_resource(gwskin.images.check, true);

    obj._pre_destroy = function () {
        for (var i in this.icons) {
            gw_unload_resource(this.icons[i]);
        }
        Browser.deleteRoute(this._touch, 'ALL', null, null);
        this._touch = null;
        this._icon_root = null;
    }
    obj.set_label(label);

    obj.on_event = function () { return false; }

    if (0 && (typeof gwskin[class_name] != 'undefined') && (typeof gwskin[class_name].height != 'undefined')) {
        obj.set_size(gwskin[class_name].height, gwskin[class_name].height);
    } else {
        obj.set_size(gwskin.default_control_height, gwskin.default_control_height);
    }
    gw_add_child(parent, obj);
    return obj;
}



function gw_new_spincontrol(parent, horizontal) {
    var touch;
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'SpinControl');

    obj.value = 0;
    if (arguments.length < 2) horizontal = false;
    obj._horizontal = horizontal;
    obj.on_click = function () { }
    var tool = gw_new_icon(obj, horizontal ? 'right' : 'up');

    obj.min = 0;
    obj.max = -1;
    tool.on_click = function () {
        if (this.parent.min < this.parent.max) {
            if (this.parent.value >= this.parent.max) {
                return;
            }
        }
        this.parent.value++;
        this.parent.on_click(this.parent.value);
    }
    tool = gw_new_icon(obj, horizontal ? 'left' : 'down');
    tool.on_click = function () {
        if (this.parent.min < this.parent.max) {
            if (this.parent.value <= this.parent.min) {
                return;
            }
        }
        this.parent.value--;
        this.parent.on_click(this.parent.value);
    }

    obj.set_size = function (width, height) {
        if (this._horizontal) {
            this.children[0].set_size(width / 2, height);
            this.children[1].set_size(width / 2, height);
            this.children[0].translation.x = width / 4;
            this.children[1].translation.x = -width / 4;
        } else {
            this.children[0].set_size(width, height / 2);
            this.children[1].set_size(width, height / 2);
            this.children[0].translation.y = height / 4;
            this.children[1].translation.y = -height / 4;
        }
        this.width = width;
        this.height = height;
    };
    obj._idx = 0;
    obj.on_event = function (evt) {
        if (evt.type == GF_EVENT_KEYDOWN) {
            var idx = this._idx;
            if (this._horizontal) {
                if (evt.keycode == 'Left') idx--;
                else if (evt.keycode == 'Right') idx++;
                else return false;
            } else {
                if (evt.keycode == 'Up') idx--;
                else if (evt.keycode == 'Down') idx++;
                else return false;
            }
            if (idx < 0) {
                return false;
            }
            if (idx > 1) {
                return false;
            }
            this._idx = idx;
            scene.set_focus(this.children[idx]);
            return true;
        }
        return false;
    }

    gw_add_child(parent, obj);
    return obj;
}




function gw_new_subscene(parent) {
    var inline = new SFNode('Inline');
    inline._name = 'Subscene';
    inline._pre_destroy = function () {
        this.url[0] = '';
        this.url.length = 0;
    }
    inline.connect = function (url) {
        this.url.length = 0;
        this.url[0] = url;
    }
    gw_add_child(parent, inline);
    return inline;
}

function gw_new_button(parent, text, class_name) {
    var label;
    if (arguments.length < 3) class_name = 'button';
    obj = gw_new_rectangle(class_name, 'normal');
    label = gw_new_text(obj, text, class_name);
    gw_object_set_hitable(obj);
    obj.on_over = function (value) {
        if (gwskin.pointing_device) {
            this.set_style(class_name, value ? 'over' : 'normal');
        }
        //if (gwskin.tooltip_callback) gwskin.tooltip_callback(this, value);
    }
    obj.on_down = function (val) {
        this.set_style(class_name, val ? 'down' : (this._over ? 'over' : 'normal'));
    }

    obj._set_size = obj.set_size;
    obj.set_size = function (width, height) {
        this._set_size(width, height);
        this.children[1].set_size(width, height);
        this.children[1].set_width(width);
    };
    obj.set_label = function (label) {
        this.children[1].set_label(label);
    }
    obj.get_label = function () {
        return this.children[1].get_label();
    }
    obj.set_labels = function () {
        this.children[1].set_labels.apply(this.children[1], arguments);
    }
    obj.on_event = function (x, y) { return false; }

    obj.disable = function () {
        this._disable_touch();
    }
    obj.enable = function () {
        this._enable_touch();
    }

    gw_add_child(parent, obj);
    return obj;
}


function gw_new_gauge(parent, text) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'ProgressBar');

    if (arguments.length <= 3) class_name = 'progress';

    obj.children[0] = gw_new_rectangle(class_name, 'normal');
    obj.children[1] = gw_new_rectangle(class_name, 'over');
    gw_new_text(obj, text, class_name);


    obj._value = 75;
    obj._set_size = obj.set_size;
    obj.set_size = function (width, height) {
        this._set_size(width, height);
        this.children[0].set_size(width, height);

        this.children[1].set_size(this._value * width / 100, height);
        this.children[1].move((this._value - 100) * width / 200, 0);

        this.children[2].set_size(width, height);
        this.children[2].set_width(width);
    };
    obj.set_label = function (label) {
        this.children[2].set_label(label);
    }
    obj.get_label = function () {
        return this.children[2].get_label();
    }

    obj.set_value = function (val) {
        this._value = (val > 100) ? 100 : ((val < 0) ? 0 : val);
        this.set_size(this.width, this.height);
    }

    obj.on_event = function (x, y) { return false; }

    gw_add_child(parent, obj);
    return obj;
}



function grid_event_navigate(dlg, children, type) {
    var i;
    if (dlg.current_focus == -1) {
        if (type == 'Left') return false;
        dlg.current_focus = 0;
        scene.set_focus(children[0]);
        return true;
    }

    if (type == 'Right') {
        var orig_focus = dlg.current_focus;
        var switch_page = 0;
        var tr = children[dlg.current_focus].translation.y - children[dlg.current_focus].height / 2;
        if (dlg.current_focus + 1 == children.length) {
            dlg.current_focus = orig_focus;
            scene.set_focus(children[dlg.current_focus]);
            return false;
        }
        /*goto right item*/
        dlg.current_focus++;
        /*check we are not going down*/
        while (1) {
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2 > tr))
                break;
            if (dlg.current_focus + 1 == children.length) {
                dlg.current_focus = orig_focus;
                scene.set_focus(children[dlg.current_focus]);
                return false;
            }
            dlg.current_focus++;
            switch_page = 1;
        }
        /*check we haven't move to a new page*/
        if (children[orig_focus].translation.y + children[orig_focus].height / 2 <= children[dlg.current_focus].translation.y - children[dlg.current_focus].height / 2) {
            switch_page = 1;
        }

    }
    else if (type == 'Left') {
        var orig_focus = dlg.current_focus;
        var switch_page = 0;
        var tr = children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2;
        if (!dlg.current_focus) {
            dlg.current_focus = -1;
            scene.set_focus(null);
            return false;
        }
        /*goto left item*/
        dlg.current_focus--;
        /*check we are not going up*/
        while (1) {
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y - children[dlg.current_focus].height / 2 < tr))
                break;
            if (!dlg.current_focus) {
                dlg.current_focus = orig_focus;
                return false;
            }
            dlg.current_focus--;
            switch_page = -1;
        }
        /*check we haven't move to a new page*/
        if (children[orig_focus].translation.y - children[orig_focus].height / 2 >= children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2) {
            switch_page = -1;
        }
    }
    else if (type == 'Down') {
        var orig_focus = dlg.current_focus;
        var switch_page = 0;
        var bottom_y = children[dlg.current_focus].translation.y - children[dlg.current_focus].height / 2;
        var top_y = children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2;
        if (dlg.current_focus + 1 == children.length) return false;
        dlg.current_focus++;
        /*goto next line*/
        while (1) {
            //next child below current
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y - children[dlg.current_focus].height / 2 < bottom_y))
                break;
            //next child above current - we need a page swicth
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2 > top_y)) {
                switch_page = 1;
                break;
            }
            //not found
            if (dlg.current_focus + 1 == children.length) {
                dlg.current_focus = orig_focus;
                return false;
            }
            dlg.current_focus++;
        }
        /*find closest item on the left*/
        var tx = children[orig_focus].translation.x - children[orig_focus].width / 2;
        while (1) {
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.x + children[dlg.current_focus].width / 2 > tx))
                break;
            if (dlg.current_focus + 1 == children.length) break;
            dlg.current_focus++;
        }
    }
    else if (type == 'Up') {
        var orig_focus = dlg.current_focus;
        var switch_page = 0;
        var bottom_y = children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2;
        var top_y = children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2;
        if (!dlg.current_focus) return false;
        dlg.current_focus--;
        /*goto previous line*/
        while (1) {
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2 > top_y))
                break;
            //next child below current - we need a page swicth
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y + children[dlg.current_focus].height / 2 < bottom_y)) {
                switch_page = -1;
                break;
            }
            if (!dlg.current_focus) {
                dlg.current_focus = orig_focus;
                return false;
            }
            dlg.current_focus--;
        }
        /*goto above child*/
        var tx = children[orig_focus].translation.x + children[orig_focus].width / 2;
        while (1) {
            if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.x - children[dlg.current_focus].width / 2 < tx))
                break;
            if (!dlg.current_focus) {
                dlg.current_focus = orig_focus;
                return false;
            }
            dlg.current_focus--;
        }
    } else {
        return false;
    }
    if (switch_page == 1) return dlg.on_next_page();
    else if (switch_page == -1) return dlg.on_prev_page();
    if (typeof children[dlg.current_focus].on_event == 'undefined') {
        return grid_event_navigate(dlg, children, type);
    }

    scene.set_focus(children[dlg.current_focus]);
    return true;
}



function gw_new_grid_container(parent) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Layout');

    obj.children[0] = new SFNode('Layer2D');
    obj._container = new SFNode('Transform2D');
    obj.children[0].children[0] = obj._container;
    obj._all_children = [];
    obj._pages = [];

    obj.on_page_changed = null;

    obj.on_prev_page = function () {
        this._move_page(-1);
        return true;
    };
    obj.on_next_page = function () {
        this._move_page(1);
        return true;
    };

    obj.is_first_page = function () {
        return this._page_idx == 0 ? true : false;
    }
    obj.is_last_page = function () {
        return this._page_idx == (this._pages.length - 1) ? true : false;
    }
    obj._page_idx = 0;
    obj._max_page_idx = 0;
    obj._move_page = function (value) {
        this._page_idx += value;
        var i, child, last_child = 0;

        if (this._page_idx <= 0) {
            this._page_idx = 0;
        }

        if (this._page_idx * this.width >= this.max_width) {
            this._page_idx = this.max_width / this.width;
        }

        this._container.translation.x = -this._page_idx * this.width;
        this._container.children.length = 0;
        if (this._page_idx + 1 >= this._pages.length) {
            last_child = this._all_children.length;
        } else {
            last_child = this._pages[this._page_idx + 1];
        }

        i = 0;
        child = this._pages[this._page_idx];
        if (this.current_focus >= 0) {
            if (this.current_focus < child) this.current_focus = child;
            else if (this.current_focus >= last_child) this.current_focus = last_child - 1;
            scene.set_focus(this._all_children[this.current_focus]);
        }


        while (child < last_child) {
            this._container.children[i] = this._all_children[child];
            child++;
            i++;
        }
        if (this.on_page_changed) this.on_page_changed();

    }

    obj.spread_h = false;
    obj.spread_v = false;
    obj.break_at_hidden = false;
    obj.break_at_line = 0;

    obj.set_focus_last = function () {
        this._page_idx = this._pages.length ? this._pages.length - 1 : 0;
        this.current_focus = this._all_children.length;
        this._move_page(0);
    }
    obj.set_focus_first = function () {
        this._page_idx = 0;
        this.current_focus = 0;
        this._move_page(0);
    }

    obj.set_size = function (width, height) {
        this.width = width;
        this.height = height;
        this.children[0].size.x = width;
        this.children[0].size.y = height;
        this.nb_visible = 0;
        this.current_focus = -1;
        this.layout();
    }
    obj.max_width = 0;
    obj.layout = function () {
        var spread_x, nb_wid_h, start_x, start_y, maxh, width, height, page_x, nb_on_line;
        var children = this._all_children;
        var width = this.width;
        var height = this.height;

        if (width <= 0) return;
        if (height <= 0) return;

        if (typeof this.on_size != 'undefined')
            this.on_size(width, height);

        var page_x = 0;
        var maxh = 0;
        var spread_x = -1;
        var start_x = -width / 2;
        var init_y = this.height / 2;
        var start_y = init_y;
        var nb_on_line = 0;

        this._pages = [0];

        for (var i = 0; i < children.length; i++) {
            
            //start of line: compute H spread and max V size
            if (spread_x == -1) {
                var j = 0, len = 0, maxh = 0, nb_child = 0, nb_spread_child = 0;
                start_x = -width / 2 + page_x;
                while (1) {
                    if (!children[i + j].visible) {
                        j++;
                        if (i + j == children.length) break;
                        if (this.break_at_hidden) break;
                        if (typeof (children[i+j-1].__separator) == 'boolean') break;
                        continue;
                    }
                    if (len + children[i + j].width > width) break;
                    len += children[i + j].width;
                    if (maxh < children[i + j].height) maxh = children[i + j].height;
                    j++;
                    nb_child++;
                    if ((i + j < children.length) && (typeof children[i+j].stick_to_previous == 'boolean') && children[i+j].stick_to_previous) {
                    } else {
                        nb_spread_child++;
                    }

                    if (this.break_at_line) break;
                    if (i + j == children.length) break;
                }
                if (nb_child <= 1) {
                    maxh = children[i].height;
                    if (this.spread_h && !this.break_at_line) start_x = -len / 2;
                }
                else if (this.spread_h && nb_spread_child) {
                    spread_x = (width - len) / (nb_spread_child);
                    start_x += spread_x / 2;
                } else {
                    spread_x = 0;
                }
            }

            if (!children[i].visible) {
                if (nb_on_line && ( this.break_at_hidden || (typeof (children[i].__separator) == 'boolean')) ) {

                    nb_on_line = 0;
                    spread_x = -1;
                    start_y -= maxh / 2;
                    start_x = -width / 2;
                }
                continue;
            }
            if (start_y - maxh < -height / 2) {
                //push new page only if not empty! (otherwise we've been asked to layout in a too small grid ...)
                if (!this._pages.length || this._pages[this._pages.length - 1] < i) {
                    nb_on_line = 0;
                    page_x += width;
                    spread_x = -1;
                    start_y = init_y;
                    this._pages.push(i);
                    i--;
                    continue;
                }
            }
            children[i].translation.x = start_x + children[i].width / 2;
            children[i].translation.y = start_y - maxh / 2;

            if ((i + 1 < children.length) && (typeof children[i + 1].stick_to_previous == 'boolean') && children[i+1].stick_to_previous) {
                start_x += children[i].width;
            } else {
                start_x += children[i].width + spread_x;
            }

            nb_on_line++;

            if (i + 1 == children.length) {
                break;
            }
            if ((nb_on_line == nb_child) || (start_x - page_x + children[i + 1].width > width / 2)) {
                nb_on_line = 0;
                spread_x = -1;
                start_y -= maxh;
                start_x = -width / 2;
            }
        }
        this.max_width = page_x;
        this._move_page(0);
    }

    obj.add_child = function (child) {
        this._all_children.push(child);
    }
    obj.remove_child = function (child) {
        var index = this._all_children.indexOf(child);
        if (index > -1) this._all_children.splice(index, 1);
    }

    obj.get_children = function () {
        return this._all_children;
    }
    obj.reset_children = function () {
        gw_close_child_list(this._all_children);
        this._all_children.length = 0;
        this._container.children.length = 0;
        this._page_idx = 0;
        this._max_page_idx = 0;
    }


    obj.on_event = function (evt) {
        switch (evt.type) {
            case GF_EVENT_MOUSEWHEEL:
		var inc = -evt.wheel;
                if (this._pages.length <= 1) return 0;
                if ((inc < 0) && (this._page_idx == 0)) return 0;
                if ((inc > 0) && (this._page_idx >= this._pages.length - 1)) return 0;
                this._move_page((inc < 0) ? -1 : 1);
                return 1;
            case GF_EVENT_KEYDOWN:
                if ((evt.keycode == 'Up') || (evt.keycode == 'Down') || (evt.keycode == 'Right') || (evt.keycode == 'Left')) {

                    if ((this.current_focus >= 0) && typeof this._all_children[this.current_focus].on_event != 'undefined') {
                        var res = this._all_children[this.current_focus].on_event(evt);
                        if (res) return res;
                    }

                    return grid_event_navigate(this, this._all_children, evt.keycode);
                }
                if ((evt.keycode == 'PageUp') && (this._page_idx > 0)) {
                    if (this._pages.length <= 1) return 0;
                    return this.on_prev_page();
                }
                if ((evt.keycode == 'PageDown') && (this._page_idx < this._pages.length)) {
                    if (this._pages.length <= 1) return 0;
                    return this.on_next_page();
                }
                return 0;
        }
        return 0;
    }

    obj._pre_destroy = function () {
        this.reset_children();
        this._container = null;
        this._all_children.length = 0;
    }

    gw_add_child(parent, obj);
    return obj;
}

function gw_new_separator(parent) {
    var obj = gw_new_container();
    obj.hide();
    obj.show = function() {};
    obj.__separator = true;
    gw_add_child(parent, obj);
    return obj;
}

function gw_new_progress_bar(parent, vertical, with_progress, class_name) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'ProgressBar');
    var ps2d;

    if (arguments.length <= 3) class_name = 'progress';
    if (arguments.length <= 2) with_progress = false;
    if (arguments.length <= 1) vertical = false;

    obj.children[0] = gw_new_rectangle(class_name, 'normal');

    if (with_progress) {
        obj.children[1] = gw_new_rectangle(class_name, 'down');
        if (vertical) {
            obj.children[1].set_corners(true, false, false, true);
        } else {
            obj.children[1].set_corners(true, false, false, true);
        }
        obj.set_progress = function (prog) {
            if (prog < 0) prog = 0;
            else if (prog > 100) prog = 100;
            this.prog = prog;
            prog /= 100;
            if (this.vertical) {
                var pheight = prog * this.height;
                this.children[1].set_size(this.width, pheight);
                this.children[1].move(0, (this.height - pheight) / 2, 0);
            } else {
                var pwidth = prog * this.width;
                this.children[1].set_size(pwidth, this.height/3);
                this.children[1].move((pwidth - this.width) / 2, 0);
            }
        }
    } else {
        obj.set_progress = function (prog) { }
    }
    obj.slide_idx = obj.children.length;
    obj.children[obj.slide_idx] = gw_new_rectangle(class_name, 'over');
    if (vertical) {
        obj.children[obj.slide_idx].set_corners(true, false, false, true);
    } else {
        obj.children[obj.slide_idx].set_corners(true, false, false, true);
    }
    obj.children[obj.slide_idx + 1] = gw_new_rectangle(class_name, 'invisible');
    ps2d = new SFNode('PlaneSensor2D');
    obj.children[obj.slide_idx + 1].children[1] = ps2d;
    ps2d.maxPosition.x = -1;
    ps2d.maxPosition.y = -1;

    obj.on_slide = function (value, type) { }

    obj._with_progress = with_progress;
    obj._sliding = false;
    obj._slide_active = function (val) {
        obj._sliding = val;
        this.on_slide(this.min + (this.max - this.min) * this.frac, val ? 1 : 2);
    }
    Browser.addRoute(ps2d, 'isActive', obj, obj._slide_active);

    obj._set_trackpoint = function (value) {
        if (this.vertical) {
            this.frac = value.y / this.height;
        } else {
            this.frac = value.x / this.width;
        }
        this.frac += 0.5;
        if (this.frac > 1) this.frac = 1;
        else if (this.frac < 0) this.frac = 0;

        this._set_frac();
        this.on_slide(this.min + (this.max - this.min) * this.frac, 0);
    }
    Browser.addRoute(ps2d, 'trackPoint_changed', obj, obj._set_trackpoint);

    obj._set_frac = function () {
        if (this.vertical) {
            var pheight = this.frac * this.height;
            if (pheight < this.width) pheight = this.width;
            this.children[this.slide_idx].set_size(this.width, pheight);
            this.children[this.slide_idx].move(0, (this.height - pheight) / 2, 0);
        } else {
            var pwidth = this.frac * this.width;
            if (pwidth < this.height) pwidth = this.height;
            this.children[this.slide_idx].set_size(pwidth, this.height);
            this.children[this.slide_idx].move((pwidth - this.width) / 2, 0);
        }
    }

    obj.set_value = function (value) {
        if (this._sliding) return;

        value -= this.min;
        if (value < 0) value = 0;
        else if (value > this.max - this.min) value = this.max - this.min;
        if (this.max == this.min) value = 0;
        else value /= (this.max - this.min);

        this.frac = value;
        this._set_frac();
    }


    obj.vertical = vertical;
    obj.set_size = function (w, h) {
        this.width = w;
        this.height = h;
        this.children[0].set_size(w, h);
        if (this._with_progress) {
            this.children[3].set_size(w, h);
        } else {
            this.children[2].set_size(w, h);
        }
        this._set_frac();
        this.set_progress(this.prog);
    }
    obj.min = 0;
    obj.max = 100;

    obj.move = function (x, y) {
        this.translation.x = x;
        this.translation.y = y;
    }
    obj.on_event = function (x, y) { return false; }

    obj.prog = 0;
    obj.frac = 0;
    obj.set_size(vertical ? 10 : 200, vertical ? 200 : 10);

    gw_add_child(parent, obj);
    return obj;
}

function gw_new_slider(parent, vertical, class_name) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Slider');

    if (arguments.length <= 2) class_name = 'progress';
    if (arguments.length <= 1) vertical = false;

    obj.children[0] = gw_new_rectangle(class_name, 'normal');
    obj.children[1] = gw_new_rectangle(class_name, 'over');
    obj.children[2] = gw_new_rectangle(class_name, 'invisible');

    var ps2d = new SFNode('PlaneSensor2D');
    obj.children[2].children[1] = ps2d;
    ps2d.maxPosition.x = -1;
    ps2d.maxPosition.y = -1;
    obj.on_slide = function (value, type) { }
    
    obj.slide_active = function (val) {
        this.on_slide(this.min + (this.max - this.min) * this.frac, val ? 1 : 2);
    }
    Browser.addRoute(ps2d, 'isActive', obj, obj.slide_active);
    obj.set_trackpoint = function (value) {
        if (vertical) {
            if (this.height == this.children[1].height) return;
            this.frac = value.y / (this.height - this.children[1].height);
        } else {
            if (this.width == this.children[1].width) return;

            this.frac = value.x / (this.width - this.children[1].width);
        }
        this.frac += 0.5;
        if (this.frac > 1) this.frac = 1;
        else if (this.frac < 0) this.frac = 0;
        if (vertical) {
            value.y = (this.frac - 0.5) * (this.height - this.children[1].height);
            value.x = 0;
        } else {
            value.x = (this.frac - 0.5) * (this.width - this.children[1].width);
            value.y = 0;
        }
        this.children[1].translation = value;
        this.on_slide(this.min + (this.max - this.min) * this.frac, 0);
    }
    Browser.addRoute(ps2d, 'trackPoint_changed', obj, obj.set_trackpoint);

    obj.set_value = function (value) {
        if (this.children[2].isActive) return;

        value -= this.min;
        if (value < 0) value = 0;
        else if (value > this.max - this.min) value = this.max - this.min;
        if (this.max == this.min) value = 0;
        else value /= (this.max - this.min);

        this.frac = value;

        value -= 0.5;
        value *= (vertical ? this.height : this.width);

        if (vertical) {
            this.children[1].translation.y = value;
        } else {
            this.children[1].translation.x = value;
        }
    }
    obj.vertical = vertical;
    obj.set_size = function (w, h, cursor_w, cursor_h) {
        if (this.height && this.width) {
            if (this.vertical) {
                this.children[1].translation.y *= h;
                this.children[1].translation.y /= this.height;
            } else {
                this.children[1].translation.x *= w;
                this.children[1].translation.x /= this.width;
            }
        }

        this.width = w;
        this.height = h;
        this.children[0].set_size(w, h);

        if (typeof (cursor_w) == 'undefined') cursor_w = this.children[1].width;
        if (typeof (cursor_h) == 'undefined') cursor_h = this.children[1].height;
        this.children[1].set_size(cursor_w, cursor_h);

        if (this.vertical) {
            this.children[2].set_size(this.children[1].width, h);
        } else {
            this.children[2].set_size(w, this.children[1].height);
        }
    }
    obj.set_cursor_size = function (w, h) {
        this.children[1].set_size(w, h);
    }
    obj.min = 0;
    obj.max = 100;
    obj.frac = 0.0;
    obj.height = 0;
    obj.width = 0;
    obj.set_size(vertical ? 10 : 200, vertical ? 200 : 10, 10, 10);

    obj.set_trackpoint( new SFVec2f(0,0) );

    obj.move = function (x, y) {
        this.translation.x = x;
        this.translation.y = y;
    }
    obj.on_event = function (x, y) { return false; }

    gw_add_child(parent, obj);
    return obj;
}


function gw_new_text_edit(parent, text_data) {
    var obj = new SFNode('Transform2D');
    var rect, edit;
    setup_gw_object(obj, 'TextEdit');
    rect = gw_new_rectangle('edit');
    gw_add_child(obj, rect);
    gw_object_set_hitable(rect);
    rect.on_down = function (val) {
        if (val) scene.set_focus(this.parent.children[1]);
    }

    edit = gw_new_text(obj, text_data, 'edit')
    //    gw_add_child(obj, edit);
    obj.on_text = null;
    obj._text_edited = function (val) {
		if (this.nb_reset) this.nb_reset--;
		else if (this.on_text) this.on_text(val[0]);
    };
    Browser.addRoute(obj.children[1].children[0].children[0].geometry, 'string', obj, obj._text_edited);

    obj.set_size = function (w, h) {
        this.width = w;
        this.height = h;
        this.children[0].set_size(w, h);
        this.children[1].move(5 - w / 2, 0);
    };
    obj.on_event = function (x, y) { return false; }
    obj.nb_reset=0;
    obj.reset = function (use_pass) {
		this.nb_reset++;
		edit.set_label('');
		if (use_pass)
			edit.children[0].children[0].geometry.fontStyle = gwskin.get_font('editpw');
		else
			edit.children[0].children[0].geometry.fontStyle = gwskin.get_font('edit');
    }
    obj.edit = edit;
    gw_add_child(parent, obj);
    return obj;
}

function gw_new_text_area(parent, text_data, class_name) {
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'TextArea');

    obj.children[0] = new SFNode('Layout');
    obj.children[0].parent = obj;
    obj.children[0].wrap = true;
    obj.children[0].spacing = 1.1;
    obj.children[0].justify[0] = "JUSTIFY";
    obj.children[0].justify[1] = "FIRST";

	if (arguments.length==2) class_name = 'text';
    gw_new_text(obj.children[0], text_data, class_name);
    obj.set_size = function (width, height) {
        this.children[0].size.x = width;
        this.children[0].size.y = height;
        this.width = width;
        this.height = height;
    }
    obj.set_content = function (text) {
        this.children[0].children[0].set_label(text);
    }

    obj.on_event = function (x, y) { return false; }

    gw_add_child(parent, obj);
    return obj;
}



function gw_new_scrolltext_area(parent, text_data) {
    var obj = gw_new_text_area(parent, text_data);
    /*erase the resize routine*/
    obj._orig_set_size = obj.set_size;
    obj.set_size = function (width, height) {
        var bounds = this._bounds;
        this.width = width;
        this.height = Math.ceil(bounds.height);
        this._orig_set_size(width, height);
    }

    obj.set_content = function (text) {
        this.set_label(text);
    }
    return obj;
}

function gw_new_window_full(parent, offscreen, the_label, child_to_insert) {
    var wnd = gw_new_window(parent, offscreen, true);

    if (arguments.length <= 3) child_to_insert = null;
    if (child_to_insert) {
        gw_add_child(wnd, child_to_insert);
    }

    wnd.area = null;

    wnd.tools = gw_new_grid_container(wnd);
    wnd.tools._name = 'ToolBar';
    wnd._has_extra_tools = false;

    wnd.label = null;
    if (the_label) {
        wnd.label = gw_new_text(wnd.tools, the_label, 'window');
        wnd.label._name = 'Label';
    }

    wnd.set_label = function (label) {
        if (wnd.label) wnd.label.set_label(label);
    }


    wnd.add_tool = function (icon, name) {
        var tool;
        if (typeof gwskin.images[icon] != 'undefined') {
            tool = gw_new_icon(this.tools, icon);
        } else {
            tool = gw_new_icon_button(this.tools, icon, name, false, 'icon');
        }
        if (this.label) {
            this.tools.remove_child(this.label);
            this.tools.add_child(this.label);
        }
        tool.set_size(gwskin.default_icon_height, gwskin.default_icon_height);
        tool._name = name;
        tool.dlg = this;
        this._has_extra_tools = true;
        return tool;
    }


    var icon = wnd.add_tool('close');
    icon.on_click = function () {
        this.dlg.close();
    }
    wnd._has_extra_tools = false;

    wnd._pre_destroy = function () {
        if (typeof this.predestroy != 'undefined') this.predestroy();
        this.tools = null;
        this.label = null;
        this.area = null;
        this._wnd_close();
    }

    wnd.on_size = null;
    wnd.layout = function (width, height) {
        var i;
        var tools = this.tools.get_children();
        var tool_size = gwskin.default_icon_height;

        if (this.on_size) this.on_size(width, height - tool_size);

        if (this.area) {
            this.area.set_size(width, height - tool_size);
            this.area.move(0, (this.area.height - height) / 2);
        }
        for (i = 0; i < tools.length; i++) {
            tools[i].set_size(tool_size, tool_size);
        }

        if (this.label) {
            var textw = width;
            for (i = 0; i < tools.length; i++) {
                if ((tools[i] != this.label) && tools[i].visible)
                    textw -= tools[i].width;
            }
            textw -= tool_size;
            if (textw < 0) textw = 0;
            this.label.set_size(textw, tool_size);
            this.label.set_width(textw);
        }

        this.tools.move(0, (height - tool_size) / 2);
        this.tools.set_size(width, tool_size);
    }

    wnd.on_event = function (evt) {
        if ((evt.type == GF_EVENT_KEYUP) && (evt.keycode == gwskin.keys.close)) {
            this.close();
            return 1;
        }
        if ((evt.type == GF_EVENT_KEYUP) && (evt.keycode == gwskin.keys.validate)) {
			if (typeof this.on_validate == 'function') {
				this.on_validate();
				return 1;
			}
			return 0;
        }
        if (!this._has_extra_tools) {
            if (this.area) return this.area.on_event(evt);
            return false;
        }

        if (!this.area) {
            return this.tools.on_event(evt);
        }

        var is_left_or_right = (evt.type == GF_EVENT_KEYDOWN) && ((evt.keycode == 'Left') || (evt.keycode == 'Right')) ? 1 : 0;
        if (this._focus_is_tools) {
            if (this.tools.on_event(evt)) return 1;
            if (this.area.on_event(evt)) {
                //focus no longer on toolbar but on main content (means this is 'right'), switch order 
                if (is_left_or_right) {
                    this._focus_is_tools = false;
                }
                return 1;
            }
        } else {
            if (this.area.on_event(evt)) return 1;
            if (this.tools.on_event(evt)) {
                //focus no longer on main but on toolbar content (means this is 'left'), switch order 
                if (is_left_or_right) {
                    this._focus_is_tools = true;
                }
                return 1;
            }
        }
        return 0;
    }
    wnd._focus_is_tools = true;
    return wnd;
}

function gw_new_message(container, label, content) {
    var notif = gw_new_window_full(container, true, label);
    notif.area = gw_new_text_area(notif, content);
    notif.on_size = function (width, height) {
        this.area.set_size(width - 10, height);
    }
    notif.set_size(0.8 * gw_display_width, 120);

    if (arguments.length < 4) {
        notif.timer = gw_new_timer(false);
        notif.timer.wnd = notif;
        notif.timer.set_timeout(gwskin.default_message_timeout, false);
        notif.timer.start(0);
        notif.timer.on_active = function (val) {
            if (!val) this.wnd.close();
        }
        notif.show_effect = 'notif';
        notif._no_focus = true;
    }
    return notif;
}

function gw_new_confirm_wnd(container, label, confirm_yes, confirm_no) {
    if (arguments.length < 4) confirm_no = 'no';
    if (arguments.length < 3) confirm_yes = 'yes';

    var notif = gw_new_window_full(container, true, label);
    notif.area = gw_new_grid_container(notif);
    notif.area.spread_h = true;
    
    notif.on_confirm = null;
    notif.btn_yes = gw_new_button(notif.area, confirm_yes);
    notif.btn_yes.dlg = notif;
    notif.btn_yes.on_click = function () {
        if (this.dlg.on_confirm) this.dlg.on_confirm(true);
        this.dlg.close();
    }

    notif.btn_no = gw_new_button(notif.area, confirm_no);
    notif.btn_no.dlg = notif;
    notif.btn_no.on_click = function () {
        if (this.dlg.on_confirm) this.dlg.on_confirm(false);
        this.dlg.close();
    }
    notif.on_close = function () {
        if (this.on_confirm) this.on_confirm(false);
    }

    notif.on_display_size = function (width, height) {
        var w = 0.9*width;
        if (w>500) w=500;
        this.btn_yes.set_size(w / 3, gwskin.default_control_height);
        this.btn_no.set_size(w / 3, gwskin.default_control_height);
        this.set_size(w, 2 * gwskin.default_control_height);
    }
    notif.on_display_size(gw_display_width, gw_display_height);
    return notif;
}


function gw_guess_mime_icon(name)
{

    var ext = name.split('.').pop();
    var reg = new RegExp(' ' + ext + ' ', "gi");
    //check default extensions
    if (gwskin.mime_video_default_ext.match(reg)) return gwskin.images.mime_video;
    else if (gwskin.mime_audio_default_ext.match(reg)) return gwskin.images.mime_audio;
    else if (gwskin.mime_image_default_ext.match(reg)) return gwskin.images.mime_image;
    else if (gwskin.mime_model_default_ext.match(reg)) return gwskin.images.mime_model;

    var idx = 0;
    while (1) {
        var mime = scene.get_option('MimeTypes', idx);
        if (mime == null) break;
        idx++;
        var mime_ext = scene.get_option('MimeTypes', mime).split('"')[1];
        if (!mime_ext.match(reg)) continue;
                
        if (mime.indexOf('video') != -1) return gwskin.images.mime_video;
        else if (mime.indexOf('audio') != -1) return gwskin.images.mime_audio;
        else if (mime.indexOf('model') != -1) return gwskin.images.mime_model;
        else if (mime.indexOf('image') != -1) return gwskin.images.mime_image;
        else if (mime.indexOf('application') != -1) return gwskin.images.mime_model;

        break;
    }
    return gwskin.images.mime_generic;
}


function gw_new_file_dialog(container, label) {
    var dlg = gw_new_window_full(container, true, label);

    dlg.area = gw_new_grid_container(dlg);
    dlg.area.break_at_line = true;
	dlg.area.dlg = dlg;

    dlg.on_close = function () {
        if (this.do_sort_wnd) {
            this.do_sort_wnd.close();
        }
        if (this.on_browse) {
            this.on_browse(null, false);
        }
    }


	dlg._name_sort_only = false;
    dlg._sort_type = 0;
    dlg.do_sort = function (value) {
        this._page_idx = 0;
        this._max_page_idx = 0;

        switch (value) {
            case 0:
                this.area._all_children.sort(function (a, b) { var A = a.filename.toLowerCase(); var B = b.filename.toLowerCase(); if (A > B) { return 1; } else if (A < B) { return -1; } else return 0; });
                break;
            case 1:
                this.area._all_children.sort(function (b, a) { var A = a.filename.toLowerCase(); var B = b.filename.toLowerCase(); if (A > B) { return 1; } else if (A < B) { return -1; } else return 0; });
                break;
            case 2:
                this.area._all_children.sort(function (a, b) { return a.size - b.size });
                break;
            case 3:
                this.area._all_children.sort(function (a, b) { return b.size - a.size });
                break;
            case 4:
                this.area._all_children.sort(function (a, b) { return a.date - b.date });
                break;
            case 5:
                this.area._all_children.sort(function (a, b) { return b.date - a.date });
                break;
        }
        this.area.layout();
    }

    dlg.go_up = dlg.add_tool('up');
    dlg.go_up.on_click = function () {
        this.dlg._browse(null, true);
    }

    dlg.go_prev = dlg.add_tool('previous');
    dlg.go_prev.on_click = function () {
        this.dlg.area._move_page(-1);
    }

    dlg.go_next = dlg.add_tool('next');
    dlg.go_next.on_click = function () {
        this.dlg.area._move_page(1);
    }

    dlg.go_root = dlg.add_tool('device');
    dlg.go_root.on_click = function () {
        this.dlg._browse('/');
    }

    dlg.sort = dlg.add_tool('sort');
    dlg.sort_wnd = null;
    dlg.sort.on_click = function() {
		
		if (this.dlg._name_sort_only) {
			var wnd = this.dlg;
			if (wnd._sort_type==0) wnd._sort_type = 1;
			else wnd._sort_type = 0;
			wnd.do_sort(wnd._sort_type);
			return;
		}
		
        if (this.dlg.sort_wnd) {
            this.dlg.sort_wnd.close();
            this.dlg.sort_wnd = null;
            return;
        }
        var wnd = gw_new_popup(this.dlg.sort, 'down');
        this.dlg.sort_wnd = wnd;
            
        wnd.dlg = this.dlg;
        wnd.on_close = function() {
            this.dlg.sort_wnd = null;
        }
        wnd.add_menu_item('by Name',  function () { 
            var wnd = this.dlg;
            if (wnd._sort_type==0) wnd._sort_type = 1;
            else wnd._sort_type = 0;
            wnd.do_sort(wnd._sort_type);
        } );

        wnd.add_menu_item('by Size',  function () { 
            var wnd = this.dlg;
            if (wnd._sort_type==2) wnd._sort_type = 3;
            else wnd._sort_type = 2;
            wnd.do_sort(wnd._sort_type);
        } );

        wnd.add_menu_item('by Date',  function () { 
            var wnd = this.dlg;
            if (wnd._sort_type==4) wnd._sort_type = 5;
            else wnd._sort_type = 4;
            wnd.do_sort(wnd._sort_type);
        } );

        wnd.on_display_size(gw_display_width, gw_display_height);
        wnd.show();
    }

    dlg.predestroy = function () {
        this.go_up = null;
        this.go_next = null;
        this.go_prev = null;
        this.go_root = null;
        this.area.dlg = null;
    }

    dlg.directory = '';
    dlg.show_directory = false;
    dlg.filter = '*';

    dlg._on_dir_browse = function () {
		if (this.path) {
			this.dlg._browse(this.path, false);
		} else {
			this.dlg._browse(this.dlg.path + this.filename, false);
		}
    }
    dlg._on_file_select = function () {
        if (this.dlg.on_browse) {
			if (this.path) {
				this.dlg.on_browse(this.path, null);
			} else {
				this.dlg.on_browse(this.dlg.path + this.filename, this.dlg.directory);
			}
        }
        this.dlg.close();
    }

    dlg.area.on_page_changed = function () {
        if (this.is_first_page()) this.dlg.go_prev.disable();
        else this.dlg.go_prev.enable();

        if (this.is_last_page()) this.dlg.go_next.disable();
        else this.dlg.go_next.enable();

        this.dlg.tools.layout(this.dlg.tools.width, this.dlg.tools.height);
    }

    dlg.on_long_click = null;

    dlg.browse = function (dir) {
		if (typeof dir == 'string') this.directory = dir;
		
        this._browse(dir, false);
    }
    dlg._browse = function (dir, up) {
        var w, h, i, y;
        var filelist;
        var is_listing = ((dir==null) || (typeof dir == 'string')) ? false : true;

        if (is_listing) {
            filelist = dir;
            this.path = null;
        } else {
            if (dir) this.directory = dir;
            if (up && !this.path) up = false;
            filelist = Sys.enum_directory(this.directory, this.filter, up);

            if (filelist.length) {
                let dir_name = filelist[0].path;
                dir_name = dir_name.substr(0, dir_name.lastIndexOf("/"));
                dir_name = dir_name.split('/').pop();
                if (dir_name.length == 0) dir_name = "/";

                this.directory = filelist[0].path;
                this.set_label(dir_name);
            } else {
                this.set_label('');
            }
            this.path = filelist.length ? filelist[0].path : '';
        }

        this.area.reset_children();

        for (i = 0; i < filelist.length; i++) {
			var f_path, f_name;
            if (!is_listing && (filelist[i].hidden || filelist[i].system)) continue;

			var icon_name = gwskin.images.mime_generic;
			if (is_listing && typeof filelist[i].icon == 'string') {
				if (filelist[i].icon != '') {
					icon_name = filelist[i].icon;
				}
			}

			if (is_listing && typeof filelist[i] == 'string') {
				f_path = f_name = filelist[i];
				this._name_sort_only = true;
			} else {
				f_path = filelist[i].path;
				f_name = filelist[i].name;
			}
			
            if ( (!is_listing || (typeof filelist[i].directory == 'boolean'))  && (filelist[i].directory || (filelist[i].name.indexOf('.') < 0))) {
                if (filelist[i].drive) icon_name = gwskin.images.drive;
                else if (filelist[i].directory) icon_name = gwskin.images.folder;
                else icon_name = gwskin.images.mime_generic;
            } else {
                icon_name = gw_guess_mime_icon(is_listing ? f_path : f_name);
            }

            var item = gw_new_icon_button(this.area, icon_name, f_name, true, 'listitem');
            item.dlg = this;
            item.filename = f_name;
			item.directory = typeof filelist[i].directory != 'undefined' ? filelist[i].directory : false;
            item.set_size(this.width, gwskin.default_control_height);
            item.path = is_listing ? f_path : null;
            item.size = typeof filelist[i].size != 'undefined' ? filelist[i].size : 0;
            item.date = typeof filelist[i].last_modified != 'undefined' ? filelist[i].last_modified : 0;

            if (item.directory) {
                item.on_click = this._on_dir_browse;
            } else {
                item.on_click = this._on_file_select;
            }
            item.on_long_click = function () {
                if (this.dlg.on_long_click) {
					var path = this.path ? this.path : (this.dlg.directory + this.filename);
					this.dlg.on_long_click(this.filename, path, this.dlg.directory);
				}
            }

        }
        this.do_sort(this._sort_type);

        if (this.directory == '/') this.go_up.disable();
        else this.go_up.enable();

        this.tools.layout(this.tools.width, this.tools.height);
    }

    dlg.on_size = function (width, height) {
        var __children = this.area.get_children();
        for (var i = 0; i < __children.length; i++) {
            __children[i].set_size(width, gwskin.default_control_height);
        }
        this.area.set_size(width, height);
    }

    return dlg;
}


function gw_new_plotter(parent) {
    var touch;
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Plotter');

    obj.children[0] = new SFNode('Layer2D');

    var class_name = 'plot';

    obj.children[0].children[0] = gw_new_rectangle(class_name, 'normal');

    obj.set_size = function (width, height) {
        this.children[0].size.x = width;
        this.children[0].size.y = height;
        this.children[0].children[0].set_size(width, height);

        var w = 1;
        var h = 1;

        for (var i = 0; i < this.series.length; i++) {
            var s = this.series[i];
            s.scale = new SFVec2f(width - this.label_width, height);
            s.but.set_size(2*this.label_width, 0.9*gwskin.default_text_font_size);
            s.but.set_font_size(0.9 * gwskin.default_text_font_size);
            s.translation.x = -this.label_width / 2;
        }
        this.width = width;
        this.height = height;
    }

    obj.label_width = 120;

    obj.series = [];
    obj.add_serie = function (legend, units, r, g, b) {
        var s = gw_new_curve2d('plot');
        s.set_color(r, g, b);
		s.set_line_width(gwskin.default_icon_height/10);
        s.dlg = this;
        s.legend = legend;
        s.units = units;
        s.max_y = 0;
        s.nb_refresh = 0;

        s.refresh_serie = function (serie, name_x, name_y, nb_x, factor) {
            var s = serie[0];
            var count = serie.length;

            var first_x = s[name_x];
 
            this.nb_refresh += 1;
            if (this.nb_refresh == 50) {
                this.max_y = 0;
                this.nb_refresh += 1;
            }
            var max_y = this.max_y;
 

            if (!factor) factor = 1;
            for (var i = 0; i < serie.length; i++) {
                var s = serie[i];
                if (max_y < s[name_y]) max_y = s[name_y];
            }
            s = serie[0];

            this.reset();
            for (var i = 0; i < count; i++) {
                s = serie[i];
                //get x and y between -0.5 and 0.5, and scale to width
                var x = s[name_x] - first_x;
                x /= nb_x;
                x -= 0.5;
                //x *= 0.9;

                var y = s[name_y];
                if (max_y) y /= max_y;
                y = -0.9 / 2 + 0.9 * y / factor;

                if (i) {
                    this.add_line_to(x, y);
                } else {
                    this.add_move_to(x, y);
                }

                if (i + 1 == count) {
                    this.but.move(0.5 * this.dlg.width - this.but.width / 2, (-0.9 / 2 + 0.9 / factor) * this.dlg.height);
                    this.but.set_label('' + this.legend + ':' + s[name_y] + ' ' + units);
                    this.but.show();
                }
            }
        }
        this.children[0].children[this.children[0].children.length] = s;
        this.series.push(s);

        s.scale.x = this.width;
        s.scale.y = this.height;


        s.but = gw_new_text(this, '' + legend, 'custom');
        s.but.set_align('END');
        s.but.set_color(r, g, b);
        s.but.set_size(2*this.label_width, gwskin.default_text_font_size);
        s.but.hide();
        return s;

    }


    gw_add_child(parent, obj);
    return obj;
}

function gw_new_popup(anchor, type)
{
    var popup = gw_new_window(null, true, false, 'popup');
    popup.area = gw_new_grid_container(popup);
    popup.anchor = anchor;
    popup.type = (type=='up') ? 0 : 1;
    
    popup._nb_over = 0;
    popup.add_menu_item = function (label, callback) {
        var item = gw_new_button(this.area, label, 'window');
        item.wnd = this;
        item.on_click = function () {
            callback.call(this.wnd);
            this.wnd.close();
        }
        item.on_over_ex = function (value) {
            this.wnd.update_visibility();
        }
        item.new_over_handler(item.on_over_ex);

        item.set_corners(false, false, false, false);
        return item;
    }
    popup.reposition = function() {
        var pos = gw_get_adjusted_abs_pos(this.anchor, this.width, this.height, this.type);
        this.move(pos.x, pos.y);
    }

    popup.update_visibility = function() {
        var nb_over = 0;
        var children = this.area.get_children();
        for (var i=0; i<children.length; i++) {
            if (children[i]._over) nb_over++;
        }
        if (!nb_over) this.close();
    }

    
    popup.on_display_size = function (w, h) {
        var children = this.area.get_children();
        var max_s = 0;
        for (var i=0; i<children.length; i++) {
          var s = children[i].get_label().length;
          if (s>max_s) max_s = s;
        }
		max_s *= 0.66;
        for (var i=0; i<children.length; i++) {
            children[i].set_size(max_s * gwskin.default_text_font_size, gwskin.default_icon_height);
        }
        this.area.set_size(max_s * gwskin.default_text_font_size, children.length * gwskin.default_icon_height);
        this.set_size(max_s * gwskin.default_text_font_size, children.length * gwskin.default_icon_height);
        this.reposition();
    }
    gw_ui_root.has_popup = true;
          
     
    popup.set_alpha(0.9);
    return popup;
}

