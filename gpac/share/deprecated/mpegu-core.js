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
//					Jean Le Feuvre, Telecom Paris
//					Jean-Claude Dufourd, Telecom Paris
//
/////////////////////////////////////////////////////////////////////////////////

// 01122011 AMD1 startWidget listWidgets getWidget implemented

/*
 The widget manager in MPEG-U reference software is implemented in 2 parts:
 - a native GPAC module called gm_widgetman implementing
   * W3C widget loading (download, unzip, manifest parsing)
   * W3C Widget APIs
   * MPEG Widgets interfaces, messages and parameters listing

 - a script module (this file) implementing
  * MPEG Widget interface binding with UPnP services (UPnP is implemented in another module)
  * MPEG Widget interface binding with other local widgets
  * MPEG Widget discovery through UPnP devices announcement. The discovery is done by setting the device presentationURL to the widget URL


 A typical user of the widget manager will therefore see the following interfaces:

interface WidgetManager {
 readonly unsigned integer num_widgets;
 string last_widget_dir;

 Widget open(string url, string source_ip);
 void bind(Widget wid);
 Widget get(unsigned int idx);
 void unload(Widget wid);
 void migrate_widget(upnp_renderer, widget);
 void corein_message(widget, msg_name, arg1name, arg1val, ....);
 
 //callback functions for the GUI to be notifed of widgets added / removed when UPnP devices are appearing/disappearing
 void on_widget_add(widget);
 void on_widget_remove(widget);

 //callback functions for core:out
 void coreOutSetSize(wid, args);
 void coreOutShow(wid, args);
 void coreOutHide(wid, args);
 void coreOutRequestActivate(wid, args);
 void coreOutRequestDeactivate(wid, args);
 void coreOutShowNotification(wid, args);
 void coreOutPlaceComponent(wid, args);
 void coreOutGetAttention(wid, args);

 }


interface Widget {
 //exported meta-data from MPEG-U Manifest
 readonly string mainEncoding;
 readonly string mainMimeType;
 readonly integer defaultWidth;
 readonly integer defaultHeight;
 
 readonly Array icons;
 readonly Array features;
 readonly Array preferences;


 readonly string identifier;
 readonly string name;
 readonly string shortName;
 readonly string authorName;
 readonly string authorEmail;
 readonly string authorHref;
 readonly string description;
 readonly string viewmodes;
 readonly string licence;
 readonly string licenceHref;
 readonly string version;
 readonly string uuid;
 readonly boolean discardable;
 readonly boolean multipleInstances;

 //widget-manager specific data
 readonly string manifest;
 readonly string main;
 readonly string localizedSrc;
 readonly Array components; //array of widget objects used as component
 boolean permanent;
 readonly boolean is_component;
 readonly Widget parent;
 readonly boolean activated;
 readonly string section;
 readonly integer num_section;
 readonly integer num_interfaces;
 readonly integer num_bound_interfaces;
 readonly integer num_components;
 

 device = local UPnP device created in order to publish interfaces of this widget as upnp services
 originating_device = the device who sent the widget, either through migration or device discovery (presentationURL)
 originating_device_ip = the IP of the device who sent the widget



 void activate(Element node_with_ref_to_main);
 void deactivate();
 }

interface Icon {
 readonly string src;
 readonly string relocated_src;
 readonly integer width;
 readonly integer height;
}

interface Preference {
 readonly string name;
 readonly string value;
 readonly boolean readonly;
}

interface Features {
 readonly string name;
 readonly boolean required;
 readonly Array params
}

interface FeatureParam {
 readonly string name;
 readonly string value;
}
 */

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
log_level = l_deb;

whiteSpaceRegExp = new RegExp(' ', 'g');

/*initializes the widget manager*/
function widget_manager_init() {
    log(l_inf, 'Initializing MPEG-U Widgets');
    /*if UPnP is enabled, override the deviceAdd callback*/
    WidgetManager.upnp = false;
    WidgetManager.migrate_widget = wmjs_migrate_widget;
    WidgetManager.probe = wmjs_probe_widget;
    WidgetManager.open = wmjs_open_widget;
    WidgetManager.bind = wmjs_bind_widget;

    WidgetManager.on_widget_add = wmjs_on_widget_add;
    WidgetManager.on_widget_remove = wmjs_on_widget_remove;

    WidgetManager.check_bindings = wmjs_bind_widgets;
    WidgetManager.unbind_widget = wmjs_unbind_widget;
    WidgetManager.corein_message = wmjs_corein_message;

    WidgetManager.get_mpegu_service_providers = wmjs_get_mpegu_service_providers;
    WidgetManager.initialize();

    if (typeof(UPnP) != 'undefined') {
        WidgetManager.upnp = true;
        log(l_inf, 'Enabling Widget UPnP Discovery')
        UPnP.onDeviceAdd = wmjs_on_device_add;
        log(l_inf, 'Creating the standard MPEG-U Service')
        wmjs_create_standard_service();
    }
    log(l_inf, 'MPEG-U Widgets successfully initialized');
    initCore();
}

function wmjs_on_widget_add(widget) {}

function wmjs_on_widget_remove(widget) {}

function wmjs_probe_widget(url) {
    if ((url.lastIndexOf('.wgt') >= 0) || (url.lastIndexOf('.mgt') >= 0) || (url.lastIndexOf('.xml') >= 0)) return 1;
    return 0;
}

function wmjs_bind_output_trigger(widget, msg, callback, udta)
{
  var res = widget.bind_output_trigger(msg, callback, udta);
  if (!res) {
   log(l_err, 'Cannot bind output trigger of message '+msg.name+' in widget '+widget.name);
  }
}

function wmjs_open_widget(url, src_ip, parent_widget) {
    log(l_deb, "wmjs_open_widget");
    var wid;
    if (arguments.length>=3) {
      wid = WidgetManager.load(url, parent_widget);
    } else {
      wid = WidgetManager.load(url);
    }
    if (wid == null) {
        log(l_err, 'File ' + url + ' is not a valid widget');
        return null;
    }
    wid.device = null;
    wid.originating_device = null;
    wid.originating_device_ip = null;
    if (src_ip) {
        wid.permanent = false;
        wid.originating_device_ip = src_ip;
        log(l_inf, 'Widget received - ip ' + wid.originating_device_ip);
    } else {
        wid.permanent = true;
        wid.originating_device_ip = null;
    }
    return wid;
}

/*performs an unbind check on all widgets*/
function wmjs_unbind_widget(widget) {
    var i, count, wid, ifce_count, j;
    log(l_inf, 'wmjs_unbind_widget ' + widget.name);

    if (WidgetManager.upnp && widget.device) {
        UPnP.DeleteDevice(widget.device);
        widget.device = null;
    }

    count = WidgetManager.num_widgets;
    ifce_count = widget.num_interfaces;
    for (i = 0; i < count; i++) {
        wid = WidgetManager.get(i);
        if (!wid) continue;
        if (wid == widget) continue;
        if (!wid.activated) continue;

        for (j = 0; j < ifce_count; j++) {
            var an_ifce = widget.get_interface(j);
            wid.unbind_interface(an_ifce, widget);
        }
    }
    wmjs_bind_widgets();
}


/*performs a bind check on all widgets*/
function wmjs_bind_widgets() {
    log(l_deb, 'wmjs_bind_widgets');
    var i, wid, count;
    count = WidgetManager.num_widgets;
    for (i = 0; i < count; i++) {
        wid = WidgetManager.get(i);
        if (!wid) continue;
        if (!wid.activated) continue;
        WidgetManager.bind(wid);
    }
}


/*performs a bind on the given widget*/
function wmjs_bind_widget(wid) {
    var i, j, ifce_count, device, service, msg, do_bind;

    if (!wid.activated) {
        log(l_inf, 'widget not activated - cannot bind');
        return;
    }
    ifce_count = wid.num_interfaces;

    log(l_inf, 'Binding widget ' + wid.name + ' - Nb Interfaces ' + ifce_count);

    /*browse all interfaces and locate services*/
    for (i = 0; i < ifce_count; i++) {
        var ifce = wid.get_interface(i);
        log(l_inf, 'Binding interface ' + ifce.type);

        /*look for a core:* service for this interface and setup*/
        if (wmjs_bind_interface_to_core_service(wid, ifce)) {
            continue;
        }
        /*if our widget is already bound to this widget, skip it*/
        if (!ifce.multipleBinding && wid.is_interface_bound(ifce)) {
            log(l_deb, 'Widget ' + wid.name + ' interface ' + ifce.type + ' already bound');
            continue;
        }
        /*if the widget is the provider of the service, create the service*/
        if (ifce.serviceProvider) {
            log(l_inf, 'widget is a service provider');
            if (WidgetManager.upnp) wmjs_create_upnp_service(wid, ifce);
            continue;
        }
        /*look for a UPnP service for this interface and setup*/
        if (WidgetManager.upnp && wmjs_bind_interface_to_upnp(wid, ifce)) {
            continue;
        }
        /*look for a local service for this interface*/
        if (wmjs_bind_interface_to_local(wid, ifce)) {
            continue;
        }
        /*no service found*/
        log(l_inf, 'Cannot find service for widget ' + wid.name + ' interface ' + ifce.type);
    }
}

/*called when a new UPnP device has been added or removed in the network*/
function wmjs_on_device_add(device, is_add) {
    log(l_deb, 'wmjs_on_device_add');
    if (!is_add) {
        //TODO JCD: this is wrong, sometimes a device is not "is_add" but is not to be removed either
        /*log(l_inf, 'Device Removed ' + device.Name);
        wmjs_standard_service_remove(device);
        wmjs_unbind_upnp_device(device);
        if (device.widget != null) {
            log(l_inf, 'Widget Removed ' + device.widget.name);
            WidgetManager.on_widget_remove(device.widget);
            device.widget.originating_device = null;
            device.widget = null;
        }
        WidgetManager.check_bindings();*/
        return;
    }
    log(l_inf, 'Device Added ' + device.Name + ' URL ' + device.PresentationURL);
    wmjs_standard_service_add(device);
    WidgetManager.check_bindings();
    /*look for a presentation url - if not given or not identifying a widget, don't do anything*/
    var url = device.PresentationURL;
    if (!url || (url == '')) return;
    if (! WidgetManager.probe(url)) return;
    /*OK load the widget*/
    var widget = WidgetManager.load(url);
    widget.newBorn = true;
    if (widget == null) {
        log(l_err, 'File ' + url + ' is not a valid widget');
        return;
    }
    widget.originating_device = device;
    device.widget = widget;
    /*indicate our widget is not to be stored*/
    widget.permanent = false;
    WidgetManager.on_widget_add(widget);
}


/*
 LOCAL INTERFACE BINDING ROUTINES

 In this implementation, widgets-to-widget communication in the same widget manager are called "local" communications.
 The implementation do not rely on any service description for that and directly matches the widgets interfaces
 */

function wmjs_output_trigger_callback_local(msg_out, wid_src, msg_in, wid_dst) {
    log(l_deb, "wmjs_output_trigger_callback_local");
    return function(value) {
        log(l_deb, "wmjs_output_trigger_callback_local/function " + value + " '" + wid_src.name + "'" + wid_dst.name + "'");
        var param_count = msg_out.num_params;
        var args = new Array();
        var is_script = 0;
        var ai = 0, i;
        if (msg_in.has_script_input) is_script = 1;
        log(l_inf, 'Invoking Widget(' + wid_src.name + ').' + msg_out.name);
        for (i = 0; i < param_count; i++) {
            var param = msg_out.get_param(i);
            if (param.is_input) continue;

            if (is_script) {
                args[ai] = wid_src.get_param_value(param);
                ai++;
            } else {
                wid_dst.set_input(msg_in.get_param(param.name), wid_src.get_param_value(param));
            }
        }
        log(l_inf, 'Calling Widget(' + wid_dst.name + ').' + msg_in.name);

        if (msg_in.has_input_action) {
            wid_dst.call_input_action(msg_in);
        } else if (is_script) {
            wid_dst.call_input_script(msg_in, args);
        }
    };
}


function wmjs_interface_invoke_callback_local(wid_dst, ifce_dst, is_reply) {
    log(l_deb, "wmjs_interface_invoke_callback_local '" + wid_dst.name + "' '" + ifce_dst.type + "' - reply " + is_reply);
    return function() {
        log(l_deb, "wmjs_interface_invoke_callback_local/function '" + wid_dst.name + "' '" + ifce_dst.type + "' - reply " + is_reply);
        var j, i, ai, param_count, msgHandler, msg_src, msg_dst, is_script;
        var args = new Array();
        is_script = 0;
        msgHandler = arguments[0];
        /*get msg from source interface (this object)*/
        msg_src = this.get_message(msgHandler.msgName);
        // JCD: take into account message repetition, if any
        for (j = 0; j < ifce_dst.num_messages; j++) {
            msg_dst = ifce_dst.get_message(j);
            if (msg_dst.name == msgHandler.msgName) {
                log(l_deb, (is_reply ? 'invokeReply ' : 'invoke ') + msg_src.name + ' on ' + wid_dst.name + '.' + msg_dst.name);
                if (msg_dst.has_script_input) is_script = 1;
                param_count = msg_src.num_params;
                ai = 1;
                for (i = 0; i < param_count; i++) {
                    var param = msg_dst.get_param(i);
                    if (! param.is_input) continue;
                    if (is_script) {
                        args[ai - 1] = arguments[ai];
                    } else {
                        wid_dst.set_input(param, arguments[ai]);
                    }
                    ai++;
                }
                if (msg_dst.has_input_action) {
                    wid_dst.call_input_action(msg_dst);
                } else if (is_script) {
                    wid_dst.call_input_script(msg_dst, args);
                }
            }
        }
    };
}

/*
 END OF LOCAL INTERFACE BINDING ROUTINES
 */


function wmjs_bind_interface_to_local(wid, ifce) {
    log(l_deb, "wmjs_bind_interface_to_local '" + wid.name + "' " + ifce.type);
    var i, count, count2, j, a_ifce, a_msg, par, a_par, k, nb_ok, ret, set_bind, a_wid;
    count = WidgetManager.num_widgets;
    ret = false;
    // this is the main loop to try to bind a widget to any other
    for (i = 0; i < count; i++) {
        a_wid = WidgetManager.get(i);
        if (!a_wid) continue;
        if (a_wid == wid) continue;
        if (!a_wid.activated) continue;
        count2 = a_wid.num_interfaces;
        // now that we know this is a valid widget and not the same as wid
        // loop on all its interfaces to find one of the right type
        for (j = 0; j < count2; j++) {
            a_ifce = a_wid.get_interface(j);
            if (a_ifce.type == ifce.type) break;
        }
        if (j == count2) continue;

        /*if our widget is already bound to this widget (multiple binding==true) or to any widget (multiple binding==false), skip it*/
        if (wid.is_interface_bound(ifce, ifce.multipleBinding ? a_wid : null)) {
            log(l_inf, 'Widget ' + wid.name + ' interface ' + ifce.type + ' is already bound');
            continue;
        }
        /*if our widget is already bound to this widget (multiple binding==true) or to any widget (multiple binding==false), skip it*/
        if (a_wid.is_interface_bound(a_ifce, a_ifce.multipleBinding ? wid : null)) {
            log(l_inf, 'Widget ' + a_wid.name + ' interface ' + a_ifce.type + ' is already bound');
            continue;
        }

        set_bind = 0;
        // we have found an interface of the right type in the other widget
        log(l_inf, 'Checking local widget ' + a_wid.name + ' for interface ' + ifce.type);
        // loop on the messages to check if they match
        for (j = 0; j < ifce.num_messages; j++) {
            var msg = ifce.get_message(j), l;
            //if (msg.is_input) continue;  // JCD: remove test
            // does the other interface have this message
            // JCD: remove next line and take repetition of messages into account
            // a_msg = a_ifce.get_message(msg.name);
            for (l = 0; l < a_ifce.num_messages; l++) {
                a_msg = a_ifce.get_message(l);
                //alert(msg.name+" "+a_msg.name);
                if (a_msg.name == msg.name) {
                    // the messages have matching names, check direction
                    if (msg.is_input == a_msg.is_input) {
                        log(l_inf, 'Local widget message for ' + msg.name + ' is not in direction ' + (msg.is_input ? 'output' : 'input'));
                        continue;
                    }
                    // the messages have matching names and directions, check params
                    if (msg.num_params != a_msg.num_params) {
                        log(l_war, 'Local widget message ' + msg.name + ' does not have the same number of parameters');
                        continue;
                    }
                    /*check all params*/
                    nb_ok = 0;
                    for (k = 0; k < msg.num_params; k++) {
                        par = msg.get_param(k);
                        a_par = a_msg.get_param(par.name);
                        if (a_par != null && par.is_input != a_par.is_input) nb_ok ++;
                    }
                    if (nb_ok != msg.num_params) {
                        log(l_war, 'Local widget message ' + msg.name + ' does not have the same input/output parameters');
                        continue;
                    }
                    set_bind ++;
                    // the messages match
                    log(l_inf, 'Binding ' + wid.name + '.' + msg.name + ' to ' + a_wid.name + '.' + a_msg.name);
                    /*OK let's bind this action: we only need to assign the output trigger, the input action will be called from the other widget*/
                    if (msg.has_output_trigger) {
                        wmjs_bind_output_trigger(wid, msg, wmjs_output_trigger_callback_local(msg, wid, a_msg, a_wid), a_wid);
                    }
                    /*OK let's bind this action: we only need to assign the output trigger, the input action will be called from the other widget*/
                    if (a_msg.has_output_trigger) {
                        wmjs_bind_output_trigger(a_wid, a_msg, wmjs_output_trigger_callback_local(a_msg, a_wid, msg, wid), wid);
                    }
                }
            }
        }
        if (!set_bind) continue;

        /*create callback for programmatic action triggers*/
        ifce.invoke = wmjs_interface_invoke_callback_local(a_wid, a_ifce, 0);
        ifce.invokeReply = wmjs_interface_invoke_callback_local(a_wid, a_ifce, 1);
        /*create callback for programmatic action triggers*/
        a_ifce.invoke = wmjs_interface_invoke_callback_local(wid, ifce, 0);
        a_ifce.invokeReply = wmjs_interface_invoke_callback_local(wid, ifce, 1);
        a_wid.bind_interface(a_ifce, wid, 'localhost');
        wid.bind_interface(ifce, a_wid, 'localhost');
        ret = true;
    }
    return ret;
}


/*
 INTERFACE BINDING TO UPNP SERVICES ROUTINES

 This implemntation supports binding interfaces to existing UPnP services
 */

/*create a listener function for input parameters*/
function wmjs_upnp_action_listener(widget, par, msg) {
    log(l_deb, "wmjs_upnp_action_listener '" + widget.name + "' " + (par != null ? par.name : 'noPar') + " " + (msg != null ? msg.name : 'noMsg'));
    return function(value) {
        log(l_deb, "wmjs_upnp_action_listener/function '" + widget.name + "' " + (par != null ? par.name : 'noPar') + " " +
                   (msg != null ? msg.name : 'noMsg') + " " + value);
        if (par != null) widget.set_input(par, value);
        if (msg != null) widget.call_input_action(msg);
    }
}

/*create a listener function for input parameters*/
function wmjs_upnp_action_listener_script(widget, msg) {
    log(l_deb, 'wmjs_upnp_action_listener_script');
    return function() {
        var i, count, act, par, rval, msgHandler, args;
        act = arguments[0];
        log(l_deb, 'wmjs_upnp_action_listener_script/function:nb args ' + arguments.length);
        msgHandler = arguments[1];
        log(l_deb, 'wmjs_upnp_action_listener_script/function:msgh ' + msgHandler);
        args = new Array();
        count = msg.num_params;
        for (i = 0; i < count; i++) {
            par = msg.get_param(i);
            if (!par.is_input) continue;
            rval = act.GetArgumentValue(par.name);
            if (par.script_type == 'number') {
                args.push(parseInt(rval));
            } else if (par.script_type == 'boolean') {
                args.push(rval == 'true');
            } else {
                args.push(rval);
            }
        }
        msgHandler.onInvokeReply(args);
    }
}


/**/
function wmjs_output_trigger_callback_upnp(service, widget, msg) {
    log(l_deb, 'wmjs_output_trigger_callback_upnp');
    return function(value) {
        log(l_deb, 'wmjs_output_trigger_callback_upnp/function ' + value);
        var i, pi, param_count = msg.num_params;
        var args = new Array();
        pi = 0;
        for (i = 0; i < param_count; i++) {
            var param = msg.get_param(i);
            if (param.is_input) continue;
            args[pi] = param.name;
            args[pi + 1] = widget.get_param_value(param);
            pi += 2;
        }
        log(l_inf, 'Calling UPnP Action ' + msg.name + " " + param_count);
        service.CallAction(msg.name, args);
    };
}


/*create the action callback*/
function wmjs_message_setup_upnp(widget, service, msg) {
    log(l_deb, 'wmjs_message_setup_upnp ' + msg.name);
    /*first let's browse all the message params*/
    var rad, i, param_count, has_script_input;
    has_script_input = false;
    param_count = msg.num_params;
    for (i = 0; i < param_count; i++) {
        var param = msg.get_param(i);
        /*interface not valid for this service*/
        if (!service.HasAction(msg.name, param.name)) {
            if (!param.is_input) {
                log(l_war, 'Output param ' + param.name + ' not found in action - cannot bind action');
                return false;
            }
            continue;
        }
        /*this is a service -> widget param */
        if (param.is_input) {
            if (msg.has_input_action) {
                service.SetActionListener(msg.name, wmjs_upnp_action_listener(widget, param, null), param.name);
            } else {
                has_script_input = true;
                // todo: pas vrai d'apres JCD
            }
        }
        /*this is a widget -> service param , used in callback function*/
    }
    /*assign the output trigger*/
    if (msg.has_output_trigger) {
        var fun_name = 'call_' + msg.name;
        widget[fun_name] = wmjs_output_trigger_callback_upnp(service, widget, msg);
        wmjs_bind_output_trigger(widget, msg, widget[fun_name], service);
    }
    /*assign the input action*/
    if (msg.has_input_action) {
        service.SetActionListener(msg.name, wmjs_upnp_action_listener(widget, null, msg));
    } else if (has_script_input) {
        service.SetActionListener(msg.name, wmjs_upnp_action_listener_script(widget, msg), true);
    }
    return true;
}


function wmjs_interface_invoke_callback_upnp(service) {
    log(l_deb, 'wmjs_interface_invoke_callback_upnp');
    return function() {
        log(l_deb, 'wmjs_interface_invoke_callback_upnp/function');
        var args = new Array();
        var i, pi, ai, param_count, msg, ifce_msg;

        msg = arguments[0];
        ifce_msg = this.get_message(msg.msgName);
        param_count = ifce_msg.num_params;
        log(l_inf, 'UPnP invoke action ' + ifce_msg.name + ' - ' + msg.msgName);

        pi = 0;
        ai = 1;
        for (i = 0; i < param_count; i++) {
            var param = ifce_msg.get_param(i);
            if (param.is_input) continue;

            args[pi] = param.name;
            args[pi + 1] = arguments[ai];
            pi += 2;
            ai++;
        }
        service.CallAction(msg.msgName, args, msg);
    };
}


function wmjs_bind_interface_to_upnp(wid, ifce) {
    log(l_deb, "wmjs_bind_interface_to_upnp");
    var do_bind, is_upnp = false;
    var service = null;

    if (wid.originating_device) {
        service = wid.originating_device.FindService(ifce.type);
    } else if (wid.originating_device_ip) {
        service = UPnP.FindService(wid.originating_device_ip, ifce.type);
    } else {
        var j, dev_count = UPnP.DevicesCount;
        log(l_deb, 'wmjs_bind_interface_to_upnp nb ' + dev_count);
        for (j = 0; j < dev_count; j++) {
            device = UPnP.GetDevice(j);
            log(l_deb, "wmjs_bind_interface_to_upnp " + device.Name + ' - type: ' + ifce.type);
            /*do we have a UPnP service with the same interface type ?*/
            service = device.FindService(ifce.type);

            /*our widget is not bound to this service*/
            if (service) {
                if (!wid.is_interface_bound(ifce, service)) break;
                service = null;
                is_upnp = true;
            }
        }
    }
    if (!service) return is_upnp;

    /*our widget is already bound to this service*/
    if (wid.is_interface_bound(ifce, service)) {
        log(l_inf, 'Found UPnP Service for interface ' + ifce.type);
        return true;
    }

    log(l_inf, 'Found UPnP Service for interface ' + ifce.type);
    do_bind = 0;
    for (j = 0; j < ifce.num_messages; j++) {
        var msg = ifce.get_message(j);
        /*if we have a state variable in the service with the same name as the pin, bind it*/
        if (service.HasStateVariable(msg.name)) {
            var param = msg.get_param(0);
            if (param && (param.name == msg.name)) {
                /*set listeners for declarative action triggers*/
                log(l_deb, 'Found UPnP state variable for message ' + msg.name);
                service.SetStateVariableListener(wmjs_upnp_action_listener(wid, param, msg), param.name);
                do_bind = 1;
            }
        }
        /*if we have an action in the service with the same name as the pin, bind it*/
        else if (service.HasAction(msg.name)) {
            /*set listeners for declarative action triggers*/
            log(l_deb, 'Found UPnP action for message ' + msg.name);
            wmjs_message_setup_upnp(wid, service, msg);
            do_bind = 1;
        }
        /*NOTE: UPnP actions are only messageOut, messageIn are UPnP events and don't have output paramaters. We therefore never need to setup
         the invokeReply callback for interfaces matching UPnP services. !! This is not true for UPnP services created to publish an interface, eg serviceProvider="true".*/
    }
    if (do_bind) {
        /*create callback for programmatic action triggers*/
        log(l_inf, 'Binding widget ' + wid.name + ' interface ' + ifce.type + ' to host ' + service.Hostname);
        ifce.invoke = wmjs_interface_invoke_callback_upnp(service);
        /*bind interface*/
        wid.bind_interface(ifce, service, service.Hostname);
    }
    return true;
}

function wmjs_unbind_upnp_device(device)
{
    var i, count, j, ifce_count;
    count = WidgetManager.num_widgets;

    for (i = 0; i < count; i++) {
        wid = WidgetManager.get(i);
        if (!wid) continue;
        if (!wid.activated) continue;

        ifce_count = wid.num_interfaces;
        for (j = 0; j < ifce_count; j++) {
            var ifce = wid.get_interface(j);
            /*do we have a UPnP service with the same interface type ?*/
            var service = device.FindService(ifce.type);
            if (!service) continue;

            /*unbind this interface*/
            if (wid.is_interface_bound(ifce, service)) {
                wid.unbind_interface(ifce, service);
            }
        }
    }
}

/*
 END OF INTERFACE BINDING TO UPNP SERVICES ROUTINES
 */

//
// section on widget migration
//

header = '<scpd xmlns="urn:schemas-upnp-org:service-1-0"><specVersion><major>1</major><minor>0</minor></specVersion><actionList>';
middler = '</actionList><serviceStateTable>';
footer = '</serviceStateTable></scpd>';
standard_service_scpd = header+'<action><name>startWidget</name><argumentList><argument><name>widgetUrl</name><direction>in</direction><relatedStateVariable>s1</relatedStateVariable></argument><argument><name>widgetUUID</name><direction>in</direction><relatedStateVariable>s2</relatedStateVariable></argument><argument><name>widgetContext</name><direction>in</direction><relatedStateVariable>s3</relatedStateVariable></argument><argument><name>errorCode</name><direction>out</direction><relatedStateVariable>s4</relatedStateVariable></argument></argumentList></action>';
{
    standard_service_scpd += '<action><name>requestCapabilitiesList</name><argumentList><argument><name>capabilitiesType</name><direction>out</direction><relatedStateVariable>s5</relatedStateVariable></argument><argument><name>capabilities</name><direction>out</direction><relatedStateVariable>s6</relatedStateVariable></argument></argumentList></action>';
    standard_service_scpd += '<action><name>listWidgets</name><argumentList><argument><name>widgetCodes</name><direction>out</direction><relatedStateVariable>s7</relatedStateVariable></argument><argument><name>widgetNames</name><direction>out</direction><relatedStateVariable>s8</relatedStateVariable></argument><argument><name>widgetIdentifiers</name><direction>out</direction><relatedStateVariable>s9</relatedStateVariable></argument></argumentList></action>';
    standard_service_scpd += '<action><name>getWidget</name><argumentList><argument><name>widgetCode</name><direction>in</direction><relatedStateVariable>s10</relatedStateVariable></argument><argument><name>errorCode</name><direction>out</direction><relatedStateVariable>s11</relatedStateVariable></argument>';
    standard_service_scpd += '<argument><name>widgetUrl</name><direction>out</direction><relatedStateVariable>s41</relatedStateVariable></argument><argument><name>widgetUUID</name><direction>out</direction><relatedStateVariable>s42</relatedStateVariable></argument><argument><name>widgetContext</name><direction>out</direction><relatedStateVariable>s43</relatedStateVariable></argument></argumentList></action>';
    standard_service_scpd += middler+'<stateVariable><name>s1</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s2</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s3</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s4</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s5</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s6</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s7</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s8</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s9</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s10</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s11</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s41</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s42</name><dataType>string</dataType></stateVariable>';
    standard_service_scpd += '<stateVariable><name>s43</name><dataType>string</dataType></stateVariable>'+footer;
}

//
// migrate a widget by calling the appropriate startWidget message of the target
// migration service
//
function wmjs_migrate_widget(render, widget) {
    if (WidgetManager.upnp) {
        var url, ctx, uri;

        url = widget.url;
        /*share the widget*/
        uri = UPnP.ShareResource(url, render.HostName);

        ctx = widget.get_context();

        if (ctx == null) ctx = "";
        log(l_inf, 'Migrating widget ' + url + ' to renderer ' + render.Name + ' as resource ' + uri);
        log(l_inf, 'Migration Context ' + ctx);

        // envoyer la methode startWidget sur render
        var args = new Array();
        args.push("widgetUrl");
        args.push(uri);
        args.push("widgetUUID");
        args.push(null);
        args.push("widgetContext");
        args.push(ctx);
        log(l_inf, "startWidget "+uri);
        log(l_inf, "startWidgetCtx "+ctx);
        alert("call StdServ "+render.Name+" "+render.standardService);
        var code = render.standardService.CallAction("startWidget", args);
        log(l_inf, "return code from standardService.CallAction is "+code);
    }
}

//
// check whether the removed device was a publisher of MPEGUStandardService
// if yes, remove it from the list
//
function wmjs_standard_service_remove(device) {
    var service = device.FindService("urn:mpeg-u:service:widget-manager:1");
    if (service != null) {
        var i = WidgetManager.MPEGUStandardServiceProviders.indexOf(device);
        if (i >= 0) {
            log(l_err, "migration service remove "+device.Name);
            WidgetManager.MPEGUStandardServiceProviders.splice(i, 1);
        } else log(l_err, "Trying to remove a standard service device which was not added first: "+device);
    }
}

//
// add a migration service provider to the list and remember the service in the device
//
function wmjs_standard_service_add(device) {
    var service = device.FindService("urn:mpeg-u:service:widget-manager:1");
    if (service != null) {
        log(l_err, "migration service add "+device.Name);
        WidgetManager.MPEGUStandardServiceProviders.push(device);
        device.standardService = service;
        alert("add StdServ "+device.Name+" "+device.standardService);
    }
}

//
// get the nth standard service provider (migration service)
//
function wmjs_get_mpegu_service_providers(index) {
    if (!WidgetManager.MPEGUStandardServiceProviders) {
        log(l_err, "uninitialized MPEGUStandardServiceProviders");
        return null;
    }
    if (index < 0 || index > WidgetManager.MPEGUStandardServiceProviders.length) {
        log(l_err, "index of MPEGUStandardServiceProviders out of bounds:"+index);
        return null;
    }
    return WidgetManager.MPEGUStandardServiceProviders[index];
}

function wmjs_create_standard_service() {
    //alert("create_standard_service");
    var name = "MPEG-U";
    var option = scene.get_option('core', 'devclass');
    if (option) name = option;
    WidgetManager.device = UPnP.CreateDevice("urn:mpeg-u:device:widget-manager:1", name+"@"+Sys.hostname);
    WidgetManager.device.enabled = 1;
    /* implement the response to a external call (messageOut of another widget) */
    WidgetManager.device.OnAction = wmjs_widget_standard_service_process_action;
    log(l_err, 'wmjs_create_standard_service');
    //log(l_inf, 'Service scpd ' + standard_service_scpd);
    /* WidgetManager.standardService = */
    var service = WidgetManager.device.SetupService("MPEG-U_Standard_Service", "urn:mpeg-u:service:widget-manager:1",
            "urn:mpeg-u:serviceId:widget-manager:1.001", standard_service_scpd);
    WidgetManager.device.Start();
    WidgetManager.MPEGUStandardServiceProviders = new Array();
    //alert("create_standard_service end");
}

/* generic processing of any action from outside for the standard MPEG-U service */
function wmjs_widget_standard_service_process_action(action) {
    log(l_err, 'wmjs_widget_standard_service_process_action Action ' + action.Name);
    /* find the message matching the action in the interface */
    if (action.Name == "startWidget") {
        process_startWidget_action(action);
    } else if (action.Name == "requestCapabilitiesList"){
        process_requestCapabilitiesList_action(action);
    } else if (action.Name == "listWidgets") {
        process_listWidgets_action(action);
    } else if (action.Name == "getWidget") {
        process_getWidget_action(action);
    } else {
        log(l_inf, 'wmjs_widget_standard_service_process_action Action not found: ' + action.Name);
    }
}

//
// processing of a request to start a widget (push migration or predefined "activateWidgetByUrl")
//
function process_startWidget_action(action) {
    log(l_err,"process start widget");
    // get action arguments
    var widgetUrl = action.GetArgument("widgetUrl");
    var widgetUUID = action.GetArgument("widgetUUID");
    var widgetContext = action.GetArgument("widgetContext");
    log(l_err,"start:"+widgetUrl+" "+widgetUUID+" "+widgetContext);
    // get widget
    if (widgetUrl == null && widgetUUID != null) {
        log(l_err, "unable to load a widget by UUID: unimplemented");
        action.SendReply(["errorCode", 1]);
        return;
    }
    var wid = WidgetManager.load(widgetUrl, null, widgetContext);
    WidgetManager.on_widget_add(wid);
    action.SendReply(["errorCode", 0]);
}

//
// processing of a request for the list of capabilities
//
function process_requestCapabilitiesList_action(action) {
    //TODO
    alert("UNIMPLEMENTED in mpegu-core.js: request capabilities list");
}

//
// processing of a request for the list of widgets (prelude to a pull migration)
//
function process_listWidgets_action(action) {
    log(l_err,"process list widgets");
    // list widget
    var count = WidgetManager.num_widgets, i, r1 = "", r2 = "", includeSpace = false;
    for (i = 0; i < count; i++) {
        var wid = WidgetManager.get(i);
        if (wid.activated) {
            if (includeSpace) {
                r1 += " ";
            } else {
                includeSpace = true;
            }
            r1 += i+1;
            r2 += wid.name.replace(whiteSpaceRegExp, '_')+" ";
        }
    }
    alert("sendReply widgetCodes=|"+r1+"| widgetNames=|"+r2+"|");
    action.SendReply(["widgetCodes", r1, "widgetNames", r2]);
}

//
// processing of a pull migration request
//
function process_getWidget_action(action) {
    log(l_deb, "process get widget "+action+" "+action.Name);
    // get action arguments
    var widgetCode = action.GetArgument("widgetCode");
    log(l_deb, "get:"+widgetCode);
    // get widget
    var wid = WidgetManager.get(parseInt(widgetCode) - 1);
    var url, ctx, uri;
    url = wid.url;
    /*share the widget*/
    uri = UPnP.ShareResource(url);
    ctx = wid.get_context();
    if (ctx == null) ctx = "";
    log(l_inf, 'sendReply widget ' + url + ' as resource ' + uri);
    log(l_inf, 'with context ' + ctx);
    var args = new Array();
    args.push("errorCode");
    args.push(0);
    args.push("widgetUrl");
    args.push(uri);
    args.push("widgetUUID");
    args.push(null);
    args.push("widgetContext");
    args.push(ctx);
    action.SendReply(args);
    WidgetManager.on_widget_remove(wid);
}

//
// end of migration service
//

/*
 INTERFACE PUBLISHING AS UPNP SERVICES ROUTINES

 This implementation supports publishing a widget interface in the network as a UPnP service. This is done
 by generating on the fly a new device associated with a widget, and one UPnP service per widget interface.
 The UPnP service description (SCPD) is generated on the fly from the interface description
 */

function wmjs_make_interface_scpd(widget, ifce) {
    log(l_deb, "wmjs_make_interface_scpd");
    var variables = '', actions = '', vars, j, msg, k, param, numstatevar = 0;
    /*do actions*/
    for (j = 0; j < ifce.num_messages; j++) {
        msg = ifce.get_message(j);
        actions += '<action><name>' + msg.name + '</name><argumentList>';
        for (k = 0; k < msg.num_params; k++) {
            param = msg.get_param(k);
            actions += '<argument><name>' + param.name + '</name><direction>' + (param.is_input ? 'in' : 'out');
            actions += '</direction><relatedStateVariable>v' + numstatevar + '</relatedStateVariable></argument>';
            numstatevar++;
        }
        actions += '</argumentList></action>';
    }
    /*do variables*/
    for (k = 0; k < numstatevar; k++) {
        variables += '<stateVariable><name>v' + k + '</name><dataType>string</dataType></stateVariable>';
    }
    return header + actions + middler + variables + footer;
}

/* generic processing of any action from outside */
function wmjs_widget_upnp_process_action(action) {
    log(l_err, 'wmjs_widget_upnp_process_action Action ' + action.Name + ' invoked on ' + action.Service.ifce.type);
    var i, ai, args, msg, has_output = false;
    /* find the message matching the action in the interface */
    msg = null;
    for (i = 0; i < action.Service.ifce.num_messages; i++) {
        msg = action.Service.ifce.get_message(i);
        if (msg.name == action.Name) break;
        msg = null;
    }
    if (msg == null) {
        log(l_inf, 'wmjs_widget_upnp_process_action Action not found: ' + action.Name + ' invoked on ' + action.Service.name);
        return;
    }
    args = new Array();
    ai = 0;
    /*assign action for any async callback from the scene (cf wmjs_upnp_action_response)*/
    action.Service.action = action;
    /* for each param in the found message */
    for (i = 0; i < msg.num_params; i++) {
        var param = msg.get_param(i);
        /* only process input params */
        if (!param.is_input) {
            has_output = true;
            continue;
        }
        if (msg.has_script_input) {
            /* input param is scripted, store argument value in args array */
            args[ai] = action.GetArgument(param.name);
        } else {
            /* input param is not scripted but declarative, store argument value directly in scene tree */
            this.widget.set_input(param, action.GetArgument(param.name));
        }
        ai++;
    }
    if (msg.has_input_action) {
        /* message is not scripted, call input action */
        this.widget.call_input_action(msg);
    } else if (msg.has_script_input) {
        /* message is scripted, call the script function specified in inputAction */
        this.widget.call_input_script(msg, args);
    }
    if (!has_output) {
        log(l_inf, 'wmjs_widget_upnp_process_action sending empty reply');
        action.SendReply();
        action.Service.action = null;
    }
}

function wmjs_upnp_event_sender(service, widget, msg) {
    return function(value) {
        log(l_deb, 'wmjs_upnp_event_sender/function ' + service.ifce.type + " " + widget.name + " " + msg.name);
        var i, pi, param_count = msg.num_params;
        var args = new Array();
        pi = 0;
        for (i = 0; i < param_count; i++) {
            var param = msg.get_param(i);
            if (param.is_input) continue;
            args[pi] = param.name;
            args[pi + 1] = widget.get_param_value(param);
            pi += 2;
        }
        log(l_inf, 'sending UPnP event ' + msg.name);
        service.CallAction(msg.name, args);
    };
}

function wmjs_upnp_action_response(service, widget, msg) {
    return function(value) {
        if (service.action == null) {
            // TODO this code avoids the bug that this function is called twice: find out why it is called twice!
            log(l_inf, 'wmjs_upnp_action_response/function DUPL(return) ' + service.ifce.type + " '" + widget.name + "' " + msg.name);
            return;
        }
        var i, pi, param_count = msg.num_params;
        var args = new Array();
        pi = 0;
        for (i = 0; i < param_count; i++) {
            var param = msg.get_param(i);
            if (param.is_input) continue;
            args[pi] = param.name;
            args[pi + 1] = widget.get_param_value(param);
            pi += 2;
        }
        log(l_inf, 'UPnP sending reply on action ' + service.action + ': ' + service.ifce.type + " '" + widget.name + "' " + msg.name);
        service.action.SendReply(args);
        service.action = null;
    };
}


function wmjs_upnp_action_response_script(service) {
    return function() {
        if (service.action == null) {
            // this code avoids the bug that this function is called twice: find out why it is called twice!
            return;
        }
        var i, pi, ai, param_count;
        var args = new Array();
        var msgHandler = arguments[0];
        var msg = service.ifce.get_message(msgHandler.msgName);

        pi = 0;
        ai = 1;
        for (i = 0; i < param_count; i++) {
            var param = msg.get_param(i);
            if (param.is_input) continue;
            args[pi] = param.name;
            args[pi + 1] = arguments[ai];
            pi += 2;
            ai++;
        }
        log(l_inf, 'UPnP sending reply on action ' + service.action);
        service.action.SendReply(args);
        service.action = null;
    };
}


function wmjs_create_upnp_service(widget, ifce) {
    var i, service, scpd, start_device = 0;
    var name;

    log(l_deb, 'widget device : ' + widget.device);

    /* at least one interface is a provider, so create a device - service or device names SHALL NOT CONTAIN SPACES*/
    name = widget.name.replace(whiteSpaceRegExp, '_');
    if (!widget.device) {
        //log(l_inf,'creating device');
        widget.device = UPnP.CreateDevice("urn:mpeg-u:device:provider-" + name + ":1", name);
        /* remember the widget in the device */
        widget.device.widget = widget;
        widget.device.enabled = 1;
        start_device = 1;
        /* implement the response to a external call (messageOut of another widget) */
        widget.device.OnAction = wmjs_widget_upnp_process_action;
    }

    /*service has already been created*/
    if (typeof(ifce.started) != 'undefined') return;

    log(l_inf, 'wmjs_create_upnp_service');
    scpd = wmjs_make_interface_scpd(widget, ifce);
    //log(l_inf, 'Service scpd ' + scpd);
    service = widget.device.SetupService(name, ifce.type, ifce.type + ".001", scpd);
    service.ifce = ifce;

    /* for each message in a provider interface */
    for (i = 0; i < ifce.num_messages; i++) {
        msg = ifce.get_message(i);
        /* if there is an output_trigger, a reply may be triggered after a call */
        if (msg.has_output_trigger) {
            if (msg.is_input) {
                /* output in an input message => this is a reply */
                var fun_name1 = 'respond_' + msg.name;
                widget[fun_name1] = wmjs_upnp_action_response(service, widget, msg);
                wmjs_bind_output_trigger(widget, msg, widget[fun_name1], service);
            } else {
                /* output in an output message => this is an event */
                var fun_name2 = 'send_event_' + msg.name;
                widget[fun_name2] = wmjs_upnp_event_sender(service, widget, msg);
                wmjs_bind_output_trigger(widget, msg, widget[fun_name2], service);
            }
        }
        /*if message is input, a reply may be sent*/
        else if (msg.is_input) {
            var j, has_out;
            has_out = 0;
            for (j = 0; j < msg.num_params; j++) {
                var param = msg.get_param(j);
                if (param.is_input) continue;
                has_out = 1;
                break;
            }
            if (has_out) {
                ifce.invokeReply = wmjs_upnp_action_response_script(service);
            }
        }
    }
    ifce.started = true;

    if (start_device) {
        //log(l_inf,"device start");
        widget.device.Start();
    }
}


/*
 END OF INTERFACE PUBLISHING AS UPNP SERVICES ROUTINES
 */

/*********************************************************
 Start of implementation of core:in and core:out interfaces
 *********************************************************/

coreIn = null, coreOut = null;

//
// function defining messages for the core:* interfaces implemented by the WM
//
function defineMessage(name, direction, executeFunction, params1, params2) {
    var msg = new Object();
    msg.name = name;
    msg.is_input = !direction; // the standard defines the point of view of the widget, not that of the WM
    msg.num_params = 0;
    // the executeFunction is the function implemented by widman.js and
    // which passes the buck to a function of the widget manager, if there is one
    if (executeFunction != null) msg.execute = executeFunction;
    // in a messageOut, params1 contains outputs and params2 contains inputs
    // in a messageIn, params1 contains inputs and params2 contains outputs
    if (direction) {
        if (params1 != null) {
            msg.paramsOut = params1;
            msg.num_params += params1.length;
        }
        if (params2 != null) {
            msg.paramsIn = params2;
            msg.num_params += params2.length;
        }
    } else {
        if (params1 != null) {
            msg.paramsIn = params1;
            msg.num_params += params1.length;
        }
        if (params2 != null) {
            msg.paramsOut = params2;
            msg.num_params += params2.length;
        }
    }
    //alert("Message defined: "+msg.name+' '+msg.num_params+' '+msg.is_input+' '+
    //      (msg.paramsIn != null ? msg.paramsIn.length : 0)+' '+
    //      (msg.paramsOut != null ? msg.paramsOut.length : 0));
    return msg;
}

//
// function defining the parameter lists for messages
// the direction of a parameter is implicit in its position in the paramsIn or paramsOut of a message
//
function defineParams() {
    var params = new Array();
    var i = 0;
    for (; i < arguments.length; i++) {
        params[i] = arguments[i];
    }
    return params;
}

//
// get message by name in a core interface (core:in or core:out
//
function getMessage(core, s) {
    var i = 0;
    for (; i < core.length; i++) {
        if (core[i].name == s) return core[i];
    }
    return null;
}

//
// get a parameter from a message by name
//
function hasParam(msg, name) {
    var i = 0;
    if (msg.paramsIn) {
        for (i = 0; i < msg.paramsIn.length; i++) {
            if (msg.paramsIn[i] == name) return true;
        }
    }
    if (msg.paramsOut) {
        for (i = 0; i < msg.paramsOut.length; i++) {
            if (msg.paramsOut[i] == name) return true;
        }
    }
    return false;
}

//
// get a parameter from a message by index
//
/*function getParam(msg, i) {
    if (!msg.is_input) {
        if (msg.paramsIn) {
            if (i < msg.paramsIn.length) {
                return msg.paramsIn[i];
            } else {
                i -= msg.paramsIn.length;
            }
        }
        if (msg.paramsOut) {
            if (i < msg.paramsOut.length) {
                return msg.paramsOut[i];
            }
        }
    } else {
        if (msg.paramsOut) {
            if (i < msg.paramsOut.length) {
                return msg.paramsOut[i];
            } else {
                i -= msg.paramsOut.length;
            }
        }
        if (msg.paramsIn) {
            if (i < msg.paramsIn.length) {
                return msg.paramsIn[i];
            }
        }
    }
    return null;
}*/

//
// get a parameter from a message by index
//
function getInputParam(msg, i) {
    if (!msg.is_input) {
        if (msg.paramsIn) {
            if (i < msg.paramsIn.length) {
                return msg.paramsIn[i];
            } else {
                return null;
            }
        }
    } else {
        if (msg.paramsOut) {
            if (i < msg.paramsOut.length) {
                return null;
            } else {
                i -= msg.paramsOut.length;
            }
        }
        if (msg.paramsIn) {
            if (i < msg.paramsIn.length) {
                return msg.paramsIn[i];
            }
        }
    }
    return null;
}

//
// reconstruct the direction of a parameter of a message by its position
//
function paramDirection(msg, name) {
    var i = 0;
    if (msg.paramsIn) {
        for (i = 0; i < msg.paramsIn.length; i++) {
            if (msg.paramsIn[i] == name) return true;
        }
    }
    if (msg.paramsOut) {
        for (i = 0; i < msg.paramsOut.length; i++) {
            if (msg.paramsOut[i] == name) return false;
        }
    }
    return false;
}

function initCore() {
    /*
     Methods implementing messageIn of the core interfaces
     In these methods, this is the message and args is the list of arguments of the message
     widman.js, as the common part of all widget managers, does not implement these methods directly
     but passes the buck to specific WM implementations.
     In specific WM implementation, an initializer function should contain
     WidgetManager.coreOutSetSize = function (...) {...};
     or similar.
     */
    WidgetManager.coreOutSetSize = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.setSize" + argsString);
    };

    WidgetManager.coreOutShow = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.show" + argsString);
    };

    WidgetManager.coreOutHide = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.hide" + argsString);
    };

    WidgetManager.coreOutRequestActivate = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.requestActivate" + argsString);
    };

    WidgetManager.coreOutRequestDeactivate = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.requestDeactivate" + argsString);
    };

    WidgetManager.coreOutShowNotification = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.showNotification" + argsString);
    };

    WidgetManager.coreOutPlaceComponent = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.placeComponent" + argsString);
    };

    WidgetManager.coreOutGetAttention = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.getAttention" + argsString);
    };
    WidgetManager.coreOutInstallWidget = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.installWidget" + argsString);
    };
    WidgetManager.coreOutMigrateComponent = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.migrateComponent" + argsString);
    };
    WidgetManager.coreOutRequestMigrationTargets = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.requestMigrationTargets" + argsString);
    };
    WidgetManager.coreOutActivateTemporaryWidget = function(wid, args) {
        var i = 0, argsString = "";
        for (; i < args.length; i++) argsString += " " + args[i];
        log(l_deb, "*** defCoreOutImpl.activateTemporaryWidget" + argsString);
    };
    /*
     Define the core:* interfaces that the widget manager implements and provides to widgets
     The structure of messages is simplified.
     messageIn have first an array of paramsIn, then an array of paramsOut for a reply
     messageOut have first an array of paramsOut, then an array of paramsIn for reply
     Because of this simplification, getParam and defineMessage are dependent on the direction of the message
     The direction of a parameter is implicit in the fact that it is stored in paramsIn or paramsOut of the message
     */
    coreIn = new Array();
    coreOut = new Array();
    coreIn[0] = defineMessage("setSize", true, null, defineParams("width", "height", "dpi"));
    coreIn.setSizeMessage = coreIn[0];
    coreIn[1] = defineMessage("show", true, null);
    coreIn.showMessage = coreIn[1];
    coreIn[2] = defineMessage("hide", true, null);
    coreIn.hideMessage = coreIn[2];
    coreIn[3] = defineMessage("activate", true, null);
    coreIn.activateMessage = coreIn[3];
    coreIn[4] = defineMessage("deactivate", true, null);
    coreIn.deactivateMessage = coreIn[4];
    coreIn.type = "urn:mpeg:mpegu:schema:widgets:core:in:2010";
    coreOut[0] = defineMessage("setSize", false, coreOutSetSize, defineParams("width", "height"));
    coreOut.setSizeMessage = coreOut[0];
    coreOut[1] = defineMessage("show", false, coreOutShow);
    coreOut.showMessage = coreOut[1];
    coreOut[2] = defineMessage("hide", false, coreOutHide);
    coreOut.hideMessage = coreOut[2];
    coreOut[3] = defineMessage("requestActivate", false, coreOutRequestActivate, null,
            defineParams("returnCode"));
    coreOut.requestActivateMessage = coreOut[3];
    coreOut[4] = defineMessage("requestDeactivate", false, coreOutRequestDeactivate, null,
            defineParams("returnCode"));
    coreOut.requestDeactivateMessage = coreOut[4];
    coreOut[5] = defineMessage("showNotification", false, coreOutShowNotification,
            defineParams("message"), defineParams("returnCode"));
    coreOut.showNotificationMessage = coreOut[5];
    coreOut[6] = defineMessage("placeComponent", false, coreOutPlaceComponent,
            defineParams("componentID", "x", "y", "w", "h", "z-index", "transparency"),
            defineParams("returnCode"));
    coreOut.placeComponentMessage = coreOut[6];
    coreOut[7] = defineMessage("getAttention", false, coreOutGetAttention,
            null, defineParams("returnCode"));
    coreOut.getAttentionMessage = coreOut[7];
    coreOut[8] = defineMessage("installWidget", false, coreOutInstallWidget,
            defineParams("url"), defineParams("returnCode"));
    coreOut.installWidgetMessage = coreOut[8];
    coreOut[9] = defineMessage("migrateComponent", false, coreOutMigrateComponent,
            defineParams("componentId", "targetCode"), defineParams("returnCode"));
    coreOut.migrateComponentMessage = coreOut[9];
    coreOut[10] = defineMessage("requestMigrationTargets", false, coreOutRequestMigrationTargets,
            null, defineParams("targetCodes", "targetNames", "targetDescriptions"));
    coreOut.requestMigrationTargetsMessage = coreOut[10];
    coreOut[11] = defineMessage("activateTemporaryWidget", false, coreOutActivateTemporaryWidget,
            defineParams("url"), defineParams("returnCode"));
    coreOut.activateTemporaryWidget = coreOut[11];
    coreOut.type = "urn:mpeg:mpegu:schema:widgets:core:out:2010";
}

function coreOutRequestMigrationTargets(wid, args) {
    WidgetManager.coreOutRequestMigrationTargets(wid, args);
}

function coreOutSetSize(wid, args) {
    WidgetManager.coreOutSetSize(wid, args);
}

function coreOutShow(wid, args) {
    WidgetManager.coreOutShow(wid, args);
}

function coreOutHide(wid, args) {
    WidgetManager.coreOutHide(wid, args);
}

function coreOutRequestActivate(wid, args) {
    WidgetManager.coreOutRequestActivate(wid, args);
}

function coreOutRequestDeactivate(wid, args) {
    WidgetManager.coreOutRequestDeactivate(wid, args);
}

function coreOutShowNotification(wid, args) {
    WidgetManager.coreOutShowNotification(wid, args);
}

function coreOutPlaceComponent(wid, args) {
    WidgetManager.coreOutPlaceComponent(wid, args);
}

function coreOutGetAttention(wid, args) {
    WidgetManager.coreOutGetAttention(wid, args);
}

function coreOutInstallWidget(wid, args) {
    WidgetManager.coreOutInstallWidget(wid, args);
}

function coreOutActivateTemporaryWidget(wid, args) {
    WidgetManager.coreOutActivateTemporaryWidget(wid, args);
}

function coreOutMigrateComponent(wid, args) {
    WidgetManager.coreOutMigrateComponent(wid, args);
}

/*
 Methods of binding and callback of core:* interfaces
 */
function wmjs_interface_invoke_callback_core(wid_src, ifce_dst, is_reply) {
    log(l_inf, "wmjs_interface_invoke_callback_core '" + ifce_dst.type + "' - reply " + is_reply);
    return function() {
        var i, ai = 0, param_count, msgHandler, msg_src, msg_dst;
        var args = new Array();
        msgHandler = arguments[0];
        /*get msg from source interface (this object)*/
        msg_src = this.get_message(msgHandler.msgName);
        msg_dst = getMessage(ifce_dst, msgHandler.msgName);
        //var argstring = "";
        //for (i = 0; i < msg_dst.num_params; i++) argstring += " "+getParam(msg_dst, i);
        //log(l_inf, argstring);
        //log(l_inf, (is_reply ? 'invokeReply ' : 'invoke ') + msg_src.name + ' on core.' + msg_dst.name + " nbpar:"+msg_src.num_params);
        param_count = msg_src.num_params;
        for (i = 0; i < param_count; i++) {
            var param = getInputParam(msg_dst, i);
            if (param == null) continue;
            args[ai] = arguments[ai+1];
            ai++;
        }
        log(l_inf, (is_reply ? 'invokeReply ' : 'invoke ') + msg_src.name + ' on core.' + msg_dst.name + " nb:"+ai);
        // call the method that implements the core:* message
        msg_dst.execute(wid_src, args);
    };
}

// function invokeReply for a reply from within a coreOut message
function wmjs_core_out_invoke_reply() {
    var i, ai, param_count, is_script, msg_src, msg_dst, wid_dst;
    var args = new Array();
    is_script = 0;
    msg_src = arguments[0];
    msg_dst = arguments[1];
    wid_dst = arguments[2];
    log(l_inf, 'coreOut/invokeReply ' + msg_src.name + ' on core.' + msg_dst.name + ' to ' +wid_dst.name+ " nb:"+ai);
    if (msg_dst.has_script_input) is_script = 1;
    param_count = msg_src.num_params;
    ai = 3;
    for (i = 0; i < param_count; i++) {
        var param = msg_dst.get_param(i);
        if (! param.is_input) continue;
        if (is_script) {
            args[ai - 3] = arguments[ai];
        } else {
            wid_dst.set_input(param, arguments[ai]);
        }
        ai++;
    }
    if (msg_dst.has_input_action) {
        wid_dst.call_input_action(msg_dst);
    } else if (is_script) {
        wid_dst.call_input_script(msg_dst, args);
    }
}

// get an interface by type in WidgetManager, since getInterfaceHandlersByType is not accessible
function getInterfaceByType(widget, type) {
    for (var i = 0; i < widget.num_interfaces; i++) {
      var ifce = widget.get_interface(i);
      if (ifce.type == type) return ifce;
    }
    return null;
}

function wmjs_bind_interface_to_core_service(wid, ifce) {
    if (ifce.type == "urn:mpeg:mpegu:schema:widgets:core:in:2010") {
        // core:in
        return wmjs_bind_interface_to_core_service1(wid, ifce, coreIn);
    } else if (ifce.type == "urn:mpeg:mpegu:schema:widgets:core:out:2010") {
        // core:out
        return wmjs_bind_interface_to_core_service1(wid, ifce, coreOut);
    }
    // not an interface to core services
    return false;
}

function wmjs_bind_interface_to_core_service1(wid, ifce, core) {
    // loop on the messages to check if they match
    log(l_inf, 'Bind interface to core service ' + wid.name + " " + ifce.type);
    var set_bind = 0, nb_ok, j, k, a_msg;
    for (j = 0; j < ifce.num_messages; j++) {
        var msg = ifce.get_message(j);
        // does the other interface have this message
        a_msg = getMessage(core, msg.name);
        if (!a_msg) {
            // no, it does not have this message, so leave
            log(l_inf, 'No core message for ' + msg.name);
            continue;
        }
        // the messages have matching names, check direction
        if (msg.is_input == a_msg.is_input) {
            log(l_inf, 'core message for ' + msg.name + ' is not in direction ' + (msg.is_input ? 'output' : 'input'));
            continue;
        }
        // the messages have matching names and directions, check params
        if (msg.num_params != a_msg.num_params) {
            log(l_war, 'core message ' + msg.name + ' does not have the same number of parameters '
                    + msg.num_params + ' ' + a_msg.num_params);
            var paramstring = "";
            for (k = 0; k < msg.num_params; k++) {
                par = msg.get_param(k);
                paramstring += " " + par.name;
            }
            log(l_deb, paramstring);
            continue;
        }
        /*check all params*/
        nb_ok = 0;
        for (k = 0; k < msg.num_params; k++) {
            par = msg.get_param(k);
            //log(l_inf, " "+par.name+" "+hasParam(a_msg, par.name)+" "+par.is_input+" "+paramDirection(a_msg, par.name));
            if (hasParam(a_msg, par.name) != null && par.is_input != paramDirection(a_msg, par.name)) nb_ok ++;
        }
        if (nb_ok != msg.num_params) {
            log(l_war, 'core message ' + msg.name + ' does not have the same input/output parameters '+nb_ok+" "+msg.num_params);
            continue;
        }
        set_bind ++;
        // the messages match
        log(l_inf, 'Binding ' + wid.name + '.' + msg.name + ' to core.' + a_msg.name);
        /*OK let's bind this action: we only need to assign the output trigger, the input
         action will be called from the other widget*/
        if (msg.has_output_trigger) {
            wmjs_bind_output_trigger(wid, msg, wmjs_output_trigger_callback_core(msg, wid, a_msg), WidgetManager);
        }
    }
    if (!set_bind) return false;
    /*create callback for programmatic action triggers*/
    ifce.invoke = wmjs_interface_invoke_callback_core(wid, core, 0);
    // ifce.invokeReply = wmjs_interface_invoke_callback_core(wid, core, 1);
    wid.bind_interface(ifce, null, 'localhost');
    return true;
}

function wmjs_output_trigger_callback_core(msg_out, wid_src, msg_in) {
    log(l_deb, "wmjs_output_trigger_callback_core");
    return function() {
        log(l_deb, "wmjs_output_trigger_callback_core/function '" + wid_src.name + "'");
        var param_count = msg_out.num_params;
        var args = new Array();
        var ai = 0, i;
        log(l_inf, 'Invoking Widget(' + wid_src.name + ').' + msg_out.name);
        for (i = 0; i < param_count; i++) {
            var param = msg_out.get_param(i);
            if (param.is_input) continue;
            args[ai] = wid_src.get_param_value(param);
            ai++;
        }
        msg_in.execute(wid_src, args);
    };
}


//
// send a core:in message with no parameter
//
function wmjs_corein_message() {
  if (arguments.length < 2) return;
  var widget = arguments[0];
  var message = arguments[1];
  
  for (var i=0; i<widget.num_interfaces; i++) {
    var ifce = widget.get_interface(i);
    if (ifce.type != 'urn:mpeg:mpegu:schema:widgets:core:in:2010') continue;
    
    for (var j=0; j<ifce.num_messages; j++) {
      var msg = ifce.get_message(j);
      if (msg.name == message) {
        if (msg.has_script_input) {
          var jsargs = new Array();
          /*for each input descibed in the manifest, get the input script value and stack it in the argument list*/
          for (var k=0; k<msg.num_params; k++) {
            par = msg.get_param(k);
            if (!par.is_input) continue;
            for (var l=2; l<arguments.length; l+=2) {
             if (arguments[l]==par.name) {
               jsargs.push(arguments[l+1]);
               break;
             }
            }
          }
          widget.call_input_script(msg, jsargs);
        } else if (msg.has_input_action) {
          /*for each input descibed in the manifest, set the widget input value*/
          for (var k=0; k<msg.num_params; k++) {
            par = msg.get_param(k);
            if (!par.is_input) continue;
            for (var l=2; l<arguments.length; l+=2) {
             if (arguments[l]==par.name) {
               widget.set_input(par, arguments[l+1]);
               break;
             }
            }
          }
          widget.call_input_action(msg);
        }
      }
    }
  }
}

/*
 End of implementation of core:in and core:out interfaces
 */
