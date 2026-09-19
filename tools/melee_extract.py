#!/usr/bin/env python3
"""Inspect or extract a user-provided GALE01 GameCube disc image."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath
from typing import BinaryIO, Callable, Iterator


GAMECUBE_MAGIC = 0xC2339F3D
SUPPORTED_GAME_ID = "GALE01"
SUPPORTED_DOL_SHA1 = "08e0bf20134dfcb260699671004527b2d6bb1a45"
DISC_HEADER_SIZE = 0x430
COPY_CHUNK_SIZE = 1024 * 1024
DVD_INDEX_MAGIC = b"MDVDIDX1"
DVD_INDEX_VERSION = 1


class DiscError(RuntimeError):
    pass


@dataclass(frozen=True)
class DiscEntry:
    entry_number: int
    path: str
    is_directory: bool
    offset: int
    size: int


@dataclass(frozen=True)
class DiscMetadata:
    game_id: str
    magic: str
    image_size: int
    dol_offset: int
    dol_size: int
    dol_sha1: str
    fst_offset: int
    fst_size: int
    entry_count: int
    supported: bool


def _read_exact(stream: BinaryIO, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0:
        raise DiscError("negative disc offset or size")
    stream.seek(offset)
    data = stream.read(size)
    if len(data) != size:
        raise DiscError(f"disc image is truncated at 0x{offset:X}")
    return data


def _be32(data: bytes, offset: int) -> int:
    try:
        return struct.unpack_from(">I", data, offset)[0]
    except struct.error as error:
        raise DiscError("truncated big-endian 32-bit field") from error


def _dol_file_size(header: bytes) -> int:
    if len(header) < 0xE4:
        raise DiscError("DOL header is truncated")

    offsets = [_be32(header, index * 4) for index in range(18)]
    sizes = [_be32(header, 0x90 + index * 4) for index in range(18)]
    section_ends = [
        offset + size
        for offset, size in zip(offsets, sizes, strict=True)
        if offset != 0 and size != 0
    ]
    if not section_ends:
        raise DiscError("DOL contains no file-backed sections")
    size = max(section_ends)
    if size < 0x100:
        raise DiscError("DOL file size is implausibly small")
    return size


def _safe_component(name: str) -> str:
    if not name or name in {".", ".."}:
        raise DiscError("FST contains an empty or relative path component")
    if "/" in name or "\\" in name or "\x00" in name:
        raise DiscError(f"FST contains an unsafe path component: {name!r}")
    return name


class GameCubeDisc:
    def __init__(self, image_path: Path):
        self.image_path = image_path.resolve()
        try:
            self.image_size = self.image_path.stat().st_size
        except OSError as error:
            raise DiscError(f"cannot stat disc image: {error}") from error
        if not self.image_path.is_file():
            raise DiscError("disc image path is not a regular file")

        with self.image_path.open("rb") as stream:
            header = _read_exact(stream, 0, DISC_HEADER_SIZE)
            try:
                self.game_id = header[0:6].decode("ascii")
            except UnicodeDecodeError as error:
                raise DiscError("disc game ID is not ASCII") from error
            self.magic = _be32(header, 0x1C)
            if self.magic != GAMECUBE_MAGIC:
                raise DiscError("input is not a GameCube disc image")

            self.dol_offset = _be32(header, 0x420)
            self.fst_offset = _be32(header, 0x424)
            self.fst_size = _be32(header, 0x428)
            self._validate_range(self.fst_offset, self.fst_size, "FST")

            dol_header = _read_exact(stream, self.dol_offset, 0x100)
            self.dol_size = _dol_file_size(dol_header)
            self._validate_range(self.dol_offset, self.dol_size, "main DOL")
            self.dol_sha1 = self._hash_range(stream, self.dol_offset, self.dol_size)
            fst = _read_exact(stream, self.fst_offset, self.fst_size)

        self.entries = self._parse_fst(fst)
        self.supported = (
            self.game_id == SUPPORTED_GAME_ID
            and self.dol_sha1 == SUPPORTED_DOL_SHA1
        )

    def _validate_range(self, offset: int, size: int, description: str) -> None:
        if offset < 0 or size < 0 or offset > self.image_size:
            raise DiscError(f"{description} has an invalid range")
        if size > self.image_size - offset:
            raise DiscError(f"{description} exceeds the disc image")

    @staticmethod
    def _hash_range(stream: BinaryIO, offset: int, size: int) -> str:
        digest = hashlib.sha1()
        stream.seek(offset)
        remaining = size
        while remaining:
            chunk = stream.read(min(remaining, COPY_CHUNK_SIZE))
            if not chunk:
                raise DiscError("disc image ended while hashing a range")
            digest.update(chunk)
            remaining -= len(chunk)
        return digest.hexdigest()

    def _parse_fst(self, fst: bytes) -> tuple[DiscEntry, ...]:
        if len(fst) < 12:
            raise DiscError("FST is smaller than its root entry")

        root_word = _be32(fst, 0)
        if (root_word >> 24) != 1:
            raise DiscError("FST root is not a directory")
        entry_count = _be32(fst, 8)
        if entry_count == 0 or entry_count > len(fst) // 12:
            raise DiscError("FST entry count is invalid")

        names_offset = entry_count * 12
        names = fst[names_offset:]

        def read_name(offset: int) -> str:
            if offset >= len(names):
                raise DiscError("FST name offset exceeds the string table")
            end = names.find(b"\0", offset)
            if end == -1:
                raise DiscError("FST name is not null-terminated")
            try:
                decoded = names[offset:end].decode("shift_jis")
            except UnicodeDecodeError as error:
                raise DiscError("FST name is not valid Shift-JIS") from error
            return _safe_component(decoded)

        entries: list[DiscEntry] = []
        directory_stack: list[tuple[PurePosixPath, int]] = [
            (PurePosixPath(), entry_count)
        ]

        for index in range(1, entry_count):
            while directory_stack and index >= directory_stack[-1][1]:
                directory_stack.pop()
            if not directory_stack:
                raise DiscError("FST directory ranges are inconsistent")

            entry_offset = index * 12
            word0, word1, word2 = struct.unpack_from(">III", fst, entry_offset)
            is_directory = (word0 >> 24) != 0
            name = read_name(word0 & 0x00FFFFFF)
            path = directory_stack[-1][0] / name

            if is_directory:
                if word2 <= index or word2 > entry_count:
                    raise DiscError("FST directory has an invalid end index")
                entries.append(DiscEntry(index, path.as_posix(), True, 0, 0))
                directory_stack.append((path, word2))
            else:
                self._validate_range(word1, word2, f"FST file {path}")
                entries.append(
                    DiscEntry(index, path.as_posix(), False, word1, word2)
                )

        return tuple(entries)

    def metadata(self) -> DiscMetadata:
        return DiscMetadata(
            game_id=self.game_id,
            magic=f"0x{self.magic:08X}",
            image_size=self.image_size,
            dol_offset=self.dol_offset,
            dol_size=self.dol_size,
            dol_sha1=self.dol_sha1,
            fst_offset=self.fst_offset,
            fst_size=self.fst_size,
            entry_count=len(self.entries) + 1,
            supported=self.supported,
        )

    def _output_path(self, destination: Path, relative: str) -> Path:
        root = destination.resolve()
        output = (root / Path(*PurePosixPath(relative).parts)).resolve()
        try:
            output.relative_to(root)
        except ValueError as error:
            raise DiscError(f"extraction path escapes destination: {relative}") from error
        return output

    def extract(
        self,
        destination: Path,
        *,
        force: bool = False,
        progress: Callable[[int, int], None] | None = None,
    ) -> dict[str, object]:
        if not self.supported:
            raise DiscError(
                "unsupported disc: expected GALE01 with main.dol SHA-1 "
                f"{SUPPORTED_DOL_SHA1}, got {self.game_id} {self.dol_sha1}"
            )

        destination = destination.resolve()
        file_entries = [entry for entry in self.entries if not entry.is_directory]
        outputs = [self._output_path(destination, entry.path) for entry in file_entries]
        outputs.append(self._output_path(destination, "sys/main.dol"))
        manifest_path = self._output_path(destination, "manifest.json")
        outputs.append(manifest_path)
        dvd_index_path = self._output_path(destination, "dvd-index.bin")
        outputs.append(dvd_index_path)

        if not force:
            existing = [path for path in outputs if path.exists()]
            if existing:
                raise DiscError(
                    f"destination would overwrite {existing[0]}; use --force explicitly"
                )

        destination.mkdir(parents=True, exist_ok=True)
        extracted: list[dict[str, object]] = []
        total_bytes = sum(entry.size for entry in file_entries) + self.dol_size
        copied_bytes = 0

        def report(bytes_written: int) -> None:
            nonlocal copied_bytes
            copied_bytes += bytes_written
            if progress is not None:
                progress(copied_bytes, total_bytes)

        if progress is not None:
            progress(0, total_bytes)
        with self.image_path.open("rb") as stream:
            for entry, output in zip(file_entries, outputs, strict=False):
                output.parent.mkdir(parents=True, exist_ok=True)
                sha1 = self._copy_range(
                    stream, entry.offset, entry.size, output, on_bytes_written=report
                )
                extracted.append(
                    {
                        "entry_number": entry.entry_number,
                        "path": entry.path,
                        "size": entry.size,
                        "sha1": sha1,
                    }
                )

            dol_output = self._output_path(destination, "sys/main.dol")
            dol_output.parent.mkdir(parents=True, exist_ok=True)
            self._copy_range(
                stream, self.dol_offset, self.dol_size, dol_output,
                on_bytes_written=report,
            )

        manifest: dict[str, object] = {
            "schema_version": 1,
            "game": asdict(self.metadata()),
            "dvd_index": "dvd-index.bin",
            "files": extracted,
            "main_dol": {
                "path": "sys/main.dol",
                "size": self.dol_size,
                "sha1": self.dol_sha1,
            },
        }
        self._write_dvd_index(dvd_index_path, extracted)
        temporary_manifest = manifest_path.with_suffix(".json.tmp")
        temporary_manifest.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary_manifest, manifest_path)
        return manifest

    @staticmethod
    def _write_dvd_index(
        destination: Path, extracted: list[dict[str, object]]
    ) -> None:
        temporary = destination.with_name(destination.name + ".tmp")
        try:
            with temporary.open("wb") as output:
                output.write(
                    struct.pack(">8sII", DVD_INDEX_MAGIC, DVD_INDEX_VERSION, len(extracted))
                )
                for entry in extracted:
                    path = str(entry["path"]).encode("utf-8")
                    digest = bytes.fromhex(str(entry["sha1"]))
                    if len(path) > 0xFFFFFFFF or len(digest) != 20:
                        raise DiscError("cannot encode DVD index entry")
                    output.write(
                        struct.pack(">IQI", int(entry["entry_number"]), int(entry["size"]), len(path))
                    )
                    output.write(digest)
                    output.write(path)
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, destination)
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise

    @staticmethod
    def _copy_range(
        stream: BinaryIO,
        offset: int,
        size: int,
        destination: Path,
        *,
        on_bytes_written: Callable[[int], None] | None = None,
    ) -> str:
        digest = hashlib.sha1()
        temporary = destination.with_name(destination.name + ".tmp")
        stream.seek(offset)
        remaining = size
        try:
            with temporary.open("wb") as output:
                while remaining:
                    chunk = stream.read(min(remaining, COPY_CHUNK_SIZE))
                    if not chunk:
                        raise DiscError("disc image ended during extraction")
                    output.write(chunk)
                    digest.update(chunk)
                    remaining -= len(chunk)
                    if on_bytes_written is not None:
                        on_bytes_written(len(chunk))
                output.flush()
                os.fsync(output.fileno())
            os.replace(temporary, destination)
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
        return digest.hexdigest()


def _metadata_json(disc: GameCubeDisc) -> str:
    payload = asdict(disc.metadata())
    payload["files"] = sum(not entry.is_directory for entry in disc.entries)
    payload["directories"] = sum(entry.is_directory for entry in disc.entries)
    return json.dumps(payload, indent=2, sort_keys=True)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect_parser = subparsers.add_parser("inspect", help="inspect and validate an image")
    inspect_parser.add_argument("image", type=Path)

    extract_parser = subparsers.add_parser("extract", help="extract a supported image")
    extract_parser.add_argument("image", type=Path)
    extract_parser.add_argument("destination", type=Path)
    extract_parser.add_argument(
        "--force", action="store_true", help="overwrite existing extracted files"
    )
    extract_parser.add_argument(
        "--progress-file", type=Path,
        help="write copied and total bytes to this file while extracting",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        disc = GameCubeDisc(args.image)
        if args.command == "inspect":
            print(_metadata_json(disc))
            return 0 if disc.supported else 3
        if args.command == "extract":
            def write_progress(copied: int, total: int) -> None:
                if args.progress_file is None:
                    return
                progress_path = args.progress_file
                progress_path.parent.mkdir(parents=True, exist_ok=True)
                # The Windows setup window polls this file while extraction is
                # running. Replacing an open file is denied on Windows, so
                # update it in place; a partially read value is simply ignored
                # by the next UI timer tick.
                progress_path.write_text(f"{copied} {total}\n", encoding="ascii")

            manifest = disc.extract(
                args.destination, force=args.force, progress=write_progress
            )
            print(json.dumps(manifest["game"], indent=2, sort_keys=True))
            return 0
        raise AssertionError(f"unhandled command: {args.command}")
    except (DiscError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
