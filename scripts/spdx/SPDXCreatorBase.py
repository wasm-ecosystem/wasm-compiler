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
from spdx.writers.tagvalue import write_document, InvalidDocumentError
from spdx.parsers.loggers import ErrorMessages
from spdx.document import Document
from spdx.license import License
from spdx.version import Version
from spdx.creationinfo import Person
from spdx.review import Review
from spdx.package import Package
from spdx.file import File, FileType
from spdx.checksum import Checksum, ChecksumAlgorithm
from urllib.parse import urljoin
import git
import uuid

import hashlib
import glob


class SPDXCreatorBase:
    def __init__(self, project_root: str, output_dir: str) -> None:
        self.__spdx_doc = Document()

        self.__output_path = output_dir
        self.project_root = project_root

    def add_documentation_info(self, name: str) -> None:
        self.__spdx_doc.name = name
        self.__spdx_doc.spdx_id = f"{name}#SPDXRef-DOCUMENT"
        self.__spdx_doc.version = Version(1, 2)
        self.__spdx_doc.comment = ""
        namespace_uuid = str(uuid.uuid4())
        self.__spdx_doc.namespace = (
            r"/wasm-compiler/browse/scripts/spdx/SPDXCreatorBase.py" + namespace_uuid
        )
        self.__spdx_doc.data_license = License.from_identifier("CC0-1.0")
        self.__spdx_doc.creation_info.add_creator(
            Person("Fabian Scheidl", "fabian.scheidl@bmw.de")
        )
        self.__spdx_doc.creation_info.set_created_now()
        review = Review(Person("Changqing Jing", "changqing.jing@bmw.com"))
        review.set_review_date_now()
        review.comment = ""
        self.__spdx_doc.add_review(review)

    def add_source_by_path(self, file_path: str, copy_right: str, license: str) -> None:
        with open(file_path, "rb") as f:
            h = hashlib.sha1()
            while True:
                data = f.read(h.block_size)
                if not data:
                    break
                h.update(data)
            sha_str = h.hexdigest()

            file_relative_path = os.path.relpath(file_path, self.project_root)

            source_file = File(file_relative_path)
            source_file.type = FileType.SOURCE
            source_file.spdx_id = (
                f"{self.__spdx_doc.name}/{file_relative_path}#SPDXRef-FILE"
            )
            source_file.comment = ""
            source_file.set_checksum(Checksum(ChecksumAlgorithm.SHA1, sha_str))
            source_file.conc_lics = License.from_identifier(license)
            source_file.add_lics(source_file.conc_lics)
            source_file.copyright = copy_right
            self.__spdx_doc.add_file(source_file)

    def add_file_recursive(self, root_dir: str, copy_right: str, license: str) -> None:
        files = glob.glob(root_dir + "/**/*.*", recursive=True)
        for file_path in files:
            self.add_source_by_path(file_path, copy_right, license)

    def set_package(self, package: Package) -> None:
        self.__spdx_doc.package = package

    def get_git_hash_of_submodule(self, submodule_name: str) -> str:
        repo = git.Repo(self.project_root)
        for submodule in repo.submodules:
            if submodule.name == submodule_name:
                return submodule.hexsha
        return ""

    def genSPDX(self) -> bool:
        try:
            with codecs.open(
                os.path.join(self.__output_path, f"{self.__spdx_doc.name}.spdx"),
                mode="w",
                encoding="utf-8",
            ) as out:
                write_document(self.__spdx_doc, out)
                return True
        except InvalidDocumentError as e:
            print("Document is Invalid:\n\t", end="")
            print("\n\t".join(e.args[0]))
            messages = ErrorMessages()
            self.__spdx_doc.validate(messages)
            print("\n".join(messages.messages))
            return False
