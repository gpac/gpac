/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//          Jean Le Feuvre, (c) 2010-2020 Telecom Paris
//
/////////////////////////////////////////////////////////////////////////////////
import {scene} from 'scenejs'
import {Storage} from 'storage'
import {Sys} from 'gpaccore'

globalThis.scene = scene;
globalThis.Sys = Sys;

let all_extensions = [];

globalThis.gw_insert_media_node = function(node) 
{
    media_root.children[media_root.children.length] = node;
}

globalThis.gw_hide_dock = function() {
    dock.hide();
}

globalThis.gw_show_dock = function() {
    dock.show();
}

//insert an offscreen image + text
globalThis.insert_dock_icon = function(label, icon)
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
    gwlog(l_deb, 'loading storage for extension ' + storage_name);

    extension.jsobj.__gpac_storage = new Storage(storage_name);

    extension.jsobj.get_option = extension_option_getter(extension);
    extension.jsobj.set_option = extension_option_setter(extension);
}

//declare global vars (we are in strict mode)
globalThis.gw_display_width = 0;
globalThis.gw_display_height = 0;
globalThis.dock = null;

//Initialize our GUI
globalThis.initialize = function () {
    //var icon;
    var i, count, wid;

    scene.caption = 'GPAC';

    gw_display_width = parseInt(scene.get_option('GUI', 'width'));
    gw_display_height = parseInt(scene.get_option('GUI', 'height'));
    if (!scene.fullscreen && (!gw_display_width || !gw_display_height)) {
        gw_display_width = 320;
        gw_display_height = 240;
    }
    if (!scene.fullscreen && gw_display_width && gw_display_height) {
        scene.set_size(gw_display_width, gw_display_height);
    } else {
        gw_display_width = scene.screen_width;
        gw_display_height = scene.screen_height;
    }
    //request event listeners on the window - GPAC specific BIFS extensions !!! We don't allow using the event proc for size events
    root.addEventListener('resize', on_resize, 0);

    /*load the UI lib*/
    Browser.loadScript('gwlib.js', false);
    gwlib_init(ui_root);

    gwskin.enable_focus(false);    
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
    let path = scene.current_path;
    var list = Sys.enum_directory(path + 'extensions', '*', 0);

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

    var i;
          
    for (i = 1; i < Sys.args.length; i++) {
      var arg = Sys.args[i];
      if ((arg=='-h') || (arg=='-ha') || (arg=='-hx') || (arg=='-hh')) {
          print_help();
          scene.exit();                    
          return;
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
    gwskin.enable_focus(true);    

}

function print_help()
{
    globalThis._gpac_log_name = "";
    print(-2, "GPAC GUI command-line help\nOptions are specified as `-opt` or `-opt=value`.\nSee `gpac -h[x] compositor` for rendering options.\n");

    //launch all default ext
    var i;
    for (i=0; i<all_extensions.length; i++) {
      if ((typeof all_extensions[i].icon.ext_description.autostart != 'undefined')
          && (typeof all_extensions[i].clopts != 'undefined')
          && all_extensions[i].icon.ext_description.autostart) {
         
        print(-2, '# ' + all_extensions[i].icon.ext_description.name + ' extension');

        if (typeof all_extensions[i].description != 'undefined')
          print(-2, '' + all_extensions[i].description);

        if (typeof all_extensions[i].clusage != 'undefined')
          print(-2, 'Usage: ' + all_extensions[i].clusage);

        print(-2, 'Options: ');

        all_extensions[i].clopts.forEach( (opt) => {
          var s = "" + opt.name;
          if (opt.type.length)
            s += " (" + opt.type + ")";
          s += ": ";
          let olen = s.length;
          while (olen<=30) {
            olen++;
            s+= ' ';
          }
          s += opt.description;
          print(-1, s);
        });
      }
    }
    globalThis._gpac_log_name = null;
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
        scene.set_size(100, gw_display_height);
        return;
    }
    if (evt.height <=80) {
        scene.set_size(gw_display_width, 40);
        return;
    }

    gw_display_width = evt.width;
    gw_display_height = evt.height;
    if (!scene.fullscreen) {
        scene.set_option('GUI', 'width', '' + gw_display_width);
        scene.set_option('GUI', 'height', '' + gw_display_height);
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

