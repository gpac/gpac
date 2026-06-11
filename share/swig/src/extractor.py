import os
import re
import json
import pickle
import tempfile
import subprocess
from dataclasses import dataclass, field

# Local imports
from logger import logger
from config import Config, ROOT_DIR


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
            *["-I" + os.path.join(ROOT_DIR, d) for d in Config.include_dirs],
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
            if any(name.startswith(ignore) for ignore in Config.ignore_fns):
                return functions

            return_type = node.get("type", {}).get("qualType", "void")
            return_type = re.sub(r"\s*\(.*\)$", "", return_type)
            args = []
            for arg in node.get("inner", []):
                if arg.get("kind") == "ParmVarDecl":
                    arg_type = arg.get("type", {}).get("desugaredQualType", None)
                    if not arg_type:
                        arg_type = arg.get("type", {}).get("qualType", "void")
                    args.append(
                        {
                            "name": arg.get("name", ""),
                            "type": arg_type,
                        }
                    )

            # Ensure there are at most 1 GF_Err * arguments
            gf_err_args = [arg for arg in args if arg["type"] == "GF_Err *"]
            if len(gf_err_args) > 1:
                logger.critical(
                    f"Function {name} has more than one GF_Err * argument, which is not supported."
                )
                exit(1)

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
                name = node.get("name", "")
                structs.append(Struct(internal_name=internal_name, name=name))
        for key, value in node.items():
            if isinstance(value, (dict, list)):
                search_structs(value, structs)
    elif isinstance(node, list):
        for item in node:
            search_structs(item, structs)
    return structs


@dataclass
class Enum:
    name: str
    values: dict = field(default_factory=dict)

    def __str__(self):
        """
        Generate the enum name.
        """
        return f"{self.name} ({', '.join(self.values.keys())})"


def search_enums(node, enums=[], stack=[]):
    """
    Recursively search for enum declarations in the AST.
    """
    if isinstance(node, dict):
        if node.get("kind") == "EnumDecl":
            stack.clear()
            inner = node.get("inner", [])
            for item in inner:
                if item.get("kind") == "EnumConstantDecl":
                    val_name = item.get("name", "")

                    if not val_name.startswith("GF_"):
                        continue

                    val_value = item.get("value", None)
                    # Check if we have a value for this value
                    for token in item.get("inner", []):
                        if token.get("kind") == "ConstantExpr":
                            val_value = token.get("value", "")
                            break
                    stack.append({"name": val_name, "value": val_value})

        if node.get("kind") == "TypedefDecl":
            type = node.get("type", {}).get("qualType", "")
            if type.startswith("enum "):
                name = node.get("name", "")

                if not name.startswith("GF_"):
                    return enums

                if stack:
                    # Create the enum
                    enum = Enum(name=name)
                    for item in stack:
                        enum.values[item["name"]] = item["value"]
                    enums.append(enum)
                    stack.clear()
                else:
                    logger.warning(f"Enum {name} has no values, skipping")

        for key, value in node.items():
            if isinstance(value, (dict, list)):
                search_enums(value, enums, stack)
    elif isinstance(node, list):
        for item in node:
            search_enums(item, enums, stack)
    return enums


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

            # Copy the function declaration
            fn_decl = fn.copy()

            # Check if the function is a cross-struct constructor
            is_special_case = False
            for special_fn, special_info in Config.cross_struct_ctors.items():
                if re.match(special_fn, fn.name):
                    fn_decl.alias = re.sub(special_info["remove"], "", fn.name)
                    fn_decl.alias = fn_decl.alias.lstrip("_")

                    # Find the parent struct
                    for struct in structs:
                        if struct.internal_name == special_info["parent"]:
                            logger.debug(
                                f"{fn.name} is a cross-struct constructor from {struct.name}"
                            )
                            struct_functions[struct.name]["functions"].append(fn_decl)
                            assert fn.args[0]["type"] == f"{struct.name} *", (
                                f"Function {fn.name}'s first argument ({fn.args[0]['type']}) does not match the parent struct {struct.name} ({struct.internal_name})"
                            )
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
            alias = Config.struct_fn_alias.get(struct_info["iname"], None)
            if fn.name.startswith(prefix):
                # Remove the prefix from the function name
                fn_decl.alias = fn.name[len(prefix) :]
            # Check if the function starts with the alias
            elif alias and re.match(alias, fn.name):
                # Remove the alias from the function name
                fn_decl.alias = re.sub(alias, "", fn.name)
                assert fn.args[0]["type"] == f"{struct_name} *", (
                    f"Function {fn.name}'s first argument ({fn.args[0]['type']}) does not match the parent struct {struct_name} ({struct_info['iname']})"
                )
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
    for struct_name, merge_name in Config.struct_merges.items():
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
        prefix = Config.struct_fn_alias.get(
            struct_info["iname"], rf"^{struct_info['iname'].replace('__', '')}"
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
                        assert len(fn_decl.args) == 1, (
                            f"Destructor {fn.name} should have one argument"
                        )
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
                        assert fn_decl.return_type == f"{struct_name} *", (
                            f"Function {fn.name} should return {struct_name} *"
                        )
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
        prefix = Config.struct_fn_alias.get(
            struct_info["iname"], rf"^{struct_info['iname'].replace('__', '')}"
        )
        for fn in functions:
            if (
                any(fn.name == sfn.name for sfn in struct_info["functions"])
                or any(fn.name == sfn.name for sfn in struct_info["constructors"])
                or (
                    struct_info["destructor"]
                    and fn.name == struct_info["destructor"].name
                )
                or fn.name in Config.ignore_fns
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


def run_extractor(no_cache=False):
    PICKLE_FILE = os.path.join(tempfile.gettempdir(), "swig_generator_objects.pickle")
    if os.path.exists(PICKLE_FILE) and not no_cache:
        logger.info("Loading cached functions and structs from pickle file.")
        with open(PICKLE_FILE, "rb") as f:
            functions, structs, enums = pickle.load(f)
    else:
        # Preprocess the headers
        ast = preprocess_headers(Config.headers)

        # Search for functions and structs
        logger.info("Searching for functions and structs in the AST.")
        functions = search_functions(ast)
        structs = search_structs(ast)
        enums = search_enums(ast)

        # Save the functions and structs to a pickle file
        with open(PICKLE_FILE, "wb") as f:
            pickle.dump((functions, structs, enums), f)

    # Prepare the structs
    logger.info("Preparing structs.")
    return prepare_structs(structs, functions), (structs, functions, enums)
