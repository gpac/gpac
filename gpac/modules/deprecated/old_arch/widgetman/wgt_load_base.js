var wid = null;
var xmlns_xlink = 'http://www.w3.org/1999/xlink';

function debug(s) {
  alert('[Widget Loader] '+s);
}

function add_text_span(parent, string) {
    var span = document.createElement('tspan');
    span.textContent = string;
    parent.appendChild(span);
    parent.appendChild(document.createElement('tbreak'));
}

function load_widget(wid_url) {
  debug('Loading Widget: '+wid_url);
  wid = WidgetManager.load(wid_url);
  var anim = document.getElementById('w_anim');
  width = 320;
  height = 240;
  anim.setAttribute('height', height/3);
  
  var info = document.createElement('textArea');
  document.documentElement.appendChild(info);
  info.setAttribute('y', height/3);
  info.setAttribute('width', width);
  
  info.setAttribute('font-size', '7');
  info.setAttribute('font-family', 'Arial Unicode MS');
  if (wid != null) {
    debug('Loading scene: '+wid.main);
    anim.setAttributeNS(xmlns_xlink, 'href', wid.main);
    add_text_span(info, 'Widget Metadata');
    add_text_span(info, 'UA Locale: \'' + gpac.get_option('Systems', 'Language2CC') + '\'');
    add_text_span(info, 'widget src (abs.): \'' + wid.url + '\'');
    add_text_span(info, 'config src (abs.): \'' + wid.manifest + '\'');
    add_text_span(info, 'content src (rel.&loc./abs.): \''+wid.main+'\' / \''+wid.localizedSrc+'\' ('+wid.mainMimeType+';'+wid.mainEncoding+')');
    add_text_span(info, 'id: \'' + wid.identifier + '\'');
    add_text_span(info, 'shortname/name: \''+wid.shortName+ '\' / \''+wid.name+'\'');
    add_text_span(info, 'version: \''+wid.version+'\'');
    add_text_span(info, 'license/href: \''+wid.license+'\' / \''+wid.licenseHref+'\'');
    add_text_span(info, 'description: \''+wid.description+'\'');
    add_text_span(info, 'author (name/email/href): \''+wid.authorName+ '\' / \''+wid.authorEmail +'\' / \''+wid.authorHref+'\'');
    var s = 'icons src (rel.&loc./abs.): ';
    for (var i=0; i<wid.icons.length; i++) {
      if (i) s += ' , ';
      s += '\'' + wid.icons[i].src + '\' / \'' + wid.icons[i].relocated_src + '\'' + ' ('+wid.icons[i].width+'x'+wid.icons[i].height+')';      
    }
    add_text_span(info, s);
    
    s = 'preferences: ';
    for (var i=0; i<wid.preferences.length; i++) {
      var p = wid.preferences[i];
      if (i) s += ' , ';
      s += '' + p.name + '=\'' + p.value + '\' (ro:'+p.readonly+')';      
    }
    add_text_span(info, s);
    
    s = 'features: ';
    add_text_span(info, s);

    for (var i=0; i<wid.features.length; i++) {
      var f = wid.features[i];
      s = 'feature: ';
      s += '' + f.name + '(' + f.required + ')';
      add_text_span(info, s);
      if (f.params.length) {
        s = '-params: ';
        for (var j=0; j<f.params.length; j++) {
          var p = f.params[j];
          if (j != 0) s += ' , ';
          s += '' + p.name + '=\'' + p.value + '\'';
        }
        add_text_span(info, s);
      }
    }      
  
    add_text_span(info, 'size: '+wid.defaultWidth+'x'+wid.defaultHeight);
    add_text_span(info, 'viewmodes: \''+wid.viewmodes+'\'');
  } else {
    debug('Invalid Widget');
    info.textContent = 'Invalid widget - no info available';
  }
}
