# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: Apache-2.0
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from SPDXCreatorBase import SPDXCreatorBase
from pathlib import Path
import os
from spdx_tools.spdx.model import Package, Checksum, ChecksumAlgorithm, SpdxNone
from license_expression import get_spdx_licensing


class BerkeleySoftFloatSPDX(SPDXCreatorBase):
    def __init__(self, output_dir: str) -> None:
        project_root = Path(__file__).parent.parent.parent.absolute()
        super(BerkeleySoftFloatSPDX, self).__init__(project_root, output_dir)

    def create_spdx_file(self):
        major_version = 3
        minor_version = "e"
        package_name = "berkeley-softfloat"
        submodule_relative_path = os.path.join("thirdparty", "berkeley-softfloat-3")
        berkeley_softfloat_dir = os.path.join(
            self.project_root, submodule_relative_path
        )
        self.add_documentation_info(package_name)
        copyright = """This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
                Package, Release 3e, by John R. Hauser.
                Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
                University of California.  All rights reserved."""
        license = "BSD-3-Clause"

        licensing = get_spdx_licensing()
        license_expr = licensing.parse(license)

        git_url = "https://github.com/ucb-bar/berkeley-softfloat-3.git"
        package_sha = self.get_git_hash_of_submodule(submodule_relative_path)

        with open(os.path.join(berkeley_softfloat_dir, "COPYING.txt"), "r") as f:
            copyright_text = f.read().replace("\n", "")

        package = Package(
            name=package_name,
            spdx_id="SPDXRef-PACKAGE",
            download_location=git_url,
            files_analyzed=True,
            version=f"{major_version}.{minor_version}",
            file_name=None,
            supplier=None,
            originator=None,
            checksums=[Checksum(ChecksumAlgorithm.SHA1, package_sha)],
            homepage=SpdxNone(),
            source_info=f"use master branch of {git_url}",
            license_concluded=license_expr,
            license_declared=license_expr,
            license_info_from_files=[license_expr],
            license_comment=None,
            copyright_text=copyright_text,
            summary="Berkeley SoftFloat Release 3",
            description="Berkeley SoftFloat is a software implementation of binary floating-point that conforms to the IEEE Standard for Floating-Point Arithmetic",
        )

        self.set_package(package)

        self.add_file_recursive(
            os.path.join(berkeley_softfloat_dir, "source"), copyright, license
        )

        success = self.genSPDX()

        if not success:
            exit(1)
