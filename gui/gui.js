/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////


all_extensions = [];


in_drag = 0;
start_drag_x=start_drag_y=0;

function my_filter_event(evt)
{
 switch (evt.type) {
 case GF_EVENT_MOUSEDOWN:
  if (evt.picked) return false;
  start_drag_x = evt.mouse_x;
  start_drag_y = evt.mouse_y;
  if (gpac.navigation != GF_NAVIGATE_NONE) return false;
  if (!gpac.fullscreen)  in_drag = 1;
  return true;
case GF_EVENT_MOUSEMOVE:
    if (in_drag) {
        in_drag = 2;
        gpac.move_window(evt.mouse_x - start_drag_x, evt.mouse_y - start_drag_y);
        return true;
    }
    return false;

case GF_EVENT_MOUSEUP:
    if (in_drag == 2) {
        in_drag = 0;
        return true;
    } 
    in_drag = 0;
    return false;
 default:
  return false;
 } 
 return false;
}

function gw_insert_media_node(node) 
{
    media_root.children[media_root.children.length] = node;
}

function gw_hide_dock() {
    dock.hide();
}

function gw_show_dock() {
    dock.show();
}

//insert an offscreen image + text
function insert_dock_icon(label, icon)
{
    var wnd = gw_new_window(dock, true, false);
    var icon = gw_new_icon_button(wnd, icon, (label=='') ? null : label);
    wnd.set_size(icon.width, icon.height);
    wnd.show();
    return icon;
}

function extension_option_setter(_ext) {
    if ((typeof _ext.config_id == 'undefined') || !_ext.config_id || (_ext.config_id == '')) {
        return function (key_name, value) {
        }
    } else {
        var ext_section = 'GUI.' + _ext.config_id;
        return function (key_name, value) {
            gpac.set_option(ext_section, key_name, value);
        }
    }
}

function extension_option_getter(_ext) {
    if ((typeof _ext.config_id == 'undefined') || !_ext.config_id || (_ext.config_id == '')) {
        return function (key_name, default_val) {
        }
    } else {
        var ext_section = 'GUI.' + _ext.config_id;
        return function (key_name, default_val) {
            var value = gpac.get_option(ext_section, key_name);
            if (value == null) {
                gpac.set_option(ext_section, key_name, default_val);
                value = default_val;
            }
            return value;
        }
    }
}

//Initialize our GUI
function initialize() {
    //var icon;
    var i, count, wid;

    gpac.caption = 'Osmo4';

    min_width = 160;
    min_height = 80;
    gw_display_width = parseInt(gpac.getOption('General', 'LastWidth'));
    gw_display_height = parseInt(gpac.getOption('General', 'LastHeight'));
    if (!gpac.fullscreen && (!gw_display_width || !gw_display_height)) {
        gw_display_width = 320;
        gw_display_height = 240;
    }
    if (!gpac.fullscreen && gw_display_width && gw_display_height) {
        gpac.set_size(gw_display_width, gw_display_height);
    } else {
        gw_display_width = gpac.get_screen_width();
        gw_display_height = gpac.get_screen_height();
    }
    //request event listeners on the window - GPAC specific BIFS extensions !!! We don't allow using the event proc for size events
    root.addEventListener('resize', on_resize, 0);

    
    /*load the UI lib*/
    Browser.loadScript('gwlib.js', false);
    gwlib_init(ui_root);

    //set background color
    root.children[0].backColor = gwskin.back_color;

    //register custom event filter
    gwlib_add_event_filter(my_filter_event);

    //what do we do with tooltips ?
//    gwskin.tooltip_callback = function(over, label) { alert('' + over ? label : ''); };


    //create the dock containing all launchers            
    dock = gw_new_grid_container(null);

    dock.on_display_size = function (w, h) {
        this.on_size(w, h);
        this.set_size(w, h);
    }
    dock.on_size = function (w, h) {
        var children = dock.get_children();
        for (var i = 0; i < children.length; i++) {
            children[i].set_size(2 * gwskin.default_control_height, gwskin.default_control_height);
        }
    }


    /*init all extensions*/
    var list = gpac.enum_directory('extensions', '*', 0);

    for (i=0; i<list.length; i++) {
        if (!list[i].directory) continue;

      if (list[i].name.indexOf('.')==0) continue;
      
      var extension = Browser.loadScript('extensions/'+list[i].name+'/init.js', true);
      if (extension == null) {
          gwlog(l_deb, 'Loading UI extension ' + list[i].name + ' failed' );
          continue;
      }

      if (typeof extension.whoami != 'string') continue;
      if (extension.whoami != 'GPAC-GUI-Extension') continue;
      if ( (typeof extension.requires_gl == 'boolean') && extension.requires_gl && !gwskin.has_opengl) continue;

      extension.path = 'extensions/' + list[i].name + '/';

      gwlog(l_deb, 'Loading UI extension ' + extension.name + ' ' + ' icon ' + extension.path + extension.icon + ' Author ' + extension.author);

      all_extensions.push(extension);

      extension.icon = insert_dock_icon(extension.name, extension.path + extension.icon);
      extension.icon.ext_description = extension;
      extension.icon.extension_obj = null;
      extension.icon.on_close = function () {
        this.ext_description.icon = null;
        this.ext_description = null;
      }
      extension.icon.on_click = function () {
          if (!this.extension_obj) {
              gwlog(l_deb, 'Loading UI extension ' + this.ext_description.name + ' - Executing JS ' + this.ext_description.path + this.ext_description.execjs);
              this.extension_obj = Browser.loadScript(this.ext_description.path + this.ext_description.execjs);
              this.compatible = (this.extension_obj != null) ? true : false;

              if (this.compatible) {
                    this.extension_obj.get_option = extension_option_getter(this.ext_description);
                    this.extension_obj.set_option = extension_option_setter(this.ext_description);
              }
          }
          if (!this.compatible) {
              var w = gw_new_message(null, 'Error', 'Extension ' + this.ext_description.name + ' is not compatible');
              w.show();
              alert('Error');
              this.parent.disable();
              return;
          }

          if (this.extension_obj && (typeof this.extension_obj.start != 'undefined')) this.extension_obj.start();
      } 
    }
    
    dock.set_size(gw_display_width, gw_display_height);
    dock.show();

    //launch all default ext
    for (i=0; i<all_extensions.length; i++) {
      if ((typeof all_extensions[i].icon.ext_description.autostart != 'undefined')
            && (all_extensions[i].icon.ext_description.autostart)) {
         
        gwlog(l_deb, 'Auto start extension ' + all_extensions[i].icon.ext_description.name);
        all_extensions[i].icon.on_click();
      }
    }

  //let's do the layout   
    layout();
}

//performs layout on all contents
function layout() 
{
    gwlib_notify_display_size(gw_display_width, gw_display_height);
}

//resize event callback
function on_resize(evt) {
    if ((gw_display_width == evt.width) && (gw_display_height == evt.height)) return;
  if (evt.width <=100) {
      gpac.set_size(100, gw_display_height);
    return;
  }
  if (evt.height <=80) {
      gpac.set_size(gw_display_width, 40);
    return;
}

gw_display_width = evt.width;
gw_display_height = evt.height;
  if (!gpac.fullscreen) {
      gpac.setOption('General', 'LastWidth', '' + gw_display_width);
      gpac.setOption('General', 'LastHeight', '' + gw_display_height);
  }
  layout();
}

function gw_background_control(enable) {
    root.children[0].set_bind = enable ? true : false;
}
