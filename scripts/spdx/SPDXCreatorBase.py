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

import os
import codecs
from datetime import datetime
from urllib.parse import urljoin
import git
import uuid
import hashlib
import glob
import re

from spdx_tools.spdx.writer.tagvalue.tagvalue_writer import write_document_to_stream
from spdx_tools.spdx.model import (
    Document,
    CreationInfo,
    Actor,
    ActorType,
    File,
    FileType,
    Package,
    PackageVerificationCode,
    Checksum,
    ChecksumAlgorithm,
    Version,
    Relationship,
    RelationshipType,
)


class SPDXCreatorBase:
    def __init__(self, project_root: str, output_dir: str) -> None:
        self.__spdx_doc = None
        self.__output_path = output_dir
        self.project_root = project_root
        self._licensing = None  # Cache licensing object

    def _read_codeowners(self) -> list:
        """Read CODEOWNERS file and extract email addresses"""
        creators = []
        codeowners_path = os.path.join(self.project_root, ".github", "CODEOWNERS")

        if os.path.exists(codeowners_path):
            with open(codeowners_path, "r") as f:
                content = f.read()
                # Extract email addresses from CODEOWNERS
                email_pattern = r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}"
                emails = re.findall(email_pattern, content)

                # Create creators from emails
                for email in emails:
                    # Extract name from email (simple heuristic)
                    name_part = email.split("@")[0]
                    # Convert dots to spaces and capitalize
                    name = name_part.replace(".", " ").title()
                    creators.append(Actor(ActorType.PERSON, name, email))

        # Fallback to default creators if no CODEOWNERS found
        if not creators:
            creators = [
                Actor(ActorType.PERSON, "Fabian Scheidl", "fabian.scheidl@bmw.de"),
                Actor(ActorType.PERSON, "Changqing Jing", "changqing.jing@bmw.com"),
            ]

        return creators

    def add_documentation_info(self, name: str) -> None:
        namespace_uuid = str(uuid.uuid4())
        document_namespace = (
            f"https://github.com/wasm-ecosystem/wasm-compiler/{namespace_uuid}"
        )

        creators = self._read_codeowners()

        creation_info = CreationInfo(
            spdx_version="SPDX-2.3",
            spdx_id="SPDXRef-DOCUMENT",
            name=name,
            document_namespace=document_namespace,
            creators=creators,
            created=datetime.now(),
            data_license="CC0-1.0",
        )

        self.__spdx_doc = Document(creation_info=creation_info)

    def add_source_by_path(self, file_path: str, copy_right: str, license: str) -> str:
        with open(file_path, "rb") as f:
            h = hashlib.sha1()
            while True:
                data = f.read(h.block_size)
                if not data:
                    break
                h.update(data)
            sha_str = h.hexdigest()

            file_relative_path = os.path.relpath(file_path, self.project_root)

            from license_expression import get_spdx_licensing

            licensing = get_spdx_licensing()
            license_expr = licensing.parse(license)

            spdx_id = f"SPDXRef-FILE-{len(self.__spdx_doc.files)}"
            source_file = File(
                name=file_relative_path,
                spdx_id=spdx_id,
                checksums=[Checksum(ChecksumAlgorithm.SHA1, sha_str)],
                file_types=[FileType.SOURCE],
                license_concluded=license_expr,
                license_info_in_file=[license_expr],
                copyright_text=copy_right,
            )

            self.__spdx_doc.files.append(source_file)
            return spdx_id

    def add_file_recursive(self, root_dir: str, copy_right: str, license: str) -> list:
        files = glob.glob(root_dir + "/**/*.*", recursive=True)
        file_ids = []
        for file_path in files:
            file_id = self.add_source_by_path(file_path, copy_right, license)
            file_ids.append(file_id)
        return file_ids

    def set_package(self, package: Package) -> None:
        self.__spdx_doc.packages.append(package)
        # Add required relationship between document and package
        relationship = Relationship(
            "SPDXRef-DOCUMENT", RelationshipType.DESCRIBES, package.spdx_id
        )
        self.__spdx_doc.relationships.append(relationship)

    def get_git_hash_of_submodule(self, submodule_name: str) -> str:
        repo = git.Repo(self.project_root)
        for submodule in repo.submodules:
            if submodule.name == submodule_name:
                return submodule.hexsha
        return ""

    def calculate_package_verification_code(
        self, files: list
    ) -> PackageVerificationCode:
        """Calculate verification code from file checksums"""
        checksums = []
        for file in files:
            if file.checksums:
                checksums.append(file.checksums[0].value)

        checksums.sort()
        verification_string = "".join(checksums)
        verification_hash = hashlib.sha1(verification_string.encode()).hexdigest()
        return PackageVerificationCode(value=verification_hash, excluded_files=[])

    def link_files_to_package(self, package_spdx_id: str, file_spdx_ids: list):
        """Link files to a package"""
        for file_spdx_id in file_spdx_ids:
            relationship = Relationship(
                package_spdx_id, RelationshipType.CONTAINS, file_spdx_id
            )
            self.__spdx_doc.relationships.append(relationship)

    def genSPDX(self) -> bool:
        try:
            output_file = os.path.join(
                self.__output_path, f"{self.__spdx_doc.creation_info.name}.spdx"
            )
            with codecs.open(output_file, mode="w", encoding="utf-8") as out:
                write_document_to_stream(self.__spdx_doc, out)
                return True
        except Exception as e:
            print(f"Error generating SPDX document: {e}")
            return False
