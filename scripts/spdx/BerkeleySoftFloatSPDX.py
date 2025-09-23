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
from spdx.package import Package
from spdx.license import License
from spdx.checksum import Checksum, ChecksumAlgorithm
from spdx.utils import SPDXNone


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

        package = Package()
        package.name = package_name
        package.version = f"{major_version}.{minor_version}"
        package.file_name = SPDXNone()
        package.spdx_id = f"{package_name}#SPDXRef-PACKAGE"
        git_url = "https://github.com/ucb-bar/berkeley-softfloat-3.git"
        package.download_location = git_url
        package_sha = self.get_git_hash_of_submodule(submodule_relative_path)

        package.set_checksum(Checksum(ChecksumAlgorithm.SHA1, package_sha))
        package.homepage = "http://www.jhauser.us/arithmetic/SoftFloat.html"

        package.conc_lics = License.from_identifier(license)
        package.license_declared = License.from_identifier(license)
        package.add_lics_from_file(License.from_identifier(license))
        package.source_info = f"<text>use master branch of {git_url}</text>"

        with open(os.path.join(berkeley_softfloat_dir, "COPYING.txt"), "r") as f:
            package.cr_text = f.read().replace("\n", "")
        package.summary = "Berkeley SoftFloat Release 3"
        package.description = "Berkeley SoftFloat is a software implementation of binary floating-point that conforms to the IEEE Standard for Floating-Point Arithmetic"

        self.set_package(package)

        self.add_file_recursive(
            os.path.join(berkeley_softfloat_dir, "source"), copyright, license
        )

        success = self.genSPDX()

        if not success:
            exit(1)
