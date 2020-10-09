#
#          GPAC - Multimedia Framework C SDK
#
#          Authors: Jean Le Feuvre
#          Copyright (c) Telecom Paris 2020
#                  All rights reserved
#
#  Python ctypes bindings for GPAC (core initialization and filters API only)
#
#  GPAC is free software; you can redistribute it and/or modify
#  it under the terfsess of the GNU Lesser General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  GPAC is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; see the file COPYING.  If not, write to
#  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#


## 
#  \defgroup pyapi_grp Python APIs
#  \brief Python API for libgpac.
#
#  # Foreword
#  This module provides [ctypes](https://docs.python.org/3/library/ctypes.html) bindings for libgpac. These bindings currently allow:
#   - initializing libgpac, setting global arguments for the library
#   - creation, running and destruction of a filter session in blocking mode or non-blocking mode
#   - inspection of filters in a session
#   - user tasks scheduling
#   - Creation of custom filters in Python
#
# \note  This is an initial work and more bindings might be needed, feel free to contribute/PR on https://github.com/gpac/gpac
#
# # Error handling
#
# Errors are handled through raising exceptions, except callback methods wich must return a \ref GF_Err value. 
# # Properties handling
#
# Properties types are automatically converted to and from string. If the property name is not a built-in property type, the property is assumed to be a user-defined property. 
# For example, when querying or setting a stream type property, use the property name `StreamType`.
# See [`gpac -h props`](https://github.com/gpac/gpac/wiki/filters_properties) for the complete list of built-in property names.
#
# Properties values are automatically converted to or from python types whenever possible. Types with no python equivalent (vectors, fractions) are defined as classes in python.
# For example:
# - when setting a UIntList property, pass a python list of ints 
# - when reading a UIntList property, a python list of ints will be returned 
# - when setting a PropVec2i property, pass a PropVec2i object 
# - when setting a PropVec2iList property, pass a python list of PropVec2i 
#
# The following builtin property types are always handled as strings in Python instead of int in libgpac :
# - StreamType:  string containing the streamtype name
# - CodecID:  string containing the codec name
# - PixelFormat:  string containing the pixel format name
# - AudioFormat:  string containing the audio format name
#
# # Basic setup
#
# You must initialize libgpac before any other calls, and uninintialize it after everything gpac-related is done.
# You also must destroy explicitly the filter session, as otherwise the garbage collection on the filter session object could happen after libgpac shutdown and will likely crash
#
# \code
# import libgpac as gpac
# gpac.init()
# #create a session in blocking mode
# fs = gpac.FilterSession()
# #setup all your filters
# ...
# #run the session
# fs.run()
# fs.delete()
# gpac.close()
# \endcode
#
# You can specify global options of libgpac and filters (e.g. --opt) by assigning arguments to libgpac:
# \code
# #set global arguments, here inherited from command line
# gpac.set_args(sys.argv)
# \endcode
#
# You can also specify the log levels you want
# \code
# gpac.set_logs("all@info")
# \endcode
# # Session using built-in filters
#
# Filters are loaded as usual in gpac as a string description
# \code
# src = fs.load_src("$URL:opt:opt2=val")
# dst = fs.load_dst("$URL2:opt3:opt4=val")
# f = fs.load("filtername:optX")
# \endcode
#
# It is possible to assign sources of a filter (much like the `@` command in `gpac`):
# \code
# dst.set_source(f)
# \endcode
#
# # Posting user tasks
# 
# You can post tasks to the session scheduler to get called back (usefull when running the session in blocking mode)
# Tasks must derive FilterTask class and implement their own `execute` method
# \code
# class MyTask(FilterTask): 
#   def exectute(self):
#       do_something()
#       #last scheduled task (session is over), abort
#       if self.session.last_task:
#           return -1 
#       //keep task active, reschedule in 500 ms 
#       return 500 
#
# task = MyTask('CustomTask')
# fs.post_task(task)
# \endcode
#
# # Creating custom filters
#
# A custom filter allows your application to interact closely with the media pipeline, but cannot be used in graph resolution.
# Custom filters can be sources, sinks, or intermediate filters. The following limitations however exist:
#- custom filters will not be cloned
#- custom filters cannot be used as destination of filters loading source or destination a filter graph dynamically, such as the dashin or dasher filters.
#
# A custom filter must implement the \ref FilterCustom class, and optionaly provide the following methods
# - configure_pid: callback for PID configuration, mandatory if your filter is not a source
# - process: callback for processing
# - process_event: callback for processing and event
# - probe_data: callback for probing a data format
# - reconfigure_output: callback for output reconfiguration (PID capability negociation)
#
# A custom filter must also declare its capabilities, input and output, using push_cap method 
# \code
# class MyFilter(FilterTask): 
#   def __init__(self, session):
#    # !! call constructor of parent class first !!
#    gpac.FilterCustom.__init__(self, session, "PYnspect")
#    #indicate what we accept and produce - this can be done either in the constructor or after, but before calling \ref run or \ref run_step 
#    #here we indicate we accept anything of type visual, and produce anything of type visual
#    self.push_cap("StreamType", "Visual", gpac.GF_CAPS_INPUT_OUTPUT)
#
#   #we accept input pids, we must configure them
#   #def configure_pid(self, pid, is_remove):
#    #do something, return value is a GF_Err value
#    #pid being removed
#    if is_remove:
#    #pid being reconfigured
#    elif pid in self.ipids:
#    #pid first connection
#    else:
#
#   #process
#   #def process(self):
#    #do something, return value is a GF_Err value
#
#   #process_event takes a \ref FilterEvent parameter
#   #def process_event(self, evt):
#    #do something, return value is True (cancelled) or False
#
#   #probe_data takes a NumPy array if numpy is available (and _size can be ignored) or a POINTER(c_ubyte) otherwise
#   #def probe_data(self, data, _size):
#    #do something, return value is mime type or None
#
#   #probe_data takes the target output pid as parameter
#   #def reconfigure_output(self, opid):
#    #do something, return value is a GF_Err value
#
# \endcode
#
# \note
# The \ref FilterCustom object has a list of input and output PIDs created. Before a callback to configure_pid, the input PID is not registered to the list 
# of input PIDs if this is the first time the PID is configured.
#
# \code
# src = fs.load_src("$URL:opt:opt2=val")
# dst = fs.load_dst("$URL2:opt3:opt4=val")
# #load a custom filter
# my_filter = MyFilter(fs)
# #make sure dst only consumes from our custom filter
# dst.set_source(f)
# \endcode
# @{
#


from ctypes import *
import datetime
import types
import os
import importlib

## set to True if numpy was successfully loaded
##\hideinitializer
numpy_support=True

try:
    importlib.import_module('numpy')
except ImportError:
    numpy_support = False
    print("\nWARNING! numpy not present, packet data type is ctypes POINTER(c_ubyte)\n")

##ctypes instance of libgpac
##\hideinitializer
_libgpac=None
#load libgpac
try:
    _libgpac = cdll.LoadLibrary("libgpac.dll")
except OSError:
    try:
        _libgpac = cdll.LoadLibrary("libgpac.so")
    except OSError:
        try:
            _libgpac = cdll.LoadLibrary("libgpac.dylib")
        except OSError:
            print('Failed to locate libgpac (.so/.dll/.dylib) - make sure it is in your system path')
            os._exit(1)
#
# Type definitions
#

##\cond private
_gf_filter_session = c_void_p
_gf_filter = c_void_p
_gf_filter_pid = c_void_p
_gf_filter_packet = c_void_p
_gf_property_entry = c_void_p
##\endcond


## fraction object, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_Fraction
class Fraction(Structure):
    ## \cond private
    _fields_ = [ ("num", c_int), ("den", c_uint)]
    def __str__(self):
        return str(self.num)+'/' + str(self.den)
    ## \endcond

## large fraction object, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_Fraction64
class Fraction64(Structure):
    ## \cond private
    _fields_ = [("num", c_longlong), ("den", c_ulonglong)]
    def __str__(self):
        return str(self.num)+'/' + str(self.den)
    ## \endcond

## filter statistics object, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FilterStats
class FilterStats(Structure):
    ## \cond private
	_fields_ = [
		("filter", _gf_filter),
		("filter_alias", _gf_filter),
		("nb_tasks_done", c_ulonglong),
		("nb_pck_processed", c_ulonglong),
		("nb_bytes_processed", c_ulonglong),
		("nb_pck_sent", c_ulonglong),
		("nb_hw_pck_sent", c_ulonglong),
		("nb_errors", c_uint),
		("nb_bytes_sent", c_ulonglong),
		("time_process", c_ulonglong),
		("percent", c_int),
		("status", c_char_p),
		("report_updated", c_bool),
		("name", c_char_p),
		("reg_name", c_char_p),
		("filter_id", c_char_p),
		("done", c_bool),
		("nb_pid_in", c_uint),
		("nb_in_pck", c_ulonglong),
		("nb_pid_out", c_uint),
		("nb_out_pck", c_ulonglong),
		("in_eos", c_bool),
		("type", c_int),
		("stream_type", c_int),
		("codecid", c_int),
		("last_ts_sent", Fraction64),
		("last_ts_drop", Fraction64)
	]
    ## \endcond

## filter argument object, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FilterArgs
class FilterArg(Structure):
    ## \cond private
    _fields_ = [
        ("name", c_char_p),
        ("reserved", c_int),
        ("description", c_char_p),
        ("type", c_int),
        ("default", c_char_p),
        ("min_max_enum", c_char_p),
        ("flags", c_uint)
    ]
    def __str__(self):
        res = '- ' + self.name.decode('utf-8') + ' (' + _libgpac.gf_props_get_type_name(self.type).decode('utf-8') + '): ' + self.description.decode('utf-8')
        if self.default or self.min_max_enum:
            res += ' ('
            if self.default:
                res += 'default: ' + self.default.decode('utf-8')
            if self.min_max_enum:
                res += ' min_max_enum: ' + self.min_max_enum.decode('utf-8')
            res += ')'
        return res
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec2i
class PropVec2i(Structure):
    ## \cond private
    _fields_ = [("x", c_int), ("y", c_int)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec2
class PropVec2(Structure):
    ## \cond private
    _fields_ = [("x", c_double),("y", c_double)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec3i
class PropVec3i(Structure):
    ## \cond private
    _fields_ = [("x", c_int), ("y", c_int), ("z", c_int)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec3
class PropVec3(Structure):
    ## \cond private
    _fields_ = [ ("x", c_double), ("y", c_double), ("z", c_double)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec4i
class PropVec4i(Structure):
    ## \cond private
    _fields_ = [("x", c_int), ("y", c_int), ("z", c_int), ("w", c_int)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)+'x' + str(self.w)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec4
class PropVec4(Structure):
    ## \cond private
    _fields_ = [("x", c_double), ("y", c_double), ("z", c_double), ("w", c_double)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)+'x' + str(self.w)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropData
class PropData(Structure):
    ## \cond private
    _fields_ = [("ptr", c_void_p), ("size", c_uint)]
    def __str__(self):
        return 'data,size:'+str(self.size)
    ## \endcond

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropUIntList
class PropStringList(Structure):
    _fields_ = [("vals", POINTER(c_char_p)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += self.vals[i].decode('utf-8')
        return res

## \cond private

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropUIntList
class PropUIntList(Structure):
    _fields_ = [("vals", POINTER(c_uint)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += str(self.vals[i])
        return res

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropIntList
class PropIntList(Structure):
    _fields_ = [("vals", POINTER(c_int)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += str(self.vals[i])
        return res

## filter prop type, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_PropVec2iList
class PropVec2iList(Structure):
    _fields_ = [("vals", POINTER(PropVec2i)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += str(self.vals[i].x) + 'x' + str(self.vals[i].y)
        return res

## filter property union value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as GF_PropertyValue
class PropertyValueUnion(Union):
	_fields_ = [
		("longuint", c_ulonglong),
		("longsint", c_longlong),
		("sint", c_int),
		("uint", c_uint),
		("boolean", c_bool),
		("frac", Fraction),
		("lfrac", Fraction64),
		("fnumber", c_float),
		("number", c_double),
		("vec2i", PropVec2i),
		("vec2", PropVec2),
		("vec3i", PropVec3i),
		("vec3", PropVec3),
		("vec4i", PropVec3i),
		("vec4", PropVec3),
		("data", PropData),
		("string", c_char_p),
		("ptr", c_void_p),
        ("string_list", PropStringList),
        ("uint_list", PropUIntList),
		("int_list", PropIntList),
		("v2i_list", PropVec2iList)
		#todo, map string list ...
	]

## filter property value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as GF_PropertyValue
class PropertyValue(Structure):
	_fields_ = [
		("type", c_uint),
		("value", PropertyValueUnion)
	]

## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_Base
class FEVT_Base(Structure):
    ## \cond private
    _fields_ =  [("type", c_uint), ("on_pid", _gf_filter_pid)]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_Play
class FEVT_Play(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("start_range", c_double),
        ("end_range", c_double),
        ("speed", c_double),
        ("from_pck", c_uint),
        ("hw_buffer_reset", c_ubyte),
        ("initial_broadcast_play", c_ubyte),
        ("timestamp_based", c_ubyte),
        ("full_file_only", c_ubyte),
        ("forced_dash_segment_switch", c_ubyte),
        ("drop_non_ref", c_ubyte),
        ("no_byterange_forward", c_ubyte)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_SourceSeek
class FEVT_SourceSeek(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("start_offset", c_ulonglong),
        ("end_offset", c_ulonglong),
        ("source_switch", c_char_p),
        ("previous_is_init_segment", c_ubyte),
        ("skip_cache_expiration", c_ubyte),
        ("hint_block_size", c_uint)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_SegmentSize
class FEVT_SegmentSize(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("seg_url", c_char_p),
        ("is_init", c_bool),
        ("media_range_start", c_ulonglong),
        ("media_range_end", c_ulonglong),
        ("idx_range_start", c_ulonglong),
        ("idx_range_end", c_ulonglong)
    ]
    ## \endcond


## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_AttachScene
class FEVT_AttachScene(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("odm", c_void_p)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_QualitySwitch
class FEVT_QualitySwitch(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("up", c_bool),
        ("dependent_group_index", c_uint),
        ("q_idx", c_int),
        ("set_tile_mode_plus_one", c_uint),
        ("quality_degradation", c_uint)
    ]
    ## \endcond

#TODO GF_FEVT_Event;

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_FileDelete
class FEVT_FileDelete(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("url", c_char_p)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_VisibilityHint
class FEVT_VisibilityHint(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("min_x", c_uint),
        ("max_x", c_uint),
        ("min_y", c_uint),
        ("max_y", c_uint),
        ("is_gaze", c_bool)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_BufferRequirement
class FEVT_BufferRequirement(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("max_buffer_us", c_uint),
        ("max_playout_us", c_uint),
        ("min_playout_us", c_uint),
        ("pid_only", c_bool)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as GF_FilterEvent
class FilterEvent(Union):
    ## constructor
    #\param type type of event
    def __init__(self, type=0):
        self.base.type = type

    ## \cond private
    _fields_ =  [
        ("base", FEVT_Base),
        ("play", FEVT_Play),
        ("seek", FEVT_SourceSeek),
        ("attach_scene", FEVT_AttachScene),
        #("user_event", FEVT_Event,
        ("quality_switch", FEVT_QualitySwitch),
        ("visibility_hint", FEVT_VisibilityHint),
        ("buffer_req", FEVT_BufferRequirement),
        ("seg_size", FEVT_SegmentSize),
        ("file_del", FEVT_FileDelete)
    ]
    ## \endcond


#
# Constants definitions
#


# scheduler type definitions
##\hideinitializer
#see \ref GF_FS_SCHEDULER_LOCK_FREE
GF_FS_SCHEDULER_LOCK_FREE=0
##\hideinitializer
##see \ref GF_FS_SCHEDULER_LOCK
GF_FS_SCHEDULER_LOCK=1
##\hideinitializer
##see \ref GF_FS_SCHEDULER_LOCK_FREE_X
GF_FS_SCHEDULER_LOCK_FREE_X=2
##\hideinitializer
##see \ref GF_FS_SCHEDULER_LOCK_FORCE
GF_FS_SCHEDULER_LOCK_FORCE=3
##\hideinitializer
##see \ref GF_FS_SCHEDULER_DIRECT
GF_FS_SCHEDULER_DIRECT=4

#session flags
##\hideinitializer
#see \ref GF_FS_FLAG_LOAD_META
GF_FS_FLAG_LOAD_META=1<<1
##\hideinitializer
#see \ref GF_FS_FLAG_NO_BLOCKING
GF_FS_FLAG_NO_BLOCKING=1<<2
##\hideinitializer
#see \ref GF_FS_FLAG_NO_GRAPH_CACHE
GF_FS_FLAG_NO_GRAPH_CACHE=1<<3
##\hideinitializer
#see \ref GF_FS_FLAG_NO_MAIN_THREAD
GF_FS_FLAG_NO_MAIN_THREAD=1<<4
##\hideinitializer
#see \ref GF_FS_FLAG_NO_REGULATION
GF_FS_FLAG_NO_REGULATION=1<<5
##\hideinitializer
#see \ref GF_FS_FLAG_NO_PROBE
GF_FS_FLAG_NO_PROBE=1<<6
##\hideinitializer
#see \ref GF_FS_FLAG_NO_REASSIGN
GF_FS_FLAG_NO_REASSIGN=1<<7
##\hideinitializer
#see \ref GF_FS_FLAG_PRINT_CONNECTIONS
GF_FS_FLAG_PRINT_CONNECTIONS=1<<8
##\hideinitializer
#see \ref GF_FS_FLAG_NO_ARG_CHECK
GF_FS_FLAG_NO_ARG_CHECK=1<<9
##\hideinitializer
#see \ref GF_FS_FLAG_NO_RESERVOIR
GF_FS_FLAG_NO_RESERVOIR=1<<10
##\hideinitializer
#see \ref GF_FS_FLAG_FULL_LINK
GF_FS_FLAG_FULL_LINK=1<<11


##\hideinitializer
#see \ref GF_PROP_FORBIDEN
GF_PROP_FORBIDEN=0
##\hideinitializer
#see \ref GF_PROP_SINT
GF_PROP_SINT=1
##\hideinitializer
#see \ref GF_PROP_UINT
GF_PROP_UINT=2
##\hideinitializer
#see \ref GF_PROP_LSINT
GF_PROP_LSINT=3
##\hideinitializer
#see \ref GF_PROP_LUINT
GF_PROP_LUINT=4
##\hideinitializer
#see \ref GF_PROP_BOOL
GF_PROP_BOOL=5
##\hideinitializer
#see \ref GF_PROP_FRACTION
GF_PROP_FRACTION=6
##\hideinitializer
#see \ref GF_PROP_FRACTION64
GF_PROP_FRACTION64=7
##\hideinitializer
#see \ref GF_PROP_FLOAT
GF_PROP_FLOAT=8
##\hideinitializer
#see \ref GF_PROP_DOUBLE
GF_PROP_DOUBLE=9
##\hideinitializer
#see \ref GF_PROP_VEC2I
GF_PROP_VEC2I=10
##\hideinitializer
#see \ref GF_PROP_VEC2
GF_PROP_VEC2=11
##\hideinitializer
#see \ref GF_PROP_VEC3I
GF_PROP_VEC3I=12
##\hideinitializer
#see \ref GF_PROP_VEC3
GF_PROP_VEC3=13
##\hideinitializer
#see \ref GF_PROP_VEC4I
GF_PROP_VEC4I=14
##\hideinitializer
#see \ref GF_PROP_VEC4
GF_PROP_VEC4=15
##\hideinitializer
#see \ref GF_PROP_PIXFMT
GF_PROP_PIXFMT=16
##\hideinitializer
#see \ref GF_PROP_PCMFMT
GF_PROP_PCMFMT=17
##\hideinitializer
#see \ref GF_PROP_STRING
GF_PROP_STRING=18
##\hideinitializer
#see \ref GF_PROP_STRING_NO_COPY
GF_PROP_STRING_NO_COPY=19
##\hideinitializer
#see \ref GF_PROP_DATA
GF_PROP_DATA=20
##\hideinitializer
#see \ref GF_PROP_NAME
GF_PROP_NAME=21
##\hideinitializer
#see \ref GF_PROP_DATA_NO_COPY
GF_PROP_DATA_NO_COPY=22
##\hideinitializer
#see \ref GF_PROP_CONST_DATA
GF_PROP_CONST_DATA=23
##\hideinitializer
#see \ref GF_PROP_POINTER
GF_PROP_POINTER=24
##\hideinitializer
#see \ref GF_PROP_STRING_LIST
GF_PROP_STRING_LIST=25
##\hideinitializer
#see \ref GF_PROP_UINT_LIST
GF_PROP_UINT_LIST=26
##\hideinitializer
#see \ref GF_PROP_SINT_LIST
GF_PROP_SINT_LIST=27
##\hideinitializer
#see \ref GF_PROP_VEC2I_LIST
GF_PROP_VEC2I_LIST=28


##\hideinitializer
#see GF_FEVT_PLAY
GF_FEVT_PLAY = 1
##\hideinitializer
#see GF_FEVT_SET_SPEED
GF_FEVT_SET_SPEED = 2
##\hideinitializer
#see GF_FEVT_STOP
GF_FEVT_STOP = 3
##\hideinitializer
#see GF_FEVT_PAUSE
GF_FEVT_PAUSE = 4
##\hideinitializer
#see GF_FEVT_RESUME
GF_FEVT_RESUME = 5
##\hideinitializer
#see GF_FEVT_SOURCE_SEEK
GF_FEVT_SOURCE_SEEK = 6
##\hideinitializer
#see GF_FEVT_SOURCE_SWITCH
GF_FEVT_SOURCE_SWITCH = 7
##\hideinitializer
#see GF_FEVT_SEGMENT_SIZE
GF_FEVT_SEGMENT_SIZE = 8
##\hideinitializer
#see GF_FEVT_ATTACH_SCENE
GF_FEVT_ATTACH_SCENE = 9
##\hideinitializer
#see GF_FEVT_RESET_SCENE
GF_FEVT_RESET_SCENE = 10
##\hideinitializer
#see GF_FEVT_QUALITY_SWITCH
GF_FEVT_QUALITY_SWITCH = 11
##\hideinitializer
#see GF_FEVT_VISIBILITY_HINT
GF_FEVT_VISIBILITY_HINT = 12
##\hideinitializer
#see GF_FEVT_INFO_UPDATE
GF_FEVT_INFO_UPDATE = 13
##\hideinitializer
#see GF_FEVT_BUFFER_REQ
GF_FEVT_BUFFER_REQ = 14
##\hideinitializer
#see GF_FEVT_CAPS_CHANGE
GF_FEVT_CAPS_CHANGE = 15
##\hideinitializer
#see GF_FEVT_CONNECT_FAIL
GF_FEVT_CONNECT_FAIL = 16
##\hideinitializer
#see GF_FEVT_USER
GF_FEVT_USER = 17
##\hideinitializer
#see GF_FEVT_PLAY_HINT
GF_FEVT_PLAY_HINT = 18
##\hideinitializer
#see GF_FEVT_FILE_DELETE
GF_FEVT_FILE_DELETE = 19


##\hideinitializer
#see GF_FS_ARG_HINT_NORMAL
GF_FS_ARG_HINT_NORMAL = 0
##\hideinitializer
#see GF_FS_ARG_HINT_ADVANCED
GF_FS_ARG_HINT_ADVANCED = 1<<1
##\hideinitializer
#see GF_FS_ARG_HINT_EXPERT
GF_FS_ARG_HINT_EXPERT = 1<<2
##\hideinitializer
#see GF_FS_ARG_HINT_HIDE
GF_FS_ARG_HINT_HIDE = 1<<3
##\hideinitializer
#see GF_FS_ARG_UPDATE
GF_FS_ARG_UPDATE = 1<<4
##\hideinitializer
#see GF_FS_ARG_META
GF_FS_ARG_META = 1<<5
##\hideinitializer
#see GF_FS_ARG_META_ALLOC
GF_FS_ARG_META_ALLOC = 1<<6
##\hideinitializer
#see GF_FS_ARG_SINK_ALIAS
GF_FS_ARG_SINK_ALIAS = 1<<7


##\hideinitializer
#see GF_CAPFLAG_IN_BUNDLE
GF_CAPFLAG_IN_BUNDLE = 1
##\hideinitializer
#see GF_CAPFLAG_INPUT
GF_CAPFLAG_INPUT = 1<<1
##\hideinitializer
#see GF_CAPFLAG_OUTPUT
GF_CAPFLAG_OUTPUT = 1<<2
##\hideinitializer
#see GF_CAPFLAG_EXCLUDED
GF_CAPFLAG_EXCLUDED = 1<<3
##\hideinitializer
#see GF_CAPFLAG_LOADED_FILTER
GF_CAPFLAG_LOADED_FILTER = 1<<4
##\hideinitializer
#see GF_CAPFLAG_STATIC
GF_CAPFLAG_STATIC = 1<<5
##\hideinitializer
#see GF_CAPFLAG_OPTIONAL
GF_CAPFLAG_OPTIONAL = 1<<6

#helpers
##\hideinitializer
#see GF_CAPS_INPUT
GF_CAPS_INPUT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT)
##\hideinitializer
#see GF_CAPS_INPUT_OPT
GF_CAPS_INPUT_OPT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OPTIONAL)
##\hideinitializer
#see GF_CAPS_INPUT_STATIC
GF_CAPS_INPUT_STATIC = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_STATIC)
##\hideinitializer
#see GF_CAPS_INPUT_STATIC_OPT
GF_CAPS_INPUT_STATIC_OPT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_STATIC|GF_CAPFLAG_OPTIONAL)
##\hideinitializer
#see GF_CAPS_INPUT_EXCLUDED
GF_CAPS_INPUT_EXCLUDED = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED)
##\hideinitializer
#see GF_CAPS_INPUT_LOADED_FILTER
GF_CAPS_INPUT_LOADED_FILTER = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_LOADED_FILTER)
##\hideinitializer
#see GF_CAPS_OUTPUT
GF_CAPS_OUTPUT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT)
##\hideinitializer
#see GF_CAPS_OUTPUT_LOADED_FILTER
GF_CAPS_OUTPUT_LOADED_FILTER = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_LOADED_FILTER)
##\hideinitializer
#see GF_CAPS_OUTPUT_EXCLUDED
GF_CAPS_OUTPUT_EXCLUDED = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_EXCLUDED)
##\hideinitializer
#see GF_CAPS_OUTPUT_STATIC
GF_CAPS_OUTPUT_STATIC = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_STATIC)
##\hideinitializer
#see GF_CAPS_OUTPUT_STATIC_EXCLUDED
GF_CAPS_OUTPUT_STATIC_EXCLUDED = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_STATIC)
##\hideinitializer
#see GF_CAPS_INPUT_OUTPUT
GF_CAPS_INPUT_OUTPUT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT)
##\hideinitializer
#see GF_CAPS_INPUT_OUTPUT_OPT
GF_CAPS_INPUT_OUTPUT_OPT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_OPTIONAL)


##\hideinitializer
#see \ref GF_SCRIPT_INFO
GF_SCRIPT_INFO = 3
##\hideinitializer
#see \ref GF_PENDING_PACKET
GF_PENDING_PACKET = 2
##\hideinitializer
#see \ref GF_EOS
GF_EOS = 1
##\hideinitializer
#see \ref GF_OK
GF_OK = 0
##\hideinitializer
#see \ref GF_BAD_PARAM
GF_BAD_PARAM = -1
##\hideinitializer
#see \ref GF_OUT_OF_MEM
GF_OUT_OF_MEM = -2
##\hideinitializer
#see \ref GF_IO_ERR
GF_IO_ERR = -3
##\hideinitializer
#see \ref GF_NOT_SUPPORTED
GF_NOT_SUPPORTED = -4
##\hideinitializer
#see \ref GF_CORRUPTED_DATA
GF_CORRUPTED_DATA = -5
##\hideinitializer
#see \ref GF_SCRIPT_ERROR
GF_SCRIPT_ERROR = -8
##\hideinitializer
#see \ref GF_BUFFER_TOO_SMALL
GF_BUFFER_TOO_SMALL = -9
##\hideinitializer
#see \ref GF_NON_COMPLIANT_BITSTREAM
GF_NON_COMPLIANT_BITSTREAM = -10
##\hideinitializer
#see \ref GF_FILTER_NOT_FOUND
GF_FILTER_NOT_FOUND = -11
##\hideinitializer
#see \ref GF_URL_ERROR
GF_URL_ERROR = -12
##\hideinitializer
#see \ref GF_SERVICE_ERROR
GF_SERVICE_ERROR = -13
##\hideinitializer
#see \ref GF_REMOTE_SERVICE_ERROR
GF_REMOTE_SERVICE_ERROR = -14
##\hideinitializer
#see \ref GF_STREAM_NOT_FOUND
GF_STREAM_NOT_FOUND = -15
##\hideinitializer
#see \ref GF_IP_ADDRESS_NOT_FOUND
GF_IP_ADDRESS_NOT_FOUND = -40
##\hideinitializer
#see \ref GF_IP_CONNECTION_FAILURE
GF_IP_CONNECTION_FAILURE = -41
##\hideinitializer
#see \ref GF_IP_NETWORK_FAILURE
GF_IP_NETWORK_FAILURE = -42
##\hideinitializer
#see \ref GF_IP_CONNECTION_CLOSED
GF_IP_CONNECTION_CLOSED = -43
##\hideinitializer
#see \ref GF_IP_NETWORK_EMPTY
GF_IP_NETWORK_EMPTY = -44
##\hideinitializer
#see \ref GF_IP_SOCK_WOULD_BLOCK
GF_IP_SOCK_WOULD_BLOCK = -45
##\hideinitializer
#see \ref GF_IP_UDP_TIMEOUT
GF_IP_UDP_TIMEOUT = -46
##\hideinitializer
#see \ref GF_AUTHENTICATION_FAILURE
GF_AUTHENTICATION_FAILURE = -50
##\hideinitializer
#see \ref GF_SCRIPT_NOT_READY
GF_SCRIPT_NOT_READY = -51
##\hideinitializer
#see \ref GF_INVALID_CONFIGURATION
GF_INVALID_CONFIGURATION = -52
##\hideinitializer
#see \ref GF_NOT_FOUND
GF_NOT_FOUND = -53
##\hideinitializer
#see \ref GF_PROFILE_NOT_SUPPORTED
GF_PROFILE_NOT_SUPPORTED = -54
##\hideinitializer
#see \ref GF_REQUIRES_NEW_INSTANCE
GF_REQUIRES_NEW_INSTANCE = -56
##\hideinitializer
#see \ref GF_FILTER_NOT_SUPPORTED
GF_FILTER_NOT_SUPPORTED = -57



##\cond private
#error to string helper
_libgpac.gf_error_to_string.argtypes = [c_int]
_libgpac.gf_error_to_string.restype = c_char_p
 
_libgpac.gf_gpac_version.restype = c_char_p
_libgpac.gf_gpac_copyright.restype = c_char_p
_libgpac.gf_gpac_copyright_cite.restype = c_char_p

_libgpac.gf_sys_set_args.argtypes = [c_int, POINTER(POINTER(c_char))]

_libgpac.gf_sys_init.argtypes = [c_int, c_char_p]
_libgpac.gf_log_set_tools_levels.argtypes = [c_char_p, c_int]
_libgpac.gf_props_get_type_name.argtypes = [c_uint]
_libgpac.gf_props_get_type_name.restype = c_char_p

_libgpac.gf_sys_clock.res = c_uint
_libgpac.gf_sys_clock_high_res.res = c_ulonglong

_libgpac.gf_sys_profiler_send.argtypes = [c_char_p]
_libgpac.gf_sys_profiler_sampling_enabled.restype = c_bool
_libgpac.gf_sys_profiler_enable_sampling.argtypes = [c_bool]

#\endcond

##libgpac version (string)
#\hideinitializer
version = _libgpac.gf_gpac_version().decode("utf-8")
##libgpac copyright notice (string)
#\hideinitializer
copyright = _libgpac.gf_gpac_copyright().decode("utf-8")
##libgpac full copyright notice (string)
#\hideinitializer
copyright_cite = _libgpac.gf_gpac_copyright_cite().decode("utf-8")

## convert error value to string message
# \param err gpac error code (int)
# \return string
def e2s(err):
    return _libgpac.gf_error_to_string(err).decode('utf-8')


mem_track_on=0
## initialize libgpac - see \ref gf_sys_init
# \param mem_track
# \param profile
# \return
#
def init(mem_track=0, profile=None):
    if mem_track!=0 or profile != None:
        err = _libgpac.gf_sys_init(mem_track, profile)
    else:
        err = _libgpac.gf_sys_init(0, None)

    mem_track_on=mem_track
    if not hasattr(_libgpac, 'gf_memory_size'):
        mem_track_on=0
    else:
        mem_track_on=mem_track
    
    if err<0: 
        raise Exception('Failed to initialize libgpac: ' + e2s(err))


## close libgpac - see \ref gf_sys_close
# \note Make sure you have destroyed all associated gpac resources before calling this !
# \return
#
def close():
    _libgpac.gf_sys_close()
    if mem_track_on:
        if _libgpac.gf_memory_size() or _libgpac.gf_file_handles_count():
            set_logs("mem@info")
            _libgpac.gf_memory_print()

## set log tools and levels - see \ref gf_log_set_tools_levels
# \note Make sure you have destroyed all associated gpac resources before calling this !
# \param logs
# \param reset if true, resets all logs to default
# \return
def set_logs(logs, reset=False):
    _libgpac.gf_log_set_tools_levels(logs.encode('utf-8'), reset)

## get clock - see \ref gf_sys_clock
# \return clock in milliseconds
def sys_clock():
    return _libgpac.gf_sys_clock()

## get high res clock - see \ref gf_sys_clock_high_res
# \return clock in microseconds
def sys_clock_high_res():
    return _libgpac.gf_sys_clock_high_res()

## set libgpac arguments - see \ref gf_sys_set_args
# \param args list of strings
# \return
def set_args(args):
    p = (POINTER(c_char)*len(args))()
    for i, arg in enumerate(args):
        enc_arg = arg.encode('utf-8')
        p[i] = create_string_buffer(enc_arg)
    _libgpac.gf_sys_set_args(len(args), cast(p, POINTER(POINTER(c_char))) )


##\cond private
_libgpac.gf_sys_profiler_set_callback.argtypes = [py_object, c_void_p]
@CFUNCTYPE(c_int, c_void_p, c_char_p)
def rmt_fun_cbk(_udta, text):
    obj = cast(_udta, py_object).value
    obj.on_rmt_event(text.decode('utf-8'))
    return 0
##\endcond private


## set profiler (Remotery) callback - see \ref gf_sys_profiler_set_callback
# \param callback_obj object to call back, must have a method `on_rmt_event` taking a single string parameter
# \return True if success, False if no Remotery support
def set_rmt_fun(callback_obj):
    if hasattr(callback_obj, 'on_rmt_event')==False:
        raise Exception('No on_rmt_event function on callback')
    err = _libgpac.gf_sys_profiler_set_callback(py_object(callback_obj), rmt_fun_cbk)
    if err<0:
        return False
    return True

## send message to profiler (Remotery) - see \ref gf_sys_profiler_send
# \param text text to send
# \return True if success, False if no Remotery support
def rmt_send(text):
    err = _libgpac.gf_sys_profiler_send(text.encode('utf-8'))
    if err<0:
        return False
    return True

## check if profiler (Remotery) sampling is enabled - see \ref gf_sys_profiler_sampling_enabled
# \return True if enabled, False otherwise
def rmt_on():
    return _libgpac.gf_sys_profiler_sampling_enabled()

## enable or disable sampling in profiler (Remotery) - see \ref gf_sys_profiler_enable_sampling
# \param value enable or disable sampling
# \return
def rmt_enable(value):
    _libgpac.gf_sys_profiler_enable_sampling(value)
    

##\cond private

_gf_filter_session = c_void_p
_gf_filter = c_void_p
_gf_filter_pid = c_void_p

#
#        filter session bindings
#

_libgpac.gf_fs_new.argtypes = [c_int, c_int, c_int, c_char_p]
_libgpac.gf_fs_new.restype = _gf_filter_session

_libgpac.gf_fs_del.argtypes = [_gf_filter_session]
_libgpac.gf_fs_del.restype =  None

_libgpac.gf_fs_run.argtypes = [_gf_filter_session]
_libgpac.gf_fs_run_step.argtypes = [_gf_filter_session]

_libgpac.gf_fs_load_source.argtypes = [_gf_filter_session, c_char_p, c_char_p, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_source.restype = _gf_filter

_libgpac.gf_fs_load_destination.argtypes = [_gf_filter_session, c_char_p, c_char_p, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_destination.restype = _gf_filter

_libgpac.gf_fs_load_filter.argtypes = [_gf_filter_session, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_filter.restype = _gf_filter

_libgpac.gf_fs_new_filter.argtypes = [_gf_filter_session, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_new_filter.restype = _gf_filter


_libgpac.gf_fs_post_user_task.argtypes = [_gf_filter_session, c_void_p, py_object, c_char_p]
_libgpac.gf_fs_post_user_task.restype = c_int

_libgpac.gf_fs_is_last_task.argtypes = [_gf_filter_session]
_libgpac.gf_fs_is_last_task.restype = c_bool

_libgpac.gf_fs_get_filters_count.argtypes = [_gf_filter_session]
_libgpac.gf_fs_get_filter.argtypes = [_gf_filter_session, c_int]
_libgpac.gf_fs_get_filter.restype = _gf_filter

_libgpac.gf_fs_abort.argtypes = [_gf_filter_session, c_bool]

_libgpac.gf_fs_get_http_max_rate.argtypes = [_gf_filter_session]
_libgpac.gf_fs_get_http_rate.argtypes = [_gf_filter_session]
_libgpac.gf_fs_set_http_max_rate.argtypes = [_gf_filter_session, c_uint]

_libgpac.gf_fs_lock_filters.argtypes = [_gf_filter_session, c_bool]
_libgpac.gf_fs_enable_reporting.argtypes = [_gf_filter_session, c_bool]

_libgpac.gf_fs_print_stats.argtypes = [_gf_filter_session]
_libgpac.gf_fs_print_connections.argtypes = [_gf_filter_session]

_libgpac.gf_fs_fire_event.argtypes = [_gf_filter_session, _gf_filter, POINTER(FilterEvent), c_bool]
_libgpac.gf_fs_fire_event.restype = c_bool

_libgpac.gf_fs_is_supported_mime.argtypes = [_gf_filter_session, c_char_p]
_libgpac.gf_fs_is_supported_mime.restype = c_bool

_libgpac.gf_fs_is_supported_mime.argtypes = [_gf_filter_session, c_char_p]
_libgpac.gf_fs_is_supported_mime.restype = c_bool

_libgpac.gf_fs_is_supported_source.argtypes = [_gf_filter_session, c_char_p]
_libgpac.gf_fs_is_supported_source.restype = c_bool


@CFUNCTYPE(c_bool, _gf_filter_session, c_void_p, POINTER(c_uint))
def fs_task_fun(sess, cbk, resched):
 obj = cast(cbk, py_object).value
 res = obj.execute()
 if res==None or res<0:
    obj.session.rem_task(obj)
    return False
 resched.contents.value=res
 return True

##\endcond


## Task object for user callbacks from libgpac scheduler 
class FilterTask:
    ##constructor for tasks
    #\param name name of the task (used for logging)
    def __init__(self, name):
        ##Task name
        #\hideinitializer
        self.name = name
        ##Filter session
        #\hideinitializer
        self.session = None
    ## task execution function
    #\return -1 or less to remove the task from the scheduler, 0 or more to indicate the number of milliseconds to wait before calling the task again
    def execute(self):
        return -1


## filter session object - see \ref GF_FilterSession
class FilterSession:
    ## constructor for  filter session - see \ref gf_fs_new
    #\param flags session flags (int)
    #\param blacklist list of blacklisted filters
    #\param nb_threads number of threads to use (int)
    #\param sched_type session scheduler type
    def __init__(self, flags=0, blacklist=None, nb_threads=0, sched_type=0):
        ##\cond private
        self._sess = None
        if blacklist:
            self._sess = _libgpac.gf_fs_new(nb_threads, sched_type, flags, blacklist.encode('utf-8'))
        else:
            self._sess = _libgpac.gf_fs_new(nb_threads, sched_type, flags, blacklist)
        if not self._sess:
            raise Exception('Failed to create new filter session')
        self._filters = []
        self._tasks = []
        ##\endcond
        #hack for doxygen to generate member vars (not support for parsing @property)
        if 0:
            ##set to true if this is the last task running, readonly - see \ref gf_fs_is_last_task
            self.last_task=0
            ##number of filters in session, readonly - see \ref gf_fs_get_filters_count
            self.nb_filters=0
            ##current HTTP cumulated download rate, readonly - see \ref gf_fs_get_http_rate
            self.http_bitrate=0
            ##HTTP max download rate - see \ref gf_fs_get_http_max_rate and \ref gf_fs_set_http_max_rate
            self.http_max_bitrate=0


    ## delete an existing filter session - see \ref gf_fs_del
    #\warning The filter session must be explicitly destroyed if close (\ref gf_sys_close) is called after that 
    #\return
    def delete(self):
        ##\cond private
        if self._sess:
            _libgpac.gf_fs_del(self._sess)
            self._sess=None
        ##\endcond private


    ##\cond private
    def _to_filter(self, f):
        for i, a_f in enumerate(self._filters):
            if a_f._filter == f:
                return a_f
        filter = Filter(self, f)
        self._filters.append(filter)
        return filter
    ##\endcond private

    ##run the session - see \ref gf_fs_run
    #\return
    def run(self):
        err = _libgpac.gf_fs_run(self._sess)
        if err<0: 
            raise Exception('Failed to run session: ' + e2s(err))

    ##step the session - see \ref gf_fs_run_step
    #\return
    def run_step(self):
        err = _libgpac.gf_fs_run_step(self._sess)
        if err<0: 
            raise Exception('Failed to run session: ' + e2s(err))

    ##load source filter - see \ref gf_fs_load_source
    #\param URL source URL to load
    #\param parentURL URL of parent resource for relative path resolution
    #\return new Filter object
    def load_src(self, URL, parentURL=None):
        errp = c_int(0)
        if parentURL:
            filter = _libgpac.gf_fs_load_source(self._sess, URL.encode('utf-8'), None, parentURL.encode('utf-8'), byref(errp))
        else:
            filter = _libgpac.gf_fs_load_source(self._sess, URL.encode('utf-8'), None, None, byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to load source filter ' + URL + ': ' + e2s(err))
        return self._to_filter(filter)

    ##load destination filter - see \ref gf_fs_load_destination
    #\param URL source URL to load
    #\param parentURL URL of parent resource for relative path resolution
    #\return new Filter object
    def load_dst(self, URL, parentURL=None):
        errp = c_int(0)
        if parentURL:
            filter = _libgpac.gf_fs_load_destination(self._sess, URL.encode('utf-8'), None, parentURL.encode('utf-8'), byref(errp))
        else:
            filter = _libgpac.gf_fs_load_destination(self._sess, URL.encode('utf-8'), None, None, byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to load destination filter ' + URL + ': ' + e2s(err))
        return self._to_filter(filter)


    ##load a filter - see \ref gf_fs_load_filter
    #\param fname filter name and options
    #\return new Filter object
    def load(self, fname):
        errp = c_int(0)
        filter = _libgpac.gf_fs_load_filter(self._sess, fname.encode('utf-8'), byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to load filter ' + fname + ': ' + e2s(err))
        return self._to_filter(filter)

    ##post a user task to the filter sesison - see \ref gf_fs_post_user_task
    #\param task task object to post
    #\return
    def post(self, task):
        task.session = self
        err = _libgpac.gf_fs_post_user_task(self._sess, fs_task_fun, py_object(task), task.name.encode('utf-8'))
        if err<0: 
            raise Exception('Failed to post task ' + task.name + ': ' + e2s(err))
        self._tasks.append(task)

    ##\cond private
    #\return
    def rem_task(self, task):
        task.session = None
        self._tasks.remove(task)
    ##\endcond

    ##abort the session - see \ref gf_fs_abort
    #\param flush flush pipeline before abort
    #\return
    def abort(self, flush=False):
        _libgpac.gf_fs_abort(self._sess, flush)
        
    ##get a filter by index - see \ref gf_fs_get_filter
    #\param index index of filter
    #\return Filter object
    def get_filter(self, index):
        f = _libgpac.gf_fs_get_filter(self._sess, index)
        if not f:
            raise Exception('Failed to get filter at index ' + str(index) )
        return self._to_filter(f)

        
    ##lock the session - see \ref gf_fs_lock_filters
    #\param lock if True, locks otherwise unlocks
    #\return
    def lock(self, lock):
        _libgpac.gf_fs_lock_filters(self._sess, lock)

    ##enable status reporting by filters - see \ref gf_fs_enable_reporting
    #\param do_report if True, enables reporting
    #\return
    def reporting(self, do_report):
        _libgpac.gf_fs_enable_reporting(self._sess, do_report)

    ##print statistics on stderr - see \ref gf_fs_print_stats
    #\return
    def print_stats(self):
        _libgpac.gf_fs_print_stats(self._sess)

    ##print graph on stderr - see \ref gf_fs_print_connections
    #\return
    def print_graph(self):
        _libgpac.gf_fs_print_connections(self._sess)

    ##fire an event on the given filter if any, or on any filter accepting user events
    # \param evt FilterEvent to fire
    # \param filter Filter to use as target
    # \param upstream if true, walks the chain towards the sink, otehrwise towards the source
    #\return
    def fire_event(self, evt, filter=None, upstream=False):
        if filter:
            return _libgpac.gf_fs_fire_event(self._sess, filter._filter, byref(evt), upstream)
        return _libgpac.gf_fs_fire_event(self._sess, None, byref(evt), upstream)

    ##checks if a given mime is supported - see \ref gf_fs_is_supported_mime
    # \param mime mime type to check
    # \return true or false
    def is_supported_mime(self, mime):
        return _libgpac.gf_fs_is_supported_mime(self._sess, mime.encode('utf-8'))

    ##checks if a given source URL is supported - see \ref gf_fs_is_supported_source
    # \param url URL to check
    # \return true or false
    def is_supported_source(self, url):
        return _libgpac.gf_fs_is_supported_source(self._sess, url.encode('utf-8'))


    ##\cond private: until end, properties
    @property
    def last_task(self):
        if _libgpac.gf_fs_is_last_task(self._sess):
            return True
        return False

    @property
    def nb_filters(self):
        return _libgpac.gf_fs_get_filters_count(self._sess)

    @property
    def http_bitrate(self):
        return _libgpac.gf_fs_get_http_rate(self._sess)
    
    @property
    def http_max_bitrate(self):
        return _libgpac.gf_fs_get_http_max_rate(self._sess)

    @http_max_bitrate.setter
    def http_max_bitrate(self, value):
        return _libgpac.gf_fs_set_http_max_rate(self._sess, value)

    ##\endcond


    #TODO
    #global session: session caps ?

    #missing features available in JSFilterSession API but with no native direct equivalents:
    #set_new_filter_fun, set_del_filter_fun, set_event_fun



## \cond private
#
#        filter bindings
#
_libgpac.gf_filter_get_name.argtypes = [_gf_filter]
_libgpac.gf_filter_get_name.restype = c_char_p
_libgpac.gf_filter_get_id.argtypes = [_gf_filter]
_libgpac.gf_filter_get_id.restype = c_char_p
_libgpac.gf_filter_get_ipid_count.argtypes = [_gf_filter]
_libgpac.gf_filter_get_opid_count.argtypes = [_gf_filter]



_libgpac.gf_filter_get_stats.argtypes = [_gf_filter, POINTER(FilterStats)]
_libgpac.gf_filter_remove.argtypes = [_gf_filter]
_libgpac.gf_fs_send_update.argtypes = [_gf_filter_session, c_char_p, _gf_filter, c_char_p, c_char_p, c_uint]
_libgpac.gf_filter_set_source.argtypes = [_gf_filter, _gf_filter, c_char_p]
_libgpac.gf_filter_reconnect_output.argtypes = [_gf_filter]

_libgpac.gf_props_get_id.argtypes = [c_char_p]
_libgpac.gf_filter_get_ipid.argtypes = [_gf_filter, c_uint]
_libgpac.gf_filter_get_ipid.restype = _gf_filter_pid
_libgpac.gf_filter_get_opid.argtypes = [_gf_filter, c_uint]
_libgpac.gf_filter_get_opid.restype = _gf_filter_pid
_libgpac.gf_filter_pid_get_property.argtypes = [_gf_filter_pid, c_uint]
_libgpac.gf_filter_pid_get_property.restype = POINTER(PropertyValue)
_libgpac.gf_filter_pid_get_property_str.argtypes = [_gf_filter_pid, c_char_p]
_libgpac.gf_filter_pid_get_property_str.restype = POINTER(PropertyValue)

_libgpac.gf_filter_pid_get_info.argtypes = [_gf_filter_pid, c_uint, POINTER(POINTER(_gf_property_entry))]
_libgpac.gf_filter_pid_get_info.restype = POINTER(PropertyValue)
_libgpac.gf_filter_pid_get_info_str.argtypes = [_gf_filter_pid, c_char_p, POINTER(POINTER(_gf_property_entry))]
_libgpac.gf_filter_pid_get_info_str.restype = POINTER(PropertyValue)
_libgpac.gf_filter_release_property.argtypes = [POINTER(_gf_property_entry)]


_libgpac.gf_pixel_fmt_name.argtypes = [c_uint]
_libgpac.gf_pixel_fmt_name.restype = c_char_p

_libgpac.gf_audio_fmt_name.argtypes = [c_uint]
_libgpac.gf_audio_fmt_name.restype = c_char_p

_libgpac.gf_filter_pid_enum_properties.argtypes = [_gf_filter_pid, POINTER(c_uint), POINTER(c_uint), POINTER(c_char_p)]
_libgpac.gf_filter_pid_enum_properties.restype = POINTER(PropertyValue)

_libgpac.gf_props_4cc_get_name.argtypes = [c_uint]
_libgpac.gf_props_4cc_get_name.restype = c_char_p

_libgpac.gf_stream_type_name.argtypes = [c_uint]
_libgpac.gf_stream_type_name.restype = c_char_p

_libgpac.gf_codecid_file_ext.argtypes = [c_uint]
_libgpac.gf_codecid_file_ext.restype = c_char_p

_libgpac.gf_filter_pid_get_source_filter.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_source_filter.restype = _gf_filter

_libgpac.gf_filter_pid_enum_destinations.argtypes = [_gf_filter_pid, c_uint]
_libgpac.gf_filter_pid_enum_destinations.restype = _gf_filter

_libgpac.gf_filter_enumerate_args.argtypes = [_gf_filter, c_uint]
_libgpac.gf_filter_enumerate_args.restype = POINTER(FilterArg)

_libgpac.gf_filter_get_info.argtypes = [_gf_filter, c_uint, POINTER(POINTER(_gf_property_entry))]
_libgpac.gf_filter_get_info.restype = POINTER(PropertyValue)
_libgpac.gf_filter_get_info_str.argtypes = [_gf_filter, c_char_p, POINTER(POINTER(_gf_property_entry))]
_libgpac.gf_filter_get_info_str.restype = POINTER(PropertyValue)


def _prop_to_python(pname, prop):
    type = prop.type
    if type==GF_PROP_SINT:
        return prop.value.sint
    if type==GF_PROP_UINT:
        if pname=="StreamType":
            return _libgpac.gf_stream_type_name(prop.value.uint).decode('utf-8')
        if pname=="CodecID":
            cid = _libgpac.gf_codecid_file_ext(prop.value.uint).decode('utf-8')
            names=cid.split('|')
            return names[0]
        if pname=="PixelFormat":
            return _libgpac.gf_pixel_fmt_name(prop.value.uint).decode('utf-8')
        if pname=="AudioFormat":
            return _libgpac.gf_audio_fmt_name(prop.value.uint).decode('utf-8')
        return prop.value.uint
    if type==GF_PROP_LSINT:
        return prop.value.longsint
    if type==GF_PROP_LUINT:
        return prop.value.longuint
    if type==GF_PROP_BOOL:
        return prop.value.boolean
    if type==GF_PROP_FRACTION:
        return prop.value.frac
    if type==GF_PROP_FRACTION64:
        return prop.value.lfrac
    if type==GF_PROP_FLOAT:
        return prop.value.fnumber
    if type==GF_PROP_DOUBLE:
        return prop.value.number
    if type==GF_PROP_VEC2I:
        return prop.value.vec2i
    if type==GF_PROP_VEC2:
        return prop.value.vec2
    if type==GF_PROP_VEC3I:
        return prop.value.vec3i
    if type==GF_PROP_VEC3:
        return prop.value.vec3
    if type==GF_PROP_VEC4I:
        return prop.value.vec4i
    if type==GF_PROP_VEC4:
        return prop.value.vec4
    if type==GF_PROP_PIXFMT:
        pname = _libgpac.gf_pixel_fmt_name(prop.value.uint)
        return pname.decode('utf-8')
    if type==GF_PROP_PCMFMT:
        pname = _libgpac.gf_audio_fmt_name(prop.value.uint)
        return pname.decode('utf-8')
    if type==GF_PROP_STRING or type==GF_PROP_STRING_NO_COPY or type==GF_PROP_NAME:
        return prop.value.string.decode('utf-8')
    if type==GF_PROP_DATA or type==GF_PROP_DATA_NO_COPY or type==GF_PROP_CONST_DATA:
        return prop.value.data
    if type==GF_PROP_POINTER:
        return prop.value.ptr
    if type==GF_PROP_STRING_LIST:
        res = [];
        for i in range(prop.value.string_list.nb_items):
            val = prop.value.string_list.vals[i].decode('utf-8')
            res.append(val)
        return res
    if type==GF_PROP_UINT_LIST:
        res = [];
        for i in range(prop.value.uint_list.nb_items):
            val = prop.value.uint_list.vals[i]
            res.append(val)
        return res
    if type==GF_PROP_SINT_LIST:
        res = [];
        for i in range(prop.value.uint_list.nb_items):
            val = prop.value.sint_list.vals[i]
            res.append(val)
        return res
    if type==GF_PROP_VEC2I_LIST:
        res = [];
        for i in range(prop.value.v2i_list.nb_items):
            val = prop.value.v2i_list.vals[i]
            res.append(val)
        return res

    raise Exception('Unknown property type ' + str(type))

## \endcond


## filter object
class Filter:
    ## \cond  private
    def __init__(self, session, filter):
        self._filter = filter
        self._session = session
    ## \endcond
        #hack for doxygen to generate member vars (not support for parsing @property)
        if 0:
            ##name of the filter, readonly - see \ref gf_filter_get_name
            #\hideinitializer
            self.name=0
            ##ID of the filter, readonly - see \ref gf_filter_get_id
            #\hideinitializer
            self.ID=0
            ##number of input pids for that filter, readonly - see \ref gf_filter_get_ipid_count
            #\hideinitializer
            self.nb_ipid=0
            ##number of output pids for that filter, readonly - see \ref gf_filter_get_opid_count
            #\hideinitializer
            self.nb_opid=0


    ## \cond  private
    def __str__(self):
        return self.name
    ## \endcond

    ## remove this filter - see \ref gf_filter_remove
    #\return
    def remove(self):
        _libgpac.gf_filter_remove(self._filter)

    ## send option update to this filter - see \ref gf_fs_send_update
    #\param name name of option (string)
    #\param value value of option (string)
    #\return
    def update(self, name, value):
        _libgpac.gf_fs_send_update(None, None, self._filter, name, value, 0)

    ## set a given filter as source for this filter - see \ref gf_filter_set_source
    #\param f source Filter
    #\param link_args link options (string)
    #\return
    def set_source(self, f, link_args=None):
        if f:
            _libgpac.gf_filter_set_source(self._filter, f._filter, link_args)

    ## insert a given filter after this filter - see \ref gf_filter_set_source and \ref gf_filter_reconnect_output
    #\param f  Filter to insert
    #\param link_args link options (string)
    #\return
    def insert(self, f, link_args=None):
        if f:
            _libgpac.gf_filter_set_source(f._filter, self._filter, link_args)
            _libgpac.gf_filter_reconnect_output(f._filter)

    ## \cond private
    def _pid_prop_ex(self, prop_name, pid, IsInfo):
        _name = prop_name.encode('utf-8')
        p4cc = _libgpac.gf_props_get_id(_name)
        if IsInfo:
            pe = POINTER(_gf_property_entry)()
            if p4cc:
                prop = _libgpac.gf_filter_pid_get_info(pid, p4cc, byref(pe))
            else:
                prop = _libgpac.gf_filter_pid_get_info_str(pid, _name, byref(pe))
            if prop:
                res = _prop_to_python(prop_name, prop.contents)
                _libgpac.gf_filter_release_property(pe)
                return res
            return None

        if p4cc:
            prop = _libgpac.gf_filter_pid_get_property(pid, p4cc)
        else:
            prop = _libgpac.gf_filter_pid_get_property_str(pid, _name)
        if prop:
            return _prop_to_python(prop_name, prop.contents)
        return None;

    def _pid_prop(self, idx, prop_name, IsInput):
        if IsInput:
            pid = _libgpac.gf_filter_get_ipid(self._filter, idx)
        else:
            pid = _libgpac.gf_filter_get_opid(self._filter, idx)
        if not pid:
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        return self._pid_prop_ex(self, prop_name, pid, False)

    def _pid_enum_props_ex(self, callback_obj, _pid):
        if hasattr(callback_obj, 'on_prop_enum')==False:
            raise Exception('No on_prop_enum function on callback')

        pidx=c_uint(0)
        while True:
            _p4cc=c_uint(0)
            _pname=c_char_p(0)
            prop = _libgpac.gf_filter_pid_enum_properties(_pid, byref(pidx), byref(_p4cc), byref(_pname))
            if prop:
                p4cc=_p4cc.value
                pname=None
                if _pname:
                    pname=_pname.value.decode('utf-8')
                else:
                    pname=_libgpac.gf_props_4cc_get_name(p4cc).decode('utf-8')
                pval = _prop_to_python(pname, prop.contents)
                callback_obj.on_prop_enum(pname, pval)
            else:
                break

    def _pid_enum_props(self, idx, callback_obj, IsInput):
        if IsInput:
            pid = _libgpac.gf_filter_get_ipid(self._filter, idx)
        else:
            pid = _libgpac.gf_filter_get_opid(self._filter, idx)

        if not pid:
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )

        self._pid_enum_props_ex(callback_obj, pid)

    ## \endcond    

    ##get an input pid property by name
    #\param idx index of input pid
    #\param prop_name name of property
    #\return property value or None if not found
    def ipid_prop(self, idx, prop_name):
        return self._pid_prop(idx, prop_name, True)
    ##enumerate an input pid properties
    #\param idx index of input pid
    #\param callback_obj callback object to use, must have a 'on_prop_enum' method defined taking two parameters, prop_name(string) and propval(property value)
    #\return
    def ipid_enum_props(self, idx, callback_obj):
        self._pid_enum_props(idx, callback_obj, True)
    ##get an output pid property by name
    #\param idx index of output pid
    #\param prop_name name of property
    #\return property value or None if not found
    def opid_prop(self, idx, prop_name):
        return self._pid_prop(idx, prop_name, False)
    ##enumerate an output pid properties
    #\param idx index of output pid
    #\param callback_obj callback object to use, must have a 'on_prop_enum' method defined taking two parameters, prop_name(string) and propval(property value)
    #\return
    def opid_enum_props(self, idx, callback_obj):
        self._pid_enum_props(idx, callback_obj, False)

    ##gets the filter at the source of an input pid
    #\param idx index of input PID
    #\return Filter or None of error
    def ipid_source(self, idx):
        pid = _libgpac.gf_filter_get_ipid(self._filter, idx)
        if not pid:
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        f = _libgpac.gf_filter_pid_get_source_filter(pid)
        if not f:
            return None
        return self._session._to_filter(f)

    ##gets the list of destination filters of an output pid
    #\param idx index of output PID
    #\return list of Filter
    def opid_sinks(self, idx):
        pid = _libgpac.gf_filter_get_opid(self._filter, idx)
        if not pid:
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        res=[]
        f_idx=0
        while True:
            f = _libgpac.gf_filter_pid_enum_destinations(pid, f_idx)
            f_idx+=1        
            if f==None:
                break
            filter = self._session._to_filter(f)
            res.append(filter)
        return res

    ##gets all defined options / arguments for a filter
    #\return list of FilterArg
    def all_args(self):
        res=[]
        a_idx=0
        while True:
            arg = _libgpac.gf_filter_enumerate_args(self._filter, a_idx)
            if arg:
                arg_val = arg.contents
                res.append(arg_val)
            else:
               break
            a_idx+=1
        return res

    ##gets a property info on a filter - see \ref gf_filter_get_info and \ref gf_filter_get_info_str
    #\param prop_name property to query
    #\return property value or None if not found
    def get_info(self, prop_name):
        _name = prop_name.encode('utf-8')
        p4cc = _libgpac.gf_props_get_id(_name)
        pe = POINTER(_gf_property_entry)()
        if p4cc:
            prop = _libgpac.gf_filter_get_info(self._filter, p4cc, byref(pe))
        else:
            prop = _libgpac.gf_filter_get_info_str(self._filter, _name, byref(pe))
        if prop:
            res = _prop_to_python(prop_name, prop.contents)
            _libgpac.gf_filter_release_property(pe)
            return res
        return None

    ##returns the statistics of a filter - see \ref gf_filter_get_stats
    #\return GF_FilterStatistics object
    def get_statistics(self):
        stats = FilterStats()
        err = _libgpac.gf_filter_get_stats(self._filter, byref(stats))
        if err<0: 
            raise Exception('Failed to fetch filter stats: ' + e2s(err))
        return stats

    ##\cond private: until end, properties

    @property
    def name(self):
        return _libgpac.gf_filter_get_name(self._filter).decode('utf-8')

    @property
    def ID(self):
        return _libgpac.gf_filter_get_id(self._filter).decode('utf-8')

    @property
    def nb_ipid(self):
        return _libgpac.gf_filter_get_ipid_count(self._filter)

    @property
    def nb_opid(self):
        return _libgpac.gf_filter_get_opid_count(self._filter)

    ##\endcond 



##\cond private

#
#        custom filter bindings
#

_libgpac.gf_filter_push_caps.argtypes = [_gf_filter, c_uint, POINTER(PropertyValue), c_char_p, c_uint, c_uint]
_libgpac.gf_props_4cc_get_type.argtypes = [c_uint]
_libgpac.gf_stream_type_by_name.argtypes = [c_char_p]
_libgpac.gf_stream_type_by_name.restype = c_uint

_libgpac.gf_codecid_parse.argtypes = [c_char_p]
_libgpac.gf_codecid_parse.restype = c_uint

_libgpac.gf_pixel_fmt_parse.argtypes = [c_char_p]
_libgpac.gf_pixel_fmt_parse.restype = c_uint

_libgpac.gf_audio_fmt_parse.argtypes = [c_char_p]
_libgpac.gf_audio_fmt_parse.restype = c_uint

def _make_prop(prop4cc, propname, prop, custom_type=0):
    prop_val = PropertyValue()
    if prop4cc==0:
        if not custom_type:
            prop_val.type = GF_PROP_STRING
            prop_val.value.string = str(prop).encode('utf-8')
            return prop_val
        type = custom_type
    else:
        type = _libgpac.gf_props_4cc_get_type(prop4cc)

    prop_val.type = type
    if propname=="StreamType":
        prop_val.value.uint = _libgpac.gf_stream_type_by_name(prop.encode('utf-8'))
        return prop_val
    elif propname=="CodecID":
        prop_val.value.uint = _libgpac.gf_codecid_parse(prop.encode('utf-8'))
        return prop_val
    elif propname=="PixelFormat":
        prop_val.value.uint = _libgpac.gf_pixel_fmt_parse(prop.encode('utf-8'))
        return prop_val
    elif propname=="AudioFormat":
        prop_val.value.uint = _libgpac.gf_audio_fmt_parse(prop.encode('utf-8'))
        return prop_val


    if type==GF_PROP_SINT:
        prop_val.value.sint = prop
    elif type==GF_PROP_UINT:
        prop_val.value.uint = prop
    elif type==GF_PROP_LSINT:
        prop_val.value.longsint = prop
    elif type==GF_PROP_LUINT:
        prop_val.value.longuint = prop
    elif type==GF_PROP_BOOL:
        prop_val.value.boolean = prop
    elif type==GF_PROP_FRACTION:
        if hasattr(prop, 'den') and hasattr(prop, 'num'):
            prop_val.value.frac.num = prop.num
            prop_val.value.frac.den = prop.den
        elif is_integer(prop):
            prop_val.value.frac.num = prop
            prop_val.value.frac.den = 1
        else:
            prop_val.value.frac.num = 1000*prop
            prop_val.value.frac.den = 1000
    elif type==GF_PROP_FRACTION64:
        if hasattr(prop, 'den') and hasattr(prop, 'num'):
            prop_val.value.lfrac.num = prop.num
            prop_val.value.lfrac.den = prop.den
        elif is_integer(prop):
            prop_val.value.lfrac.num = prop
            prop_val.value.lfrac.den = 1
        else:
            prop_val.value.lfrac.num = 1000000*prop
            prop_val.value.lfrac.den = 1000000
    elif type==GF_PROP_FLOAT:
        prop_val.value.fnumber = prop
    elif type==GF_PROP_DOUBLE:
        prop_val.value.number = prop
    elif type==GF_PROP_VEC2I:
        if hasattr(prop, 'x') and hasattr(prop, 'y'):
            prop_val.value.vec2i.x = prop.x
            prop_val.value.vec2i.y = prop.y
        else:
            raise Exception('Invalid property value for vec2i: ' + str(prop))
    elif type==GF_PROP_VEC2:
        if hasattr(prop, 'x') and hasattr(prop, 'y'):
            prop_val.value.vec2.x = prop.x
            prop_val.value.vec2.y = prop.y
        else:
            raise Exception('Invalid property value for vec2: ' + str(prop))
    elif type==GF_PROP_VEC3I:
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z'):
            prop_val.value.vec3i.x = prop.x
            prop_val.value.vec3i.y = prop.y
            prop_val.value.vec3i.z = prop.z
        else:
            raise Exception('Invalid property value for vec3i: ' + str(prop))
    elif type==GF_PROP_VEC3:
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z'):
            prop_val.value.vec3i.x = prop.x
            prop_val.value.vec3i.y = prop.y
            prop_val.value.vec3i.z = prop.z
        else:
            raise Exception('Invalid property value for vec3: ' + str(prop))
    elif type==GF_PROP_VEC4I:
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z') and hasattr(prop, 'w'):
            prop_val.value.vec4i.x = prop.x
            prop_val.value.vec4i.y = prop.y
            prop_val.value.vec4i.z = prop.z
            prop_val.value.vec4i.w = prop.w
        else:
            raise Exception('Invalid property value for vec4i: ' + str(prop))
    elif type==GF_PROP_VEC4:
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z') and hasattr(prop, 'w'):
            prop_val.value.vec4.x = prop.x
            prop_val.value.vec4.y = prop.y
            prop_val.value.vec4.z = prop.z
            prop_val.value.vec4.w = prop.w
        else:
            raise Exception('Invalid property value for vec4: ' + str(prop))
    elif type==GF_PROP_STRING or type==GF_PROP_STRING_NO_COPY or type==GF_PROP_NAME:
        prop_val.value.string = str(prop).encode('utf-8')
    elif type==GF_PROP_DATA or type==GF_PROP_DATA_NO_COPY or type==GF_PROP_CONST_DATA:
        raise Exception('Setting data property from python not yet implemented !')
    elif type==GF_PROP_POINTER:
        raise Exception('Setting pointer property from python not yet implemented !')
    elif type==GF_PROP_STRING_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.string_list.nb_items = len(prop)
        prop_val.value.string_list.vals = (ctypes.c_char_p * len(prop))
        i=0
        for str in list:
            prop_val.value.string_list.vals[i] = str.encode('utf-8')
            i+=1
    elif type==GF_PROP_UINT_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.uint_list.nb_items = len(prop)
        prop_val.value.uint_list.vals = (ctypes.c_uint * len(prop))(*prop)
    elif type==GF_PROP_SINT_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.sint_list.nb_items = len(prop)
        prop_val.value.sint_list.vals = (ctypes.c_int * len(prop))(*prop)
    elif type==GF_PROP_VEC2I_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.sint_list.nb_items = len(prop)
        prop_val.value.v2i_list.vals = (GF_PropVec2i * len(prop))
        for i in range (len(prop)):
            prop_val.value.v2i_list.vals[i].x = prop[i].x
            prop_val.value.v2i_list.vals[i].y = prop[i].y
    else:
        raise Exception('Unsupported property type ' + str(type) )

    return prop_val


_libgpac.gf_filter_set_rt_udta.argtypes = [_gf_filter, py_object]
_libgpac.gf_filter_get_rt_udta.argtypes = [_gf_filter]
_libgpac.gf_filter_get_rt_udta.restype = c_void_p

_libgpac.gf_filter_pid_set_udta.argtypes = [_gf_filter, py_object]
_libgpac.gf_filter_pid_get_udta.argtypes = [_gf_filter]
_libgpac.gf_filter_pid_get_udta.restype = c_void_p

_libgpac.gf_filter_pid_send_event.argtypes = [_gf_filter_pid, POINTER(FilterEvent)]


_libgpac.gf_filter_set_configure_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter, _gf_filter_pid, c_bool)
def filter_cbk_configure(_f, _pid, is_remove):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    obj = _libgpac.gf_filter_pid_get_udta(_pid)
    if obj:
        pid_obj = cast(obj, py_object).value
    else:
        pid_obj=None

    first=True
    if pid_obj:
        first=False
    else:
        pid_obj = FilterPid(filter, _pid, True)
        _libgpac.gf_filter_pid_set_udta(_pid, py_object(pid_obj))
    res = filter.configure_pid(pid_obj, is_remove);

    if is_remove:
        _libgpac.gf_filter_pid_set_udta(_pid, None)
        filter.ipids.remove(pid_obj)
        return

    if first:
        filter.ipids.append(pid_obj)

    return res


_libgpac.gf_filter_set_process_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter)
def filter_cbk_process(_f):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    return filter.process();

_libgpac.gf_filter_set_process_event_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_bool, _gf_filter, POINTER(FilterEvent) )
def filter_cbk_process_event(_f, _evt):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    return filter.process_event(_evt.contents);

_libgpac.gf_filter_set_probe_data_cbk.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, c_char_p, c_uint, POINTER(c_uint) )
def filter_cbk_probe_data(_data, _size, _probe):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    if numpy_support:
        ar_data = np.ctypeslib.as_array(_data, (_size,))
        ar_data.flags.writeable=False
        res = filter.probe_data(ar_data, _size);
    else:
        res = filter.probe_data(_data, _size);
    if res==None:
        _probe.contents=0
        return None
    _probe.contents=1; #GF_FPROBE_MAYBE_SUPPORTED
    return res.encode('utf-8')


_libgpac.gf_filter_set_reconfigure_output_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter, _gf_filter_pid )
def filter_cbk_reconfigure_output(_f, _pid):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    obj = _libgpac.gf_filter_pid_get_udta(_pid)
    if obj:
        pid_obj = cast(obj, py_object).value
    else:
        pid_obj=None
    if pid_obj:
        return filter.reconfigure_output(_f);
    raise Exception('Reconfigure on unknown output pid !')


_libgpac.gf_filter_pid_get_packet.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_packet.restype = _gf_filter_packet

_libgpac.gf_filter_pid_drop_packet.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_new.argtypes = [_gf_filter]
_libgpac.gf_filter_pid_new.restype = _gf_filter_pid

_libgpac.gf_filter_update_status.argtypes = [_gf_filter, c_uint, c_char_p]

_libgpac.gf_filter_ask_rt_reschedule.argtypes = [_gf_filter]
_libgpac.gf_filter_post_process_task.argtypes = [_gf_filter]


_libgpac.gf_filter_ask_rt_reschedule.argtypes = [_gf_filter, c_int, c_bool]
_libgpac.gf_filter_setup_failure.argtypes = [_gf_filter, c_int]
_libgpac.gf_filter_make_sticky.argtypes = [_gf_filter]
_libgpac.gf_filter_prevent_blocking.argtypes = [_gf_filter, c_bool]
_libgpac.gf_filter_block_eos.argtypes = [_gf_filter, c_bool]
_libgpac.gf_filter_set_max_extra_input_pids.argtypes = [_gf_filter, c_uint]
_libgpac.gf_filter_block_enabled.argtypes = [_gf_filter]
_libgpac.gf_filter_block_enabled.restype = c_bool
_libgpac.gf_filter_get_output_buffer_max.argtypes = [_gf_filter, POINTER(c_uint), POINTER(c_uint)]
_libgpac.gf_filter_all_sinks_done.argtypes = [_gf_filter]
_libgpac.gf_filter_all_sinks_done.restype = c_bool
_libgpac.gf_filter_get_num_events_queued.argtypes = [_gf_filter]

_libgpac.gf_filter_get_clock_hint.argtypes = [_gf_filter, POINTER(c_ulonglong), POINTER(Fraction64)]
_libgpac.gf_filter_connections_pending.argtypes = [_gf_filter]
_libgpac.gf_filter_connections_pending.restype = c_bool
_libgpac.gf_filter_hint_single_clock.argtypes = [_gf_filter, c_ulonglong, Fraction64]

##\endcond

##notification is a setup error, the filter chain was never connected
GF_SETUP_ERROR=0
##notification is an error but keep the filter chain connected
GF_NOTIF_ERROR=1
##notification is an error and disconnect the filter chain
GF_NOTIF_ERROR_AND_DISCONNECT=2

## Base class used to create custom filters in python
class FilterCustom(Filter):

    ##constructor
    #\param session FilterSession object
    #\param fname name of the filter
    def __init__(self, session, fname="Custom"):
        errp = c_int(0)
        _filter = _libgpac.gf_fs_new_filter(session._sess, fname.encode('utf-8'), byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to create filter ' + URL + ': ' + e2s(err))
        #create base class
        Filter.__init__(self, session, _filter)
        #register with our filter bank
        session._filters.append(self)

        #setup callback udta and callback functions
        _libgpac.gf_filter_set_rt_udta(_filter, py_object(self) )
        if hasattr(self, 'configure_pid'):
            _libgpac.gf_filter_set_configure_ckb(self._filter, filter_cbk_configure)

        if hasattr(self, 'process'):
            _libgpac.gf_filter_set_process_ckb(self._filter, filter_cbk_process)

        if hasattr(self, 'process_event'):
            _libgpac.gf_filter_set_process_event_ckb(self._filter, filter_cbk_process_event)

        if hasattr(self, 'probe_data'):
            _libgpac.gf_filter_set_probe_data_ckb(self._filter, filter_cbk_probe_data)

        if hasattr(self, 'reconfigure_output'):
            _libgpac.gf_filter_set_reconfigure_output_ckb(self._filter, filter_cbk_reconfigure_output)

        ##List of input FilterPid
        self.ipids=[]
        ##List of output FilterPid
        self.opids=[]

        #hack for doxygen to generate member vars (not support for parsing @property)
        if 0:
            ##filter blocking is enabled, readonly - see \ref gf_filter_block_enabled
            #\hideinitializer
            self.block_enabled=0
            ##maximum output buffer time, readonly - see \ref gf_filter_get_output_buffer_max
            #\hideinitializer
            self.output_buffer=0
            ##maximum plyaout buffer time, readonly - see \ref gf_filter_get_output_buffer_max
            #\hideinitializer
            self.playout_buffer=0
            ##all sinks are done for this filter, readonly - see \ref gf_filter_all_sinks_done
            #\hideinitializer
            self.sinks_done=0
            ##number of queued events on the filter, readonly - see \ref gf_filter_get_num_events_queued
            #\hideinitializer
            self.nb_evts_queued=0
            ##clock hint value in microseconds, readonly - see \ref gf_filter_get_clock_hint
            #\hideinitializer
            self.clock_hint_time=0
            ##clock hint media time as Fraction64, readonly - see \ref gf_filter_get_clock_hint
            #\hideinitializer
            self.clock_hint_mediatime=0
            ##boolean indicating connections are pending on the filter, readonly - see \ref gf_filter_connections_pending
            #\hideinitializer
            self.connections_pending=0



    ##push a capability in the current capability bundle - see \ref gf_filter_push_caps
    #\param pcode capability name (property type)
    #\param prop capability value
    #\param flag capability flags (input, output, etc)
    #\param priority capability priority
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def push_cap(self, pcode, prop, flag, priority=0, custom_type=0):
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        err = _libgpac.gf_filter_push_caps(self._filter, prop_4cc, byref(prop_val), prop_name, flag, priority)
        if err<0: 
            raise Exception('Failed to push cap: ' + e2s(err))

    ##create a new output pid for this filter - see \ref gf_filter_pid_new
    #\return FilterPid object
    def new_pid(self):
        _pid = _libgpac.gf_filter_pid_new(self._filter)
        if _pid:
            pid_obj = FilterPid(self, _pid, False)
            _libgpac.gf_filter_pid_set_udta(_pid, py_object(pid_obj))
            self.opids.append(pid_obj)
            pid_obj.pck_refs = []
            return pid_obj
        raise Exception('Failed to create output pid')

    ##update filter status - see \ref gf_filter_update_status
    #\param status status string
    #\param percent progress in per 10000, int
    #\return
    def update_status(self, status, percent):
        _libgpac.gf_filter_update_status(self._filter, percent, status.encode('utf-8'))

    ##reschedule the filter after a given delay - see \ref gf_filter_ask_rt_reschedule and \ref gf_filter_post_process_task
    #\param when delay in microseconds
    #\return
    def reschedule(self, when=0):
        if when:
            _libgpac.gf_filter_ask_rt_reschedule(self._filter, when)
        else:
            _libgpac.gf_filter_post_process_task(self._filter, when)

    ##notify an internal failure of the filter has happend - see \ref gf_filter_notification_failure and \ref gf_filter_setup_failure
    #\param err the failure reason (gpac error code, int)
    #\param error_type the failure notification type
    #\return
    def notify_failure(self, err, error_type=GF_SETUP_ERROR):
        if error_type==GF_NOTIF_ERROR:
            _libgpac.gf_filter_notification_failure(self._filter, err, False)
        elif error_type==GF_NOTIF_ERROR_AND_DISCONNECT:
            _libgpac.gf_filter_notification_failure(self._filter, err, True)
        else:
            _libgpac.gf_filter_setup_failure(self._filter, err)

    ##make the filter sticky - see \ref gf_filter_make_sticky
    #\return
    def make_sticky(self):
        _libgpac.gf_filter_make_sticky(self._filter)

    ##prevent blocking on the filter - see \ref gf_filter_prevent_blocking
    #\param enable if True, blocking prevention is enabled
    #\return
    def prevent_blocking(self, enable):
        _libgpac.gf_filter_prevent_blocking(self._filter, enable)

    ##block eos signaling on the filter - see \ref gf_filter_block_eos
    #\param enable if True, eos blocking is enabled
    #\return
    def block_eos(self, enable):
        _libgpac.gf_filter_block_eos(self._filter, enable)

    ##set maximum number of extra pids accepted by this filter - see \ref gf_filter_set_max_extra_input_pids
    #\param max_pids number of extra pids
    #\return
    def set_max_pids(self, max_pids):
        _libgpac.gf_filter_set_max_extra_input_pids(self._filter, max_pids)

    ##set clock hint - see \ref gf_filter_hint_single_clock
    #\param clock_us clock in microseconds
    #\param media_time media time as Fraction64
    #\return
    def hint_clock(self, clock_us, media_time):
        _libgpac.gf_filter_hint_single_clock(self._filter, clock_us, media_time)


    ##\cond private: until end, properties

    @property
    def block_enabled(self):
        return _libgpac.gf_filter_block_enabled(self._filter)

    @property
    def output_buffer(self):
        val = c_uint(0)
        _libgpac.gf_filter_get_output_buffer_max(self._filter, byref(val), None)
        return val.value

    @property
    def playout_buffer(self):
        val = c_uint(0)
        _libgpac.gf_filter_get_output_buffer_max(self._filter, None, byref(val))
        return val.value

    @property
    def sinks_done(self):
        return _libgpac.gf_filter_all_sinks_done(self._filter)

    @property
    def nb_evts_queued(self):
        return _libgpac.gf_filter_get_num_events_queued(self._filter)

    @property 
    def clock_hint_time(self):
        val = c_ulonglong(0)
        _libgpac.gf_filter_get_clock_hint(self._filter, byref(val), None)
        return val.value;

    @property 
    def clock_hint_mediatime(self):
        val = Fraction64
        _libgpac.gf_filter_get_clock_hint(self._filter, None, byref(val))
        return val.value;

    @property 
    def connections_pending(self):
        return _libgpac.gf_filter_connections_pending(self._filter)

    ##\endcond

    #todo
    #add_source, add_destination, add_filter: not needed, acces to filter session



#
#        filter PID bindings
#

## \cond private
_libgpac.gf_filter_pid_copy_properties.argtypes = [_gf_filter_pid, _gf_filter_pid]
_libgpac.gf_filter_pck_forward.argtypes = [_gf_filter_packet, _gf_filter_pid]

_libgpac.gf_filter_pid_set_property.argtypes = [_gf_filter_pid, c_uint, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_set_property_str.argtypes = [_gf_filter_pid, c_char_p, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_set_info.argtypes = [_gf_filter_pid, c_uint, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_set_info_str.argtypes = [_gf_filter_pid, c_char_p, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_clear_eos.argtypes = [_gf_filter_pid, c_bool]
_libgpac.gf_filter_pid_check_caps.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_check_caps.restype = c_bool
_libgpac.gf_filter_pid_discard_block.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_allow_direct_dispatch.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_reset_properties.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_clock_info.argtypes = [_gf_filter_pid, POINTER(c_longlong), POINTER(c_uint)]
_libgpac.gf_filter_pid_remove.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_filter_in_parents.argtypes = [_gf_filter_pid, _gf_filter]
_libgpac.gf_filter_pid_is_filter_in_parents.restype = c_bool

_libgpac.gf_filter_pid_get_name.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_name.restype = c_char_p
_libgpac.gf_filter_pid_set_name.argtypes = [_gf_filter_pid, c_char_p]

_libgpac.gf_filter_pid_is_eos.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_eos.restype = c_bool
_libgpac.gf_filter_pid_set_eos.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_has_seen_eos.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_has_seen_eos.restype = c_bool

_libgpac.gf_filter_pid_would_block.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_would_block.restype = c_bool
_libgpac.gf_filter_pid_set_loose_connect.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_set_framing_mode.argtypes = [_gf_filter_pid, c_bool]

_libgpac.gf_filter_pid_get_max_buffer.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_max_buffer.restype = c_uint
_libgpac.gf_filter_pid_set_max_buffer.argtypes = [_gf_filter_pid, c_uint]

_libgpac.gf_filter_pid_query_buffer_duration.argtypes = [_gf_filter_pid, c_bool]
_libgpac.gf_filter_pid_query_buffer_duration.restype = c_ulonglong

_libgpac.gf_filter_pid_first_packet_is_empty.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_first_packet_is_empty.restype = c_bool

_libgpac.gf_filter_pid_get_first_packet_cts.argtypes = [_gf_filter_pid, POINTER(c_ulonglong)]
_libgpac.gf_filter_pid_get_first_packet_cts.restype = c_bool

_libgpac.gf_filter_pid_get_packet_count.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_timescale.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_set_clock_mode.argtypes = [_gf_filter_pid, c_bool]
_libgpac.gf_filter_pid_set_discard.argtypes = [_gf_filter_pid, c_bool]

_libgpac.gf_filter_pid_require_source_id.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_recompute_dts.argtypes = [_gf_filter_pid, c_bool]


_libgpac.gf_filter_pid_get_min_pck_duration.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_playing.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_playing.restype = c_bool

_libgpac.gf_filter_pid_get_filter_name.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_playing.restype = c_char_p


_libgpac.gf_filter_pid_caps_query.argtypes = [_gf_filter_pid, c_uint]
_libgpac.gf_filter_pid_caps_query.restype = POINTER(PropertyValue)
_libgpac.gf_filter_pid_caps_query_str.argtypes = [_gf_filter_pid, c_char_p]
_libgpac.gf_filter_pid_caps_query_str.restype = POINTER(PropertyValue)

_libgpac.gf_filter_pid_negociate_property.argtypes = [_gf_filter_pid, c_uint, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_negociate_property_dyn.argtypes = [_gf_filter_pid, c_char_p, POINTER(PropertyValue)]

_libgpac.gf_filter_pck_new_ref.argtypes = [_gf_filter_pid, c_uint, c_uint, _gf_filter_packet]
_libgpac.gf_filter_pck_new_ref.restype = _gf_filter_packet

_libgpac.gf_filter_pck_new_alloc.argtypes = [_gf_filter_pid, c_uint, POINTER(c_void_p)]
_libgpac.gf_filter_pck_new_alloc.restype = _gf_filter_packet

_libgpac.gf_filter_pck_new_clone.argtypes = [_gf_filter_pid, _gf_filter_packet, POINTER(c_void_p)]
_libgpac.gf_filter_pck_new_clone.restype = _gf_filter_packet
_libgpac.gf_filter_pck_new_copy.argtypes = [_gf_filter_pid, _gf_filter_packet, POINTER(c_void_p)]
_libgpac.gf_filter_pck_new_copy.restype = _gf_filter_packet

## \endcond private

## Buffer occupancy object 
class BufferOccupancy:
  ##\cond private
  def __init__(self, max_units, nb_pck, max_dur, dur, is_final_flush):
  ##\endcond
    ##maximum number of packets (partial or full AU) allowed in buffer
    self.max_units = max_units
    ##number of block allowed in buffer
    self.nb_pck = nb_pck
    ##maximum buffer duration in microseconds
    self.max_dur = max_dur
    ## buffer duration in microseconds
    self.dur = dur
    ##if true, the session has been aborted and this is the final flush for this buffer
    self.is_final_flush = is_final_flush


## \cond private

#callback for packet destruction

_libgpac.gf_filter_pck_new_shared.argtypes = [_gf_filter_pid, POINTER(c_ubyte), c_uint, c_void_p]
_libgpac.gf_filter_pck_new_shared.restype = _gf_filter_packet

@CFUNCTYPE(c_int, _gf_filter, _gf_filter_pid, _gf_filter_packet)
def filter_cbk_release_packet(_f, _pid, _pck):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    pck_obj=None
    obj = _libgpac.gf_filter_pid_get_udta(_pid)
    if obj:
        pid_obj = cast(obj, py_object).value
    else:
        raise Exception('Unknwon PID on packet destruction callback')

    for pck in pid_obj.pck_refs:
        if pck._pck==_pck:
            pck_obj = pck
            break

    if not pck_obj:
        raise Exception('Unknwon packet on packet destruction callback')
    pid_obj.pck_refs.remove(pck_obj)
    filter.packet_release(pid_obj, pck_obj)
    return 0

## \endcond private

## Object representing a PID of a custom filter
class FilterPid:
    ## \cond private
    def __init__(self, filter, pid, IsInput):
        self._filter = filter
        self._pid = pid
        self._cur_pck = None
        self._input = IsInput
    ## \endcond
        #hack for doxygen to generate member vars (not support for parsing @property)
        if 0:
            ##name of the PID - see \ref gf_filter_pid_get_name and \ref gf_filter_pid_set_name
            #\hideinitializer
            self.name=0
            ##name of the parent filter, readonly - see \ref gf_filter_pid_get_filter_name
            #\hideinitializer
            self.filter_name=0
            ##end of stream property of PID  - see \ref gf_filter_pid_is_eos and \ref gf_filter_pid_set_eos
            #\hideinitializer
            self.eos=0
            ##True if end of stream was seen in the chain but not yet reached by the filter, readonly  - see \ref gf_filter_pid_has_seen_eos
            #\hideinitializer
            self.has_seen_eos=0
            ##True if PID would block, readonly - see \ref gf_filter_pid_would_block
            #\hideinitializer
            self.would_block=0
            ##maximum buffer of PID in microseconds - see \ref gf_filter_pid_get_max_buffer and \ref gf_filter_pid_set_max_buffer
            #\hideinitializer
            self.max_buffer=0
            ##buffer of PID in microseconds, readonly - see \ref gf_filter_pid_query_buffer_duration
            #\hideinitializer
            self.buffer=0
            ##True if buffer is full, readonly - see \ref gf_filter_pid_query_buffer_duration
            #\hideinitializer
            self.buffer_full=0
            ##True if no pending packet, readonly - see \ref gf_filter_pid_first_packet_is_empty
            #\hideinitializer
            self.first_empty=0
            ##value of CTS of first pending packet, None if none, readonly - see \ref gf_filter_pid_get_first_packet_cts
            #\hideinitializer
            self.first_cts=0
            ##number of queued packets for input pid, readonly - see \ref gf_filter_pid_get_packet_count
            #\hideinitializer
            self.nb_pck_queued=0
            ##timescale of pid, readonly - see \ref gf_filter_pid_get_timescale
            #\hideinitializer
            self.timescale=0
            ##minimum packet duration (in timescale) of pid, readonly - see \ref gf_filter_pid_get_min_pck_duration
            #\hideinitializer
            self.min_pck_dur=0
            ##True if PID is playing, readonly  - see \ref gf_filter_pid_is_playing
            #\hideinitializer
            self.playing=0



    ##send an event on the pid - see \ref gf_filter_pid_send_event
    #\param evt FilterEvent to send
    #\return
    def send_event(self, evt):
        evt.base.on_pid = self._pid
        _libgpac.gf_filter_pid_send_event(self._pid, byref(evt))

    ##removes this output pid - see \ref gf_filter_pid_remove
    #\return
    def remove(self):
        if self._input:
            raise Exception('Cannot remove input pid')
            return
        _libgpac.gf_filter_pid_remove(self._pid)

    ##enumerates property on pid
    #\param callback_obj callback object to use, must have a 'on_prop_enum' method defined taking two parameters, prop_name(string) and propval(property value)
    #\return
    def enum_props(self, callback_obj):
        self._filter._pid_enum_props_ex(callback_obj, self._pid)

    ##get a PID property
    #\param pname property name
    #\return property value or None if not found
    def get_prop(self, pname):
        return self._filter._pid_prop_ex(pname, self._pid, False)

    ##get a PID info
    #\param pname property name
    #\return property value or None if not found
    def get_info(self, pname):
        return self._filter._pid_prop_ex(pname, self._pid, True)

    ##get first packet of input PID - see gf_filter_pid_get_packet
    #\return FilterPacket or None if no packet available
    def get_packet(self):
        ##\cond private
        if self._cur_pck:
            return self._cur_pck
        pck = _libgpac.gf_filter_pid_get_packet(self._pid)
        if pck:
            self._cur_pck = FilterPacket(pck, True)
            return self._cur_pck
        ##\endcond

    ##drops (removes) the first packet of input PID - see \ref gf_filter_pid_drop_packet
    #\return
    def drop_packet(self):
        ##\cond private
        if self._cur_pck:
            self._cur_pck = None
            _libgpac.gf_filter_pid_drop_packet(self._pid)
        ##\endcond

    ##copy property of given PID to the current pid - see \ref gf_filter_pid_copy_properties
    #\param ipid FilterPid to copy from
    #\return
    def copy_props(self, ipid):
        if self._input:
            raise Exception('Cannot copy properties on input PID')
            return
        if ipid==None:
            return
        err = _libgpac.gf_filter_pid_copy_properties(self._pid, ipid._pid)
        if err<0:
            raise Exception('Cannot copy properties: ' + e2s(err) )
    
    ##removes all properties of the current pid - see \ref gf_filter_pid_reset_properties
    #\return
    def reset_props(self):
        if self._input:
            raise Exception('Cannot reset properties on input PID')
            return
        _libgpac.gf_filter_pid_reset_properties(self._pid)

    ##forward a packet on the current pid - see \ref gf_filter_pck_forward
    #\param ipck packet to forward
    #\return
    def forward(self, ipck):
        if self._input:
            raise Exception('Cannot copy properties on input PID')
            return
        if ipck==None:
            return
        _libgpac.gf_filter_pck_forward(ipck._pck, self._pid)

    ##set a property the current pid - see \ref gf_filter_pid_set_property and \ref gf_filter_pid_set_property_str
    #\param pcode property type
    #\param prop property value to set
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def set_prop(self, pcode, prop, custom_type=0):
        if self._input:
            raise Exception('Cannot set properties on input PID')
            return
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop==None:
            if prop_4cc:
                _libgpac.gf_filter_pid_set_property(self._pid, prop_4cc, None)
            else:
                _libgpac.gf_filter_pid_set_property_str(self._pid, prop_name, None)
            return

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pid_set_property(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_set_property_str(self._pid, prop_name, byref(prop_val))

    ##set a info property the current pid - see \ref gf_filter_pid_set_info and \ref gf_filter_pid_set_info_str
    #\param pcode property type
    #\param prop property value to set
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def set_info(self, pcode, prop, custom_type=0):
        if self._input:
            raise Exception('Cannot set info on input PID')
            return
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop==None:
            if prop_4cc:
                _libgpac.gf_filter_pid_set_info(self._pid, prop_4cc, None)
            else:
                _libgpac.gf_filter_pid_set_info_str(self._pid, prop_name, None)
            return

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pid_set_info(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_set_info_str(self._pid, prop_name, byref(prop_val))

    ##clears EOS on the current PID - see \ref gf_filter_pid_clear_eos
    #\param all_pids if True, clears eos on all input pids
    #\return
    def clear_eos(self, all_pids):
        _libgpac.gf_filter_pid_clear_eos(self._pid, all_pids)

    ##check PID properties match capability of filter - see \ref gf_filter_pid_check_caps
    #\return
    def check_caps(self):
        return _libgpac.gf_filter_pid_check_caps(self._pid)

    ##discard blocking mode on PID - see \ref gf_filter_pid_discard_block
    #\return
    def discard_block(self):
        _libgpac.gf_filter_pid_discard_block(self._pid)

    ##allow direct dispatch of output to destinations - see \ref gf_filter_pid_allow_direct_dispatch
    #\return
    def allow_direct_dispatch(self):
        _libgpac.gf_filter_pid_allow_direct_dispatch(self._pid)

    ##get current clock type info - see \ref gf_filter_pid_get_clock_info
    #\return clock type (int)
    def get_clock_type(self):
        return _libgpac.gf_filter_pid_get_clock_info(self._pid, None, None)

    ##get current clock time stamp - see \ref gf_filter_pid_get_clock_info
    #\return clock timestamp (Fraction64)
    def get_clock_timestamp(self):
        timestamp = c_longlong(0)
        timescale = c_uint(0)
        _libgpac.gf_filter_pid_get_clock_info(self._pid, byref(timestamp), byref(timescale))
        v = Fraction64();
        v.value.num = timestamp.value
        v.value.den = timescale.value
        return v

    ##check if a filter is in the parent filter chain of the pid - see \ref gf_filter_pid_is_filter_in_parents
    #\param filter Filter to check
    #\return True or False
    def is_filter_in_parents(self, filter):
        return _libgpac.gf_filter_pid_is_filter_in_parents(self._pid, filter._f);

    ##get buffer occupancy - see \ref gf_filter_pid_get_buffer_occupancy
    #\return BufferOccupancy object
    def get_buffer_occupancy(self):
        max_units = c_uint(0)
        nb_pck = c_uint(0)
        max_dur = c_uint(0)
        dur = c_uint(0)
        in_final_flush = _libgpac.gf_filter_pid_get_buffer_occupancy(self._pid, byref(max_units), byref(nb_pck), byref(max_dur), byref(dur) )
        in_final_flush = not in_final_flush
        return BufferOccupancy(max_units, nb_pck, max_dur, dur, not_in_final_flush)

    ##sets loose connect mode - see \ref gf_filter_pid_set_loose_connect
    #\return
    def loose_connect(self):
        _libgpac.gf_filter_pid_set_loose_connect(slef._pid)

    ##sets framing mode - see \ref gf_filter_pid_set_framing_mode
    #\param framed if True, complete frames only will be delivered on the pid
    #\return
    def set_framing(self, framed):
        _libgpac.gf_filter_pid_set_framing_mode(self._pid, framed)

    ##sets clock mode - see \ref gf_filter_pid_set_clock_mode
    #\param cmode clock mode operation of filter
    #\return
    def set_clock_mode(self, cmode):
        _libgpac.gf_filter_pid_set_clock_mode(self._pid, cmode)

    ##sets discard mode - see \ref gf_filter_pid_set_discard
    #\param do_discard if True, discard is on
    #\return
    def set_discard(self, do_discard):
        _libgpac.gf_filter_pid_set_discard(self._pid, do_discard)

    ##checks if sourceID is required - see \ref require_source_id
    #\return True if required
    def require_source_id(self):
        return _libgpac.gf_filter_pid_require_source_id(self._pid)

    ##sets DTS recomputing mode - see \ref gf_filter_pid_recompute_dts
    #\param do_compute if True, DTS are recomputed
    #\return
    def recompute_dts(self, do_compute):
        _libgpac.gf_filter_pid_recompute_dts(self._pid, do_compute)

    ##queries a capability property on output PID - see \ref gf_filter_pid_caps_query and \ref gf_filter_pid_caps_query_str
    #\param pcode property to check
    #\return property value if found, None otherwise
    def query_cap(self, pcode):
        if self._input:
            raise Exception('Cannot query caps on input PID')
            return
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop_4cc:
            prop_val = _libgpac.gf_filter_pid_caps_query(self._pid, prop_4cc)
        else:
            prop_val = _libgpac.gf_filter_pid_caps_query_str(self._pid, _pname)

        if prop_val:
            return _prop_to_python(pname, prop.contents)
        return None

    ##negociates a capability property on input PID - see \ref gf_filter_pid_negociate_property and \ref gf_filter_pid_negociate_property_dyn
    #\param pcode property to negotiate
    #\param prop property to negotiate
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def negociate_cap(self, pcode, prop, custom_type=0):
        if not self._input:
            raise Exception('Cannot negociate caps on output PID')
            return
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pid_negociate_property(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_negociate_property_dyn(self._pid, prop_name, byref(prop_val))

    ##resolves a template string - see \ref gf_filter_pid_resolve_file_template
    #\param template the template string
    #\param file_idx the file index
    #\param suffix the file suffix
    #\return the resolved template string
    def resolve_template(self, template, file_idx=0, suffix=None):
        res = create_string_buffer(2000)
        err = _libgpac.gf_filter_pid_resolve_file_template(self._pid, res, template.encode('utf-8'), file_idx, suffix);
        if err<0:
            raise Exception('Cannot resolve file template ' + template + ': ' + e2s(err))
        return res.raw.decode('utf-8')

    ##creates a new packet refering to an existing packet - see \ref gf_filter_pck_new_ref
    #\param ipck the input (referenced) packet
    #\param size the data size of the new packet
    #\param offset the offset in the original data
    #\return the new FilterPacket or None if failure
    def new_pck_ref(self, ipck, size=0, offset=0):
        if ipck==None:
            raise Exception('NULL input packet in new_pck_ref !')
        _pck = _libgpac.gf_filter_pck_new_ref(self._pid, offset, size, ipck._pck)
        if _pck:
            return FilterPacket(_pck, False)
        return None

    ##creates a new packet of the given size, allocating memory in libgpac - see \ref gf_filter_pck_new_alloc
    #\param size the data size of the new packet
    #\return the new FilterPacket or None if failure
    def new_pck(self, size=0):
        _pck = _libgpac.gf_filter_pck_new_alloc(self._pid, size, None)
        if _pck:
            pck = FilterPacket(_pck, False)
            pck._readonly = False
            return pck
        return None

    ##creates a new packet sharing memory of the filter - see \ref gf_filter_pck_new_shared
    #The filter object must have a `packet_release` method with arguments [ FilterPid,  FilterPacket ] 
    #\param data the data to use. If NumPy is detected, accept a NP Array. Otherwise data is type casted into POINTER(c_ubyte)
    #\return the new FilterPacket or None if failure
    def new_pck_shared(self, data):
        data_p = None
        data_len = 0
        readonly=False
        if not hasattr(self._filter, 'packet_release'):
            raise Exception('Filter ' + self._filter.name + ' has no packet_release callback function defined')

        if numpy_support and isinstance(data, np.ndarray):
                data_p = data.ctypes.data_as(POINTER(c_ubyte))
                data_len = data.size * data.itemsize
                readonly = not data.flags.writeable
        else:
            data_p = cast(data, POINTER(c_ubyte) )
            data_len = len(data)

        if not data_p:
            raise Exception('Unknown data reference class ' + data.__class__.__name__)

        _pck = _libgpac.gf_filter_pck_new_shared(self._pid, data_p, data_len, filter_cbk_release_packet)
        if _pck:
            pck = FilterPacket(_pck, False)
            pck._readonly = readonly
            self.pck_refs.append(pck)
            return pck
        return None

    ##creates a new packet copying a source packet - see \ref gf_filter_pck_new_copy
    #\param ipck the FilterPacket to copy
    #\return the new FilterPacket or None if failure
    def new_pck_copy(self, ipck):
        if ipck==None:
            raise Exception('NULL input packet in new_pck_copy !')
        _pck = _libgpac.gf_filter_pck_new_copy(self._pid, ipck._pck, None)
        if _pck:
            pck = FilterPacket(_pck, False)
            pck._readonly = False
            return pck
        return None

    ##creates a new packet cloning a source packet - see \ref gf_filter_pck_new_clone
    #\param ipck the FilterPacket to clone
    #\return the new FilterPacket or None if failure
    def new_pck_clone(self, ipck):
        if ipck==None:
            raise Exception('NULL input packet in new_pck_clone !')
        _pck = _libgpac.gf_filter_pck_new_clone(self._pid, ipck._pck, None)
        if _pck:
            pck = FilterPacket(_pck, False)
            pck._readonly = False
            return pck
        return None

    ##\cond private: until end, properties
    ##name of the PID - see \ref gf_filter_pid_get_name and \ref gf_filter_pid_set_name
    #\return
    @property
    def name(self):
        return _libgpac.gf_filter_pid_get_name(self._pid).decode('utf-8')

    ##\cond private
    @name.setter
    def name(self, val):
        _libgpac.gf_filter_pid_set_name(self._pid, val.encode('utf-8'))
    ##\endcond

    ##name of the parent filter - see \ref gf_filter_pid_get_filter_name
    #\return
    @property
    def filter_name(self):
        return _libgpac.gf_filter_pid_get_filter_name(self._pid).decode('utf-8')

    ##end of stream property of PID  - see \ref gf_filter_pid_is_eos and \ref gf_filter_pid_set_eos
    #\return
    @property
    def eos(self):
        return _libgpac.gf_filter_pid_is_eos(self._pid)

    ##\cond private
    @eos.setter
    def eos(self, Value):
        _libgpac.gf_filter_pid_set_eos(self._pid)
    ##\endcond private

    ##True if end of stream was seen in the chain but not yet reached by the filter  - see \ref gf_filter_pid_has_seen_eos
    #\return
    @property
    def has_seen_eos(self):
        return _libgpac.gf_filter_pid_has_seen_eos(self._pid)

    ##True if PID would block - see \ref gf_filter_pid_would_block
    #\return
    @property
    def would_block(self):
        return _libgpac.gf_filter_pid_would_block(self._pid)

    ##maximum buffer of PID in microseconds - see \ref gf_filter_pid_get_max_buffer and \ref gf_filter_pid_set_max_buffer
    #\return
    @property
    def max_buffer(self):
        return _libgpac.gf_filter_pid_get_max_buffer(self._pid)

    ##\cond private
    @max_buffer.setter
    def max_buffer(self, value):
        return _libgpac.gf_filter_pid_set_max_buffer(self._pid, value)
    ##\endcond

    ##buffer of PID in microseconds - see \ref gf_filter_pid_query_buffer_duration
    #\return
    @property
    def buffer(self):
        return _libgpac.gf_filter_pid_query_buffer_duration(self._pid, False)

    ##True if buffer is full - see \ref gf_filter_pid_query_buffer_duration
    #\return
    @property
    def buffer_full(self):
        dur = _libgpac.gf_filter_pid_query_buffer_duration(self._pid, True)
        if dur:
            return True
        else:
            return False

    ##True if no pending packet - see \ref gf_filter_pid_first_packet_is_empty
    #\return
    @property
    def first_empty(self):
        return _libgpac.gf_filter_pid_first_packet_is_empty(self._pid)

    ##value of CTS of first pending packet, None if none - see \ref gf_filter_pid_get_first_packet_cts
    #\return
    @property
    def first_cts(self):
        cts = c_ulonglong(0)
        res = _libgpac.gf_filter_pid_get_first_packet_cts(self._pid, byref(cts))
        if not res:
            return None
        return cts.value

    ##number of queued packets for input pid - see \ref gf_filter_pid_get_packet_count
    #\return
    @property
    def nb_pck_queued(self):
        return _libgpac.gf_filter_pid_get_packet_count(self._pid)

    ##timescale of pid - see \ref gf_filter_pid_get_timescale
    #\return
    @property
    def timescale(self):
        return _libgpac.gf_filter_pid_get_timescale(self._pid)

    ##minimum packet duration (in timescale) of pid - see \ref gf_filter_pid_get_min_pck_duration
    #\return
    @property
    def min_pck_dur(self):
        return _libgpac.gf_filter_pid_get_min_pck_duration(self._pid)

    ##True if PID is playing  - see \ref gf_filter_pid_is_playing
    #\return
    @property
    def playing(self):
        return _libgpac.gf_filter_pid_is_playing(self._pid)

    ##\endcond

    #vars:
    #src_name, args, src_args, src_url, dst_url: probably not needed since the entire session is exposed to the custom filter




#
#        filter packet bindings
#

##\cond private

#getters
_libgpac.gf_filter_pck_get_dts.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_dts.restype = c_ulonglong
_libgpac.gf_filter_pck_set_dts.argtypes = [_gf_filter_packet, c_ulonglong]

_libgpac.gf_filter_pck_get_cts.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_cts.restype = c_ulonglong
_libgpac.gf_filter_pck_set_cts.argtypes = [_gf_filter_packet, c_ulonglong]

_libgpac.gf_filter_pck_get_sap.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_sap.restype = c_uint
_libgpac.gf_filter_pck_set_sap.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_duration.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_duration.restype = c_uint
_libgpac.gf_filter_pck_set_duration.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_data.argtypes = [_gf_filter_packet, POINTER(c_uint)]
_libgpac.gf_filter_pck_get_data.restype = POINTER(c_ubyte)

_libgpac.gf_filter_pck_get_framing.argtypes = [_gf_filter_packet, POINTER(c_bool), POINTER(c_bool)]
_libgpac.gf_filter_pck_set_framing.argtypes = [_gf_filter_packet, c_bool, c_bool]

_libgpac.gf_filter_pck_get_timescale.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_timescale.restype = c_uint

_libgpac.gf_filter_pck_get_interlaced.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_interlaced.restype = c_uint
_libgpac.gf_filter_pck_set_interlaced.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_corrupted.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_corrupted.restype = c_bool
_libgpac.gf_filter_pck_set_corrupted.argtypes = [_gf_filter_packet, c_bool]

_libgpac.gf_filter_pck_get_seek_flag.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_seek_flag.restype = c_bool
_libgpac.gf_filter_pck_set_seek_flag.argtypes = [_gf_filter_packet, c_bool]

_libgpac.gf_filter_pck_get_byte_offset.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_byte_offset.restype = c_ulonglong
_libgpac.gf_filter_pck_set_byte_offset.argtypes = [_gf_filter_packet, c_ulonglong]

_libgpac.gf_filter_pck_get_roll_info.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_roll_info.restype = c_short
_libgpac.gf_filter_pck_set_roll_info.argtypes = [_gf_filter_packet, c_short]

_libgpac.gf_filter_pck_get_crypt_flags.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_crypt_flags.restype = c_uint
_libgpac.gf_filter_pck_set_crypt_flags.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_clock_type.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_clock_type.restype = c_uint
_libgpac.gf_filter_pck_set_clock_type.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_carousel_version.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_carousel_version.restype = c_uint
_libgpac.gf_filter_pck_set_carousel_version.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_seq_num.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_seq_num.restype = c_uint
_libgpac.gf_filter_pck_set_seq_num.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_dependency_flags.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_dependency_flags.restype = c_uint
_libgpac.gf_filter_pck_set_dependency_flags.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_frame_interface.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_frame_interface.restype = c_void_p

_libgpac.gf_filter_pck_is_blocking_ref.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_is_blocking_ref.restype = c_bool

_libgpac.gf_filter_pck_enum_properties.argtypes = [_gf_filter_packet, POINTER(c_uint), POINTER(c_uint), POINTER(c_char_p)]
_libgpac.gf_filter_pck_enum_properties.restype = POINTER(PropertyValue)

_libgpac.gf_filter_pck_get_property.argtypes = [_gf_filter_packet, c_uint]
_libgpac.gf_filter_pck_get_property.restype = POINTER(PropertyValue)
_libgpac.gf_filter_pck_get_property_str.argtypes = [_gf_filter_packet, c_char_p]
_libgpac.gf_filter_pck_get_property_str.restype = POINTER(PropertyValue)

_libgpac.gf_filter_pck_ref_ex.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_ref_ex.restype = _gf_filter_packet
_libgpac.gf_filter_pck_unref.argtypes = [_gf_filter_packet]

_libgpac.gf_filter_pck_discard.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_set_readonly.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_send.argtypes = [_gf_filter_packet]

_libgpac.gf_filter_pck_merge_properties.argtypes = [_gf_filter_packet, _gf_filter_packet]

_libgpac.gf_filter_pck_set_property.argtypes = [_gf_filter_packet, c_uint, POINTER(PropertyValue)]
_libgpac.gf_filter_pck_set_property_str.argtypes = [_gf_filter_packet, c_char_p, POINTER(PropertyValue)]
_libgpac.gf_filter_pck_truncate.argtypes = [_gf_filter_packet, c_uint]


##\endcond private

##filter packet object
class FilterPacket:
    ##\cond private
    def __init__(self, pck, is_src):
        self._pck = pck
        self._is_src = is_src
        self._readonly = True
    ##\endcond private
        #hack for doxygen to generate member vars (not support for parsing @property)
        if 0:
            ##Decode Timestamp - see \ref gf_filter_pck_get_dts and \ref gf_filter_pck_set_dts
            #\hideinitializer
            self.dts=0
            ##Compose Timestamp - see \ref gf_filter_pck_get_cts and \ref gf_filter_pck_set_cts
            #\hideinitializer
            self.cts=0
            ##SAP type - see \ref gf_filter_pck_get_sap and \ref gf_filter_pck_set_sap
            #\hideinitializer
            self.sap=0
            ##Duration - see \ref gf_filter_pck_get_duration and \ref gf_filter_pck_set_duration
            #\hideinitializer
            self.dur=0
            ##Size of packet data, readonly
            #\hideinitializer
            self.size=0
            ##Packet data - see \ref gf_filter_pck_get_data
            #If NumPy is available, the data is returned as a np Array, otherwise it is a POINTER(c_ubyte) object
            #The property is readonly (cannot be set) but content can be modified for non-refs output packets
            #\hideinitializer
            self.data=0
            ##frame start - see \ref gf_filter_pck_get_framing and \ref gf_filter_pck_set_framing
            #\hideinitializer
            self.start=0
            ##frame end - see \ref gf_filter_pck_get_framing and \ref gf_filter_pck_set_framing
            #\hideinitializer
            self.end=0
            ##associated timescale, readonly - see \ref gf_filter_pck_get_timescale
            #\hideinitializer
            self.timescale=0
            ##Interlaced flags - see \ref gf_filter_pck_get_interlaced and \ref gf_filter_pck_set_interlaced
            #\hideinitializer
            self.interlaced=0
            ##Corrupted flag - see \ref gf_filter_pck_get_corrupted and \ref gf_filter_pck_set_corrupted
            #\hideinitializer
            self.corrupted=0
            ##Seek flag - see \ref gf_filter_pck_get_seek_flag and \ref gf_filter_pck_set_seek_flag
            #\hideinitializer
            self.seek=0
            ##Byte offset - see \ref gf_filter_pck_get_byte_offset and \ref gf_filter_pck_set_byte_offset
            #\hideinitializer
            self.byte_offset=0
            ##Roll info - see \ref gf_filter_pck_get_roll_info and \ref gf_filter_pck_set_roll_info
            #\hideinitializer
            self.roll=0
            ##Encryption flags - see \ref gf_filter_pck_get_crypt_flags and \ref gf_filter_pck_set_crypt_flags
            #\hideinitializer
            self.crypt=0
            ##Clock reference flag - see \ref gf_filter_pck_get_clock_type and \ref gf_filter_pck_set_clock_type
            #\hideinitializer
            self.clock=0
            ##Carousel version - see \ref gf_filter_pck_get_carousel_version and \ref gf_filter_pck_set_carousel_version
            #\hideinitializer
            self.carousel=0
            ##Sequence number - see \ref gf_filter_pck_get_seq_num and \ref gf_filter_pck_set_seq_num
            #\hideinitializer
            self.seqnum=0
            ##Dependency flags - see \ref gf_filter_pck_get_dependency_flags and \ref gf_filter_pck_set_dependency_flags
            #\hideinitializer
            self.deps=0


    ##enumerate an packet properties
    #\param callback_obj callback object to use, must have a 'on_prop_enum' method defined taking two parameters, prop_name(string) and propval
    #\return
    def enum_props(self, callback_obj):
        if not hasattr(callback_obj, 'on_prop_enum'):
            return

        pidx=c_uint(0)
        while True:
            _p4cc=c_uint(0)
            _pname=c_char_p(0)
            prop = _libgpac.gf_filter_pck_enum_properties(self._pck, byref(pidx), byref(_p4cc), byref(_pname))
            if prop:
                p4cc=_p4cc.value
                pname=None
                if _pname:
                    pname=_pname.value.decode('utf-8')
                else:
                    pname=_libgpac.gf_props_4cc_get_name(p4cc).decode('utf-8')
                pval = _prop_to_python(pname, prop.contents)
                callback_obj.on_prop_enum(pname, pval)
            else:
                break

    ##get a packet property - see \ref gf_filter_pck_get_property and \ref gf_filter_pck_get_property_str
    #\param prop_name name of property to get
    #\return property value, or None if not found
    def get_prop(self, prop_name):
        _name = prop_name.encode('utf-8')
        p4cc = _libgpac.gf_props_get_id(_name)
        if p4cc:
            prop = _libgpac.gf_filter_pck_get_property(self._pck, p4cc)
        else:
            prop = _libgpac.gf_filter_pck_get_property_str(self._pck, _name)
        if prop:
            return _prop_to_python(prop_name, prop.contents)
        return None;

    ##increase packet reference count - see \ref gf_filter_pck_ref_ex
    #\return
    def ref(self):
        ##\cond private
        self._pck = _libgpac.gf_filter_pck_ref_ex(self._pck)
        ##\endcond private

    ##decrease packet reference count - see \ref gf_filter_pck_unref
    #\return
    def unref(self):
        _libgpac.gf_filter_pck_unref(self._pck)


    ##discard an output packet instead of sending it - see \ref gf_filter_pck_discard
    #\return
    def discard(self):
        ##\cond private
        if self._is_src:
            raise Exception('Cannot discard on source packet')
        _libgpac.gf_filter_pck_discard(self._pck)
        self._pck = None
        ##\endcond private

    ##mark an output packet as readonly - see \ref gf_filter_pck_set_readonly
    #\return
    def readonly(self):
        if self._is_src:
            raise Exception('Cannot set readonly on source packet')
        _libgpac.gf_filter_pck_set_readonly(self._pck)

    ##send the packet - see \ref gf_filter_pck_send
    #\return
    def send(self):
        if self._is_src:
            raise Exception('Cannot send on source packet')
        _libgpac.gf_filter_pck_send(self._pck)

    ##copy properties of source packet in this packet - see \ref gf_filter_pck_merge_properties
    #\param ipck source packet
    #\return
    def copy_props(self, ipck):
        if self._is_src:
            raise Exception('Cannot copy properties on source packet')
        if ipck==None:
            return
        err = _libgpac.gf_filter_pck_merge_properties(ipck._pck, self._pck)
        if err<0:
            raise Exception('Cannot copy properties: ' + e2s(err) )

    ##set property in this packet - see \ref gf_filter_pck_set_property and \ref gf_filter_pck_set_property_str
    #\param pcode name of property
    #\param prop property value
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def set_prop(self, pcode, prop, custom_type=0):
        if self._is_src:
            raise Exception('Cannot copy properties on source packet')
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop==None:
            if prop_4cc:
                _libgpac.gf_filter_pck_set_property(self._pck, prop_4cc, None)
            else:
                _libgpac.gf_filter_pck_set_property_str(self._pck, prop_name, None)
            return

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pck_set_property(self._pck, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pck_set_property_str(self._pck, prop_name, byref(prop_val))

    ##truncates an output packet to the given size - see \ref gf_filter_pck_truncate
    #\param size new size of packet
    #\return
    def truncate(self, size):
        if self._is_src:
            raise Exception('Cannot truncate on source packet')
        _libgpac.gf_filter_pck_truncate(self._pck, size)

    ##true if packet holds a GF_FrameInterface object and not a data packet
    #\return True if packet is a frame interface object
    def is_frame_ifce(self):
        p = _libgpac.gf_filter_pck_get_frame_interface(self._pck)
        if p:
            return True
        return False

    ##Check if packet is a blocking reference - see \ref gf_filter_pck_is_blocking_ref
    #\return true if packet is a blocking reference
    def blocking(self):
        return _libgpac.gf_filter_pck_is_blocking_ref(self._pck)


    ##\cond private: until end, all packet properties
    @property
    def dts(self):
        return _libgpac.gf_filter_pck_get_dts(self._pck)

    @dts.setter
    def dts(self, val):
        if self._is_src:
            raise Exception('Cannot set DTS on source packet')
        _libgpac.gf_filter_pck_set_dts(self._pck, val)

    @property
    def cts(self):
        return _libgpac.gf_filter_pck_get_cts(self._pck)

    @cts.setter
    def cts(self, val):
        if self._is_src:
            raise Exception('Cannot set CTS on source packet')
        return _libgpac.gf_filter_pck_set_cts(self._pck, val)

    @property
    def sap(self):
        return _libgpac.gf_filter_pck_get_sap(self._pck)

    @sap.setter
    def sap(self, val):
        if self._is_src:
            raise Exception('Cannot set SAP on source packet')
        return _libgpac.gf_filter_pck_set_sap(self._pck, val)

    @property
    def dur(self):
        return _libgpac.gf_filter_pck_get_duration(self._pck)

    @dur.setter
    def dur(self, val):
        if self._is_src:
            raise Exception('Cannot set duration on source packet')
        return _libgpac.gf_filter_pck_set_duration(self._pck, val)

    @property
    def size(self):
        size = c_uint(0)
        _libgpac.gf_filter_pck_get_data(self._pck, byref(size))
        return size.value

    @property
    def data(self):
        size = c_uint(0)
        data = _libgpac.gf_filter_pck_get_data(self._pck, byref(size))
        if not data:
            return None

        if numpy_support:
            data = np.ctypeslib.as_array(data, (size.value,))
            if self._readonly:
                data.flags.writeable=False
        return data        

    @property
    def start(self):
        start = c_bool(False)
        end = c_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        return start.value

    @start.setter
    def start(self, value):
        if self._is_src:
            raise Exception('Cannot set framing on source packet')
        start = c_bool(False)
        end = c_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        _libgpac.gf_filter_pck_set_framing(self._pck, value, end.value)

    @property
    def end(self):
        start = c_bool(False)
        end = c_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        return end.value

    @end.setter
    def end(self, value):
        if self._is_src:
            raise Exception('Cannot set framing on source packet')
        start = c_bool(False)
        end = c_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        _libgpac.gf_filter_pck_set_framing(self._pck, start.value, value)

    @property
    def timescale(self):
        return _libgpac.gf_filter_pck_get_timescale(self._pck)

    @property
    def interlaced(self):
        return _libgpac.gf_filter_pck_get_interlaced(self._pck)

    @interlaced.setter
    def interlaced(self, value):
        if self._is_src:
            raise Exception('Cannot set interlaced on source packet')
        return _libgpac.gf_filter_pck_set_interlaced(self._pck, value)

    @property
    def corrupted(self):
        return _libgpac.gf_filter_pck_get_corrupted(self._pck)

    @corrupted.setter
    def corrupted(self, value):
        if self._is_src:
            raise Exception('Cannot set corrupted on source packet')
        return _libgpac.gf_filter_pck_set_corrupted(self._pck, value)

    @property
    def seek(self):
        return _libgpac.gf_filter_pck_get_seek_flag(self._pck)

    @seek.setter
    def seek(self, value):
        if self._is_src:
            raise Exception('Cannot set seek on source packet')
        return _libgpac.gf_filter_pck_set_seek_flag(self._pck, value)

    @property
    def byte_offset(self):
        return _libgpac.gf_filter_pck_get_byte_offset(self._pck)

    @byte_offset.setter
    def byte_offset(self, value):
        if self._is_src:
            raise Exception('Cannot set byte offset on source packet')
        return _libgpac.gf_filter_pck_set_byte_offset(self._pck, value)

    @property
    def roll(self):
        return _libgpac.gf_filter_pck_get_roll_info(self._pck)

    @roll.setter
    def roll(self):
        if self._is_src:
            raise Exception('Cannot set roll on source packet')
        return _libgpac.gf_filter_pck_set_roll_info(self._pck, value)

    @property
    def crypt(self):
        return _libgpac.gf_filter_pck_get_crypt_flags(self._pck)

    @crypt.setter
    def crypt(self, value):
        if self._is_src:
            raise Exception('Cannot set crypt on source packet')
        return _libgpac.gf_filter_pck_set_crypt_flags(self._pck, value)

    @property
    def clock(self):
        return _libgpac.gf_filter_pck_get_clock_type(self._pck)

    @clock.setter
    def clock(self, value):
        if self._is_src:
            raise Exception('Cannot set clock on source packet')
        return _libgpac.gf_filter_pck_set_clock_type(self._pck, value)

    @property
    def carousel(self):
        return _libgpac.gf_filter_pck_get_carousel_version(self._pck)

    @carousel.setter
    def carousel(self, value):
        if self._is_src:
            raise Exception('Cannot set carousel on source packet')
        return _libgpac.gf_filter_pck_set_carousel_version(self._pck, value)

    @property
    def seqnum(self):
        return _libgpac.gf_filter_pck_get_seq_num(self._pck)

    @seqnum.setter
    def seqnum(self):
        if self._is_src:
            raise Exception('Cannot set segnum on source packet')
        return _libgpac.gf_filter_pck_set_seq_num(self._pck, value)

    @property
    def deps(self):
        return _libgpac.gf_filter_pck_get_dependency_flags(self._pck)

    @deps.setter
    def deps(self, value):
        if self._is_src:
            raise Exception('Cannot set deps on source packet')
        return _libgpac.gf_filter_pck_set_dependency_flags(self._pck, value)

    ##\endcond private

    #todo
    #append ?



## @}
#
