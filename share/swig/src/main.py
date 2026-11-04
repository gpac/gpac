import os
import sys
import logging
import argparse
import subprocess

# Local imports
from logger import logger
from config import Config, SWIG_DIR, ROOT_DIR
from extractor import run_extractor
from generators import DTSGenerator, SWIGGenerator

# External imports
from jinja2 import Environment, FileSystemLoader

if not (sys.version_info.major == 3 and sys.version_info.minor >= 12):
    raise RuntimeError("This script requires Python 3.12 or higher.")


def contains(value, item):
    return item in value


def main(args):
    # Get the structs and their associated functions
    logger.info("Extracting functions and structs.")
    struct_functions, info = run_extractor(args.no_cache)

    # Set up Jinja2 environment
    env = Environment(
        loader=FileSystemLoader(os.path.join(SWIG_DIR, "templates")),
        lstrip_blocks=True,
        trim_blocks=True,
    )
    env.add_extension("jinja2.ext.loopcontrols")
    env.filters["contains"] = contains

    # Generate the SWIG file
    logger.info("Generating SWIG file.")
    template = env.get_template("gpac.i.j2")
    generator = SWIGGenerator(struct_functions, info)
    output = template.render(
        lang=args.lang, config=Config.for_jinja(), **generator.get_globals()
    )

    with open(os.path.join(SWIG_DIR, "gpac.i"), "w") as f:
        f.write(output)
    logger.info("SWIG file gpac.i generated.")

    # Create the language bindings
    logger.info(f"Creating language bindings for {args.lang}.")
    if args.lang not in ["python", "go", "node"]:
        logger.error(f"Unsupported language: {args.lang}")
        exit(1)
    if args.lang == "node":
        options = ["-javascript", "-napi", "-c++"]
    elif args.lang == "go":
        options = ["-go", "-cgo", "-c++", "-intgosize", "64"]
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
        *["-I" + os.path.join(ROOT_DIR, d) for d in Config.include_dirs],
        "-outdir",
        output_dir,
        "-o",
        os.path.join(output_dir, "gpac_wrap.cpp"),
        os.path.join(SWIG_DIR, "gpac.i"),
    ]
    subprocess.run(cmd, check=True)
    logger.info(f"Language bindings for {args.lang} created in {args.lang}/gpac_wrap.c")

    # Generate .d.ts file for TypeScript
    if args.lang == "node":
        template = env.get_template("gpac.d.ts.j2")
        generator = DTSGenerator(struct_functions, info)
        output = template.render(config=Config.for_jinja(), **generator.get_globals())
        with open(os.path.join(output_dir, "index.d.ts"), "w") as f:
            f.write(output)
        logger.info("Generated TypeScript declaration file index.d.ts.")

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
