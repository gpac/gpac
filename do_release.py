#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys
from textwrap import dedent

SED="sed"

if sys.platform == "darwin":
    SED="gsed"


def cmd(command, log=False, check=True, printcmd=False):

    stdout = subprocess.PIPE if log else None

    if printcmd:
        print("Running: ", command)

    res = subprocess.run(command, text=True, shell=True, check=check, stdout=stdout, stderr=subprocess.STDOUT)

    return res.returncode, res.stdout


VER_PATTERN = re.compile(r"^(\d+)\.(\d+)(\.(\d+))?(-[a-zA-Z0-9]+)?$")
ABI_PATTERN = re.compile(r"define GPAC_VERSION_MAJOR\s+(\d+).*?define GPAC_VERSION_MINOR\s+(\d+).*?define GPAC_VERSION_MICRO\s+(\d+)",  re.M | re.S)

GITLOG_FILTERS = [
    re.compile(r"(fix|fixes|fixed|close|closes) #\d+", re.IGNORECASE),
    re.compile(r"^fuzz: ", re.IGNORECASE),
    re.compile(r" update to gpac_monitor@", re.IGNORECASE),
]


VERSIONH_PATH = "include/gpac/version.h"
LIBGPACPY_PATH = "share/python/libgpac/libgpac.py"

def parse_version(ver):

    m = VER_PATTERN.match(ver)

    if not m: return False, 0, 0, 0

    return True, m.group(1), m.group(2), 0 if m.group(4) is None else m.group(4)


def get_current_abi(versionh):

    m = ABI_PATTERN.search(versionh)

    if not m: return False, 0, 0, 0

    return True, m.group(1), m.group(2), m.group(3)


def filter_gitlog(gitlog):

    lines = []
    for line in gitlog.split("\n"):
        parts = line.split(" ")
        #commit = parts[0]
        msg = " ".join(parts[1:])

        if not any(filter.search(msg) for filter in GITLOG_FILTERS):
            lines.append(line)

    return "\n".join(lines)



def switch_version(version, abi=None):

    with open(VERSIONH_PATH, "r") as f:
        versionh = f.read()

    valid, abi1, abi2, abi3 = parse_version(abi) if abi else get_current_abi(versionh)
    if not valid:
        print(f"Invalid abi version: {abi}")
        exit(1)

    #################################################
    print(f"\n* Patching {VERSIONH_PATH}...")

    versionh = re.sub(r'(#define GPAC_VERSION)(\s+)".*?"', rf'\1\2"{version}"', versionh)

    if abi:
        versionh = re.sub(r'(#define GPAC_VERSION_MAJOR) (\s*)\d+', rf'\1\2 {abi1}' , versionh)
        versionh = re.sub(r'(#define GPAC_VERSION_MINOR) (\s*)\d+', rf'\1\2 {abi2}' , versionh)
        versionh = re.sub(r'(#define GPAC_VERSION_MICRO) (\s*)\d+', rf'\1\2 {abi3}' , versionh)

    with open(VERSIONH_PATH, "w") as f:
        f.write(versionh)


    #################################################
    if abi:
        print(f"\n* Patching {LIBGPACPY_PATH}...")

        cmd(f"{SED} -Ei 's/GF_ABI_MAJOR[[:space:]]*=[[:space:]]*[[:digit:]]+/GF_ABI_MAJOR={abi1}/' {LIBGPACPY_PATH}")
        cmd(f"{SED} -Ei 's/GF_ABI_MINOR[[:space:]]*=[[:space:]]*[[:digit:]]+/GF_ABI_MINOR={abi2}/' {LIBGPACPY_PATH}")


    #################################################
    print(f"\n* Running change_version.sh...")

    rc, log = cmd("bash change_version.sh", log=True)
    if rc:
        print("Error running change_version.sh: \n", log)
        exit(rc)


    print("Final diff:\n")
    cmd(f"git --no-pager diff --color")

    return True, abi1, abi2, abi3


def main(args):

    rc, oldtag = cmd("git describe --tags --abbrev=0 --match 'v*'", log=True)
    if rc or oldtag=="":
        print(f"Couldn't get current tag.")
        exit(1)

    args.oldtag = oldtag.strip()
    # args.nextver = f"{args.v1}.{int(args.v2)+1:0{len(args.v2)}}-DEV"

    print(f"Current tag: {args.oldtag}")
    if args.tag:
        print(f"Next tag: {args.tag}")
    if args.nextver:
        print(f"Next DEV version: {args.nextver}")



    if args.version:

        valid, args.abi1, args.abi2, args.abi3 = switch_version(args.version, args.abi)
        if not valid:
            print(f"Switching to version {args.version} failed")
            exit(1)

        #################################################
        if args.make:
            print(f"\n* Building gpac...")

            cmd("make distclean")
            cmd("./configure")
            cmd("make -j4")


        #################################################
        if not args.no_commit:
            print(f"\n* Commiting and tagging...")

            cmd(f"git commit -am 'GPAC Release {args.version}'", printcmd=True)
            cmd(f"git tag -a {args.tag} -m 'GPAC Release {args.version}'", printcmd=True)


    #################################################
    if args.gitlog or args.version:

        print(f"\n* Generating git log...")

        HEAD = args.tag if args.tag else "HEAD"

        rc, gitlog = cmd(f"git log --no-merges --oneline {args.oldtag}..{HEAD}", log=True, printcmd=True)
        if not rc and gitlog!="":

            # filter gitlog lines
            gitlog = filter_gitlog(gitlog)


            gitlog_fn = f"gitlog_{args.oldtag}..{HEAD}.txt"

            with open(gitlog_fn, "w") as f:
                f.write(gitlog)

            print(f"Wrote git log to file: {gitlog_fn}")

    #################################################
    if args.nextver:
        print(f"\n* Switching HEAD to new DEV version {args.nextver}...")

        valid, args.abi1, args.abi2, args.abi3 = switch_version(args.nextver, args.abi)

        print(f"\n -> switched to version {args.nextver}")

        if not args.version:
            if not args.no_commit:
                cmd(f"git commit -am 'moving to next dev version [noCI]'")
                print(" -> next version ready to push")
            else:
                print(f"\t-> ready to commit and push with: git commit -am 'moving to next dev version [noCI]' && git push")



    if args.version:
        print(f"\nYou can now push with")
        print(f"\tgit push --atomic origin master {args.tag}")
        #print(f"\tOR git push origin master && git push --tags")

        print(f"\nRelease {args.version} ready to push and publish.")

        if args.nextver:
            print(f"\nAfter the release is pushed you can commit and push the next DEV version:")
            print(f"\tgit commit -am 'moving to next dev version [noCI]' && git push")




if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=dedent('''\
            '''),
        epilog=dedent('''\
            You must provide at least a version to release, or --gitlog, or --nextver <VERSION>

            Examples:
                ./do_release.py --gitlog --dirty
                ./do_release.py --abi 16.7 26.02.1
                ./do_release.py --nextver 26.02.2-DEV
            ''')
    )

    parser.add_argument('version', help="main version number in the form M.m[.u]", nargs='?', default=None)
    parser.add_argument('--abi', help="ABI version in the form M.m[.u]. If absent, will be read from version.h")

    parser.add_argument('--make', action="store_true", help="build after changes")
    parser.add_argument('--no-commit', action="store_true", help="skips commit and tag")
    # parser.add_argument('--no-nextver', action="store_true", help="skips switching to next DEV version")

    parser.add_argument('--gitlog', action="store_true", help="generate a filtered git log between the last tag and the current HEAD")
    parser.add_argument('--nextver', help="switch to the giving next DEV version")

    parser.add_argument('--dirty', action="store_true", help="skips checking of working directory has uncommited changes")

    args = parser.parse_args()


    if not args.version and not args.nextver and not args.gitlog:
        parser.print_help()
        print(f"\nProvide at least new release version, or --nextver <VER>, or --gitlog")
        exit(1)

    if args.version:
        valid, args.v1, args.v2, args.v3 = parse_version(args.version)
        if not valid:
            parser.print_help()
            print(f"\nInvalid version: {args.version}")
            exit(1)

        args.tag = f"v{args.v1}.{args.v2}.{args.v3}"
    else:
        args.tag = None

    if args.nextver:
        valid, _, _, _ = parse_version(args.nextver)
        if not valid:
            parser.print_help()
            print(f"\nInvalid nextver: {args.nextver}")
            exit(1)


    if not args.dirty:
        rc, log = cmd("git status --porcelain --untracked-files=no", log=True)
        if rc or log!="":
            print(f"You have uncommited changes:\n{log}\nStash or commit before running the script.")
            exit(1)


    main(args)

    exit(0)
