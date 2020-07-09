var movie1 = "../widgets/media/local_video/movies/Clovis Cornillac.mp4";
var movie2 = "../widgets/media/local_video/movies/Airheads2001_edit.mp4";
var movie3 = "../widgets/media/local_video/movies/CH Video - Alberto Alessi.mp4";
var movie4 = "../widgets/media/local_video/movies/Crossing2001_edit.mp4";
var movie5 = "../widgets/media/local_video/movies/EnemyAtT2001_512kb.mp4";
var movie6 = "../widgets/media/local_video/movies/KAMI2001_512kb.mp4";
var movie7 = "../widgets/media/local_video/movies/PipeDrea2001_512kb.mp4";
var movie8 = "../widgets/media/local_video/movies/LAUTRETE2001_512kb.mp4";
var movie9 = "../widgets/media/local_video/movies/Unexpect2001_512kb.mp4";
var movie0 = "../widgets/media/local_video/movies/Animatrix_The_Second_Renaissance_l.mp4";

var xlinkns = 'http://www.w3.org/1999/xlink';

/* override the print function to use SVG alert */
print = alert;

/*******************************************************************************
 *Global elements referenced in this script
 ******************************************************************************/

/* Root of the SVG document presenting the widgets, the dock ...*/     
var root;
/* The dock element presents the widget icons or simplified representations */
var dock;
/* The movie element presents the media content, widgets are added on top of it */
var movie;
/* The widget_display element contains the full representation of all activated widgets, it is on top of the movie element */
var widget_display;
/* The display element contains things to be displayed on top of the widgets */
var display;


function create_icon(url, short_name) {
    var icon_g;
    icon_g = document.createElement('g');
    icon_g.short_name = short_name;

    var back_rect;
    back_rect = document.createElement('rect');
    back_rect.setAttribute('fill', 'url(#inactiveGradient)');
    back_rect.setAttribute('rx', 5);
    back_rect.setAttribute('stroke', 'black');
    back_rect.setAttribute('stroke-width', 1);
    icon_g.appendChild(back_rect);

    var icon;
    icon = document.createElement('animation');
    icon.setAttributeNS(xlinkns ,'href', url);
    icon_g.appendChild(icon);

    var text;
    text = document.createElement('textArea');
    text.setAttribute('fill', 'white');
    text.textContent = short_name;
    icon_g.appendChild(text);
    
    return icon_g;
}

function set_icon_active(icon, value) {
  var back_rect;
  back_rect = icon.firstElementChild;
  if (!value) back_rect.setAttribute('fill', 'url(#inactiveGradient)');
  else back_rect.setAttribute('fill', 'url(#activeGradient)');
}

function set_icon_visible(icon, value) {
  if (value) icon.setAttribute('display', 'inline');
  else icon.setAttribute('display', 'none');
}

function set_icon_position(icon, x, y) {
  icon.setAttribute('transform', 'translate('+x+','+y+')');
}

function set_icon_size(icon, w, h) {
  var back_rect = icon.firstElementChild;
  back_rect.setAttribute('width', w);
  back_rect.setAttribute('height', h);

  var anim = back_rect.nextElementSibling;
  anim.setAttribute('x', 5*w/8);
  anim.setAttribute('y', -10);
  anim.setAttribute('width', 1.2*h);
  anim.setAttribute('height', 1.2*h);
  
  var tA = anim.nextElementSibling;
  tA.setAttribute('x', icon_spacing);
  tA.setAttribute('y', 5);
  tA.setAttribute('height', h);
  if (w > 100) {
	tA.setAttribute('font-size', h/3);
	tA.setAttribute('width', w/1.8);
  } else {
	tA.setAttribute('font-size', h/5);
	tA.setAttribute('width', 4*w/8);
  }
}

function add_widget_to_dock(wid) {
    var wid_icon_url = null;
    var icon;
    var preferredIconType = '.svg';
    for (var i = 0; i < wid.icons.length; i++) {
        wid_icon_url = wid.icons[i].relocated_src;
        if (wid.icons[i].relocated_src.indexOf(preferredIconType) > 0) {
            break;
        }
    }
    if (wid_icon_url == null) wid_icon_url = 'icons\\applications-system.svg';
    //alert(wid_icon_url);
    icon = create_icon(wid_icon_url, wid.name);
    icon.widget = wid;
    set_icon_visible(icon, false);
    dock.appendChild(icon);
    wid.icon_dock = icon;
}


function resize() {
  /*dummy values, assign upon window resize event*/
  alert('Size:'+root.viewport.width+'x'+root.viewport.height);    
  display_width  = root.viewport.width;//scene.screen_width;
  display_height = root.viewport.height;//scene.screen_height;
  
  nb_icon_max = 4;
  if (display_width > 800) {
	  out_spacing = 30;
	  icon_spacing = 10;
  } else {
	  out_spacing = 10;
	  icon_spacing = 4;
  }
  dock_width = display_width;
  icon_width = (dock_width-((nb_icon_max-1)*icon_spacing+2*out_spacing))/nb_icon_max;

  dock_height = display_height/10;
  icon_height = dock_height+4;

  movie.setAttribute('width', display_width);
  if (is_dock_visible) {
    movie.setAttribute('height', display_height - dock_height);
  } else { 
    movie.setAttribute('height', display_height);
  }

  dock.setAttribute('transform', 'translate(0, '+(display_height-dock_height)+')');

  var dock_back = dock.firstElementChild.firstElementChild;
  dock_back.setAttribute('x', 0);
  dock_back.setAttribute('y', 0);
  dock_back.setAttribute('height', dock_height);
  dock_back.setAttribute('width', dock_width);

  var left_arrow = dock_back.nextElementSibling;
  left_arrow.setAttribute('x', 0);
  left_arrow.setAttribute('y', 0);
  left_arrow.setAttribute('height', dock_height - 5);
  left_arrow.setAttribute('width', out_spacing);
  
  var right_arrow = left_arrow.nextElementSibling;
  right_arrow.setAttribute('x', dock_width-out_spacing);
  right_arrow.setAttribute('y', 0);
  right_arrow.setAttribute('height', dock_height-5);
  right_arrow.setAttribute('width', out_spacing);
  
  dock_layout(); 
  alert('ok');
}

/*******************************************************************************
 * Global Initialization function, called when the SVG document is loaded
 * Initializes global variables (elements)
 * Finds the available widgets and add them to the dock  
 ******************************************************************************/
function initialize() {
    /* root of the SVG document presenting the widgets, the dock ...*/     
    root = document.documentElement;
    
    /* The dock presents the widget icons or simplified representations */
    dock = document.getElementById('dock');

    /* The movie element presents the media content, widgets are added on top of it */
    movie = document.getElementById('movie');
    
    /* The widget display element contains the full representation of all activated widgets, it is on top of the movie_inline element */
    widget_display = document.getElementById('widget_display');

    /* The display element contains things to be displayed on top of the widgets */
    display = document.getElementById('display');

    is_dock_visible = false;
    if (!is_dock_visible) dock.setAttribute('display', 'none');
     
    activate = new Array();
    selected_widget_index = 0;
    first_displayed_widget_index = 0;
    nb_widgets_visible = 0;
    current_widget_pos = 0;
    next_widget_pos = 0;
    vertical_spacing = 10;
    vertical_offset = 0;
    
    /* if the WidgetManager object is not defined (this is a GPAC extension), we define a dummy one to enable debugging in Opera for example */
    if (typeof WidgetManager == 'undefined') {
      WidgetManager = new Object;
      WidgetManager.initialize = function () {}
      WidgetManager.num_widgets = 8;
      WidgetManager.get = function(i) {
        var wid = new Object;
        if (i < 2) {
          wid.icon = 'widgets/clock/appointment-new.svg';
          wid.main = 'widgets/clock/appointment-new.svg';
          wid.name = 'Clock';
          wid.visible = false;
          wid.x = 0;
          wid.y = 0;
          wid.width = 100;
          wid.height = 100;
        } else {
          wid.icon = 'icons/audio-volume-high.svg';
          wid.main = 'icons/audio-volume-high.svg';
          wid.name = 'Music';
          wid.visible = false;
          wid.x = 0;
          wid.y = 0;
          wid.width = 100;
          wid.height = 100;
        }
        return wid;
      }
      WidgetManager.unload = function (wid) {}
    }
    
    /* Setup the GPAC Widget Manager - this will also scan the available widgets */
    widget_manager_init();
    
    /* scan all the widgets from the widget manager, create an iconic view, set its size, add it to the dock element but make it invisible */    
    var i; 
    nb_widgets = WidgetManager.num_widgets;
    for (i=0; i<nb_widgets; i++) {
      var wid = WidgetManager.get(i);
      if (wid == null) continue;
      add_widget_to_dock(wid);
      activate[i] = true;
    }
    
    //resize();
   
    root.addEventListener('keyup', on_key_up, false);

    /* register the callback to be notified of incoming widgets */
    has_upnp = (typeof UPnP != 'undefined');
    alert('has upnp:'+ has_upnp);
    if (has_upnp) {
     /* setting the callback to allow other devices to push their widgets */
     UPnP.onMediaConnect = onMediaConnect;
     UPnP.onMediaStop = onMediaStop;
     UPnP.onMediaPause = onMediaPause;
     UPnP.onMediaPlay = onMediaPlay;
     /* Tell GPAC that the calls to the main Renderer (like open, ...) must be forwared to this scene */
     UPnP.BindRenderer();
    }
    
}

function dock_layout() {

  alert('dock layout first='+ first_displayed_widget_index+ '/' + nb_widgets+' active='+selected_widget_index);
  var i;
  var docFirstIcon = dock.firstElementChild.nextElementSibling;
  /* for all icons before the first visible, make them invisible */ 
  var child = docFirstIcon;
  i = 0;
  while (child && i < first_displayed_widget_index) {
    set_icon_visible(child, false);
    alert('setting child '+i+' invisible'+(i == selected_widget_index ? '*' :' ')+ ' '+child.getAttribute('transform'));
    child = child.nextElementSibling;
    i++;
  }

  /* for all icons from the first visible, make them visible and set their position 
     and for all the icons after the max number of icons, make them invisible */ 
  i = first_displayed_widget_index;
  var start_x = out_spacing;
  while (child && i < nb_widgets) {
    if (i >= (first_displayed_widget_index+nb_icon_max)) {
      set_icon_visible(child, false);
      alert('setting child '+i+' invisible'+(i == selected_widget_index ? '*' :' ')+ ' '+child.getAttribute('transform'));
    } else {
      var offset_x = start_x + (i-first_displayed_widget_index)*(icon_width+icon_spacing);
      set_icon_visible(child, true);
      if (i == selected_widget_index) set_icon_active(child, true);
      else set_icon_active(child, false);
      set_icon_size(child, icon_width, icon_height);
      set_icon_position(child, offset_x, dock_height-icon_height);
//      alert('setting child '+i+'   visible'+(i == selected_widget_index ? '*' :' ')+ ' '+child.getAttribute('transform'));
    }
    child = child.nextElementSibling;
    i++;
  }
}

function focusNextWidget() {
  alert('next');
  if (selected_widget_index < nb_widgets - 1) selected_widget_index++;
  if (selected_widget_index > first_displayed_widget_index + nb_icon_max -1 && first_displayed_widget_index + nb_icon_max < nb_widgets) first_displayed_widget_index++;
  dock_layout();
}

function focusPrevWidget() {
  alert('prev');
  if (selected_widget_index > 0) selected_widget_index--;
  if (first_displayed_widget_index > selected_widget_index && first_displayed_widget_index > 0) first_displayed_widget_index--;
  
  dock_layout();
}

function on_key_up(evt) {
  alert(evt.keyIdentifier + ' released '+evt.keyCode);
  if (evt.keyIdentifier == 'Right' || evt.keyCode == 39) {
    focusNextWidget();
  } else if (evt.keyIdentifier == 'Left' || evt.keyCode == 37) {
    focusPrevWidget();
  } else if (evt.keyIdentifier == 'Up') {
    vertical_offset += root.viewport.width / 3; 
    widget_display.setAttribute('transform', 'translate(0,'+vertical_offset+')');
  } else if (evt.keyIdentifier == 'Down') {
    vertical_offset -= root.viewport.width / 3; 
    widget_display.setAttribute('transform', 'translate(0,'+vertical_offset+')');
  } else if (evt.keyIdentifier == 'Enter') {
    if (is_dock_visible) {
      if (activate[selected_widget_index]) {
        widget_launch(WidgetManager.get(selected_widget_index), widget_display);
        activate[selected_widget_index] = false;
        nb_widgets_visible++;        
        if (nb_widgets_visible == 1) {
		}
      } else {
        widget_close(WidgetManager.get(selected_widget_index));
        activate[selected_widget_index] = true;
        nb_widgets_visible--;
        if (nb_widgets_visible == 0) {
        
		}
      }     
    }
  } else if (evt.keyIdentifier == 'F1') {
    dock_toggle_visible();
  } else if (evt.keyIdentifier == 'U+0030') {
    movie.setAttributeNS(xlinkns, 'href', movie0);
  } else if (evt.keyIdentifier == 'U+0031') {
    movie.setAttributeNS(xlinkns, 'href', movie1);
  } else if (evt.keyIdentifier == 'U+0032') {
    movie.setAttributeNS(xlinkns, 'href', movie2);
  } else if (evt.keyIdentifier == 'U+0033') {
    movie.setAttributeNS(xlinkns, 'href', movie3);
  } else if (evt.keyIdentifier == 'U+0034') {
    movie.setAttributeNS(xlinkns, 'href', movie4);
  } else if (evt.keyIdentifier == 'U+0035') {
    movie.setAttributeNS(xlinkns, 'href', movie5);
  } else if (evt.keyIdentifier == 'U+0036') {
    movie.setAttributeNS(xlinkns, 'href', movie6);
  } else if (evt.keyIdentifier == 'U+0037') {
    movie.setAttributeNS(xlinkns, 'href', movie7);
  } else if (evt.keyIdentifier == 'U+0038') {
    movie.setAttributeNS(xlinkns, 'href', movie8);
  } else if (evt.keyIdentifier == 'U+0039') {
    movie.setAttributeNS(xlinkns, 'href', movie9);
  }
}

function widget_close(widget) {
    alert('widget_close:'+widget.name);
    if (typeof widget.deactivate != 'undefined') { 
      widget.deactivate();
    } else {
      wid.visible = false;
    }
    next_widget_pos -= widget.height + vertical_spacing;
    widget.scene_container.removeChild(widget.widget_control);
    /*force disconnect of main resource - we do this because we are not sure when the widget_control will be destroyed due to JS GC*/
    widget.widget_control.firstElementChild.setAttributeNS(xlinkns, 'href', '');
    //widget.widget_control.firstElementChild.removeAttributeNS(xlinkns, 'href');
}

/* todo ...*/
function widget_remove(wid) 
{
  WidgetManager.unload(wid);
}

/* Function that starts to present the Widget Full representation */
function widget_launch(wid, scene_container) {
  alert('widget_launch:'+wid.name);

  var widg_ctrl, anim;
  
  //assign default size to the widget
  wid.width = root.viewport.width / 3;
  wid.height = root.viewport.width / 3;

  var w = wid.width;
  var h = wid.height;
  alert('w '+w + ' h '+h);
  
  widg_ctrl = document.createElement('g');
  widg_ctrl.wid = wid;
//  widg_ctrl.setAttribute('transform', 'translate('+(nb_widgets_visible>=2?600:0)+','+400*nb_widgets_visible+')');
//  var y = (nb_widgets_visible%2)*310;
//  var x = ((nb_widgets_visible - nb_widgets_visible%2)/2*310);
  var x = 0;
  var y = next_widget_pos;
  next_widget_pos += h+vertical_spacing; 
  
  alert('x:'+x+', y:' +y);
  widg_ctrl.setAttribute('transform', 'translate('+x+','+y+')');
  
  anim = document.createElement('animation');
  anim.setAttributeNS(xlinkns, 'href', wid.main);
  anim.setAttribute('width', w);
  anim.setAttribute('height', h);
  anim.setAttribute('preserveAspectRatio', 'xMidYMid');

  widg_ctrl.appendChild(anim);

  wid.widget_control = widg_ctrl;
  wid.scene_container = scene_container;
  /*this will setup the scene graph for the widget in order to filter input and output communication pins*/
  wid.on_load = function() {
     alert('wid.on_load:'+this.name); 
     WidgetManager.bind(this);
  }
  wid.activate(anim);
  scene_container.appendChild(widg_ctrl);  
}

function dock_toggle_visible() {
  if (is_dock_visible) {
    dock.setAttribute('display', 'none');
    movie.setAttribute('height', display_height);
    is_dock_visible = false;
  } else { 
    dock.setAttribute('display', 'inline');
    is_dock_visible = true;
    movie.setAttribute('height', display_height - dock_height);
  }
}  


function onMediaConnect(url, src_ip)
{
 alert('onMediaConnect :\"'+url+'\"');
 
 if (WidgetManager.probe(url) ) {
  var new_wid = WidgetManager.open(url, src_ip);
  if (new_wid==null) return;

  add_widget_to_dock(new_wid);
  for (var i = nb_widgets; i < WidgetManager.num_widgets; i++) {
    activate[i] = true;
  } 
  nb_widgets = WidgetManager.num_widgets;  
  dock_layout();
 } else {
  alert('invalid widget extension');
  /* TODO if this is not a widget url, we should probably change the movie being played */
 }
}

function onMediaStop()
{
 alert('Media Stop');
 movie.endElement();
}
function onMediaPause()
{
 alert('Media pause');
 movie.pauseElement();
}
function onMediaPlay()
{
 alert('Media Play');
 movie.resumeElement();
}
