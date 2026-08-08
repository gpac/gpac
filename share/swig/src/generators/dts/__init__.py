from config import Config


class DTSGenerator:
    def __init__(self, struct_functions, info):
        self.struct_functions = struct_functions
        self.structs, self.functions, self.enums = info

    def get_globals(self):
        return {
            "struct_functions": self.struct_functions,
            "const_declarations": self.get_const_declarations,
            "non_class_functions": self.get_non_class_functions,
            "to_js": self.to_js,
            "to_js_args": self.to_js_args,
            "to_js_fn_args": self.to_js_fn_args,
        }

    def get_const_declarations(self):
        for enum in sorted(self.enums, key=lambda e: e.name):
            for name in enum.values.keys():
                yield name

    def get_non_class_functions(self):
        fns = []
        ignored = set()
        struct_fn_names = set()
        for struct_name, struct_info in self.struct_functions.items():
            if struct_name in Config.ignore_structs:
                for fn in struct_info["functions"]:
                    ignored.add(fn.name)
                continue
            for fn in struct_info["functions"]:
                struct_fn_names.add(fn.name)

        added = set()
        for fn in self.functions:
            if (
                fn.name not in struct_fn_names
                and fn.alias not in added
                and fn.name not in ignored
                and fn.name not in Config.ignore_fns
            ):
                fns.append(fn)
                added.add(fn.alias)

        return fns

    def to_js(self, c_type):
        c_type = c_type.replace("*", "")
        c_type = c_type.replace("const", "")
        c_type = c_type.strip()

        if "()" in c_type:
            # Function pointer
            return "VoidPointer"
        if c_type == "time_t":
            return "UnexportedStruct"
        if c_type in ["char", "unsigned char", "signed char"]:
            return "string"
        if c_type in [
            "int",
            "short",
            "size_t",
            "unsigned int",
            "unsigned short",
            "unsigned long",
            "float",
            "double",
            "long",
            "u8",
            "u16",
            "u32",
            "u64",
            "s8",
            "s16",
            "s32",
            "s64",
        ]:
            return "number"
        if c_type.startswith("gf_"):
            return "VoidPointer"
        if c_type in Config.struct_merges:
            return Config.struct_merges[c_type]
        if c_type.startswith("gfio_"):
            return "VoidPointer"

        # Check if the c_type is an enum
        for enum in self.enums:
            if c_type == enum.name:
                return "number"  # Getting values of enum constants and mapping them to each language is difficult if not impossible

        # Check if the c_type is a struct
        for struct_name, _ in self.struct_functions.items():
            if c_type == struct_name:
                return c_type

        if c_type.startswith("GF_"):
            # This is most likely an enum that we weren't able to map
            return "number"

        # If it starts with uppercase, we don't know what it is
        if c_type[0].isupper():
            return "VoidPointer"

        if "struct " in c_type:
            # we don't know what this is
            return "UnexportedStruct"

        return c_type

    def to_js_args(self, args):
        for p in args:
            yield f"{p['name']}: {self.to_js(p['type'])}"

    def to_js_fn_args(self, args, struct_name):
        for p in args:
            # If the argument is a pointer to the struct, we skip it
            if p["type"] == f"{struct_name} *" or p["type"] == "GF_Err *":
                continue
            yield f"{p['name']}: {self.to_js(p['type'])}"
