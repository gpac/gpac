#
#          GPAC - Multimedia Framework C SDK
#
#          Authors: Jean Le Feuvre
#          Copyright (c) Telecom Paris 2020-2024
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
# See [`gpac -h props`](https://wiki.gpac.io/Filters/filters_properties/) for the complete list of built-in property names.
#
# Properties values are automatically converted to or from python types whenever possible. Types with no python equivalent (vectors, fractions) are defined as classes in python.
# For example:
# - when setting a UIntList property, pass a python list of ints 
# - when reading a UIntList property, a python list of ints will be returned 
# - when setting a PropVec2i property, pass a PropVec2i object 
# - when setting a PropVec2iList property, pass a python list of PropVec2i 
#
# 4CCs are handled as strings in python, and list of 4CCs are handled as list of strings
#
# The following builtin property types are always handled as strings in Python instead of int in libgpac :
# - StreamType: string containing the streamtype name
# - CodecID: string containing the codec name
# - Enumeration properties (PixelFormat, AudioFormat, ...): string containing the corresponding name
#
# # Basic setup
#
# You can initialize libgpac before any other calls to set memory tracker or profile. When importing the module, libgpac is by default initialized with no memory tracking and using the default profile.
# You currently must uninintialize it after everything gpac-related is done.
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
# By default the filter session will run with implicit linking.
#
# It is possible to assign sources of a filter (much like the `@` command in `gpac`):
# \code
# dst.set_source(f)
# \endcode
#
#
# # Posting user tasks
# 
# You can post tasks to the session scheduler to get called back (useful when running the session in blocking mode)
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
#- custom filters cannot be used as sources of filters loading a source filter graph dynamically, such as the dashin filter.
#- custom filters cannot be used as destination of filters loading a destination filter graph dynamically, such as the dasher filters.
#
# A custom filter must implement the \ref FilterCustom class, and optionally provide the following methods
# - configure_pid: callback for PID configuration, mandatory if your filter is not a source
# - process: callback for processing
# - process_event: callback for processing and event
# - probe_data: callback for probing a data format
# - reconfigure_output: callback for output reconfiguration (PID capability negotiation)
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


## 
#  \defgroup pycore_grp libgpac core tools
#  \ingroup pyapi_grp Python APIs
#  \brief Core tools for libgpac.
#
# @{


from ctypes import *
from ctypes.util import find_library
import datetime
import types
import os
import copy

## set to True if numpy was successfully loaded
##\hideinitializer
numpy_support=True

try:
    import numpy as np
except ImportError:
    numpy_support = False
    print("\nWARNING! numpy not present, packet data type is ctypes POINTER(c_ubyte)\n")

##ctypes instance of libgpac
##\hideinitializer
_libgpac=None

## \cond private
#load libgpac
try:
    dll_path = find_library("libgpac.dll")
    if not dll_path:
        raise OSError("No libgpac.dll")

    _libgpac = cdll.LoadLibrary(os.path.abspath(dll_path))
except OSError:
    try:
        _libgpac = cdll.LoadLibrary("libgpac.so")
    except OSError:
        try:
            _libgpac = cdll.LoadLibrary("libgpac.dylib")
        except OSError:
            print('Failed to locate libgpac (.so/.dll/.dylib) - make sure it is in your system path')
            os._exit(1)

#change this to reflect API we encapsulate. An incomatibility in either of these will throw a warning
GF_ABI_MAJOR=12
GF_ABI_MINOR=12

gpac_abi_major=_libgpac.gf_gpac_abi_major()
gpac_abi_minor=_libgpac.gf_gpac_abi_minor()

## \endcond private

## Set to true if mismatch was detected between the ABI version the Python wrapper was designed and the libgpac shared library ABI version
# A warning is thrown if mismatched, but it is left up to the pythin script to decide whether it still wants to use libgpac wrapper
_libgpac_abi_mismatch=False

## \cond private
if (gpac_abi_major != GF_ABI_MAJOR) or (gpac_abi_minor != GF_ABI_MINOR):
    abi_mismatch=True
    print('WARNING: this python wrapper is for GPAC ABI ' + str(GF_ABI_MAJOR) + '.' + str(GF_ABI_MINOR)  + ' but native libgpac ABI is ' + str(gpac_abi_major) + '.'  + str(gpac_abi_minor) + '\n\tUndefined behavior or crashes might happen, please update libgpac.py')

## \endcond private

#
# Type definitions
#

##\cond private
_gf_filter_session = c_void_p
_gf_filter = c_void_p
_gf_filter_pid = c_void_p
_gf_filter_packet = c_void_p
_gf_property_entry = c_void_p
_gf_list = c_void_p
gf_bool = c_uint

##\endcond




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

_libgpac.gf_sys_clock.restype = c_uint
_libgpac.gf_sys_clock_high_res.restype = c_ulonglong

_libgpac.gf_sys_profiler_log.argtypes = [c_char_p]
_libgpac.gf_sys_profiler_send.argtypes = [c_char_p]
_libgpac.gf_sys_profiler_sampling_enabled.restype = gf_bool
_libgpac.gf_sys_profiler_enable_sampling.argtypes = [gf_bool]

_libgpac.gf_4cc_to_str.argtypes = [c_uint]
_libgpac.gf_4cc_to_str.restype = c_char_p

_libgpac.gf_4cc_parse.argtypes = [c_char_p]
_libgpac.gf_4cc_parse.restype = c_uint

_libgpac.gf_sleep.argtypes = [c_uint]
_libgpac.gf_sleep.restype = c_uint

_libgpac.gf_props_enum_name.argtypes = [c_uint, c_uint]
_libgpac.gf_props_enum_name.restype = c_char_p

#default init of libgpac
_libgpac.user_init = False
err = _libgpac.gf_sys_init(0, None)
if err<0:
	raise Exception('Failed to initialize libgpac: ' + e2s(err))

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


## initialize libgpac - see \ref gf_sys_init
# \param mem_track
# \param profile
# \return
#
def init(mem_track=0, profile=None):
    if _libgpac.user_init:
        return
    _libgpac.user_init = True
    _libgpac.gf_sys_close()
    err = _libgpac.gf_sys_init(mem_track, profile)
    if err<0:
        raise Exception('Failed to initialize libgpac: ' + e2s(err))


## close libgpac - see \ref gf_sys_close
# \note Make sure you have destroyed all associated gpac resources before calling this !
# \return
#
def close():
    _libgpac.gf_sys_close()
    if hasattr(_libgpac, 'gf_memory_size'):
        if _libgpac.gf_memory_size() or _libgpac.gf_file_handles_count():
            set_logs("mem@info")
            _libgpac.gf_memory_print()
            return 2
    return 0

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

##\cond private

#keep args at libgpac level to avoid python GC
_libgpac._args=None

##\endcond private

## set libgpac arguments - see \ref gf_sys_set_args
# \param args list of strings, the first string is ignored (considered to be the executable name)
# \return
def set_args(args):
    _libgpac.user_init = True
    nb_args = len(args)
    _libgpac._args = (POINTER(c_char)*nb_args)()
    for i, arg in enumerate(args):
        enc_arg = arg.encode('utf-8')
        _libgpac._args[i] = create_string_buffer(enc_arg)
    _libgpac.gf_sys_set_args(nb_args, cast(_libgpac._args, POINTER(POINTER(c_char))) )


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
    _libgpac.user_init = True
    if hasattr(callback_obj, 'on_rmt_event')==False:
        raise Exception('No on_rmt_event function on callback')
    err = _libgpac.gf_sys_profiler_set_callback(py_object(callback_obj), rmt_fun_cbk)
    if err<0:
        return False
    return True

## send message to profiler (Remotery) - see \ref gf_sys_profiler_log
# \param text text to send
# \return True if success, False if no Remotery support
def rmt_log(text):
    err = _libgpac.gf_sys_profiler_log(text.encode('utf-8'))
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
    _libgpac.user_init = True
    _libgpac.gf_sys_profiler_enable_sampling(value)
    

## sleep for given time in milliseconds
# \param value time to sleep
# \return
def sleep(value):
    _libgpac.gf_sleep(value)


## @}


## 
#  \defgroup pystruct_grp Structure Wrappers
#  \ingroup pyapi_grp
#  \brief Python Structures 
#
# Python Wrappers for gpac C structures used in this API
#
# @{

## fraction object, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_Fraction
class Fraction(Structure):
    ## \cond private
    def  __init(self, num, den):
        self.num = num
        self.den = den
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
		("report_updated", gf_bool),
		("name", c_char_p),
		("reg_name", c_char_p),
		("filter_id", c_char_p),
		("done", gf_bool),
		("nb_pid_in", c_uint),
		("nb_in_pck", c_ulonglong),
		("nb_pid_out", c_uint),
		("nb_out_pck", c_ulonglong),
		("in_eos", gf_bool),
		("type", c_int),
		("stream_type", c_int),
		("codecid", c_int),
		("last_ts_sent", Fraction64),
		("last_ts_drop", Fraction64)
	]
    ## \endcond

## filter pid statistics object, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FilterPidStatistics
class FilterPidStatistics(Structure):
    ## \cond private
	_fields_ = [
		("disconnected", c_uint),
		("average_process_rate", c_uint),
		("max_process_rate", c_uint),
		("average_bitrate", c_uint),
		("max_bitrate", c_uint),
		("nb_processed", c_uint),
		("max_process_time", c_uint),
		("total_process_time", c_ulonglong),
		("first_process_time", c_ulonglong),
		("last_process_time", c_ulonglong),
		("min_frame_dur", c_uint),
		("nb_saps", c_uint),
		("max_sap_process_time", c_uint),
		("total_sap_process_time", c_ulonglong),
		("max_buffer_time", c_ulonglong),
		("max_playout_time", c_ulonglong),
		("min_playout_time", c_ulonglong),
		("buffer_time", c_ulonglong),
		("nb_buffer_units", c_uint),
		("last_rt_report", c_ulonglong),
		("rtt", c_uint),
		("jitter", c_uint),
		("loss_rate", c_uint),
		("last_ts_drop", Fraction64),
		("last_ts_sent", Fraction64)
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
#Fields have the same types, names and semantics as \ref GF_PropVec4i
class PropVec4i(Structure):
    ## \cond private
    _fields_ = [("x", c_int), ("y", c_int), ("z", c_int), ("w", c_int)]
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
    ## \cond private
    _fields_ = [("vals", POINTER(c_char_p)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += self.vals[i].decode('utf-8')
        return res
    ## \endcond private

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
		("boolean", gf_bool),
		("frac", Fraction),
		("lfrac", Fraction64),
		("fnumber", c_float),
		("number", c_double),
		("vec2i", PropVec2i),
		("vec2", PropVec2),
		("vec3i", PropVec3i),
		("vec4i", PropVec3i),
		("data", PropData),
		("string", c_char_p),
		("ptr", c_void_p),
        ("string_list", PropStringList),
        ("uint_list", PropUIntList),
		("sint_list", PropIntList),
		("v2i_list", PropVec2iList)
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
        ("no_byterange_forward", c_ubyte),
        ("to_pck", c_uint),
        ("orig_delay", c_uint),
        ("hint_first_dts", c_ulonglong),
        ("hint_start_offset", c_ulonglong),
        ("hint_end_offset", c_ulonglong)
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
        ("is_init_segment", c_ubyte),
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
        ("is_init", gf_bool),
        ("media_range_start", c_ulonglong),
        ("media_range_end", c_ulonglong),
        ("idx_range_start", c_ulonglong),
        ("idx_range_end", c_ulonglong),
        ("is_init", c_ubyte),
        ("is_shift", c_ubyte)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_FragmentSize
class FEVT_FragmentSize(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("is_last", gf_bool),
        ("offset", c_ulonglong),
        ("size", c_ulonglong),
        ("duration", Fraction64),
        ("independent", gf_bool),
    ]
    ## \endcond


## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_AttachScene
class FEVT_AttachScene(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("odm", c_void_p),
        ("node", c_void_p)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_QualitySwitch
class FEVT_QualitySwitch(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("up", gf_bool),
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
        ("is_gaze", gf_bool)
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
        ("pid_only", gf_bool)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_EncodeHints
class FEVT_EncodeHints(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("intra_period", Fraction),
        ("gen_dsi_only", gf_bool)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_NTPRef
class FEVT_NTPRef(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("ntp", c_ulonglong)
    ]
    ## \endcond


## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_EventMouse
class EVT_mouse(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("x", c_int),
        ("y", c_int),
        ("wheel", c_float),
        ("button", c_int),
        ("keys", c_uint),
        ("window_id", c_uint)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_EventMultiTouch
class EVT_mtouch(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("x", c_float),
        ("y", c_float),
        ("rotation", c_float),
        ("pinch", c_float),
        ("num_fingers", c_uint),
        ("window_id", c_uint)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_EventKey
class EVT_keys(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("key_code", c_uint),
        ("hw_code", c_uint),
        ("flags", c_uint),
        ("window_id", c_uint)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_EventChar
class EVT_char(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("unicode_char", c_uint),
        ("window_id", c_uint)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_EventSize
class EVT_size(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("width", c_uint),
        ("height", c_uint),
        ("orientation", c_uint),
        ("window_id", c_uint)
    ]
    ## \endcond

## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_EventShow
class EVT_show(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("show_type", c_uint),
        ("window_id", c_uint)
    ]
    ## \endcond


## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as GF_Event
#only common events from GPAC video ouput are mapped
class EVT_base(Union):
    ## \cond private
    _fields_ =  [
        ("type", c_ubyte),
        ("mouse", EVT_mouse),
        ("mtouch", EVT_mtouch),
        ("keys", EVT_keys),
        ("char", EVT_char),
        ("size", EVT_size),
        ("show", EVT_show)
    ]
    ## \endcond


## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as \ref GF_FEVT_Event
class FEVT_UserEvent(Structure):
    ## \cond private
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", _gf_filter_pid),
        ("event", EVT_base)
    ]
    ## \endcond


## event value, as defined in libgpac and usable as a Python object
#Fields have the same types, names and semantics as GF_FilterEvent
class FilterEvent(Union):
    ## constructor
    #\param evt_type type of event
    def __init__(self, evt_type=0):
        self.base.type = evt_type
    ## \cond private
        _libgpac.user_init = True

    _fields_ =  [
        ("base", FEVT_Base),
        ("play", FEVT_Play),
        ("seek", FEVT_SourceSeek),
        ("attach_scene", FEVT_AttachScene),
        ("user", FEVT_UserEvent),
        ("quality_switch", FEVT_QualitySwitch),
        ("visibility_hint", FEVT_VisibilityHint),
        ("buffer_req", FEVT_BufferRequirement),
        ("seg_size", FEVT_SegmentSize),
        ("frag_size", FEVT_FragmentSize),
        ("file_del", FEVT_FileDelete),
        ("encode_hints", FEVT_EncodeHints),
        ("ntp", FEVT_NTPRef)
    ]
    ## \endcond


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


## @}


## 
#  \defgroup pycst_grp Constants
#  \ingroup pyapi_grp
#  \brief Constants definitions
#
# Python Wrappers for gpac C constants used in this API
#
# @{


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
#see \ref GF_FS_FLAG_NON_BLOCKING
GF_FS_FLAG_NON_BLOCKING=1<<2
##\hideinitializer
#see \ref GF_FS_FLAG_NO_GRAPH_CACHE
GF_FS_FLAG_NO_GRAPH_CACHE=1<<3
##\hideinitializer
#see \ref GF_FS_FLAG_NO_REGULATION
GF_FS_FLAG_NO_REGULATION=1<<4
##\hideinitializer
#see \ref GF_FS_FLAG_NO_PROBE
GF_FS_FLAG_NO_PROBE=1<<5
##\hideinitializer
#see \ref GF_FS_FLAG_NO_REASSIGN
GF_FS_FLAG_NO_REASSIGN=1<<6
##\hideinitializer
#see \ref GF_FS_FLAG_PRINT_CONNECTIONS
GF_FS_FLAG_PRINT_CONNECTIONS=1<<7
##\hideinitializer
#see \ref GF_FS_FLAG_NO_ARG_CHECK
GF_FS_FLAG_NO_ARG_CHECK=1<<8
##\hideinitializer
#see \ref GF_FS_FLAG_NO_RESERVOIR
GF_FS_FLAG_NO_RESERVOIR=1<<0
##\hideinitializer
#see \ref GF_FS_FLAG_FULL_LINK
GF_FS_FLAG_FULL_LINK=1<<10
##\hideinitializer
#see \ref GF_FS_FLAG_NO_IMPLICIT
GF_FS_FLAG_NO_IMPLICIT=1<<11
##\hideinitializer
#see \ref GF_FS_FLAG_REQUIRE_SOURCE_ID
GF_FS_FLAG_REQUIRE_SOURCE_ID=1<<12
##\hideinitializer
#see \ref GF_FS_FLAG_FORCE_DEFER_LINK
GF_FS_FLAG_FORCE_DEFER_LINK = 1<<13
##\hideinitializer
#see \ref GF_FS_FLAG_PREVENT_PLAY
GF_FS_FLAG_PREVENT_PLAY = 1<<14

##\hideinitializer
#see \ref GF_PROP_FORBIDDEN
GF_PROP_FORBIDDEN=0
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
#see \ref GF_PROP_VEC4I
GF_PROP_VEC4I=13
##\hideinitializer
#see \ref GF_PROP_STRING
GF_PROP_STRING=14
##\hideinitializer
#see \ref GF_PROP_STRING_NO_COPY
GF_PROP_STRING_NO_COPY=15
##\hideinitializer
#see \ref GF_PROP_DATA
GF_PROP_DATA=16
##\hideinitializer
#see \ref GF_PROP_NAME
GF_PROP_NAME=17
##\hideinitializer
#see \ref GF_PROP_DATA_NO_COPY
GF_PROP_DATA_NO_COPY=18
##\hideinitializer
#see \ref GF_PROP_CONST_DATA
GF_PROP_CONST_DATA=19
##\hideinitializer
#see \ref GF_PROP_POINTER
GF_PROP_POINTER=20
##\hideinitializer
#see \ref GF_PROP_STRING_LIST
GF_PROP_STRING_LIST=21
##\hideinitializer
#see \ref GF_PROP_UINT_LIST
GF_PROP_UINT_LIST=22
##\hideinitializer
#see \ref GF_PROP_SINT_LIST
GF_PROP_SINT_LIST=23
##\hideinitializer
#see \ref GF_PROP_VEC2I_LIST
GF_PROP_VEC2I_LIST=24
##\hideinitializer
#see \ref GF_PROP_4CC
GF_PROP_4CC=25
##\hideinitializer
#see \ref GF_PROP_4CC_LIST
GF_PROP_4CC_LIST=26

##\hideinitializer
#see \ref GF_PROP_FIRST_ENUM
GF_PROP_FIRST_ENUM=40
##\hideinitializer
#see \ref GF_PROP_PIXFMT
GF_PROP_PIXFMT=GF_PROP_FIRST_ENUM
##\hideinitializer
#see \ref GF_PROP_PCMFMT
GF_PROP_PCMFMT=GF_PROP_FIRST_ENUM+1
##\hideinitializer
#see \ref GF_PROP_CICP_COL_PRIM
GF_PROP_CICP_COL_PRIM=GF_PROP_FIRST_ENUM+2
##\hideinitializer
#see \ref GF_PROP_CICP_COL_TFC
GF_PROP_CICP_COL_TFC=GF_PROP_FIRST_ENUM+3
##\hideinitializer
#see \ref GF_PROP_CICP_COL_MX
GF_PROP_CICP_COL_MX=GF_PROP_FIRST_ENUM+4
##\hideinitializer
#see \ref GF_PROP_CICP_LAYOUT
GF_PROP_CICP_LAYOUT=GF_PROP_FIRST_ENUM+5

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
#see GF_STATS_LOCAL
GF_STATS_LOCAL = 0
##\hideinitializer
#see GF_STATS_LOCAL_INPUTS
GF_STATS_LOCAL_INPUTS = 1
##\hideinitializer
#see GF_STATS_DECODER_SINK
GF_STATS_DECODER_SINK = 2
##\hideinitializer
#see GF_STATS_DECODER_SOURCE
GF_STATS_DECODER_SOURCE = 3
##\hideinitializer
#see GF_STATS_ENCODER_SINK
GF_STATS_ENCODER_SINK = 4
##\hideinitializer
#see GF_STATS_ENCODER_SOURCE
GF_STATS_ENCODER_SOURCE = 5
##\hideinitializer
#see GF_STATS_SINK
GF_STATS_SINK = 6


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
#see \ref GF_URL_REMOVED
GF_URL_REMOVED = -16
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
#see \ref GF_IP_UDP_TIMEOUT
GF_IP_UDP_TIMEOUT = -46
##\hideinitializer
#see \ref GF_AUTHENTICATION_FAILURE
GF_AUTHENTICATION_FAILURE = -50
##\hideinitializer
#see \ref GF_NOT_READY
GF_NOT_READY = -51
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

##notification is a setup error, the filter chain was never connected
GF_SETUP_ERROR=0
##notification is an error but keep the filter chain connected
GF_NOTIF_ERROR=1
##notification is an error and disconnect the filter chain
GF_NOTIF_ERROR_AND_DISCONNECT=2


##Do not flush session: everything is discarded, potentially breaking output files
GF_FS_FLUSH_NONE=0,
##Flush all pending data before closing sessions:  sources will be forced into end of stream and all emitted packets will be processed
GF_FS_FLUSH_ALL=1
##Stop session (resetting buffers) and flush pipeline
GF_FS_FLUSH_FAST=2

## @}

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

_libgpac.gf_fs_load_source.argtypes = [_gf_filter_session, c_char_p, c_char_p, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_source.restype = _gf_filter

_libgpac.gf_fs_load_destination.argtypes = [_gf_filter_session, c_char_p, c_char_p, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_destination.restype = _gf_filter

_libgpac.gf_fs_load_filter.argtypes = [_gf_filter_session, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_filter.restype = _gf_filter

_libgpac.gf_fs_new_filter.argtypes = [_gf_filter_session, c_char_p, c_uint, POINTER(c_int)]
_libgpac.gf_fs_new_filter.restype = _gf_filter


_libgpac.gf_fs_post_user_task.argtypes = [_gf_filter_session, c_void_p, py_object, c_char_p]
_libgpac.gf_fs_post_user_task.restype = c_int

_libgpac.gf_fs_is_last_task.argtypes = [_gf_filter_session]
_libgpac.gf_fs_is_last_task.restype = gf_bool

_libgpac.gf_fs_get_filters_count.argtypes = [_gf_filter_session]
_libgpac.gf_fs_get_filter.argtypes = [_gf_filter_session, c_int]
_libgpac.gf_fs_get_filter.restype = _gf_filter

_libgpac.gf_fs_abort.argtypes = [_gf_filter_session, c_int]

_libgpac.gf_fs_get_http_max_rate.argtypes = [_gf_filter_session]
_libgpac.gf_fs_get_http_rate.argtypes = [_gf_filter_session]
_libgpac.gf_fs_set_http_max_rate.argtypes = [_gf_filter_session, c_uint]

_libgpac.gf_fs_lock_filters.argtypes = [_gf_filter_session, gf_bool]
_libgpac.gf_fs_enable_reporting.argtypes = [_gf_filter_session, gf_bool]

_libgpac.gf_fs_print_stats.argtypes = [_gf_filter_session]
_libgpac.gf_fs_print_connections.argtypes = [_gf_filter_session]

_libgpac.gf_fs_fire_event.argtypes = [_gf_filter_session, _gf_filter, POINTER(FilterEvent), gf_bool]
_libgpac.gf_fs_fire_event.restype = gf_bool

_libgpac.gf_fs_is_supported_mime.argtypes = [_gf_filter_session, c_char_p]
_libgpac.gf_fs_is_supported_mime.restype = gf_bool

_libgpac.gf_fs_is_supported_mime.argtypes = [_gf_filter_session, c_char_p]
_libgpac.gf_fs_is_supported_mime.restype = gf_bool

_libgpac.gf_fs_is_supported_source.argtypes = [_gf_filter_session, c_char_p, c_char_p]
_libgpac.gf_fs_is_supported_source.restype = gf_bool

_libgpac.gf_pixel_fmt_sname.argtypes = [c_uint]
_libgpac.gf_pixel_fmt_sname.restype = c_char_p


@CFUNCTYPE(c_int, _gf_filter_session, c_void_p, POINTER(c_uint))
def fs_task_fun(sess, cbk, resched):
 obj = cast(cbk, py_object).value
 res = obj.execute()
 if res==None or res<0:
    obj.session.rem_task(obj)
    return 0
 resched.contents.value=res
 return 1



_libgpac.gf_fs_set_filter_creation_callback.argtypes = [_gf_filter_session, c_void_p, py_object, gf_bool]
@CFUNCTYPE(c_int, c_void_p, _gf_filter, gf_bool)
def on_filter_new_del(cbk, _filter, is_del):
 sess = cast(cbk, py_object).value
 f = sess._to_filter(_filter)
 if is_del:
    if hasattr(sess, 'on_filter_del'):
        sess.on_filter_del(f)
    sess._filters.remove(f)
 elif hasattr(sess, 'on_filter_new'):
    sess.on_filter_new(f)

 return 0


_libgpac.gf_fs_set_external_gl_provider.argtypes = [_gf_filter_session, c_void_p, py_object]
@CFUNCTYPE(c_int, c_void_p, gf_bool)
def on_gl_ctx_activate(cbk, set_active):
 sess = cast(cbk, py_object).value
 if hasattr(sess, 'on_gl_activate'):
    sess.on_gl_activate(set_active)
 return 0

##\endcond


## Task object for user callbacks from libgpac scheduler 
class FilterTask:
    ## constructor for tasks
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
    ## constructor for filter session - see \ref gf_fs_new
    #\param flags session flags (int)
    #\param blacklist list of blacklisted filters
    #\param nb_threads number of threads to use (int)
    #\param sched_type session scheduler type
    def __init__(self, flags=0, blacklist=None, nb_threads=0, sched_type=0):
        ##\cond private
        _libgpac.user_init = True
        self._sess = None
        if blacklist:
            self._sess = _libgpac.gf_fs_new(nb_threads, sched_type, flags, blacklist.encode('utf-8'))
        else:
            self._sess = _libgpac.gf_fs_new(nb_threads, sched_type, flags, None)
        if not self._sess:
            raise Exception('Failed to create new filter session')
        self._filters = []
        self._tasks = []
        #for now force sync user tasks and filter management to run on main thread, not tested otherwise
        _libgpac.gf_fs_set_filter_creation_callback(self._sess, on_filter_new_del, py_object(self), True)

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

    ## called whenever a new filter is added, typically used by classes deriving from FilterSession
    #\param _filter Filter object being added
    #\return
    def on_filter_new(self, _filter):
        pass

    ## called whenever a filter is destroyed, typically used by classes deriving from FilterSession
    #\param _filter Filter object being removed
    #\return
    def on_filter_del(self, _filter):
        pass

    ## called whenever a GL context must be activated, typically used by classes deriving from FilterSession
    #\param do_activate set to true if openGL context ust be activated for calling thread
    #\return
    def on_gl_activate(self, do_activate):
        return 0

    ## call this function to prevent openGL context creation in libgpac, delegating GL context management to the calling app
    #\return
    def external_opengl_provider(self):
        _libgpac.gf_fs_set_external_gl_provider(self._sess, on_gl_ctx_activate, py_object(self))

    ##\cond private
    def _to_filter(self, f):
        for i, a_f in enumerate(self._filters):
            if a_f._filter == f:
                return a_f
        _filter = Filter(self, f)
        self._filters.append(_filter)
        return _filter
    ##\endcond private

    ##run the session - see \ref gf_fs_run
    #\return
    def run(self):
        err = _libgpac.gf_fs_run(self._sess)
        if err<0: 
            raise Exception('Failed to run session: ' + e2s(err))

    ##load source filter - see \ref gf_fs_load_source
    #\param URL source URL to load
    #\param parentURL URL of parent resource for relative path resolution
    #\return new Filter object
    def load_src(self, URL, parentURL=None):
        errp = c_int(0)
        if parentURL:
            _filter = _libgpac.gf_fs_load_source(self._sess, URL.encode('utf-8'), None, parentURL.encode('utf-8'), byref(errp))
        else:
            _filter = _libgpac.gf_fs_load_source(self._sess, URL.encode('utf-8'), None, None, byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to load source filter ' + URL + ': ' + e2s(err))
        return self._to_filter(_filter)

    ##load destination filter - see \ref gf_fs_load_destination
    #\param URL source URL to load
    #\param parentURL URL of parent resource for relative path resolution
    #\return new Filter object
    def load_dst(self, URL, parentURL=None):
        errp = c_int(0)
        if parentURL:
            _filter = _libgpac.gf_fs_load_destination(self._sess, URL.encode('utf-8'), None, parentURL.encode('utf-8'), byref(errp))
        else:
            _filter = _libgpac.gf_fs_load_destination(self._sess, URL.encode('utf-8'), None, None, byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to load destination filter ' + URL + ': ' + e2s(err))
        return self._to_filter(_filter)


    ##load a filter - see \ref gf_fs_load_filter
    #\param fname filter name and options
    #\return new Filter object
    def load(self, fname):
        errp = c_int(0)
        _filter = _libgpac.gf_fs_load_filter(self._sess, fname.encode('utf-8'), byref(errp))
        err = errp.value
        if err<0: 
            raise Exception('Failed to load filter ' + fname + ': ' + e2s(err))
        return self._to_filter(_filter)

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
    def abort(self, flush=0):
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
    # \param _filter Filter to use as target
    # \param upstream if true, walks the chain towards the sink, otehrwise towards the source
    #\return
    def fire_event(self, evt, _filter=None, upstream=False):
        if _filter:
            return _libgpac.gf_fs_fire_event(self._sess, _filter._filter, byref(evt), upstream)
        return _libgpac.gf_fs_fire_event(self._sess, None, byref(evt), upstream)

    ##checks if a given mime is supported - see \ref gf_fs_is_supported_mime
    # \param mime mime type to check
    # \return true or false
    def is_supported_mime(self, mime):
        return _libgpac.gf_fs_is_supported_mime(self._sess, mime.encode('utf-8'))

    ##checks if a given source URL is supported - see \ref gf_fs_is_supported_source
    # \param url URL to check
    # \param parent parent URL for relative URLs
    # \return true or false
    def is_supported_source(self, url, parent=None):
        if parent:
            return _libgpac.gf_fs_is_supported_source(self._sess, url.encode('utf-8'), parent.encode('utf-8'))
        return _libgpac.gf_fs_is_supported_source(self._sess, url.encode('utf-8'), None)


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
        _libgpac.gf_fs_set_http_max_rate(self._sess, value)

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
_libgpac.gf_filter_set_source_restricted.argtypes = [_gf_filter, _gf_filter, c_char_p]
_libgpac.gf_filter_reconnect_output.argtypes = [_gf_filter, _gf_filter_pid]

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

_libgpac.gf_filter_pid_get_statistics.argtypes = [_gf_filter, POINTER(FilterPidStatistics), c_uint]

_libgpac.gf_props_type_is_enum.argtypes = [c_uint]
_libgpac.gf_props_type_is_enum.restype = c_int

_libgpac.gf_props_parse_enum.argtypes = [c_uint, c_char_p]
_libgpac.gf_props_parse_enum.restype = c_uint

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

_libgpac.gf_filter_require_source_id.argtypes = [_gf_filter]

_libgpac.gf_filter_probe_link.argtypes = [_gf_filter, c_uint, c_char_p, POINTER(c_char_p)]
_libgpac.gf_filter_get_possible_destinations.argtypes = [_gf_filter, c_int, POINTER(c_char_p)]


_libgpac.gf_httpout_send_request.argtypes = [c_void_p, py_object, c_uint, c_char_p, c_uint, POINTER(POINTER(c_char)), c_void_p, c_void_p, c_void_p, c_void_p]

@CFUNCTYPE(c_uint, c_void_p, c_ulonglong, c_ulonglong)
def httpout_cbk_throttle(cbk, done, total):
    obj = cast(cbk, py_object).value
    return obj.throttle(done, total)

@CFUNCTYPE(c_int, c_void_p, POINTER(c_ubyte), c_uint)
def httpout_cbk_read(cbk, buf, size):
    obj = cast(cbk, py_object).value
    if numpy_support:
        ar_data = np.ctypeslib.as_array(buf, shape=(size,))
        ar_data.flags.writeable=True
        return obj.read(ar_data, size)
    return obj.read(buf, size)

@CFUNCTYPE(c_uint, c_void_p, POINTER(c_ubyte), c_uint)
def httpout_cbk_write(cbk, buf, size):
    obj = cast(cbk, py_object).value
    if buf==None:
        obj.write(None, 0)
    else:
        if numpy_support:
            ar_data = np.ctypeslib.as_array(buf, shape=(size,))
            ar_data.flags.writeable=False
            obj.write(ar_data, size)
        else:
            obj.write(buf, size)
    return 0

@CFUNCTYPE(None, c_void_p, c_int)
def httpout_cbk_close(cbk, err):
    obj = cast(cbk, py_object).value
    obj._root.gc_exclude.remove(obj)
    if not obj._skip_close:
        obj.close(err)
    return 0

## \endcond

##
#  \defgroup pyhttpout_grp HTTP server bindings
#  \ingroup pyapi_grp
#  \brief Python API for libgpac httpout module.
#
# HTTP server requests can be monitored and/or handled in Python. This is achieved by calling \ref Filter.bind on the httpout filter using an object deriving from HTTPOutRequest.
#
# The requests can either be handled by GPAC as usual, or by the python script.
# @{

## HTTP request handler object
class HTTPOutRequest:
    ## Constructor
    def __init__(self):
        ## \cond private
        self._session=None
        self._skip_close=0
        ## \endcond

        ## reply code
        ## A reply code of 0 means httpout will serve the resource as usual
        self.reply=0

        ## reply body
        ## - if reply is 0, this can be set to a file path to serve. If not set, usual URL resolving of httpout is done
        ## - if reply is not 0, this can be set to a string containing the body. To deliver a binary file or a large file, use read function
        self.body=None

        ## list of headers to add
        ## even values are header names, odd values are header values
        self.headers_out=[]

    ## throttle the connection - if not overriden by subclass, not used
    #\param done amount of bytes of ressource sent
    #\param total total size of ressource
    #\return a timeout in microseconds, or 0 to process immediately
    def throttle(self, done, total):
        return 0

    ## read data for the request - if not overriden by subclass, not used
    #\param buf NP array (or c_ubyte pointer if no numpy support) to write data to
    #\param size size of array to fill
    #\return amount of bytes read, negative value means no data available yet, 0 means end of file
    def read(self, buf, size):
        return 0

    ## write data for the request (PUT/POST) - if not overriden by subclass, not used
    #\param buf NP array (or c_ubyte pointer if no numpy support) containing data from client
    #\param size number of valid bytes in the array
    #\return
    def write(self, buf, size):
        pass

    ## close callback for the request - if not overriden by subclass, not used
    #\param reason GPAC error code of the end of session
    #\return
    def close(self, reason):
        pass

    ## callback for the request - this shoulld be overriden by subclass, default behaviour being to delegate to GPAC
    #\param method HTTP method used, as string
    #\param url URL of the HTTP request
    #\param auth_code Authentication reply code - requests are pre-identified using GPAC credentials: a value of 401 indicates no identification, 200 indicates identification OK, 403 indicates failure
    #\param headers list of headers of input request, even values are header names, odd values are header values
    #\return
    def on_request(self, method, url, auth_code, headers):
        self.send()

    ## Send the reply to the client. This can be called aither upon \ref on_request or later (asynchronously)
    #\return
    def send(self):
        hdrs = None
        nb_hdrs=len(self.headers_out)
        if nb_hdrs:
            hdrs = (POINTER(c_char)*nb_hdrs)()
            i=0
            for str in self.headers_out:
                hdrs[i] = create_string_buffer(str.encode('utf-8'))
                i+=1
        body = None;
        if self.body:
            body = self.body.encode('utf-8')
        ret = _libgpac.gf_httpout_send_request(self._session, py_object(self), self.reply, body, nb_hdrs, hdrs,
            None if type(self).throttle == HTTPOutRequest.throttle else httpout_cbk_throttle,
            None if type(self).read == HTTPOutRequest.read else httpout_cbk_read,
            None if type(self).write == HTTPOutRequest.write else httpout_cbk_write,
            httpout_cbk_close
        )
        ## \cond private
        self._skip_close = True if type(self).close == HTTPOutRequest.close else False;
        ## \endcond

## @}

## \cond private

_libgpac.gf_filter_bind_httpout_callbacks.argtypes = [_gf_filter, py_object, c_void_p]
@CFUNCTYPE(c_int, c_void_p, c_void_p, c_char_p, c_char_p, c_uint, c_uint, POINTER(c_char_p))
def httpout_on_request(cbk, sess, method, url, auth_code, nb_hdrs, hdrs):
    obj = cast(cbk, py_object).value
    _ck = obj.__class__
    req = _ck()
    req._session = sess
    req._root = obj
    obj.gc_exclude.append(req)
    headers=[]
    for i in range(nb_hdrs):
        headers.append(hdrs[i].decode('utf-8'))
    req.on_request(method.decode('utf-8'), url.decode('utf-8'), auth_code, headers);
    return 0


_libgpac.gf_filter_bind_dash_algo_callbacks.argtypes = [_gf_filter, py_object, c_void_p, c_void_p, c_void_p, c_void_p]
@CFUNCTYPE(c_int, c_void_p, c_uint)
def dash_period_reset(cbk, reset_type):
    obj = cast(cbk, py_object).value
    if hasattr(obj, 'on_period_reset'):
        obj.on_period_reset(reset_type)
    if not reset_type:
        obj.groups = []
    return 0

class DASHQualityInfoNat(Structure):
    _fields_ = [
        ("bandwidth", c_uint),
        ("ID", c_char_p),
        ("mime", c_char_p),
        ("codec", c_char_p),
        ("width", c_uint),
        ("height", c_uint),
        ("interlaced", gf_bool),
        ("fps_num", c_uint),
        ("fps_den", c_uint),
        ("sar_num", c_uint),
        ("sar_den", c_uint),
        ("sample_rate", c_uint),
        ("nb_channels", c_uint),
        ("disabled", gf_bool),
        ("is_selected", gf_bool),
        ("ast_offset", c_double),
        ("avg_duration", c_double),
        ("sizes", _gf_list),
    ]

class DASHByteRange(Structure):
    _fields_ = [
        ("start", c_ulonglong),
        ("end", c_ulonglong)
    ]

class DASHSegURL(Structure):
    _fields_ = [
        ("media", c_char_p),
        ("media_range", POINTER(DASHByteRange))
            #ignore the rest
    ]


_libgpac.gf_dash_group_get_num_qualities.argtypes = [c_void_p, c_uint]
_libgpac.gf_dash_get_period_duration.argtypes = [c_void_p]
_libgpac.gf_dash_group_get_quality_info.argtypes = [c_void_p, c_uint, c_uint, POINTER(DASHQualityInfoNat) ]

_libgpac.gf_dash_group_get_srd_info.argtypes = [c_void_p, c_uint, POINTER(c_uint), POINTER(c_uint), POINTER(c_uint), POINTER(c_uint), POINTER(c_uint), POINTER(c_uint), POINTER(c_uint) ]

_libgpac.gf_list_count.argtypes = [c_void_p]
_libgpac.gf_list_count.restype = c_uint

_libgpac.gf_list_get.argtypes = [c_void_p, c_uint]
_libgpac.gf_list_get.restype = c_void_p


## \endcond

## 
#  \defgroup pydash_grp DASH custom algorithm
#  \ingroup pyapi_grp
#  \brief Python API for libgpac DASH client.
#
# DASH client logic can be controlled in Python. This is achieved by calling \ref Filter.bind on the dashin filter using an object deriving from DASHCustomAlgorithm.
# @{

## DASH media quality information (Representation info)
class DASHQualityInfo:
    ## \cond private
    def __init__(self, qinfon):
    ## \endcond
        ## bandwidth in bits per second
        self.bandwidth = qinfon.bandwidth
        ## ID (representation ID in DASH)
        self.ID = qinfon.ID.decode('utf-8')
        ## MIME type
        self.mime = qinfon.mime.decode('utf-8')
        ## codec parameter string
        self.codec = qinfon.codec.decode('utf-8')
        ## width in pixels, 0 if not visual
        self.width = qinfon.width
        ## height in pixels, 0 if not visual
        self.height = qinfon.height
        ## interlaced flag, false 0 if not visual
        self.interlaced = qinfon.interlaced
        ## Frame Rate (Fraction), 0/0 if not visual
        self.fps = Fraction(qinfon.fps_num, qinfon.fps_den)
        ## Sample Aspect Ration (Fraction), 0/0 if not visual
        self.sar = Fraction(qinfon.sar_num, qinfon.sar_den)
        ## Samplerate, 0 if not audio
        self.sample_rate = qinfon.sample_rate
        ## Number of channels, 0 if not audio
        self.nb_channels = qinfon.nb_channels
        ## set to true if quality is disabled (no playback support)
        self.disabled = qinfon.disabled
        ## set to true if quality is selected
        self.is_selected = qinfon.is_selected
        ## AST offset for DASH low latency mode, 0 otherwise
        self.ast_offset = qinfon.ast_offset
        ## Average segment duration in seconds, 0 if unknown
        self.avg_duration = qinfon.avg_duration
        ## list of segment sizes for VoD cases, None otherwise or if unknown
        self.sizes = None
        ## \cond private
        if qinfon.sizes == None:
            return
        count = _libgpac.gf_list_count(qinfon.sizes)
        self.sizes = []
        for i in range(count):
            surl = cast(_libgpac.gf_list_get(qinfon.sizes, i), POINTER(DASHSegURL)).contents
            self.sizes.append( surl.media_range.contents.end - surl.media_range.contents.start + 1)
        ## \endcond priv

##DASH Spatial Relation Descriptor object, used for tiling
class DASHSRD:
    ## \cond private
    def __init__(self, id, x, y, w, h, fw, fh):
    ## \endcond
        ## ID of SRD source - all SRD with same source describe the same video composition, possibly with different grid sizes
        self.id = id
        ## X coordinate of SRD for this tile
        self.x = x
        ## Y coordinate of SRD for this tile
        self.y = y
        ## width of SRD for this tile - 0 for tile base track
        self.w = w
        ## height of SRD for this tile - 0 for tile base track
        self.h = h
        ## total width of SRD descriptor for this tile
        self.fw = fw
        ## total height of SRD descriptor for this tile
        self.fh = fh

##\cond private
def make_srd(dashptr, groupidx):
    srd_id=c_uint(0)
    srd_x=c_uint(0)
    srd_y=c_uint(0)
    srd_w=c_uint(0)
    srd_h=c_uint(0)
    srd_fw=c_uint(0)
    srd_fh=c_uint(0)
    _libgpac.gf_dash_group_get_srd_info(dashptr, groupidx, byref(srd_id), byref(srd_x), byref(srd_y), byref(srd_w), byref(srd_h), byref(srd_fw), byref(srd_fh) )
    if not srd_fw.value or not srd_fh.value:
        return None
    return DASHSRD(srd_id.value, srd_x.value, srd_y.value, srd_w.value, srd_h.value, srd_fw.value, srd_fh.value)

##\endcond

## DASH group object
class DASHGroup:
    ## \cond private
    def __init__(self, ptr_dash, groupidx):
    ## \endcond
        ## Index of group, as used in callbacks
        self.idx = groupidx
        ## List of DASHQualityInfo for group
        self.qualities = []
        ## period duration in milliseconds, 0 if unknown
        self.duration = _libgpac.gf_dash_get_period_duration(ptr_dash)
        ## SRD object or None if no SRD defined
        self.SRD = make_srd(ptr_dash, groupidx)
        ## \cond private
        self._dash = ptr_dash
        nb_qualities = _libgpac.gf_dash_group_get_num_qualities(ptr_dash, groupidx)
        for i in range(nb_qualities):
            qinfo = DASHQualityInfoNat()
            _libgpac.gf_dash_group_get_quality_info(ptr_dash, groupidx, i, byref(qinfo))

            self.qualities.append( DASHQualityInfo(qinfo) )
        ## \endcond

## DASH groups statistics object
class DASHGroupStatistics(Structure):
    ## \cond private
    def __init__(self):
    ## \endcond
        ##download rate of last segment in bits per second, divided by current playback speed
        self.download_rate = 0
        ##size of last segment in bytes
        self.filesize = 0
        ##current playback speed
        self.speed = 0
        ##max playback speed based on associated codec runtime statistics
        self.max_available_speed = 0
        ##display width in pixels of object
        self.display_width = 0
        ##display height in pixels of object
        self.display_height = 0
        ##index of current quality or of last downloaded segment quality if previous was skipped
        self.active_quality_idx = 0
        ##minimum buffer in milliseconds, below witch rebuffer occurs
        self.buffer_min = 0
        ##maximum buffer in milliseconds, algorithm should not fill more than this
        self.buffer_max = 0
        ##current buffer in milliseconds
        self.buffer = 0
        ##degradation hint, 0 means no degradation, 100 means tile completely hidden
        self.quality_degradation_hint = 0
        ##cumulated download rate of all active groups in bytes per seconds - 0 means all files are local
        self.total_rate = 0

    ## \cond private
    _fields_ = [
        ("download_rate", c_uint),
        ("filesize", c_uint),
        ("speed", c_double),
        ("max_available_speed", c_double),
        ("display_width", c_uint),
        ("display_height", c_uint),
        ("active_quality_idx", c_uint),
        ("buffer_min", c_uint),
        ("buffer_max", c_uint),
        ("buffer", c_uint),
        ("degradation_hint", c_uint),
        ("total_rate", c_uint)
    ]
    def __str__(self):
        res = 'active_quality_idx ' + str(self.active_quality_idx)
        res += ' rate ' + str(self.download_rate) + ' speed ' + str(self.speed) + ' max_speed ' + str(self.max_available_speed)
        res += ' display_width ' + str(self.display_width) + ' display_height ' + str(self.display_height)
        res += ' buffer_min ' + str(self.buffer_min) + ' buffer_max ' + str(self.buffer_max) + ' buffer ' + str(self.buffer)
        return res
    ## \endcond

## DASH group current segment download statistics object
class DASHGroupDownloadStatistics(Structure):
    ## \cond private
    def __init__(self):
    ## \endcond
        ##download rate of last segment in bits per second
        self.bits_per_sec = 0
        ##total number of bytes in segment
        self.total_bytes = 0
        ##number of downloaded bytes from segment (starting from first byte)
        self.bytes_done = 0
        ##number of microseconds elapsed since segment was scheduled for download
        self.time_since_start = 0
        ##current buffer length in milliseconds
        self.buffer_dur = 0
        ##duration of segment being downloaded, in milliseconds - 0 if unknown
        self.current_seg_dur = 0

    ## \cond private
    _fields_ = [
        ("bits_per_sec", c_uint),
        ("total_bytes", c_ulonglong),
        ("bytes_done", c_ulonglong),
        ("time_since_start", c_ulonglong),
        ("buffer_dur", c_uint),
        ("current_seg_dur", c_uint),
    ]

    def __str__(self):
        res = 'bits_per_sec ' + str(self.bits_per_sec)
        res += ' - total_bytes ' + str(self.total_bytes) + ' bytes_done ' + str(self.bytes_done)
        res += ' - time_since_start ' + str(self.time_since_start) + ' us - buffer_dur ' + str(self.buffer_dur) + ' ms - current_seg_dur ' + str(self.current_seg_dur) + ' ms'
        return res
    ## \endcond



## DASH custom algo
# Upon successful binding to the dashin filter, the object will be assigned a list member called `groups`, containing the declared group for the active period
class DASHCustomAlgorithm:

    ##Callback (optional) called upon a period reset.
    #\param reset_type indicate the type of period reset. Values can be:
    #   - 0: end of period (groups are no longer valid)
    #   - 1: start of a static period
    #   - 2: start of a dynamic (live) period
    #\return 
    def on_period_reset(self, reset_type):
        pass

    ##Callback (optional) called when a new group (adaptation set) is created
    #\param group the newly created \ref DASHGroup
    #\return 
    def on_new_group(self, group):
        pass


    ##Callback (mandatory) called at the end of the segment download to perform rate adaptation
    #\param group the \ref DASHGroup on which to perform adaptation
    #\param base_group the associated base \ref DASHGroup (tiling only), or None if no base group
    #\param force_low_complexity indicates that the client would like a lower complexity (typically because it is dropping frames)
    #\param stats the \ref DASHGroupStatistics  for the downloaded segment
    #\return value can be:
    # - new quality index,
    # - -1 to take no decision
    # - -2 to disable quality (debug, will drop segment)
    # - other negative values are handled as error
    def on_rate_adaptation(self, group, base_group, force_low_complexity, stats):
        pass

    ##Callback (optional) called on regular basis during a segment download
    #\param group the \ref DASHGroup associated with the current download
    #\param stats the \ref DASHGroupDownloadStatistics for the download
    #\return value can be:
    #   - `-1` to continue download
    #   - `-2` to abort download but without retrying to downloading the same segment at lower quality
    #   - the index of the new quality to download for the same segment index (same time)
    def on_download_monitor(self, group, stats):
        pass

## @}


## \cond private


@CFUNCTYPE(c_int, c_void_p, c_uint, c_void_p)
def dash_group_new(cbk, groupidx, _dashobj):
 obj = cast(cbk, py_object).value
 if not hasattr(obj, 'on_new_group'):
    return 0
 new_group = DASHGroup(_dashobj, groupidx)
 obj.groups.append(new_group)
 obj.on_new_group(new_group)
 return 0



@CFUNCTYPE(c_int, c_void_p, c_uint, c_uint, gf_bool, POINTER(DASHGroupStatistics))
def dash_rate_adaptation(cbk, groupidx, base_groupidx, force_low_complex, stats):
 obj = cast(cbk, py_object).value
 group = None
 base_group = None
 for i in range(len(obj.groups)):
    if obj.groups[i].idx==groupidx:
        group = obj.groups[i]
    if obj.groups[i].idx==base_groupidx:
        base_group = obj.groups[i]

 return obj.on_rate_adaptation(group, base_group, force_low_complex, stats.contents)


@CFUNCTYPE(c_int, c_void_p, c_uint, POINTER(DASHGroupDownloadStatistics))
def dash_download_monitor(cbk, groupidx, stats):
 obj = cast(cbk, py_object).value
 group = None
 for i in range(len(obj.groups)):
    if obj.groups[i].idx==groupidx:
        group = obj.groups[i]
        break

 return obj.on_download_monitor(group, stats.contents)


def _prop_to_python(pname, prop):
    ptype = prop.type

    if ptype==GF_PROP_SINT:
        return prop.value.sint
    if ptype==GF_PROP_UINT:
        if pname=="StreamType":
            return _libgpac.gf_stream_type_name(prop.value.uint).decode('utf-8')
        if pname=="PixelFormat":
            return _libgpac.gf_pixel_fmt_sname(prop.value.uint).decode('utf-8')
        if pname=="CodecID":
            cid = _libgpac.gf_codecid_file_ext(prop.value.uint).decode('utf-8')
            names=cid.split('|')
            return names[0]
        return prop.value.uint

    if _libgpac.gf_props_type_is_enum(ptype):
        pname = _libgpac.gf_props_enum_name(ptype, prop.value.uint)
        return pname.decode('utf-8')

    if ptype==GF_PROP_4CC:
        return _libgpac.gf_4cc_to_str(prop.value.uint).decode('utf-8')
    if ptype==GF_PROP_LSINT:
        return prop.value.longsint
    if ptype==GF_PROP_LUINT:
        return prop.value.longuint
    if ptype==GF_PROP_BOOL:
        return prop.value.boolean
    if ptype==GF_PROP_FRACTION:
        return prop.value.frac
    if ptype==GF_PROP_FRACTION64:
        return prop.value.lfrac
    if ptype==GF_PROP_FLOAT:
        return prop.value.fnumber
    if ptype==GF_PROP_DOUBLE:
        return prop.value.number
    if ptype==GF_PROP_VEC2I:
        return prop.value.vec2i
    if ptype==GF_PROP_VEC2:
        return prop.value.vec2
    if ptype==GF_PROP_VEC3I:
        return prop.value.vec3i
    if ptype==GF_PROP_VEC4I:
        return prop.value.vec4i
    if ptype==GF_PROP_STRING or ptype==GF_PROP_STRING_NO_COPY or ptype==GF_PROP_NAME:
        return prop.value.string.decode('utf-8')
    if ptype==GF_PROP_DATA or ptype==GF_PROP_DATA_NO_COPY or ptype==GF_PROP_CONST_DATA:
        return prop.value.data
    if ptype==GF_PROP_POINTER:
        return prop.value.ptr
    if ptype==GF_PROP_STRING_LIST:
        res = []
        for i in range(prop.value.string_list.nb_items):
            val = prop.value.string_list.vals[i].decode('utf-8')
            res.append(val)
        return res
    if ptype==GF_PROP_UINT_LIST:
        res = []
        for i in range(prop.value.uint_list.nb_items):
            val = prop.value.uint_list.vals[i]
            res.append(val)
        return res
    if ptype==GF_PROP_4CC_LIST:
        res = []
        for i in range(prop.value.uint_list.nb_items):
            val = _libgpac.gf_4cc_to_str(prop.value.uint).decode('utf-8')
            res.append(val)
        return res
    if ptype==GF_PROP_SINT_LIST:
        res = []
        for i in range(prop.value.uint_list.nb_items):
            val = prop.value.sint_list.vals[i]
            res.append(val)
        return res
    if ptype==GF_PROP_VEC2I_LIST:
        res = []
        for i in range(prop.value.v2i_list.nb_items):
            val = prop.value.v2i_list.vals[i]
            res.append(val)
        return res

    raise Exception('Unknown property type ' + str(ptype))

## \endcond


## filter object
class Filter:
    ## \cond  private
    def __init__(self, session, _filter):
        self._filter = _filter
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
	#\param propagate_mask flags indicating if updates must be send to up-chain filters (2), down-chain filters (1), both (3) or only on filter (0)
    #\return
    def update(self, name, value, propagate_mask=0):
        _libgpac.gf_fs_send_update(None, None, self._filter, name.encode('utf-8'), value.encode('utf-8'), propagate_mask)

    ## set a given filter as source for this filter - see \ref gf_filter_set_source
    #
    #\note Setting a source will force the filter session linker to run in explicit linking mode.
    #\param f source Filter
    #\param link_args link options (string)
    #\return
    def set_source(self, f, link_args=None):
        if f:
            _libgpac.gf_filter_set_source(self._filter, f._filter, None if not link_args else link_args.encode('utf-8'))

    ## set a given filter as restricted source for this filter - see \ref gf_filter_set_source_restricted
    #\param f source Filter
    #\param link_args link options (string)
    #\return
    def set_source_restricted(self, f, link_args=None):
        if f:
            _libgpac.gf_filter_set_source_restricted(self._filter, f._filter, None if not link_args else link_args.encode('utf-8'))


    ## insert a given filter after this filter - see \ref gf_filter_set_source and \ref gf_filter_reconnect_output
    #\param f  Filter to insert
    #\param opid index of output pid to reconnect, -1 for all pids
    #\param link_args link options (string)
    #\return
    def insert(self, f, opid=-1, link_args=None):
        if f:
            _libgpac.gf_filter_set_source(f._filter, self._filter, None if not link_args else link_args.encode('utf-8'))
            pid = None
            if opid>=0:
                pid = _libgpac.gf_filter_get_opid(self._filter, opid)
                if not pid:
                    raise Exception('No output PID with index ' + str(opid) + ' in filter ' + self.name )
            err = _libgpac.gf_filter_reconnect_output(f._filter, pid)
            if err<0:
                raise Exception('Failed to reconnect output: ' + e2s(err))

    ## reconnect the filter output - see \ref gf_filter_set_source and \ref gf_filter_reconnect_output
    #\param opid index of output pid to reconnect, -1 for all pids
    #\return
    def reconnect(self, opid=-1):
        pid = None
        if opid>=0:
            pid = _libgpac.gf_filter_get_opid(self._filter, opid)
            if not pid:
                raise Exception('No output PID with index ' + str(opid) + ' in filter ' + self.name )
        err = _libgpac.gf_filter_reconnect_output(self._filter, pid)
        if err<0:
            raise Exception('Failed to reconnect output: ' + e2s(err))

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
        return None

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

    ##Gets the statistics of an input pid of filter - see \ref gf_filter_pid_get_statistics
    #\param idx index of input pid
    #\param mode search mode for stats cf \ref GF_FilterPidStatsLocation
    #\return FilterPidStatistics object
    def ipid_stats(self, idx, mode=0):
        pid = _libgpac.gf_filter_get_ipid(self._filter, idx)
        if not pid:
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        stats = FilterPidStatistics()
        err = _libgpac.gf_filter_pid_get_statistics(pid, byref(stats), mode)
        if err<0:
            raise Exception('Failed to fetch filter pid stats: ' + e2s(err))
        return stats

    ##Gets the statistics of an output pid of filter - see \ref gf_filter_pid_get_statistics
    #\param idx index of output pid
    #\param mode search mode for stats cf \ref GF_FilterPidStatsLocation
    #\return FilterPidStatistics object
    def opid_stats(self, idx, mode=0):
        pid = _libgpac.gf_filter_get_opid(self._filter, idx)
        if not pid:
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        stats = FilterPidStatistics()
        err = _libgpac.gf_filter_pid_get_statistics(pid, byref(stats), mode)
        if err<0:
            raise Exception('Failed to fetch filter pid stats: ' + e2s(err))
        return stats

    ##gets the filter at the source of an input pid
    #\param idx index of input PID
    #\return Filter or None if error
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
            _filter = self._session._to_filter(f)
            res.append(_filter)
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

    ##Gets the statistics of a filter - see \ref gf_filter_get_stats
    #\return FilterStats object
    def get_statistics(self):
        stats = FilterStats()
        err = _libgpac.gf_filter_get_stats(self._filter, byref(stats))
        if err<0: 
            raise Exception('Failed to fetch filter stats: ' + e2s(err))
        return stats

    ##enforces sourceID to be present for output pids of this filter - see \ref gf_filter_require_source_id
    #\return
    def require_source_id(self):
        _libgpac.gf_filter_require_source_id(self._filter)
        return

    ##Resolves link from given output pid of filter to a filter description. The described filter is not loaded in the graph - see \ref gf_filter_probe_link
    #\param opid_idx 0-based index of the output pid
    #\param name filter description to link to; this can be any filter description
    #\return None if no possible link or a list containing the filters in the resolved chain from current filter to destination
    def probe_link(self, opid_idx, name):
        links = c_char_p(0)
        ret = _libgpac.gf_filter_probe_link(self._filter, opid_idx, name.encode("utf-8"), byref(links))
        if ret<0:
            return None
        if not links:
            return None
        link_list = links.value.decode('utf-8').split(",")
        _libgpac.gf_url_free(links)
        return link_list

    ##Gets all possible destination filter for this filter or one of its output PID - see \ref gf_filter_get_possible_destinations
    #\param opid_idx 0-based index of the output pid, use -1 to check all output pids
    #\return None if no possible connections to known filter, or a list containing all possible direct connections
    def get_destinations(self, opid_idx):
        dests = c_char_p(0)
        ret = _libgpac.gf_filter_get_possible_destinations(self._filter, opid_idx, byref(dests))
        if ret<0:
            return None
        if not dests:
            return None
        dests_list = dests.value.decode('utf-8').split(",")
        _libgpac.gf_url_free(dests)
        return dests_list

    ##\cond private
    def _bind_dash_algo(self, object):
        if not hasattr(object, 'on_rate_adaptation'):
            raise Exception('Missing on_rate_adaptation member function on object, cannot bind')
        object.groups = []
        if hasattr(object, 'on_download_monitor'):
            err = _libgpac.gf_filter_bind_dash_algo_callbacks(self._filter, py_object(object), dash_period_reset, dash_group_new, dash_rate_adaptation, dash_download_monitor)
        else:
            err = _libgpac.gf_filter_bind_dash_algo_callbacks(self._filter, py_object(object), dash_period_reset, dash_group_new, dash_rate_adaptation, None)
        if err<0: 
            raise Exception('Failed to bind dash algo: ' + e2s(err))
        return 0

    def _bind_httpout(self, object):
        if not hasattr(object, 'on_request'):
            raise Exception('Missing on_request member function on object, cannot bind')
        object.sessions = []
        err = _libgpac.gf_filter_bind_httpout_callbacks(self._filter, py_object(object), httpout_on_request)
        if err<0:
            raise Exception('Failed to bind httpout: ' + e2s(err))
        object.gc_exclude=[]
        return 0
    ##\endcond private


    ## \brief binds a given object to the filter
    #
    #Binds the given object to the underlying filter for callbacks override - only supported by DASH demuxer for the current time
    #
    #For DASH, the object must derive from or implement the methods of the \ref DASHCustomAlgorithm class:
    #
    #\param object object to bind
    #\return
    def bind(self, object):
        if self.name=="dashin":
            return self._bind_dash_algo(object)
        if self.name=="httpout":
            return self._bind_httpout(object)
        raise Exception('No possible binding to filter class ' + self.name)

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

_libgpac.gf_cicp_color_primaries_name.argtypes = [c_uint]
_libgpac.gf_cicp_color_primaries_name.restype = c_char_p
_libgpac.gf_cicp_color_transfer_name.argtypes = [c_uint]
_libgpac.gf_cicp_color_transfer_name.restype = c_char_p
_libgpac.gf_cicp_color_matrix_name.argtypes = [c_uint]
_libgpac.gf_cicp_color_matrix_name.restype = c_char_p

_libgpac.gf_cicp_parse_color_primaries.argtypes = [c_char_p]
_libgpac.gf_cicp_parse_color_primaries.restype = c_uint
_libgpac.gf_cicp_parse_color_transfer.argtypes = [c_char_p]
_libgpac.gf_cicp_parse_color_transfer.restype = c_uint
_libgpac.gf_cicp_parse_color_matrix.argtypes = [c_char_p]
_libgpac.gf_cicp_parse_color_matrix.restype = c_uint



def _make_prop(prop4cc, propname, prop, custom_type=0):
    prop_val = PropertyValue()
    if prop4cc==0:
        if not custom_type:
            prop_val.type = GF_PROP_STRING
            prop_val.value.string = str(prop).encode('utf-8')
            return prop_val
        ptype = custom_type
    else:
        ptype = _libgpac.gf_props_4cc_get_type(prop4cc)

    prop_val.type = ptype
    if propname=="StreamType":
        prop_val.value.uint = _libgpac.gf_stream_type_by_name(prop.encode('utf-8'))
        return prop_val
    elif propname=="CodecID":
        prop_val.value.uint = _libgpac.gf_codecid_parse(prop.encode('utf-8'))
        return prop_val

    if ptype==GF_PROP_SINT:
        prop_val.value.sint = prop
    elif ptype==GF_PROP_UINT:
        prop_val.value.uint = prop
    elif ptype==GF_PROP_4CC:
        prop_val.value.uint = _libgpac.gf_4cc_parse(prop.encode('utf-8'))
    elif ptype==GF_PROP_LSINT:
        prop_val.value.longsint = prop
    elif ptype==GF_PROP_LUINT:
        prop_val.value.longuint = prop
    elif ptype==GF_PROP_BOOL:
        prop_val.value.boolean = prop
    elif ptype==GF_PROP_FRACTION:
        if hasattr(prop, 'den') and hasattr(prop, 'num'):
            prop_val.value.frac.num = prop.num
            prop_val.value.frac.den = prop.den
        elif is_integer(prop):
            prop_val.value.frac.num = prop
            prop_val.value.frac.den = 1
        else:
            prop_val.value.frac.num = 1000*prop
            prop_val.value.frac.den = 1000
    elif ptype==GF_PROP_FRACTION64:
        if hasattr(prop, 'den') and hasattr(prop, 'num'):
            prop_val.value.lfrac.num = prop.num
            prop_val.value.lfrac.den = prop.den
        elif is_integer(prop):
            prop_val.value.lfrac.num = prop
            prop_val.value.lfrac.den = 1
        else:
            prop_val.value.lfrac.num = 1000000*prop
            prop_val.value.lfrac.den = 1000000
    elif ptype==GF_PROP_FLOAT:
        prop_val.value.fnumber = prop
    elif ptype==GF_PROP_DOUBLE:
        prop_val.value.number = prop
    elif ptype==GF_PROP_VEC2I:
        if hasattr(prop, 'x') and hasattr(prop, 'y'):
            prop_val.value.vec2i.x = prop.x
            prop_val.value.vec2i.y = prop.y
        else:
            raise Exception('Invalid property value for vec2i: ' + str(prop))
    elif ptype==GF_PROP_VEC2:
        if hasattr(prop, 'x') and hasattr(prop, 'y'):
            prop_val.value.vec2.x = prop.x
            prop_val.value.vec2.y = prop.y
        else:
            raise Exception('Invalid property value for vec2: ' + str(prop))
    elif ptype==GF_PROP_VEC3I:
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z'):
            prop_val.value.vec3i.x = prop.x
            prop_val.value.vec3i.y = prop.y
            prop_val.value.vec3i.z = prop.z
        else:
            raise Exception('Invalid property value for vec3i: ' + str(prop))
    elif ptype==GF_PROP_VEC4I:
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z') and hasattr(prop, 'w'):
            prop_val.value.vec4i.x = prop.x
            prop_val.value.vec4i.y = prop.y
            prop_val.value.vec4i.z = prop.z
            prop_val.value.vec4i.w = prop.w
        else:
            raise Exception('Invalid property value for vec4i: ' + str(prop))
    elif ptype==GF_PROP_STRING or ptype==GF_PROP_STRING_NO_COPY or ptype==GF_PROP_NAME:
        prop_val.value.string = str(prop).encode('utf-8')
    elif ptype==GF_PROP_DATA or ptype==GF_PROP_DATA_NO_COPY or ptype==GF_PROP_CONST_DATA:
        raise Exception('Setting data property from python not yet implemented !')
    elif ptype==GF_PROP_POINTER:
        raise Exception('Setting pointer property from python not yet implemented !')
    elif ptype==GF_PROP_STRING_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.string_list.nb_items = len(prop)
        prop_val.value.string_list.vals = (POINTER(c_char) * len(prop))()
        i=0
        for str in list:
            prop_val.value.string_list.vals[i] = create_string_buffer(str.encode('utf-8'))
            i+=1
    elif ptype==GF_PROP_UINT_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.uint_list.nb_items = len(prop)
        prop_val.value.uint_list.vals = (c_uint * len(prop))(*prop)
    elif ptype==GF_PROP_4CC_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.uint_list.nb_items = len(prop)
        prop_val.value.uint_list.vals = (c_uint * len(prop))
        i=0
        for str in list:
            prop_val.value.uint_list.vals[i] = _libgpac.gf_4cc_parse( str.encode('utf-8') )
            i+=1
    elif ptype==GF_PROP_SINT_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.sint_list.nb_items = len(prop)
        prop_val.value.sint_list.vals = (c_int * len(prop))(*prop)
    elif ptype==GF_PROP_VEC2I_LIST:
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.sint_list.nb_items = len(prop)
        prop_val.value.v2i_list.vals = (GF_PropVec2i * len(prop))
        for i in range (len(prop)):
            prop_val.value.v2i_list.vals[i].x = prop[i].x
            prop_val.value.v2i_list.vals[i].y = prop[i].y

    elif _libgpac.gf_props_type_is_enum(ptype):
        prop_val.value.uint = _libgpac.gf_props_parse_enum(ptype, prop.encode('utf-8'))
        return prop_val
    else:
        raise Exception('Unsupported property type ' + str(ptype) )

    return prop_val


_libgpac.gf_filter_set_rt_udta.argtypes = [_gf_filter, py_object]
_libgpac.gf_filter_get_rt_udta.argtypes = [_gf_filter]
_libgpac.gf_filter_get_rt_udta.restype = c_void_p

_libgpac.gf_filter_pid_set_udta.argtypes = [_gf_filter, py_object]
_libgpac.gf_filter_pid_get_udta.argtypes = [_gf_filter]
_libgpac.gf_filter_pid_get_udta.restype = c_void_p

_libgpac.gf_filter_pid_send_event.argtypes = [_gf_filter_pid, POINTER(FilterEvent)]


_libgpac.gf_filter_set_configure_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter, _gf_filter_pid, gf_bool)
def filter_cbk_configure(_f, _pid, is_remove):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    _filter = cast(obj, py_object).value
    obj = _libgpac.gf_filter_pid_get_udta(_pid)
    if obj:
        pid_obj = cast(obj, py_object).value
    else:
        pid_obj=None

    first=True
    if pid_obj:
        first=False
    else:
        pid_obj = FilterPid(_filter, _pid, True)
        _libgpac.gf_filter_pid_set_udta(_pid, py_object(pid_obj))
    res = _filter.configure_pid(pid_obj, is_remove)

    if is_remove:
        _libgpac.gf_filter_pid_set_udta(_pid, None)
        if pid_obj in _filter.ipids:
            _filter.ipids.remove(pid_obj)
        return res

    if first:
        _filter.ipids.append(pid_obj)

    return res


_libgpac.gf_filter_set_process_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter)
def filter_cbk_process(_f):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    _filter = cast(obj, py_object).value
    return _filter.process()

_libgpac.gf_filter_set_process_event_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter, POINTER(FilterEvent) )
def filter_cbk_process_event(_f, _evt):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    _filter = cast(obj, py_object).value
    res = _filter.process_event(_evt.contents)
    if res:
        return 1
    return 0

_libgpac.gf_filter_set_probe_data_cbk.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, POINTER(c_ubyte), c_uint, POINTER(c_uint) )
def filter_cbk_probe_data(_data, _size, _probe):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    _filter = cast(obj, py_object).value
    if numpy_support:
        ar_data = np.ctypeslib.as_array(_data, (_size,))
        ar_data.flags.writeable=False
        res = _filter.probe_data(ar_data, _size)
    else:
        res = _filter.probe_data(_data, _size)
    if res==None:
        _probe.contents=0
        return None
    _probe.contents=2 #GF_FPROBE_MAYBE_SUPPORTED
    return res.encode('utf-8')


_libgpac.gf_filter_set_reconfigure_output_ckb.argtypes = [_gf_filter, c_void_p]
@CFUNCTYPE(c_int, _gf_filter, _gf_filter_pid )
def filter_cbk_reconfigure_output(_f, _pid):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    _filter = cast(obj, py_object).value
    obj = _libgpac.gf_filter_pid_get_udta(_pid)
    if obj:
        pid_obj = cast(obj, py_object).value
    else:
        pid_obj=None
    if pid_obj:
        return _filter.reconfigure_output(_f)
    raise Exception('Reconfigure on unknown output pid !')


_libgpac.gf_filter_pid_get_packet.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_packet.restype = _gf_filter_packet

_libgpac.gf_filter_pid_drop_packet.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_new.argtypes = [_gf_filter]
_libgpac.gf_filter_pid_new.restype = _gf_filter_pid

_libgpac.gf_filter_update_status.argtypes = [_gf_filter, c_uint, c_char_p]

_libgpac.gf_filter_ask_rt_reschedule.argtypes = [_gf_filter, c_uint]
_libgpac.gf_filter_post_process_task.argtypes = [_gf_filter]

_libgpac.gf_filter_setup_failure.argtypes = [_gf_filter, c_int]
_libgpac.gf_filter_make_sticky.argtypes = [_gf_filter]
_libgpac.gf_filter_prevent_blocking.argtypes = [_gf_filter, gf_bool]
_libgpac.gf_filter_block_eos.argtypes = [_gf_filter, gf_bool]
_libgpac.gf_filter_set_max_extra_input_pids.argtypes = [_gf_filter, c_uint]
_libgpac.gf_filter_block_enabled.argtypes = [_gf_filter]
_libgpac.gf_filter_block_enabled.restype = gf_bool
_libgpac.gf_filter_get_output_buffer_max.argtypes = [_gf_filter, POINTER(c_uint), POINTER(c_uint)]
_libgpac.gf_filter_all_sinks_done.argtypes = [_gf_filter]
_libgpac.gf_filter_all_sinks_done.restype = gf_bool
_libgpac.gf_filter_get_num_events_queued.argtypes = [_gf_filter]

_libgpac.gf_filter_get_clock_hint.argtypes = [_gf_filter, POINTER(c_ulonglong), POINTER(Fraction64)]
_libgpac.gf_filter_connections_pending.argtypes = [_gf_filter]
_libgpac.gf_filter_connections_pending.restype = gf_bool
_libgpac.gf_filter_hint_single_clock.argtypes = [_gf_filter, c_ulonglong, Fraction64]

GF_FS_REG_MAIN_THREAD=1<<1

##\endcond

## Base class used to create custom filters in python
class FilterCustom(Filter):

    ##constructor, see \ref gf_fs_new_filter
    #\param session FilterSession object
    #\param fname name of the filter
    #\param flags flags for the filter
    def __init__(self, session, fname="Custom", flags=0):
        errp = c_int(0)
        _filter = _libgpac.gf_fs_new_filter(session._sess, fname.encode('utf-8'), flags | GF_FS_REG_MAIN_THREAD, byref(errp))
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
        prop_4cc = pcode
        prop_name = None
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
    #\param when delay in microseconds, negative value will just post a process task. A value of 0 will mark filter as active even if no packet was dropped/sent
    #\return
    def reschedule(self, when=0):
        if when>=0:
            _libgpac.gf_filter_ask_rt_reschedule(self._filter, when)
        else:
            _libgpac.gf_filter_post_process_task(self._filter)

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
        return val.value

    @property 
    def clock_hint_mediatime(self):
        val = Fraction64
        _libgpac.gf_filter_get_clock_hint(self._filter, None, byref(val))
        return val.value

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
_libgpac.gf_filter_pid_set_property_dyn.argtypes = [_gf_filter_pid, c_char_p, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_set_info.argtypes = [_gf_filter_pid, c_uint, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_set_info_dyn.argtypes = [_gf_filter_pid, c_char_p, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_clear_eos.argtypes = [_gf_filter_pid, gf_bool]
_libgpac.gf_filter_pid_check_caps.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_check_caps.restype = gf_bool
_libgpac.gf_filter_pid_discard_block.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_allow_direct_dispatch.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_reset_properties.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_clock_info.argtypes = [_gf_filter_pid, POINTER(c_longlong), POINTER(c_uint)]
_libgpac.gf_filter_pid_remove.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_filter_in_parents.argtypes = [_gf_filter_pid, _gf_filter]
_libgpac.gf_filter_pid_is_filter_in_parents.restype = gf_bool

_libgpac.gf_filter_pid_get_name.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_name.restype = c_char_p
_libgpac.gf_filter_pid_set_name.argtypes = [_gf_filter_pid, c_char_p]

_libgpac.gf_filter_pid_is_eos.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_eos.restype = gf_bool
_libgpac.gf_filter_pid_set_eos.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_has_seen_eos.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_has_seen_eos.restype = gf_bool

_libgpac.gf_filter_pid_eos_received.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_eos_received.restype = gf_bool

_libgpac.gf_filter_pid_would_block.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_would_block.restype = gf_bool
_libgpac.gf_filter_pid_set_loose_connect.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_is_sparse.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_sparse.restype = gf_bool

_libgpac.gf_filter_pid_set_framing_mode.argtypes = [_gf_filter_pid, gf_bool]

_libgpac.gf_filter_pid_get_max_buffer.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_max_buffer.restype = c_uint
_libgpac.gf_filter_pid_set_max_buffer.argtypes = [_gf_filter_pid, c_uint]

_libgpac.gf_filter_pid_query_buffer_duration.argtypes = [_gf_filter_pid, gf_bool]
_libgpac.gf_filter_pid_query_buffer_duration.restype = c_ulonglong

_libgpac.gf_filter_pid_first_packet_is_empty.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_first_packet_is_empty.restype = gf_bool

_libgpac.gf_filter_pid_get_first_packet_cts.argtypes = [_gf_filter_pid, POINTER(c_ulonglong)]
_libgpac.gf_filter_pid_get_first_packet_cts.restype = gf_bool

_libgpac.gf_filter_pid_get_packet_count.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_timescale.argtypes = [_gf_filter_pid]

_libgpac.gf_filter_pid_set_clock_mode.argtypes = [_gf_filter_pid, gf_bool]
_libgpac.gf_filter_pid_set_discard.argtypes = [_gf_filter_pid, gf_bool]

_libgpac.gf_filter_pid_require_source_id.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_recompute_dts.argtypes = [_gf_filter_pid, gf_bool]


_libgpac.gf_filter_pid_get_min_pck_duration.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_playing.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_is_playing.restype = gf_bool

_libgpac.gf_filter_pid_get_filter_name.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_filter_name.restype = c_char_p

_libgpac.gf_filter_pid_get_next_ts.argtypes = [_gf_filter_pid]
_libgpac.gf_filter_pid_get_next_ts.restype = c_longlong


_libgpac.gf_filter_pid_caps_query.argtypes = [_gf_filter_pid, c_uint]
_libgpac.gf_filter_pid_caps_query.restype = POINTER(PropertyValue)
_libgpac.gf_filter_pid_caps_query_str.argtypes = [_gf_filter_pid, c_char_p]
_libgpac.gf_filter_pid_caps_query_str.restype = POINTER(PropertyValue)

_libgpac.gf_filter_pid_negotiate_property.argtypes = [_gf_filter_pid, c_uint, POINTER(PropertyValue)]
_libgpac.gf_filter_pid_negotiate_property_dyn.argtypes = [_gf_filter_pid, c_char_p, POINTER(PropertyValue)]

_libgpac.gf_filter_pck_new_ref.argtypes = [_gf_filter_pid, c_uint, c_uint, _gf_filter_packet]
_libgpac.gf_filter_pck_new_ref.restype = _gf_filter_packet

_libgpac.gf_filter_pck_new_alloc.argtypes = [_gf_filter_pid, c_uint, POINTER(c_void_p)]
_libgpac.gf_filter_pck_new_alloc.restype = _gf_filter_packet

_libgpac.gf_filter_pck_new_clone.argtypes = [_gf_filter_pid, _gf_filter_packet, POINTER(c_void_p)]
_libgpac.gf_filter_pck_new_clone.restype = _gf_filter_packet
_libgpac.gf_filter_pck_new_copy.argtypes = [_gf_filter_pid, _gf_filter_packet, POINTER(c_void_p)]
_libgpac.gf_filter_pck_new_copy.restype = _gf_filter_packet

## \endcond private


## \cond private

#callback for packet destruction

_libgpac.gf_filter_pck_new_shared.argtypes = [_gf_filter_pid, POINTER(c_ubyte), c_uint, c_void_p]
_libgpac.gf_filter_pck_new_shared.restype = _gf_filter_packet

@CFUNCTYPE(c_int, _gf_filter, _gf_filter_pid, _gf_filter_packet)
def filter_cbk_release_packet(_f, _pid, _pck):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    _filter = cast(obj, py_object).value
    pck_obj=None
    obj = _libgpac.gf_filter_pid_get_udta(_pid)
    if obj:
        pid_obj = cast(obj, py_object).value
    else:
        raise Exception('unknown PID on packet destruction callback')

    for pck in pid_obj.pck_refs:
        if pck._pck==_pck:
            pck_obj = pck
            break

    if not pck_obj:
        raise Exception('unknown packet on packet destruction callback')
    pid_obj.pck_refs.remove(pck_obj)
    _filter.packet_release(pid_obj, pck_obj)
    return 0

## \endcond private

## Object representing a PID of a custom filter
class FilterPid:
    ## \cond private
    def __init__(self, _filter, pid, IsInput):
        self._filter = _filter
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
            ##True if end of stream was seen in the chain but has not yet reached the filter, readonly  - see \ref gf_filter_pid_has_seen_eos
            #\hideinitializer
            self.has_seen_eos=0
            ##True if end of stream was seen on the input PID but some packets are still to be processed, readonly  - see \ref gf_filter_pid_eos_received
            #\hideinitializer
            self.eos_received=0
            ##True if PID would block, readonly - see \ref gf_filter_pid_would_block
            #\hideinitializer
            self.would_block=0
            ##True if PID is sparse, readonly - see \ref gf_filter_pid_is_sparse
            #\hideinitializer
            self.sparse=0
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
            ##Next estimated timestamp on pid, readonly  - see \ref gf_filter_pid_get_next_ts
            #\hideinitializer
            self.next_ts=0


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
        prop_4cc = pcode
        prop_name = None
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop==None:
            if prop_4cc:
                _libgpac.gf_filter_pid_set_property(self._pid, prop_4cc, None)
            else:
                _libgpac.gf_filter_pid_set_property_dyn(self._pid, prop_name, None)
            return

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pid_set_property(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_set_property_dyn(self._pid, prop_name, byref(prop_val))

    ##set a info property the current pid - see \ref gf_filter_pid_set_info and \ref gf_filter_pid_set_info_str
    #\param pcode property type
    #\param prop property value to set
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def set_info(self, pcode, prop, custom_type=0):
        if self._input:
            raise Exception('Cannot set info on input PID')
            return
        prop_4cc = pcode
        prop_name = None
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop==None:
            if prop_4cc:
                _libgpac.gf_filter_pid_set_info(self._pid, prop_4cc, None)
            else:
                _libgpac.gf_filter_pid_set_info_dyn(self._pid, prop_name, None)
            return

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pid_set_info(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_set_info_dyn(self._pid, prop_name, byref(prop_val))

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
        v = Fraction64()
        v.value.num = timestamp.value
        v.value.den = timescale.value
        return v

    ##check if a filter is in the parent filter chain of the pid - see \ref gf_filter_pid_is_filter_in_parents
    #\param _filter Filter to check
    #\return True or False
    def is_filter_in_parents(self, _filter):
        return _libgpac.gf_filter_pid_is_filter_in_parents(self._pid, _filter._f)

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
        _libgpac.gf_filter_pid_set_loose_connect(self._pid)

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

    ##enforces sourceID to be present for output pids of this filter - see \ref gf_filter_pid_require_source_id
    #\return
    def require_source_id(self):
        err = _libgpac.gf_filter_pid_require_source_id(self._pid)
        if err<0:
            raise Exception('Failed to require sourceID for pid: ' + e2s(err))
        return

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
        prop_4cc = pcode
        prop_name = None
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

    ##negotiates a capability property on input PID - see \ref gf_filter_pid_negotiate_property and \ref gf_filter_pid_negotiate_property_dyn
    #\param pcode property to negotiate
    #\param prop property to negotiate
    #\param custom_type type of property if user-defined property. If not set and user-defined, property is a string
    #\return
    def negotiate_cap(self, pcode, prop, custom_type=0):
        if not self._input:
            raise Exception('Cannot negotiate caps on output PID')
            return
        prop_4cc = pcode
        prop_name = None
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pid_negotiate_property(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_negotiate_property_dyn(self._pid, prop_name, byref(prop_val))

    ##resolves a template string - see \ref gf_filter_pid_resolve_file_template
    #\param template the template string
    #\param file_idx the file index
    #\param suffix the file suffix
    #\return the resolved template string
    def resolve_template(self, template, file_idx=0, suffix=None):
        res = create_string_buffer(2000)
        err = _libgpac.gf_filter_pid_resolve_file_template(self._pid, res, template.encode('utf-8'), file_idx, suffix)
        if err<0:
            raise Exception('Cannot resolve file template ' + template + ': ' + e2s(err))
        return res.raw.decode('utf-8')

    ##creates a new packet referring to an existing packet - see \ref gf_filter_pck_new_ref
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

    ##True if end of stream was seen in the chain but not yet reached by the filter - see \ref gf_filter_pid_has_seen_eos
    #\return
    @property
    def has_seen_eos(self):
        return _libgpac.gf_filter_pid_has_seen_eos(self._pid)

    ##True if end of stream was seen on pid but some packets are still pending - see \ref gf_filter_pid_eos_received
    #\return
    @property
    def eos_received(self):
        return _libgpac.gf_filter_pid_eos_received(self._pid)

    ##True if PID would block - see \ref gf_filter_pid_would_block
    #\return
    @property
    def would_block(self):
        return _libgpac.gf_filter_pid_would_block(self._pid)

    ##True if PID is sparse - see \ref gf_filter_pid_is_sparse
    #\return
    @property
    def sparse(self):
        return _libgpac.gf_filter_pid_is_sparse(self._pid)

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

    #\return
    @property
    def next_ts(self):
        return _libgpac.gf_filter_pid_get_next_ts(self._pid)

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

_libgpac.gf_filter_pck_get_framing.argtypes = [_gf_filter_packet, POINTER(gf_bool), POINTER(gf_bool)]
_libgpac.gf_filter_pck_set_framing.argtypes = [_gf_filter_packet, gf_bool, gf_bool]

_libgpac.gf_filter_pck_get_timescale.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_timescale.restype = c_uint

_libgpac.gf_filter_pck_get_interlaced.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_interlaced.restype = c_uint
_libgpac.gf_filter_pck_set_interlaced.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_get_corrupted.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_corrupted.restype = gf_bool
_libgpac.gf_filter_pck_set_corrupted.argtypes = [_gf_filter_packet, gf_bool]

_libgpac.gf_filter_pck_get_seek_flag.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_seek_flag.restype = gf_bool
_libgpac.gf_filter_pck_set_seek_flag.argtypes = [_gf_filter_packet, gf_bool]

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


class FrameInterface(Structure):
    _fields_ = [
        ("get_plane", c_void_p),
        ("get_gl_texture", c_void_p),
        ("flags", c_uint),
        ("user_data", c_void_p)
    ]

get_gl_tex_fun = CFUNCTYPE(c_int, POINTER(FrameInterface), c_uint, POINTER(c_uint), POINTER(c_uint), POINTER(c_void_p))


_libgpac.gf_filter_pck_get_frame_interface.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_get_frame_interface.restype = POINTER(FrameInterface)

_libgpac.gf_filter_pck_is_blocking_ref.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_is_blocking_ref.restype = gf_bool

_libgpac.gf_filter_pck_has_properties.argtypes = [_gf_filter_packet]
_libgpac.gf_filter_pck_has_properties.restype = gf_bool

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
_libgpac.gf_filter_pck_set_property_dyn.argtypes = [_gf_filter_packet, c_char_p, POINTER(PropertyValue)]
_libgpac.gf_filter_pck_truncate.argtypes = [_gf_filter_packet, c_uint]

_libgpac.gf_filter_pck_dangling_copy.argtypes = [_gf_filter_packet, _gf_filter_packet]
_libgpac.gf_filter_pck_dangling_copy.restype = _gf_filter_packet

##\endcond private

## OpenGL texture info 
class GLTextureInfo:
  ##\cond private
  def __init__(self, gl_id, gl_format):
  ##\endcond
    ##OpenGL texture ID
    #\hideinitializer
    self.id = gl_id
    ##OpenGL texture format (e.g., GL_TEXTURE_2D)
    #\hideinitializer
    self.format = gl_format

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
            ##true if packet holds a GF_FrameInterface object and not a data packet, readonly
            #\hideinitializer
            self.frame_ifce=0
            ##true if packet holds a GF_FrameInterface object providing OpenGL only access and not a data packet, readonly
            #\hideinitializer
            self.frame_ifce_gl=0
            ##Custom properties present, readonly - see \ref gf_filter_pck_has_properties
            #\hideinitializer
            self.has_properties=0
            ##true if packet is a blocking reference, readonly - see \ref gf_filter_pck_is_blocking_ref
            #\hideinitializer
            self.blocking_ref=0

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
        return None

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

    ##creates a new packet cloning a source packet - see \ref gf_filter_pck_dangling_copy.
    #The resulting packet is read/write mode and may have its own memory allocated.
    #This is typically used by sink filters wishing to access underling GPU data of a packet using frame interface.
    #the resulting packet can be explicitly discarded using \ref discard, otherwise will be garbage collected.
    #\param cached_pck if set, will be reuse for creation of new packet. This can greatly reduce memory allocations
    #\return the new FilterPacket or None if failure or None if failure ( if grabbing the frame into a local copy failed)
    def clone(self, cached_pck=None):
        if cached_pck:
            _pck = _libgpac.gf_filter_pck_dangling_copy(self._pck, cached_pck._pck)
        else:
            _pck = _libgpac.gf_filter_pck_dangling_copy(self._pck, None)

        if _pck:
            pck = FilterPacket(_pck, False)
            pck._readonly = False
            return pck
        return None


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
        prop_4cc = pcode
        prop_name = None
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        if prop==None:
            if prop_4cc:
                _libgpac.gf_filter_pck_set_property(self._pck, prop_4cc, None)
            else:
                _libgpac.gf_filter_pck_set_property_dyn(self._pck, prop_name, None)
            return

        prop_val = _make_prop(prop_4cc, pcode, prop, custom_type)
        if prop_4cc:
            _libgpac.gf_filter_pck_set_property(self._pck, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pck_set_property_dyn(self._pck, prop_name, byref(prop_val))

    ##truncates an output packet to the given size - see \ref gf_filter_pck_truncate
    #\param size new size of packet
    #\return
    def truncate(self, size):
        if self._is_src:
            raise Exception('Cannot truncate an source packet')
        _libgpac.gf_filter_pck_truncate(self._pck, size)


    ##return OpenGL texture info for a given color plane of a frame interface packet
    #\param idx index of texture to fetch. The number of texture depends on the underlying pixel format
    #\return GLTextureInfo object or None if error
    def get_gl_texture(self, idx):
        p = _libgpac.gf_filter_pck_get_frame_interface(self._pck)
        if not p:
            return None
        if not p.contents.get_gl_texture:
            return None

        get_tex = get_gl_tex_fun(p.contents.get_gl_texture)
        gl_format=c_uint(0)
        gl_tex_id=c_uint(0)
        ret = get_tex(p, idx, byref(gl_format), byref(gl_tex_id), None)
        if ret:
            return None
        obj = GLTextureInfo(gl_tex_id.value, gl_format.value)
        return obj

    ##\cond private: until end, all packet properties
    @property
    def blocking_ref(self):
        return _libgpac.gf_filter_pck_is_blocking_ref(self._pck)

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
        start = gf_bool(False)
        end = gf_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        return start.value

    @start.setter
    def start(self, value):
        if self._is_src:
            raise Exception('Cannot set framing on source packet')
        start = gf_bool(False)
        end = gf_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        _libgpac.gf_filter_pck_set_framing(self._pck, value, end.value)

    @property
    def end(self):
        start = gf_bool(False)
        end = gf_bool(False)
        _libgpac.gf_filter_pck_get_framing(self._pck, byref(start), byref(end))
        return end.value

    @end.setter
    def end(self, value):
        if self._is_src:
            raise Exception('Cannot set framing on source packet')
        start = gf_bool(False)
        end = gf_bool(False)
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

    @property
    def has_properties(self):
        if _libgpac.gf_filter_pck_has_properties(self._pck):
            return True
        return False

    @property
    def frame_ifce(self):
        p = _libgpac.gf_filter_pck_get_frame_interface(self._pck)
        if p:
            return True
        return False

    @property
    def frame_ifce_gl(self):
        p = _libgpac.gf_filter_pck_get_frame_interface(self._pck)
        if not p:
            return False
        if p.contents == 0:
            return False
        if p.contents.get_gl_texture == None:
            return False
        return True

    ##\endcond private

    #todo
    #append ?

## @}


##\cond private
_gf_fileio = c_void_p
_libgpac.gf_fileio_new.argtypes = [c_char_p, py_object, c_void_p, c_void_p, c_void_p, c_void_p, c_void_p, c_void_p, c_void_p]
_libgpac.gf_fileio_new.restype = _gf_fileio

_libgpac.gf_fileio_url.argtypes = [_gf_fileio]
_libgpac.gf_fileio_url.restype = c_char_p

_libgpac.gf_fileio_resource_url.argtypes = [_gf_fileio]
_libgpac.gf_fileio_resource_url.restype = c_char_p

_libgpac.gf_fileio_translate_url.argtypes = [c_char_p]
_libgpac.gf_fileio_translate_url.restype = c_char_p

_libgpac.gf_fileio_get_udta.argtypes = [_gf_fileio]
_libgpac.gf_fileio_get_udta.restype = c_void_p

_libgpac.gf_fileio_del.argtypes = [_gf_fileio]

_libgpac.gf_fileio_tag_main_thread.argtypes = [_gf_fileio]

#wraps gf_url_concatenate to be able to free the value
#we declare a void * return value
_libgpac.gf_url_concatenate.argtypes = [c_char_p, c_char_p]
_libgpac.gf_url_concatenate.restype = c_void_p
_libgpac.gf_url_free.argtypes = [c_void_p]

def url_concatenate(_parent, _url):
	#this is c_void_p
	_path = _libgpac.gf_url_concatenate(_parent, _url)
	if _path==None:
		raise Exception('Failed to concatenate path, out of memory')
	#this is a string
	path = cast(_path, c_char_p).value.decode('utf-8')
	#we still have the pointer, we can free it
	_libgpac.gf_url_free(_path)
	return path

#fio open callback
@CFUNCTYPE(_gf_fileio, _gf_fileio, c_char_p, c_char_p, POINTER(c_int))
def fileio_cbk_open(_fio_ref, _url, _mode, error):
    fio_ref = cast(_libgpac.gf_fileio_get_udta(_fio_ref), py_object).value
    mode = _mode.decode('utf-8')
    error.contents.value=GF_OK
    if mode=="url":
        path = url_concatenate(_libgpac.gf_fileio_resource_url(_fio_ref), _url);
        new_gfio = FileIO(path, fio_ref)
        fio_ref.factory.pending_urls.append(new_gfio)
        #prevent garbage collection
        fio_ref.factory.gc_exclude.append(new_gfio)
        fio_ref.factory.all_refs+=1
        return new_gfio._gf_fio

    if mode=="ref":
        fio_ref.nb_refs+=1
        fio_ref.factory.all_refs+=1
        return _fio_ref

    if mode=="probe":
        if not hasattr(fio_ref.factory.root_obj, 'exists'):
            return None
        if not fio_ref.factory.root_obj.exists(url):
            error.contents.value=GF_URL_ERROR
        return None

    do_delete=False
    if mode=="unref":
        fio_ref.nb_refs-=1
        fio_ref.factory.all_refs-=1
        if fio_ref.nb_refs:
            return _fio_ref
        do_delete=True

    if mode=="close":
        if fio_ref.py_obj != None:
            fio_ref.py_obj.close()
            fio_ref.factory.all_refs-=1

        if fio_ref.nb_refs:
            return None
        do_delete=True

    if do_delete:
        #file no longer used, can now be garbage collected
        fio_ref.factory.gc_exclude.remove(fio_ref)

        if not fio_ref.factory == fio_ref:
            _libgpac.gf_fileio_del(fio_ref._gf_fio)
            fio_ref._gf_fio = None

        if not fio_ref.factory.all_refs and not len(fio_ref.factory.pending_urls) and not len(fio_ref.factory.gc_exclude):
            _libgpac.gf_fileio_del(fio_ref.factory._gf_fio)
            fio_ref.factory._gf_fio = None
            fio_ref.factory.root_obj = None
        return None


	#this is an open() call
    fio_url = _url.decode('utf-8')

	#check if associated object exists, if not so we can use this instance
    fio = None
    if fio_ref.py_obj == None:
        fio = fio_ref
        if fio_ref in fio_ref.factory.pending_urls:
            fio_ref.factory.all_refs-=1
            fio_ref.factory.pending_urls.remove(fio_ref)

	#check in pending urls
    if fio == None:
        for afio in fio_ref.factory.pending_urls:
            a_url = _libgpac.gf_fileio_resource_url(afio._gf_fio).decode('utf-8')
            if a_url == fio_url:
                fio_ref.factory.all_refs-=1
                fio_ref.factory.pending_urls.remove(afio)
                fio = afio
                break;

	#we need to create a new FileIO
    created=False
    if fio==None:
        created=True

	#read url, translate
    if fio_url.startswith('gfio://')==True:
        fio_url = _libgpac.gf_fileio_translate_url(_url).decode('utf-8')
	#if we create a new FileIO, we need to resolve url, against parent URL
    elif created:
        fio_url = url_concatenate(_libgpac.gf_fileio_resource_url(fio_ref._gf_fio), _url)

    if fio==None:
        fio = FileIO(fio_url, fio_ref)

    #create object, direct copy of the object passed at construction
    fio.py_obj = copy.copy(fio.factory.root_obj)
    res = fio.py_obj.open(fio_url, mode)

    if res==True:
        if not fio in fio.factory.gc_exclude:
            fio.factory.gc_exclude.append(fio)
        fio.factory.all_refs+=1
        return fio._gf_fio

    #failure to open a new created object, remove FileIO and let the object be garbage collected
    if created:
        _libgpac.gf_fileio_del(fio._gf_fio)
        fio._gf_fio = None

    error.contents.value=GF_URL_ERROR
    return None

#fio write callback
@CFUNCTYPE(c_uint, _gf_fileio, POINTER(c_ubyte), c_uint)
def fileio_cbk_write(_fio, buf, size):
    fio = cast(_libgpac.gf_fileio_get_udta(_fio), py_object).value
    if numpy_support:
        ar_data = np.ctypeslib.as_array(buf, shape=(size,))
        ar_data.flags.writeable=False
        return fio.py_obj.write(ar_data, size)
    return fio.py_obj.write(buf, size)

#fio read callback
@CFUNCTYPE(c_uint, _gf_fileio, POINTER(c_ubyte), c_uint)
def fileio_cbk_read(_fio, buf, size):
    fio = cast(_libgpac.gf_fileio_get_udta(_fio), py_object).value
    if numpy_support:
        ar_data = np.ctypeslib.as_array(buf, shape=(size,))
        ar_data.flags.writeable=True
        return fio.py_obj.read(ar_data, size)
    return fio.py_obj.read(buf, size)

#fio seek callback
@CFUNCTYPE(c_uint, _gf_fileio, c_ulonglong, c_int)
def fileio_cbk_seek(_fio, pos, whence):
    fio = cast(_libgpac.gf_fileio_get_udta(_fio), py_object).value
    return fio.py_obj.seek(pos, whence)

#fio tell callback
@CFUNCTYPE(c_ulonglong, _gf_fileio)
def fileio_cbk_tell(_fio):
    fio = cast(_libgpac.gf_fileio_get_udta(_fio), py_object).value
    return fio.py_obj.tell()

#fio eof callback
@CFUNCTYPE(gf_bool, _gf_fileio)
def fileio_cbk_eof(_fio):
    fio = cast(_libgpac.gf_fileio_get_udta(_fio), py_object).value
    return fio.py_obj.eof()



##\endcond private

##
#  \defgroup pyfileio_grp libgpac core tools
#  \ingroup pycore_grp Python APIs
#  \brief FileIO tools for libgpac.
#
# @{


## FileIO object for file IO callbacks from libgpac
#
#
# The FileIO object is used to create input or output interfaces in which GPAC will read or write.
# This allows generating content in python without any disk IO, or passing python data as input to GPAC without intermediate file.
#
# This object passed to the FileIO constructor must implement the following callbacks:
#
# \code boolean open(string URL, string mode)\endcode
#Opens the file in read or write mode.
#- URL: string containing file to open
#- mode: open mode (same as fopen)
#- return true if success, false otherwise
#
#\note There is no guarantee that the URL is checked for existence before calling open
#\note There is no guarantee that the first call to open is on the URL provided for the constructor (e.g. for DASH generation, this will depend on the DASH profile used which may require to write the manifest after one or more segments)
#
# \code void close()\endcode
#Closes the file
#
# \code int write(numpy buffer, unsigned long size)\endcode
#Writes the file
#- buffer: numpy array to fill if numpy support, ctypes.c_ubyte otherwise
#- size: number of bytes to write starting from first byte in buffer
#- return number of bytes writen, at most the size of the array
#
# \code int read(numy buffer, unsigned long size)\endcode
#Reads the file
#- buffer: numpy array to read if numpy support, ctypes.c_ubyte otherwise
#- size: number of bytes to read
#- return number of bytes read, at most the size of the array
#
# \code void seek(unsigned long long pos, int whence)\endcode
#Seeks current position in file (same as fopen)
#- pos: position
#- whence: origin of the position
#- return 0 if no error, error code otherwise
#
# \code unsigned long long tell()\endcode
#Gets current position in file (same as ftell)
#- pos: position
#- return current position
#
# \code boolean eof()\endcode
#Checks if the current position is at the end of the file (same as feof)
#- return true if file end reached, false otherwise
#
# \code boolean exists(string URL)\endcode
#Checks if the given URL exists. This callback is optional
#- return true if associated URL exists, false otherwise
#
#\warning All these callbacks MUST perform synchronously
#
# The URL passed to the constructor indentifies the file name wrapped.
# Some file types such as HLS or DASH manifest may imply reading or writing several files.
# To handle these cases, the object passed to the constructor will be cloned for each call to open().
#
#\warning This object is a shallow copy of the factory object, not an empty object (difference with NodeJS bindings).
#
# All FileIO callbacks will be done in the main thread.
#
class FileIO:
	## constructor for FileIO
	#\param url url for this fileIO factory
	#\param obj instance of the class used for IOs. This instance will be cloned (shallow copy) for each new file to open
	def __init__(self, url, obj):
		if 0:
			## the underlying gfio:// URL to be provided to GPAC
			self.url = 0

##\cond private
		_libgpac.user_init = True
		self.nb_refs=0
		self.py_obj = None
		#this is internal constructor call from open
		if obj.__class__.__name__=="FileIO":
			self.factory = obj.factory
		#this is regular constructor call
		else:
			if hasattr(obj, 'open')==False:
				raise Exception('No open function on FileIO callback')
			if hasattr(obj, 'close')==False:
				raise Exception('No close function on FileIO callback')
			if hasattr(obj, 'write')==False:
				raise Exception('No write function on FileIO callback')
			if hasattr(obj, 'read')==False:
				raise Exception('No read function on FileIO callback')
			if hasattr(obj, 'seek')==False:
				raise Exception('No see function on FileIO callback')
			if hasattr(obj, 'tell')==False:
				raise Exception('No tell function on FileIO callback')
			self.factory = self
			self.factory.all_refs=0
			self.factory.pending_urls=[]
			#keep all created file IOs still useful here to prevent garbage collection
			self.factory.gc_exclude=[]
			self.factory.root_obj = obj

		self._gf_fio = _libgpac.gf_fileio_new(url.encode('utf-8'), py_object(self), fileio_cbk_open, fileio_cbk_seek, fileio_cbk_read, fileio_cbk_write, fileio_cbk_tell, fileio_cbk_eof, None )
		if self._gf_fio==None:
			raise Exception('Failed to create FileIO for ' + url)
		#ask for all file IOs to happen on main thread
		_libgpac.gf_fileio_tag_main_thread(self._gf_fio)

	@property
	def url(self):
		_url = _libgpac.gf_fileio_url(self._gf_fio)
		if _url==None:
			return None
		return _url.decode('utf-8')
##\endcond private

## @}
#
