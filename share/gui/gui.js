/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////


all_extensions = [];

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
    var icon = gw_new_icon_button(wnd, icon, (label=='') ? null : label, 'root_icon');
    wnd.set_size(icon.width, icon.height);
    wnd.show();
    return icon;
}

function extension_option_setter(_ext) {
    if ((typeof _ext.config_id == 'undefined') || !_ext.config_id || (_ext.config_id == '')) {
        return function (key_name, value) {
        }
    } else {
        return function (key_name, value) {
            this.__gpac_storage.set_option('Config', key_name, value);
            this.__gpac_storage.save();
        }
    }
}

function extension_option_getter(_ext) {
    if ((typeof _ext.config_id == 'undefined') || !_ext.config_id || (_ext.config_id == '')) {
        return function (key_name, default_val) {
        }
    } else {
        return function (key_name, default_val) {
			if (key_name=='path') return _ext.path;
            var value = this.__gpac_storage.get_option('Config', key_name);
            if (value == null) {
                this.set_option(key_name, default_val);
                value = default_val;
            }
            return value;
        }
    }
}

function setup_extension_storage(extension) {
    var storage_name = 'config:' + extension.path + '' + extension.name;
    gwlog(l_inf, 'loading storage for extension ' + storage_name);

    extension.jsobj.__gpac_storage = gpac.new_storage(storage_name);

    extension.jsobj.get_option = extension_option_getter(extension);
    extension.jsobj.set_option = extension_option_setter(extension);
}

//Initialize our GUI
function initialize() {
    //var icon;
    var i, count, wid;

    gpac.caption = 'Osmo4';

    min_width = 160;
    min_height = 80;
    gw_display_width = parseInt(gpac.get_option('General', 'LastWidth'));
    gw_display_height = parseInt(gpac.get_option('General', 'LastHeight'));
    if (!gpac.fullscreen && (!gw_display_width || !gw_display_height)) {
        gw_display_width = 320;
        gw_display_height = 240;
    }
    if (!gpac.fullscreen && gw_display_width && gw_display_height) {
        gpac.set_size(gw_display_width, gw_display_height);
    } else {
        gw_display_width = gpac.screen_width;
        gw_display_height = gpac.screen_height;
    }
    //request event listeners on the window - GPAC specific BIFS extensions !!! We don't allow using the event proc for size events
    root.addEventListener('resize', on_resize, 0);

    
    /*load the UI lib*/
    Browser.loadScript('gwlib.js', false);
    gwlib_init(ui_root);

	gwskin.enable_background(true);

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
      extension.jsobj = null;

      gwlog(l_deb, 'Loading UI extension ' + extension.name + ' ' + ' icon ' + extension.path + extension.icon + ' Author ' + extension.author);

      if (extension.name == 'Player') {
          all_extensions.unshift(extension);
      } else {
          all_extensions.push(extension);
      }
    }

    for (i = 0; i < all_extensions.length; i++) {
      var extension = all_extensions[i];

      extension.icon = insert_dock_icon(extension.name, extension.path + extension.icon);
      extension.icon.ext_description = extension;
      extension.icon.on_close = function () {
        this.ext_description.icon = null;
        this.ext_description = null;
      }
      extension.icon.on_click = function () {
          var extension = this.ext_description;
          if (!extension.jsobj) {
              for (var i = 0; i < extension.execjs.length; i++) {
                  gwlog(l_deb, 'Loading UI extension ' + extension.name + ' - Executing JS ' + extension.path + extension.execjs[i]);
                  if (!i) {
                      extension.jsobj = Browser.loadScript(extension.path + extension.execjs[i]);
                      extension.compatible = (extension.jsobj != null) ? true : false;
                      if (!extension.compatible) break;

                      setup_extension_storage(extension);
                  } else {
                      Browser.loadScript(extension.path + extension.execjs[i]);
                  }
              }

          }
          if (!extension.compatible) {
              var w = gw_new_message(null, 'Error', 'Extension ' + extension.name + ' is not compatible');
              w.show();
              alert('Error');
              this.parent.disable();
              return;
          }

          if (extension.jsobj && (typeof extension.jsobj.start != 'undefined')) extension.jsobj.start();
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
        gpac.set_option('General', 'LastWidth', '' + gw_display_width);
        gpac.set_option('General', 'LastHeight', '' + gw_display_height);
    }
/*
    var v = 12;
    if (gw_display_height<160) {
    }
    else if (gw_display_height<576) {
        v = 24;
    }
    else if (gw_display_height<720) {
        v = 32;
    }
    else {
        v = 48;
    }
    gwskin_set_default_control_height(v*2);
    gwskin_set_default_icon_height(v);
    gwlib_refresh_layout();
    
*/

    layout();
}

