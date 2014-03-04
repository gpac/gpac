/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////


/*log function*/
function log(lev, str) {
    if (lev <= log_level) alert('[WM] ' + str);
}

/*log levels*/
l_err = 0;
l_war = 1;
l_inf = 2;
l_deb = 3;

/*default log level*/
log_level = l_inf;

dictionary = null;
top_wnd = null;
controlled_renderer = null;
movie_connected = false;
UPnP_Enabled = false;
browser_mode = false;
upnp_renderers = null;
current_url = '';
current_duration = 0.0;
current_time = 0.0;
player_control = null;
icon_pause=1;
icon_play=0;
max_playercontrol_width=1024;
dynamic_scene=1;
screen_width = 0;
screen_height = 0;

GF_STATE_PLAY=0;
GF_STATE_PAUSE=1;
GF_STATE_STOP=2;
GF_STATE_TRICK=3;

all_extensions = [];

function new_extension()
{
 var obj = new Object();
 obj.built_in = false;
 obj.upnp_add = function(name, uuid, is_add) {};
 all_extensions.push(obj);
 return obj;
}

function on_movie_duration(value)
{
  if (value<0) value=0;
  current_duration = value;
  player_control.set_duration(value);
  if (UPnP_Enabled) UPnP.MovieDuration = value;
}

function on_movie_active(value)
{
  if (!value) {
   movie_ctrl.mediaStartTime = -1; 
  }
}

function on_movie_time(value)
{
  var diff = current_time - value;
  if (diff<0) diff = -diff;
  /*filter out every 1/2 seconds*/
  if (diff < 0.5) return;
  current_time = value;
  player_control.set_time(value);
  if (UPnP_Enabled) UPnP.MovieTime = value;
}


in_drag = false;
start_drag_x=start_drag_y=0;

function filter_event(evt)
{
 if (top_wnd && top_wnd.on_event(evt) ) return true;

 switch (evt.type) {
 case GF_EVENT_MOUSEDOWN:
  if (evt.picked) return false;
  start_drag_x = evt.mouse_x;
  start_drag_y = evt.mouse_y;
  if (gpac.navigation!=GF_NAVIGATE_NONE) return false;
  in_drag = true;
  return true;
 case GF_EVENT_MOUSEMOVE:
  if (in_drag) {
    gpac.move_window(evt.mouse_x - start_drag_x, evt.mouse_y - start_drag_y);
    return true;
  }
  return false;
  
 case GF_EVENT_OPENFILE:
  var files = evt.files;
  /*todo - handle playlist*/
  if (files.length) {
   set_movie_url(files[0]);
  }
  return true;
  
 case GF_EVENT_MOUSEUP:
  in_drag = false;
  if (evt.picked) return false;
  if ((start_drag_x == evt.mouse_x) && (start_drag_y == evt.mouse_y)) {
/*
   if (dock.visible) {
    show_dock(false);
   } else 
*/   
   {
    show_dock(false);
    if (player_control.visible) {
      //player_control.hide();
      top_wnd = null;
    } else {
      player_control.show();
      top_wnd = player_control;
    }
   }
   return false;
  }
  return false;
 case GF_EVENT_KEYUP:
//  alert(evt.keycode);
  //commented out as HOME is used for viewpoint reset
/*
  if (evt.keycode=='Home') {
    show_dock(!dock.visible);
    return true;
  }
*/
  if (evt.keycode=='Up') {
   if (dock.visible) return false;
    if (player_control.visible) {
      player_control.hide();
      top_wnd = null;
    } else {
      player_control.show();
      top_wnd = player_control;
    }
    return true;
  }
  return false;
 case GF_EVENT_KEYDOWN:
  if (evt.keycode=='Left') {
   gpac.set_focus('previous');
   return true;
  }
  else if (evt.keycode=='Right') {
   gpac.set_focus('next');
   return true;
  }
  return false;
 case GF_EVENT_NAVIGATE_INFO:
  //alert('Navigate to '+evt.target_url);
  return true;
 case GF_EVENT_NAVIGATE:
  set_movie_url(evt.target_url);
  return true;
 default:
  return false;
 } 
 return false;
}


function show_dock(show)
{
 if (show) {
  dock.show();
  if (movie_connected) {
   dock.set_size(display_width, display_height/2);
   dock.move(0, -display_height/4);
   movie.scale.x = movie.scale.y = 0.5;
   movie.translation.y = display_height/4;
  } else {
   dock.set_size(display_width, display_height);
   dock.move(0, 0);
  }
  dock.layout();
  player_control.hide();
//  set_movie_url('');
  top_wnd = dock;
//  uidisplay.hide();
 } else {
  movie.scale.x = movie.scale.y = 1;
  movie.translation.y = 0;
  dock.hide();
  player_control.hide();
  top_wnd = null;
  if (uidisplay.children.length && typeof uidisplay.children[uidisplay.children.length-1].on_event != 'undefined') {
   top_wnd = uidisplay.children[uidisplay.children.length-1];
  }
 }
}

function gpacui_insert_dock_icon(label, icon)
{
  if (1) {
    var wnd = gw_new_window(dock, true, 'offscreen');
    var icon = gw_new_icon_button(wnd, icon, label, 'osdbutton');
    wnd.set_size(icon.width, icon.height);
    wnd.show();
    dock.set_size(display_width, display_height);
    return icon;
  } else {
    var icon = gw_new_icon_button(dock, icon, label, 'osdbutton');
    dock.set_size(display_width, display_height);
    return icon;
  }
}


function gpacui_show_window(obj)
{
 gw_add_child(uidisplay, obj);
  if (!obj.width || !obj.height) {
   obj.set_size(200, 200);
  }
 obj.show();
 top_wnd = null;
 if (uidisplay.children.length && typeof uidisplay.children[uidisplay.children.length-1].on_event != 'undefined') {
  top_wnd = uidisplay.children[uidisplay.children.length-1];
 }
 layout();
}
 
function compute_movie_size(width, height)
{
   var w, h, r_w, r_h;
   if (!width || !height) return;

   if (width > gpac.screen_width) width = gpac.screen_width;
   if (height > gpac.screen_height) height = gpac.screen_height;
   w = width;
   h = height;
   r_w = r_h = 1;
   if (w < min_width) r_w = Math.ceil(min_width/w);
   if (h < min_height) r_h = Math.ceil(min_height/h);
   if (r_w < r_h) r_w = r_h;
   w = r_w * w;
   h = r_w * h;
   gpac.set_size(w, h);
}

//Initialize the main UI script
function initialize() {
    //var icon;
    var i, count, wid;


    gpac.caption = 'Osmo4';
    current_time = 0;

    min_width = 160;
    min_height = 80;
    
    /*load the UI lib*/
    Browser.loadScript('gwlib.js', false);

    browser_mode = gpac.getOption('Temp', 'BrowserMode');
    if (browser_mode && (browser_mode=='yes')) {
     browser_mode = true;
    } else {
     browser_mode = false;
    }

    
//    gwskin.tooltip_callback = function(over, label) { alert('' + over ? label : ''); };

    root.children[0].backColor = gwskin.back_color;
    movie.children[0].on_size = function(evt) {
      if (!movie_connected) {
         movie_connected = true;
         gpac.set_3d(evt.type3d ? 1 : 0);
         player_control.play.switch_icon(icon_pause);
         dynamic_scene = evt.dynamic_scene;
      }
      if (!gpac.fullscreen) {
       compute_movie_size(evt.width, evt.height);
      }
    }

    movie.children[0].on_media_progress = function(evt) {
     if (!current_duration) return;
     /*this is not conform to HTML5, we're using the old MediaAccessEvent syntax ...*/
     var percent_dload = 100.0 * evt.loaded / evt.total;
     var percent_playback = 100.0 * current_time / current_duration;
     //alert('URL data ' + percent_dload + ' - ' + percent_playback + ' playback');     
    }

    movie.children[0].on_media_playing = function(evt) {
     player_control.play.switch_icon(icon_pause);
    }
    movie.children[0].on_media_end = function(evt) {
     if (player_control.duration && movie_ctrl.loop) {
      movie_ctrl.mediaStartTime = 0;
      current_time=0; 
     }
    }
    
    movie.children[0].addEventListener('gpac_scene_attached', movie.children[0].on_size, 0);
    movie.children[0].addEventListener('progress', movie.children[0].on_media_progress, 0);
    movie.children[0].addEventListener('playing', movie.children[0].on_media_playing, 0);
    movie.children[0].addEventListener('canplay', movie.children[0].on_media_playing, 0);
    movie.children[0].addEventListener('ended', movie.children[0].on_media_end, 0);

    movie.children[0].on_media_waiting = function(evt) {
     alert('URL is now buffering');     
    }
    movie.children[0].addEventListener('waiting', movie.children[0].on_media_waiting, 0);

    
    display_width = parseInt( gpac.getOption('General', 'LastWidth') );
    display_height = parseInt( gpac.getOption('General', 'LastHeight') );
    
    if (!gpac.fullscreen && (!display_width || !display_height)) {
     display_width = 320;
     display_height = 240;
    }
    
    if (!gpac.fullscreen && display_width && display_height) {
     gpac.set_size(display_width, display_height);
    } else {
     display_width = gpac.get_screen_width();
     display_height = gpac.get_screen_height();
    }
    screen_dpi = gpac.get_horizontal_dpi();


    dictionary = gw_new_container(root);
    dictionary.hide();
    
    //request event listeners on the window - GPAC specific BIFS extensions !!!     
    root.addEventListener('resize', on_resize, 0);
    root.addEventListener('zoom', on_zoom, 0);
    root.addEventListener('scroll', on_scroll, 0);


    Browser.addRoute(movie_sensor, 'mediaDuration', movie_sensor, on_movie_duration); 
    Browser.addRoute(movie_sensor, 'mediaCurrentTime', movie_sensor, on_movie_time); 
    Browser.addRoute(movie_sensor, 'isActive', movie_sensor, on_movie_active); 
    
    scene_width = 0;

    /*init UPnP*/
  	controlled_renderer = null;
  	UPnP_Enabled = eval("(typeof(UPnP) != 'undefined');");
  	if (UPnP_Enabled) {
  		UPnP.onMediaRendererAdd = onMediaRendererAdd;
  		UPnP.onMediaConnect = onMediaConnect;
  		UPnP.onMediaStop = onMediaStop;
  		UPnP.onMediaPause = onMediaPause;
  		UPnP.onMediaPlay = onMediaPlay;
  		UPnP.onMediaSeek = OnMediaSeek;
  		UPnP.onMigrate = OnMediaMigrate;
  		UPnP.onMediaTimeChanged = onMediaTimeChanged;
  		UPnP.onMediaDurationChanged = onMediaDurationChanged;
  		UPnP.BindRenderer();
  		UPnP.MovieURL = '';
  		UPnP.MovieDuration = 0.0;
  		UPnP.MovieTime = 0.0;
  	}
            
    dock = gw_new_grid_container(ui_root);
    dock.spread_h = true;
    dock.hide();

    player_control = new_player_control(ui_root);
    player_control.hide();

    uidisplay = gw_new_container();
    uidisplay._name = 'Root Display';
    gw_add_child(ui_root, uidisplay);
    
    uidisplay.remove_child = function(child) {
     this.removeChildren[0] = child;
     top_wnd = null;
     if (this.children.length && typeof this.children[this.children.length-1].on_event != 'undefined') {
      top_wnd = this.children[this.children.length-1];
     }
     layout();
    }


    /*init our internal extensions*/
    var icon = gpacui_insert_dock_icon('Player', 'icons/applications-multimedia.svg');
    icon.on_click = function () { show_dock(false); player_control.show(); };
    
    if (UPnP_Enabled) {
      icon = gpacui_insert_dock_icon('Control', 'icons/video-display.svg');
      icon.on_click = function () { show_dock(false); select_remote_display('select'); };

      icon = gpacui_insert_dock_icon('Remote', 'icons/applications-internet.svg');
      icon.on_click = function () { set_movie_url(''); show_dock(false); browse_remote_servers('select'); };
    }

    /*init all extensions*/
    var list = gpac.enum_directory('extensions', '*', 0);

    for (i=0; i<list.length; i++) {
      if (!list[i].directory) continue;
      var extension = new_extension();
      extension.path = 'extensions/' + list[i].name+'/';

      if (list[i].name.indexOf('.')==0) continue;
      
      if ( Browser.loadScript('extensions/'+list[i].name+'/init.js', true) == false) continue;
      if (!setup_extension(extension)) {
        log(l_inf, 'UI extension '+list[i].name + ' is disabled');
        continue;
      }      
      log(l_inf, 'Loading UI extension '+list[i].name + ' icon '+ extension.path+extension.icon);
      
      if (extension.icon && extension.launch) {
       var icon = gpacui_insert_dock_icon(extension.label, extension.path+extension.icon);
       icon.extension = extension;
       icon.on_click = function () { show_dock(false); this.extension.launch(this.extension); }
      }
      if (extension.initialize) extension.initialize(extension);
    }

    current_url = '';
    //let's do the layout   
    layout();
    gpac.set_event_filter(filter_event);

    var url = gpac.getOption('Temp', 'GUIStartupFile');
    if (url) {
      if (url.indexOf('://')<0) set_movie_url('gpac://'+url);
      else set_movie_url(url);
    } else {
        player_control.show();
  }
}


function set_movie_url(url)
{  
  if ((url=='') || (url==current_url)) {
    movie.children[0].url[0] = url;
    movie_ctrl.url[0] = url;
    movie_sensor.url[0] = url;    
    if (UPnP_Enabled) UPnP.MovieURL = url;
    movie_connected = (url=='') ? false : true;
    root.children[0].set_bind = movie_connected ? FALSE : TRUE;
  } else if (controlled_renderer==null) {
    /*connect a new resource to the destination - if success, switch resources*/
    var test_resource = new SFNode('Inline');

    test_resource.callback_done = false;
    test_resource.on_attached = function(evt) {

      this.callback_done = true;
      current_url = this.url[0];
      
      /*process the error or connect service*/
      if (evt.error) {
        var notif = gw_new_message(null, 'Error!', 'Failed to open\n'+this.url[0]+ '\nReason: '+gpac.error_string(evt.error) );
        gpacui_show_window(notif);
      } else {
        movie.children[0].url[0] = current_url;
        movie_ctrl.mediaSpeed = 1;
        movie_ctrl.mediaStartTime = 0;
        movie_ctrl.url[0] = current_url;
        movie_sensor.url[0] = current_url;
        root.children[0].set_bind = FALSE;

        movie.children[0].on_size(evt);
      }
      /*destroy the resource node*/
      this.url.length = 0;
      gw_detach_child(this);
      test_resource.removeEventListener('gpac_scene_attached', test_resource.on_attached, 0);
      this.on_attached = null;
      
    }

    /*get notified when service loads or fails*/
    test_resource.addEventListener('gpac_scene_attached', test_resource.on_attached, 0);

    /*handle navigation between local files*/    
    if ((current_url.indexOf('gpac://')>=0) && (url.indexOf('://')<0)) {
      test_resource.url[0] = "gpac://"+url;
    } else {
      test_resource.url[0] = url;
    }
    /*URL assignment may trigger synchronous replies*/
    if (!test_resource.callback_done) {
      gw_add_child(dictionary, test_resource);  
    }

    return;
    
  } else {
    var uri = UPnP.ShareResource(url);
    controlled_renderer.Open(uri); 
  }
  current_url = url;
  current_time = 0;
  
  if (url == '') player_control.show();
  else player_control.hide();

}

//performs layout on all contents
function layout() 
{
  var i, list, start_x, w;
  w = display_width;
  if (max_playercontrol_width && (w>max_playercontrol_width)) w=max_playercontrol_width;
  player_control.set_size(w, player_control.height);

  
  dock.set_size(display_width, display_height);
//  dock.set_size(display_width, display_height);
//  if (uidisplay.children.length) uidisplay.children[0].set_size(display_width, display_height);
}

//resize event callback
function on_resize(evt) {
  if ((display_width == evt.width) && (display_height == evt.height)) return;
  if (evt.width <=100) {
    gpac.set_size(100, display_height);
    return;
  }
  if (evt.height <=80) {
    gpac.set_size(display_width, 40);
    return;
  }

  display_width = evt.width;
  display_height = evt.height;
  if (!gpac.fullscreen) {
    gpac.setOption('General', 'LastWidth', ''+display_width);
    gpac.setOption('General', 'LastHeight', ''+display_height);
  }
  layout();

  for (var i in all_extensions) {
   if (typeof all_extensions[i].on_resize != 'undefined') {
      all_extensions[i].on_resize(display_width, display_height);
   }
  }
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


function new_player_control(container)
{ 
  var control_icon_size = gwskin.default_icon_size;
  var small_control_icon_size;
  var wnd = gw_new_window(container, true, 'osdwindow');
  wnd.connected = false;
  this.stoped_url = null;

  small_control_icon_size = control_icon_size;

  wnd.set_corners(true, true, false, false);

  /*first set of controls*/
  wnd.infobar = gw_new_grid_container(wnd);
  wnd.infobar.spread_h = true;
  
  /*add our controls in order*/
  if (1) {
    wnd.snd_low = gw_new_icon_button(wnd.infobar, 'icons/audio-volume-low.svg', 'Lower', 'icon');
    wnd.snd_ctrl = gw_new_slider(wnd.infobar);
    wnd.snd_low.add_icon('icons/audio-volume-muted.svg');
    wnd.muted = 0;
    wnd.snd_low.on_click = function() {
     if (player_control.muted) {
      gpac.volume = player_control.muted;
      player_control.muted = 0;
      this.switch_icon(0);
     } else {
      player_control.muted = gpac.volume ? gpac.volume : 1;
      gpac.volume = 0;
      this.switch_icon(1);
     }
    }
    wnd.snd_ctrl.on_slide = function(value, type) {
     if (player_control.muted) player_control.snd_low.on_click();
     gpac.volume = value;
    }
    wnd.snd_low.set_size(small_control_icon_size, small_control_icon_size);
    wnd.snd_ctrl.set_size(2*small_control_icon_size, 2, control_icon_size/3, control_icon_size/3);
    wnd.snd_ctrl.set_value(gpac.volume);  
  } else {
    wnd.snd_low = null;
    wnd.snd_ctrl = null;
  }
  
  
  wnd.set_state = function(state) {
   if (!movie_connected && !controlled_renderer) return;

   if (state==this.state) return;
   
   if (state == GF_STATE_STOP) {
     this.stoped_url = ''+current_url;
     if (controlled_renderer) controlled_renderer.Stop();
     else {
      set_movie_url(''); 
      /*override movie_connected to avoid auto-resizing*/
      movie_connected = true;
     }
     movie_ctrl.mediaStartTime = 0; 
     this.media_line.set_value(0);
     this.play.switch_icon(icon_play);
     this.state = GF_STATE_STOP;
     return;
   }
   if (state==GF_STATE_PAUSE) {
    if (this.state==GF_STATE_STOP) return;    
    if (controlled_renderer) controlled_renderer.Pause();
    movie_ctrl.mediaSpeed = 0; 
    this.state=GF_STATE_PAUSE;
    this.play.switch_icon(icon_play);
    return;
   }
   //we are playing, resume from stop if needed
   if (this.stoped_url) {
    if (controlled_renderer) {
      controlled_renderer.Play();
    } else {
     set_movie_url(this.stoped_url);
    }
    this.stoped_url = null;
    //not in trick mode, next pause/play will restart from current time
    if (state != GF_STATE_TRICK)
      movie_ctrl.mediaStartTime = -1; 
   }
   
  
   if (state==GF_STATE_PLAY) {
    if (controlled_renderer) controlled_renderer.Play();
    this.state = state;
    movie_ctrl.mediaSpeed = 1; 
    this.play.switch_icon(icon_pause);
    return;
   }
   if (state==GF_STATE_TRICK) {
    this.state = state;
    this.play.switch_icon(icon_play);
    movie_ctrl.mediaStartTime = -1; 
    return;
   }
  }
  
  wnd.stop = gw_new_icon_button(wnd.infobar, 'icons/media-playback-stop.svg', 'Stop', 'icon');
  wnd.stop.on_click = function() {
   player_control.set_state(GF_STATE_STOP);
  }
  wnd.stop.set_size(small_control_icon_size, small_control_icon_size);


  if (0) {
    wnd.rewind = gw_new_icon_button(wnd.infobar, 'icons/media-seek-backward.svg', 'Rewind', 'icon');
    wnd.rewind.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.rewind = null;
  } 

  wnd.play = gw_new_icon_button(wnd.infobar, 'icons/media-playback-start.svg', 'Play', 'icon');
  wnd.play.set_size(control_icon_size, control_icon_size);
  wnd.state = GF_STATE_PLAY;
  wnd.play.add_icon('icons/media-playback-pause.svg');
  wnd.play.on_click = function() {
    player_control.set_state( (player_control.state==GF_STATE_PLAY) ? GF_STATE_PAUSE : GF_STATE_PLAY);
  }
  
  if (!browser_mode) {
    wnd.forward = gw_new_icon_button(wnd.infobar, 'icons/media-seek-forward.svg', 'Forward', 'icon');
    wnd.forward.on_click = function() {
     if (movie_ctrl.mediaSpeed) {
      player_control.set_state(GF_STATE_TRICK);
      movie_ctrl.mediaSpeed = 2*movie_ctrl.mediaSpeed;
     }
    }
    wnd.forward.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.forward = null;
  }

  wnd.media_line = gw_new_progress_bar(wnd.infobar, false, true);
  wnd.media_line.on_slide = function(value, type) { 
   if (!movie_connected && !controlled_renderer) {
    this.set_value(0);
    return;
   }
   
   var duration = player_control.duration;
   if (!duration) return;
   var time = value*duration/100;

   if (controlled_renderer) {
    controlled_renderer.Seek(time);
    return;
   }
   root.children[0].set_bind = FALSE;
   switch (type) {
   //sliding
   case 1:
    player_control.set_state(GF_STATE_PAUSE);
    movie_ctrl.mediaStartTime = time; 
    movie_ctrl.mediaSpeed = 0;
    break;
   //done sliding
   case 2:
    player_control.set_state(GF_STATE_PLAY);
    if (time!= movie_ctrl.mediaStartTime) movie_ctrl.mediaStartTime = time;
    movie_ctrl.mediaSpeed = 1;
    break;
   //init slide, go in play mode
   default:
    if (player_control.state==GF_STATE_STOP)
      player_control.set_state(GF_STATE_PLAY);
      
    player_control.set_state(GF_STATE_PAUSE);
    movie_ctrl.mediaStartTime = time;
    break;
   }   
  }  
  

  wnd.time = gw_new_text(wnd.infobar, '00:00:00', 'osdwindow');
  gw_object_set_hitable(wnd.time);
  wnd.time.reversed = false;
  wnd.time.on_down = function(val) {
   if (!val) return;
   this.reversed = !this.reversed;
   player_control.set_time(player_control.current_time);
  }
  wnd.time.set_size(control_icon_size, control_icon_size);
  wnd.time.set_width(4*wnd.time.font_size() );
  
  if (0) {
    wnd.loop = gw_new_icon_button(wnd.infobar, 'vector/loop.svg', 'Loop', 'icon');
    wnd.loop.on_click = function () { 
      movie_ctrl.loop = movie_ctrl.loop ? FALSE : TRUE;
    }
    wnd.loop.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.loop = null;
  }

  wnd.view = gw_new_icon_button(wnd.infobar, 'icons/edit-find.svg', 'Navigation', 'icon');
  wnd.view.on_click = function() {
    select_navigation_type();
  }
  wnd.view.set_size(small_control_icon_size, small_control_icon_size);

    
  if (!browser_mode) {
    wnd.open = gw_new_icon_button(wnd.infobar, 'icons/folder.svg', 'Open', 'icon');
    wnd.open.on_click = function () { open_local_file(); }
    wnd.open.on_long_click = function () { open_url(); }
    wnd.open.set_size(small_control_icon_size, small_control_icon_size);
  } else{ 
    wnd.open = null;
  }
  
  
  if (!browser_mode) {
    wnd.home = gw_new_icon_button(wnd.infobar, 'icons/go-home.svg', 'Home', 'icon');
    wnd.home.on_click = function() { show_dock(true); }
    wnd.home.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.home = null;
  }
  
  
  if (UPnP_Enabled) {
    wnd.remote =  gw_new_icon_button(wnd.infobar, 'icons/video-display.svg', 'Select Display', 'icon');
    wnd.remote.on_click = function () { select_remote_display('push'); }
    wnd.remote.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.remote =  null;
  }
  
  if (1) {
    wnd.fullscreen = gw_new_icon_button(wnd.infobar, 'icons/view-fullscreen.svg', 'Fullscreen', 'icon');
    wnd.fullscreen.on_click = function() { gpac.fullscreen = !gpac.fullscreen; }
    wnd.fullscreen.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.fullscreen = null;
  }
  
  if (!browser_mode) {
    wnd.exit = gw_new_icon_button(wnd.infobar, gwskin.images.cancel, gwskin.labels.close, 'icon');
    wnd.exit.on_click = function() { gpac.exit(); }
    wnd.exit.set_size(small_control_icon_size, small_control_icon_size);
  } else {
    wnd.exit = null;
  }
       
  
  
  wnd.layout = function(width, height) {
   var min_w, full_w, time_w;
   var control_icon_size = gwskin.default_icon_size;
   this.move(0, Math.floor( (height-display_height)/2) );
    
   width -= control_icon_size/2;
   min_w = this.play.width + this.time.width;
   if (this.open) min_w += this.open.width;
   if (this.home) min_w += this.home.width;
   if (this.exit && gpac.fullscreen) min_w += this.exit.width;
   full_w = 0;
   if (this.snd_low) full_w += this.snd_low.width;
   if (this.snd_ctrl) full_w += this.snd_ctrl.width;
   if (this.fullscreen) full_w += this.fullscreen.width;

   if (this.view) {
     this.view.hide();
     if (!dynamic_scene && movie_connected && (gpac.navigation_type!= GF_NAVIGATE_TYPE_NONE) ) {
      full_w+= this.view.width;
     }
   }
   
   if (this.duration) {
     if (this.stop) full_w += this.stop.width;
     if (this.play) full_w += this.play.width;
     if (this.rewind) full_w+= this.rewind.width;
     if (this.forward) full_w+= this.forward.width;
     if (this.loop) full_w += this.loop.width;
   }

	 if (this.remote && UPnP.MediaRenderersCount && (current_url!='')) {
      full_w += this.remote.width;
   }
   time_w = this.media_line.visible ? 2*control_icon_size : 0;

   if (this.exit) {
    if (gpac.fullscreen) {
      this.exit.show();
    } else {
      this.exit.hide();
    }
   } 

   if (min_w + full_w + time_w < width) {
     if (this.media_line.visible)
       this.media_line.set_size(width - min_w - full_w - control_icon_size/3, control_icon_size/3);
  
     if (this.snd_low) this.snd_low.show();
     if (this.snd_ctrl) this.snd_ctrl.show();
     if (this.duration) {
      if (this.rewind) this.rewind.show();
      if (this.forward) this.forward.show();
      if (this.loop) this.loop.show();
      if (this.stop) this.stop.show();
     }
     if (wnd.fullscreen) wnd.fullscreen.show();
  	 
     if (this.remote) {
      if (UPnP.MediaRenderersCount && (current_url!='')) {
        this.remote.show();
      } else {
        this.remote.hide();
      }
  	 }
  	 
     if (this.view && !dynamic_scene && movie_connected && (gpac.navigation_type!= GF_NAVIGATE_TYPE_NONE) ) {
      this.view.show();
     }
   } else {
        
     if (this.snd_low) this.snd_low.hide();
     if (this.snd_ctrl) this.snd_ctrl.hide();
     if (this.rewind) this.rewind.hide();
     if (this.stop) this.stop.hide();
     if (this.forward) this.forward.hide();
     if (this.loop) this.loop.hide();
     if (this.fullscreen) this.fullscreen.hide();
  	 if (this.remote) this.remote.hide();
  	 
     if (this.view && movie_connected && (gpac.navigation_type!= GF_NAVIGATE_TYPE_NONE) ) {
      if (min_w + time_w + this.view.width < width) {
       min_w += this.view.width;
       this.view.show();
      }
     }
     
     if (this.remote) {
      if (UPnP.MediaRenderersCount && (current_url!='') && (min_w + time_w + this.remote.width < width)) {
       min_w += this.remote.width;
       this.remote.show();
      } else {
        this.remote.hide();
  	  }
 	   }
     
     if (this.media_line.visible)
       this.media_line.set_size(width - min_w - 5, control_icon_size/3);

   }   
   width += control_icon_size/2;
   this.infobar.set_size(width, height);
  }

  wnd.current_time = 0;
  wnd.duration = 0;
  wnd.set_duration = function(value) { 
    this.duration = value; 
    if (!value) {
      wnd.time.hide();
      wnd.media_line.hide();
      if (wnd.rewind) wnd.rewind.hide();
      if (wnd.stop) wnd.stop.hide();
      if (wnd.forward) wnd.forward.hide();
      if (wnd.loop) wnd.loop.hide();
      wnd.time.set_size(0, control_icon_size);
      wnd.time.set_width(0);
    } else {
      wnd.time.show();
      wnd.media_line.show();
      if (wnd.rewind) wnd.rewind.show();
      if (wnd.stop) wnd.stop.show();
      if (wnd.forward) wnd.forward.show();
      if (wnd.loop) wnd.loop.show();
      if (value<3600) {
        wnd.time.set_size(control_icon_size/2, control_icon_size);
        wnd.time.set_width(3*wnd.time.font_size() );
      } else {
        wnd.time.set_size(control_icon_size, control_icon_size);
        wnd.time.set_width(4*wnd.time.font_size() );
      }
    }
    this.layout(this.width, this.height);
  }
  wnd.set_time = function(value) {
   var h, m, s, str;
   if (!this.duration) return;
   this.current_time = value;
   if (this.duration) { 
    this.media_line.set_value(100*value / this.duration);
   }
   str='';
   if (this.time.reversed) {
    value = this.duration-value;
    str='-';
   }
   h = Math.floor(value/3600);
   value -= h*3600;
   m = Math.floor(value / 60);
   value -= m*60;
   s = Math.floor(value);
   if (h) {
     if (h<10) str += '0';
     str += h + ':';
   }
   if (m<10) str += '0';
   str += m + ':';
   if (s<10) str += '0';
   str += s;
   this.time.set_label(str);   
  }
  wnd.on_event = function(evt) {
   if (this.infobar.on_event(evt)) return true;
   return false;
  }
  
  gw_object_set_hitable(wnd);
  wnd.set_size(200, control_icon_size);
  wnd.set_duration(0);
  wnd.set_time(0);
  return wnd;
}



function open_local_file()
{
  var filebrowse = gw_new_file_open();
  filebrowse.filter = '*';
  filebrowse.browse(gpac.last_working_directory);  
  
  filebrowse.on_browse = function(value, directory) {
    if (value==null) {
      player_control.set_state(this.prev_state);
      player_control.show();
    } else {
     if (directory) gpac.last_working_directory = directory;
     set_movie_url(value);
     show_dock(false);
    }
 }
 var w = display_width/2;
 if (w<200) w = display_width-20;
 filebrowse.set_size(w, 3*display_height/4);
 if (gpac.hardware_rgba) filebrowse.set_alpha(0.8);

 player_control.hide();
 gpacui_show_window(filebrowse);

 filebrowse.prev_state = player_control.state;
 player_control.set_state(GF_STATE_PAUSE);
}

urldlg = null;

function open_url()
{
  if (urldlg) return;
  urldlg = gw_new_window(null, true, 'window');

  urldlg.sizer = gw_new_grid_container(urldlg);

  urldlg.icon = gw_new_icon_button(urldlg.sizer, gwskin.images.cancel, gwskin.labels.close, 'icon');
  urldlg.icon.set_size(gwskin.default_icon_size, gwskin.default_icon_size);
  urldlg.icon.on_click = function() {
    urldlg.close();
    urldlg = null;
  }
  urldlg.label = gw_new_text(urldlg.sizer, 'URL', 'window');
  urldlg.label.set_size(gwskin.default_icon_size, gwskin.default_icon_size);
  urldlg.label.set_width(gwskin.default_icon_size);
    
  urldlg.edit = gw_new_text_edit(urldlg.sizer, '');
  
  urldlg.edit.on_text = function(value) {
    if (value != '') set_movie_url(value);
    urldlg.close();
    urldlg = null;
 }
 urldlg.layout = function(width, height) {
  var w = width - this.icon.width - this.label.width-5;
  this.edit.set_size(w, 4*gwskin.default_icon_size/5);
  this.sizer.set_size(width, height);
 }
 urldlg.close  = function() {
  this.label = null;
  this.icon = null;
  this.edit = null;
  this.sizer = null;
  this.hide(this._on_wnd_close);
 }
 gpacui_show_window(urldlg);
 urldlg.set_size(display_width, gwskin.default_icon_size);
}

function onMediaRendererAdd(name, uuid, is_add)
{
	if (upnp_renderers) upnp_renderers.browse('');
	if (!is_add && controlled_renderer && (name==controlled_renderer.Name) ) controlled_renderer = null;

  /*redo player control*/	
	player_control.layout(player_control.width, player_control.height);

  for (var i in all_extensions) {
   if (typeof all_extensions[i].on_upnp_add != 'undefined') all_extensions[i].on_upnp_add(name, uuid, is_add);
  }
}

function onMediaConnect(url, src_ip)
{
  for (var i in all_extensions) {
   if (typeof all_extensions[i].on_media_open != 'undefined') {
    if (all_extensions[i].on_media_open(url, src_ip)>0) return;
   }
  }

  {
		log(l_inf, 'DLNA URL connect ' + url);
		movie_ctrl.mediaStartTime = -1;
		set_movie_url(url);
	}
}

function onMediaStop()
{
	log(l_inf, 'DLNA Media Stop');
	movie_ctrl.mediaStartTime = 0;
	movie_ctrl.mediaSpeed = 0;
}
function onMediaPause()
{
	log(l_inf, 'DLNA Media pause');
	movie_ctrl.mediaSpeed = 0;
	movie_ctrl.mediaStartTime = -1;
}
function onMediaPlay()
{
	log(l_inf, 'DLNA Media Play');
	movie_ctrl.mediaSpeed = 1;
}

function OnMediaSeek(time)
{
	log(l_inf, 'DLNA Media Seek to '+time);
	movie_ctrl.mediaStartTime = time;
}

function OnMediaMigrate() {
	log(l_inf, 'Migration Request received');
	var str = gpac.migrate_url(movie.children[0].url[0]);
	set_movie_url('');
	return str;
}

function onMediaTimeChanged(render_idx, time)
{
 if (!controlled_renderer) return;
 if (UPnP.GetMediaRenderer(render_idx).Name==controlled_renderer.Name) {
  player_control.set_time(time);
 }
}

function onMediaDurationChanged(render_idx, time)
{
 if (!controlled_renderer) return;
 if (UPnP.GetMediaRenderer(render_idx).Name==controlled_renderer.Name) {
  player_control.set_duration(time);
 }
}


function select_remote_display(action_type, callback)
{ 
  if (upnp_renderers) return;
  upnp_renderers = gw_new_file_open();
  upnp_renderers.set_label('Select Remote Display');
  upnp_renderers.action_type = action_type;
  if (arguments.length<2) callback=null;
  upnp_renderers.callback = callback;

  upnp_renderers.on_close = function() {
   upnp_renderers = null;
  }
  /*override browse function*/
  upnp_renderers._on_upnp_click = function() {
   	var renderer = (this.render_idx<0) ? null : UPnP.GetMediaRenderer(this.render_idx);
    var act_type = upnp_renderers.action_type;
    
    if (act_type=='callback') {
			if (upnp_renderers.callback) upnp_renderers.callback(renderer);
			upnp_renderers.callback = null;
      upnp_renderers.close();
      upnp_renderers = null;
      return;
    }

    upnp_renderers.close();
    upnp_renderers = null;

    if (act_type=='select') {
     controlled_renderer = renderer;
     return;
    }

    if ((current_url.indexOf('://') < 0) || (current_url.indexOf('file://') >= 0) ) {
			if (! UPnP.MediaServerEnabled) {
       log(l_err, 'GPAC Media Server is disabled - Cannot share '+current_url);
       return;
  		}
	  	uri = UPnP.ShareResource(current_url);
			log(l_inf, 'Sharing '+current_url+' to renderer '+renderer.Name + ' as resource '+uri);
			renderer.Open(uri);
			renderer.Seek(current_time);
  	} else if (gpac.getOption('Network', 'MobileIPEnabled')=='yes') { 
   		uri = gpac.migrate_url(movie.children[0].url[0]);
 			log(l_inf, 'Migrating '+current_url+' to renderer '+renderer.Name + ' as Mobile IP resource '+uri);
 			renderer.Open(uri);
 		} else {
  		log(l_inf, 'Migrating '+current_url+' to renderer '+renderer.Name);
  		renderer.Open(current_url);
  		renderer.Seek(current_time);
  	}
 		set_movie_url('');
    player_control.show();
  };

  upnp_renderers._browse = function(dir, up) {
   var w, h, i, y;
   this.area.reset_children();
 
   if (this.action_type=='select') {
    var item = gw_new_icon_button(this.area, controlled_renderer ? 'icons/applications-internet.svg' : 'icons/preferences-desktop-remote-desktop.svg', 'Local Display', 'button');
    item.set_size(item.width, item.height);
    item.render_idx = -1;
    item.on_click = this._on_upnp_click;
   }   
   for (i=0; i<UPnP.MediaRenderersCount; i++) {
		var render = UPnP.GetMediaRenderer(i);
		var	icon = 'icons/applications-internet.svg';
    if ((this.action_type=='select') && controlled_renderer && (controlled_renderer.Name==render.Name)) icon = 'icons/preferences-desktop-remote-desktop.svg';
    var item = gw_new_icon_button(this.area, icon, render.Name, 'button');
    item.set_size(item.width, item.height);
    item.render_idx = i;
    item.on_click = this._on_upnp_click;
   }  
   this.layout(this.width, this.height);
 }
 upnp_renderers.browse('');  
 upnp_renderers.go_up.hide();
 upnp_renderers.set_size(display_width, display_height);
 gpacui_show_window(upnp_renderers);
}


remote_servers = null;
function browse_remote_servers()
{
  if (remote_servers) return;
  remote_servers = gw_new_file_open();
  remote_servers.set_label('Browse Remote Servers');


  remote_servers.icon_server = gw_load_resource( 'icons/applications-internet.svg' );
  remote_servers.icon_folder = gw_load_resource( gwskin.images.folder );
  remote_servers.icon_file = gw_load_resource( gwskin.images.mime_generic );
  
  /*override browse function*/
  remote_servers.server = null;
  remote_servers._on_server_click = function() {
   if (this.is_server) {
    remote_servers.server = UPnP.GetMediaServer(this.server_idx);
    remote_servers._browse('0', false);
    return;
   }
   if (this.directory) {
    remote_servers._browse(this.path, false);
    return;
   }
   //alert('Opening file '+this.resource_uri);
   set_movie_url(this.resource_uri);
   remote_servers.close(); 
   remote_servers = null;
  }

  remote_servers._browse = function(dir, up) {
   var w, h, i, y;
   this.area.reset_children();

   //alert('Browsing dir '+dir+' on server '+ (this.server ? this.server.Name : 'null') );   
   if (!this.server || (up && !this.server.HasParentDirectory())) dir='';
   this.directory = dir;
   
   if (dir=='') {
     for (i=0; i<UPnP.MediaServersCount; i++) {
  		var server = UPnP.GetMediaServer(i);
      var item = gw_new_icon_button(this.area, this.icon_server, server.Name, 'button');
      item.set_size(item.width, item.height);
      item.server_idx = i;
  		item.directory = true;
			item.is_server = true;
      item.on_click = this._on_server_click;
     }  
   } else {
     if (up) dir = '..';
		 this.server.Browse(dir, this.filter);
     //alert('Nb Files '+this.server.FilesCount);
     this.directory = this.server.Name;
		 
		 for (var i=0; i<this.server.FilesCount; i++) {
				var file = this.server.GetFile(i);
				if (file.Directory) {
          var item = gw_new_icon_button(this.area, this.icon_folder, file.Name, 'button');
					item.directory = true;
					item.path = file.ObjectID;
					item.is_server = false;
          item.on_click = this._on_server_click;
				} else {
				  /*FIXME - need a better way to handle this !!*/
//					for (var j=0; j<file.ResourceCount; j++) {
  					for (var j=0; j<1; j++) {
            var item = gw_new_icon_button(this.area, this.icon_file, file.Name, 'button');
						item.directory = false;
						item.resource_uri = file.GetResourceURI(j);
						var protoInfo = item.resource_uri.split(":");
            item.set_label(file.Name + ' ('+protoInfo[0]+')' );
						item.path = file.ObjectID;
						item.is_server = false;
            item.on_click = this._on_server_click;
					}
				}
			}
    }
   //alert('Done Browsing, refreshing layout');
   
 	 if (this.directory == '') this.go_up.hide();
   else this.go_up.show();

   this.layout(this.width, this.height);
 }
 
 remote_servers.predestroy = function() {
  this.server = null;
  this.scan_dir = null;
  this.go_up = null;
  gw_unload_resource(this.icon_server);
  gw_unload_resource(this.icon_folder);
  gw_unload_resource(this.icon_file);
  remote_servers = null;
 }
 
 remote_servers.browse('');  
 remote_servers.set_size(display_width, display_height);
 gpacui_show_window(remote_servers);
} 

nav_sel_wnd = null;

function make_select_item(container, text, type, current_type)
{
  if (current_type==type) text = '* ' + text+ ' *';
  var info = gw_new_button(container, text, 'osdwindow');
	info.on_click = function() { nav_sel_wnd.select(type); };	
	info.set_size(120, 20);
	return info;
}

function select_navigation_type()
{
  var nb_items = 0;
  var type = gpac.navigation;
  if (nav_sel_wnd) return;
  nav_sel_wnd = gw_new_window(null, true, 'osdwindow');
  nav_sel_wnd.area = gw_new_grid_container(nav_sel_wnd);
	nav_sel_wnd.layout = function(width, height) {
   this.area.set_size(width, height);
  }
  nav_sel_wnd.select = function(type) {
	  this.area = null;
    this.close();
    nav_sel_wnd = null;
    if (type=='reset') {
      gpac.navigation_type = 0;
    } else {
      gpac.navigation = type;
    }
  }

  var info = gw_new_button(nav_sel_wnd.area, 'Reset', 'button');
	info.on_click = function() { 
    nav_sel_wnd.select('reset');
  };	
	info.set_size(120, 20);
	nb_items = 1;
	

  var info = make_select_item(nav_sel_wnd.area, 'None', GF_NAVIGATE_NONE, type);	
  make_select_item(nav_sel_wnd.area, 'Slide', GF_NAVIGATE_SLIDE, type);	
  make_select_item(nav_sel_wnd.area, 'Examine', GF_NAVIGATE_EXAMINE, type);	
  nb_items += 3;

  if (gpac.navigation_type==GF_NAVIGATE_TYPE_3D) {
    make_select_item(nav_sel_wnd.area, 'Walk', GF_NAVIGATE_WALK, type);	
    make_select_item(nav_sel_wnd.area, 'Fly', GF_NAVIGATE_FLY, type);	
    make_select_item(nav_sel_wnd.area, 'Pan', GF_NAVIGATE_PAN, type);	
    make_select_item(nav_sel_wnd.area, 'Game', GF_NAVIGATE_GAME, type);	
    make_select_item(nav_sel_wnd.area, 'Orbit', GF_NAVIGATE_ORBIT, type);	
    make_select_item(nav_sel_wnd.area, 'VR', GF_NAVIGATE_VR, type);	
    nb_items += 6;
  }
  nav_sel_wnd.on_event = function(evt) {
   if (this.area && this.area.on_event(evt)) return true;
   return false;
  }

  gpacui_show_window(nav_sel_wnd);  
  nav_sel_wnd.set_size(120, nb_items*info.height);
  nav_sel_wnd.show();

}

