/*****************************************************************************************
 *
 * Basic Test for HTML 5 Audio support
 *
 *****************************************************************************************/

alert("Script loaded");

function init()
{
	a = document.getElementById("a");
	alert(""+a);
	a.src = "a.mp4";
	alert(""+a.src);
}

init();