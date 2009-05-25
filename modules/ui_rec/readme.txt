This is a UI recorder module for the GPAC framework. This module allows recording and playback of user events (mouse and keyboard).

To use this module:
- The variable "Mode" in the section [UIRecord] has to be set to one of the following values
    * "Disable": module is disabled
    * "Record": module will record user interaction
    * "Play": module will play user interaction

The module is designed to work with one scene, events are recorded according to the scene time of the event dispatch. 
The events are recorded/read from a file, which is indicated by the variable "File" of the [UIRecord] section.
Note that the module will automatically set the mode to "Disabled" when closing the player.

This module has been successfully tested under Win32 and Linux

