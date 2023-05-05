# This script must only use Python Standard Library

import json as _json
from collections import OrderedDict as _OrderedDict
from dataclasses import dataclass as _dataclass
from pathlib import Path as _Path
from typing import Iterator as _Iterator


@_dataclass(kw_only=True)
class BaseDependency:
    name: str
    kind: str
    url: str
    path: _Path
    patch: _Path | None


@_dataclass(kw_only=True)
class GitDependency(BaseDependency):
    version: str
    full_clone: bool


@_dataclass(kw_only=True)
class ArchiveDependency(BaseDependency):
    strip_root: bool


class Manifest:
    def __init__(self, manifest_path: _Path) -> None:
        with open(manifest_path, "r", encoding="UTF-8") as file:
            manifest = _json.loads(file.read())

        self._dict: _OrderedDict[str, BaseDependency] = _OrderedDict()
        for dep in manifest:
            name = dep["name"].strip()
            kind = dep["kind"].strip()
            url = dep["url"].strip()
            path = (manifest_path.parent / dep["path"]).resolve()
            patch = dep.get("patch", None)
            if patch is not None:
                patch = manifest_path.parent / patch
            dependency: BaseDependency
            if kind == "git":
                version = dep["version"].strip()
                full_clone = dep.get("full_clone", False)
                dependency = GitDependency(
                    name=name, kind=kind, url=url, path=path, patch=patch, version=version, full_clone=full_clone
                )
            elif kind == "archive":
                strip_root = dep.get("strip_root", False)
                dependency = ArchiveDependency(
                    name=name, kind=kind, url=url, path=path, patch=patch, strip_root=strip_root
                )
            else:
                assert False, f"Unknown dependency kind: {kind}"

            self._dict[name] = dependency

    def __iter__(self) -> _Iterator[BaseDependency]:
        for _, dep in self._dict.items():
            yield dep

    def __getitem__(self, key: str) -> BaseDependency:
        return self._dict[key]
