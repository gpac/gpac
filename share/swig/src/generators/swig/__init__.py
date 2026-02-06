import re
from logger import logger
from config import Config


class SWIGGenerator:
    def __init__(self, struct_functions, info):
        self.struct_functions = struct_functions
        self.structs, self.functions, self.enums = info

    def get_globals(self):
        # Return a dictionary of global variables needed for SWIG generation
        return {
            "forward_declarations": self.get_forward_declarations(),
            "non_compliant_structs": self.get_non_compliant_structs(),
            "extra_internal_renames": self.get_extra_internal_renames(),
            "struct_functions": self.get_struct_functions(),
            "function_pointers": self.get_function_pointers(),
            "struct_ignored_functions": self.get_struct_ignored_functions,
            "is_ignored_struct": self.is_ignored_struct,
            "csn": self.csn,
            "get_args": self.get_args,
        }

    def get_forward_declarations(self):
        # Return forward declarations for SWIG file
        yield from (
            struct_info["iname"]
            for _, struct_info in sorted(self.struct_functions.items())
            if struct_info["iname"] not in Config.ignore_structs
        )

    def get_non_compliant_structs(self, log=True):
        # Return a list of non-compliant structs
        non_compliant = set()
        for sname, info in sorted(self.struct_functions.items(), key=lambda s: s[0]):
            if (
                info["iname"].startswith("__gf")
                and len(info["functions"]) > 0
                and info["iname"] not in non_compliant
            ):
                if log:
                    logger.debug(
                        f"Struct {sname} is non-compliant and will be renamed to {info['iname']}"
                    )
                non_compliant.add(info["iname"])
                yield (sname, info["iname"])

    def get_extra_internal_renames(self):
        for sname, rename in sorted(Config.extra_internal_rename.items()):
            yield (sname, rename)

        for sname in sorted(self.structs, key=lambda s: s.name):
            if (
                not sname.internal_name.startswith("__gf")
                or sname.internal_name
                in map(lambda x: x[1], self.get_non_compliant_structs(log=False))
                or sname.internal_name in Config.extra_internal_rename
            ):
                continue
            logger.debug(
                f"Renaming {sname.internal_name} to {sname.internal_name.replace('__', '_')} for compatibility. Please check the generated file."
            )
            yield (sname.internal_name, sname.internal_name.replace("__", "_"))

    def get_struct_functions(self):
        return sorted(self.struct_functions.items(), key=lambda s: s[0])

    def get_function_pointers(self):
        unique_fps = set()
        unique_names = set()
        for fn in self.functions:
            if fn.name in Config.ignore_fns:
                continue

            for arg in fn.args:
                # Check if this function pointer is ignored
                if (fn.name, arg["name"]) in Config.ignore_fn_pointers:
                    continue

                # Check if argument is a function pointer
                parts = self._dissamble_function_pointer(arg["type"])
                if parts:
                    if arg["type"] in unique_fps:
                        continue
                    unique_fps.add(arg["type"])

                    return_type, pointer_part, args = parts
                    prepared_args = []
                    prepared_arg_names = []
                    prepared_arg_types = []
                    for i, a in enumerate(args):
                        arg_type = a.replace("struct", "").lstrip()
                        name = f"arg{i}"
                        prepared_args.append(f"{arg_type} {name}")
                        prepared_arg_names.append(name)
                        prepared_arg_types.append(arg_type)

                    # Decide on a unique name
                    base_name = (arg["name"], 0)
                    while base_name in unique_names:
                        base_name = (base_name[0], base_name[1] + 1)
                    unique_names.add(base_name)
                    base_name = (
                        f"{base_name[0]}_{base_name[1]}"
                        if base_name[1] > 0
                        else base_name[0]
                    )

                    yield {
                        "name": f"{base_name}_fp",
                        "return_type": (return_type + pointer_part).strip(),
                        "arg_list": ", ".join(prepared_args),
                        "arg_names": prepared_arg_names,
                        "arg_types": prepared_arg_types,
                    }

    def get_struct_ignored_functions(self, struct_name):
        if struct_name in self.struct_functions:
            struct_info = self.struct_functions[struct_name]
            ignored = set()
            for fn in struct_info["constructors"]:
                ignored.add(fn.name)
            for fn in struct_info["extra_functions"]:
                ignored.add(fn.name)
            for fn in struct_info["functions"]:
                ignored.add(fn.name)
            return sorted(ignored)
        return []

    def is_ignored_struct(self, struct_name):
        for struct in self.structs:
            if struct.name == struct_name:
                return struct.internal_name in Config.ignore_structs
        return False

    def csn(self, name, is_ctor=False):
        """Convert the struct name to the internal name."""
        public_type = re.match(r"(GF_[^\s]+)", name)
        if public_type:
            # Check if we have the struct entry
            if public_type.group(1) in self.struct_functions:
                if is_ctor:
                    return name.replace(
                        public_type.group(1),
                        self.struct_functions[public_type.group(1)]["iname"],
                    )
                return name.replace(
                    public_type.group(1),
                    f"struct {self.struct_functions[public_type.group(1)]['iname']}",
                )
        return name

    def _dissamble_function_pointer(self, type):
        match = re.match(r"^(\w+)([^\(]+)\(\*\)(\(.*\))$", type)
        if match:
            return_type = self.csn(match.group(1))
            pointer_part = match.group(2)
            args = match.group(3)[1:-1].split(",")
            prepared_args = [self.csn(arg.strip()) for arg in args]
            return return_type, pointer_part, prepared_args
        return None

    def _prepare_argument(self, type, name):
        prepared_type = f"{self.csn(type)} {name}"

        # Check if type is a function pointer
        parts = self._dissamble_function_pointer(type)
        if parts:
            return_type, pointer_part, args = parts
            prepared_type = ""
            prepared_type += return_type
            prepared_type += pointer_part
            prepared_type += f"(*{name})"

            # Split args and run through csn
            prepared_type += "("
            prepared_type += ", ".join(args)
            prepared_type += ")"

        return prepared_type

    def get_args(self, fn, lang):
        offset = 0 if fn.ctor or fn.static else 1
        public_parts = []
        if lang == "go":
            for arg in fn.args[offset:]:
                public_parts.append(self._prepare_argument(arg["type"], arg["name"]))
        else:
            for arg in fn.args[offset:]:
                if arg["type"] != "GF_Err *":
                    public_parts.append(
                        self._prepare_argument(arg["type"], arg["name"])
                    )
        public = ", ".join(public_parts)

        # Determine internal arguments
        internal_parts = []
        if offset == 1:
            internal_parts.append("$self")
        for arg in fn.args[offset:]:
            if arg["type"] == "GF_Err *":
                internal_parts.append("&swig_err")
            else:
                internal_parts.append(self.csn(arg["name"]))
        internal = ", ".join(internal_parts)

        return public, internal
