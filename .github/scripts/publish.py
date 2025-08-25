#!/usr/bin/env python3
import os
import re
import sys
import glob
import shutil
import logging
import tempfile
import datetime
import subprocess

logging.basicConfig(level=logging.INFO)

# If you remove any targets from .github/workflows/dist-linux.yml,
# keep them here until it is needed to remove them from the apt repository entirely.
DEB_TARGETS = [
    ("ubuntu", "jammy"),
    ("ubuntu", "noble"),
    ("debian", "bullseye"),
    ("debian", "bookworm"),
]
DEB_COMPONENTS = ["main", "nightly"]

# If you remove any targets from .github/workflows/dist-wasm.yml,
# keep them here until it is needed to remove them from the apt repository entirely.
WASM_TARGETS = [("full", "threaded"), ("full", "single"), ("lite", "single")]
WASM_COMPONENTS = ["nightly"]

BASE_URL = "http://localhost:8080"
LINUX_BASE_URL = f"{BASE_URL}/linux"
WASM_BASE_URL = f"{BASE_URL}/wasm"


def run_command(command, cwd=None):
    """Run a shell command and log its output."""
    logging.info(f"Running command: {' '.join(command)}")
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode != 0:
        logging.error(f"Command failed with return code {result.returncode}")
        logging.error(result.stderr)
        sys.exit(result.returncode)
    logging.info(result.stdout)


def check_url(url):
    """Check if a URL is reachable."""
    try:
        result = subprocess.run(
            ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}", url],
            text=True,
            capture_output=True,
        )
        status_code = int(result.stdout.strip())
        return status_code == 200
    except Exception as e:
        logging.error(f"Error checking URL {url}: {e}")
        return False


def mirror_deb():
    for distro, codename in DEB_TARGETS:
        for component in DEB_COMPONENTS:
            slug = f"{distro}-{codename}-{component}"
            logging.info(f"Processing {slug} component.")

            # Create the repository
            run_command(
                [
                    "aptly",
                    "repo",
                    "create",
                    *["-distribution", codename],
                    *["-component", component],
                    f"repo-{slug}",
                ]
            )

            # Skip mirror if SKIP_MIRROR is set
            if os.getenv("SKIP_MIRROR", "false").lower() == "true":
                logging.info(
                    f"Skipping mirror for {slug} component as SKIP_MIRROR is set to true."
                )
                continue

            # Check if there is a valid apt repository for the target
            repo_url = f"{LINUX_BASE_URL}/{distro}/dists/{codename}/Release"
            response = check_url(repo_url)
            if not response:
                logging.warning(
                    f"Repository {repo_url} is not reachable. Skipping {distro} {codename}."
                )
                continue
            logging.info(f"Creating mirror for {slug} component.")

            # Mirror the apt repository
            run_command(
                [
                    "aptly",
                    "mirror",
                    "create",
                    "-keyring=~/.gnupg/pubring.kbx",
                    f"mirror-{slug}",
                    f"{LINUX_BASE_URL}/{distro}",
                    *[codename, component],
                ]
            )

            # Update the mirror
            run_command(
                [
                    "aptly",
                    "mirror",
                    "update",
                    "-keyring=~/.gnupg/pubring.kbx",
                    f"mirror-{slug}",
                ]
            )

            # Import the mirror into the repository
            run_command(
                [
                    "aptly",
                    "repo",
                    "import",
                    f"mirror-{slug}",
                    f"repo-{slug}",
                    "Name",  # Wildcard for all packages
                ]
            )


def mirror_wasm():
    temp_dir = os.path.join(tempfile.gettempdir(), "wasm-mirror")
    os.makedirs(temp_dir, exist_ok=True)

    logging.info(f"Mirroring {WASM_BASE_URL} to {temp_dir}")
    run_command(["cp", "-r", "ftp/wasm/.", temp_dir])


def publish_deb():
    for distro, codename in DEB_TARGETS:
        for component in DEB_COMPONENTS:
            slug = f"{distro}-{codename}-{component}"

            # Add new artifacts
            search_pattern = f"artifacts/linux-{component}-*-{distro}-{codename}*/*.deb"
            artifacts = glob.glob(search_pattern)
            for artifact in artifacts:
                logging.info(f"Adding artifact {artifact} to {slug} repository.")

                # Rename the artifact so that there is no conflict
                assert artifact.endswith(".deb"), "Artifact must be a .deb file"
                with tempfile.TemporaryDirectory() as temp_dir:
                    # Get the folder name from the artifact path
                    folder_name = os.path.basename(os.path.dirname(artifact))

                    # Extract the required parts from the folder name
                    groups = re.match(r"linux-\w+-([^-]+)-\w+-\w+@(\w+)", folder_name)
                    if not groups:
                        logging.error(f"Invalid folder name format: {folder_name}")
                        continue
                    tag = groups.group(1)
                    arch = groups.group(2)
                    date = datetime.datetime.now().strftime("%Y%m%d%H%M%S")

                    # Get the version from the artifact name
                    file_name = os.path.basename(artifact)
                    identifier = file_name.split("-")[0]
                    version_match = file_name.split("_")[1]

                    # Create a new name for the artifact
                    new_name = (
                        f"{identifier}_{version_match}_{tag}_{arch}_{slug}_{date}.deb"
                    )

                    # Move the artifact to the temporary directory with the new name
                    new_path = os.path.join(temp_dir, new_name)
                    os.rename(artifact, new_path)
                    logging.info(f"Renamed artifact to {new_name}.")

                    # Add the new artifact to the repository
                    run_command(
                        [
                            "aptly",
                            "repo",
                            "add",
                            f"repo-{slug}",
                            new_path,
                        ]
                    )

    for distro, codename in DEB_TARGETS:
        repos = [
            f"repo-{distro}-{codename}-{component}" for component in DEB_COMPONENTS
        ]

        # Publish the repository
        logging.info(f"Publishing {slug} component.")
        run_command(
            [
                "aptly",
                "publish",
                "repo",
                *["-distribution", codename],
                *["-component", ",".join(DEB_COMPONENTS)],
                *repos,
                distro,
            ]
        )


def publish_wasm():
    temp_dir = os.path.join(tempfile.gettempdir(), "wasm-mirror")
    if not os.path.exists(temp_dir):
        logging.error(f"Temporary directory {temp_dir} does not exist.")
        return

    for variant, threaded in WASM_TARGETS:
        for component in WASM_COMPONENTS:
            search_pattern = f"wasm-{component}-*@{variant}-{threaded}/*"
            artifacts = glob.glob(f"artifacts/{search_pattern}")
            for artifact in artifacts:
                slug = os.path.basename(os.path.dirname(artifact))
                groups = re.match(r"wasm-\w+-([^@]+)", slug)
                tag = groups.group(1) if groups else "unknown"

                if component == "nightly":
                    # For nightly, we use the current date as the tag
                    tag = datetime.datetime.now().strftime("%Y%m%d")

                logging.info(
                    f"Processing artifact {artifact} for component {component}, "
                    f"variant {variant}, threaded {threaded}, tag {tag}."
                )

                # Special handling for nightly releases
                if component == "nightly":
                    shutil.copy(
                        artifact,
                        os.path.join(temp_dir, os.path.basename(artifact)),
                    )

                # Now place everything in the right place
                target = os.path.join(
                    temp_dir,
                    component,
                    tag,
                    os.path.basename(artifact),
                )
                os.makedirs(os.path.dirname(target), exist_ok=True)
                shutil.copy(artifact, target)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: publish.py <command>")
        sys.exit(1)
    command = sys.argv[1]

    if command == "mirror":
        logging.info("Mirroring deb packages...")
        mirror_deb()
        logging.info("Mirroring wasm packages...")
        mirror_wasm()
    elif command == "publish":
        logging.info("Publishing deb packages...")
        publish_deb()
        logging.info("Publishing wasm packages...")
        publish_wasm()
