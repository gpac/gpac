/*container for widgets*/
widget_display = gw_new_container();
widget_display.on_event = function(evt) 
{
 if(!widget_display.children.length) return false;
 return widget_display.children[widget_display.children.length-1].on_event(evt);
}

widget_tool_size = 18;
widget_min_size = 5*widget_tool_size;
widget_default_size = 100;


function check_widget_display(width, height) 
{
	var count = WidgetManager.num_widgets;
	for (var i=0; i<count; i++) {
	  var x, y;
		var wid = WidgetManager.get(i);
		if (wid == null) continue;
		if (!wid.visible) continue;
		
	  x = wid.x;
    y = wid.y;
		if (x-wid.width/2 <= -width/2) x = wid.width/2-width/2;
		else if (x+wid.width/2 >= width/2) x = width/2 - wid.width/2;
		
		if (y + wid.height/2 >= height/2) y = height/2 - wid.height/2;
		else if (y  - wid.height/2 <= - height/2) y = -height/2 + wid.height/2;

		if (wid.widget_control) wid.widget_control.move(x, y);
	}
}

function scan_directory(dir)
{
	var i, j, count, list, new_wid, uri;
	list = Sys.enum_directory(dir, '.xml;.wgt;.mgt', 0);
	for (i=0; i<list.length; i++) {
		uri = list[i].path + list[i].name;
		if (list[i].directory) {
			scan_directory(uri);
		} else {
			count = WidgetManager.num_widgets;
			for (j=0; j<count; j++) {
				var wid = WidgetManager.get(j);
				if (wid.url==uri) break;
			}
			if (j==count) {
				new_wid = WidgetManager.open(uri, null);
				if (new_wid!=null) {
					widget_insert_icon(new_wid);
				}
			}
		}
	}
}

widman_cfg_dlg = null; 

function open_widget_manager(extension)
{
  if (widman_cfg_dlg) return;

  widman_cfg_dlg = gw_new_window_full(null, true, 'Widget Manager');
  widman_cfg_dlg.area = gw_new_grid_container(widman_cfg_dlg);

  var icon = gw_new_icon_button(widman_cfg_dlg.area, gwskin.images.add, 'Add widget', 'icon_label');
  icon.on_click = function() {
    widman_cfg_dlg.close();
    widman_cfg_dlg = null;
    
    var filebrowse = gw_new_file_open();
    filebrowse.show_directory = true;
    filebrowse.filter = '*.xml;*.wgt;*.mgt';
    filebrowse.browse(WidgetManager.last_widget_dir);  
  
    filebrowse.on_browse = function(value, directory) {
  		var new_wid = WidgetManager.open(value, null);
	  	if (new_wid==null) return;
		  WidgetManager.last_widget_dir = directory;
		  widget_insert_icon(new_wid);
		  dock.layout(dock.width, dock.height);
		  show_dock(true);
    }
    filebrowse.on_directory = function(directory) {
  		WidgetManager.last_widget_dir = directory;
   		scan_directory(directory);
		  show_dock(true);
    }
    filebrowse.set_size(320 , 240);
    gpacui_show_window(filebrowse);
  }
var icon = gw_new_icon_button(widman_cfg_dlg.area, gwskin.images.trash, 'Remove all widgets', 'icon_label');
  icon.on_click = function() {
		while (1) {
			var wid = WidgetManager.get(0);
			if (wid==null) break;
			widget_remove(wid);
		}
    widman_cfg_dlg.close();
    widman_cfg_dlg = null;
	  dock.layout(dock.width, dock.height);
	  show_dock(true);
  }
  widman_cfg_dlg.on_close = function() {
   widman_cfg_dlg = null;
  }
  widman_cfg_dlg.set_size(320 , 240);
  gpacui_show_window(widman_cfg_dlg);
}

function widget_insert_icon(new_wid) {
	var icon = gpacui_insert_dock_icon(new_wid.name, widget_get_icon(new_wid) );
	new_wid.in_panel = true;
	new_wid.visible = false;
	new_wid.icon_dock = icon;
	icon.on_click = on_widget_launch;
	icon.widget = new_wid;
}

function collapsable_list(info_dlg, title)
{
	var info = gw_new_button(info_dlg.area, title, 'button');
	var children = info_dlg.area.get_children();
	
	info.first_idx = children.length;
	info.dlg = info_dlg;
	info.displayed = true;
  info.on_click = function() {
   var children = this.dlg.area.get_children();
   this.displayed = !this.displayed;
	 for (var i=this.first_idx; i<this.last_idx; i++) {
    if (this.displayed) children[i].show();
    else children[i].hide();
  	
    if (typeof children[i].last_idx != 'undefined') {
      if (!children[i].displayed) children[i].on_click();
     }
   }
   this.dlg.area.layout_changed();
  }
  return info;
}

//widget information dialog
function display_widget_info(wid)
{
	var i, j, k, info;
	var y, txt, pref;

	var info_dlg = gw_new_window_full(null, true, 'Widget ' + wid.name + ' Information', 'window', null);
//  info_dlg.area = gw_new_grid_container(info_dlg);
  info_dlg.area = gw_new_listbox(info_dlg);
 
	var info = collapsable_list(info_dlg, 'Widget Metadata');
	
  gw_new_text(info_dlg.area, 'id: ' + wid.identifier + ' - shortname: '+wid.shortName + ' - name: '+wid.name, 'text');
  gw_new_text(info_dlg.area, 'version: '+wid.version, 'text');
  gw_new_text(info_dlg.area, 'content type: ' + wid.mainMimeType + ' - content encoding: '+wid.mainEncoding, 'text');
  gw_new_text(info_dlg.area, 'default size: Width = ' + wid.defaultWidth + ' Height = '+wid.defaultHeight, 'text');
  gw_new_text(info_dlg.area, 'license: '+wid.license, 'text');
  gw_new_text(info_dlg.area, 'license ref: '+wid.licenseHref, 'text');
  gw_new_text(info_dlg.area, 'description: '+wid.description, 'text');
  gw_new_text(info_dlg.area, 'author name: '+wid.authorName + ' (mail: '+wid.authorEmail+')', 'text');
  gw_new_text(info_dlg.area, 'author href: '+wid.authorHref, 'text');
  gw_new_text(info_dlg.area, 'view modes: '+wid.viewmodes, 'text');
  gw_new_text(info_dlg.area, 'UUID: '+wid.uuid, 'text');
  gw_new_text(info_dlg.area, 'Discardable: '+wid.discardable, 'text');
  gw_new_text(info_dlg.area, 'Muliple Instances: '+wid.discardable, 'text');
	var icons = wid.icons;
	for (j=0; j<icons.length; j++) {
    gw_new_text(info_dlg.area, 'Icon #'+(j+1)+': ' + icons[j].src, 'text');
	}
	info.last_idx = info_dlg.area.get_children().length;
	info.on_click();

	info = collapsable_list(info_dlg, 'Widget Manager Info');

	gw_new_text(info_dlg.area, 'nb instances: '+wid.num_instances + ' nb components: '+wid.num_components, 'text' );
	gw_new_text(info_dlg.area, 'Permanently installed: '+wid.permanent + ' - is component: '+wid.is_component, 'text' );
	if (wid.is_component) {
		gw_new_text(info_dlg.area, 'parent widget name' + wid.parent.name, 'text');
	}
	if (wid.originating_device_ip) {
		gw_new_text(info_dlg.area, 'Widget was pushed from device IP '+wid.originating_device_ip, 'text' );
	}
	gw_new_text(info_dlg.area, 'Section name in GPAC config file: '+wid.section, 'text' );
	gw_new_text(info_dlg.area, 'UA Locale: ' + scene.get_option('core', 'lang'), 'text');
	gw_new_text(info_dlg.area, 'widget src: ' + wid.url , 'text');
	gw_new_text(info_dlg.area, 'config src: ' + wid.manifest , 'text');
	gw_new_text(info_dlg.area, 'content src : '+wid.localizedSrc, 'text' );
	info.last_idx = info_dlg.area.get_children().length;
	info.on_click();

	pref = wid.features;
	if (pref.length) {
  	info = collapsable_list(info_dlg, 'Features');
  	for (j=0; j<pref.length; j++) {
  		gw_new_text(info_dlg.area, 'Feature #'+(j+1)+' name=\''+pref[j].name+'\' required=\''+pref[j].required+'\'', 'text');
  	}
  	info.last_idx = info_dlg.area.get_children().length;
  	info.on_click();
  }

	pref = wid.preferences;
	if (pref.length) {
  	info = collapsable_list(info_dlg, 'Preferences');
  	for (j=0; j<pref.length; j++) {
  		var val = pref[j].value;
  		if (val == '') val = scene.get_option(wid.section, pref[j].name);
  		gw_new_text(info_dlg.area, 'Preference #'+(j+1)+' name=\''+pref[j].name+'\' value=\''+val+'\' readOnly=\''+pref[j].readonly +'\'', 'text');
  	}
  	info.last_idx = info_dlg.area.get_children().length;
  	info.on_click();
  }

	info = collapsable_list(info_dlg, 'MPEG-U Migration Context');
	txt=wid.get_context();
	if (!txt) txt='';

	while (1) {
		var idx = txt.indexOf('\n', 0);
		if (idx>0) {
			gw_new_text(info_dlg.area, txt.substring(0, idx), 'text');
			txt = txt.substring(idx+1, txt.length);
		} else {
			gw_new_text(info_dlg.area, txt, 'text');
			break;
		}
	}
	info.last_idx = info_dlg.area.get_children().length;
	info.on_click();

  if (wid.num_interfaces) {
  	info = collapsable_list(info_dlg, 'MPEG-U Interfaces', 'button');
 		var spacer = gw_new_text(info_dlg.area, '', 'text');
  	spacer.hide();
  	info.first_idx = info_dlg.area.get_children().length;
    for (j=0; j<wid.num_interfaces; j++) {
  		var idx;
  		var ifce = wid.get_interface(j);
    	var item = collapsable_list(info_dlg, 'Interface #' + (j+1) + ' type: '+ifce.type);
      item.sublist = true;
  		gw_new_text(info_dlg.area, 'Multiple Binding: '+ifce.multipleBinding + ' - Service provider: '+ ifce.serviceProvider + ' - bound: ' + wid.is_interface_bound(ifce) , 'text');
  		for (k=0; k<ifce.num_messages; k++) {
  			var string, l;
  			var msg = ifce.get_message(k);
  			string = '  Message #'+ (k+1) + ':   ' + msg.name + '(';
  			for (l=0; l<msg.num_params; l++) {
  				par = msg.get_param(l);
  				string += (par.is_input ? 'in' : 'out') + ':' +par.name + ' ';
  			}
  			string += ')';
  			gw_new_text(info_dlg.area, string, 'text');
  		}
  	  item.last_idx = info_dlg.area.get_children().length;
    	item.on_click();
    	item.hide();
  	}
  	info.last_idx = info_dlg.area.get_children().length;
  	info.on_click();
  }

  info_dlg.area.spread_h = true;
  info_dlg.area.break_at_hidden = true;
  
  info_dlg.on_size = function(width, height) {
   var i;
   var children = this.area.get_children();
   for (i=0; i<children.length; i++) {
    if ((typeof children[i].first_idx != 'undefined') && (typeof children[i].sublist== 'undefined')) {
      children[i].set_size( (width>200) ? 200 : width, 20);
    } else {
      children[i].set_size(width, 16);
    }
   }
  }
  
  info_dlg.on_event = function(evt) {
   if (this.area && this.area.on_event(evt)) return true;
   return false;
  }

  info_dlg.set_size(320, 240);
  gpacui_show_window(info_dlg);
}


//widget close function
function widget_close(widget, force_remove)
{
	var is_comp = widget.is_component;

	if (widget.visible) {
		widget.visible = false;
		WidgetManager.corein_message(widget, 'hide');
		WidgetManager.corein_message(widget, 'deactivate');
		widget.deactivate();
		/*force disconnect of main resource - we do this because we are not sure when the widget_control will be destroyed due to JS GC*/
		if (widget.widget_control) {
			widget.widget_control.set_url('');
			widget.widget_control.close();
		}
	}
	if (!is_comp && (!widget.permanent || force_remove)) {
		WidgetManager.unload(widget, force_remove ? true : false);
	}
}

//widget remove function (close and unregister)
function widget_remove(wid) {
	if (typeof(wid.icon_dock) != 'undefined') {
		wid.icon_dock.close();
    wid.icon_dock = null;
	}		
	widget_close(wid, 1);
}

function new_migrate_callback(widget) {
 return function(renderer) {
  WidgetManager.migrate_widget(renderer, widget);
	widget_close(widget, 0);
 }
}

function on_widget_load() 
{
	WidgetManager.corein_message(this, 'activate');
	WidgetManager.corein_message(this, 'show');
	WidgetManager.corein_message(this, 'setSize', 'width', this.width, 'height', this.height, 'dpi', screen_dpi);
}


mpegu_targets = null;

function select_mpegu_target(callback)
{ 
  if (mpegu_targets) return;
  mpegu_targets = gw_new_file_open();
  mpegu_targets.set_label('Select Remote Display');
  mpegu_targets.callback = callback;

  mpegu_targets.on_close = function() {
   mpegu_targets = null;
  }
  /*override browse function*/
  mpegu_targets._on_mpegu_click = function() {
   	var target = (this.render_idx<0) ? null : WidgetManager.get_mpegu_service_providers(this.render_idx);

		mpegu_targets.callback(target);
		mpegu_targets.callback = null;
    mpegu_targets.close();
    mpegu_targets = null;
  }
  mpegu_targets._browse = function(dir, up) {
   var w, h, i, y;
   this.area.reset_children();
 
   for (i=0; ; i++) {
		var target = WidgetManager.get_mpegu_service_providers(i);
		if (!target) break;
		var	icon = 'icons/applications-internet.svg';
    var item = gw_new_icon_button(this.area, icon, target.Name);
    item.set_size(item.width, item.height);
    item.render_idx = i;
    item.on_click = this._on_mpegu_click;
   }  
   this.layout(this.width, this.height);
 }
 mpegu_targets.browse('');  
 mpegu_targets.go_up.hide();
 mpegu_targets.set_size(display_width, display_height);
 gpacui_show_window(mpegu_targets);
}


function new_widget_control(widget)
{
  var ctrl;
  

  var inline = gw_new_subscene(null);
  ctrl = gw_new_window_full(widget_display, true, null, 'offscreen', inline);
	ctrl.set_size(widget.width, widget.height);

  ctrl.inline = inline;
  ctrl.tools.spread_h = true;
  
	ctrl.component_bound = false;
	ctrl.widget = widget;
	widget.widget_control = ctrl;  
	widget.visible = true;
  
  ctrl.on_close = function() {
  	this.inline.url[0] = '';
  	this.widget.widget_control = null;  
		if (this.widget.discardable) widget_remove(this.widget);
		else widget_close(this.widget, 0);
		this.widget = null;
  }

  ctrl.tools.hide();
  gw_object_set_dragable(ctrl);

  gw_object_set_hitable(ctrl);
  ctrl.controls_visible = false;

	this.maximized = false;
  ctrl.on_double_click = function() {
   if (this.maximized) {
		this.maximized = false;
		this.move(this.widget.x, this.widget.y);
		this.set_size(this.prev_width, this.prev_height);
   } else {
		this.maximized = true;
		this.prev_width = this.widget.width;
		this.prev_height = this.widget.height;
		this.move(0, 0);
		this.set_size(display_width, display_height);
   }   
   this.controls_visible = false;
   this.tools.hide();
  }

  ctrl.on_click = function() {
    this.controls_visible = ! this.controls_visible;
    if (this.controls_visible) this.tools.show();
    else this.tools.hide();
  }
  /*push on top*/
  ctrl.on_down = function(value) {
		if (value) return;
		if (this.widget==null) return;

		//widget is a component, do not push on top but hide some controls
		if (this.widget.is_component) {
			if (this.component_bound) {
			  var chldren = this.tools.get_children();
				children[0].hide(); //close
				children[1].hide(); //remove
				children[4].hide(); //resize
			}
			return;
		}
		//otherwise push widget on top
		widget_display.push_to_top(this);
		//and push components
		comps = this.widget.components;
		for (i=0; i<comps.length; i++) {
			if (comps[i].widget_control && comps[i].widget_control.component_bound) {
				widget_display.push_to_top(comps[i].widget_control);
			}
		}
  }

  ctrl._orig_move = ctrl.move;
  ctrl.move = function(x, y) {
    this.widget.x = x;
		this.widget.y = y;
		this._orig_move(x, y);
	}

  ctrl.on_drag = function(x, y) {
		if (this.maximized) return;
    this.move(x, y);
  }
  ctrl.on_event = function(evt) {
   var x, y;
   if (evt.type != GF_EVENT_KEYDOWN) return false;
   if (!this.controls_visible) return false;

   x = this.widget.x;
   y = this.widget.y;
   if (evt.keycode=='Up') y += 10;
   else if (evt.keycode=='Down') y -= 10;
   else if (evt.keycode=='Left') x -= 10;
   else if (evt.keycode=='Right') x += 10;
   else return false;
   this.move(x, y);
   return true;
  }
 
  /*widget is not temporary*/
	if (!widget.discardable && widget.icon_dock) {
    var icon = ctrl.add_tool(gwskin.images.trash, gwskin.labels.trash);
    icon.on_click = function() { widget_remove(this.dlg.widget); };  
  }
  
  if (UPnP_Enabled) {
    ctrl.remote = ctrl.add_tool(gwskin.images.remote_display, gwskin.labels.remote_display);
    ctrl.remote.on_click = function() { 
      var dlg = select_mpegu_target(new_migrate_callback(this.dlg.widget) );    
    };  
  	
    ctrl.show_remote = function(show) {
     if (show) this.remote.show();
     else this.remote.hide();
    }
  }  

  var icon = ctrl.add_tool(gwskin.images.information, gwskin.labels.information);
  icon.on_click = function() { 
    display_widget_info(this.dlg.widget); 
  };  

  var icon = ctrl.add_tool(gwskin.images.resize, gwskin.labels.resize);
  gw_object_set_dragable(icon);
	ctrl.prev_x=0;
	ctrl.prev_y=0;
  icon.begin_drag = function() {
  	this.dlg.prev_x=this.startx;
	  this.dlg.prev_y=this.starty;
  }
  icon.on_drag = function(valx, valy) {
    var _dlg = this.dlg;
		if (_dlg.widget.width + 2*(valx - _dlg.prev_x) < widget_min_size) return;
		if (_dlg.widget.height + 2*(valy-_dlg.prev_y) < widget_min_size) return;

		_dlg.widget.width += 2*(valx - _dlg.prev_x);
		_dlg.prev_x = valx;
		_dlg.widget.height += 2*(valy - _dlg.prev_y);
		_dlg.prev_y = valy;
		_dlg.set_size(_dlg.widget.width, _dlg.widget.height);
  }
  
  ctrl.set_url = function(url) {
 	 ctrl.inline.url[0] = url;
  }
  ctrl.predestroy = function() {
   this.inline = null;
   this.remote = null;
   this.widget = null;
  }
		
	ctrl.sub_w = 0;
	ctrl.sub_h = 0;
	ctrl.sub_x = 0;
	ctrl.sub_y = 0;
	ctrl.sub_vp_w = 0;
	ctrl.sub_vp_h = 0;

  ctrl.on_widget_vp_changed = function(evt) {
		this.sub_vp_w = evt.width;
		this.sub_vp_h = evt.height;
		this.sub_vp_x = evt.offset_x;
		this.sub_vp_y = evt.offset_y;
		this.sub_w = evt.vp_width;
		this.sub_h = evt.vp_height;
  }
	ctrl.inline.addEventListener('gpac_vp_changed', ctrl.on_widget_vp_changed, 0);
		
//  ctrl.set_tool_size(widget_tool_size);  
	ctrl.set_size(widget.width, widget.height);
	if (widget.x < -display_width/2) widget.x = 0;
	else if (widget.x > display_width/2) widget.x = 0;
	if (widget.y < -display_height/2) widget.y = 0;
	else if (widget.y > display_height/2) widget.y = 0;
	ctrl.move(widget.x, widget.y);

	/*this will setup the scene graph for the widget in order to filter input and output communication pins*/
	widget.activate(ctrl.inline);

	ctrl.set_url(widget.main);

	/*send notifications once the widget scene is loaded*/
	widget.on_load = on_widget_load;

  ctrl.show();
  return ctrl;
}

function flash_window(wnd) {
	wnd.time = gw_new_timer(true);
	wnd.time.set_timeout(0.25, true);
	wnd.time.ctrl = wnd;
	wnd.time.on_fraction = function(val) {
		var scale = (val<0.5) ? 1+val/2 : 1.5-val/2;
		this.ctrl.scale.x = this.ctrl.scale.y = scale;
	}
	wnd.time.on_active = function (val) {
		 if (!val) {
      this.ctrl.time = null;
      this.ctrl = null;
     }
  }
	wnd.time.stop(1);
	wnd.time.start(0);
}

//widget launcher function
function widget_launch(wid) {
	var widg_ctrl;

	//assign default size to the widget
	if (wid.width == undefined) {
		wid.width = wid.defaultWidth;
		if (wid.width == 0) wid.width = widget_default_size;
	}
	if (wid.height == undefined) {
		wid.height = wid.defaultHeight;
		if (wid.height == 0) wid.height = widget_default_size;
	}
	if (wid.x== undefined) wid.x = 0;
	if (wid.y== undefined) wid.y = 0;

	widg_ctrl = new_widget_control(wid);
}


//starts a widget
function on_widget_launch() {
	if (this.widget.visible) {
		var awid;
		if (!this.widget.multipleInstances) return;
		awid = WidgetManager.open(this.widget.manifest, null);
		widget_launch(awid);
	} else {
		widget_launch(this.widget);
	}
  show_dock(false);
}


function widget_get_icon(widget)
{
	var icon = 'icons/image-missing.svg';
	var preferredIconType = '.svg';

	for (var i = 0; i < widget.icons.length; i++) {
		icon = widget.icons[i].relocated_src;
		if (widget.icons[i].relocated_src.indexOf(preferredIconType) > 0) {
			break;
		}
	}
	return icon;
}

function widget_insert(widget)
{
	/*insert the widget icon*/
	if (widget.permanent && !widget.is_component)
		widget_insert_icon(widget);

	/*and load the widget - comment this line to disable auto load of widget*/
	widget_launch(widget);
}

function __get_int(arg)
{
		return (typeof arg == 'string') ? parseInt(arg) : arg;
}

function widget_request_size(widget, args)
{
	if (args.length==2) {
		w = __get_int(args[0]);
		h = __get_int(args[1]);
		widget.widget_control.set_size(w, h);
	}
}

function widget_request_show(widget, args)
{
	widget.widget_control.show();
}
function widget_request_hide(widget, args)
{
	widget.widget_control.hide();
}
function widget_request_activate(widget, args)
{
	if (!widget.visible)
		widget_launch(widget);
}
function widget_request_deactivate(widget, args)
{
	if (widget.visible)
		widget_close(widget, 0);
}

function widget_request_attention(widget, args)
{
	if (widget.visible) {
		widget_display.push_to_top(widget.widget_control);
		flash_window(widget.widget_control);
	}
}

function widget_request_notification(widget, args)
{
  var notif = gw_new_message(null, ''+widget.name+' Alert!', args[0]);
//	widget_display.push_to_top(notif);
  gpacui_show_window(notif);
	notif.set_size(240, 120);
}

function widget_refresh_components_layout(ctrl, send_resize, comp_target)
{
	var i;
	var x, y, w, h, scale_x, scale_y;
	var comps;

	/*local to subscene transformation not known*/
	if (!ctrl.sub_w) return;
	if (!ctrl.sub_h) return;

	comps = ctrl.widget.components;
	for (i=0; i<comps.length; i++) {
		var comp = comps[i];
		if (!comp.widget_control || !comp.widget_control.component_bound) continue;

		//compute scale from Widget Manager coord system to widget internal coordinate system
		scale_x = ctrl.sub_vp_w / ctrl.sub_w;
		scale_y = ctrl.sub_vp_h / ctrl.sub_h;

		w = comp.widget_control.place_w * scale_x;
		h = comp.widget_control.place_h * scale_y;

		x = ctrl.translation.x - ctrl.widget.width/2 + ctrl.sub_vp_x + comp.widget_control.place_x * scale_x + w/2;
		comp.widget_control.translation.x = x;

		y = ctrl.widget.height/2 + ctrl.translation.y - h/2 - ctrl.sub_vp_y - comp.widget_control.place_y * scale_y;
		comp.widget_control.translation.y = y;

		if (send_resize || (comp_target==comp))
			comp.widget_control.set_size(w, h);
	}
}

function widget_place_component(widget, args)
{
	var comp = widget.get_component(args[0]);

	if (comp==null) {
		log(l_err, 'Component '+args[0]+' cannot be found in widget '+widget.name);
		return;
	}
	comp.widget_control.place_x = __get_int(args[1]);
	comp.widget_control.place_y = __get_int(args[2]);
	comp.widget_control.place_w = __get_int(args[3]);
	comp.widget_control.place_h = __get_int(args[4]);
	comp.widget_control.place_z = __get_int(args[5]);
	comp.widget_control.component_bound = true;
	widget_refresh_components_layout(widget.widget_control, false, comp);
}



//
// implementation of core:out install widget
//
function widget_request_install(wid, args)
{
	var wid_url = args[0];
	/*locate widget with same URL*/
	var j, count = WidgetManager.num_widgets;
	for (j = 0; j < count; j++) {
		var wid = WidgetManager.get(j);
		if (wid.url == wid_url) {
			if (!wid.in_panel) {
				widget_insert_icon(wid);
			}
			break;
		}
	}
	alert('opening widget '+wid_url);
	/*not found, install new widget*/
	if (j == count) {
		var new_wid = WidgetManager.open(wid_url, null, wid);
		if (new_wid==null) return;
		widget_insert_icon(new_wid);
	}

	var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
	if (ifce != null) {
		wmjs_core_out_invoke_reply(coreOut.installWidgetMessage, ifce.get_message("installWidget"), wid, 1); // send return code 1 = success
	}
}
function widget_migrate_component(wid, args)
{
	var comp = wid.get_component(args[0], true);
	var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");

	if (comp==null) {
		log(l_err, 'Component '+args[0]+' cannot be found in widget '+wid.name);
		if (ifce != null) {
			wmjs_core_out_invoke_reply(coreOut.migrateComponentMessage, ifce.get_message("migrateComponent"), wid, 0);
		}
		return;
	}
	comp.widget_control = null;
	if (args.length > 1 && args[1] != null) {
		alert('Migrating component to ' + UPnP.GetMediaRenderer(parseInt(args[1])).Name);
		WidgetManager.migrate_widget(UPnP.GetMediaRenderer(parseInt(args[1])), comp);
		widget_close(comp);
	} else {
    var dlg = select_mpegu_target(new_migrate_callback(comp) );    
	}
	if (ifce != null) {
		wmjs_core_out_invoke_reply(coreOut.migrateComponentMessage, ifce.get_message("migrateComponent"), wid, 1); // send return code 1 = success
	}
}

function widget_migration_targets(wid, args)
{
	var count = UPnP.MediaRenderersCount, codes = new Array(), names = new Array(), descriptions = new Array(), i, i;

	for (i = 0; i < count; i++) {
		var render = UPnP.GetMediaRenderer(i);
		codes.push(""+i);
		names.push(render.Name);
		descriptions.push(render.HostName +" "+ render.UUID);
	}
	i = null;
	var ifce_count = wid.num_interfaces, j;
	for (j = 0; j < ifce_count; j++) {
		var ifce = wid.get_interface(j);
		if (ifce.type == "urn:mpeg:mpegu:schema:widgets:core:out:2010") {
			i = ifce;
			break;
		}
	}
	if (i != null) {
		wmjs_core_out_invoke_reply(coreOut.requestMigrationTargetsMessage, i.get_message("requestMigrationTargets"),
			wid, codes, names, descriptions);
	}
}


//
// implementation of core:out activate temporary widget
//
function widget_activate_temporary_widget(wid, args) {
	var w = WidgetManager.open(args[0], null);
	if (w != null) widget_launch(w);
	var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
	if (ifce != null) {
		wmjs_core_out_invoke_reply(coreOut.activateTemporaryWidgetMessage, ifce.get_message("activateTemporaryWidget"),
			wid, (w != null ? 1 : 0)); // send return code 1 = success
	}
}

function setup_extension(extension)
{ 
  var count, i;

  /*WidgetManager module not present*/ 
  if (typeof(WidgetManager) == 'undefined') return 0;
  
  /*load MPEG-U*/ 
  Browser.loadScript('mpegu-core.js', true); 
  widget_manager_init();
   
  extension.icon = 'applications-system.svg';
  extension.label = 'Widget Manager';
  extension.desc = 'Configure Widgets';
  extension.launch = open_widget_manager;
  
  extension.initialize = function(extension) {
		WidgetManager.on_widget_remove = widget_remove;
		WidgetManager.on_widget_add = widget_insert;
		WidgetManager.coreOutSetSize = widget_request_size;
		WidgetManager.coreOutShow = widget_request_show;
		WidgetManager.coreOutHide = widget_request_hide;
		WidgetManager.coreOutRequestActivate = widget_request_activate;
		WidgetManager.coreOutRequestDeactivate = widget_request_deactivate;
		WidgetManager.coreOutShowNotification = widget_request_notification;
		WidgetManager.coreOutPlaceComponent = widget_place_component;
		WidgetManager.coreOutGetAttention = widget_request_attention;
		WidgetManager.coreOutInstallWidget = widget_request_install;
		WidgetManager.coreOutMigrateComponent = widget_migrate_component;
		WidgetManager.coreOutRequestMigrationTargets = widget_migration_targets;
		WidgetManager.coreOutActivateTemporaryWidget = widget_activate_temporary_widget;


   	gpacui_show_window(widget_display);
   	
  	/*restore our widgets*/
  	count = WidgetManager.num_widgets;
  	for (i=0; i<count; i++) {
  		var wid = WidgetManager.get(i);
  		if (wid == null) continue;
  		wid.device = null;
  		wid.device_ip = null;

  		if (wid.in_panel == true) {
        var icon = gpacui_insert_dock_icon(wid.name, widget_get_icon(wid));
  			icon.widget = wid;
  			icon.on_click = on_widget_launch;
  			wid.icon_dock = icon;
  		}

  		if (wid.visible) {
  			widget_launch(wid);
  		}
  	}
  	check_widget_display(display_width, display_height);
  }	

  if (UPnP_Enabled) {  
    extension.on_upnp_add = function(name, uuid, is_add) {
    	var i, count;
    	var show = UPnP.MediaRenderersCount ? 1 : 0;
    	count = WidgetManager.num_widgets;
    	for (i=0; i<count; i++) {
    		wid = WidgetManager.get(i);
    		if (wid == null) continue;
    		if (!wid.widget_control) continue;
    		wid.widget_control.show_remote(show);
    	}
    }
    extension.on_media_open = function(url, src_ip) {
    	if (! WidgetManager.probe(url)) return 0;
   		var new_wid = WidgetManager.open(url, src_ip);
  		if (new_wid!=null) {
  	     widget_insert(new_wid);
  		}
  		return 1;
    }
  }
  extension.on_resize = check_widget_display;
  
  /*setup all widgets*/
  return 1;
}

