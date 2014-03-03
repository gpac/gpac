/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////

function gw_new_timer(progressive)
{
	var obj = new SFNode('TimeSensor');

	obj.set_timeout = function(time, loop) {
   this.cycleInterval = time ? time : 1;
   this.loop = loop;
  }
  this.startTime = -1;
	obj.start = function(when) {
		this.startTime = when + this.getTime();
	}
	obj.stop = function(when) {
		this.stopTime = when + this.getTime();
	}
	obj.on_event = function(val){};
	obj._event = function(val) {
		this.on_event( Math.floor(val/this.cycleInterval) );
	}
	Browser.addRoute(obj, 'cycleTime', obj, obj._event);

  if (progressive) {
 	 obj.on_fraction = function(val){}
	 obj._fraction = function(frac) {
		this.on_fraction(frac);
	 }
	 Browser.addRoute(obj, 'fraction_changed', obj, obj._fraction);
  }
  obj.on_active = function(val) {}
  obj._active = function(val) {
   this.on_active(val);
  }
  Browser.addRoute(obj, 'isActive', obj, obj._active);
	return obj;
}

function setup_gw_object(obj, name)
{
 obj._name = name;
 obj.set_color = function() {}
 obj.set_size = function(width, height) {
  this.width = width;
  this.height = height;
 }
 obj.hide = function() { 
  this.scale.x = this.scale.y = 0; 
  this.visible = false;
  if (arguments.length==1) {
   arguments[0].apply(this);
  } 
 }
 obj.show = function() { this.scale.x = this.scale.y = 1; this.visible = true;}
 obj.visible = true;
 obj.set_size = function(width, height) {
  this.width = width;
  this.height = height;
 }
 obj.move = function(x, y) {
  this.translation.x = x;
  this.translation.y = y;
 }
 obj.width = obj.height = 0;

 obj.close = function() {
  if (typeof this._pre_destroy != 'undefined') this._pre_destroy();
  gw_detach_child(this);
 };
}
  
function gw_object_set_dragable(obj)
{
  if (typeof (obj.on_drag) != 'undefined') return;
  
  var ps2d = new SFNode('PlaneSensor2D');
  obj._ps2d_idx = obj.children.length;
  obj.children[obj.children.length] = ps2d;
  ps2d.maxPosition.x = ps2d.maxPosition.y = -1;
  ps2d.autoOffset = FALSE;
  obj._drag = false;
  obj.begin_drag = null;
  obj.end_drag = null;
  obj._drag_active = function(val) {
   if (val) {
    this.startx = this.translation.x;
    this.starty = this.translation.y;
    if (this.begin_drag) this.begin_drag();
   } else {
    if (this.end_drag) this.end_drag();
   }
  }
  obj.on_drag = null;
  obj._on_drag = function(val) {
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

function gw_object_set_hitable(obj)
{
  if (typeof (obj._active) != 'undefined') return;
  
  var ts = new SFNode('TouchSensor');
  obj._ts_idx = obj.children.length;
  obj.children[obj.children.length] = ts;
  obj._drag = false;
  obj._over = false;
  obj.on_over = null;
  obj._on_over = function(val) {
   this._over = val;
   if (this.on_over) this.on_over(val);
  }
  obj.last_click = 0;
  obj.on_click = null;
  obj.on_down = null;
  obj._on_active = function(val, timestamp) {
   if (!this._over) return;

   if (this.on_down) this.on_down(val);

   if (!val) {
    if (this._drag) {
      this._drag = false;
      return;
    }
    
    if (this.last_click && (timestamp - this.last_click<gwskin.double_click_delay) ) {
      this.last_click = timestamp;
      if (this.on_double_click) {
       this.on_double_click();
       return;
      }
    }
    
    this.last_click = timestamp;
    if(this.on_click && this._over) this.on_click();
   }
  }
  Browser.addRoute(ts, 'isOver', obj, obj._on_over); 
  Browser.addRoute(ts, 'isActive', obj, obj._on_active); 
}


function gw_new_appearance(r, g, b)
{
    var appearance = new SFNode('Appearance');
    appearance.material = new SFNode('Material2D');
    appearance.material.filled = TRUE;
    appearance.material.emissiveColor = new SFColor(r, g, b);
    return appearance;
}

function gw_make_gradient(type, keys, colors)
{
 var i;
 var obj = new SFNode(type=='radial' ? 'RadialGradient' : 'LinearGradient');
 for (i=0; i<keys.length; i++) {
  obj.key[i] = keys[i];
 }
 for (i=0; i<colors.length / 3; i++) {
  obj.keyValue[i] = new SFColor(colors[3*i], colors[3*i+1], colors[3*i+2]);
 }
 if (type=='vertical') {
  obj.endPoint.x = 0;
  obj.endPoint.y = 1;
 }
 return obj;
}

function gw_new_fontstyle(size, align)
{
    var fs = new SFNode('FontStyle');
    fs.size = size;
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
    fs.justify[1] = 'FIRST';
    return fs;
}

gw_resources_bank = [];

function gw_load_resource(url)
{
  var new_res;
  if (gwskin.use_resource_bank) {
   for (var i in gw_resources_bank) {
    if (gw_resources_bank[i].url == url) {
      gw_resources_bank[i].nb_instances++;
      alert('Found existing resource for url '+url);
      return gw_resources_bank[i];
    }
  }
 }
  if (gwskin.use_offscreen_icons) {
   new_res = new SFNode('OffscreenGroup');
   new_res.offscreen_mode = 2;
   new_res.children[0] = new SFNode('Inline');
   new_res.children[0].url[0] = url;
  } else {
   new_res = new SFNode('Inline');
   new_res.url[0] = url;
  }
  new_res.nb_instances = 1;
  if (gwskin.use_resource_bank) {
   new_res.url = url;
   gw_resources_bank.push(new_res);
   alert('Created resource for url '+url + ' nb loaded resources '+gw_resources_bank.length);
  }
  return new_res;
}

function gw_unload_resource(res)
{
 if (!gwskin.use_resource_bank) return;

 if (!res.nb_instances) return;
 res.nb_instances--;
 if (res.nb_instances) return;
 
 for (var i in gw_resources_bank) {
  if (gw_resources_bank[i] == res) {
   alert('Unloading resource '+ res  + ' nb loaded resources '+gw_resources_bank.length);
   gw_resources_bank.splice(i, 1);
   return;
  }
 } 
 alert('Unloading resource for url '+res.children[0].url[0] + ' not found in resource bank');
}


function gw_window_show_hide()
{
 if (typeof this.timer == 'undefined') {
   this.timer = gw_new_timer(1);
   this.timer.set_timeout(0.25, false);
   this.timer.on_fraction = function(val) {
    if (!this.wnd) return;
    if (this.wnd.visible) {
      this.wnd.scale.x = 1-val;
      this.wnd.scale.y = 1-val;
      this.wnd.set_alpha( (1-val) * this.wnd.alpha);
    } else {
      this.wnd.scale.x = val;
      this.wnd.scale.y = val;
      this.wnd.set_alpha(val * this.wnd.alpha);
    }
   }
   this.timer.on_active = function(val) {
    var fun;
    if (val || !this.wnd) return;
    var wnd = this.wnd;
    this.wnd = null;
    wnd.visible = !wnd.visible;
    wnd.scale.x = wnd.visible ? 1 : 0;
    wnd.scale.y = wnd.visible ? 1 : 0;
    wnd.set_alpha(wnd.alpha);
    fun = this.call_on_end;
    this.call_on_end = null;
    if (fun) {
      fun.apply(wnd);
    }
   }
 }
 /*not done yet! This can happen when the function is called faster than the animation duration*/
 if (this.timer.wnd) return;
 
 this.alpha = this.get_alpha();
 this.set_alpha(1.0);
 this.timer.wnd = this;
 this.timer.start(0);
 this.timer.call_on_end = null;
 if (arguments.length) {
  this.timer.call_on_end = arguments[0];
 }
}

function gw_window_show()
{
 if (this.visible) return;
 gw_window_show_hide.call(this);
}

function gw_window_hide()
{
 if (!this.visible) return;
 gw_window_show_hide.apply(this, arguments);
}

gwskin = new Object();
gwskin.back_color = new SFColor(0.2, 0.2, 0.2);
gwskin.pointing_device = true;
gwskin.long_click_delay = 0.5;
gwskin.double_click_delay = 0.5;
gwskin.use_resource_bank = false;
gwskin.use_offscreen_icons = false;
gwskin.default_icon_size = 32;
gwskin.appearance_transparent = gw_new_appearance(0, 0, 0);
gwskin.appearance_transparent.material.transparency = 1;
gwskin.appearance_transparent.skin = true;

gwskin.tooltip_callback = null;

gwskin.default_text_size = 18;

gwskin.control = new Object();
gwskin.control.normal = gw_new_appearance(0.6, 0.6, 0.6);
gwskin.control.normal.skin = true;
if (gwskin.pointing_device) {
 gwskin.control.over = gw_new_appearance(0.7, 0.7, 0.8);
 gwskin.control.over.material.transparency = 0.5;
 gwskin.control.over.skin = true;
} else {
 gwskin.control.over = null;
}

gwskin.button = new Object();
gwskin.button.width = 48;
gwskin.button.height = 48;
gwskin.button.font_size = 12;
gwskin.button.normal = gw_new_appearance(0.7, 0.7, 0.8);
gwskin.button.normal.skin = true;
if (gwskin.pointing_device) {
 gwskin.button.over = gw_new_appearance(0.7, 0.7, 0.8);
 gwskin.button.over.material.transparency = 0.4;
 gwskin.button.over.skin = true;
} else {
 gwskin.button.over = null;
}
gwskin.button.down = gw_new_appearance(0.3, 0.3, 0.4);
gwskin.button.down.material.transparency = 0.3;
gwskin.button.down.skin = true;
gwskin.button.text = gw_new_appearance(0, 0, 0);
gwskin.button.text.skin = true;
gwskin.button.font = gw_new_fontstyle(gwskin.button.font_size, 1);
gwskin.button.font.skin = true;


gwskin.osdbutton = new Object();
gwskin.osdbutton.width = 48;
gwskin.osdbutton.height = 48;
gwskin.osdbutton.font_size = 12;
gwskin.osdbutton.normal = gw_new_appearance(0.7, 0.7, 0.8);
gwskin.osdbutton.normal.skin = true;
if (gwskin.pointing_device) {
 gwskin.osdbutton.over = gw_new_appearance(0.7, 0.7, 0.8);
 gwskin.osdbutton.over.material.transparency = 0.4;
 gwskin.osdbutton.over.skin = true;
} else {
 gwskin.button.over = null;
}
gwskin.osdbutton.down = gw_new_appearance(0.3, 0.3, 0.4);
gwskin.osdbutton.down.material.transparency = 0.3;
gwskin.osdbutton.down.skin = true;
gwskin.osdbutton.text = gw_new_appearance(1, 1, 1);
gwskin.osdbutton.text.skin = true;
gwskin.osdbutton.font = gw_new_fontstyle(gwskin.button.font_size, 1);
gwskin.osdbutton.font.skin = true;

gwskin.listitem = new Object();
gwskin.listitem.width = 48;
gwskin.listitem.height = 48;
gwskin.listitem.font_size = 12;
gwskin.listitem.normal = gwskin.button.normal;
gwskin.listitem.over = gwskin.button.over;
gwskin.listitem.down = gwskin.button.down;
gwskin.listitem.text = gwskin.button.text;
gwskin.listitem.font = gw_new_fontstyle(gwskin.listitem.font_size, 0);
gwskin.listitem.font.skin = true;

gwskin.label = new Object();
gwskin.label.font_size = 18;
gwskin.label.text = gw_new_appearance(0, 0, 0);
gwskin.label.text.skin = true;
gwskin.label.font = gw_new_fontstyle(gwskin.label.font_size, 1);
gwskin.label.font.skin = true;

gwskin.text = new Object();
gwskin.text.font_size = 14;
gwskin.text.text = gw_new_appearance(0, 0, 0);
gwskin.text.text.skin = true;
gwskin.text.font = gw_new_fontstyle(gwskin.text.font_size, 0);
gwskin.text.font.skin = true;


gwskin.edit = new Object();
gwskin.edit.normal = gw_new_appearance(1, 1, 1);
gwskin.edit.font_size = 18;
gwskin.edit.text = gwskin.label.text;
gwskin.edit.font = gw_new_fontstyle(gwskin.edit.font_size, 0);
gwskin.edit.font.style += ' SIMPLE_EDIT';
gwskin.edit.font.skin = true;

gwskin.window = new Object();
gwskin.window.font_size = 14;
gwskin.window.width = 320;
gwskin.window.height = 240;
gwskin.window.normal = gw_new_appearance(0.6, 0.6, 0.6);
//gwskin.window.normal.texture = gw_make_gradient('vertical', [0, 0.85, 1], [0.6, 0.6, 0.6, 1, 1, 1, 0.6, 0.6, 0.6]);
gwskin.window.normal.skin = true;
gwskin.window.text = gwskin.label.text;
gwskin.window.font = gw_new_fontstyle(gwskin.window.font_size, 1);
gwskin.window.font.skin = true;
gwskin.window.hide = gw_window_hide;
gwskin.window.show = gw_window_show;

gwskin.offscreen = new Object();
gwskin.offscreen.font_size = 14;
gwskin.offscreen.width = 100;
gwskin.offscreen.height = 100;
gwskin.offscreen.normal = gwskin.appearance_transparent;
gwskin.offscreen.hide = gwskin.window.hide;
gwskin.offscreen.show = gwskin.window.show;


gwskin.osdwindow = new Object();
gwskin.osdwindow.font_size = 14;
gwskin.osdwindow.width = 320;
gwskin.osdwindow.height = 240;
gwskin.osdwindow.normal = gw_new_appearance(0.6, 0.6, 0.6);
gwskin.osdwindow.normal.texture = gw_make_gradient('vertical', [0, 0.15, 0.85, 1], [0.6, 0.6, 0.6, 0, 0, 0, 0, 0, 0, 0.6, 0.6, 0.6]);
gwskin.osdwindow.normal.skin = true;
gwskin.osdwindow.text =  gw_new_appearance(1, 1, 1);
gwskin.osdwindow.text.skin = true;
gwskin.osdwindow.font = gw_new_fontstyle(gwskin.osdwindow.font_size, 1);
gwskin.osdwindow.font.justify[1] = 'MIDDLE';
gwskin.osdwindow.font.skin = true;
gwskin.osdwindow.hide = gw_window_hide;
gwskin.osdwindow.show = gw_window_show;
gwskin.osdwindow.over = gwskin.button.over;
gwskin.osdwindow.down = gwskin.button.down;

gwskin.progress = new Object();
gwskin.progress.width = 200;
gwskin.progress.height = 48;
gwskin.progress.normal = gw_new_appearance(0.6, 0.6, 0.6);
gwskin.progress.normal.texture = gw_make_gradient('vertical', [0, 0.5, 1], [0.6, 0.6, 0.6, 0, 0, 0, 0.6, 0.6, 0.6]);
gwskin.progress.normal.skin = true;
gwskin.progress.over = gw_new_appearance(0.7, 0.7, 0.8);
gwskin.progress.over.texture = gw_make_gradient('vertical', [0, 0.5, 1], [0.6, 0.6, 0.6, 0, 0, 1, 0.6, 0.6, 0.6]);
gwskin.progress.over.skin = true;
gwskin.progress.down = gw_new_appearance(0.7, 0.7, 0.8);
gwskin.progress.down.texture = gw_make_gradient('vertical', [0, 0.5, 1], [0.6, 0.6, 0.6, 1, 1, 1, 0.6, 0.6, 0.6]);
gwskin.progress.down.skin = true;
gwskin.progress.text = null;
gwskin.progress.font = null;

gwskin.icon = new Object();
gwskin.icon.normal = gwskin.button.normal;
gwskin.icon.over = gwskin.button.over;
gwskin.icon.down = gwskin.button.down;
gwskin.icon.text = null;
gwskin.icon.font = null;
gwskin.icon.width = 32;
gwskin.icon.height = 32;

gwskin.images = new Object();
gwskin.images.cancel = 'icons/emblem-unreadable.svg';
gwskin.images.previous = 'icons/go-previous.svg';
gwskin.images.next = 'icons/go-next.svg';
gwskin.images.up = 'icons/go-up.svg';
gwskin.images.scan_directory = 'icons/emblem-symbolic-link.svg';
gwskin.images.folder = 'icons/folder.svg';
gwskin.images.mime_generic = 'icons/applications-multimedia.svg';
gwskin.images.trash = 'icons/user-trash.svg';
gwskin.images.add = 'icons/list-add.svg';
gwskin.images.remove = 'icons/list-remove.svg';
gwskin.images.remote_display = 'icons/video-display.svg';
gwskin.images.information = 'icons/dialog-information.svg';
gwskin.images.resize = 'icons/media-record.svg';

gwskin.labels = new Object();
gwskin.labels.file_open = 'Open';
gwskin.labels.close = 'Close';
gwskin.labels.cancel = 'Cancel';
gwskin.labels.up = 'Up';
gwskin.labels.down = 'Down';
gwskin.labels.left = 'Left';
gwskin.labels.right = 'Right';
gwskin.labels.scan_directory = 'Select directory';
gwskin.labels.previous = 'Previous';
gwskin.labels.next = 'Next';
gwskin.labels.trash = 'Delete';
gwskin.labels.remote_display = 'Push to';
gwskin.labels.information = 'Information';
gwskin.labels.resize = 'Resize';

gwskin.keys = new Object();
gwskin.keys.close = 'Enter';


function gw_add_child(container, child)
{
 if (container ==null) return;
 if (typeof (container.add_child) != 'undefined') {
  container.add_child(child);
 } else {
  container.children[container.children.length] = child;
 }
 child.parent = container;
}

function gw_detach_child(child)
{
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
}

function gw_close_child_list(list)
{
  var count = list.length;
 if (!count) return;
 for (var i=0;i<count; i++) {
  var child = list[i];
  if (typeof child.close != 'undefined') child.close();
  else gw_detach_child(child);
    
  if (list.length < count) {
   i-= (count - list.length);
   count = list.length;
  }
 }
}

gwskin.get_style = function(class_name, style_name)
{
 if (arguments.length==1) style_name = 'normal';
 if (style_name=='invisible') return gwskin.appearance_transparent;
 
 if (class_name=='null') return null;
 if (class_name=='self') return gw_new_appearance(0.6, 0.6, 0.6);
 if (typeof gwskin[class_name] == 'undefined') {
  alert('Unknown class name ' + class_name);
  return null;
 }
 if (typeof gwskin[class_name][style_name] != 'undefined') 
  return gwskin[class_name][style_name];
  
 alert('Unknown style '+style_name + ' in class ' + class_name);
 return null;
}

gwskin.get_font = function(class_name)
{
 if (class_name=='null') return null;
 if (class_name=='self') return gw_new_fontstyle(gwskin.default_text_size, 1);

 if (typeof gwskin[class_name] == 'undefined') {
  alert('Unknown class name ' + class_name);
  return null;
 }
 if (typeof gwskin[class_name].font != 'undefined') 
  return gwskin[class_name].font;
  
 alert('No font style in class ' + class_name);
 return null;
}

function gw_new_container()
{
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'Container');
  obj.push_to_top = function(wnd) {
   this.removeChildren[0] = wnd;
   this.addChildren[0] = wnd;
  }
  return obj;
}
  

function gw_new_rectangle(class_name, style)
{
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'Rectangle');
  var shape = new SFNode('Shape');
  obj.children[0] = shape;
  if (arguments.length==1) style='normal';

  shape.appearance = gwskin.get_style(class_name, style);
 
  if (shape.appearance && (typeof (shape.appearance.skin) == 'undefined')) {
   obj.set_alpha = function(alpha) {
    this.children[0].appearance.material.transparency = 1-alpha;
   }
   obj.get_alpha = function() {
    return 1 - this.children[0].appearance.material.transparency;
   } 
   obj.set_fill_color = function(r, g, b) {
    this.children[0].appearance.material.emissiveColor.r = r;
    this.children[0].appearance.material.emissiveColor.g = g;
    this.children[0].appearance.material.emissiveColor.b = b;
   }
   obj.set_strike_color = function(r, g, b) {
    if (!this.children[0].appearance.material.lineProps) this.children[0].appearance.material.lineProps = new SFNode('LineProperties');
    this.children[0].appearance.material.lineProps.lineColor.r = r;
    this.children[0].appearance.material.lineProps.lineColor.g = g;
    this.children[0].appearance.material.lineProps.lineColor.b = b;
   }
   obj.set_line_width = function(width) {
    if (!this.children[0].appearance.material.lineProps) this.children[0].appearance.material.lineProps = new SFNode('LineProperties');
    this.children[0].appearance.material.lineProps.width = width;
   }
  } else {
   obj.set_alpha = function(alpha) {}
   obj.set_fill_color = function(r, g, b) {}
   obj.set_strike_color = function(r, g, b) {}
   obj.set_line_width = function(width) {}
   obj.get_alpha = function() { return 1; }
  }
  
  obj.set_style = function(classname, style) {
   var app = gwskin.get_style(classname, style);
   if (app) this.children[0].appearance = app;
  }


  shape.geometry = new SFNode('Curve2D');
  shape.geometry.point = new SFNode('Coordinate2D');

  obj.corner_tl = true;
  obj.corner_tr = true;
  obj.corner_bl = true;
  obj.corner_br = true;
  obj.set_corners = function(tl, tr, br, bl) {
    this.corner_tl = tl;
    this.corner_tr = tr;
    this.corner_bl = bl;
    this.corner_br = br;
  }

  obj.set_size = function(w, h) {
   var hw, hh, rx_bl, ry_bl, rx_br, ry_br, rx_tl, ry_tl, rx_tr, ry_tr, rx, ry;
   var temp;
   hw = w/2;
   hh = h/2;
   this.width = w;
   this.height = h;
  temp = this.children[0].geometry.type;
  temp[0] = 7;
  temp[1] = 1;
  temp[2] = 7;
  temp[3] = 1;
  temp[4] = 7;
  temp[5] = 1;
  temp[6] = 7;
  temp[7] = 6;/*close*/

   /*compute default rx/ry*/   
   ry = rx = 10;
   if (rx>=hw) rx=hw;
   if (ry>=hh) ry=hh;
   rx_bl = rx_br = rx_tl = rx_tr = rx;
   ry_bl = ry_br = ry_tl = ry_tr = ry;
   if (!this.corner_tl) rx_tl = ry_tl = 0;
   if (!this.corner_tr) rx_tr = ry_tr = 0;
   if (!this.corner_bl) rx_bl = ry_bl = 0;
   if (!this.corner_br) rx_br = ry_br = 0;

   temp = this.children[0].geometry.point.point;
   temp[0] = new SFVec2f(hw-rx_tr, hh);
   temp[1] = new SFVec2f(hw, hh);/*bezier ctrl point or line-to*/
   temp[2] = new SFVec2f(hw, hh-ry_tr);
   temp[3] = new SFVec2f(hw, -hh+ry_br);
   temp[4] = new SFVec2f(hw, -hh);/*bezier control point*/
   temp[5] = new SFVec2f(hw-rx_br, -hh);
   temp[6] = new SFVec2f(-hw+rx_bl, -hh);
   temp[7] = new SFVec2f(-hw, -hh);/*bezier control point*/
   temp[8] = new SFVec2f(-hw, -hh+ry_bl);
   temp[9] = new SFVec2f(-hw, hh-ry_tl);
   temp[10] = new SFVec2f(-hw, hh);/*bezier control point*/
   temp[11] = new SFVec2f(-hw+rx_tl, hh);
  }
  return obj;
}

function gw_new_text(parent, label, class_name)
{
    var obj = new SFNode('Transform2D');
    setup_gw_object(obj, 'Text');
    gw_add_child(parent, obj);
    obj.children[0] = new SFNode('Transform2D');
    obj.children[0].children[0] = new SFNode('Shape');
    obj.children[0].children[0].appearance = gwskin.get_style(class_name, 'text');

    obj.children[0].children[0].geometry = new SFNode('Text');
    obj.children[0].children[0].geometry.fontStyle = gwskin.get_font(class_name);


    obj.set_label = function(value) {
     this.children[0].children[0].geometry.string.length = 0;
     this.children[0].children[0].geometry.string[0] = value;
    }

    obj.set_labels = function() {
     this.children[0].children[0].geometry.string.length = 0;
     for (var i=0; i<arguments.length; i++) {
       this.children[0].children[0].geometry.string[i] = '' + arguments[i];
     }
    }
    obj.get_label = function() {
     return this.children[0].children[0].geometry.string[0];
    }
    obj.set_width = function(value) {
     this.children[0].children[0].geometry.maxExtent = -value;
     this.width = value;
    }
    obj.font_size = function(value) {
     return this.children[0].children[0].geometry.fontStyle ? this.children[0].children[0].geometry.fontStyle.size : 1;
    }

    if (obj.children[0].children[0].appearance && (typeof (obj.children[0].children[0].appearance.skin) == 'undefined')) {
     obj.set_color = function(r, g, b) {
      this.children[0].children[0].appearance.material.emissiveColor = new SFColor(r, g, b);
     }
     obj.children[0].set_alpha = function(a) {
      this.children[0].children[0].appearance.material.transparency = 1-a;
     }
    } else {
     obj.set_color = function(r, g, b) {}
     obj.set_alpha = function(a) {}
    }
    obj.set_size = function(width, height) {
     var justify = this.children[0].children[0].geometry.fontStyle ? this.children[0].children[0].geometry.fontStyle.justify[0] : '';
     if (justify=='BEGIN') {
      this.children[0].translation.x = - width / 2;
     } else if (justify=='END') {
      this.children[0].translation.x = width / 2;
     } else {
      this.children[0].translation.x = 0;
     }
     this.width = width;
     this.height = height;     
    }

    if (obj.children[0].children[0].geometry.fontStyle && (typeof (obj.children[0].children[0].geometry.fontStyle.skin) == 'undefined')) {
     obj.children[0].set_font_size = function(value) {
      this.children[0].children[0].geometry.fontStyle.size = value;
     }
     obj.children[0].set_align = function(value) {
      this.children[0].children[0].geometry.fontStyle.justify[0] = (value==0) ? 'BEGIN' : (value==2) ? 'END' : 'MIDDLE';
     }
    } else {
     obj.set_font_size = function(value) {}
     obj.set_align = function(value) {}
    }    
    if (label) obj.set_label(label);
    return obj;
}


function gw_new_window(parent, offscreen, class_name)
{
  var obj = new SFNode('Transform2D');  
  setup_gw_object(obj, 'Window');
  obj.rect = gw_new_rectangle(class_name);  
  obj.visible = false;
  obj.scale.x = obj.scale.y = 0;
  
  obj.on_event = function(evt) { return 0; }
  
  obj.set_style = function(classname, style) {
    this.rect.set_style(classname, style);
  }
  if (! offscreen) {
    gw_add_child(obj, rect);
    var item = new SFNode('Layer2D');  
    setup_gw_object(item, 'Layer');
    gw_add_child(obj, item);
    obj.set_size = function(width, height) { 
     this.width = width;
     this.height = height;
     this.children[0].set_size(width, height);
     this.children[1].size.x = width;
     this.children[1].size.y = height;
     this.layout(width, height);
    }  
    obj.set_alpha = function(alpha) {
     this.children[0].set_alpha(alpha);
    } 
    obj.add_child = function(child) {
     this.children[1].children[this.children[1].children.length] = child;
    }
    obj.remove_child = function(child) {
     this.children[1].removeChildren[0] = child;
    }
    obj._wnd_close = function() {
     obj.rect = null;
     gw_close_child_list(this.children);
    }
  } else {
    var shape = new SFNode('Shape'); 
    shape.appearance = new SFNode('Appearance');
    shape.appearance.material = new SFNode('Material2D');
    shape.appearance.material.filled = TRUE;
    shape.appearance.texture = new SFNode('CompositeTexture2D');
    shape.appearance.texture.children[0] = obj.rect;
    shape.geometry = new SFNode('Bitmap');
    obj.children[0] = shape;
    
    obj.set_size = function(width, height) { 
     this.width = width;
     this.height = height;
     this.children[0].appearance.texture.pixelWidth = width;
     this.children[0].appearance.texture.pixelHeight = height;
     this.children[0].appearance.texture.children[0].set_size(width, height);
     this.layout(width, height);
    }
  
    obj.set_alpha = function(alpha) {
     this.children[0].appearance.material.transparency = 1 - alpha;
    } 
    obj.get_alpha = function() {
     return 1 - this.children[0].appearance.material.transparency;
    } 

    obj.add_child = function(child) {
     this.children[0].appearance.texture.children[this.children[0].appearance.texture.children.length] = child;
    }
    obj.remove_child = function(child) {
     this.children[0].appearance.texture.removeChildren[0] = child;
    }
    obj._wnd_close = function() {
     obj.rect = null;
     gw_close_child_list(this.children[0].appearance.texture.children);
    }
  }
  obj._pre_destroy = obj._wnd_close;
  obj.set_corners = function(tl, tr, br, bl) {
   this.rect.set_corners(tl, tr, br, bl);
  }
  
  if (typeof gwskin[class_name] != 'undefined') {
   if (typeof gwskin[class_name].show != 'undefined') obj.show = gwskin[class_name].show;
   if (typeof gwskin[class_name].hide != 'undefined') obj.hide = gwskin[class_name].hide;
  }
  obj._on_wnd_close = obj.close;
  obj.close = function () {
   this.hide(this._on_wnd_close);
  }
 obj.layout = function (w, h) {}
   
  gw_add_child(parent, obj);
  return obj;
}

function gw_new_icon_button(parent, icon_url, label, class_name, horizontal)
{
  var touch;
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'IconButton');       

  if (arguments.length<5) horizontal = false;
  
  obj.children[0] = new SFNode('Transform2D');
  obj.children[0].children[0] = new SFNode('Layer2D');    
  obj.children[1] = gw_new_rectangle(class_name, 'invisible');  

  obj.children[2] = new SFNode('TouchSensor');

  if (gwskin.get_font(class_name) != null) {
    obj.children[3] = gw_new_text(null, label, class_name);
    if (horizontal) {
      obj.set_size = function(width, height) { 
       this.children[0].children[0].size.x = height; 
       this.children[0].children[0].size.y = height;
       this.children[0].translation.x = (height-width)/2;
       this.children[1].set_size(width, height);
       this.children[3].set_width(width-height);
       this.children[3].translation.x = height - width/2;
       this.width = width;
       this.height = height;
      };
    } else {
      obj.set_size = function(width, height) { 
       var fsize = this.children[3].font_size();
       var h = height - fsize;
       this.children[0].children[0].size.x = h; 
       this.children[0].children[0].size.y = h;
       this.children[0].translation.y = (height-h)/2;
       this.children[1].set_size(3*width/2, height);
       this.children[3].set_width(3*width/2);
       this.children[3].translation.y = -height/2+fsize/2; 
       this.width = 3*width/2;
       this.height = height;
      };
    }
    obj.set_label = function(label) {
     this.children[3].set_label(label);
    } 
    obj.get_label = function() {
     return this.children[3].get_label();
    } 
  } else {
    obj.set_size = function(width, height) { 
     this.children[0].children[0].size.x = width; 
     this.children[0].children[0].size.y = height;
     this.children[1].set_size(width, height);
     this.width = width;
     this.height = height;
    };
    obj.set_label = function(label) {
     this.label = label;
    }
    obj.get_label = function() {
     return this.label;
    }
  }

  obj._last_ts = 0;
  obj.on_long_click = NULL;
  obj.on_click = NULL;
  obj.on_over = null;
  obj.down = false;
  obj.over = false;
  obj._on_active = function(value, timestamp) { 
   if (value) {
    this.down = true;
    this.children[1].set_style(class_name, 'down');
    this._last_ts = timestamp;
   } else {
    if (this.down && this.over) {
      if (this.on_long_click && (timestamp - this._last_ts > gwskin.long_click_delay)) this.on_long_click();
      else if (this.on_click) this.on_click(); 
    }
    this.down = false;
    this.children[1].set_style(class_name, (this.over && gwskin.pointing_device) ? 'over' : 'invisible');
   }
  };
  obj._on_over = function(value) { 
   this.over = value; 
   if (gwskin.pointing_device) {
    var app = gwskin.get_style(class_name, 'over');
    if (app) {   
     this.children[1].set_style(class_name, value ? 'over' : 'invisible');
    }
    if (this.on_over) this.on_over(value); 
    if (gwskin.tooltip_callback) gwskin.tooltip_callback(value, this.get_label() );
   }
  };
  Browser.addRoute(obj.children[2], 'isOver', obj, obj._on_over); 
  Browser.addRoute(obj.children[2], 'isActive', obj, obj._on_active); 

  obj.icons = [];
  obj.add_icon = function(url) {
  
   if(typeof url == 'string') {
     var inl = gw_load_resource(url);
   } else {
     var inl = url;
   }
   this.icons[this.icons.length] = inl;

   if (this.children[0].children[0].children.length==0) {
    this.children[0].children[0].children[0] = this.icons[0];
   }
  }
  obj.switch_icon = function(idx) {
   while (idx>this.icons.length) idx-=this.icons.length;
   this.children[0].children[0].children[0] = this.icons[idx];
  }
  obj._pre_destroy = function() {
   for (var i in this.icons) {
    gw_unload_resource(this.icons[i]);
   }
   this.icons.length = 0;
   Browser.deleteRoute(this.children[2], 'ALL',  null, null);
  }
   
  
  if (icon_url) obj.add_icon(icon_url);
  obj.set_label(label);
  
  
  if (typeof gwskin[class_name] != 'undefined') {
    if (typeof gwskin[class_name].width == 'undefined') gwskin[class_name].width = control_icon_size;
    if (typeof gwskin[class_name].height == 'undefined') gwskin[class_name].height = control_icon_size;
    obj.set_size(gwskin[class_name].width, gwskin[class_name].height);
  } else {
   obj.set_size(gwskin.control_icon_size, gwskin.control_icon_size);
  }
  gw_add_child(parent, obj);
  return obj;
}


function gw_new_subscene(parent)
{
	var inline = new SFNode('Inline');
	inline._name = 'Subscene';
	inline._pre_destroy = function() {
    this.url[0]=''; 
    this.url.length = 0;
  }
	inline.connect = function(url) {
    this.url.length = 0;
    this.url[0]=url; 
	}
	gw_add_child(parent, inline);
	return inline;
}

function gw_new_button(parent, text, class_name)
{
  var label;
  obj = gw_new_rectangle(class_name, 'normal');
  label = gw_new_text(obj, text, class_name);
  gw_object_set_hitable(obj);
  obj.on_over = function(val) {
    if (gwskin.pointing_device) {
     this.set_style(class_name, val ? 'over' : 'normal');
    }
    if (gwskin.tooltip_callback) gwskin.tooltip_callback(value, this.get_label() );
  }
  obj.on_down = function(val) {
    this.set_style(class_name, val ? 'down' : (this._over ? 'over' : 'normal') );
  }
  
  obj._set_size = obj.set_size; 
  obj.set_size = function(width, height) { 
   this._set_size(width, height);
   this.children[1].set_size(width, height);
   this.children[1].set_width(width);
  };
  obj.set_label = function(label) {
   this.children[1].set_label(label);
  } 
  obj.get_label = function() {
   return this.children[1].get_label();
  } 
  obj.set_labels = function() {
   this.children[1].set_labels.apply(this.children[1], arguments);
  } 
  gw_add_child(parent, obj);
  return obj;
}



function grid_event_navigate(dlg, children, type) 
{
 var i;
 if (dlg.current_focus==-1) {
  for (i=0; i<children.length; i++) {
   if (children[i].visible && (children[i].translation.x - children[i].width/2 >= -dlg.width/2)) {
    dlg.current_focus = i;
    gpac.set_focus(children[i]);
    return true;
   } 
  }
 }
 if (type=='Right') {
  var orig_focus = dlg.current_focus;
  var switch_page = 0;
  var tr = children[dlg.current_focus].translation.y - children[dlg.current_focus].height/2;
  if (dlg.current_focus + 1 == children.length) return false;
  /*goto right item*/
  dlg.current_focus++;
  /*check we are not going down*/
  while (1) {
   if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y + children[dlg.current_focus].height/2>tr))
    break;
   if (dlg.current_focus + 1 == children.length) {
    dlg.current_focus = orig_focus;
    return false;
   }
   dlg.current_focus++;
   switch_page=1;
  }
  /*check we haven't move to a new page*/
  if (children[orig_focus].translation.y + children[orig_focus].height/2 <= children[dlg.current_focus].translation.y - children[dlg.current_focus].height/2) {
   switch_page=1;
  }       
  gpac.set_focus(children[dlg.current_focus]);
  if (switch_page) dlg._move_to(dlg.translate - dlg.width);
  return true;
 }
 
 if (type=='Left') {
  var orig_focus = dlg.current_focus;
  var switch_page = 0;
  var tr = children[dlg.current_focus].translation.y + children[dlg.current_focus].height/2;
  if (!dlg.current_focus) return false;
  /*goto left item*/
  dlg.current_focus--;
  /*check we are not going up*/
  while (1) {
   if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y - children[dlg.current_focus].height/2 < tr))
     break;
   if (!dlg.current_focus) {
    dlg.current_focus = orig_focus;
    return false;
   }
   dlg.current_focus--;
   switch_page=1;
  }
  /*check we haven't move to a new page*/
  if (children[orig_focus].translation.y - children[orig_focus].height/2 >= children[dlg.current_focus].translation.y + children[dlg.current_focus].height/2) {
   switch_page=1;
  }    
  gpac.set_focus(children[dlg.current_focus]);
  if (switch_page) dlg._move_to(dlg.translate + dlg.width);
  return true;
 }

 if (type=='Down') {
  var orig_focus = dlg.current_focus;
  var switch_page = 0;
  var ty = children[dlg.current_focus].translation.y - children[dlg.current_focus].height/2;
  if (dlg.current_focus + 1 == children.length) return false;
  dlg.current_focus++;
  /*goto next line*/
  while (1) {
   if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y - children[dlg.current_focus].height/2<ty))
    break;
   if (dlg.current_focus + 1 == children.length) {
    dlg.current_focus = orig_focus;
    return false;
   }
   dlg.current_focus++;
  }
  /*goto bottom child*/
  var tx = children[orig_focus].translation.x - children[orig_focus].width/2;
  while (1) {
   if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.x + children[dlg.current_focus].width/2 > tx)) 
      break;
   if (dlg.current_focus + 1 == children.length) break;
   dlg.current_focus++;
  }
  
  gpac.set_focus(children[dlg.current_focus]);
  return true;
 }
 
 if (type=='Up') {
  var orig_focus = dlg.current_focus;
  var switch_page = 0;
  var ty = children[dlg.current_focus].translation.y + children[dlg.current_focus].height/2;
  if (!dlg.current_focus) return false;
  dlg.current_focus--;
  /*goto previous line*/
  while (1) {
   if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.y + children[dlg.current_focus].height/2>ty))
     break;
   if (!dlg.current_focus) {
    dlg.current_focus = orig_focus;
    return false;
   }
   dlg.current_focus--;
  }
  /*goto above child*/
  var tx = children[orig_focus].translation.x + children[orig_focus].width/2;
  while (1) {
   if (children[dlg.current_focus].visible && (children[dlg.current_focus].translation.x - children[dlg.current_focus].width/2 < tx)) 
    break;
   if (!dlg.current_focus) {
    dlg.current_focus = orig_focus;
    return false;
   }
   dlg.current_focus--;
  }
  gpac.set_focus(children[dlg.current_focus]);
  return true;
 }
 return false;
}
  
function gw_new_grid_container(parent)
{
  var obj = new SFNode('Transform2D');  
  setup_gw_object(obj, 'FlipContainer');       
  
  obj._ps2d = new SFNode('PlaneSensor2D');
  obj._ps2d.maxPosition.x = obj._ps2d.maxPosition.y = -1;
  obj._ps2d.autoOffset = TRUE;

  obj.children[0] = new SFNode('Layer2D');  
//  obj.children[0].children[0] = gw_new_rectangle('', 'invisible');
  obj.children[0].children[0] = obj._ps2d;
  obj.children[0].children[1] = new SFNode('Transform2D');
    
  obj._move_to = function(value) {
   this.translate = value;
   if (this.translate>0) this.translate = 0;
   else if (this.translate<-this.max_width) this.translate = -this.max_width;
   
   if (0) {
     this.timer = gw_new_timer(true);
     this.timer.set_timeout(0.25, false);
     this.timer.from = this.children[0].children[1].translation.x;
     this.timer.dlg = this;
     this.timer.on_fraction = function(val) {
      var x = (1-val)*this.from + val*this.dlg.translate;
      this.dlg.children[0].children[1].translation.x = x;
      
  /*
      var children = obj.children[0].children[1].children;
      for (var i=0; i<children.length; i++) {
       children[i].scale.x = (val>0.5) ? (2*val - 1) : (1 - 2*val);
      }
  */
     }
     this.timer.on_active = function(val) {
      if (!val) {
       this.dlg.timer = null;
       this.dlg = null;
      }
     }
     this.timer.start(0);
   } else {
     this.children[0].children[1].translation.x = this.translate;
   }
   this._ps2d.offset.x = -this.translate;
  }
  
  obj.translate = 0;
  obj._on_slide = function(val) {
   this.translate = - val.x;
   if (this.translate>0) this.translate = 0;
   else if (this.translate<-this.max_width) this.translate = -this.max_width;
   this.children[0].children[1].translation.x = this.translate;
  }
  obj._on_active = function(val) {
   if (!val) {
    var off_x = 0;
    while (off_x - this.width/2 > this.translate) off_x -= this.width;
    this._move_to(off_x);
   }
  }
  Browser.addRoute(obj._ps2d, 'translation_changed', obj, obj._on_slide);
  Browser.addRoute(obj._ps2d, 'isActive', obj, obj._on_active);
  obj.children[0].children[2] = obj._ps2d;
 
  obj.spread_h = false;
  obj.spread_v = false;
  obj.break_at_hidden = false;
  obj.set_size = function(width, height) {  
    this.width = width;
    this.height = height;
    this.children[0].size.x = width;
    this.children[0].size.y = height;
//    this.children[0].children[0].set_size(width, height);
    this.nb_visible=0;
    this.first_visible=0;
    this.layout();
  }
  obj.max_width = 0;
  obj.layout = function() {  
   var spread_x, nb_wid_h, start_x, start_y, maxh, width, height, page_x, nb_on_line;
   var children = obj.children[0].children[1].children;
   width = this.width;
   height = this.height;   

   page_x = 0;   
   maxh = 0;
   spread_x = -1;
   start_x = -width/2;
   start_y = height/2;
   nb_on_line = 0;
   this.current_focus = -1;
   for (var i=0; i<children.length; i++) {
    //start of line: compute H spread and max V size
    if (spread_x == -1) {
      var j=0, len = 0, maxh = 0, nb_child = 0;
      start_x = - width/2 + page_x;
      while (1) {
        if (!children[i+j].visible) {
         j++;
         if (i+j==children.length) break;
         if (this.break_at_hidden) break;
         continue;
        }
        if (len + children[i+j].width > width) break;
        len += children[i+j].width;
        if (maxh < children[i+j].height) maxh = children[i+j].height;
        j++;
        nb_child++;
        if (i+j==children.length) break;
      }
      if (nb_child<=1) {
       maxh = children[i].height;
       if (this.spread_h) start_x = -len/2;
      }
      else if (this.spread_h) {
       spread_x = (width - len) / (nb_child);
       start_x += spread_x/2;
      } else {
       spread_x = 0;
      }
    }

    if (!children[i].visible) {
     if (nb_on_line && this.break_at_hidden) {
       nb_on_line = 0;
       spread_x = -1;
       start_y -= maxh/2;
       start_x = - width/2;
     }
     continue;
   }
   if (start_y - maxh < - height/2) {
     nb_on_line = 0;
     page_x += width;
     spread_x = -1;
     start_y = height/2;
     i--;
     continue;
    }
    children[i].translation.x = start_x + children[i].width/2;
    children[i].translation.y = start_y - maxh/2;

//    alert('child i' + i + '/' + children.length + ' size ' + children[i].width + 'x' + children[i].height + ' translation ' + children[i].translation);
    start_x += children[i].width + spread_x;
     
    nb_on_line++;

    if (i+1==children.length) {
     break;
    }
    if (start_x - page_x + children[i+1].width > width/2) {
     nb_on_line = 0;
     spread_x = -1;
     start_y -= maxh;
     start_x = - width/2;
    }
   }
   this.max_width = page_x;
   this._ps2d.enabled = (this.max_width) ? TRUE : FALSE;
  }
  
  obj.add_child = function(child) {
   this.children[0].children[1].children[this.children[0].children[1].children.length] = child;
   gw_add_child(child, this._ps2d);
  }
  obj.remove_child = function(child) {
   /*remove ps2d from each child, otherwise we create references which screw up JS GC*/
   if (typeof child.remove_child != 'undefined') child.remove_child(this._ps2d);
   else child.removeChildren[0] = this._ps2d;
   
   this.children[0].children[1].removeChildren[0] = child;
  }

  obj.get_children = function() {
  return this.children[0].children[1].children;
  }
  obj.reset_children = function () {
   gw_close_child_list(this.children[0].children[1].children);
   this.children[0].children[1].children.length = 0;
  }
  obj._pre_destroy = function() {
    Browser.deleteRoute(this._ps2d, 'ALL', null, null);
    this.reset_children();
    this.children.length = 0;
    this._ps2d = null;
  }
  
  obj.set_focus = function(type) {
   grid_event_navigate(this, this.children[0].children[1].children, type);
  }

  obj.on_event = function(evt) {
   switch (evt.type) {
   case GF_EVENT_MOUSEWHEEL:
    if (evt.button==1) {
      var tr = this.translate + (evt.wheel>0 ? this.width : -this.width);
      this._move_to(this.translate + (evt.wheel<0 ? this.width : -this.width) );
      return 1;
    }
    return 0;
   case GF_EVENT_KEYDOWN:
    if ((evt.keycode=='Up') || (evt.keycode=='Down') || (evt.keycode=='Right') || (evt.keycode=='Left') ) {
      return grid_event_navigate(this, this.children[0].children[1].children, evt.keycode);
    } 
   case GF_EVENT_KEYDOWN:
    if ((evt.keycode=='Up') || (evt.keycode=='Down') || (evt.keycode=='Right') || (evt.keycode=='Left') ) {
      return grid_event_navigate(this, this.children[0].children[1].children, evt.keycode);
    } 
    return 0;
   }
   return 0;
  } 

  gw_add_child(parent, obj);
  return obj;
}



function gw_new_progress_bar(parent, vertical, with_progress, class_name)
{
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'ProgressBar');
  var ps2d;

  if (arguments.length<=3) class_name = 'progress';
  if (arguments.length<=2) with_progress = false;
  if (arguments.length<=1) vertical = false;
  
  obj.children[0] = gw_new_rectangle(class_name, 'normal');
  if (with_progress) {
    obj.children[1] = gw_new_rectangle(class_name, 'down');

    obj.set_progress = function(prog) {
     if (prog<0) prog=0;
     else if (prog>100) prog=100;
     this.prog = prog;
     prog/=100;
     if (this.vertical) {
       var pheight = prog*this.height;
       this.children[1].set_size(this.width, pheight);
       this.children[1].move(0, (this.height-pheight)/2, 0);
     } else {
       var pwidth = prog*this.width;
       this.children[1].set_size(pwidth, this.height);
       this.children[1].move((pwidth-this.width)/2, 0);
     }
    }
  } else {
    obj.set_progress = function(prog) {}
  }
  obj.slide_idx = obj.children.length;
  obj.children[obj.slide_idx] = gw_new_rectangle(class_name, 'over');

  obj.children[obj.slide_idx+1] = gw_new_rectangle(class_name, 'invisible');
  ps2d = new SFNode('PlaneSensor2D');
  obj.children[obj.slide_idx+1].children[1] = ps2d;
  ps2d.maxPosition.x = -1;
  ps2d.maxPosition.y = -1;
  
  obj.on_slide = function(value, type) { }

  obj._with_progress = with_progress;
  obj._sliding = false;
  obj._slide_active = function(val) {
   obj._sliding = val;
   this.on_slide(this.min + (this.max-this.min) * this.frac, val ? 1 : 2);
  }
  Browser.addRoute(ps2d, 'isActive', obj, obj._slide_active);  

  obj._set_trackpoint = function(value) {
   if (this.vertical) {
    this.frac = value.y/this.height;
   } else {
    this.frac = value.x/this.width;
   }
   this.frac += 0.5;
   if (this.frac>1) this.frac=1;
   else if (this.frac<0) this.frac=0;

   this._set_frac();
   this.on_slide(this.min + (this.max-this.min) * this.frac, 0);
  }
  Browser.addRoute(ps2d, 'trackPoint_changed', obj, obj._set_trackpoint);  

  obj._set_frac = function() {
   if (this.vertical) {
     var pheight = this.frac*this.height;
     if (pheight<this.width) pheight = this.width;
     this.children[this.slide_idx].set_size(this.width, pheight);
     this.children[this.slide_idx].move(0, (this.height-pheight)/2, 0);
   } else {
     var pwidth = this.frac*this.width;
     if (pwidth<this.height) pwidth = this.height;
     this.children[this.slide_idx].set_size(pwidth, this.height);
     this.children[this.slide_idx].move((pwidth-this.width)/2, 0);
   }
  }

  obj.set_value = function(value) {
   if (this._sliding) return;
   
   value -= this.min;
   if (value<0) value = 0;
   else if (value>this.max-this.min) value = this.max-this.min;
   if (this.max==this.min) value = 0;
   else value /= (this.max-this.min);

   this.frac = value;
   this._set_frac();   
  }
    

  obj.vertical = vertical;
  obj.set_size = function(w, h) {
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
  
  obj.move = function(x,y) {
   this.translation.x = x;
   this.translation.y = y;
  }

  obj.prog = 0;
  obj.frac = 0;
  obj.set_size(vertical ? 10 : 200, vertical ? 200 : 10);
    
  gw_add_child(parent, obj);
  return obj;
}

function gw_new_slider(parent, vertical, class_name)
{
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'Slider');

  if (arguments.length<=2) class_name = 'progress';
  if (arguments.length<=1) vertical = false;

  obj.children[0] = gw_new_rectangle(class_name, 'normal');
  obj.children[1] = gw_new_rectangle(class_name, 'over');
  obj.children[2] = gw_new_rectangle(class_name, 'invisible');

  var ps2d = new SFNode('PlaneSensor2D');
  obj.children[2].children[1] = ps2d;
  ps2d.maxPosition.x = -1;
  ps2d.maxPosition.y = -1;
  obj.on_slide = function(value, type) { }

  obj.slide_active = function(val) {
   this.on_slide(this.min + (this.max-this.min) * this.frac, val ? 1 : 2);
  }
  Browser.addRoute(ps2d, 'isActive', obj, obj.slide_active);  
  obj.set_trackpoint = function(value) {
   if (vertical) {
    this.frac = value.y/(this.height-this.children[1].height);
   } else {
    this.frac = value.x/(this.width-this.children[1].width);
   }
   this.frac += 0.5;
   if (this.frac>1) this.frac=1;
   else if (this.frac<0) this.frac=0;

   if (vertical) {
    value.y = (this.frac-0.5) * (this.height-this.children[1].height);
    value.x = 0;
   } else {
    value.x = (this.frac-0.5) * (this.width-this.children[1].width);
    value.y = 0;
   }
   this.children[1].translation = value;
   this.on_slide(this.min + (this.max-this.min) * this.frac, 0);
  }
  Browser.addRoute(ps2d, 'trackPoint_changed', obj, obj.set_trackpoint);  

  obj.set_value = function(value) {
   if (this.children[2].isActive) return;
   
   value -= this.min;
   if (value<0) value = 0;
   else if (value>this.max-this.min) value = this.max-this.min;
   if (this.max==this.min) value = 0;
   else value /= (this.max-this.min);

   value -= 0.5;
   value *= (vertical ? this.height : this.width);

   if (vertical) {
    this.children[1].translation.y = value;
   } else {
    this.children[1].translation.x = value;
   }   
  }
    
  obj.vertical = vertical;
  obj.set_size = function(w, h, cursor_w, cursor_h) {
   this.width = w;
   this.height = h;
   this.children[0].set_size(w, h);

   if (typeof(cursor_w)=='undefined') cursor_w = this.children[1].width;
   if (typeof(cursor_h)=='undefined') cursor_h = this.children[1].height;
   this.children[1].set_size(cursor_w, cursor_h);

   if (this.vertical) {
    this.children[2].set_size(this.children[1].width, h);
   } else {
    this.children[2].set_size(w, this.children[1].height);
   }
  }
  obj.set_cursor_size = function(w, h) {
   this.children[1].set_size(w, h);
  }
  obj.min = 0;
  obj.max = 100;
  obj.frac = 0;
  obj.set_trackpoint(0);
  
  obj.move = function(x,y) {
   this.translation.x = x;
   this.translation.y = y;
  }
  obj.set_size(vertical ? 10 : 200, vertical ? 200 : 10, 10, 10);

  gw_add_child(parent, obj);
  return obj;
}


function gw_new_window_full(parent, offscreen, the_label, class_name, child_to_insert)
{
 var wnd = gw_new_window(parent, offscreen, class_name);

 if (arguments.length<=4) child_to_insert = null;
 if (child_to_insert) {
  gw_add_child(wnd, child_to_insert);
 }
 
 wnd.area = null;

 wnd.tools = gw_new_grid_container(wnd); 
 wnd.tools._name = 'ToolBar';
 wnd.tool_size = gwskin.icon.width;

 wnd.label = null;
 if (the_label) {
  wnd.label = gw_new_text(wnd.tools, the_label, class_name);
  wnd.label.set_size(wnd.tool_size, wnd.tool_size);
  wnd.label.set_width(wnd.tool_size);
 }
 
 wnd.set_label = function(label) {
  if (wnd.label) wnd.label.set_label(label);
 }

 
 wnd.add_tool = function(icon, name) {
  var tool = gw_new_icon_button(this.tools, icon, name, 'icon');
  if (this.label) {
   this.tools.remove_child(this.label);
   this.tools.add_child(this.label);
  }
  tool.set_size(wnd.tool_size, wnd.tool_size);
  tool._name = name;
  tool.dlg = this;
  return tool;
 }

 
 wnd.on_close = null;
 var icon = wnd.add_tool(gwskin.images.cancel, gwskin.labels.close);
 icon.on_click = function() {
  if (this.dlg.on_close) this.dlg.on_close();
  this.dlg.close(); 
 }

 wnd._pre_destroy = function() {
  if (typeof this.predestroy != 'undefined') this.predestroy();
  this.tools = null;
  this.label = null;
  this.area = null;
  this._wnd_close();
 }
 
 wnd.on_size = null;
 wnd.layout = function(width, height) {  
  var i;
  var tools = this.tools.get_children();

  if (this.area) {
    this.area.set_size(width, height - this.tool_size);
    this.area.move(0, (this.area.height-height)/2);
  }
  if (wnd.on_size) wnd.on_size(width, tools.length ? (height - this.tool_size) : height);
  
  if (this.label) {
    var textw = width;
    for (i=0; i<tools.length-1; i++) {
     if (tools[i].visible) textw -= tools[i].width;
    }
    if (textw<0) textw=0;
    this.label.set_size(textw, this.tool_size);
	  this.label.set_width(textw);
  }

  this.tools.move(0, (height - this.tool_size)/2);
  this.tools.set_size(width, this.tool_size);
 }
 
  wnd.set_tool_size = function(widget_tool_size) {
   var i;
   var tools = this.tools.get_children();
   this.tool_size = widget_tool_size;
   for (i=0; i<tools.length; i++) {
    tools[i].set_size(this.tool_size, this.tool_size);
   }
   this.tools.set_size(this.width, widget_tool_size);
  }

 return wnd; 
}

function gw_new_listbox(container)
{
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'List');
  obj.children[0] = new SFNode('Layer2D');

  obj._ps2d = new SFNode('PlaneSensor2D');
  obj._ps2d.maxPosition.x = obj._ps2d.maxPosition.y = -1;
  obj._ps2d.autoOffset = FALSE;
  obj._on_slide = function(val) {
   this._translation_y = -val.y;
   this.layout();
  }
  Browser.addRoute(obj._ps2d, 'translation_changed', obj, obj._on_slide);

  obj.max_height = 0;
  obj.set_size = function(width, height) {
   for (var i=0; i<this.children[0].children.length; i++) {
     this.children[0].children[i].set_size(width, this.children[0].children[i].height);
   }
   this.width = width;
   this.height = height;
   this.children[0].size.x = width;
   this.children[0].size.y = height;
   this._translation_y = 0;
   this.selected_idx = 0;   
   this.layout_changed();
  }
  obj.layout_changed = function() {
   this.max_height = 0;
   for (i=0; i<this.children[0].children.length; i++) {
    if (this.children[0].children[i].visible) {
      this.max_height += this.children[0].children[i].height;
    }
   }
   this.max_height -= this.height;
   if (this.max_height < 0) this.max_height=0;
   this.layout();
  }

  obj.get_selection = function() {
   if (this.selected_idx<this.children[0].children.length) return this.children[0].children[this.selected_idx];
   return null;
  }

  obj._translation_y = 0;
  obj.width = obj.height = 0;
  obj.first_visible = -1;
  obj.last_visible = -1;
  
  obj.layout = function() {
   var children = this.children[0].children;
   var start_y = this.height/2;
   
   if (this._translation_y < 0) this._translation_y = 0;
   else if (this._translation_y > this.max_height) this._translation_y = this.max_height;

   this.first_visible = -1;
   this.last_visible = -1;
   
   for (var i=0; i<children.length; i++) {
     var y = this._translation_y + start_y;
     if (! children[i].visible) continue;
     
     if (y  - children[i].height > this.height/2) {
       children[i].scale.x = 0;
     } else if (y  + children[i].height < -this.height/2) {
       children[i].scale.x = 0;
     } else {
       children[i].move(0, y - children[i].height/2);
       children[i].scale.x = 1;

       if (this.first_visible == -1) {
        if (y  <= this.height/2) this.first_visible = i;
       }
       if (y - children[i].height >= -this.height/2) this.last_visible = i;

       if (this.selected_idx == i) {
        gpac.set_focus(children[i]);
       }
     }
     start_y -= children[i].height;
   }
   if (this.selected_idx < this.first_visible) this.selected_idx = this.first_visible;
   else if (this.selected_idx > this.last_visible) this.selected_idx = this.last_visible;
  }
  obj.add_child = function(child) {
   this.children[0].children[this.children[0].children.length] = child;
   gw_add_child(child, this._ps2d);
  }
  obj.remove_child = function(child) {
   /*remove ps2d from each child, otherwise we create references which screw up JS GC*/
   if (typeof child.remove_child != 'undefined') child.remove_child(this._ps2d);
   else child.removeChildren[0] = this._ps2d;
   this.children[0].removeChildren[0] = child;
  }

  obj.get_children = function () {
   return this.children[0].children;
  }
  obj.reset_children = function () {
   gw_close_child_list(this.children[0].children);
   this.children[0].children.length = 0;
  }
  obj._pre_destroy = function() {
    this.reset_children();
    this._ps2d = null;
  }
  
  obj.on_event = function(evt) {
   switch (evt.type) {
   case GF_EVENT_MOUSEWHEEL:
    this._translation_y += (evt.wheel>0) ? -this.height/20 : this.height/20;
    this.layout();
    return 1;
   case GF_EVENT_KEYDOWN:
    var children = this.get_children();
    if (evt.keycode=='Up') {
//      alert('sel '+ this.selected_idx + ' first '+this.first_visible+ ' last '+this.last_visible);
      if (children[this.selected_idx].translation.y + children[this.selected_idx].height/2 > this.height/2  ) {
       this._translation_y -= this.height;
       this.layout();
       return 1;
      }
      if (!this.selected_idx) return 0;
      this.selected_idx--;
      if (this.selected_idx<this.first_visible)
        this._translation_y -= children[this.last_visible].height;
      this.layout();
      return 1;
    } else if (evt.keycode=='Down') {
//      alert('sel '+ this.selected_idx + ' first '+this.first_visible+ ' last '+this.last_visible);
      if (this.selected_idx==children.length) return 0;
  
      if (children[this.selected_idx].translation.y - children[this.selected_idx].height/2 < -this.height/2  ) {
       this._translation_y += this.height;
       this.layout();
       return 1;
      }
      this.selected_idx++;
      if (this.selected_idx>this.last_visible)
        this._translation_y += children[this.first_visible].height;
      this.layout();
      return 1;
    }
    return 0;
   }
   return 0;
  } 

  
  gw_add_child(container, obj);
  return obj;
}


function gw_new_file_open(class_name)
{
  if (arguments.length==0) class_name = 'window';
  
  var dlg = gw_new_window_full(null, true, 'Browse FileSystem', 'window');
  
  dlg.area = gw_new_listbox(dlg);

  dlg.on_event = function(evt) {
   if (evt.type==GF_EVENT_KEYDOWN) {
    if (evt.keycode=='Left') {
     this._browse(null, true);
     return 1;
    }
    else if (evt.keycode=='Enter') {
     var child = this.area.get_selection();
     if (child) child.on_click();
     return 1;
    }    
   }
   if (this.area && typeof this.area.on_event != 'undefined') {
    return this.area.on_event(evt); 
   }
  }

  dlg.on_close = function () {
    if (this.on_browse) {
      this.on_browse(null, false);
    }
  }
  
  dlg.go_up = dlg.add_tool(gwskin.images.previous, gwskin.labels.up);
  dlg.go_up.on_click = function() {
   this.dlg._browse(null, true);
	}

  dlg.on_directory = null;
  dlg.scan_dir = dlg.add_tool(gwskin.images.scan_directory, gwskin.labels.scan_directory);
  dlg.scan_dir.on_click = function() {
   if (this.dlg.on_directory) this.dlg.on_directory(this.dlg.directory);
   this.dlg.close(); 
	}
	
  dlg.predestroy = function() {
   this.scan_dir = null;
   this.go_up = null;
  }

  dlg.directory = '';
  dlg.show_directory = false;
  dlg.filter = '*';

  dlg._on_dir_browse = function() {
    this.dlg._browse(this.dlg.path + this.filename, false);
  }
  dlg._on_file_select = function() {
    if (this.dlg.on_browse) {
      this.dlg.on_browse(this.dlg.path + this.filename, this.dlg.directory);
    }
    this.dlg.close(); 
  }
 
  dlg.browse = function(dir) {
    this.directory = dir;
    this._browse(dir, false);
  }
  dlg._browse = function(dir, up) {
   var w, h, i, y;
   if (dir) this.directory = dir;
 
   var filelist = gpac.enum_directory(this.directory, this.filter, up);

   if (filelist.length) {
    this.directory = filelist[0].path;
    this.set_label(this.directory);
   } else {
    this.set_label('');
   }

   this.area.reset_children();
   
   this.set_label(this.directory);

   this.path = filelist.length ? filelist[0].path : '';

   for (i=0; i<filelist.length; i++) {
    var item = gw_new_icon_button(this.area, filelist[i].directory ? gwskin.images.folder : gwskin.images.mime_generic, filelist[i].name, 'listitem', true);
    item.dlg = this;
    item.filename = filelist[i].name;

    if (filelist[i].directory) {
      item.on_click = this._on_dir_browse;
    } else {      
      item.on_click = this._on_file_select;
    }
   }  
   this.layout(this.width, this.height);

 	 if (this.directory == '') this.go_up.hide();
   else this.go_up.show();

   if (this.show_directory || (this.filter == 'dir')) this.scan_dir.show();
   else this.scan_dir.hide();

   this.tools.layout(this.tools.width, this.tools.height);
  }
  dlg.scan_dir.hide();

  dlg.on_size = function(width, height) {
    dlg.area.set_size(width, height);
  }
  dlg.set_size(20, 20);
  return dlg;
}


function gw_new_text_input(text_data, class_name)
{
  var obj = gw_new_text(null, text_data, 'edit');
  obj.on_text = null;
  obj._text_edited = function(val) {
   if (this.on_text) this.on_text(val[0]);
  }
  Browser.addRoute(obj.children[0].children[0].geometry, 'string', obj, obj._text_edited);
  return obj;
}

function gw_new_text_edit(parent, text_data)
{
  var obj = new SFNode('Transform2D');
  var rect, edit;
  setup_gw_object(obj, 'TextEdit');
  rect = gw_new_rectangle('edit');
  gw_add_child(obj, rect);
  gw_object_set_hitable(rect);
  rect.on_down = function(val) {
   if (val) gpac.set_focus( this.parent.children[1] );
  }

  edit = gw_new_text(null, text_data, 'edit')
  gw_add_child(obj, edit);
  obj.on_text = null;
  obj._text_edited = function(val) {
   if (this.on_text) this.on_text(val[0]);
  };  
  Browser.addRoute(obj.children[1].children[0].children[0].geometry, 'string', obj, obj._text_edited);

  obj.set_size = function(w, h) {
   this.width = w;
   this.height = h;
   this.children[0].set_size(w, h);
   this.children[1].move(5-w/2, 0);
  };
  
  gw_add_child(parent, obj);

  /*set focus to our edit node*/
  gpac.set_focus(edit);

  return obj;
}

function gw_new_text_area(parent, text_data, class_name)
{
  var obj = new SFNode('Transform2D');
  setup_gw_object(obj, 'TextEdit');

  obj.children[0] = new SFNode('Layout');
  obj.children[0].parent = obj;
  obj.children[0].wrap = true;
  obj.children[0].justify[0] = "JUSTIFY";
  obj.children[0].justify[1] = "FIRST";

  gw_new_text(obj.children[0], text_data,class_name);
  obj.set_size = function(width, height) {
    this.children[0].size.x = width;
    this.children[0].size.y = height;
    this.width = width;
    this.height = height;
  }
  obj.set_content = function(text) {
    this.children[0].children[0].set_label(text);
  }

  gw_add_child(parent, obj);
  return obj;
}



function gw_new_scrolltext_area(parent, text_data, class_name)
{
  var obj = gw_new_listbox(parent);
  var child = gw_new_text_area(obj, text_data, class_name);
  /*erase the resize routine*/
  child.set_size = function(width, height) {
    var bounds = this.children[0]._bounds;
    this.children[0].size.x = width;
    this.children[0].size.y = Math.ceil(bounds.height);
    this.width = width;
    this.height = this.children[0].size.y;
  }
  
  obj._orig_set_size = obj.set_size;
  obj.set_size = function(width, height) {
    this.get_children()[0].set_size(width, height);
    this._orig_set_size(width, height);
  }
  obj.set_content = function(text) {
    this.get_children()[0].set_label(text);
  }
  return obj;
}


function gw_new_message(container, label, content)
{
  var notif = gw_new_window_full(null, true, label, 'window');  
  notif.area = gw_new_scrolltext_area(notif, content, 'window');
  notif.on_size = function(width, height) {
   this.area.set_size(width-10, height);
  } 
  notif.on_event = function(evt) { 
    if ((evt.type==GF_EVENT_KEYUP) && evt.keycode==gwskin.keys.close) {
     this.close();
     return 1;
    }
    return this.area.on_event(evt); 
  }
	notif.set_size(240, 120);
	return notif;
}
