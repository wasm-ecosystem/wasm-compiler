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
from spdx_tools.spdx.model import (
    Package,
    Checksum,
    ChecksumAlgorithm,
    SpdxNone,
    Relationship,
    RelationshipType,
)
from license_expression import get_spdx_licensing
import git


class WasmCompilerSPDX(SPDXCreatorBase):
    def __init__(self, output_dir: str) -> None:
        project_root = Path(__file__).parent.parent.parent.absolute()
        super(WasmCompilerSPDX, self).__init__(project_root, output_dir)
        self.main_package_file_ids = []
        self.berkeley_package_file_ids = []

    def create_spdx_file(self):
        package_name = "wasm-compiler"
        self.add_documentation_info(package_name)

        # Get version from git
        try:
            repo = git.Repo(self.project_root)
            # Try to get version from latest tag, fallback to commit hash
            try:
                version = repo.git.describe("--tags", "--abbrev=0")
            except:
                version = repo.head.commit.hexsha[:8]
        except:
            version = "unknown"

        copyright_text = (
            "Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)"
        )
        license_id = "Apache-2.0"

        licensing = get_spdx_licensing()
        license_expr = licensing.parse(license_id)

        # Main repository information
        git_url = "https://github.com/wasm-ecosystem/wasm-compiler.git"  # Update with actual repo URL

        # Get current commit hash
        try:
            repo = git.Repo(self.project_root)
            commit_sha = repo.head.commit.hexsha
        except:
            commit_sha = "unknown"

        package = Package(
            name=package_name,
            spdx_id="SPDXRef-PACKAGE",
            download_location=git_url,
            files_analyzed=True,
            version=version,
            file_name=None,
            supplier=None,
            originator=None,
            checksums=[Checksum(ChecksumAlgorithm.SHA1, commit_sha)],
            homepage=SpdxNone(),
            source_info=f"Main wasm-compiler repository at commit {commit_sha[:8]}",
            license_concluded=license_expr,
            license_declared=license_expr,
            license_info_from_files=[license_expr],
            license_comment=None,
            copyright_text=copyright_text,
            summary="WebAssembly compiler and runtime",
            description="A high-performance WebAssembly compiler and runtime implementation",
        )

        self.set_package(package)

        # Add source files from main directories
        source_dirs = ["src"]
        for src_dir in source_dirs:
            src_path = os.path.join(self.project_root, src_dir)
            if os.path.exists(src_path):
                print(f"Adding files from {src_dir}...")
                file_ids = self.add_file_recursive(src_path, copyright_text, license_id)
                self.main_package_file_ids.extend(file_ids)

        # Add header files if they exist
        include_dir = os.path.join(self.project_root, "include")
        if os.path.exists(include_dir):
            print("Adding header files...")
            file_ids = self.add_file_recursive(include_dir, copyright_text, license_id)
            self.main_package_file_ids.extend(file_ids)

        # Add CMake files
        cmake_files = ["CMakeLists.txt"]
        for cmake_file in cmake_files:
            cmake_path = os.path.join(self.project_root, cmake_file)
            if os.path.exists(cmake_path):
                file_id = self.add_source_by_path(
                    cmake_path, copyright_text, license_id
                )
                self.main_package_file_ids.append(file_id)

        # Add Berkeley SoftFloat dependency
        self._add_berkeley_softfloat_dependency()

        # Update package with verification code and link files
        self._finalize_packages()

        success = self.genSPDX()

        if not success:
            exit(1)
        else:
            print(
                f"Successfully generated consolidated SPDX file for {package_name} and its dependencies"
            )

    def _add_berkeley_softfloat_dependency(self):
        """Add Berkeley SoftFloat as a dependency package"""
        print("Adding Berkeley SoftFloat dependency...")

        major_version = 3
        minor_version = "e"
        package_name = "berkeley-softfloat"
        submodule_relative_path = os.path.join("thirdparty", "berkeley-softfloat-3")
        berkeley_softfloat_dir = os.path.join(
            self.project_root, submodule_relative_path
        )

        # Check if the dependency exists
        if not os.path.exists(berkeley_softfloat_dir):
            print(
                f"Warning: Berkeley SoftFloat dependency not found at {berkeley_softfloat_dir}"
            )
            return

        copyright_text = """This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
                Package, Release 3e, by John R. Hauser.
                Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
                University of California.  All rights reserved."""
        license_id = "BSD-3-Clause"

        licensing = get_spdx_licensing()
        license_expr = licensing.parse(license_id)

        git_url = "https://github.com/ucb-bar/berkeley-softfloat-3.git"
        package_sha = self.get_git_hash_of_submodule(submodule_relative_path)

        try:
            with open(os.path.join(berkeley_softfloat_dir, "COPYING.txt"), "r") as f:
                copyright_text = f.read().replace("\n", "")
        except FileNotFoundError:
            print("Warning: COPYING.txt not found, using default copyright text")

        dependency_package = Package(
            name=package_name,
            spdx_id="SPDXRef-PACKAGE-BerkeleySoftFloat",
            download_location=git_url,
            files_analyzed=True,
            version=f"{major_version}.{minor_version}",
            file_name=None,
            supplier=None,
            originator=None,
            checksums=(
                [Checksum(ChecksumAlgorithm.SHA1, package_sha)] if package_sha else []
            ),
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

        # Add the dependency package to the document
        self._SPDXCreatorBase__spdx_doc.packages.append(dependency_package)

        # Add relationship: main package depends on this dependency
        dependency_relationship = Relationship(
            "SPDXRef-PACKAGE",
            RelationshipType.DEPENDS_ON,
            "SPDXRef-PACKAGE-BerkeleySoftFloat",
        )
        self._SPDXCreatorBase__spdx_doc.relationships.append(dependency_relationship)

        # Add files from Berkeley SoftFloat source directory
        source_dir = os.path.join(berkeley_softfloat_dir, "source")
        if os.path.exists(source_dir):
            print("Adding Berkeley SoftFloat source files...")
            file_ids = self.add_file_recursive(source_dir, copyright_text, license_id)
            self.berkeley_package_file_ids.extend(file_ids)
        else:
            print(
                f"Warning: Berkeley SoftFloat source directory not found at {source_dir}"
            )

    def _finalize_packages(self):
        """Finalize packages by adding verification codes and linking files"""
        print("Finalizing packages with verification codes and file relationships...")

        # Get all files from document
        all_files = self._SPDXCreatorBase__spdx_doc.files

        # Update main package (wasm-compiler)
        for package in self._SPDXCreatorBase__spdx_doc.packages:
            if package.spdx_id == "SPDXRef-PACKAGE":
                # Get files for main package
                main_files = [
                    f for f in all_files if f.spdx_id in self.main_package_file_ids
                ]
                if main_files:
                    package.verification_code = (
                        self.calculate_package_verification_code(main_files)
                    )
                    self.link_files_to_package(
                        package.spdx_id, self.main_package_file_ids
                    )
                    print(
                        f"  Main package: {len(main_files)} files, verification code: {package.verification_code.value}"
                    )

            elif package.spdx_id == "SPDXRef-PACKAGE-BerkeleySoftFloat":
                # Get files for Berkeley SoftFloat package
                berkeley_files = [
                    f for f in all_files if f.spdx_id in self.berkeley_package_file_ids
                ]
                if berkeley_files:
                    package.verification_code = (
                        self.calculate_package_verification_code(berkeley_files)
                    )
                    self.link_files_to_package(
                        package.spdx_id, self.berkeley_package_file_ids
                    )
                    print(
                        f"  Berkeley SoftFloat: {len(berkeley_files)} files, verification code: {package.verification_code.value}"
                    )
