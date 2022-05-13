// 01122011 AMD1 startWidget listWidgets getWidget implemented
/* wrapper as a module
 var iphone_wm_gui = (function () {
 */
// to make sure the initialization is done only once
var init = true;

// constant
var xlinkns = 'http://www.w3.org/1999/xlink';
//var evns = 'http://www.w3.org/2001/xml-events';

// state of the widget manager: displays homepage (value=home) or executes widget (value="exec")
var state = 'home';

// convenience variables for SVG elements
var homepage = null, arrows = null, icons = null, widgetContainer = null, homebar = null, execbar = null;

// where is the index into pages of icons on the home page
var where = 0,maxwhere = 0,whereW = 0;

// array of activated widgets
var activatedWidgets = new Array();
var numActivatedWidgets = 0;

// variables for flexible layout
// variables for flexible layout
var totalWidth = 0,totalHeight = 0,iconNbHoriz = 0,iconNbVert = 0,iconsPerPage = 0;

//previous size
var previousWidth = 0,previousHeight = 0;

// to differentiate between install icon and scan dir for icons
var isThisAScan = null;

// preferred icon type
var preferredIconType = '.svg';

// adapt layout to the size of the screen
function adaptLayoutToSize() {
    if (l_deb < log_level) {
        alert("[UI] adaptLayoutToSize");
    }
    display_width = parseInt(scene.get_option('GUI', 'LastWidth'));
    display_height = parseInt(scene.get_option('GUI', 'LastHeight'));
    alert("display "+display_width+" "+display_height);
    if (!scene.fullscreen && display_width && display_height) {
     scene.set_size(display_width, display_height);
    }
    var tmpObject, tmpObj2;
    // get size to adapt to
    totalWidth = document.documentElement.viewport.width;
    totalHeight = document.documentElement.viewport.height;
    if (totalWidth == 0) {
        totalWidth = 160;
    }
    if (totalHeight == 0) {
        totalHeight = 280;
    }
    while (totalWidth < 160 || totalHeight < 280) {
        totalWidth *= 4;
        totalHeight *= 4;
    }
    // min size is 160 by 280
    if (totalWidth < 160) {
        totalWidth = 160;
    }
    if (totalHeight < 280) {
        totalHeight = 280;
    }
    // round to lower multiple of 80
    totalWidth -= totalWidth % 80;
    var iconHeight = totalHeight - 120;
    iconHeight -= iconHeight % 80;
    // how many lines and columns of icons
    iconNbHoriz = totalWidth / 80;
    iconNbVert = iconHeight / 80;
    totalHeight = iconHeight + 120;
    // 120 is upper bar (60) + lower bar (60)
    iconsPerPage = iconNbHoriz * iconNbVert;
    // fix svg viewbox
    document.getElementById("svg").setAttribute("viewBox", "0 0 " + totalWidth + " " + totalHeight);
    // fix odd line
    tmpObj2 = document.getElementById("odd");
    var i, already = tmpObj2.getElementsByTagName("use").length;
    //alert("odd line "+already+" "+iconNbHoriz);
    for (i = already; i < iconNbHoriz; i++) {
        tmpObject = document.createElement("use");
        tmpObject.setAttribute("x", i * 80);
        tmpObject.setAttributeNS(xlinkns, "href", (((i % 2) == 0) ? "#lightRect" : "#darkRect" ));
        tmpObj2.appendChild(tmpObject);
    }
    // fix even line
    tmpObj2 = document.getElementById("even");
    already = tmpObj2.getElementsByTagName("use").length;
    //alert("even line "+already+" "+iconNbHoriz);
    for (i = already; i < iconNbHoriz; i++) {
        tmpObject = document.createElement("use");
        tmpObject.setAttribute("x", i * 80);
        tmpObject.setAttributeNS(xlinkns, "href", (((i % 2) == 0) ? "#darkRect" : "#lightRect" ));
        tmpObj2.appendChild(tmpObject);
    }
    // fix frame (black with rounded corners)
    tmpObject = document.getElementById("frame");
    tmpObject.setAttribute("width", totalWidth);
    tmpObject.setAttribute("height", totalHeight);
    tmpObject = document.getElementById("frame2");
    tmpObject.setAttribute("width", totalWidth);
    tmpObject.setAttribute("height", totalHeight);
    // fix screen (white)
    tmpObject = document.getElementById("screen");
    tmpObject.setAttribute("width", totalWidth);
    tmpObject.setAttribute("height", totalHeight - 120);
    // fix grid back (gray)
    tmpObject = document.getElementById("gridback");
    tmpObject.setAttribute("width", totalWidth);
    tmpObject.setAttribute("height", totalHeight - 120);
    // fix icon background grid
    tmpObject = document.getElementById("grid");
    already = tmpObject.getElementsByTagName("use").length;
    for (i = already; i < iconNbVert; i++) {
        tmpObj2 = document.createElement("use");
        tmpObj2.setAttribute("y", i * 80);
        tmpObj2.setAttributeNS(xlinkns, "href", (((i % 2) == 0) ? "#odd" : "#even" ));
        tmpObject.appendChild(tmpObj2);
    }
    // if there are already too many lines, remove the extra ones otherwise they are rendered on top of the lower
    // part of the decoration
    if (already > iconNbVert) {
        while (already-- > iconNbVert) {
            tmpObject.removeChild(tmpObject.lastChild);
        }
    }
    // fix commands (lower bar)
    document.getElementById("commands").setAttribute("transform", "translate(0, " + (totalHeight - 60) + ")");
    document.getElementById("homeButton").setAttribute("transform", "translate(" + (totalWidth / 2) + ", 30)");
    document.getElementById("right").setAttribute("transform", "translate(" + (totalWidth - 50) + ", 10)");
    document.getElementById("rightW").setAttribute("transform", "translate(" + (totalWidth - 50) + ", 10)");
    // fix the cuts (white rects left and right because we have no clipping)
    tmpObject = document.getElementById("leftCut");
    tmpObject.setAttribute("width", totalWidth);
    tmpObject.setAttribute("height", totalHeight);
    tmpObject.setAttribute("x", -1 - totalWidth);
    tmpObject = document.getElementById("rightCut");
    tmpObject.setAttribute("width", totalWidth);
    tmpObject.setAttribute("height", totalHeight);
    tmpObject.setAttribute("x", 1 + totalWidth);
    // respace executing widgets if any
    tmpObject = widgetContainer.getElementsByTagName("g");
    for (i = 0; i < tmpObject.length; i++) {
        var gg = tmpObject.item(i);
        gg.setAttribute("transform", "translate(" + (totalWidth * (i - 1)) + ", 0)");
        if (gg.firstElementChild != null) {
            gg.firstElementChild.setAttribute("width", totalWidth);
            gg.firstElementChild.setAttribute("height", totalHeight - 120);
        }
    }
}

//
// widget close on click at "Kill" button
//
function on_kill_widget() {
    if (state == 'exec') {
        widget_close(activatedWidgets[whereW]);
    }
}

//
// widget get on click at "GetW" button
//
function on_get_widget() {
    alert("on_get_widget " + WidgetManager.MPEGUStandardServiceProviders.length);
    if (WidgetManager.MPEGUStandardServiceProviders.length != 0) {
        upnp_renders = selector_window1();
        upnp_renders.on_select = function(item) {
            upnp_renders.unregister(root);
            upnp_renders = null;
            if (item == -1) {
                return;
            }
            alert("upnp_renders.on_select(" + item + ")");
            var device = WidgetManager.MPEGUStandardServiceProviders[item];
            device.standardService.SetActionListener("listWidgets", get_widget_callback(device), true);
            device.standardService.CallAction("listWidgets", new Array());
        }
        upnp_renders.register(root);
    }
}

function get_widget_callback(device) {
    return function() {
        //alert("get_widget_callback");
        // msgHandler is the first argument, the next arguments are from the reply to listWidgets
        var act = arguments[0];
        var act1 = act.GetArgumentValue("widgetCodes");
        var act2 = act.GetArgumentValue("widgetNames");
        //alert(act1+" "+act2);
        target_widgets = selector_window2(act1.split(" "), act2.split(" "));
        target_widgets.on_select = function(item) {
            alert("target_widgets.on_select");
            target_widgets.unregister(root);
            target_widgets = null;
            if (item == -1) {
                return;
            }
            alert("target_widgets.on_select(" + item + ")");
            var args = new Array();
            args[0] = "widgetCode";
            args[1] = item;
            device.standardService.SetActionListener("getWidget", get_widget_callback2, true);
            device.standardService.CallAction("getWidget", args);
        }
        target_widgets.register(root);
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

//
// creates the menu of available targets for pushing a widget elsewhere
//
function selector_window1() {
    var i, count, render;
    var selector = document.createElement('g'), obj, child;
    selector.setAttribute('transform', 'translate(10,10)');
    count = WidgetManager.MPEGUStandardServiceProviders.length;
    selector.appendChild(rect(0, 0, 300, 20 * (count + 1), 'white', 'black', null));
    for (i = 0; i < count; i++) {
        render = WidgetManager.MPEGUStandardServiceProviders[i];
        obj = createtext(render.Name, 'black', 5, 17 + (20 * i), 15, 'sans-serif');
        obj.setAttribute('id', "select" + i);
        selector.appendChild(obj);
        obj.addEventListener('mouseover', sw1("select" + i), false);
        obj.addEventListener('mouseout', sw2("select" + i), false);
        obj.addEventListener('click', sw4(i), false);
    }
    obj = createtext('Cancel', 'rgb(0,0,120)', 55, 17 + (20 * i), 15, 'sans-serif');
    obj.setAttribute('id', "canc");
    selector.appendChild(obj);
    obj.addEventListener('mouseover', function() { document.getElementById("canc").setAttribute("fill", "red"); }, false);
    obj.addEventListener('mouseout', function() { document.getElementById("canc").setAttribute("fill", "black"); }, false);
    obj.addEventListener('click', function() { upnp_renders.on_select(-1); }, false);
    selector.register = function(disp) {
        disp.appendChild(this);
    };
    selector.unregister = function(disp) {
        disp.removeChild(this);
    };
    return selector;
}

//
// creates the menu of available targets for pushing a widget elsewhere
//
function selector_window2(codes, names) {
    alert("selector_window2");
    var i, count, render;
    var selector = document.createElement('g'), obj, child;
    selector.setAttribute('transform', 'translate(10,10)');
    count = codes.length;
    selector.appendChild(rect(0, 0, 300, 20 * (count + 1), 'white', 'black', null));
    for (i = 0; i < count; i++) {
        render = names[i];
        obj = createtext(render, 'black', 5, 17 + (20 * i), 15, 'sans-serif');
        obj.setAttribute('id', "selecto" + i);
        selector.appendChild(obj);
        obj.addEventListener('mouseover', sw1("selecto" + i), false);
        obj.addEventListener('mouseout', sw2("selecto" + i), false);
        obj.addEventListener('click', sw5(i), false);
    }
    obj = createtext('Cancel', 'rgb(0,0,120)', 55, 17 + (20 * i), 15, 'sans-serif');
    obj.setAttribute('id', "cance");
    selector.appendChild(obj);
    obj.addEventListener('mouseover', function() { document.getElementById("cance").setAttribute("fill", "red"); }, false);
    obj.addEventListener('mouseout', function() { document.getElementById("cance").setAttribute("fill", "black"); }, false);
    obj.addEventListener('click', function() { target_widgets.on_select(-1); }, false);
    selector.register = function(disp) {
        disp.appendChild(this);
    };
    selector.unregister = function(disp) {
        disp.removeChild(this);
    };
    return selector;
}

function sw4(si) {
    return function() { upnp_renders.on_select(si); };
}

function sw5(si) {
    return function() { target_widgets.on_select(si); };
}

//
// when deleting an executing widgets, all executing widgets to its right are moved to the left
//
function recurseMoveAfterDelete(target) {
    if (target != null && target.nextElementSibling != null) {
        var v = target.nextElementSibling;
        recurseMoveAfterDelete(v);
        v.setAttribute("transform", target.getAttribute("transform"));
    }
}

//
// click on home button to switch from icons to executing widgets
//
function home_button(evt) {
    if (l_deb < log_level) {
        alert("[UI] home_button");
    }
    if (state != 'home') {
        state = 'home';
        widgetContainer.setAttribute('display', 'none');
        homepage.setAttribute('display', 'inline');
        homebar.setAttribute('display', 'inline');
        execbar.setAttribute('display', 'none');
        arrows.setAttribute('display', 'inline');
        arrowsW.setAttribute('display', 'none');
        widgetAddList.setAttribute('display', 'none');
    } else {
        state = 'exec';
        widgetContainer.setAttribute('display', 'inline');
        homepage.setAttribute('display', 'none');
        homebar.setAttribute('display', 'none');
        execbar.setAttribute('display', 'inline');
        arrows.setAttribute('display', 'none');
        arrowsW.setAttribute('display', 'inline');
        widgetAddList.setAttribute('display', 'none');
    }
}

// constants
var adjustFrom = "0,0";
var animDue = true;

//
// after each change of icon page, this function adjusts the visibility of arrows in the lower bar
//
function adjustwhere(animDue) {
    if (l_deb < log_level) {
        alert("[UI] adjust " + where + " 0 " + maxwhere);
    }
    document.getElementById("left").setAttribute("display", ((where > 0) ? "inline" : "none"));
    document.getElementById("right").setAttribute("display", ((where < maxwhere) ? "inline" : "none"));
    if (!animDue) {
        icons.setAttribute("transform", 'translate(' + (-80 * iconNbHoriz * where) + ',0)');
    } else {
        var aw = document.createElement("animateTransform");
        aw.setAttribute("attributeName", "transform");
        aw.setAttribute("type", "translate");
        //alert('time '+document.documentElement.getCurrentTime());
        aw.setAttribute("begin", document.documentElement.getCurrentTime());
        aw.setAttribute("dur", "1s");
        aw.setAttribute("fill", "freeze");
        aw.setAttributeNS(xlinkns, "href", "#icons");
        aw.setAttribute("from", adjustFrom);
        aw.setAttribute("to", (-80 * iconNbHoriz * where) + ",0");
        document.documentElement.appendChild(aw);
    }
    adjustFrom = (-80 * iconNbHoriz * where) + ",0";
}

//
// action of the left button on the lower bar
//
function left_button() {
    if (l_deb < log_level) {
        alert("[UI] left button " + where + " 0");
    }
    if (where > 0) {
        where--;
        adjustwhere(true);
    }
}

//
// action of the right button of the lower bar
//
function right_button() {
    if (l_deb < log_level) {
        alert("[UI] right button " + where + " " + maxwhere);
    }
    if (where < maxwhere) {
        where++;
        adjustwhere(true);
    }
}

var adjustFromW = "0,0";
var oldwhereW = -1;

//
// after each change of icon page, this function adjusts the visibility of arrows in the lower bar
//
function adjustWhereWidgets(animDue) {
    if (oldwhereW != whereW) {
        // hide oldwhereW
        if (oldwhereW >= 0 && activatedWidgets[oldwhereW] != null) {
            WidgetManager.corein_message(activatedWidgets[oldwhereW], "hide");
        }
    }
    if (l_deb < log_level) {
        alert("[UI] adjustW " + whereW + " 0 " + (numActivatedWidgets - 1));
    }
    document.getElementById("leftW").setAttribute("display", ((whereW > 0) ? "inline" : "none"));
    document.getElementById("rightW").setAttribute("display", ((whereW < (numActivatedWidgets - 1)) ? "inline" : "none"));
    if (!animDue) {
        widgetContainer.setAttribute("transform", "translate(" + (-totalWidth * whereW) + ",0)");
    } else {
        var aw = document.createElement("animateTransform");
        aw.setAttribute("attributeName", "transform");
        aw.setAttribute("type", "translate");
        aw.setAttribute("begin", document.documentElement.getCurrentTime());
        aw.setAttribute("dur", "1s");
        aw.setAttribute("fill", "freeze");
        aw.setAttributeNS(xlinkns, "href", "#widget");
        aw.setAttribute("from", adjustFromW);
        aw.setAttribute("to", (-totalWidth * whereW) + ",0");
        document.documentElement.appendChild(aw);
    }
    adjustFromW = (-totalWidth * whereW) + ",0";
    document.getElementById("widgetName").textContent =
            (whereW >= 0 && activatedWidgets[whereW] != null ? activatedWidgets[whereW].shortName : '-');
    if (oldwhereW != whereW) {
        // show whereW
        if (whereW >= 0 && activatedWidgets[whereW] != null) {
            WidgetManager.corein_message(activatedWidgets[whereW], "show");
        }
        oldwhereW = whereW;
    }
}

//
// action of the left button on the lower bar
//
function left_buttonW() {
    if (l_deb < log_level) {
        alert("[UI] left button " + whereW);
    }
    if (whereW > 0) {
        whereW--;
        adjustWhereWidgets(animDue);
    }
}

//
// action of the right button of the lower bar
//
function right_buttonW() {
    if (l_deb < log_level) {
        alert("[UI] right button " + whereW + " " + (numActivatedWidgets - 1));
    }
    if (whereW < (numActivatedWidgets - 1)) {
        whereW++;
        adjustWhereWidgets(animDue);
    }
}

//
// action of each icon, starting the matching widget
//
function activating_widget(w) {
    if (l_deb < log_level) {
        alert("[UI] activating widget: " + w.name);
    }
    widget_add(w);
}

//
// main initialization function
// init variables, then init the widget manager C code, then inits UPnP
//
function initialize() {
    if (l_deb < log_level) {
        alert("[UI] initialize");
    }
    init = false;
    var display_width = parseInt(scene.get_option('Widgets', 'LastWMWidth'));
    var display_height = parseInt(scene.get_option('Widgets', 'LastWMHeight'));
    if (display_width && display_height) {
        scene.set_size(display_width, display_height);
    }
    root = document.documentElement;
    homepage = document.getElementById('homepage');
    homebar = document.getElementById('homebar');
    execbar = document.getElementById('execbar');
    arrows = document.getElementById('arrows');
    arrowsW = document.getElementById('arrowsW');
    icons = document.getElementById('icons');
    widgetContainer = document.getElementById('widget');
    widgetAddList = document.getElementById('widgetAddList');
    /* Setup the GPAC Widget Manager - this will also scan the available widgets */
    log_level = l_inf;
    widget_manager_init();
    WidgetManager.on_widget_remove = widget_remove;
    WidgetManager.on_widget_add = widget_add;
    /* register the callback to be notified of incoming widgets */
    has_upnp = (typeof UPnP != 'undefined');
    if (has_upnp) {
        /* setting the callback to allow other devices to push their widgets */
        UPnP.onMediaConnect = onMediaConnect;
        /* Tell GPAC that the calls to the main Renderer (like open, ...) must be forwared to this scene */
        UPnP.BindRenderer();
    }
    WidgetManager.coreOutShow = coreOutShowImplementation;
    WidgetManager.coreOutGetAttention = coreOutGetAttentionImplementation;
    WidgetManager.coreOutHide = coreOutHideImplementation;
    WidgetManager.coreOutRequestDeactivate = coreOutRequestDeactivateImplementation;
    WidgetManager.coreOutInstallWidget = coreOutInstallWidgetImplementation;
    WidgetManager.coreOutActivateTemporaryWidget = coreOutActivateTemporaryWidgetImplementation;
    WidgetManager.coreOutMigrateComponent = coreOutMigrateComponentImplementation;
    WidgetManager.coreOutRequestMigrationTargets = coreOutRequestMigrationTargetsImplementation;
}

//
// implementation of core:out install widget
//
function coreOutInstallWidgetImplementation(wid, args) {
    var w = widgetInstall(args[0], true, false, wid);
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.installWidgetMessage, ifce.get_message("installWidget"),
                wid, (w != null ? 1 : 0)); // send return code 1 = success
    }
}

//
// implementation of core:out activate temporary widget
//
function coreOutActivateTemporaryWidgetImplementation(wid, args) {
    var w = widgetInstall(args[0], true, true, null);
    if (w != null) {
        activating_widget(w);
    }
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.activateTemporaryWidgetMessage, ifce.get_message("activateTemporaryWidget"),
                wid, (w != null ? 1 : 0)); // send return code 1 = success
    }
}

//
// implementation of core:out migrate component
//
function coreOutMigrateComponentImplementation(wid, args) {
    //alert("coreOutMigrateComponent "+wid.name+" "+args.length);
    var comp = wid.get_component(args[0], true);
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (comp == null) {
        log(l_err, 'Component ' + args[0] + ' cannot be found in widget ' + wid.name);
        if (ifce != null) {
            wmjs_core_out_invoke_reply(coreOut.migrateComponentMessage, ifce.get_message("migrateComponent"), wid, 0);
        }
        return;
    }
    if (args.length > 1 && args[1] != null) {
        //WidgetManager.migrate_widget(UPnP.GetMediaRenderer(parseInt(args[1])), comp);
        WidgetManager.migrate_widget(WidgetManager.get_mpegu_service_providers(parseInt(args[1])), comp);
        widget_close(comp);
    } else {
        upnp_renders = selector_window(comp);
        upnp_renders.on_select = function(item, wid) {
            upnp_renders.unregister(root);
            upnp_renders = null;
            if (item == -1) {
                return;
            }
            if (comp != null) {
                alert("upnp_renders.on_select(" + item + "," + comp.name + ")");
                //WidgetManager.migrate_widget(UPnP.GetMediaRenderer(item), comp);
                WidgetManager.migrate_widget(WidgetManager.get_mpegu_service_providers(item), comp);
                widget_close(comp);
            }
        };
        upnp_renders.register(root);
    }
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.migrateComponentMessage, ifce.get_message("migrateComponent"), wid, 1); // send return code 1 = success
    }
}

//
// implementation of core:out request migration targets
//
function coreOutRequestMigrationTargetsImplementation(wid, args) {
    var count = UPnP.MediaRenderersCount, codes = new Array(), names = new Array(), descriptions = new Array(), i;
    for (i = 0; i < count; i++) {
        var render = UPnP.GetMediaRenderer(i);
        codes.push("" + i);
        names.push(render.Name);
        descriptions.push(render.HostName + " " + render.UUID);
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
// implementation of core:out Show message
// this is a request by the widget to be shown
//
function coreOutShowImplementation(wid, args) {
    //alert("core:out show "+wid.name);
    var target = widgetContainer.firstElementChild;
    var i;
    for (i = 0; i < numActivatedWidgets; i++) {
        //alert("is it "+activatedWidgets[i].name);
        if (activatedWidgets[i] == wid) {
            break;
        }
        target = target.nextElementSibling;
    }
    // here, i is the index of the current widget
    //alert(" "+i+" "+numActivatedWidgets);
    if (i < numActivatedWidgets) {
        whereW = i;
        adjustWhereWidgets(false);
    }
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.showMessage, ifce.get_message("show"), wid, 1); // send return code 1 = success
    }
}

//
// implementation of core:out GetAttention message
// this is a request by the widget to be shown and some special signal is given
//
function coreOutGetAttentionImplementation(wid, args) {
    //alert("core:out getAttention "+wid.name);
    var target = widgetContainer.firstElementChild;
    var i;
    for (i = 0; i < numActivatedWidgets; i++) {
        //alert("is it "+activatedWidgets[i].name);
        if (activatedWidgets[i] == wid) {
            break;
        }
        target = target.nextElementSibling;
    }
    // here, i is the index of the current widget
    //alert(" "+i+" "+numActivatedWidgets);
    if (i < numActivatedWidgets) {
        whereW = i;
        adjustWhereWidgets(false);
    }
    document.getElementById("getAttention").beginElement();
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.getAttentionMessage, ifce.get_message("getAttention"), wid, 1); // send return code 1 = success
    }
}

//
// implementation of core:out hide message
// this is a request by the widget to be hidden (some other widget (if any) is shown)
//
function coreOutHideImplementation(wid, args) {
    //alert("core:out hide "+wid.name);
    var target = widgetContainer.firstElementChild;
    var i;
    for (i = 0; i < numActivatedWidgets; i++) {
        //alert("is it "+activatedWidgets[i].name);
        if (activatedWidgets[i] == wid) {
            break;
        }
        target = target.nextElementSibling;
    }
    // here, i is the index of the current widget
    //alert("hide "+i+" "+numActivatedWidgets);
    if (i < numActivatedWidgets) {
        if (whereW > 0) {
            whereW--;
        }
        else if (whereW < numActivatedWidgets - 1) {
            whereW++;
        }
        else {
            return;
        }
        adjustWhereWidgets(false);
    }
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.hideMessage, ifce.get_message("hide"), wid, 1); // send return code 1 = success
    }
}

//
// implementation of core:out requestDeactivate message
// this is a request by the widget to be stopped
//
function coreOutRequestDeactivateImplementation(wid, args) {
    //alert("core:out hide "+wid.name);
    var target = widgetContainer.firstElementChild;
    var i;
    for (i = 0; i < numActivatedWidgets; i++) {
        //alert("is it "+activatedWidgets[i].name);
        if (activatedWidgets[i] == wid) {
            break;
        }
        target = target.nextElementSibling;
    }
    // here, i is the index of the current widget
    //alert("hide "+i+" "+numActivatedWidgets);
    if (i < numActivatedWidgets) {
        widget_close(activatedWidgets[i]);
    }
    var ifce = getInterfaceByType(wid, "urn:mpeg:mpegu:schema:widgets:core:out:2010");
    if (ifce != null) {
        wmjs_core_out_invoke_reply(coreOut.requestDeactivateMessage, ifce.get_message("requestDeactivate"), wid, 1); // send return code 1 = success
    }
}

//
// WM callback for when a component is activated by its parent
//
function widget_add(w) {
    log(l_inf, "widget add " + w.name);
    if (!w.activated) {
        if (sameFileIgnoringSVGView(w.icon, w.main)) {
            // same file in icon and main
        } else {
            widget_launch(w, document.createElement("animation"));
        }
    } else if (w.multipleInstances) {
        var newwid = WidgetManager.open(w.manifest, null);
        widget_launch(newwid, document.createElement("animation"));
    } else {
        return false;
    }
    state = 'exec';
    widgetContainer.setAttribute('display', 'inline');
    homepage.setAttribute('display', 'none');
    homebar.setAttribute('display', 'none');
    execbar.setAttribute('display', 'inline');
    arrows.setAttribute('display', 'none');
    arrowsW.setAttribute('display', 'inline');
    widgetAddList.setAttribute('display', 'none');
    return true;
}

function sameFileIgnoringSVGView(name1, name2) {
    if (name1 == name2) {
        return true;
    }
    if (name1 == null) {
        return false;
    }
    var i1 = name1.indexOf("#");
    var i2 = name2.indexOf("#");
    if (i1 < 0) {
        i1 = name1.length;
    }
    else {
        i1--;
    }
    if (i2 < 0) {
        i2 = name2.length;
    }
    else {
        i2--;
    }
    return name1.substring(0, i1) == name2.substring(0, i2);
}

//
// recompute the number of widgets loaded currently
//
function getNbWidgets() {
    var i, nbWidgets = 0;
    for (i = 0; i < WidgetManager.num_widgets; i++) {
        var w = WidgetManager.get(i);
        if (w != null && w.loaded) {
            nbWidgets++;
        }
    }
    return nbWidgets;
}

//
// just resize the window and viewport, and initialize the first time this is called
//
function resize() {
    if (init) {
        initialize();
    }
    if (document.documentElement.viewport.width == previousWidth && document.documentElement.viewport.height == previousHeight) {
        return;
    }
    if (l_deb < log_level) {
        alert("[UI] start initialize() w:" + document.documentElement.viewport.width + " h:" + document.documentElement.viewport.height);
    }
    adaptLayoutToSize();
    // start by filling the "home page" with known icons
    where = 0;
    var nbWidgets = getNbWidgets();
    maxwhere = ((nbWidgets - 1) - ((nbWidgets - 1) % iconsPerPage)) / iconsPerPage;
    var wid;
    var iconIterator = document.getElementById("icons").firstElementChild;
    var position = 0;
    for (i = 1; i <= WidgetManager.num_widgets; i++) {
        wid = WidgetManager.get(i - 1);
        // alert("build:"+wid.main+" "+wid.loaded);
        if (wid.loaded) {
            insert_icon(wid, position, i - 1, iconIterator);
            WidgetManager.corein_message(wid, 'setSize', 'width', totalWidth, 'height', totalHeight - 120, 'dpi', 96);
            position++;
            if (iconIterator != null) {
                iconIterator = iconIterator.nextElementSibling;
            }
        }
    }
    adjustwhere(false);
    adjustWhereWidgets(false);
    previousWidth = document.documentElement.viewport.width;
    previousHeight = document.documentElement.viewport.height;
    scene.set_option("Widgets", "LastWMWidth", '' + previousWidth);
    scene.set_option("Widgets", "LastWMHeight", '' + previousHeight);
}

//
// function inserting an icon on the home page
//
function insert_icon(widget, position, widgetIndex, previousIcon) {
    if (l_deb < log_level) {
        alert("[UI] insert_icon: " + widget.shortName + " " + position + " " + widgetIndex + " WMnw:" + WidgetManager.num_widgets);
    }
    widget.loaded = true;
    if (l_deb < log_level) {
        alert("[UI] widget name: " + widget.name);
    }
    var icon = null, original = null;
    for (var i = 0; i < widget.icons.length; i++) {
        // default to the first icon even if not of the preferred type
        if (i == 0) {
            icon = widget.icons[0].relocated_src;
            //original = widget.icons[0].original;
            // alert("choosing default icon " + icon);
        }
        // check for preferred type
        if (widget.icons[i].relocated_src.indexOf(preferredIconType) > 0) {
            icon = widget.icons[i].relocated_src;
            //original = widget.icons[i].original;
            break;
        }
    }
    var shortName = widget.shortName;
    if (typeof shortName == 'undefined') {
        shortName = widget.name.substring(0, 9);
    }
    createIconSVGdecoration(previousIcon, widget, (((position % iconNbHoriz) * 80) + ((80 * (position - (position % iconsPerPage))) / iconNbVert)),
            ((((position % iconsPerPage) - (position % iconNbHoriz)) / iconNbHoriz) * 80), "icons", icon,
            shortName, widgetIndex);
}

// constant
// const corein = "urn:mpeg:mpegu:schema:widgets:core:in:2010";

//
// commodity method to empty a list of children
//
function removeAllChildren(o) {
    if (o != null && o.hasChildNodes()) {
        while (o.childNodes.length >= 1) {
            o.removeChild(o.firstChild);
        }
    }
}

//
// create the home page icon with all its behaviors
//
function createIconSVGdecoration(previousIcon, widget, x, y, fatherId, iconUrl, name, widIndex) {
    var g, g2;
    if (l_inf < log_level) {
        alert("[UI] createIconSVGdecoration " + iconUrl + " " + x + " " + y);
    }
    if (previousIcon != null) {
        g = previousIcon;
    } else {
        g = document.createElement("g");
        document.getElementById(fatherId).appendChild(g);
    }
    g.setAttribute("transform", 'translate(' + x + ',' + y + ')');
    if (previousIcon != null) {
        g2 = g.firstElementChild;
    } else {
        g2 = document.createElement("g");
        g2.setAttribute("transform", 'translate(15,10)');
        g.appendChild(g2);
    }
    if (iconUrl == null || iconUrl == "") {
        iconUrl = "icons/face-surprise.svg";
    }
    //
    // process differently cases where widget.icon == widget.main
    //
    var container;
    if (sameFileIgnoringSVGView(iconUrl, widget.main) && widget.main.indexOf('.svg') >= 0) {
        // see if the animation already exists
        container = document.getElementById(name);
        if (container == null) {
            // if the animation does not exist yet, create it
            container = media('animation', iconUrl, 50, 50);
            // store the animation in the main defs
            document.getElementById("mainDefs").appendChild(container);
            container.setAttribute('id', name);
        }
        if (previousIcon == null) {
            // put the container in a use
            var use = document.createElement("use");
            use.setAttribute('id', 'iconContainer' + name);
            use.setAttributeNS(xlinkns, 'href', '#' + name);
            g2.appendChild(use);
        }
    } else {
        if (previousIcon == null) {
            container = appropriateElementForMedia(iconUrl, 50, 50);
            container.setAttribute('id', name);
            g2.appendChild(container);
        }
    }
    if (previousIcon == null) {
        g2 = document.createElement("g");
        g2.setAttribute("transform", 'translate(40,70)');
        g.appendChild(g2);
        var anim = createtext(name, 'white', 0, 0, 14, 'Arial Unicode MS');
        anim.setAttribute("text-anchor", "middle");
        anim.setAttribute("display-align", "center");
        g2.appendChild(anim);
        var rect = invisible_rect(80, 80);
        g.appendChild(rect);
        rect.addEventListener("click", csi(widget), false);
    }
}

function csi(widget) {
    return function(evt) { activating_widget(widget);};
}

//
// widget closing action (WM callback)
//
function widget_close(wid) {
    if (wid == null) {
        return;
    }
    if (l_inf <= log_level) {
        alert('[UI] widget_close:' + wid.name);
    }
    // maybe inform the widget that it is going to be deactivated
    WidgetManager.corein_message(wid, "deactivate");
    var target = widgetContainer.firstElementChild;
    var i;
    for (i = 0; i < numActivatedWidgets; i++) {
        if (activatedWidgets[i] == wid) {
            break;
        }
        target = target.nextElementSibling;
    }
    if (target != null) {
        // move next widgets back one slot
        recurseMoveAfterDelete(target);
        // stop the subscene
        if (target.firstElementChild != null) {
            target.firstElementChild.setAttributeNS(xlinkns, "href", "");
        }
        // end trying
        widgetContainer.removeChild(target);
    }
    wid.deactivate();
    wid.activated = false;
    activatedWidgets.splice(i, 1);
    numActivatedWidgets--;
    whereW = (whereW >= i ? (whereW > 0 ? whereW - 1 : 0) : whereW);
    adjustWhereWidgets(false);
    // if no more widgets, go back to the icons
    if (numActivatedWidgets == 0) {
        state = 'home';
        widgetContainer.setAttribute('display', 'none');
        homepage.setAttribute('display', 'inline');
        homebar.setAttribute('display', 'inline');
        execbar.setAttribute('display', 'none');
        arrows.setAttribute('display', 'inline');
        arrowsW.setAttribute('display', 'none');
        widgetAddList.setAttribute('display', 'none');
    }
    if (!wid.permanent) {
        WidgetManager.unload(wid, false);
    }
}

//
// widget unloading action (WM callback)
//
function widget_remove(wid) {
    if (l_deb <= log_level) {
        alert('[UI] widget_remove:' + wid.name);
    }
    widget_close(wid);
    WidgetManager.unload(wid, false);
}

//
// widget launcher action
//
function widget_launch(wid, scene_container) {
    if (l_inf <= log_level) {
        alert('[UI] widget_launch:' + wid.name);
    }
    var tmp = document.createElement("g");
    tmp.setAttribute("transform", "translate(" + (totalWidth * numActivatedWidgets) + ", 0)");
    widgetContainer.appendChild(tmp);
    var icon = null;
    alert("wid: " + wid.name + "|" + wid.shortName);
    if (typeof wid.shortName != 'undefined') {
        var container = document.getElementById(wid.shortName);
        if (container != null) {
            icon = container.getAttributeNS(xlinkns, 'href');
        }
    }
    if (icon != null &&
            sameFileIgnoringSVGView(icon, wid.main) &&
            endsWith(wid.main, '.svg')) {
        // get the animation with id=shortName stored in mainDefs
        scene_container = document.getElementById(wid.shortName);
        // get the original use on it, used in the icon
        var iconContainer = document.getElementById('iconContainer' + wid.shortName);
        // create a new use
        var use = document.createElement('use');
        // point to the animation
        use.setAttributeNS(xlinkns, 'href', '#' + wid.shortName);
        // resize the animation
        scene_container.setAttribute("width", totalWidth);
        scene_container.setAttribute("height", totalHeight - 120);
        // resize the original use
        //should do: fix the aspect ratio conservation
        var m, t = Math.abs(totalHeight - 120 - totalWidth) / 2;
        if (totalWidth > totalHeight - 120) {
            m = 50 / (totalHeight - 120);
            iconContainer.setAttribute('transform', 'scale(' + m + ',' + m + ') translate(' + (-t) + ',0)');
        } else {
            m = 50 / totalWidth;
            iconContainer.setAttribute('transform', 'scale(' + m + ',' + m + ') translate(0,' + (-t) + ') ');
        }
        // add the new use as widget execution container
        tmp.appendChild(use);
        wid.activate(scene_container);
    } else {
        scene_container.setAttribute("width", totalWidth);
        scene_container.setAttribute("height", totalHeight - 120);
        tmp.appendChild(scene_container);
        scene_container.setAttributeNS(xlinkns, 'href', wid.main);
        wid.activate(scene_container);
    }
    wid.activated = true;
    activatedWidgets.splice(numActivatedWidgets, 0, wid);
    whereW = numActivatedWidgets;
    numActivatedWidgets++;
    adjustWhereWidgets(false);
    wid.load_component = widget_load_component;
    wid.permanent = true;
    wid.on_load = function () {
        WidgetManager.corein_message(this, 'setSize', 'width', totalWidth, 'height', totalHeight - 120, 'dpi', 96);
    };
    //
    if (log_level > l_inf) {
        var i = 0;
        alert(">>>>>>>>>>>>> " + wid.name + " interfaces:");
        for (; i < wid.num_interfaces; i++) {
            alert("" + wid.get_interface(i).type);
        }
    }
    //
}

//
// widget load component (WM callback)
//
function widget_load_component(comp, is_unload) {
    if (l_deb <= log_level) {
        alert('[UI] widget_load_component:' + comp.name);
    }
    if (is_unload) {
        widget_close(comp);
        comp.parent = null;
    } else {
        widget_add(comp);
        comp.permanent = false;
        comp.parent = this;
    }
}

var upnp_renders = null, target_widgets = null;

//
// widget remoting function
//
function on_widget_remote() {
    if (WidgetManager.MPEGUStandardServiceProviders.length != 0 && numActivatedWidgets > 0) {
        upnp_renders = selector_window(activatedWidgets[whereW]);
        upnp_renders.on_select = function(item, wid) {
            upnp_renders.unregister(root);
            upnp_renders = null;
            if (item == -1) {
                return;
            }
            if (wid != null) {
                alert("upnp_renders.on_select(" + item + "," + wid.name + ")");
                //WidgetManager.migrate_widget(UPnP.GetMediaRenderer(item), wid);
                WidgetManager.migrate_widget(WidgetManager.get_mpegu_service_providers(item), wid);
                widget_close(wid);
            }
        };
        upnp_renders.register(root);
    }
}

//
// creates the menu of available targets for pushing a widget elsewhere
//
function selector_window(widget) {
    var i, count, render;
    var selector = document.createElement('g'), obj, child;
    selector.setAttribute('transform', 'translate(10,10)');
    count = WidgetManager.MPEGUStandardServiceProviders.length;
    selector.appendChild(rect(0, 0, 300, 20 * (count + 1), 'white', 'black'));
    for (i = 0; i < count; i++) {
        render = WidgetManager.MPEGUStandardServiceProviders[i];
        obj = createtext(render.Name, 'black', 5, 17 + (20 * i), 15, 'sans-serif');
        obj.setAttribute('id', "selector" + i);
        selector.appendChild(obj);
        obj.addEventListener('mouseover', sw1("selector" + i), false);
        obj.addEventListener('mouseout', sw2("selector" + i), false);
        obj.addEventListener('click', sw3(i, widget), false);
    }
    obj = createtext('Cancel', 'rgb(0,0,120)', 55, 17 + (20 * i), 15, 'sans-serif');
    obj.setAttribute('id', "cancel");
    selector.appendChild(obj);
    obj.addEventListener('mouseover', function(evt) { document.getElementById("cancel").setAttribute("fill", "red"); }, false);
    obj.addEventListener('mouseout', function(evt) { document.getElementById("cancel").setAttribute("fill", "black"); }, false);
    obj.addEventListener('click', function(evt) { upnp_renders.on_select(-1, null); }, false);
    selector.register = function(disp) {
        disp.appendChild(this);
    };
    selector.unregister = function(disp) {
        disp.removeChild(this);
    };
    return selector;
}

function sw1(s) {
    return function(evt) { document.getElementById(s).setAttribute("fill", "blue"); };
}

function sw2(s) {
    return function(evt) { document.getElementById(s).setAttribute("fill", "black"); };
}

function sw3(si, widget) {
    return function(evt) { upnp_renders.on_select(si, widget); };
}

//
// when a widget is pushed to here, install the widget and execute it
//
function onMediaConnect(url, src_ip) {
    if (l_inf <= log_level) {
        alert('[UI] onMediaConnect :\"' + url + '\"');
    }
    if (WidgetManager.probe(url)) {
        var w = WidgetManager.open(url, src_ip);
        if (w == null) {
            return;
        }
        widget_add(w);
        adjustWhereWidgets(false);
        w.permanent = false;
    }
}

//
// file list vars
//
var flstart = 0,fllist = null,maxFileNames = 14;

//
// create a file menu in the main screen, allowing to navigate directories and choose widget config files
//
function on_widget_add_menu() {
    state = 'list';
    widgetContainer.setAttribute('display', 'none');
    homepage.setAttribute('display', 'none');
    homebar.setAttribute('display', 'none');
    execbar.setAttribute('display', 'inline');
    arrows.setAttribute('display', 'none');
    arrowsW.setAttribute('display', 'none');
    widgetAddList.setAttribute('display', 'inline');
    maxFileNames = Math.round(((iconNbVert * 80) / 25) - 1.4);
    isThisAScan = false;
    refillWidgetAddList(false);
}

//
// create a file menu in the main screen, allowing to navigate directories and choose a directory to scan for widgets
// and load all their icons
//
function on_dir_scan() {
    state = 'list';
    widgetContainer.setAttribute('display', 'none');
    homepage.setAttribute('display', 'none');
    homebar.setAttribute('display', 'none');
    execbar.setAttribute('display', 'inline');
    arrows.setAttribute('display', 'none');
    arrowsW.setAttribute('display', 'none');
    widgetAddList.setAttribute('display', 'inline');
    maxFileNames = Math.round(((iconNbVert * 80) / 25) - 1.4);
    isThisAScan = true;
    refillWidgetAddList(true);
}

//
// remove all installed icons
//
function on_clean_up() {
    var i;
    //if (l_inf <= log_level) alert('[UI] unloading ' + WidgetManager.num_widgets + ' widgets');
    for (i = WidgetManager.num_widgets - 1; i >= 0; i--) {
        var w = WidgetManager.get(i);
        if (w.loaded) {
            alert("unloading " + w.name);
            w.loaded = false;
            WidgetManager.unload(w, false);
        }
    }
    where = 0;
    maxwhere = 0;
    removeAllChildren(document.getElementById("icons"));
    adjustwhere(false);
}

//
// install, but do not launch, the widget whose config.xml has been chosen, and return to the home page
//
function widgetInstall(uri, manual, temporary, parent_wid) {
    var wid, j, count = WidgetManager.num_widgets, nbWidgets = getNbWidgets();
    for (j = 0; j < count; j++) {
        wid = WidgetManager.get(j);
        if (wid.url == uri) {
            if (wid.loaded) {
                break;
            }
            if (temporary) {
                wid.permanent = false;
            }
            else {
                insert_icon(wid, nbWidgets, nbWidgets);
            }
        }
    }
    if (j == count) {
        wid = WidgetManager.open(uri, null, parent_wid);
        if (wid != null) {
            if (temporary) {
                wid.permanent = false;
            }
            else {
                insert_icon(wid, nbWidgets, nbWidgets);
            }
        }
    }
    if (manual) {
        return wid;
    }
    state = 'home';
    widgetContainer.setAttribute('display', 'none');
    homepage.setAttribute('display', 'inline');
    homebar.setAttribute('display', 'inline');
    execbar.setAttribute('display', 'none');
    arrows.setAttribute('display', 'inline');
    arrowsW.setAttribute('display', 'none');
    removeAllChildren(widgetAddList);
    maxwhere = ((nbWidgets - 1) - ((nbWidgets - 1) % iconsPerPage)) / iconsPerPage;
    where = maxwhere;
    adjustwhere(false);
    return wid;
}

//
// clean up file list space and refill it
//
function refillWidgetAddList(flag) {
    removeAllChildren(widgetAddList);
    fllist = null;
    flstart = 0;
    fllist = Sys.enum_directory(Sys.last_wdir, "", false);
    fillWidgetAddList(flag);
}

//
// go to parent directory
//
function flUpDir(evt) {
    var s = Sys.last_wdir;
    if (l_inf <= log_level) {
        alert("[UI] lwd:" + Sys.last_wdir);
    }
    var index = s.lastIndexOf("\\");
    if (index != -1) {
        Sys.last_wdir = s.substring(0, index);
        refillWidgetAddList(isThisAScan);
    } else {
        index = s.lastIndexOf("/");
        if (index != -1) {
            Sys.last_wdir = s.substring(0, index);
            refillWidgetAddList(isThisAScan);
        } else {
            Sys.last_wdir = "/";
            refillWidgetAddList(isThisAScan);
        }
    }
}

//
// go to a named directory
//
function flGoTo(newDir) {
    //alert("goto "+newDir);
    var s = Sys.last_wdir;
    if (s == "/") {
        Sys.last_wdir = newDir;
    } else {
        var c = s.charAt(s.length - 1);
        if (c != '\\' && c != '/') {
            s += "/";
        }
        Sys.last_wdir = s + newDir;
    }
    //alert(Sys.last_wdir);
    refillWidgetAddList(isThisAScan);
}

//
// if the directory contains more files that can be shown, show previous page of file names
//
function flPrevFiles(evt) {
    if (flstart == 0) {
        return;
    }
    flstart -= maxFileNames;
    if (flstart < 0) {
        flstart = 0;
    }
    removeAllChildren(widgetAddList);
    fillWidgetAddList(isThisAScan);
}

//
// if the directory contains more files that can be shown, show next page of file names
//
function flNextFiles(evt) {
    if (flstart + maxFileNames < fllist.length) {
        flstart += maxFileNames;
        removeAllChildren(widgetAddList);
        fillWidgetAddList(isThisAScan);
    }
}

//
// scan the current directory recursively for widgets, clean up and return to home page
//
function flScanDir(evt) {
    scan_directory(Sys.last_wdir);
    state = 'home';
    var nbWidgets = getNbWidgets();
    widgetContainer.setAttribute('display', 'none');
    homepage.setAttribute('display', 'inline');
    homebar.setAttribute('display', 'inline');
    execbar.setAttribute('display', 'none');
    arrows.setAttribute('display', 'inline');
    arrowsW.setAttribute('display', 'none');
    removeAllChildren(widgetAddList);
    maxwhere = ((nbWidgets - 1) - ((nbWidgets - 1) % iconsPerPage)) / iconsPerPage;
    where = maxwhere;
    adjustwhere(false);
}

//
// scanning
//
function scan_directory(dir) {
    var ii, j, count, list, w, uri, loadedWidgets = 0;
    list = Sys.enum_directory(dir, '.xml;.wgt', 0);
    for (ii = 0; ii < list.length; ii++) {
        uri = list[ii].path + list[ii].name;
        if (list[ii].directory) {
            scan_directory(uri);
        } else {
            count = WidgetManager.num_widgets;
            for (j = 0; j < count; j++) {
                var wid = WidgetManager.get(j);
                if (wid.loaded) {
                    loadedWidgets++;
                }
                if (wid.url == uri) {
                    if (wid.loaded) {
                        break;
                    }
                    insert_icon(wid, getNbWidgets(), j);
                    break;
                }
            }
            if (j == count) {
                w = WidgetManager.open(uri, null);
                if (w != null) {
                    insert_icon(w, loadedWidgets, WidgetManager.num_widgets - 1);
                }
            }
        }
    }
}

//
// create the up, prev, next button, show current directory and as many file names as possible
// the file names are active: clicking on a directory name goes to that directory
// clicking on a file tries to load that file as a widget
//
function fillWidgetAddList(flag) {
    if (flag) {
        widgetAddList.appendChild(use("cartoucheflag"));
        document.getElementById("dirflag").textContent = Sys.last_wdir;
    } else {
        widgetAddList.appendChild(use("cartouche"));
        document.getElementById("dir").textContent = Sys.last_wdir;
    }
    // next lines are file names
    var obj;
    for (i = 0; i < (fllist.length - flstart) && i < maxFileNames; i++) {
        obj = use("fileMenuElement" + i);
        obj.setAttribute('transform', 'translate(0,' + (25 * (i + 1)) + ')');
        widgetAddList.appendChild(obj);
        document.getElementById("fileMenuElement" + i + "u").setAttributeNS(xlinkns, 'href', "#" + (fllist[i + flstart].directory ? 'folder' : 'new'));
        document.getElementById("fileMenuElement" + i + "t").textContent = fllist[i + flstart].name;
        if (obj.listener != null) {
            obj.removeEventListener("click", obj.listener);
        }
        if (fllist[i + flstart].directory) {
            obj.listener = createGoto(escaping(fllist[i + flstart].name));
            obj.addEventListener("click", obj.listener, false);
        } else if (isWidgetFileName(fllist[i + flstart].name)) {
            obj.listener = createWidgetInstall(escaping(Sys.last_wdir + '/' + fllist[i + flstart].name));
            obj.addEventListener("click", obj.listener, false);
        }
    }
}

function createGoto(s) {
    return function () {
        flGoTo(s);
    };
}

function createWidgetInstall(s) {
    return function () {
        widgetInstall(s, false, false, null);
    };
}

// // // // // // // // // // // // // // //
// function library
// // // // // // // // // // // // // // //

function isWidgetFileName(s) {
    if (endsWith(s, 'config.xml')) {
        return true;
    }
    if (endsWith(s, '.wgt')) {
        return true;
    }
    return false;
}

//
// replace globally \ with / in a string
//
function escaping(s) {
    s = s.replace(/\\/g, '/');
    return s;
}

//
// create rect
//
function rect(x, y, w, h, fill, stroke, id) {
    var child = document.createElement('rect');
    if (id != null) {
        child.setAttribute('id', id);
    }
    child.setAttribute('x', x);
    child.setAttribute('y', y);
    child.setAttribute('width', w);
    child.setAttribute('height', h);
    child.setAttribute('fill', fill);
    child.setAttribute('stroke', stroke);
    return child;
}

//
// create text
//
function createtext(content, fill, x, y, size, family) {
    var child = document.createElement('text');
    child.setAttribute('fill', fill);
    child.textContent = content;
    child.setAttribute('x', x);
    child.setAttribute('y', y);
    child.setAttribute('font-size', size);
    child.setAttribute('font-family', family);
    return child;
}

//
// create invisible rect getting all events
//
function invisible_rect(w, h) {
    var child = document.createElement('rect');
    child.setAttribute('width', w);
    child.setAttribute('height', h);
    child.setAttribute('fill', 'none');
    child.setAttribute('stroke', 'none');
    child.setAttribute('pointer-events', 'all');
    return child;
}

//
// create animation
//
function media(etype, uri, w, h) {
    var child = document.createElement(etype);
    child.setAttributeNS(xlinkns, 'href', uri);
    child.setAttribute('width', w);
    child.setAttribute('height', h);
    if (etype == 'animation') {
        child.setAttributeNS('http://gpac.io/svg-extensions', 'use-as-primary', 'false');
    }
    return child;
}

//
// create use
//
function use(uri) {
    var child = document.createElement('use');
    child.setAttributeNS(xlinkns, 'href', '#' + uri);
    return child;
}

//
// create appropriate element for media reference by the given uri
//
function appropriateElementForMedia(uri, w, h) {
    if (uri.indexOf('#') != -1) {
        return media('animation', uri, w, h);
    }
    if (endsWith(uri, '.svg')) {
        return media('animation', uri, w, h);
    }
    if (endsWith(uri, '.bt')) {
        return media('animation', uri, w, h);
    }
    if (endsWith(uri, '.png')) {
        return media('image', uri, w, h);
    }
    if (endsWith(uri, '.jpg')) {
        return media('image', uri, w, h);
    }
    if (endsWith(uri, '.gif')) {
        return media('image', uri, w, h);
    }
    if (l_war <= log_level) {
        alert("[UI] WARNING: bad suffix for an icon URI: " + uri);
    }
    return media('image', uri, w, h);
}

//
// substitute for the useful predefined function endsWith
//
function endsWith(s1, s2) {
    return s1.toLowerCase().substring(s1.length - s2.length) == s2;
}

/* wrapper as a module
 // export the resize function as the resize member of iphone_wm_gui, as well as other functions
 return {
 resize: resize,
 left_button: left_button,
 right_button: right_button,
 left_buttonW: left_buttonW,
 right_buttonW: right_buttonW,
 home_button: home_button,
 on_dir_scan: on_dir_scan,
 on_clean_up: on_clean_up,
 on_widget_add_menu: on_widget_add_menu,
 on_kill_widget: on_kill_widget,
 on_get_widget: on_get_widget,
 on_widget_remote: on_widget_remote,
 flUpDir: flUpDir,
 flPrevFiles: flPrevFiles,
 flNextFiles: flNextFiles,
 flScanDir: flScanDir,
 get_widget_callback2: get_widget_callback2
 }

 }());*/

function widget_activated_and_bound(wid) {
    WidgetManager.corein_message(wid, "activate");
}

function printAllFieldsOf(obj, printableName) {
    var details = "fields of " + printableName + ":\n";
    for (var field in obj) {
        fieldContents = obj[field];
        if (typeof(fieldContents) == "function") {
            fieldContents = "(function)";
        }
        details += "  " + field + ": " + fieldContents + "\n";
    }
    alert(details);
}
