import json as _json
import shutil as _shutil
import subprocess as _subprocess
from pathlib import Path as _Path


# pylint: disable-next=unused-argument
def deploy(graph, output_folder: str, **kwargs):  # type: ignore
    print("Behold! I am Conan the Deployer")

    openocd_dep = graph.root.conanfile.dependencies["openocd"]

    if not str(openocd_dep.ref).startswith("openocd"):
        raise RuntimeError(
            f'ref "{openocd_dep.ref}" does not start with openocd'
        )
    print(f"Package Folder: {openocd_dep.package_folder}")

    if any(_Path(output_folder).iterdir()) != 0:
        raise RuntimeError(f'output folder "{output_folder}" is not empty')

    build_info = [
        str(openocd_dep.settings.arch),
        str(openocd_dep.options.build_type),
        str(openocd_dep.settings.os.value),
    ]

    if openocd_dep.settings.os.value == "Windows":
        build_info.append(str(openocd_dep.settings.os.subsystem.value))
    else:
        build_info.append(str(openocd_dep.settings.os.distro.value))
        build_info.append(str(openocd_dep.settings.os.distro.version.value))
    build_config_info = "-".join(build_info).lower()

    # copy binaries from local conan cache to the output_folder
    openocd_copy_folder = _Path(output_folder) / "openocd"
    _shutil.copytree(openocd_dep.package_folder, openocd_copy_folder)
    # TODO: figure out if there is more scalable way to get rid of these files
    (openocd_copy_folder / "conanmanifest.txt").unlink()
    (openocd_copy_folder / "conaninfo.txt").unlink()

    # save build information
    build_id = {}
    build_id["ref"] = str(openocd_dep.ref)
    build_id["build_info"] = build_config_info
    with open(
        openocd_copy_folder / "share" / "internal_build_info.json",
        "w",
        encoding="utf-8",
    ) as bi_file:
        bi_file.write(_json.dumps(build_id, ensure_ascii=False, indent=4))

    if openocd_dep.settings.os.value == "Windows":
        archive_format = "zip"
    else:
        archive_format = "xztar"
    out_archive = _Path(output_folder) / f"bundle-openocd-{build_config_info}"
    _shutil.make_archive(
        base_name=str(out_archive),
        format=archive_format,
        root_dir=output_folder,
        base_dir="openocd",
    )
