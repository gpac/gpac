#!/usr/bin/env python3

import argparse
import re
import subprocess


def cmd(command, log=False, check=True, printcmd=False):

    stdout = subprocess.PIPE if log else None

    if printcmd:
        print("Running: ", command)

    res = subprocess.run(command, text=True, shell=True, check=check, stdout=stdout, stderr=subprocess.STDOUT)

    return res.returncode, res.stdout


VER_PATTERN = re.compile(r"^(\d+)\.(\d+)(\.(\d+))?$")
ABI_PATTERN = re.compile(r"define GPAC_VERSION_MAJOR\s+(\d+).*?define GPAC_VERSION_MINOR\s+(\d+).*?define GPAC_VERSION_MICRO\s+(\d+)",  re.M | re.S)

GITLOG_PATTERN = re.compile(r"(fix|fixes|fixed|close|closes) #\d+", re.IGNORECASE)


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

        if not GITLOG_PATTERN.search(msg):
            lines.append(line)

    return "\n".join(lines)



def main(args):

    step = 0

    rc, log = cmd("git status --porcelain --untracked-files=no", log=True)
    if rc or log!="":
        print(f"You have uncommited changes:\n{log}\nStash or commit before running the script.")
        exit(1)

    rc, oldtag = cmd("git describe --tags --abbrev=0 --match 'v*'", log=True)
    if rc or oldtag=="":
        print(f"Couldn't get current tag.")
        exit(1)

    args.oldtag = oldtag.strip()
    args.nextver = f"{args.v1}.{int(args.v2)+1}-DEV"

    print(f"Current tag: {args.oldtag}")
    print(f"Next tag: {args.tag}")
    print(f"Next DEV version: {args.nextver}")


    with open(VERSIONH_PATH, "r") as f:
        versionh = f.read()

    valid, args.abi1, args.abi2, args.abi3 = parse_version(args.abi) if args.abi else get_current_abi(versionh)
    if not valid:
        parser.print_usage()
        exit(1)


    #################################################
    step+=1; print(f"\n{step}. Patching {VERSIONH_PATH}...")

    versionh = re.sub(r'(#define GPAC_VERSION)(\s+)".*?"', rf'\1\2"{args.version}"', versionh)

    versionh = re.sub(r'(#define GPAC_VERSION_MAJOR) (\s*)\d+', rf'\1\2 {args.abi1}' , versionh)
    versionh = re.sub(r'(#define GPAC_VERSION_MINOR) (\s*)\d+', rf'\1\2 {args.abi2}' , versionh)
    versionh = re.sub(r'(#define GPAC_VERSION_MICRO) (\s*)\d+', rf'\1\2 {args.abi3}' , versionh)

    with open(VERSIONH_PATH, "w") as f:
        f.write(versionh)


    #################################################
    step+=1; print(f"\n{step}. Patching {LIBGPACPY_PATH}...")

    cmd(f"sed -Ei 's/GF_ABI_MAJOR[[:space:]]*=[[:space:]]*[[:digit:]]+/GF_ABI_MAJOR={args.abi1}/' {LIBGPACPY_PATH}")
    cmd(f"sed -Ei 's/GF_ABI_MINOR[[:space:]]*=[[:space:]]*[[:digit:]]+/GF_ABI_MAJOR={args.abi2}/' {LIBGPACPY_PATH}")


    #################################################
    step+=1; print(f"\n{step}. Running change_version.sh...")

    rc, log = cmd("bash change_version.sh", log=True)
    if rc:
        print("Error running change_version.sh: \n", log)
        exit(rc)


    print("Final diff:\n")
    cmd(f"git --no-pager diff --color")


    #################################################
    if not args.no_make:
        step+=1; print(f"\n{step}. Building gpac...")

        cmd("make distclean")
        cmd("./configure")
        cmd("make -j4")


    #################################################
    if not args.no_commit:
        step+=1; print(f"\n{step}. Commiting and tagging...")

        cmd(f"git commit -am 'GPAC Release {args.version}'", printcmd=True)
        cmd(f"git tag -a {args.tag} -m 'GPAC Release {args.version}'", printcmd=True)


        #################################################
        step+=1; print(f"\n{step}. Generating git log...")
        rc, gitlog = cmd(f"git log --no-merges --oneline {args.oldtag}..{args.tag}", log=True, printcmd=True)
        if not rc and gitlog!="":
            # filter gitlog lines
            gitlog = filter_gitlog(gitlog)
            with open(f"gitlog_{args.oldtag}..{args.tag}.txt", "w") as f:
                f.write(gitlog)

        #################################################
        if not args.no_nextver:
            step+=1; print(f"\n{step}. Switching HEAD to new DEV version...")

            with open(VERSIONH_PATH, "r") as f:
                versionh = f.read()

            versionh = re.sub(r'(#define GPAC_VERSION)(\s+)".*?"', rf'\1\2"{args.nextver}"', versionh)

            with open(VERSIONH_PATH, "w") as f:
                f.write(versionh)

            rc, log = cmd("bash change_version.sh", log=True)
            if rc:
                print("Error running change_version.sh: \n", log)
                exit(rc)

            #cmd(f"git commit -am 'moving to next dev version [noCI]'")


        print(f"\nYou can now push with")
        print(f"\tgit push --atomic origin master {args.tag}")
        #print(f"\tOR git push origin master && git push --tags")


    print(f"\nRelease {args.version} ready to push and publish.")

    if not args.no_nextver:
        print(f"\nAfter the release is pushed you can commit and push the next DEV version:")
        print(f"\tgit commit -am 'moving to next dev version [noCI]' && git push")




if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument('version', help="main version number in the form M.m[.u]")
    parser.add_argument('--abi', help="ABI version in the form M.m[.u]. If absent, will be read from version.h")

    parser.add_argument('--no-make', action="store_true", help="skips build after changes")
    parser.add_argument('--no-commit', action="store_true", help="skips commit and tag")
    parser.add_argument('--no-nextver', action="store_true", help="skips switching to next DEV version")

    args = parser.parse_args()


    valid, args.v1, args.v2, args.v3 = parse_version(args.version)
    if not valid:
        parser.print_usage()
        exit(1)


    args.tag = f"v{args.v1}.{args.v2}.{args.v3}"

    main(args)

    exit(0)
