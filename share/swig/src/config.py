import os

# Public headers to include in the SWIG file
PUBLIC_HEADERS = [
    "gpac/filters.h",
    "gpac/tools.h",
    "gpac/version.h",
]

# Include directories during SWIG generation
# These directories are relative to the root directory
INCLUDE_DIRS = ["include"]

# Ignore these functions during SWIG generation
IGNORE_FNS = [
    "gf_vfprintf",  # issue
    "gf_log_va_list",  # issue
    "gf_bin128_parse",  # issue
    "gf_filter_set_probe_data_cbk",  # issue
    "gf_filter_pid_raw_new",  # defined in __gf_filter and __gf_filter_pid
    "gf_filter_pid_raw_gmem",  # defined in __gf_filter and __gf_filter_pid
    "gf_user_credentials_find_for_site",  # requires manual mapping
    "gf_user_credentials_register",  # requires manual mapping
    "gf_props_reset_single",  # requires manual mapping
    "gf_filter_release_property",  # requires manual mapping
    "gf_fileio_new", # printf callback uses __va_list_tag which is not supported
    # More functions may be added dynamically
]

# Ignore these function pointers during SWIG generation
# Mapping: (function_name, argument_name)
IGNORE_FN_POINTERS = [
    ("gf_fs_print_all_connections", "print_fn"), # because of ...
    ("gf_filter_print_all_connections", "print_fn"), # because of ...
    ("gf_log_set_callback", "cbk"), # because of __va_list_tag
    ("gf_dm_set_auth_callback", "get_pass") # too complex
]

# Ignore these structs during SWIG generation
IGNORE_STRUCTS = ["__gf_blob"]

# Sometimes struct name and function name are not the same
# and we need to rename them
STRUCT_FN_ALIAS = {
    "__gf_file_io": r"^gf_fileio",
    "__gf_filter": r"^gf_filter_(?!pid|pck)",
    "__gf_filter_session": r"^gf_fs_",
    "__gf_download_session": r"^gf_dm_sess",
    "__gf_download_manager": r"^gf_dm_(?!sess)",
}

# Some struct members generate other structs, we need to rename them so that
# it's clear what they create.
# So for example: GF_FilterPid would be used as a first argument to gf_filter_pck_new_ref, but the default behavior would assume it's used by GF_Filter. We change the parent to __gf_filter_pid and tell our generator to remove the gf_filter prefix.
CROSS_STRUCT_CTORS = {
    r"^gf_filter_pck_new": {
        "parent": "__gf_filter_pid",
        "remove": r"^gf_filter",
    },
    r"^gf_dm_sess_new": {
        "parent": "__gf_download_manager",
        "remove": r"^gf_dm",
    },
    r"^gf_dm_new": {
        "parent": "__gf_filter_session",
        "remove": r"^gf",
    },
}

# Some structs are not compliant with SWIG naming conventions
# and we need to rename them
# Mapping: original_name: new_name
EXTRA_INTERNAL_RENAME = {
    "__gf_prop_val": "_gf_prop_val",
    "__gf_prop_val_value": "_gf_prop_val_value",
    "__gf_filter_event": "_gf_filter_event",
}

# Some structs are aliased to others
# and we need merge them with the relevant struct
# Mapping: alias_name: original_name (to merge with)
STRUCT_MERGES = {"GF_DownloadFilterSession": "GF_FilterSession"}

# Some keywords are reserved in some languages
# and we need to ignore them
RESERVED_KEYWORDS = {"go": ["type"], "python": ["register", "del"]}


# Get the directory paths
SWIG_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


# Complete export of config variables
class Config:
    headers = PUBLIC_HEADERS
    include_dirs = INCLUDE_DIRS
    ignore_fns = IGNORE_FNS
    ignore_fn_pointers = IGNORE_FN_POINTERS
    ignore_structs = IGNORE_STRUCTS
    struct_fn_alias = STRUCT_FN_ALIAS
    cross_struct_ctors = CROSS_STRUCT_CTORS
    extra_internal_rename = EXTRA_INTERNAL_RENAME
    struct_merges = STRUCT_MERGES
    reserved_keywords = RESERVED_KEYWORDS

    @staticmethod
    def for_jinja():
        return {
            "headers": Config.headers,
            "ignore_fns": Config.ignore_fns,
            "reserved_keywords": Config.reserved_keywords,
        }
