from typing import Literal

from .base import (
    bundle_node_modules,
    constants,
    extract_node_modules,
    PackageJson,
    utils,
    PackageManagerCommandError,
    BasePackageManager,
    BaseLockfile,
)
from .base.package_json import PackageJsonWorkspaceError
from .pnpm import PnpmPackageManager
from .npm import NpmPackageManager


type PackageManagerType = Literal["pnpm", "npm"]

manager = PnpmPackageManager


def get_package_manager_type(key: PackageManagerType) -> type[BasePackageManager]:
    if key == "pnpm":
        return PnpmPackageManager
    if key == "npm":
        return NpmPackageManager
    raise ValueError(f"Invalid package manager key: {key}")


__all__ = [
    "BaseLockfile",
    "BasePackageManager",
    "PnpmPackageManager",
    "NpmPackageManager",
    "PackageJson",
    "PackageJsonWorkspaceError",
    "PackageManagerCommandError",
    "PackageManagerType",
    "bundle_node_modules",
    "constants",
    "extract_node_modules",
    "get_package_manager_type",
    "manager",
    "utils",
]
