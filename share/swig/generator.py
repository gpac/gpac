import os
import re
import json
import subprocess
import tempfile
import argparse
import pickle
import logging
from dataclasses import dataclass, field


class CustomFormatter(logging.Formatter):

    dark_grey = "\x1b[38;5;236m"
    grey = "\x1b[38;20m"
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_red = "\x1b[31;1m"
    reset = "\x1b[0m"
    format = (
        "%(asctime)s - %(name)s - %(levelname)s - %(message)s (%(filename)s:%(lineno)d)"
    )

    FORMATS = {
        logging.DEBUG: dark_grey + format + reset,
        logging.INFO: grey + format + reset,
        logging.WARNING: yellow + format + reset,
        logging.ERROR: red + format + reset,
        logging.CRITICAL: bold_red + format + reset,
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


# Set up logging
logger = logging.getLogger("swig_generator")
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
ch.setFormatter(CustomFormatter())
logger.addHandler(ch)


# Change to the root directory of the project
SWIG_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# Public headers to include in the SWIG file
PUBLIC_HEADERS = [
    "gpac/filters.h",
    "gpac/tools.h",
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
    "gf_user_credentials_find_for_site",  # requires manual mapping
    "gf_user_credentials_register",  # requires manual mapping
    "gf_props_reset_single",  # requires manual mapping
    "gf_filter_release_property",  # requires manual mapping
    # More functions may be added dynamically
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
# it's clear what they create
CROSS_STRUCT_CTORS = {
    "gf_filter_pck_new": {
        "parent": "__gf_filter_pid",
        "remove": r"^gf_filter",
    },
    "gf_dm_sess_new": {
        "parent": "__gf_download_manager",
        "remove": r"^gf_dm",
    },
    "gf_dm_new": {
        "parent": "__gf_filter_session",
        "remove": r"^gf",
    },
}

# Some structs are not compliant with SWIG naming conventions
# and we need to rename them
EXTRA_INTERNAL_RENAME = {
    "__gf_prop_val": "_gf_prop_val",
    "__gf_prop_val_value": "_gf_prop_val_value",
    "__gf_filter_event": "_gf_filter_event",
}

# Some structs are aliased to others
# and we need merge them with the relevant struct
STRUCT_MERGES = {"GF_DownloadFilterSession": "GF_FilterSession"}

# Some keywords are reserved in some languages
# and we need to ignore them
RESERVED_KEYWORDS = ["register", "del"]

# Extra type definitions for SWIG
# These are used to generate pointer functions for the types
EXTRA_TYPE_DEFINITIONS = ["GF_Err"]


def preprocess_headers(headers):
    """
    Preprocess the headers using the GCC preprocessor.
    """
    with tempfile.TemporaryDirectory() as temp_dir:
        # Create a temporary source file to include the headers
        with tempfile.NamedTemporaryFile(
            delete=False, suffix=".c", dir=temp_dir
        ) as c_source:
            for header in headers:
                c_source.write(f'#include "{header}"\n'.encode())
            c_source_path = c_source.name

        # Clang
        cmd = [
            "clang",
            "-Xclang",
            "-ast-dump=json",
            "-fsyntax-only",
            "-I" + ROOT_DIR,
            *["-I" + os.path.join(ROOT_DIR, d) for d in INCLUDE_DIRS],
            "-DGPAC_HAVE_CONFIG_H",
            c_source_path,
        ]
        logger.debug(f"Running command: {' '.join(cmd)}")
        output = subprocess.run(cmd, check=True, capture_output=True, text=True)
        with open(os.path.join(tempfile.gettempdir(), "ast.json"), "w") as f:
            f.write(output.stdout)
        return json.loads(output.stdout)


@dataclass
class Function:
    name: str
    alias: str = None
    ctor: bool = False
    dtor: bool = False
    static: bool = False
    args: list = field(default_factory=list)
    return_type: str = None

    def __post_init__(self):
        if not self.alias:
            self.alias = self.name

    def __str__(self):
        """
        Generate the function signature.
        """
        args_str = ", ".join(
            f"{arg['type']} {arg['name']}" for arg in self.args if arg["name"]
        )
        return_type = self.return_type if self.return_type else "void"
        return f"{return_type} {self.name}({args_str})"

    def copy(self):
        """
        Create a copy of the Function instance.
        """
        return Function(
            name=self.name,
            alias=self.alias,
            ctor=self.ctor,
            dtor=self.dtor,
            static=self.static,
            args=[arg.copy() for arg in self.args],
            return_type=self.return_type,
        )


def search_functions(node, functions=[]):
    """
    Recursively search for function declarations in the AST.
    """
    if isinstance(node, dict):
        if node.get("kind") == "FunctionDecl":
            # Skip external functions
            if node.get("storageClass") == "extern":
                return functions

            # Skip non-exported functions
            name = node.get("name")
            if not name or not name.startswith("gf_"):
                return functions

            # Skip functions that are in the ignore list
            if any(name.startswith(ignore) for ignore in IGNORE_FNS):
                return functions

            return_type = node.get("type", {}).get("qualType", "void")
            return_type = re.sub(r"\s*\(.*\)$", "", return_type)
            args = [
                {
                    "name": arg.get("name", ""),
                    "type": arg.get("type", {}).get("qualType", "void"),
                }
                for arg in node.get("inner", [])
                if arg.get("kind") == "ParmVarDecl"
            ]
            functions.append(Function(name=name, args=args, return_type=return_type))
        for key, value in node.items():
            if isinstance(value, (dict, list)):
                search_functions(value, functions)
    elif isinstance(node, list):
        for item in node:
            search_functions(item, functions)
    return functions


@dataclass
class Struct:
    internal_name: str = None
    name: str = None

    def __str__(self):
        """
        Generate the struct name.
        """
        return f"{self.name} ({self.internal_name})"


def search_structs(node, structs=[]):
    """
    Recursively search for struct declarations in the AST.
    """
    if isinstance(node, dict):
        if node.get("kind") == "TypedefDecl":
            type = node.get("type", {}).get("qualType", "")
            if type.startswith("struct "):
                internal_name = type.split(" ")[1]
                if internal_name in IGNORE_STRUCTS:
                    return structs
                name = node.get("name", "")
                structs.append(Struct(internal_name=internal_name, name=name))
        for key, value in node.items():
            if isinstance(value, (dict, list)):
                search_structs(value, structs)
    elif isinstance(node, list):
        for item in node:
            search_structs(item, structs)
    return structs


def prepare_structs(structs, functions):
    # Collect functions for structs
    struct_functions = {}
    for struct in structs:
        if struct.internal_name.startswith("__gf"):
            # These are the private structs that we expose
            struct_functions[struct.name] = {
                "iname": struct.internal_name,
                "self_type": f"{struct.name} *",
                "functions": [],
                "constructors": [],
                "extra_functions": [],
                "destructor": None,
            }
    logger.info(f"Found {len(struct_functions)} internal structs.")

    # Add the functions to the struct
    logger.info("Adding functions to structs.")
    for struct_name, struct_info in struct_functions.items():
        for fn in functions:
            if not fn.args:
                continue

            # Check if the first argument of the function is a pointer to the struct
            if fn.args[0]["type"] != f"{struct_name} *":
                continue

            # Skip arguments with function pointers
            skip = False
            for arg in fn.args:
                if re.search(r"\s*\(.*\)$", arg["type"]):
                    logger.info(
                        f"Skipping {fn.name} because it has a function pointer argument(s)"
                    )
                    IGNORE_FNS.append(fn.name)
                    skip = True
                    break
            if skip:
                continue

            # Copy the function declaration
            fn_decl = fn.copy()

            # Check if the function is a cross-struct constructor
            is_special_case = False
            for special_fn, special_info in CROSS_STRUCT_CTORS.items():
                if fn.name.startswith(special_fn):
                    fn_decl.alias = re.sub(special_info["remove"], "", fn.name)
                    fn_decl.alias = fn_decl.alias.lstrip("_")

                    # Find the parent struct
                    for struct in structs:
                        if struct.internal_name == special_info["parent"]:
                            logger.debug(
                                f"{fn.name} is a cross-struct constructor from {struct.name}"
                            )
                            struct_functions[struct.name]["functions"].append(fn_decl)
                            assert (
                                fn.args[0]["type"] == f"{struct.name} *"
                            ), f"Function {fn.name}'s first argument ({fn.args[0]['type']}) does not match the parent struct {struct.name} ({struct.internal_name})"
                            break
                    else:
                        logger.error(
                            f"Parent struct {special_info['parent']} not found for function {fn.name}, skipping"
                        )
                    is_special_case = True
                    break
            if is_special_case:
                continue

            # Check if the function starts with the prefix including the alias
            prefix = struct_info["iname"].replace("__", "")
            alias = STRUCT_FN_ALIAS.get(struct_info["iname"], None)
            if fn.name.startswith(prefix):
                # Remove the prefix from the function name
                fn_decl.alias = fn.name[len(prefix) :]
            # Check if the function starts with the alias
            elif alias and re.match(alias, fn.name):
                # Remove the alias from the function name
                fn_decl.alias = re.sub(alias, "", fn.name)
                assert (
                    fn.args[0]["type"] == f"{struct_name} *"
                ), f"Function {fn.name}'s first argument ({fn.args[0]['type']}) does not match the parent struct {struct_name} ({struct_info['iname']})"
            else:
                logger.error(
                    f"Function {fn.name} does not start with prefix {prefix} or alias {alias}, skipping"
                )
                continue

            # Remove leading underscores from the function name
            fn_decl.alias = fn_decl.alias.lstrip("_")
            struct_info["functions"].append(fn_decl)

    # Remove empty structs
    struct_functions = {
        k: v for k, v in struct_functions.items() if len(v["functions"]) > 0
    }

    # Merge structs
    for struct_name, merge_name in STRUCT_MERGES.items():
        if merge_name in struct_functions:
            struct_functions[merge_name]["functions"].extend(
                struct_functions[struct_name]["functions"]
            )
            del struct_functions[struct_name]
    logger.info(
        f"Found {len(struct_functions)} structs with functions after filtering and merging."
    )

    # Decide on constructors and destructors
    logger.info("Deciding on constructors and destructors.")
    for struct_name, struct_info in struct_functions.items():
        prefix = STRUCT_FN_ALIAS.get(
            struct_info["iname"], rf"^{struct_info["iname"].replace("__", "")}"
        )
        for fn in functions:
            if any(fn.name == sfn.alias for sfn in struct_info["functions"]):
                continue
            if re.match(prefix, fn.name):
                first_arg = fn.args[0]["type"]
                if first_arg == f"{struct_name} *":
                    if (
                        re.match(r".*(del|unref)$", fn.name)
                        and fn.return_type == "void"
                    ):
                        if struct_info["destructor"] is not None:
                            logger.warning(
                                f"Multiple destructors found for struct {struct_name}, skipping {fn.name}"
                            )
                            continue

                        # Create the function declaration
                        fn_decl = fn.copy()
                        fn_decl.dtor = True
                        assert (
                            len(fn_decl.args) == 1
                        ), f"Destructor {fn.name} should have one argument"
                        logger.debug(
                            f"Found destructor {fn.name} for struct {struct_name}"
                        )
                        struct_info["destructor"] = fn_decl

                        # Remove the function from the list of functions
                        for sfn in struct_info["functions"]:
                            if sfn.name == fn.name:
                                struct_info["functions"].remove(sfn)
                                logger.debug(
                                    f"Removing function {fn.name} from struct {struct_name}"
                                )
                                break
                    continue
                # Check if this is a constructor
                elif fn.return_type == f"{struct_name} *":
                    # Create the function declaration
                    fn_decl = fn.copy()

                    # Check if default constructor
                    if re.match(r".*new$", fn.name):
                        fn_decl.ctor = True
                        assert (
                            fn_decl.return_type == f"{struct_name} *"
                        ), f"Function {fn.name} should return {struct_name} *"
                        logger.debug(
                            f"Found constructor {fn.name} for struct {struct_name}"
                        )
                    else:
                        fn_decl.static = True
                        fn_decl.alias = re.sub(prefix, "", fn.name)
                        fn_decl.alias = fn_decl.alias.lstrip("_")
                        logger.debug(
                            f"Found static constructor {fn.name} ({fn.alias}) for struct {struct_name}"
                        )

                    # Add the function to the struct
                    struct_info["constructors"].append(fn_decl)

    # Check ctor and dtor for structs
    for struct_name, struct_info in struct_functions.items():
        if not struct_info["constructors"]:
            logger.warning(
                f"Struct {struct_name} has no constructor, please check the generated file."
            )
        if not struct_info["destructor"]:
            logger.warning(
                f"Struct {struct_name} has no destructor, please check the generated file."
            )

    # Check unmapped functions for constructors
    logger.info("Checking unmapped functions for constructors.")
    for struct_name, struct_info in struct_functions.items():
        prefix = STRUCT_FN_ALIAS.get(
            struct_info["iname"], rf"^{struct_info["iname"].replace("__", "")}"
        )
        for fn in functions:
            if (
                any(fn.name == sfn.name for sfn in struct_info["functions"])
                or any(fn.name == sfn.name for sfn in struct_info["constructors"])
                or (
                    struct_info["destructor"]
                    and fn.name == struct_info["destructor"].name
                )
                or fn.name in IGNORE_FNS
            ):
                continue
            if re.match(prefix, fn.name):
                fn_decl = fn.copy()
                fn_decl.alias = re.sub(prefix, "", fn.name)
                fn_decl.alias = fn_decl.alias.lstrip("_")
                fn_decl.static = True
                struct_info["extra_functions"].append(fn_decl)
                logger.debug(
                    f"Found extra static function {fn.name} for struct {struct_name}"
                )

    return struct_functions


def generate_swig_file(struct_functions, structs, lang):
    swig_out = ""

    # Add the module declaration
    swig_out += "%module gpac\n"

    # Add the headers
    swig_out += "\n// Include the headers\n"
    swig_out += "%{\n"
    for header in sorted(set(PUBLIC_HEADERS)):
        swig_out += f"#include <{header}>\n"
    swig_out += "%}\n"

    # Add type definitions for scalar types
    swig_out += """
// Handle scalar types
%include "stdint.i"
typedef uint64_t  u64;
typedef int64_t   s64;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint16_t  u16;
typedef int16_t   s16;
typedef uint8_t   u8;
typedef int8_t    s8;

%include <cpointer.i>
%pointer_functions(uint64_t, u64p);
%pointer_functions(uint32_t, u32p);
%pointer_functions(uint16_t, u16p);
%pointer_functions(uint8_t, u8p);
%pointer_functions(int64_t, s64p);
%pointer_functions(int32_t, s32p);
%pointer_functions(int16_t, s16p);
%pointer_functions(int8_t, s8p);
"""

    # Add extra type definitions
    if EXTRA_TYPE_DEFINITIONS:
        swig_out += "\n// Extra type definitions\n"
        for type in sorted(EXTRA_TYPE_DEFINITIONS):
            swig_out += f"%pointer_functions({type}, {type}p);\n"
    swig_out += "\n"

    # Add the forward declarations
    swig_out += "// Forward declarations\n"
    swig_out += "%{\n"
    for _, struct_info in sorted(struct_functions.items(), key=lambda s: s[0]):
        swig_out += f"struct {struct_info["iname"]} {{}};\n"
    swig_out += "%}\n"

    # Rename non-compliant structs
    swig_out += "\n// Rename non-compliant structs\n"
    renamed = set()
    for sname, info in sorted(struct_functions.items(), key=lambda s: s[0]):
        if (
            info["iname"].startswith("__gf")
            and info["iname"] not in renamed
            and len(info["functions"]) > 0
        ):
            swig_out += f"%rename({sname}) {info['iname']};\n"
            swig_out += f"struct {info['iname']} {{}};\n\n"
            renamed.add(info["iname"])

    # Rename extra internal types
    swig_out += "// Rename extra internal types\n"
    for sname, rename in sorted(EXTRA_INTERNAL_RENAME.items()):
        swig_out += f"%rename({rename}) {sname};\n"
        logger.debug(
            f"Renaming {sname} to {rename} for compatibility. Please check the generated file."
        )
    for sname in sorted(structs, key=lambda s: s.name):
        if (
            not sname.internal_name.startswith("__gf")
            or sname.internal_name in renamed
            or sname.internal_name in EXTRA_INTERNAL_RENAME
        ):
            continue
        swig_out += f"%rename({sname.internal_name.replace('__', '_')}) {sname.internal_name};\n"
        logger.debug(
            f"Renaming {sname.internal_name} to {sname.internal_name.replace('__', '_')} for compatibility. Please check the generated file."
        )

    if "go" == lang:
        swig_out += "\n// Rename non-compliant types for Go\n"
        swig_out += f"%rename(gf_type) type;\n"
        logger.warning(
            "Renaming 'type' to 'gf_type' for Go compatibility. Please check the generated file."
        )

    # Global ignores
    swig_out += "\n// Global ignores\n"
    for fn in sorted(IGNORE_FNS):
        swig_out += f"%ignore {fn};\n"
    swig_out += ""

    # Extend structs
    for struct_name, struct_info in sorted(
        struct_functions.items(), key=lambda s: s[0]
    ):
        internal_name = struct_info["iname"]
        swig_out += f"\n// Extend {struct_name}\n"

        # Ignore mapped functions
        fns = set()
        for fn in struct_info["constructors"]:
            fns.add(fn.name)
        for fn in struct_info["extra_functions"]:
            fns.add(fn.name)
        for fn in struct_info["functions"]:
            fns.add(fn.name)
        for fn in sorted(fns):
            swig_out += f"%ignore {fn};\n"
        swig_out += "\n"

        # Don't generate default ctor/dtor
        swig_out += f"%nodefaultctor {internal_name};\n"
        swig_out += f"%nodefaultdtor {internal_name};\n"
        swig_out += "\n"

        # Extend the struct
        swig_out += f"%extend {internal_name} {{\n"

        def csn(name, is_ctor=False):
            """Convert the struct name to the internal name."""
            public_type = re.match(r"(GF_[^\s]+)", name)
            if public_type:
                # Check if we have the struct entry
                if public_type.group(1) in struct_functions:
                    if is_ctor:
                        return name.replace(
                            public_type.group(1),
                            struct_functions[public_type.group(1)]["iname"],
                        )
                    return name.replace(
                        public_type.group(1),
                        f"struct {struct_functions[public_type.group(1)]['iname']}",
                    )
            return name

        def get_args(fn):
            offset = 0 if fn.ctor or fn.static else 1
            public = ", ".join(
                map(
                    csn,
                    [
                        arg["type"] + " " + arg["name"]
                        for arg in filter(
                            lambda x: x["type"] != "GF_Err *", fn.args[offset:]
                        )
                    ],
                )
            )
            internal = ", ".join(
                map(
                    csn,
                    (["$self"] if offset == 1 else [])
                    + [arg["name"] for arg in fn.args[offset:]],
                )
            )
            return public, internal

        def wrap_gf_err(fn, returns):
            # There must be only one "GF_Err *" argument
            errp_count = 0
            for arg in fn.args:
                if arg["type"] == "GF_Err *":
                    errp_count += 1
            assert (
                errp_count <= 1
            ), f"Function {fn.name} has more than one GF_Err * argument"

            if errp_count == 0 and fn.return_type != "GF_Err":
                internal = get_args(fn)[1]
                return f"\t\treturn {fn.name}({internal});"

            if errp_count == 1:
                assert (
                    fn.return_type != "GF_Err"
                ), f"Function {fn.name} has GF_Err * argument and returns GF_Err"

                # Get the error variable name
                err_var = None
                for arg in fn.args:
                    if arg["type"] == "GF_Err *":
                        err_var = arg["name"]
                        break

                # Convert the error variable to a pointer
                new_args = get_args(fn)[1].split(", ")
                for i in range(len(new_args)):
                    if new_args[i] == err_var:
                        new_args[i] = f"&{err_var}"
                new_args = ", ".join(new_args)

                out = f"\t\tGF_Err {err_var} = GF_OK;\n"
                out += f"\t\t{returns} swig_ret = {fn.name}({new_args});\n"
                out += f"\t\tif ({err_var} != GF_OK) {{\n"
                out += f"\t\t\tSWIG_exception(SWIG_RuntimeError, gf_error_to_string({err_var}));\n"
                out += f"\t\t}}\n"
                out += "\t\tfail:\n"
                out += f"\t\treturn swig_ret;"

                return out
            else:
                out = f"\t\tGF_Err swig_ret = {fn.name}({get_args(fn)[1]});\n"
                out += f"\t\tif (swig_ret != GF_OK) {{\n"
                out += f"\t\t\tSWIG_exception(SWIG_RuntimeError, gf_error_to_string(swig_ret));\n"
                out += f"\t\t}}\n"
                out += "\t\tfail:\n"
                out += f"\t\t\t(void)swig_ret;"
                return out

        # Add the constructor
        for i, fn in enumerate(
            sorted(struct_info["constructors"], key=lambda f: f.name)
        ):
            public, internal = get_args(fn)

            # Check function alias for reserved keywords
            if any(fn.alias == kw for kw in RESERVED_KEYWORDS):
                logger.warning(
                    f"Function alias {fn.alias} is a reserved keyword, skipping {fn.name}"
                )
                continue

            if fn.ctor:
                swig_out += f"""
\t{csn(struct_name, True)}({public}) {{
{wrap_gf_err(fn, csn(struct_name) + " *")}
\t}}
"""
            elif fn.static:
                swig_out += f"""
\tstatic {csn(fn.return_type)} {fn.alias}({public}) {{
{wrap_gf_err(fn, csn(fn.return_type))}
\t}}
"""
            else:
                raise ValueError(
                    f"Constructor {fn.name} is not a constructor or static function"
                )

        # Add the destructor
        if struct_info["destructor"]:
            public, internal = get_args(struct_info["destructor"])
            swig_out += f"""
\t~{csn(struct_name, True)}() {{
\t\t{struct_info["destructor"].name}({internal});
\t}}
"""

        # Add the functions
        other_fns = sorted(
            struct_info["functions"] + struct_info["extra_functions"],
            key=lambda f: f.name,
        )
        for i, fn in enumerate(other_fns):
            public, internal = get_args(fn)

            # Check function alias for reserved keywords
            if any(fn.alias == kw for kw in RESERVED_KEYWORDS):
                logger.warning(
                    f"Function alias {fn.alias} is a reserved keyword, skipping {fn.name}"
                )
                continue

            if csn(fn.return_type) == "GF_Err":
                swig_out += f"""
\t{"static " if fn.static else ""}void {fn.alias}({public}) {{"""
            else:
                swig_out += f"""
\t{"static " if fn.static else ""}{csn(fn.return_type)} {fn.alias}({public}) {{"""
            swig_out += f"""\n{wrap_gf_err(fn, csn(fn.return_type))}\n\t}}\n"""

        swig_out += "}\n"

        # Signal the constructors
        ctors = sorted(struct_info["constructors"], key=lambda f: f.name)
        ctors = [fn for fn in ctors if fn.static]
        if ctors:
            swig_out += "\n"
        for fn in ctors:
            swig_out += f"%newobject {csn(struct_name, True)}::{fn.alias};\n"

    # Include the headers
    swig_out += "\n// Include the headers\n"
    swig_out += f"%immutable;\n"
    for header in PUBLIC_HEADERS:
        swig_out += f"%include <{header}>\n"
    swig_out += f"%mutable;\n"

    return swig_out


def main(args):
    PICKLE_FILE = os.path.join(tempfile.gettempdir(), "swig_generator_objects.pickle")
    if os.path.exists(PICKLE_FILE) and not args.no_cache:
        logger.info("Loading cached functions and structs from pickle file.")
        with open(PICKLE_FILE, "rb") as f:
            functions, structs = pickle.load(f)
    else:
        # Preprocess the headers
        ast = preprocess_headers(PUBLIC_HEADERS)

        # Search for functions and structs
        logger.info("Searching for functions and structs in the AST.")
        functions = search_functions(ast)
        structs = search_structs(ast)

        # Save the functions and structs to a pickle file
        with open(PICKLE_FILE, "wb") as f:
            pickle.dump((functions, structs), f)

    # Prepare the structs
    logger.info("Preparing structs.")
    struct_functions = prepare_structs(structs, functions)

    # Generate the SWIG file
    logger.info("Generating SWIG file.")
    swig_out = generate_swig_file(struct_functions, structs, args.lang)

    # Write the SWIG file
    swig_file = os.path.join(SWIG_DIR, "gpac.i")
    with open(swig_file, "w") as f:
        f.write(swig_out)
    logger.info(f"SWIG file generated at {swig_file}")

    # Create the language bindings
    logger.info(f"Creating language bindings for {args.lang}.")
    if args.lang not in ["python", "go", "node"]:
        logger.error(f"Unsupported language: {args.lang}")
        exit(1)
    if args.lang == "node":
        options = ["-javascript", "-node"]
    elif args.lang == "go":
        options = ["-go"]
    else:
        options = ["-python"]

    # Create or empty the output directory
    output_dir = os.path.join(SWIG_DIR, args.lang)
    os.makedirs(output_dir, exist_ok=True)

    # Generate the language bindings
    cmd = [
        "swig",
        *options,
        "-I" + ROOT_DIR,
        *["-I" + os.path.join(ROOT_DIR, d) for d in INCLUDE_DIRS],
        "-outdir",
        output_dir,
        "-o",
        os.path.join(
            output_dir, "gpac_wrap" + (".cpp" if args.lang == "node" else ".c")
        ),
        os.path.join(SWIG_DIR, "gpac.i"),
    ]
    subprocess.run(cmd, check=True)
    logger.info(f"Language bindings for {args.lang} created in {args.lang}/gpac_wrap.c")
    logger.info("SWIG generation completed.")


if __name__ == "__main__":
    # Set up argument parser
    parser = argparse.ArgumentParser(
        description="Generate language bindings using SWIG."
    )
    parser.add_argument("--no-cache", action="store_true", help="Disable caching.")
    parser.add_argument("-s", "--silent", action="store_true", help="Suppress output.")
    parser.add_argument(
        "--language",
        "-l",
        dest="lang",
        default="node",
        help="Specify the language to generate bindings for.",
        choices=["python", "go", "node"],
    )
    args = parser.parse_args()

    if args.silent:
        logger.setLevel(logging.WARNING)
    else:
        logger.setLevel(logging.DEBUG)

    main(args)
