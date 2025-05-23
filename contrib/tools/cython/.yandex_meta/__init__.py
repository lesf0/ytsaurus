import os

from devtools.yamaker.fileutil import re_sub_file
from devtools.yamaker.project import NixProject
from devtools.yamaker import python


def post_install(self):
    dist_files = python.extract_dist_info(self)

    re_sub_file(
        f"{self.dstdir}/cython.py",
        r"# Change content of this file to change uids for cython programs - cython.*",
        rf"# Change content of this file to change uids for cython programs - cython {self.version} r0",
    )

    self.yamakes.clear()
    self.yamakes["."] = self.module(
        module="PY3_LIBRARY",
        NO_LINT=True,
        RESOURCE_FILES=python.prepare_resource_files(self, *dist_files),
    )

    for path, dirs, files in os.walk(self.dstdir):
        for file in files:
            if file.endswith(".c"):
                file = f"{path}/{file}"
                with open(file) as f:
                    first_line = f.readline()
                if first_line.startswith("/* Generated by Cython"):
                    os.remove(file)


cython = NixProject(
    owners=["g:python-contrib"],
    arcdir="contrib/tools/cython",
    nixattr=python.make_nixattr("cython"),
    copy_sources=["Cython/", "cygdb.py", "cython.py"],
    keep_paths=[
        "Cython/Includes/numpy.pxd",
        "Cython/Utility/CommonTypes.c",
        "Cython/ya.make",
        "generated_c_headers.h",
        "generated_cpp_headers.h",
        "a.yaml",
    ],
    post_install=post_install,
)
