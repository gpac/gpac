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


#
#   This is an initial work and more bindings might be needed, feel free to contribute/PR on https://github.com/gpac/gpac
#
#  These bindings allow:
#   - initializing libgpac, setting global arguments for the lib
#   - creation, running and destruction of a filter session in blocking mode or non-blocking mode
#   - inspection of filters in a session
#   - user tasks scheduling
#   - Creation of custom filters in Python
#


from ctypes import *
import datetime
import types

#
# Type definitions
#

GF_FilterSession = c_void_p
GF_Filter = c_void_p
GF_FilterPid = c_void_p
GF_FilterPacket = c_void_p
GF_PropertyEntry = c_void_p


class GF_Fraction(Structure):
    _fields_ = [ ("num", c_int), ("den", c_uint)]
    def __str__(self):
        return str(self.num)+'/' + str(self.den)

class GF_Fraction64(Structure):
    _fields_ = [("num", c_longlong), ("den", c_ulonglong)]
    def __str__(self):
        return str(self.num)+'/' + str(self.den)

class GF_FilterStats(Structure):
    pass
GF_FilterStats._fields_ = [
    ("filter", GF_Filter),
    ("filter_alias", GF_Filter),
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
    ("last_ts_sent", GF_Fraction64),
    ("last_ts_drop", GF_Fraction64)
]

class GF_FilterArg(Structure):
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

class GF_PropVec2i(Structure):
    _fields_ = [("x", c_int), ("y", c_int)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)

class GF_PropVec2(Structure):
    _fields_ = [("x", c_double),("y", c_double)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)

class GF_PropVec3i(Structure):
    _fields_ = [("x", c_int), ("y", c_int), ("z", c_int)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)

class GF_PropVec3(Structure):
    _fields_ = [ ("x", c_double), ("y", c_double), ("z", c_double)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)

class GF_PropVec4i(Structure):
    _fields_ = [("x", c_int), ("y", c_int), ("z", c_int), ("w", c_int)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)+'x' + str(self.w)

class GF_PropVec4(Structure):
    _fields_ = [("x", c_double), ("y", c_double), ("z", c_double), ("w", c_double)]
    def __str__(self):
        return str(self.x)+'x' + str(self.y)+'x' + str(self.z)+'x' + str(self.w)

class GF_PropData(Structure):
    _fields_ = [("ptr", c_void_p), ("size", c_uint)]
    def __str__(self):
        return 'data,size:'+str(self.size)

class GF_PropUIntList(Structure):
    _fields_ = [("vals", POINTER(c_uint)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += str(self.vals[i])
        return res

class GF_PropIntList(Structure):
    _fields_ = [("vals", POINTER(c_int)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += str(self.vals[i])
        return res

class GF_PropVec2iList(Structure):
    _fields_ = [("vals", POINTER(GF_PropVec2i)), ("nb_items", c_uint)]
    def __str__(self):
        res=''
        for i in range(self.nb_items):
            if i:
                res += ','
            res += str(self.vals[i].x) + 'x' + str(self.vals[i].y)
        return res

class GF_PropertyValueUnion(Union):
    pass
GF_PropertyValueUnion._fields_ = [
    ("longuint", c_ulonglong),
    ("longsint", c_longlong),
    ("sint", c_int),
    ("uint", c_uint),
    ("boolean", c_bool),
    ("frac", GF_Fraction),
    ("lfrac", GF_Fraction64),
    ("fnumber", c_float),
    ("number", c_double),
    ("vec2i", GF_PropVec2i),
    ("vec2", GF_PropVec2),
    ("vec3i", GF_PropVec3i),
    ("vec3", GF_PropVec3),
    ("vec4i", GF_PropVec3i),
    ("vec4", GF_PropVec3),
    ("data", GF_PropData),
    ("string", c_char_p),
    ("ptr", c_void_p),
    ("uint_list", GF_PropUIntList),
    ("int_list", GF_PropIntList),
    ("v2i_list", GF_PropVec2iList)
    #todo, map string list ...
]

class GF_PropertyValue(Structure):
    pass
GF_PropertyValue._fields_ = [
    ("type", c_uint),
    ("value", GF_PropertyValueUnion)
]


class GF_FEVT_Base(Structure):
    _fields_ =  [("type", c_uint), ("on_pid", GF_FilterPid)]

class GF_FEVT_Play(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
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

class GF_FEVT_SourceSeek(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("start_offset", c_ulonglong),
        ("end_offset", c_ulonglong),
        ("source_switch", c_char_p),
        ("previous_is_init_segment", c_ubyte),
        ("skip_cache_expiration", c_ubyte),
        ("hint_block_size", c_uint)
    ]

class GF_FEVT_SegmentSize(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("seg_url", c_char_p),
        ("is_init", c_bool),
        ("media_range_start", c_ulonglong),
        ("media_range_end", c_ulonglong),
        ("idx_range_start", c_ulonglong),
        ("idx_range_end", c_ulonglong)
    ]


class GF_FEVT_AttachScene(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("odm", c_void_p)
    ]

class GF_FEVT_QualitySwitch(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("up", c_bool),
        ("dependent_group_index", c_uint),
        ("q_idx", c_int),
        ("set_tile_mode_plus_one", c_uint),
        ("quality_degradation", c_uint)
    ]

#TODO GF_FEVT_Event;

class GF_FEVT_FileDelete(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("url", c_char_p)
    ]

class GF_FEVT_VisibililityHint(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("min_x", c_uint),
        ("max_x", c_uint),
        ("min_y", c_uint),
        ("max_y", c_uint),
        ("is_gaze", c_bool)
    ]

class GF_FEVT_BufferRequirement(Structure):
    _fields_ =  [
        ("type", c_uint),
        ("on_pid", GF_FilterPid),
        ("max_buffer_us", c_uint),
        ("max_playout_us", c_uint),
        ("min_playout_us", c_uint),
        ("pid_only", c_bool)
    ]

class GF_FilterEvent(Union):
    def __init__(self, type=0):
        self.base.type = type

    _fields_ =  [
        ("base", GF_FEVT_Base),
        ("play", GF_FEVT_Play),
        ("seek", GF_FEVT_SourceSeek),
        ("attach_scene", GF_FEVT_AttachScene),
        #("user_event", GF_FEVT_Event,
        ("quality_switch", GF_FEVT_QualitySwitch),
        ("visibility_hint", GF_FEVT_VisibililityHint),
        ("buffer_req", GF_FEVT_BufferRequirement),
        ("seg_size", GF_FEVT_SegmentSize),
        ("file_del", GF_FEVT_FileDelete)
    ]


#
# Constants definitions
#


#scheduler type
GF_FS_SCHEDULER_LOCK_FREE=0
GF_FS_SCHEDULER_LOCK=1
GF_FS_SCHEDULER_LOCK_FREE_X=2
GF_FS_SCHEDULER_LOCK_FORCE=3
GF_FS_SCHEDULER_DIRECT=4

#session flags
GF_FS_FLAG_LOAD_META=1<<1
GF_FS_FLAG_NO_BLOCKING=1<<2
GF_FS_FLAG_NO_GRAPH_CACHE=1<<3
GF_FS_FLAG_NO_MAIN_THREAD=1<<4
GF_FS_FLAG_NO_REGULATION=1<<5
GF_FS_FLAG_NO_PROBE=1<<6
GF_FS_FLAG_NO_REASSIGN=1<<7
GF_FS_FLAG_PRINT_CONNECTIONS=1<<8
GF_FS_FLAG_NO_ARG_CHECK=1<<9
GF_FS_FLAG_NO_RESERVOIR=1<<10
GF_FS_FLAG_FULL_LINK=1<<11


GF_PROP_FORBIDEN=0
GF_PROP_SINT=1
GF_PROP_UINT=2
GF_PROP_LSINT=3
GF_PROP_LUINT=4
GF_PROP_BOOL=5
GF_PROP_FRACTION=6
GF_PROP_FRACTION64=7
GF_PROP_FLOAT=8
GF_PROP_DOUBLE=9
GF_PROP_VEC2I=10
GF_PROP_VEC2=11
GF_PROP_VEC3I=12
GF_PROP_VEC3=13
GF_PROP_VEC4I=14
GF_PROP_VEC4=15
GF_PROP_PIXFMT=16
GF_PROP_PCMFMT=17
GF_PROP_STRING=18
GF_PROP_STRING_NO_COPY=19
GF_PROP_DATA=20
GF_PROP_NAME=21
GF_PROP_DATA_NO_COPY=22
GF_PROP_CONST_DATA=23
GF_PROP_POINTER=24
GF_PROP_STRING_LIST=25
GF_PROP_UINT_LIST=26
GF_PROP_SINT_LIST=27
GF_PROP_VEC2I_LIST=28


GF_FEVT_PLAY = 1
GF_FEVT_SET_SPEED = 2
GF_FEVT_STOP = 3
GF_FEVT_PAUSE = 4
GF_FEVT_RESUME = 5
GF_FEVT_SOURCE_SEEK = 6
GF_FEVT_SOURCE_SWITCH = 7
GF_FEVT_SEGMENT_SIZE = 8
GF_FEVT_ATTACH_SCENE = 9
GF_FEVT_RESET_SCENE = 10
GF_FEVT_QUALITY_SWITCH = 11
GF_FEVT_VISIBILITY_HINT = 12
GF_FEVT_INFO_UPDATE = 13
GF_FEVT_BUFFER_REQ = 14
GF_FEVT_CAPS_CHANGE = 15
GF_FEVT_CONNECT_FAIL = 16
GF_FEVT_USER = 17
GF_FEVT_PLAY_HINT = 18
GF_FEVT_FILE_DELETE = 19


GF_FS_ARG_HINT_NORMAL = 0
GF_FS_ARG_HINT_ADVANCED = 1<<1
GF_FS_ARG_HINT_EXPERT = 1<<2
GF_FS_ARG_HINT_HIDE = 1<<3
GF_FS_ARG_UPDATE = 1<<4
GF_FS_ARG_META = 1<<5
GF_FS_ARG_META_ALLOC = 1<<6
GF_FS_ARG_SINK_ALIAS = 1<<7


GF_CAPFLAG_IN_BUNDLE = 1
GF_CAPFLAG_INPUT = 1<<1
GF_CAPFLAG_OUTPUT = 1<<2
GF_CAPFLAG_EXCLUDED = 1<<3
GF_CAPFLAG_LOADED_FILTER = 1<<4
GF_CAPFLAG_STATIC = 1<<5
GF_CAPFLAG_OPTIONAL = 1<<6

#helpers
GF_CAPS_INPUT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT)
GF_CAPS_INPUT_OPT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OPTIONAL)
GF_CAPS_INPUT_STATIC = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_STATIC)
GF_CAPS_INPUT_STATIC_OPT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_STATIC|GF_CAPFLAG_OPTIONAL)
GF_CAPS_INPUT_EXCLUDED = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_EXCLUDED)
GF_CAPS_INPUT_LOADED_FILTER = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_LOADED_FILTER)
GF_CAPS_OUTPUT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT)
GF_CAPS_OUTPUT_LOADED_FILTER = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_LOADED_FILTER)
GF_CAPS_OUTPUT_EXCLUDED = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_EXCLUDED)
GF_CAPS_OUTPUT_STATIC = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_STATIC)
GF_CAPS_OUTPUT_STATIC_EXCLUDED = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_EXCLUDED|GF_CAPFLAG_STATIC)
GF_CAPS_INPUT_OUTPUT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT)
GF_CAPS_INPUT_OUTPUT_OPT = (GF_CAPFLAG_IN_BUNDLE|GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT|GF_CAPFLAG_OPTIONAL)


#load libgpac
_libgpac = cdll.LoadLibrary("libgpac.dylib")

#error to string helper
_libgpac.gf_error_to_string.argtypes = [c_int]
_libgpac.gf_error_to_string.restype = c_char_p
 
_libgpac.gf_gpac_version.restype = c_char_p
_libgpac.gf_gpac_copyright.restype = c_char_p
_libgpac.gf_gpac_copyright_cite.restype = c_char_p

_libgpac.gf_sys_set_args.argtypes = [c_int, POINTER(POINTER(c_char))]

version = str(_libgpac.gf_gpac_version(), "utf-8")
copyright = str(_libgpac.gf_gpac_copyright(), "utf-8")
copyright_cite = str(_libgpac.gf_gpac_copyright_cite(), "utf-8")


_libgpac.gf_sys_init.argtypes = [c_int, c_char_p]
_libgpac.gf_log_set_tools_levels.argtypes = [c_char_p, c_int]
_libgpac.gf_props_get_type_name.argtypes = [c_uint]
_libgpac.gf_props_get_type_name.restype = c_char_p

_libgpac.gf_sys_clock.res = c_uint
_libgpac.gf_sys_clock_high_res.res = c_ulonglong


last_err=0;
last_errp = c_int(0)

def e2s(err):
    return _libgpac.gf_error_to_string(err).decode()


mem_track_on=0
def init(mem_track=0, profile=None):
    if (mem_track!=0) or (profile != None):
        last_err = _libgpac.gf_sys_init(mem_track, profile)
    else:
        last_err = _libgpac.gf_sys_init(0, None)

    mem_track_on=mem_track
    if (hasattr(_libgpac, 'gf_memory_size')==False):
        mem_track_on=0
    else:
        mem_track_on=mem_track
    
    if (last_err<0): 
        raise Exception('Failed to initialize libgpac: ' + e2s(last_err))


def close():
    _libgpac.gf_sys_close()
    if mem_track_on:
        if _libgpac.gf_memory_size() or _libgpac.gf_file_handles_count():
            set_logs("mem@info")
            _libgpac.gf_memory_print()

def set_logs(logs):
    _libgpac.gf_log_set_tools_levels(logs.encode('utf-8'), 0)

def sys_clock():
    return _libgpac.gf_sys_clock()

def sys_clock_high_res():
    return _libgpac.gf_sys_clock_high_res()

def set_args(args):
    p = (POINTER(c_char)*len(args))()
    for i, arg in enumerate(args):
        enc_arg = arg.encode('utf-8')
        p[i] = create_string_buffer(enc_arg)
    _libgpac.gf_sys_set_args(len(args), cast(p, POINTER(POINTER(c_char))) )



GF_FilterSession = c_void_p
GF_Filter = c_void_p
GF_FilterPid = c_void_p

#
#        filter session bindings
#

_libgpac.gf_fs_new.argtypes = [c_int, c_int, c_int, c_char_p]
_libgpac.gf_fs_new.restype = GF_FilterSession

_libgpac.gf_fs_del.argtypes = [GF_FilterSession]
_libgpac.gf_fs_del.restype =  None

_libgpac.gf_fs_run.argtypes = [GF_FilterSession]
_libgpac.gf_fs_run.restype = c_int

_libgpac.gf_fs_load_source.argtypes = [GF_FilterSession, c_char_p, c_char_p, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_source.restype = GF_Filter

_libgpac.gf_fs_load_destination.argtypes = [GF_FilterSession, c_char_p, c_char_p, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_destination.restype = GF_Filter

_libgpac.gf_fs_load_filter.argtypes = [GF_FilterSession, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_load_filter.restype = GF_Filter

_libgpac.gf_fs_new_filter.argtypes = [GF_FilterSession, c_char_p, POINTER(c_int)]
_libgpac.gf_fs_new_filter.restype = GF_Filter


_libgpac.gf_fs_post_user_task.argtypes = [GF_FilterSession, c_void_p, py_object, c_char_p]
_libgpac.gf_fs_post_user_task.restype = c_int

_libgpac.gf_fs_is_last_task.argtypes = [GF_FilterSession]
_libgpac.gf_fs_is_last_task.restype = c_bool

_libgpac.gf_fs_get_filters_count.argtypes = [GF_FilterSession]
_libgpac.gf_fs_get_filter.argtypes = [GF_FilterSession, c_int]
_libgpac.gf_fs_get_filter.restype = GF_Filter

_libgpac.gf_fs_abort.argtypes = [GF_FilterSession, c_bool]

_libgpac.gf_fs_get_http_max_rate.argtypes = [GF_FilterSession]
_libgpac.gf_fs_get_http_rate.argtypes = [GF_FilterSession]
_libgpac.gf_fs_lock_filters.argtypes = [GF_FilterSession, c_bool]
_libgpac.gf_fs_enable_reporting.argtypes = [GF_FilterSession, c_bool]

_libgpac.gf_fs_print_stats.argtypes = [GF_FilterSession]
_libgpac.gf_fs_print_connections.argtypes = [GF_FilterSession]

_libgpac.gf_fs_fire_event.argtypes = [GF_FilterSession, GF_Filter, POINTER(GF_FilterEvent), c_bool]
_libgpac.gf_fs_fire_event.restype = c_bool

_libgpac.gf_fs_mime_supported.argtypes = [GF_FilterSession, c_char_p]
_libgpac.gf_fs_mime_supported.restype = c_bool

_libgpac.gf_fs_mime_supported.argtypes = [GF_FilterSession, c_char_p]
_libgpac.gf_fs_mime_supported.restype = c_bool

_libgpac.gf_fs_is_supported_source.argtypes = [GF_FilterSession, c_char_p]
_libgpac.gf_fs_is_supported_source.restype = c_bool


@CFUNCTYPE(c_bool, GF_FilterSession, c_void_p, POINTER(c_uint))
def fs_task_fun(sess, cbk, resched):
 obj = cast(cbk, py_object).value
 res = obj.execute()
 if ((res==None) or (res<0)):
    obj.session.rem_task(obj)
    return False
 resched.contents.value=res
 return True

class FilterTask:
    def __init__(self, name):
        self.name = name
    def execute(self):
        return -1


class FilterSession:
    def __init__(self, flags=0, blacklist=None, nb_threads=0, sched_type=0):
        if (blacklist!=None):
            self._sess = _libgpac.gf_fs_new(nb_threads, sched_type, flags, blacklist.encode('utf-8'))
        else:
            self._sess = _libgpac.gf_fs_new(nb_threads, sched_type, flags, blacklist)
        if (self._sess==None):
            raise Exception('Failed to create new filter session')
        self._filters = []
        self._tasks = []


    def delete(self):
        if self._sess:
            _libgpac.gf_fs_del(self._sess)
            self._sess=None


    def _to_filter(self, f, IsCustom=False):
        for i, a_f in enumerate(self._filters):
            if (a_f._filter == f):
                return a_f

        if (IsCustom):
            filter = FilterCustom(self, f)
        else:
            filter = Filter(self, f)
        self._filters.append(filter)
        return filter

    def run(self):
        last_err = _libgpac.gf_fs_run(self._sess)
        if (last_err<0): 
            raise Exception('Failed to run session: ' + e2s(last_err))

    def load_src(self, URL, parentURL=None):
        if (parentURL!=None):
            filter = _libgpac.gf_fs_load_source(self._sess, URL.encode('utf-8'), None, parentURL.encode('utf-8'), byref(last_errp))
        else:
            filter = _libgpac.gf_fs_load_source(self._sess, URL.encode('utf-8'), None, None, byref(last_errp))
        last_err = last_errp.value
        if (last_err<0): 
            raise Exception('Failed to load source filter ' + URL + ': ' + e2s(last_err))
        return self._to_filter(filter)

    def load_dst(self, URL, parentURL=None):
        if (parentURL!=None):
            filter = _libgpac.gf_fs_load_destination(self._sess, URL.encode('utf-8'), None, parentURL.encode('utf-8'), byref(last_errp))
        else:
            filter = _libgpac.gf_fs_load_destination(self._sess, URL.encode('utf-8'), None, None, byref(last_errp))
        last_err = last_errp.value
        if (last_err<0): 
            raise Exception('Failed to load destination filter ' + URL + ': ' + e2s(last_err))
        return self._to_filter(filter)


    def load(self, fname):
        filter = _libgpac.gf_fs_load_filter(self._sess, fname.encode('utf-8'), byref(last_errp))
        last_err = last_errp.value
        if (last_err<0): 
            raise Exception('Failed to load filter ' + URL + ': ' + e2s(last_err))
        return self._to_filter(filter)

    def post(self, task):
        task.fs = self
        task.session = self
        last_err = _libgpac.gf_fs_post_user_task(self._sess, fs_task_fun, py_object(task), task.name.encode('utf-8'))
        if (last_err<0): 
            raise Exception('Failed to load filter ' + URL + ': ' + e2s(last_err))
        self._tasks.append(task)

    def rem_task(self, task):
        task.fs = None
        task.session = None
        self._tasks.remove(task)

    def abort(self, flush=False):
        _libgpac.gf_fs_abort(self._sess, flush)
        
    def get_filter(self, index):
        f = _libgpac.gf_fs_get_filter(self._sess, index)
        if (f==None):
            raise Exception('Failed to get filter at index ' + str(index) )
        return self._to_filter(f)

    def lock(self, lock):
        _libgpac.gf_fs_lock_filters(self._sess, lock)

    def reporting(self, do_report):
        _libgpac.gf_fs_enable_reporting(self._sess, do_report)

    def print_stats(self):
        _libgpac.gf_fs_print_stats(self._sess)

    def print_graph(self):
        _libgpac.gf_fs_print_connections(self._sess)

    def fire_event(self, evt, filter=None, upstream=False):
        if filter:
            return _libgpac.gf_fs_fire_event(self._sess, filter._filter, byref(evt), upstream)
        return _libgpac.gf_fs_fire_event(self._sess, None, byref(evt), upstream)

    def is_supported_mime(self, mime):
        return _libgpac.gf_fs_mime_supported(self._sess, mime.encode('utf-8'))

    def is_supported_mime(self, url):
        return _libgpac.gf_fs_is_supported_source(self._sess, url.encode('utf-8'))


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

    #TODO
    #global session: session caps ?

    #missing features available in JSFilterSession API but with no native direct equivalents:
    # rmt_send, rmt_set_fun, set_new_filter_fun, set_del_filter_fun, set_event_fun



#
#        filter bindings
#
_libgpac.gf_filter_get_name.argtypes = [GF_Filter]
_libgpac.gf_filter_get_name.restype = c_char_p
_libgpac.gf_filter_get_id.argtypes = [GF_Filter]
_libgpac.gf_filter_get_id.restype = c_char_p
_libgpac.gf_filter_get_ipid_count.argtypes = [GF_Filter]
_libgpac.gf_filter_get_opid_count.argtypes = [GF_Filter]



_libgpac.gf_filter_get_stats.argtypes = [GF_Filter, POINTER(GF_FilterStats)]
_libgpac.gf_filter_remove.argtypes = [GF_Filter]
_libgpac.gf_fs_send_update.argtypes = [GF_FilterSession, c_char_p, GF_Filter, c_char_p, c_char_p, c_uint]
_libgpac.gf_filter_set_source.argtypes = [GF_Filter, GF_Filter, c_char_p]
_libgpac.gf_filter_reconnect_output.argtypes = [GF_Filter]

_libgpac.gf_props_get_id.argtypes = [c_char_p]
_libgpac.gf_filter_get_ipid.argtypes = [GF_Filter, c_uint]
_libgpac.gf_filter_get_ipid.restype = GF_FilterPid
_libgpac.gf_filter_get_opid.argtypes = [GF_Filter, c_uint]
_libgpac.gf_filter_get_opid.restype = GF_FilterPid
_libgpac.gf_filter_pid_get_property.argtypes = [GF_FilterPid, c_uint]
_libgpac.gf_filter_pid_get_property.restype = POINTER(GF_PropertyValue)
_libgpac.gf_filter_pid_get_property_str.argtypes = [GF_FilterPid, c_char_p]
_libgpac.gf_filter_pid_get_property_str.restype = POINTER(GF_PropertyValue)

_libgpac.gf_filter_pid_get_info.argtypes = [GF_FilterPid, c_uint, POINTER(POINTER(GF_PropertyEntry))]
_libgpac.gf_filter_pid_get_info.restype = POINTER(GF_PropertyValue)
_libgpac.gf_filter_pid_get_info_str.argtypes = [GF_FilterPid, c_char_p, POINTER(POINTER(GF_PropertyEntry))]
_libgpac.gf_filter_pid_get_info_str.restype = POINTER(GF_PropertyValue)
_libgpac.gf_filter_release_property.argtypes = [POINTER(GF_PropertyEntry)]


_libgpac.gf_pixel_fmt_name.argtypes = [c_uint]
_libgpac.gf_pixel_fmt_name.restype = c_char_p

_libgpac.gf_audio_fmt_name.argtypes = [c_uint]
_libgpac.gf_audio_fmt_name.restype = c_char_p

_libgpac.gf_filter_pid_enum_properties.argtypes = [GF_FilterPid, POINTER(c_uint), POINTER(c_uint), POINTER(c_char_p)]
_libgpac.gf_filter_pid_enum_properties.restype = POINTER(GF_PropertyValue)

_libgpac.gf_props_4cc_get_name.argtypes = [c_uint]
_libgpac.gf_props_4cc_get_name.restype = c_char_p

_libgpac.gf_stream_type_name.argtypes = [c_uint]
_libgpac.gf_stream_type_name.restype = c_char_p

_libgpac.gf_codecid_file_ext.argtypes = [c_uint]
_libgpac.gf_codecid_file_ext.restype = c_char_p

_libgpac.gf_filter_pid_get_source_filter.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_get_source_filter.restype = GF_Filter

_libgpac.gf_filter_pid_enum_destinations.argtypes = [GF_FilterPid, c_uint]
_libgpac.gf_filter_pid_enum_destinations.restype = GF_Filter

_libgpac.gf_filter_enumerate_args.argtypes = [GF_Filter, c_uint]
_libgpac.gf_filter_enumerate_args.restype = POINTER(GF_FilterArg)

_libgpac.gf_filter_get_info.argtypes = [GF_Filter, c_uint, POINTER(POINTER(GF_PropertyEntry))]
_libgpac.gf_filter_get_info.restype = POINTER(GF_PropertyValue)
_libgpac.gf_filter_get_info_str.argtypes = [GF_Filter, c_char_p, POINTER(POINTER(GF_PropertyEntry))]
_libgpac.gf_filter_get_info_str.restype = POINTER(GF_PropertyValue)


def _prop_to_python(pname, prop):
    type = prop.type
    if (type==GF_PROP_SINT):
        return prop.value.sint
    if (type==GF_PROP_UINT):
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
    if (type==GF_PROP_LSINT):
        return prop.value.longsint
    if (type==GF_PROP_LUINT):
        return prop.value.longuint
    if (type==GF_PROP_BOOL):
        return prop.value.boolean
    if (type==GF_PROP_FRACTION):
        return prop.value.frac
    if (type==GF_PROP_FRACTION64):
        return prop.value.lfrac
    if (type==GF_PROP_FLOAT):
        return prop.value.fnumber
    if (type==GF_PROP_DOUBLE):
        return prop.value.number
    if (type==GF_PROP_VEC2I):
        return prop.value.vec2i
    if (type==GF_PROP_VEC2):
        return prop.value.vec2
    if (type==GF_PROP_VEC3I):
        return prop.value.vec3i
    if (type==GF_PROP_VEC3):
        return prop.value.vec3
    if (type==GF_PROP_VEC4I):
        return prop.value.vec4i
    if (type==GF_PROP_VEC4):
        return prop.value.vec4
    if (type==GF_PROP_PIXFMT):
        pname = _libgpac.gf_pixel_fmt_name(prop.value.uint)
        return pname.decode('utf-8')
    if (type==GF_PROP_PCMFMT):
        pname = _libgpac.gf_audio_fmt_name(prop.value.uint)
        return pname.decode('utf-8')
    if (type==GF_PROP_STRING) or (type==GF_PROP_STRING_NO_COPY) or (type==GF_PROP_NAME):
        return prop.value.string.decode('utf-8')
    if (type==GF_PROP_DATA) or (type==GF_PROP_DATA_NO_COPY) or (type==GF_PROP_CONST_DATA):
        return prop.value.data
    if (type==GF_PROP_POINTER):
        return prop.value.ptr
    if (type==GF_PROP_STRING_LIST):
        raise Exception('Mapping from stringlist to python not implemented ')
    if (type==GF_PROP_UINT_LIST):
        res = [];
        for i in range(prop.value.uint_list.nb_items):
            val = prop.value.uint_list.vals[i]
            res.append(val)
        return res
    if (type==GF_PROP_SINT_LIST):
        res = [];
        for i in range(prop.value.uint_list.nb_items):
            val = prop.value.sint_list.vals[i]
            res.append(val)
        return res
    if (type==GF_PROP_VEC2I_LIST):
        res = [];
        for i in range(prop.value.v2i_list.nb_items):
            val = prop.value.v2i_list.vals[i]
            res.append(val)
        return res

    raise Exception('Unknown property type ' + str(type))



class Filter:
    def __init__(self, session, filter):
        self._filter = filter
        self._session = session

    def __str__(self):
        return self.name

    def remove(self):
        _libgpac.gf_filter_remove(self._filter)

    def update(self, name, value):
        _libgpac.gf_fs_send_update(None, None, self._filter, name, value, 0)

    def set_source(self, f, link_args=None):
        if (f!=None):
            _libgpac.gf_filter_set_source(self._filter, f._filter, link_args)

    def insert(self, f, link_args=None):
        if (f!=None):
            _libgpac.gf_filter_set_source(f._filter, self._filter, link_args)
            _libgpac.gf_filter_reconnect_output(f._filter)

    def _pid_prop_ex(self, prop_name, pid, IsInfo):
        _name = prop_name.encode('utf-8')
        p4cc = _libgpac.gf_props_get_id(_name)
        if IsInfo:
            pe = POINTER(GF_PropertyEntry)()
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
        if (pid==None):
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        return self._pid_prop_ex(self, prop_name, pid, False)

    def _pid_enum_props_ex(self, callback_obj, _pid):
        if (hasattr(callback_obj, 'on_prop_enum')==False):
            return

        pidx=c_uint(0)
        while True:
            _p4cc=c_uint(0)
            _pname=c_char_p(0)
            prop = _libgpac.gf_filter_pid_enum_properties(_pid, byref(pidx), byref(_p4cc), byref(_pname))
            if (prop):
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

        if (pid==None):
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )

        _pid_enum_props_ex(self, callback_obj, pid)

    def ipid_prop(self, idx, prop_name):
        return self._pid_prop(idx, prop_name, True)
    def ipid_enum_props(self, idx, callback_obj):
        self._pid_enum_props(idx, callback_obj, True)
    def opid_prop(self, idx, prop_name):
        return self._pid_prop(idx, prop_name, False)
    def opid_enum_props(self, idx, callback_obj):
        self._pid_enum_props(idx, callback_obj, False)


    def ipid_source(self, idx):
        pid = _libgpac.gf_filter_get_ipid(self._filter, idx)
        if (pid==None):
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        f = _libgpac.gf_filter_pid_get_source_filter(pid)
        if f==None:
            return None
        return self._session._to_filter(f)

    def opid_sinks(self, idx):
        pid = _libgpac.gf_filter_get_opid(self._filter, idx)
        if (pid==None):
            raise Exception('No PID with index ' + str(idx) + ' in filter ' + self.name )
        res=[]
        f_idx=0
        while True:
            f = _libgpac.gf_filter_pid_enum_destinations(pid, f_idx)
            f_idx+=1        
            if f==None:
                break
            filter = self._session._to_filter(f)
            print('sink ' + filter.name)
            res.append(filter)
        return res

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

    def get_info(self, prop_name):
        _name = prop_name.encode('utf-8')
        p4cc = _libgpac.gf_props_get_id(_name)
        pe = POINTER(GF_PropertyEntry)()
        if p4cc:
            prop = _libgpac.gf_filter_get_info(self._filter, p4cc, byref(pe))
        else:
            prop = _libgpac.gf_filter_get_info_str(self._filter, _name, byref(pe))
        if prop:
            res = _prop_to_python(prop_name, prop.contents)
            _libgpac.gf_filter_release_property(pe)
            return res
        return None

    @property
    def stats(self):
        stats = GF_FilterStats()
        err = _libgpac.gf_filter_get_stats(self._filter, byref(stats))
        if (err<0): 
            raise Exception('Failed to fetch filter stats: ' + e2s(err))
        return stats

    @property
    def name(self):
        return str(_libgpac.gf_filter_get_name(self._filter), 'utf-8')

    @property
    def ID(self):
        return str(_libgpac.gf_filter_get_id(self._filter), 'utf-8')

    @property
    def nb_ipid(self):
        return _libgpac.gf_filter_get_ipid_count(self._filter)

    @property
    def nb_opid(self):
        return _libgpac.gf_filter_get_opid_count(self._filter)


#
#        custom filter bindings
#

_libgpac.gf_filter_push_caps.argtypes = [GF_Filter, c_uint, POINTER(GF_PropertyValue), c_char_p, c_uint, c_uint]
_libgpac.gf_props_4cc_get_type.argtypes = [c_uint]
_libgpac.gf_stream_type_by_name.argtypes = [c_char_p]
_libgpac.gf_stream_type_by_name.restype = c_uint

_libgpac.gf_codec_parse.argtypes = [c_char_p]
_libgpac.gf_codec_parse.restype = c_uint

_libgpac.gf_pixel_fmt_parse.argtypes = [c_char_p]
_libgpac.gf_pixel_fmt_parse.restype = c_uint

_libgpac.gf_audio_fmt_parse.argtypes = [c_char_p]
_libgpac.gf_audio_fmt_parse.restype = c_uint

def _make_prop(prop4cc, propname, prop):
    prop_val = GF_PropertyValue()
    if prop4cc==0:
        prop_val.type = GF_PROP_STRING
        prop_val.value.string = str(prop).encode('utf-8')
        return prop_val

    type = _libgpac.gf_props_4cc_get_type(prop4cc)
    prop_val.type = type
    if propname=="StreamType":
        prop_val.value.uint = _libgpac.gf_stream_type_by_name(prop.encode('utf-8'))
        return prop_val
    elif propname=="CodecID":
        prop_val.value.uint = _libgpac.gf_codec_parse(prop.encode('utf-8'))
        return prop_val
    elif propname=="PixelFormat":
        prop_val.value.uint = _libgpac.gf_pixel_fmt_parse(prop.encode('utf-8'))
        return prop_val
    elif propname=="AudioFormat":
        prop_val.value.uint = _libgpac.gf_audio_fmt_parse(prop.encode('utf-8'))
        return prop_val


    if (type==GF_PROP_SINT):
        prop_val.value.sint = prop
    elif (type==GF_PROP_UINT):
        prop_val.value.uint = prop
    elif (type==GF_PROP_LSINT):
        prop_val.value.longsint = prop
    elif (type==GF_PROP_LUINT):
        prop_val.value.longuint = prop
    elif (type==GF_PROP_BOOL):
        prop_val.value.boolean = prop
    elif (type==GF_PROP_FRACTION):
        if hasattr(prop, 'den') and hasattr(prop, 'num'):
            prop_val.value.frac.num = prop.num
            prop_val.value.frac.den = prop.den
        elif is_integer(prop):
            prop_val.value.frac.num = prop
            prop_val.value.frac.den = 1
        else:
            prop_val.value.frac.num = 1000*prop
            prop_val.value.frac.den = 1000
    elif (type==GF_PROP_FRACTION64):
        if hasattr(prop, 'den') and hasattr(prop, 'num'):
            prop_val.value.lfrac.num = prop.num
            prop_val.value.lfrac.den = prop.den
        elif is_integer(prop):
            prop_val.value.lfrac.num = prop
            prop_val.value.lfrac.den = 1
        else:
            prop_val.value.lfrac.num = 1000000*prop
            prop_val.value.lfrac.den = 1000000
    elif (type==GF_PROP_FLOAT):
        prop_val.value.fnumber = prop
    elif (type==GF_PROP_DOUBLE):
        prop_val.value.number = prop
    elif (type==GF_PROP_VEC2I):
        if hasattr(prop, 'x') and hasattr(prop, 'y'):
            prop_val.value.vec2i.x = prop.x
            prop_val.value.vec2i.y = prop.y
        else:
            raise Exception('Invalid property value for vec2i: ' + str(prop))
    elif (type==GF_PROP_VEC2):
        if hasattr(prop, 'x') and hasattr(prop, 'y'):
            prop_val.value.vec2.x = prop.x
            prop_val.value.vec2.y = prop.y
        else:
            raise Exception('Invalid property value for vec2: ' + str(prop))
    elif (type==GF_PROP_VEC3I):
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z'):
            prop_val.value.vec3i.x = prop.x
            prop_val.value.vec3i.y = prop.y
            prop_val.value.vec3i.z = prop.z
        else:
            raise Exception('Invalid property value for vec3i: ' + str(prop))
    elif (type==GF_PROP_VEC3):
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z'):
            prop_val.value.vec3i.x = prop.x
            prop_val.value.vec3i.y = prop.y
            prop_val.value.vec3i.z = prop.z
        else:
            raise Exception('Invalid property value for vec3: ' + str(prop))
    elif (type==GF_PROP_VEC4I):
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z') and hasattr(prop, 'w'):
            prop_val.value.vec4i.x = prop.x
            prop_val.value.vec4i.y = prop.y
            prop_val.value.vec4i.z = prop.z
            prop_val.value.vec4i.w = prop.w
        else:
            raise Exception('Invalid property value for vec4i: ' + str(prop))
    elif (type==GF_PROP_VEC4):
        if hasattr(prop, 'x') and hasattr(prop, 'y') and hasattr(prop, 'z') and hasattr(prop, 'w'):
            prop_val.value.vec4.x = prop.x
            prop_val.value.vec4.y = prop.y
            prop_val.value.vec4.z = prop.z
            prop_val.value.vec4.w = prop.w
        else:
            raise Exception('Invalid property value for vec4: ' + str(prop))
    elif (type==GF_PROP_STRING) or (type==GF_PROP_STRING_NO_COPY) or (type==GF_PROP_NAME):
        prop_val.value.string = str(prop).encode('utf-8')
    elif (type==GF_PROP_DATA) or (type==GF_PROP_DATA_NO_COPY) or (type==GF_PROP_CONST_DATA):
        raise Exception('Setting data property from pythin not yet implemented !')
    elif (type==GF_PROP_POINTER):
        raise Exception('Setting pointer property from python not yet implemented !')
    elif (type==GF_PROP_STRING_LIST):
        raise Exception('Mapping from stringlist to python not implemented ')
    elif (type==GF_PROP_UINT_LIST):
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.uint_list.nb_items = len(prop)
        prop_val.value.uint_list.vals = (ctypes.c_uint * len(prop))(*prop)
    elif (type==GF_PROP_SINT_LIST):
        if isinstance(prop, list)==False:
            raise Exception('Property is not a list')
        prop_val.value.sint_list.nb_items = len(prop)
        prop_val.value.sint_list.vals = (ctypes.c_int * len(prop))(*prop)
    elif (type==GF_PROP_VEC2I_LIST):
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


_libgpac.gf_filter_set_rt_udta.argtypes = [GF_Filter, py_object]
_libgpac.gf_filter_get_rt_udta.argtypes = [GF_Filter]
_libgpac.gf_filter_get_rt_udta.restype = c_void_p

_libgpac.gf_filter_pid_set_udta.argtypes = [GF_Filter, py_object]
_libgpac.gf_filter_pid_get_udta.argtypes = [GF_Filter]
_libgpac.gf_filter_pid_get_udta.restype = c_void_p

_libgpac.gf_filter_pid_send_event.argtypes = [GF_FilterPid, POINTER(GF_FilterEvent)]


_libgpac.gf_filter_set_configure_ckb.argtypes = [GF_Filter, c_void_p]
@CFUNCTYPE(c_int, GF_Filter, GF_FilterPid, c_bool)
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


_libgpac.gf_filter_set_process_ckb.argtypes = [GF_Filter, c_void_p]
@CFUNCTYPE(c_int, GF_Filter)
def filter_cbk_process(_f):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    return filter.process();

_libgpac.gf_filter_set_process_event_ckb.argtypes = [GF_Filter, c_void_p]
@CFUNCTYPE(c_bool, GF_Filter, POINTER(GF_FilterEvent) )
def filter_cbk_process_event(_f, _evt):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    return filter.process_event(_evt.contents);

_libgpac.gf_filter_set_probe_data_cbk.argtypes = [GF_Filter, c_void_p]
@CFUNCTYPE(c_int, c_char_p, c_uint )
def filter_cbk_probe_data(_data, _size):
    obj = _libgpac.gf_filter_get_rt_udta(_f)
    filter = cast(obj, py_object).value
    return filter.probe_data(_data, _size);

_libgpac.gf_filter_set_reconfigure_output_ckb.argtypes = [GF_Filter, c_void_p]
@CFUNCTYPE(c_int, GF_Filter, GF_FilterPid )
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


_libgpac.gf_filter_pid_get_packet.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_get_packet.restype = GF_FilterPacket

_libgpac.gf_filter_pid_drop_packet.argtypes = [GF_FilterPid]

_libgpac.gf_filter_pid_new.argtypes = [GF_Filter]
_libgpac.gf_filter_pid_new.restype = GF_FilterPid

_libgpac.gf_filter_update_status.argtypes = [GF_Filter, c_uint, c_char_p]

_libgpac.gf_filter_ask_rt_reschedule.argtypes = [GF_Filter]
_libgpac.gf_filter_post_process_task.argtypes = [GF_Filter]


GF_SETUP_ERROR=0
GF_NOTIF_ERROR=1
GF_NOTIF_ERROR_AND_DISCONNECT=2

_libgpac.gf_filter_ask_rt_reschedule.argtypes = [GF_Filter, c_int, c_bool]
_libgpac.gf_filter_setup_failure.argtypes = [GF_Filter, c_int]
_libgpac.gf_filter_make_sticky.argtypes = [GF_Filter]
_libgpac.gf_filter_prevent_blocking.argtypes = [GF_Filter, c_bool]
_libgpac.gf_filter_block_eos.argtypes = [GF_Filter, c_bool]
_libgpac.gf_filter_set_max_extra_input_pids.argtypes = [GF_Filter, c_uint]
_libgpac.gf_filter_block_enabled.argtypes = [GF_Filter]
_libgpac.gf_filter_block_enabled.restype = c_bool
_libgpac.gf_filter_get_output_buffer_max.argtypes = [GF_Filter, POINTER(c_uint), POINTER(c_uint)]
_libgpac.gf_filter_all_sinks_done.argtypes = [GF_Filter]
_libgpac.gf_filter_all_sinks_done.restype = c_bool
_libgpac.gf_filter_get_num_events_queued.argtypes = [GF_Filter]

_libgpac.gf_filter_get_clock_hint.argtypes = [GF_Filter, POINTER(c_ulonglong), POINTER(GF_Fraction64)]
_libgpac.gf_filter_connections_pending.argtypes = [GF_Filter]
_libgpac.gf_filter_connections_pending.restype = c_bool
_libgpac.gf_filter_hint_single_clock.argtypes = [GF_Filter, c_ulonglong, GF_Fraction64]


class FilterCustom(Filter):

    def __init__(self, session, fname="Custom"):
        _filter = _libgpac.gf_fs_new_filter(session._sess, fname.encode('utf-8'), byref(last_errp))
        last_err = last_errp.value
        if (last_err<0): 
            raise Exception('Failed to create filter ' + URL + ': ' + e2s(last_err))
        #create base class
        Filter.__init__(self, session, _filter)
        #register with our filter bank
        session._filters.append(self)

        #setup callback udta and callback functions
        _libgpac.gf_filter_set_rt_udta(_filter, py_object(self) )
        if (hasattr(self, 'configure_pid')):
            _libgpac.gf_filter_set_configure_ckb(self._filter, filter_cbk_configure)

        if (hasattr(self, 'process')):
            _libgpac.gf_filter_set_process_ckb(self._filter, filter_cbk_process)

        if (hasattr(self, 'process_event')):
            _libgpac.gf_filter_set_process_event_ckb(self._filter, filter_cbk_process_event)

        if (hasattr(self, 'probe_data')):
            _libgpac.gf_filter_set_probe_data_ckb(self._filter, filter_cbk_probe_data)

        if (hasattr(self, 'reconfigure_output')):
            _libgpac.gf_filter_set_reconfigure_output_ckb(self._filter, filter_cbk_reconfigure_output)

        self.ipids=[]
        self.opids=[]


    def push_cap(self, pcode, prop, flag, priority=0):
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        prop_val = _make_prop(prop_4cc, pcode, prop)
        err = _libgpac.gf_filter_push_caps(self._filter, prop_4cc, byref(prop_val), prop_name, flag, priority)
        if (err<0): 
            raise Exception('Failed to push cap: ' + e2s(err))

    def new_pid(self):
        _pid = _libgpac.gf_filter_pid_new(self._filter)
        if _pid:
            pid_obj = FilterPid(self, _pid, False)
            _libgpac.gf_filter_pid_set_udta(_pid, py_object(pid_obj))
            self.opids.append(pid_obj)
            return pid_obj
        raise Exception('Failed to create output pid')

    def update_status(self, status, percent):
        _libgpac.gf_filter_update_status(self._filter, percent, status.encode('utf-8'))

    def reschedule(self, when=0):
        if when:
            _libgpac.gf_filter_ask_rt_reschedule(self._filter, when)
        else:
            _libgpac.gf_filter_post_process_task(self._filter, when)

    def notify_failure(self, err, error_type=GF_SETUP_ERROR):
        if error_type==GF_NOTIF_ERROR:
            _libgpac.gf_filter_notification_failure(self._filter, err, False)
        elif error_type==GF_NOTIF_ERROR_AND_DISCONNECT:
            _libgpac.gf_filter_notification_failure(self._filter, err, True)
        else:
            _libgpac.gf_filter_setup_failure(self._filter, err)

    def make_sticky(self):
        _libgpac.gf_filter_make_sticky(self._filter)

    def prevent_blocking(self, enable):
        _libgpac.gf_filter_prevent_blocking(self._filter, enable)

    def block_eos(self, enable):
        _libgpac.gf_filter_block_eos(self._filter, enable)

    def set_max_pids(self, max_pids):
        _libgpac.gf_filter_set_max_extra_input_pids(self._filter, max_pids)

    def hint_clock(self, clock_us, media_time):
        _libgpac.gf_filter_hint_single_clock(self._filter, clock_us, media_time)

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
        val = GF_Fraction64
        _libgpac.gf_filter_get_clock_hint(self._filter, None, byref(val))
        return val.value;

    @property 
    def connections_pending(self):
        return _libgpac.gf_filter_connections_pending(self._filter)

    #todo
    #add_source, add_destination, add_filter: not needed, acces to filter session

#
#        filter PID bindings
#

_libgpac.gf_filter_pid_copy_properties.argtypes = [GF_FilterPid, GF_FilterPid]
_libgpac.gf_filter_pck_forward.argtypes = [GF_FilterPacket, GF_FilterPid]

_libgpac.gf_filter_pid_set_property.argtypes = [GF_FilterPid, c_uint, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pid_set_property_str.argtypes = [GF_FilterPid, c_char_p, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pid_set_info.argtypes = [GF_FilterPid, c_uint, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pid_set_info_str.argtypes = [GF_FilterPid, c_char_p, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pid_clear_eos.argtypes = [GF_FilterPid, c_bool]
_libgpac.gf_filter_pid_check_caps.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_check_caps.restype = c_bool
_libgpac.gf_filter_pid_discard_block.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_allow_direct_dispatch.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_reset_properties.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_get_clock_info.argtypes = [GF_FilterPid, POINTER(c_longlong), POINTER(c_uint)]
_libgpac.gf_filter_pid_remove.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_is_filter_in_parents.argtypes = [GF_FilterPid, GF_Filter]
_libgpac.gf_filter_pid_is_filter_in_parents.restype = c_bool

_libgpac.gf_filter_pid_get_name.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_get_name.restype = c_char_p
_libgpac.gf_filter_pid_set_name.argtypes = [GF_FilterPid, c_char_p]

_libgpac.gf_filter_pid_is_eos.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_is_eos.restype = c_bool
_libgpac.gf_filter_pid_set_eos.argtypes = [GF_FilterPid]

_libgpac.gf_filter_pid_has_seen_eos.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_has_seen_eos.restype = c_bool

_libgpac.gf_filter_pid_would_block.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_would_block.restype = c_bool
_libgpac.gf_filter_pid_set_loose_connect.argtypes = [GF_FilterPid]

_libgpac.gf_filter_pid_set_framing_mode.argtypes = [GF_FilterPid, c_bool]

_libgpac.gf_filter_pid_get_max_buffer.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_get_max_buffer.restype = c_uint
_libgpac.gf_filter_pid_set_max_buffer.argtypes = [GF_FilterPid, c_uint]

_libgpac.gf_filter_pid_query_buffer_duration.argtypes = [GF_FilterPid, c_bool]
_libgpac.gf_filter_pid_query_buffer_duration.restype = c_ulonglong

_libgpac.gf_filter_pid_first_packet_is_empty.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_first_packet_is_empty.restype = c_bool

_libgpac.gf_filter_pid_get_first_packet_cts.argtypes = [GF_FilterPid, POINTER(c_ulonglong)]
_libgpac.gf_filter_pid_get_first_packet_cts.restype = c_bool

_libgpac.gf_filter_pid_get_packet_count.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_get_timescale.argtypes = [GF_FilterPid]

_libgpac.gf_filter_pid_set_clock_mode.argtypes = [GF_FilterPid, c_bool]
_libgpac.gf_filter_pid_set_discard.argtypes = [GF_FilterPid, c_bool]

_libgpac.gf_filter_pid_require_source_id.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_recompute_dts.argtypes = [GF_FilterPid, c_bool]


_libgpac.gf_filter_pid_get_min_pck_duration.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_is_playing.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_is_playing.restype = c_bool

_libgpac.gf_filter_pid_get_filter_name.argtypes = [GF_FilterPid]
_libgpac.gf_filter_pid_is_playing.restype = c_char_p


_libgpac.gf_filter_pid_caps_query.argtypes = [GF_FilterPid, c_uint]
_libgpac.gf_filter_pid_caps_query.restype = POINTER(GF_PropertyValue)
_libgpac.gf_filter_pid_caps_query_str.argtypes = [GF_FilterPid, c_char_p]
_libgpac.gf_filter_pid_caps_query_str.restype = POINTER(GF_PropertyValue)

_libgpac.gf_filter_pid_negociate_property.argtypes = [GF_FilterPid, c_uint, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pid_negociate_property_dyn.argtypes = [GF_FilterPid, c_char_p, POINTER(GF_PropertyValue)]


class BufferOccupancy:
  def __init__(self, max_units, nb_pck, max_dur, dur, is_final_flush):
    self.max_units = max_units
    self.nb_pck = nb_pck
    self.max_dur = max_dur
    self.dur = dur
    self.is_final_flush = is_final_flush


class FilterPid:
    def __init__(self, filter, pid, IsInput):
        self._filter = filter
        self._pid = pid
        self._cur_pck = None
        self._input = IsInput

    def send_event(self, evt):
        evt.base.on_pid = self._pid
        _libgpac.gf_filter_pid_send_event(self._pid, byref(evt))

    def remove(self):
        if self._input:
            raise Exception('Cannot remove input pid')
            return
        _libgpac.gf_filter_pid_remove(self._pid)

    def enum_props(self, callback_obj):
        self._filter._pid_enum_props_ex(callback_obj, self._pid)

    def get_prop(self, pname):
        return self._filter._pid_prop_ex(pname, self._pid, False)

    def get_info(self, pname):
        return self._filter._pid_prop_ex(pname, self._pid, True)

    def get_packet(self):
        if self._cur_pck:
            return self._cur_pck
        pck = _libgpac.gf_filter_pid_get_packet(self._pid)
        if pck:
            self._cur_pck = FilterPacket(pck, True)
            return self._cur_pck

    def drop_packet(self):
        if self._cur_pck:
            self._cur_pck = None
            _libgpac.gf_filter_pid_drop_packet(self._pid)


    def copy_props(self, ipid):
        if self._input:
            raise Exception('Cannot copy properties on input PID')
            return
        if ipid==None:
            return
        err = _libgpac.gf_filter_pid_copy_properties(self._pid, ipid._pid)
        if err<0:
            raise Exception('Cannot copy properties: ' + e2s(err) )
    
    def reset_props(self):
        if self._input:
            raise Exception('Cannot reset properties on input PID')
            return
        _libgpac.gf_filter_pid_reset_properties(self._pid)

    def forward(self, ipck):
        if self._input:
            raise Exception('Cannot copy properties on input PID')
            return
        if ipck==None:
            return
        _libgpac.gf_filter_pck_forward(ipck._pck, self._pid)

    def set_prop(self, pcode, prop):
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

        prop_val = _make_prop(prop_4cc, pcode, prop)
        if prop_4cc:
            _libgpac.gf_filter_pid_set_property(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_set_property_str(self._pid, prop_name, byref(prop_val))

    def set_info(self, pcode, prop):
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

        prop_val = _make_prop(prop_4cc, pcode, prop)
        if prop_4cc:
            _libgpac.gf_filter_pid_set_info(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_set_info_str(self._pid, prop_name, byref(prop_val))

    def clear_eos(_self, all_pids):
        _libgpac.gf_filter_pid_clear_eos(self._pid, all_pids)

    def check_caps(self):
        return _libgpac.gf_filter_pid_check_caps(self._pid)

    def discard_block(self):
        _libgpac.gf_filter_pid_discard_block(self._pid)

    def allow_direct_dispatch(self):
        _libgpac.gf_filter_pid_allow_direct_dispatch(self._pid)

    def get_clock_type(self):
        return _libgpac.gf_filter_pid_get_clock_info(self._pid, None, None)

    def get_clock_timestamp(self):
        timestamp = c_longlong(0)
        timescale = c_uint(0)
        _libgpac.gf_filter_pid_get_clock_info(self._pid, byref(timestamp), byref(timescale))
        v = GF_Fraction64();
        v.value.num = timestamp.value
        v.value.den = timescale.value
        return v

    def is_filter_in_parents(self, filter):
        return _libgpac.gf_filter_pid_is_filter_in_parents(self._pid, filter._f);

    def get_buffer_occupancy(self):
        max_units = c_uint(0)
        nb_pck = c_uint(0)
        max_dur = c_uint(0)
        dur = c_uint(0)
        in_final_flush = _libgpac.gf_filter_pid_get_buffer_occupancy(self._pid, byref(max_units), byref(nb_pck), byref(max_dur), byref(dur) )
        in_final_flush = not in_final_flush
        return BufferOccupancy(max_units, nb_pck, max_dur, dur, not_in_final_flush)

    def loose_connect(self):
        return _libgpac.gf_filter_pid_set_loose_connect(slef._pid)

    def set_framing(self, framed):
        _libgpac.gf_filter_pid_set_framing_mode(self._pid, framed)

    def set_clock_mode(self, cmode):
        _libgpac.gf_filter_pid_set_clock_mode(self._pid, cmode)

    def set_clock_mode(self, do_discard):
        _libgpac.gf_filter_pid_set_discard(self._pid, do_discard)

    def require_source_id(self):
        return _libgpac.gf_filter_pid_require_source_id(self._pid)

    def set_clock_mode(self, do_compute):
        _libgpac.gf_filter_pid_recompute_dts(self._pid, do_compute)

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

    def negociate_cap(self, pcode, prop):
        if self._input:
            raise Exception('Cannot negociate caps on input PID')
            return
        prop_4cc = pcode;
        prop_name = None;
        if isinstance(pcode, str):
            _pname = pcode.encode('utf-8')
            prop_4cc = _libgpac.gf_props_get_id(_pname)
            if prop_4cc==0:
                prop_name = _pname

        prop_val = _make_prop(prop_4cc, pcode, prop)
        if prop_4cc:
            _libgpac.gf_filter_pid_negociate_property(self._pid, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pid_negociate_property_dyn(self._pid, prop_name, byref(prop_val))

    def resolve_template(self, template, file_idx=0, suffix=None):
        res = create_string_buffer(2000)
        err = _libgpac.gf_filter_pid_resolve_file_template(self._pid, res, template.encode('utf-8'), file_idx, suffix);
        if err<0:
            raise Exception('Cannot resolve file template ' + template + ': ' + e2s(err))
        return res.raw.decode('utf-8')

    #fun:
    #new packets 


    @property
    def name(self):
        return _libgpac.gf_filter_pid_get_name(self._pid).decode('utf-8')

    @name.setter
    def name(self, val):
        _libgpac.gf_filter_pid_set_name(self._pid, val.encode('utf-8'))

    @property
    def filter_name(self):
        return _libgpac.gf_filter_pid_get_filter_name(self._pid).decode('utf-8')

    @property
    def eos(self):
        return _libgpac.gf_filter_pid_is_eos(self._pid)

    @eos.setter
    def eos(self):
        _libgpac.gf_filter_pid_set_eos(self._pid)

    @property
    def has_seen_eos(self):
        return _libgpac.gf_filter_pid_has_seen_eos(self._pid)

    @property
    def would_block(self):
        return _libgpac.gf_filter_pid_would_block(self._pid)

    @property
    def max_buffer(self):
        return _libgpac.gf_filter_pid_get_max_buffer(self._pid)

    @max_buffer.setter
    def max_buffer(self, value):
        return _libgpac.gf_filter_pid_set_max_buffer(self._pid, value)

    @property
    def buffer(self):
        return _libgpac.gf_filter_pid_query_buffer_duration(self._pid, False)

    @property
    def buffer_full(self):
        dur = _libgpac.gf_filter_pid_query_buffer_duration(self._pid, True)
        if dur:
            return True
        else:
            return False

    @property
    def first_empty(self):
        return _libgpac.gf_filter_pid_first_packet_is_empty(self._pid)

    @property
    def first_cts(self):
        cts = c_ulonglong(0)
        res = _libgpac.gf_filter_pid_get_first_packet_cts(self._pid, byref(cts))
        if not res:
            return None
        return cts.value

    @property
    def nb_pck_queued(self):
        return _libgpac.gf_filter_pid_get_packet_count(self._pid)

    @property
    def timescale(self):
        return _libgpac.gf_filter_pid_get_timescale(self._pid)

    @property
    def min_pck_dur(self):
        return _libgpac.gf_filter_pid_get_min_pck_duration(self._pid)

    @property
    def playing(self):
        return _libgpac.gf_filter_pid_is_playing(self._pid)

    #vars:
    #src_name, args, src_args, src_url, dst_url: probably not needed since the entire session is exposed to the custom filter




#
#        filter packet bindings
#

#getters
_libgpac.gf_filter_pck_get_dts.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_dts.restype = c_ulonglong
_libgpac.gf_filter_pck_set_dts.argtypes = [GF_FilterPacket, c_ulonglong]

_libgpac.gf_filter_pck_get_cts.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_cts.restype = c_ulonglong
_libgpac.gf_filter_pck_set_cts.argtypes = [GF_FilterPacket, c_ulonglong]

_libgpac.gf_filter_pck_get_sap.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_sap.restype = c_uint
_libgpac.gf_filter_pck_set_sap.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_duration.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_duration.restype = c_uint
_libgpac.gf_filter_pck_set_duration.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_data.argtypes = [GF_FilterPacket, POINTER(c_uint)]
_libgpac.gf_filter_pck_get_data.restype = POINTER(c_ubyte)

_libgpac.gf_filter_pck_get_framing.argtypes = [GF_FilterPacket, POINTER(c_bool), POINTER(c_bool)]
_libgpac.gf_filter_pck_set_framing.argtypes = [GF_FilterPacket, c_bool, c_bool]

_libgpac.gf_filter_pck_get_timescale.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_timescale.restype = c_uint

_libgpac.gf_filter_pck_get_interlaced.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_interlaced.restype = c_uint
_libgpac.gf_filter_pck_set_interlaced.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_corrupted.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_corrupted.restype = c_bool
_libgpac.gf_filter_pck_set_corrupted.argtypes = [GF_FilterPacket, c_bool]

_libgpac.gf_filter_pck_get_seek_flag.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_seek_flag.restype = c_bool
_libgpac.gf_filter_pck_set_seek_flag.argtypes = [GF_FilterPacket, c_bool]

_libgpac.gf_filter_pck_get_byte_offset.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_byte_offset.restype = c_ulonglong
_libgpac.gf_filter_pck_set_byte_offset.argtypes = [GF_FilterPacket, c_ulonglong]

_libgpac.gf_filter_pck_get_roll_info.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_roll_info.restype = c_short
_libgpac.gf_filter_pck_set_roll_info.argtypes = [GF_FilterPacket, c_short]

_libgpac.gf_filter_pck_get_crypt_flags.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_crypt_flags.restype = c_uint
_libgpac.gf_filter_pck_set_crypt_flags.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_clock_type.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_clock_type.restype = c_uint
_libgpac.gf_filter_pck_set_clock_type.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_carousel_version.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_carousel_version.restype = c_uint
_libgpac.gf_filter_pck_set_carousel_version.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_seq_num.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_seq_num.restype = c_uint
_libgpac.gf_filter_pck_set_seq_num.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_dependency_flags.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_dependency_flags.restype = c_uint
_libgpac.gf_filter_pck_set_dependency_flags.argtypes = [GF_FilterPacket, c_uint]

_libgpac.gf_filter_pck_get_frame_interface.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_get_frame_interface.restype = c_void_p

_libgpac.gf_filter_pck_is_blocking_ref.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_is_blocking_ref.restype = c_bool

_libgpac.gf_filter_pck_enum_properties.argtypes = [GF_FilterPacket, POINTER(c_uint), POINTER(c_uint), POINTER(c_char_p)]
_libgpac.gf_filter_pck_enum_properties.restype = POINTER(GF_PropertyValue)

_libgpac.gf_filter_pck_get_property.argtypes = [GF_FilterPacket, c_uint]
_libgpac.gf_filter_pck_get_property.restype = POINTER(GF_PropertyValue)
_libgpac.gf_filter_pck_get_property_str.argtypes = [GF_FilterPacket, c_char_p]
_libgpac.gf_filter_pck_get_property_str.restype = POINTER(GF_PropertyValue)

_libgpac.gf_filter_pck_ref_ex.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_ref_ex.restype = GF_FilterPacket
_libgpac.gf_filter_pck_unref.argtypes = [GF_FilterPacket]

_libgpac.gf_filter_pck_discard.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_set_readonly.argtypes = [GF_FilterPacket]
_libgpac.gf_filter_pck_send.argtypes = [GF_FilterPacket]

_libgpac.gf_filter_pck_merge_properties.argtypes = [GF_FilterPacket, GF_FilterPacket]

_libgpac.gf_filter_pck_set_property.argtypes = [GF_FilterPacket, c_uint, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pck_set_property_str.argtypes = [GF_FilterPacket, c_char_p, POINTER(GF_PropertyValue)]
_libgpac.gf_filter_pck_truncate.argtypes = [GF_FilterPacket, c_uint]


class FilterPacket:
    def __init__(self, pck, is_src):
        self._pck = pck
        self._is_src = is_src

    def enum_props(self, callback_obj):
        if (hasattr(callback_obj, 'on_prop_enum')==False):
            return

        pidx=c_uint(0)
        while True:
            _p4cc=c_uint(0)
            _pname=c_char_p(0)
            prop = _libgpac.gf_filter_pck_enum_properties(self._pck, byref(pidx), byref(_p4cc), byref(_pname))
            if (prop):
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

    def ref(self):
        self._pck = _libgpac.gf_filter_pck_ref_ex(self._pck)

    def unref(self):
        _libgpac.gf_filter_pck_unref(self._pck)


    def discard(self):
        if self._is_src:
            raise Exception('Cannot discard on source packet')
        _libgpac.gf_filter_pck_discard(self._pck)
        self._pck = None

    def readonly(self):
        if self._is_src:
            raise Exception('Cannot set readonly on source packet')
        _libgpac.gf_filter_pck_set_readonly(self.pck)

    def send(self):
        if self._is_src:
            raise Exception('Cannot send on source packet')
        _libgpac.gf_filter_pck_send(self.pck)

    def copy_props(self, ipck):
        if self._is_src:
            raise Exception('Cannot copy properties on source packet')
        if ipck==None:
            return
        err = _libgpac.gf_filter_pck_merge_properties(self._pid, ipck._pck)
        if err<0:
            raise Exception('Cannot copy properties: ' + e2s(err) )

    def set_prop(self, pcode, prop):
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

        prop_val = _make_prop(prop_4cc, pcode, prop)
        if prop_4cc:
            _libgpac.gf_filter_pck_set_property(self._pck, prop_4cc, byref(prop_val))
        else:
            _libgpac.gf_filter_pck_set_property_str(self._pck, prop_name, byref(prop_val))

    def truncate(self, len):
        if self._is_src:
            raise Exception('Cannot truncate on source packet')
        _libgpac.gf_filter_pck_truncate(self._pck, len)

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
        s = data[:size.value]
        return s
        

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
    def blocking(self):
        return _libgpac.gf_filter_pck_is_blocking_ref(self._pck)

    @property
    def deps(self):
        return _libgpac.gf_filter_pck_get_dependency_flags(self._pck)

    @deps.setter
    def deps(self, value):
        if self._is_src:
            raise Exception('Cannot set deps on source packet')
        return _libgpac.gf_filter_pck_set_dependency_flags(self._pck, value)

    @property
    def is_frame_ifce(self):
        p = _libgpac.gf_filter_pck_get_frame_interface(self._pck)
        if p:
            return True
        return False

    #todo
    #append



