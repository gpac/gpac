//This software module was originally developed by Telecom Paris in the
//course of the development of MPEG-U Widgets (ISO/IEC 23007-1) standard.
//
//This software module is an implementation of a part of one or 
//more MPEG-U Widgets (ISO/IEC 23007-1) tools as specified by the MPEG-U Widgets
//(ISO/IEC 23007-1) standard. ISO/IEC gives users of the MPEG-U Widgets
//(ISO/IEC 23007-1) free license to this software module or modifications
//thereof for use in hardware or software products claiming conformance to
//the MPEG-U Widgets (ISO/IEC 23007-1). Those intending to use this software
//module in hardware or software products are advised that its use may
//infringe existing patents.
//The original developer of this software module and his/her company, the
//subsequent editors and their companies, and ISO/IEC have no liability 
//for use of this software module or modifications thereof in an implementation. 
//Copyright is not released for non MPEG-U Widgets (ISO/IEC 23007-1) conforming 
//products. 
//Telecom Paris retains full right to use the code for his/her own purpose, 
//assign or donate the code to a third party and to inhibit third parties from 
//using the code for non MPEG-U Widgets (ISO/IEC 23007-1) conforming products. 
//
//This copyright notice must be included in all copies or derivative works.
//
//Copyright (c) 2009.
//
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, (c) 2007-2010 Telecom Paris
//
/////////////////////////////////////////////////////////////////////////////////

// 01122011 AMD1 startWidget implemented
// TODO AMD1 listWidgets getWidget requestCapabilitiesList

//Initialize the main UI script
function initialize() {
    //var icon;
    var i, count, wid;

    scene.caption = 'MPEG-U @ Osmo4';

    display_width = parseInt(scene.get_option('Widgets', 'LastWMWidth'));
    display_height = parseInt(scene.get_option('Widgets', 'LastWMHeight'));
    
    if (display_width && display_height) {
     scene.set_size(display_width, display_height);
    } else {
     display_width = scene.screen_width;
     display_height = scene.screen_height;
    }

    //request event listeners on the window - GPAC specific extensions !!!     
    root.addEventListener('resize', on_resize, 0);
    root.addEventListener('zoom', on_zoom, 0);
    root.addEventListener('scroll', on_scroll, 0);

    widget_default_size = 200;
    scene_width = 0;
    
    wctl_iconsize = new SFVec2f(24, 24);
    icon_size = 48;
    dock_height = 48;
    info_height = 32;
    toggle_bar_height = 16;
    screen_dpi = scene.get_horizontal_dpi();
     
    widget_remote_candidate = null;
    has_upnp = eval("(typeof(UPnP) != 'undefined');");
    if (has_upnp) {
     UPnP.onMediaRendererAdd = onMediaRendererAdd;
     UPnP.onMediaConnect = onMediaConnect;
     UPnP.BindRenderer();
     upnp_renders = null;
    }
     
    /*setup dock*/
    widget_screen_visible = false;
        
    /*widget subtree*/
    widget_display = new SFNode('Transform2D');
    ui_root.children[0] = widget_display;  

    /*widget manager page*/
    widget_screen = new SFNode('Layer2D');
    ui_root.children[1] = widget_screen;  
    
    /*all other UI elements subtree*/
    dlg_display = new SFNode('Transform2D');
    ui_root.children[2] = dlg_display;  

    infobar = text_label('', 'MIDDLE');
    ui_root.children[3] = infobar;

    /*our dock*/
    dock = new SFNode('Transform2D');
    ui_root.children[4] = dock;  
        
    /*init the widget manager*/
    Browser.loadScript('mpegu-core.js', true);
    log_level = l_inf;
    widget_manager_init();

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
    WidgetManager.coreOutInstallWidget = widget_install_widget;
    WidgetManager.coreOutMigrateComponent = widget_migrate_component;
    WidgetManager.coreOutRequestMigrationTargets = widget_request_migration_targets;

    nb_widgets_on_screen = 0;
    first_visible_widget = 0;
    setup_icons();
    
    /*restore our widgets*/    
    widgets_init();

    //let's do the layout   
    layout();
}

function new_timeout(time)
{
 var obj = new SFNode('TimeSensor');
 
 obj.cycleInterval = time;
 obj.start = function(when) {
  var t = this.getTime();
  this.startTime = when + this.getTime();
 };
 obj.stop = function(when) {
  this.stopTime = when + this.getTime();
 };
 obj.on_event = null;
 obj.event = function(val) {
  if (this.on_event) this.on_event(val);
 };
 Browser.addRoute(obj, 'fraction_changed', obj, obj.event); 
 return obj;
}

function rectangle()
{
  var obj = new SFNode('Shape');

  obj.appearance = new SFNode('Appearance');
  obj.appearance.material = new SFNode('Material2D');
  obj.appearance.material.filled = TRUE;
  obj.appearance.material.emissiveColor = new SFColor(0.7, 0.7, 0.8);
  obj.appearance.material.lineProps = new SFNode('LineProperties');
  obj.appearance.material.lineProps.width = 2;
  obj.appearance.material.lineProps.lineColor = new SFColor(0, 0, 0);

  obj.geometry = new SFNode('Curve2D');
  obj.geometry.point = new SFNode('Coordinate2D');
  temp = obj.geometry.type;
  temp[0] = 7;
  temp[1] = 1;
  temp[2] = 7;
  temp[3] = 1;
  temp[4] = 7;
  temp[5] = 1;
  temp[6] = 7;
  temp[7] = 6;/*close*/

  obj.set_size = function(w, h) {
   var hw, hh, rx, ry;
   var temp;
   hw = w/2;
   hh = h/2;

   /*compute default rx/ry*/   
   ry = rx = 10;
   if ( (2*rx>=hw) || (2*ry>=hh)) rx = ry = 6;

   temp = this.geometry.point.point;
   temp[0] = new SFVec2f(hw-rx, hh);
   temp[1] = new SFVec2f(hw, hh);/*bezier ctrl point*/
   temp[2] = new SFVec2f(hw, hh-ry);
   temp[3] = new SFVec2f(hw, -hh+ry);
   temp[4] = new SFVec2f(hw, -hh);/*bezier control point*/
   temp[5] = new SFVec2f(hw-rx, -hh);
   temp[6] = new SFVec2f(-hw+rx, -hh);
   temp[7] = new SFVec2f(-hw, -hh);/*bezier control point*/
   temp[8] = new SFVec2f(-hw, -hh+ry);
   temp[9] = new SFVec2f(-hw, hh-ry);
   temp[10] = new SFVec2f(-hw, hh);/*bezier control point*/
   temp[11] = new SFVec2f(-hw+rx, hh);
  };
  obj.set_color = function(r, g, b) {
   this.appearance.material.emissiveColor.r = r;
   this.appearance.material.emissiveColor.g = g;
   this.appearance.material.emissiveColor.b = b;
  };
  return obj;
}

function icon_button(url, label, no_back)
{
    var obj = new SFNode('Transform2D');
    obj.children[0] = new SFNode('Transform2D');
    obj.children[0].scale.x = 0;
    obj.children[0].children[0] = rectangle();
    
    obj.children[1] = new SFNode('Layer2D');
    obj.children[1].size.x = icon_size;
    obj.children[1].size.y = icon_size;
    obj.children[1].children[0] = new SFNode('Inline');
    obj.children[1].children[0].url[0] = url;
    
    obj.touch = new SFNode('TouchSensor');
    obj.children[1].children[1] = obj.touch;
    obj.button_click = NULL;
    obj.down = false;
    obj.over = false;
    obj.on_active = function(value) { 
     if (value) {
      this.down = true;
     } else {
      if (this.down && this.over && this.button_click) this.button_click(); 
      this.down = false;
     }
    };
    obj.button_over = on_icon_over;
    obj.on_over = function(value) { 
     this.over = value; 
     if (!no_back)
      this.children[0].scale.x = value ? 1 : 0;
     if (this.button_over) this.button_over(value); 
    };
    Browser.addRoute(obj.touch, 'isOver', obj, obj.on_over); 
    Browser.addRoute(obj.touch, 'isActive', obj, obj.on_active); 
    obj.label = label;
    obj.hide = function() { this.scale.x = this.scale.y = 0;};
    obj.show = function() { this.scale.x = this.scale.y = 1;};
    obj.set_size = function(x, y) { 
     this.children[0].children[0].set_size(x, y);
     this.children[1].size.x = x; 
     this.children[1].size.y = y;
    };
    return obj;
}

function text_label(label, justify)
{
    var obj = new SFNode('Transform2D');
    obj.children[0] = new SFNode('Shape');
    obj.children[0].appearance = new SFNode('Appearance');
    obj.children[0].appearance.material = new SFNode('Material2D');
    obj.children[0].appearance.material.filled = TRUE;
    obj.children[0].appearance.material.emissiveColor = new SFColor(0, 0, 0);
    obj.children[0].geometry = new SFNode('Text');
    obj.children[0].geometry.string[0] = label;
    obj.children[0].geometry.fontStyle = new SFNode('FontStyle');
    obj.children[0].geometry.fontStyle.justify[0] = justify;
    obj.children[0].geometry.fontStyle.justify[1] = 'MIDDLE';
    obj.children[0].geometry.fontStyle.size = 20;
    obj.set_label = function(value) {
     this.children[0].geometry.string[0] = value;
    };
    return obj;
}


function text_rect(label)
{
  var obj = new SFNode('Transform2D');
  obj.children[0] = rectangle();
  obj.children[0].set_color(0.7, 0.7, 0.8);

  obj.children[1] = new SFNode('Shape');
  obj.children[1].appearance = new SFNode('Appearance');
  obj.children[1].appearance.material = new SFNode('Material2D');
  obj.children[1].appearance.material.filled = TRUE;
  obj.children[1].appearance.material.emissiveColor = new SFColor(0, 0, 0);
  obj.children[1].geometry = new SFNode('Text');
  obj.children[1].geometry.string[0] = label;
  obj.children[1].geometry.fontStyle = new SFNode('FontStyle');
  obj.children[1].geometry.fontStyle.justify[0] = 'MIDDLE';
  obj.children[1].geometry.fontStyle.justify[1] = 'MIDDLE';
  obj.children[1].geometry.fontStyle.size = 20;
  obj.children[2] = new SFNode('TouchSensor');

  obj.set_size = function(w, h) { 
   this.children[0].set_size(w, h);
  };


  obj.over = false;
  obj.on_over = function(value) { 
    this.children[0].set_color(0.7, value ? 0.5 : 0.7, 0.8);
    this.over = value;
  };
  Browser.addRoute(obj.children[2], 'isOver', obj, obj.on_over); 

  obj.down = false;
  obj.button_click = null;
  obj.on_active = function(value) { 
   if (value) {
    this.down = true;
   } else {
    if (this.down && this.over && this.button_click) this.button_click(); 
    this.down = false;
   }
  };
  Browser.addRoute(obj.children[2], 'isActive', obj, obj.on_active); 

  return obj;  
}

function new_widget_control(widget)
{
  var obj = new SFNode('Transform2D');
  
  obj.children[0] = new SFNode('Transform2D');
  
  obj.children[0].children[0] = rectangle();

  obj.children[0].children[1] = new SFNode('TouchSensor');


  obj.component_bound=false;
  obj.show_ctrl = true;
  obj.onClick = function(value) {
   if (!value) return;
   this.show_ctrl = !this.show_ctrl;
   if (this.show_ctrl) {
    var i, comps, idx;
    this.children[0].children[0].appearance.material.transparency = 0;
    this.children[1].scale.x = 1;
    this.children[3].scale.x = 1;

    for (i=0; i<widget_display.children.length; i++) {
     if (widget_display.children[i]==this) continue;
     if (widget_display.children[i].show_ctrl) {
       widget_display.children[i].onClick(true);
      }
    }
    //widget is a component, do not push on top but hide some controls
    if (widget.is_component) {
     if (this.component_bound) {
      this.children[1].children[0].hide(); //close
      this.children[1].children[1].hide(); //remove
      this.children[1].children[4].hide(); //resize
     }
     return;
    }
    //otherwise push widget on top
    idx=0;
    widget_display.removeChildren[idx++] = this;
    widget_display.addChildren[idx++] = this;
    //and push components    
    comps = widget.components;
    for (i=0; i<comps.length; i++) {
     if (comps[i].widget_control.component_bound) {
      widget_display.removeChildren[idx++] = comps[i].widget_control;
      widget_display.addChildren[idx++] = comps[i].widget_control;
     }
    }
    
   } else {
    this.children[0].children[0].appearance.material.transparency = 1;
    this.children[1].scale.x = 0;
    this.children[3].scale.x = 0;
   }
  };
  Browser.addRoute(obj.children[0].children[1], 'isActive', obj, obj.onClick);
  
  obj.children[1] = new SFNode('Transform2D');
  obj.children[1].children[0] = icon_button('icons/process-stop.svg', 'Close', 0);
  obj.children[1].children[0].button_click = function() {
    if (widget.discardable) widget_remove(widget);
    else widget_close(widget, 0);
  };
  
  obj.children[1].children[1] = icon_button('icons/user-trash.svg', 'Uninstall', 0);
  obj.children[1].children[1].button_click = function() { widget_remove(widget);  }
  
  obj.children[1].children[2] = icon_button('icons/applications-internet.svg', 'Push to remote display', 0);
  obj.children[1].children[2].button_click = function() { 
    if (has_upnp && UPnP.MediaRenderersCount) {
     widget_remote_candidate = widget;
     on_upnpopen();
    }
  };

  obj.children[1].children[3] = icon_button('icons/dialog-information.svg', 'Widget Information', 0);
  obj.children[1].children[3].button_click = function() { 
   display_widget_info(widget);
  };

  obj.children[1].children[4] = icon_button('icons/media-record.svg', 'Resize', 1);
  obj.children[1].children[4].children[1].children[2] = new SFNode('PlaneSensor2D');  
  obj.children[1].children[4].children[1].children[2].maxPosition = new SFVec2f(-1, -1);

  obj.prev_x=0;
  obj.prev_y=0;
  obj.onSize = function(value) {
   if (widget.width + 2*(value.x - this.prev_x)<0) return;
   if (widget.height + 2*(this.prev_y-value.y)<0) return;
   
   widget.width += 2*(value.x - this.prev_x);
   this.prev_x = value.x;
   widget.height += 2*(this.prev_y - value.y);
   this.prev_y = value.y;
   this.set_size(widget.width, widget.height);
  };
  Browser.addRoute(obj.children[1].children[4].children[1].children[2], 'translation_changed', obj, obj.onSize);
  
  obj.children[2] = new SFNode('Layer2D');
  obj.inline = new SFNode('Inline');
  obj.children[2].children[0] = obj.inline;


  obj.children[3] = new SFNode('Transform2D');
  obj.children[3].children[0] = new SFNode('Shape');
  obj.children[3].children[0].appearance = new SFNode('Appearance');
  obj.children[3].children[0].appearance.material = new SFNode('Material2D');
  obj.children[3].children[0].appearance.material.filled = TRUE;
  obj.children[3].children[0].appearance.material.transparency = 0.5;
  obj.children[3].children[0].appearance.material.emissiveColor = new SFColor(0.6, 0.6, 0.6);
  obj.children[3].children[0].geometry = new SFNode('Rectangle');
  obj.children[3].children[0].geometry.size = new SFVec2f(50, 50);
  obj.children[3].children[1] = new SFNode('PlaneSensor2D');
  obj.children[3].children[1].maxPosition = new SFVec2f(-1, -1);
  obj.children[3].children[1].offset = new SFVec2f(widget.x, widget.y);  
  obj.onMove = function(value) {
   if (this.maximized) return;
   this.translation = value;
   widget.x = value.x;
   widget.y = value.y;
   this.refresh_layout(false, null);
  };
  Browser.addRoute(obj.children[3].children[1], 'translation_changed', obj, obj.onMove);

  obj.children[3].children[2] = new SFNode('TouchSensor');
  obj.last_ts = 0;
  obj.onMaximize = function(value, timestamp) {
   if (!value) return;
   if (timestamp - this.last_ts < 0.5) {
    if (this.maximized) {
     this.maximized = false; 
     this.translation.x = widget.x;
     this.translation.y = widget.y;
     this.set_size(this.prev_width, this.prev_height);
    } else {
     this.maximized = true;
     this.prev_width = widget.width;
     this.prev_height = widget.height;
     this.translation.x = 0;
     this.translation.y = -info_height;
     this.set_size(display_width, display_height - 2*info_height);
    }   
   }
   this.last_ts = timestamp;
  };
  Browser.addRoute(obj.children[3].children[2], 'isActive', obj, obj.onMaximize);

  obj.set_size = function(w, h) {
   var i, x, s;
   s = 24;
   this.children[2].size.x = w;
   this.children[2].size.y = h;
   
   this.children[1].children[0].translation.y = this.children[1].children[1].translation.y = this.children[1].children[2].translation.y = this.children[1].children[3].translation.y = this.children[1].children[4].translation.y = h/2 + s/2;

   this.children[0].children[0].set_size(w+s, h+s);
   this.children[3].children[0].geometry.size.x = w;
   this.children[3].children[0].geometry.size.y = h;

   this.children[1].children[0].set_size(s, s);
   this.children[1].children[1].set_size(s, s);
   this.children[1].children[2].set_size(s, s);
   this.children[1].children[3].set_size(s, s);
   this.children[1].children[4].set_size(s, s);
   this.children[1].children[0].translation.x = -w/2;
   this.children[1].children[1].translation.x = -w/4;
   this.children[1].children[2].translation.x = 0;
   this.children[1].children[3].translation.x = w/4;
   this.children[1].children[4].translation.x = w/2;

   //set widget input params
   widget.width = w;
   widget.height = h;
   widget.set_input('width', w);
   widget.set_input('height', h);
   this.refresh_layout(true, null);
   //call core:in
   WidgetManager.corein_message(widget, 'setSize', 'width', w, 'height', w, 'dpi', screen_dpi);
  };
  obj.refresh_layout = function(send_resize, comp_target) {
   var i;
   var x, y, w, h, scale_x, scale_y;
   var comps;

   /*local to subscene transformation not known*/
   if (!this.sub_w) return;
   if (!this.sub_h) return;

   comps = widget.components;
   for (i=0; i<comps.length; i++) {
    var comp = comps[i];
    if (!comp.widget_control.component_bound) continue;

    //compute scale from Widget Manager coord system to widget internal coordinate system
    scale_x = this.sub_vp_w / this.sub_w;
    scale_y = this.sub_vp_h / this.sub_h;

    w = comp.widget_control.place_w * scale_x;
    h = comp.widget_control.place_h * scale_y;
 
    x = this.translation.x - widget.width/2 + this.sub_vp_x + comp.widget_control.place_x * scale_x + w/2;
    comp.widget_control.translation.x = x;

    y = widget.height/2 + this.translation.y - h/2 - this.sub_vp_y - comp.widget_control.place_y * scale_y;
    comp.widget_control.translation.y = y;

    if (send_resize || (comp_target==comp)) 
     comp.widget_control.set_size(w, h);
   }
  };
  
  obj.hide = function() { 
   this.scale.x = 0;
   WidgetManager.corein_message(widget, 'hide');
  };
  obj.show = function() { 
    this.scale.x = 1; 
    WidgetManager.corein_message(widget, 'show');
  };

  obj.show_remote = function () {
   if (WidgetManager.upnp && UPnP.MediaRenderersCount) {
    this.children[1].children[2].show();
   } else {
    this.children[1].children[2].hide();
   }
  };
  obj.show_remove = function(show) {
    if (show) this.children[1].children[1].show();
    else this.children[1].children[1].hide();
  };

  obj.flash = function() {
    var time = new_timeout(0.25);
    time.loop = true;
    time.ctrl = this;
    time.on_event = function(val) {
     var scale = (val<0.5) ? 1+val : 2-val;
     this.ctrl.scale.x = this.ctrl.scale.y = scale;
    };
    time.stop(1);
    time.start(0);
  };
  obj.maximized = false;
  obj.show_remote();
  obj.onClick(true);
  return obj;
}



function on_icon_over(value)
{
 infobar.set_label(value ? this.label : '');
}

function display_widget_info(wid)
{
 var info_dlg = new SFNode('Transform2D');
 var i, j, k, info;
 var y, txt, pref;

 infobar.set_label('Widget ' + wid.name + ' Information');

 info = text_rect('Close');
 info_dlg.children[0] = info;
 info.button_click = function() {
  dlg_display.children.length = 0;
  widget_display.scale.x = 1;
  infobar.set_label('');
  layout();
 };
 
 info = text_rect('Widget Metadata');
 info_dlg.children[info_dlg.children.length] = info;
 info.visible = false;
 info.button_click = function() { 
   this.visible = !this.visible;
   layout();
 };
 i=3;
 info.children[i++] = text_label('id: ' + wid.identifier + ' - shortname: '+wid.shortName + ' - name: '+wid.name, 'BEGIN');
 info.children[i++] = text_label('version: '+wid.version, 'BEGIN');
 info.children[i++] = text_label('content type: ' + wid.mainMimeType + ' - content encoding: '+wid.mainEncoding, 'BEGIN');
 info.children[i++] = text_label('default size: Width = ' + wid.defaultWidth + ' Height = '+wid.defaultHeight, 'BEGIN');
 info.children[i++] = text_label('license: '+wid.license, 'BEGIN');
 info.children[i++] = text_label('license ref: '+wid.licenseHref, 'BEGIN');
 info.children[i++] = text_label('description: '+wid.description, 'BEGIN');
 info.children[i++] = text_label('author name: '+wid.authorName + ' (mail: '+wid.authorEmail+')', 'BEGIN');
 info.children[i++] = text_label('author href: '+wid.authorHref, 'BEGIN');
 info.children[i++] = text_label('view modes: '+wid.viewmodes, 'BEGIN');
 info.children[i++] = text_label('UUID: '+wid.uuid, 'BEGIN');
 info.children[i++] = text_label('Discardable: '+wid.discardable, 'BEGIN');
 info.children[i++] = text_label('Muliple Instances: '+wid.discardable, 'BEGIN');
 var icons = wid.icons;
 for (j=0; j<icons.length; j++) {
  info.children[i++] = text_label('icon #'+(j+1)+': ' + icons[j].src, 'BEGIN');
 }

 info = text_rect('Widget Manager Info');
 info_dlg.children[info_dlg.children.length] = info;
 info.visible = false;
 info.button_click = function() { 
   this.visible = !this.visible;
   layout();
 };
 i=3;
 info.children[i++] = text_label('nb instances: '+wid.num_instances + ' nb components: '+wid.num_components, 'BEGIN' );
 info.children[i++] = text_label('Permanently installed: '+wid.permanent + ' - is component: '+wid.is_component, 'BEGIN' );
 if (wid.is_component) {
   info.children[i++] = text_label('parent widget name' + wid.parent.name, 'BEGIN');
 }
 if (wid.originating_device_ip) {
  info.children[i++] = text_label('Widget was pushed from device IP '+wid.originating_device_ip, 'BEGIN' );
 }
 info.children[i++] = text_label('Section name in GPAC config file: '+wid.section, 'BEGIN' );
 info.children[i++] = text_label('UA Locale: ' + scene.get_option('core', 'lang'), 'BEGIN');
 info.children[i++] = text_label('widget src: ' + wid.url , 'BEGIN');
 info.children[i++] = text_label('config src: ' + wid.manifest , 'BEGIN');
 info.children[i++] = text_label('content src : '+wid.localizedSrc, 'BEGIN' );

 pref = wid.features;
 info = text_rect('Features (' + pref.length + ')' ); 
 info_dlg.children[info_dlg.children.length] = info;
 info.visible = false;
 info.button_click = function() { 
   this.visible = !this.visible;
   layout();
 };
 i=3;
 for (j=0; j<pref.length; j++) {
  info.children[i++] = text_label('Feature #'+(j+1)+' name=\''+pref[j].name+'\' required=\''+pref[j].required+'\'', 'BEGIN');
 }


 pref = wid.preferences;
 info = text_rect('Preferences ('+pref.length+')'); 
 info_dlg.children[info_dlg.children.length] = info;
 info.visible = false;
 info.button_click = function() { 
   this.visible = !this.visible;
   layout();
 };
 i=3;
 for (j=0; j<pref.length; j++) {
  var val = pref[j].value;
  if (val == '') val = scene.get_option(wid.section, pref[j].name);
  info.children[i++] = text_label('Preference #'+(j+1)+' name=\''+pref[j].name+'\' value=\''+val+'\' readOnly=\''+pref[j].readonly +'\'', 'BEGIN');
 }

 info = text_rect('Migration Context', 'BEGIN'); 
 info_dlg.children[info_dlg.children.length] = info;
 info.visible = false;
 info.button_click = function() { 
   this.visible = !this.visible;
   layout();
 };
 i=3;
 txt=wid.get_context();
 while (1) {
  var idx = txt.indexOf('\n', 0);
  if (idx>0) {
   info.children[i++] = text_label(txt.substring(0, idx), 'BEGIN'); 
   txt = txt.substring(idx+1, txt.length); 
  } else {
   info.children[i++] = text_label(txt, 'BEGIN'); 
   break;
  }
 }

 info_dlg.ifce_idx = info_dlg.children.length;
 info = text_rect('Interfaces (count: ' + wid.num_interfaces + ' - bound: ' + wid.num_bound_interfaces+')', 'BEGIN'); 
 info_dlg.children[info_dlg.ifce_idx] = info;
 info.visible = false;
 info.button_click = function() { 
   this.visible = !this.visible;
   layout();
 };
 i=3;
 for (j=0; j<wid.num_interfaces; j++) {
  var idx;
  var ifce = wid.get_interface(j);
  var item = text_rect('Interface #' + (j+1) + ' type: '+ifce.type);
  info.children[i++] = item;
  item.visible = false;
  item.button_click = function() { 
    this.visible = !this.visible;
    layout();
  };
  idx=3;
  item.children[idx++] = text_label('Multiple Binding: '+ifce.multipleBinding + ' - Service provider: '+ ifce.serviceProvider + ' - bound: ' + wid.is_interface_bound(ifce) , 'BEGIN');
  for (k=0; k<ifce.num_messages; k++) {
    var string, l;
    var msg = ifce.get_message(k);
    string = '  Message #'+ (k+1) + ':   ' + msg.name + '(';
    for (l=0; l<msg.num_params; l++) {
        par = msg.get_param(l);
        string += (par.is_input ? 'in' : 'out') + ':' +par.name + ' ';
    }
    string += ')';
    item.children[idx++] = text_label(string, 'BEGIN'); 
  }
 }
 
 info_dlg.set_size = function(w, h) {
  var i, j, y, dy;
  y = h/2 - 20;

  for (i=0; i<this.children.length; i++) {
   var item = this.children[i];
   item.translation.x = 0;
   item.translation.y = y;
   item.set_size(w, 20);
   y -= 20;
   if (!i) continue;
   
   dy = 0;
   for (j=3; j<item.children.length; j++) {
    if (item.visible) {
     item.children[j].scale.x = 1;
     dy -= 20;
     item.children[j].translation.y = dy;
     if (i<this.ifce_idx) {
      item.children[j].translation.x = -w/2+10;
     } else {
      item.children[j].set_size(w-20, 20);
      var ddy=0;
      var k, sitem;
      sitem = item.children[j];
      for (k=3; k<sitem.children.length; k++) {
       if (item.children[j].visible) {
        sitem.children[k].scale.x = 1;
        sitem.children[k].translation.x = -w/2+10;
        ddy -= 20;
        sitem.children[k].translation.y = ddy;
       } else {
        sitem.children[k].scale.x = 0;
       } 
      }
      dy+=ddy;
     }
    } else {
     item.children[j].scale.x = 0;
    }
   }
   y += dy;
  }
 };

 dlg_display.children[0] = info_dlg;
 widget_display.scale.x = 0;
 layout();
}


function widget_insert(widget)
{
  /*insert the widget icon*/
  if (widget.permanent && !widget.is_component)
    insert_widget_icon(widget, 0);
    
  /*and load the widget - comment this line to disable auto load of widget*/
  widget_launch(widget);
}

function setup_icons()
{
    var icon;
      
    //Widgets Icon
    icon = icon_button('icons/applications-system.svg', 'Available Widgets', 0);
    icon.button_click = function () {
     widget_screen_visible = !widget_screen_visible;
     layout();
    };
    dock.children[0] = icon;

    //Get Widget Icon
    icon = icon_button('icons/document-save.svg', 'Get Widget', 0);
    icon.button_click = function () {
     widget_get();
    };
    dock.children[1] = icon;

    //Widgets add Icon
    icon = icon_button('icons/list-add.svg', 'Add Widgets', 0);
    icon.button_click = function () {
     widget_browse();
    };
    dock.children[2] = icon;


    icon = icon_button('icons/user-trash.svg', 'Remove all widgets', 0);
    icon.button_click = function () {
      while (1) {
        var wid = WidgetManager.get(0);
        if (wid==null) break;
        widget_close(wid, 1);
      }
      widget_screen.children.length = 0;
      layout();
    };
    dock.children[3] = icon;

    //exit Icon
    icon = icon_button('icons/emblem-unreadable.svg', 'Exit', 0);
    icon.button_click = function() { scene.exit(); };
    dock.children[4] = icon;
}



/*dock layout*/
function dock_layout() {
  var i;
      
  dock.translation.y = (dock_height - display_height)/2;
  infobar.translation.y = display_height/2 - info_height/2;
  
  num_in_dock = dock.children.length;
  tot_len = num_in_dock*icon_size;
  
  if (tot_len>display_width) {
   start_x = (icon_size-display_width)/2;
  } else {
   start_x = (icon_size-tot_len)/2;
  }
  /*translate / size all items in the dock*/
  for (i=0;i<num_in_dock; i++) {
   dock.children[i].set_size(icon_size, icon_size);
   dock.children[i].translation.x = start_x + i*icon_size;
  }
}

function widget_screen_layout(dir)
{
  var count, i, start_x, start_y, spread_x, nb_wid_h, nb_wid_v, nb_wid;
  
  if (!widget_screen_visible) {
    widget_screen.size.x = 0;
    widget_screen.size.y = 0;
    return;
  }

  widget_screen.size.x = display_width;
  widget_screen.size.y = display_height;

  start_x = (icon_size/2-display_width)/2;
  start_y = (display_height-icon_size/2)/2 - info_height;
  start_x = (icon_size-display_width)/2;
  start_y = (display_height-icon_size)/2 - icon_size/2 - info_height;
    
  count = widget_screen.children.length;
  if (first_visible_widget<0) first_visible_widget=0;
    
  for (i=0; i<count; i++) {
   var wid = widget_screen.children[i];
   wid.hide();
  }

  nb_wid_h = Math.floor(display_width / icon_size);
  if (!nb_wid_h) nb_wid_h=1;

  nb_wid_v = Math.floor((display_height-dock_height-icon_size) / icon_size);
  if (!nb_wid_v) nb_wid_v=1;
  
  spread_x = (display_width / nb_wid_h) - icon_size;
  start_x += spread_x/2;
  
  nb_wid = (nb_wid_h*nb_wid_v);
  if (dir<0) {
   first_visible_widget -= nb_wid;
   if (first_visible_widget < 0) first_visible_widget = 0;
  }
  else if (dir>0) {
   first_visible_widget += nb_wid;
  }
  
  for (i=0; i<count; i++) {
   var wid;
   if (i +first_visible_widget >= count) {
    break;
   }
   wid = widget_screen.children[i +first_visible_widget];
   wid.show();
   wid.set_size(icon_size, icon_size);
   wid.translation.x = start_x;
   wid.translation.y = start_y;
   start_x += icon_size + spread_x;
   if (start_x + icon_size / 2 >= display_width/2) {
    start_x = (icon_size-display_width)/2 + spread_x/2;
    start_y -= icon_size;
   }
   nb_widgets_on_screen = i+1;
   if (start_y - icon_size < (dock_height-display_height)/2) {
    i++;
    break;
   }
  }
}

//performs layout on all contents
function layout() {
  var i, list, start_x;

  scene.set_option('Widgets', 'LastWMWidth', '' + display_width);
  scene.set_option('Widgets', 'LastWMHeight', '' + display_height);


  if (WidgetManager.num_widgets) {
    dock.children[0].show();
  } else {
    dock.children[0].hide();
  }
  if (dlg_display.children.length) {
    widget_display.scale.x = 0;
    widget_screen_visible = false;
  }
  //layout all icons in the dock
  dock_layout();
  widget_screen_layout(0);

  if (dlg_display.children.length) {
    list = dlg_display.children;
    for (i=0; i<list.length; i++) {
     var dlg = list[i];
     if (typeof (dlg.set_size) != 'undefined') dlg.set_size(display_width, display_height-icon_size-info_height);
     dlg.translation.y = (icon_size-info_height)/2;
    }
  }
  else if (widget_screen_visible) {
    widget_display.scale.x = 0;
  } else {
    widget_display.scale.x = 1;
    list = widget_display.children;
    for (i=0; i<list.length; i++) {
     var widctrl = list[i];
     if (widctrl.maximized) {
      widctrl.translation.y = - info_height;
      widctrl.set_size(display_width, display_height - 2*info_height);
     }
    }
  }
}

//resize event callback
function on_resize(evt) {
    display_width = evt.width;
    display_height = evt.height;
    layout();
}
//zoom event callback
function on_zoom(evt) {
    display_width = evt.width;
    display_height = evt.height;
    layout();
}
//scroll event callback
function on_scroll(evt) {
  layout();
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
}
      
function widget_get_icon(widget) 
{
  var icon = 'icons/process-stop.svg';
  var preferredIconType = '.svg';
  for (var i = 0; i < widget.icons.length; i++) {
      icon = widget.icons[i].relocated_src;
      if (widget.icons[i].relocated_src.indexOf(preferredIconType) > 0) {
          break;
      }
  }
 return icon;
}

//initialize GPAC widget manager and load all widgets
function widgets_init() {
    
    count = WidgetManager.num_widgets;      
    for (i=0; i<count; i++) {
     wid = WidgetManager.get(i);
     if (wid == null) continue;
     wid.device = null;
     wid.device_ip = null;
     if (wid.in_panel == true) {
       icon = icon_button(widget_get_icon(wid), wid.name, 0);
       icon.tooltip = wid.name;
       icon.widget = wid;
       icon.button_click = on_widget_launch;
  
       wid.icon_dock = icon;
       widget_screen.children[widget_screen.children.length] = wid.icon_dock;
     }
     if (wid.visible) {
      widget_launch(wid);
     }        
    }
}

function on_widget_size(value) {
  //remember variables
  this.width = value.x;
  this.height = value.y;
  //and set widget input params
  this.set_input('width', value.x);
  this.set_input('height', value.y);
}

function on_widget_move(value) {
  this.x = value.x;
  this.y = value.y;
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
      widget.scene_container.removeChildren[0] = widget.widget_control;
      /*force disconnect of main resource - we do this because we are not sure when the widget_control will be destroyed due to JS GC*/
      widget.widget_control.inline.url.length = 0;
    }
    if (!is_comp && (!widget.permanent || force_remove)) {
      WidgetManager.unload(widget, false);
    }
}
   
function on_widget_close(value) {
    widget_close(this, 0);
}
   
//widget remove function (close and unregister)
function widget_remove(wid) {
  if (typeof(wid.icon_dock) != 'undefined')
    widget_screen.removeChildren[0] = wid.icon_dock;
  widget_close(wid, 0);
  layout();
}

function on_widget_control(value)
{
 var i, count;
 if (!value) return;
 count = this.scene_container.children.length;
 for (i=0; i<count; i++) {
  if (this.scene_container.children[i] == this.widget_control) {
   this.scene_container.removeChildren[0] = this.widget_control;
   this.scene_container.children[this.scene_container.children.length] = this.widget_control;
   return;
  }
 }
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
   widg_ctrl.component_bound = false;

   wid.visible = true;

   widg_ctrl.set_size(wid.width, wid.height);
   widg_ctrl.translation.x = wid.x;
   widg_ctrl.translation.y = wid.y;
    
   wid.widget_control = widg_ctrl;
   wid.scene_container = widget_display;

   widg_ctrl.show_remove( (!wid.discardable && wid.icon_dock) ? 1 : 0);

   widg_ctrl.sub_width = 0;
   widg_ctrl.sub_height = 0;
   widg_ctrl.sub_x = 0;
   widg_ctrl.sub_y = 0;
   widg_ctrl.sub_vp_w = 0;
   widg_ctrl.sub_vp_h = 0;

   widg_ctrl.inline.addEventListener('gpac_vp_changed', 
                                  function(evt) { 
                                   widg_ctrl.sub_vp_w = evt.width;
                                   widg_ctrl.sub_vp_h = evt.height;
                                   widg_ctrl.sub_vp_x = evt.offset_x;
                                   widg_ctrl.sub_vp_y = evt.offset_y;
                                   widg_ctrl.sub_w = evt.vp_width;
                                   widg_ctrl.sub_h = evt.vp_height;
                                   widg_ctrl.refresh_layout(true, null);
                                  }, 
                                0);
    
   /*this will setup the scene graph for the widget in order to filter input and output communication pins*/
   wid.activate(widg_ctrl.inline);

   widg_ctrl.inline.url[0] = wid.main;
   widget_display.addChildren[0] = widg_ctrl;

    /*send notifications once the widget scene is loaded*/
    wid.on_load = function () {
      WidgetManager.corein_message(this, 'activate');
      WidgetManager.corein_message(this, 'show');
      WidgetManager.corein_message(this, 'setSize', 'width', 50, 'height', 50, 'dpi', 96);
    };

  if (widget_screen_visible) {
   widget_screen_visible = 0;
   layout();
  }
    //
    if (log_level > l_inf) {
        var i = 0;
        alert(">>>>>>>>>>>>> "+wid.name+" interfaces:");
        for (;i < wid.num_interfaces; i++) {
            alert(""+wid.get_interface(i).type);
        }
    }
    //
}


//core out install widget implementation JCD
function widget_install_widget(widget, args)
{
    var uri = args[0], j;
    var count = WidgetManager.num_widgets;
    for (j=0; j<count; j++) {
        var wid = WidgetManager.get(j);
        if (wid.url==uri) break;
    }
    if (j==count) {
        new_wid = WidgetManager.open(uri, null);
        if (new_wid!=null) {
            insert_widget_icon(new_wid, 1);
        }
    }
}

//core out migrate component implementation JCD
function widget_migrate_component(widget, args)
{
    if (has_upnp && UPnP.MediaRenderersCount) {
        widget_remote_candidate = widget.get_component(args[0]);
        if (widget_remote_candidate==null) {
         log(l_err, 'Component '+args[0]+' cannot be found in widget '+widget.name);
         return;
        }
        if (args.length > 1 && args[1] != null) {
            var render = WidgetManager.MPEGUStandardServiceProviders[parseInt(args[1])];
            WidgetManager.migrate_widget(render, widget_remote_candidate);
            widget_close(widget_remote_candidate, 0);
            widget_remote_candidate = null;
        } else {
            on_upnpopen();
        }
    }
}

// core out request migration targets JCD
function widget_request_migration_targets(wid, args)
{
    var count = WidgetManager.MPEGUStandardServiceProviders.length,
        codes = new Array(), names = new Array(), descriptions = new Array(), i;
    for (i = 0; i < count; i++) {
        var render = WidgetManager.MPEGUStandardServiceProviders[i];
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

function widget_request_size(widget, args)
{
  if (args.length==2) {
   w = (typeof args[0] == 'string') ? parseInt(args[0]) : args[0];
   h = (typeof args[1] == 'string') ? parseInt(args[1]) : args[1];
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
  widget_display.removeChildren[0] = widget.widget_control;
  widget_display.addChildren[0] = widget.widget_control;
  widget.widget_control.flash();
 }
}

function widget_request_notification(widget, args)
{
 var notif = text_rect('');
 notif.children[1].geometry.string[0] = 'Notification from widget';
 notif.children[1].geometry.string[1] = ' '+widget.name;
 notif.children[1].geometry.string[2] = ' ';
 notif.children[1].geometry.string[3] = args[0];
 dlg_display.children[0] = notif;
 notif.set_size(320, 240);
 notif.button_click = function() {
  dlg_display.removeChildren[0] = this;
 } 
}

function widget_place_component(widget, args)
{
 var comp = widget.get_component(args[0]);
 
 if (comp==null) {
  log(l_err, 'Component '+args[0]+' cannot be found in widget '+widget.name);
  return;
 }
 comp.widget_control.place_x = args[1];
 comp.widget_control.place_y = args[2];
 comp.widget_control.place_w = args[3];
 comp.widget_control.place_h = args[4];
 comp.widget_control.place_z = args[5];
 comp.widget_control.component_bound = true;
 widget.widget_control.refresh_layout(false, comp);
}

 
     
function insert_widget_icon(new_wid, no_layout) {
 var icon;
 icon = icon_button(widget_get_icon(new_wid), new_wid.name, 0);
 icon.tooltip = new_wid.name;
 new_wid.in_panel = true;
 new_wid.visible = false;
 new_wid.icon_dock = icon;
 icon.button_click = on_widget_launch;
 icon.widget = new_wid;
 widget_screen.addChildren[0] = new_wid.icon_dock; 
 if (!no_layout) layout();    
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
      insert_widget_icon(new_wid, 1);
     }
    }
   }
  }
}

function widget_browse()
{
  filebrowse = new SFNode('Transform2D');
  infobar.set_label('Select widget');
  dlg_display.children[0] = filebrowse;
  
  filebrowse.children[0] = icon_button('icons/emblem-unreadable.svg', 'Close', 0);
  filebrowse.children[0].button_click = function() {
   dlg_display.children.length = 0;
   widget_display.scale.x = 1;
   layout();
  }
  
  filebrowse.children[1] = icon_button('icons/go-previous.svg', 'Previous', 0);
  filebrowse.children[1].filebrowse = filebrowse;
  filebrowse.children[1].button_click = function () { this.filebrowse.layout(0) };
  
  filebrowse.children[2] = icon_button('icons/go-next.svg', 'Next', 0);
  filebrowse.children[2].filebrowse = filebrowse;
  filebrowse.children[2].button_click = function () { this.filebrowse.layout(1) };
  
  filebrowse.children[3] = icon_button('icons/go-up.svg', 'Up', 0);
  filebrowse.children[3].filebrowse = filebrowse;
  filebrowse.children[3].button_click = function () { this.filebrowse.browse(true) };
  
  filebrowse.children[4] = icon_button('icons/Folder.svg', 'Scan Directory', 0);
  filebrowse.children[4].filebrowse = filebrowse;
  filebrowse.children[4].button_click = function () {
   scan_directory(this.filebrowse.directory);
   WidgetManager.last_widget_dir = this.filebrowse.directory;
   dlg_display.children.length = 0;
   widget_display.scale.x = 0;
   widget_screen_visible = true;
   layout();
  }
  filebrowse.children[5] = text_label('', 'BEGIN');
  filebrowse.set_label = function(label) {
    filebrowse.children[5].set_label(label);
  }
  filebrowse.nb_tools = filebrowse.children.length;

  filebrowse.browse = function(go_up) {
   this.list = Sys.enum_directory(this.directory, '*.xml;*.wgt;*.mgt', go_up);
   if (this.list.length) {
    this.directory = this.list[0].path;
    this.set_label(this.directory);
   } else {
    this.set_label('');
   }
   this.first = 0;
   this.layout(0);
  }
  filebrowse.layout = function(type) {
   var w, h, i, y;
   this.children.length = this.nb_tools;

   this.children[1].hide();
   this.children[2].hide();
   if (this.directory == '') 
    this.children[3].hide();
   else
    this.children[3].show();

   if (type==0) {
    this.first -= this.nb_items;
    if (this.first<0) this.first = 0;
   }
   else if (type) {
    this.first += this.nb_items;
    if (this.first + this.nb_items > this.list.length) this.first = this.list.length - this.nb_items;
   }
   if (this.first) this.children[1].show();
   if (this.first+this.nb_items < this.list.length) this.children[2].show();

   for (i=0; i<this.nb_items; i++) {
    var item;
    if (i+this.first>=this.list.length) break;
    item = text_rect(this.list[i+this.first].name);
    item.path = this.list[i+this.first].path;
    item.name = this.list[i+this.first].name;
    item.directory = this.list[i+this.first].directory;
    item.filebrowse = this;
    item.button_click = function() {
     if (this.directory) {
      this.filebrowse.directory = this.path + this.name;
      this.filebrowse.browse(false);
     } else {
      var value = this.path + this.name;
      dlg_display.children.length = 0;
      widget_display.scale.x = 1;
      widget_screen_visible = true;
      layout();

      var new_wid = WidgetManager.open(value, null);
      if (new_wid==null) return;

      WidgetManager.last_widget_dir = this.filebrowse.directory;
      insert_widget_icon(new_wid, 0);
     }
    }
    this.children[this.nb_tools+i] = item;
   }   
   this.set_size(this.width, this.height);
  }
  
  
  filebrowse.set_size = function(w, h) {
   var i, x, y, isize, nbi;
   isize = 24;
   if (w>display_width - isize) w = display_width - isize;

   this.width = w;
   this.height = h;

   i = 1;
   while ((i+1)*isize <= h - isize) i++;
   
   if (i != this.nb_items) {
    this.nb_items = i;
    this.layout(0);
    return;
   }
   x = -w/2 + isize/2;
   y = h/2 - isize/2;

   for (i=0;i<this.nb_tools;i++) {
     if (this.nb_tools>i+1) {
       this.children[i].set_size(isize, isize);
     }
     this.children[i].translation.x = x;
     this.children[i].translation.y = y;
     x += isize;
   }
   y-=isize;
   while (i<this.children.length) {
     this.children[i].set_size(w, isize);
     this.children[i].translation.x = 0;
     this.children[i].translation.y = y;
     y-=isize;
     i++;
   }
  }  
  filebrowse.nb_items = 0;
  filebrowse.directory = WidgetManager.last_widget_dir;
  filebrowse.browse(0);

  widget_display.scale.x = 0;
  layout();
}

function onMediaRendererAdd(name, uuid, is_add)
{
 var i, count;

 count = WidgetManager.num_widgets;
 for (i=0; i<count; i++) {
  wid = WidgetManager.get(i);
  if (wid == null) continue;
  if (!wid.widget_control) continue;
  wid.widget_control.show_remote();
 }

 if (upnp_renders) upnp_renders.refresh();
}

function on_upnpopen()
{
  upnp_renders = new SFNode('Transform2D');
  upnp_renders.nb_items = 0;
  upnp_renders.refresh = function () {
    var i, count, render, item, start_y, w;
    this.children.length = 0;
    count = WidgetManager.MPEGUStandardServiceProviders.length;
    if (count+1>this.nb_items) count = this.nb_items-1;

    item = text_rect('Close');
    item.button_click = function() {
     dlg_display.children.length = 0;
     upnp_renders=null;
     widget_display.scale.x = 1;
     infobar.set_label('');
    }
    this.children[this.children.length] = item;

    for (i=0; i<count; i++) {
      render = WidgetManager.MPEGUStandardServiceProviders[i];
      item = text_rect(render.Name);
      item.render = render;
  
      item.button_click = function() {
       dlg_display.children.length = 0;
       upnp_renders=null;
       widget_display.scale.x = 1;
       infobar.set_label('');
  
       WidgetManager.migrate_widget(this.render, widget_remote_candidate);
       widget_close(widget_remote_candidate, 0);
       widget_remote_candidate = null;   
      }  
      this.children[this.children.length] = item;
    }
    this.set_size(this.width, this.height);
  }

  upnp_renders.set_size = function (w, h) {
    var i, count, start_y, w;   

    this.width = w<300 ? w : 300;
    this.height = h;
    
    i = 1;
    while ((i+1)*icon_size <= h - icon_size) i++;
    if (i != this.nb_items) {
      this.nb_items = i;
      this.refresh();
      return;
    }
    count = this.children.length;
    start_y = this.height/2 - icon_size - 4;
    for (i=0; i<count; i++) {
      this.children[i].set_size(this.width, icon_size);
      this.children[i].translation.y = start_y;
      start_y-=icon_size+2;
    }
  }

  infobar.set_label('Select remote display');
  dlg_display.children[0] = upnp_renders;
  widget_display.scale.x = 0;
  layout();
}


function onMediaConnect(url, src_ip)
{
 if (WidgetManager.probe(url)) {
  var new_wid = WidgetManager.open(url, src_ip);
  if (new_wid==null) {
   return;
  }
  widget_insert(new_wid); 
 }
}

// widget get: UI for listWidgets and getWidget
// added by Jean-Claude Dufourd, modeled on filebrowse
function widget_get() {
    alert("widget_get");
    widgetbrowse = new SFNode('Transform2D');
    infobar.set_label('Select widget manager');
    dlg_display.children[0] = widgetbrowse;

    widgetbrowse.children[0] = icon_button('icons/emblem-unreadable.svg', 'Close', 0);
    widgetbrowse.children[0].button_click = function() {
        dlg_display.children.length = 0;
        widget_display.scale.x = 1;
        layout();
    };

    widgetbrowse.children[1] = text_label('', 'BEGIN');
    widgetbrowse.set_label = function(label) {
        widgetbrowse.children[1].set_label(label);
    };

    widgetbrowse.nb_tools = widgetbrowse.children.length;

    // this function builds the whole UI the first time
    widgetbrowse.layout = function() {
        alert("widgetbrowse.layout");
        var w, h, i, y;
        this.children.length = this.nb_tools;
        for (i = 0; i < this.nb_items; i++) {
            var item;
            if (i >= this.list.length) {
                break;
            }
            item = text_rect(this.list[i].Name);
            item.wm = this.list[i];
            item.widgetbrowse = this;
            item.button_click = function() {
                alert("click to select wm: "+this.wm.Name);
                this.widgetbrowse.wm = this.wm;
                this.wm.standardService.SetActionListener("listWidgets", get_widget_callback(this.wm, this.widgetbrowse), true);
                this.wm.standardService.CallAction("listWidgets", new Array());
            };
            this.children[this.nb_tools + i] = item;
        }
        this.set_size(this.width, this.height);
    };

    widgetbrowse.set_size = function(w, h) {
        alert("widgetbrowse.set_size "+w+" "+h);
        var i, x, y, isize, nbi;
        isize = 24;
        if (w > display_width - isize) {
            w = display_width - isize;
        }
        this.width = w;
        this.height = h;
        i = 1;
        while ((i + 1) * isize <= h - isize) {
            i++;
        }
        if (i != this.nb_items) {
            this.nb_items = i;
            this.layout(0);
            return;
        }
        x = -w / 2 + isize / 2;
        y = h / 2 - isize / 2;
        for (i = 0; i < this.nb_tools; i++) {
            if (this.nb_tools > i + 1) {
                this.children[i].set_size(isize, isize);
            }
            this.children[i].translation.x = x;
            this.children[i].translation.y = y;
            x += isize;
        }
        y -= isize;
        while (i < this.children.length) {
            this.children[i].set_size(w, isize);
            this.children[i].translation.x = 0;
            this.children[i].translation.y = y;
            y -= isize;
            i++;
        }
    };

    widgetbrowse.nb_items = WidgetManager.MPEGUStandardServiceProviders.length;
    widgetbrowse.list = WidgetManager.MPEGUStandardServiceProviders;
    widgetbrowse.set_label('Select Widget Manager');
    widgetbrowse.layout(0);
    widget_display.scale.x = 0;
    layout();
}

function get_widget_callback(device, widgetbrowse) {
    alert("get_widget_callback "+device.Name+" "+widgetbrowse);
    return function() {
        // msgHandler is the first argument, the next arguments are from the reply to listWidgets
        var act = arguments[0];
        var act1 = act.GetArgumentValue("widgetCodes");
        var act2 = act.GetArgumentValue("widgetNames");
        alert("callback |"+act1+"| |"+act2+"|");
        widgetbrowse.set_label("Select Widget");
        var w, h, i, y;
        act1 = act1.split(" ");
        act2 = act2.split(" ");
        widgetbrowse.children.length = widgetbrowse.nb_tools;
        for (i = 0; i < act2.length; i++) {
            if (act1[i]=="" || act2[i]=="") continue;
            var item = text_rect(act2[i]);
            item.widgetCode = act1[i];
            item.widgetName = act2[i];
            item.widgetbrowse = widgetbrowse;
            item.button_click = function() {
                alert("selected widget: "+this.widgetName);
                dlg_display.children.length = 0;
                widget_display.scale.x = 1;
                widget_screen_visible = true;
                layout();
                var arr = new Array();
                arr[0] = "widgetCode";
                arr[1] = this.widgetCode;
                this.widgetbrowse.wm.standardService.SetActionListener("getWidget", get_widget_callback2, true);
                this.widgetbrowse.wm.standardService.CallAction("getWidget", arr);
            };
            widgetbrowse.children[this.nb_tools + i] = item;
        }
        widgetbrowse.set_size(widgetbrowse.width, widgetbrowse.height);
    }
}

function get_widget_callback2() {
    // msgHandler is the first argument, the next arguments are from the reply to listWidgets
    alert("callback2-1");
    var act = arguments[0];
    var act1 = act.GetArgumentValue("widgetUrl");
    var act2 = act.GetArgumentValue("widgetContext");
    alert("callback2-2 " + act1 + " " + act2);
    var wid = WidgetManager.load(act1, null, act2);
    WidgetManager.on_widget_add(wid);
}
