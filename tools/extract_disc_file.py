#!/usr/bin/env python3
"""Extract a file from a GameCube disc image the user owns."""

import argparse
import os
import struct
import sys


class DiscError(Exception):
    pass


def load_fst(disc, disc_size):
    header = disc.read(0x42C)
    if len(header) < 0x42C or struct.unpack_from(">I", header, 0x1C)[0] != 0xC2339F3D:
        raise DiscError("not a valid GameCube disc image (bad magic)")
    offset, size = struct.unpack_from(">II", header, 0x424)
    if offset + size > disc_size or size < 12:
        raise DiscError("invalid FST bounds in disc header")
    disc.seek(offset)
    root = disc.read(12)
    if len(root) != 12:
        raise DiscError("truncated disc header")
    tag, value = struct.unpack_from(">I", root)[0], struct.unpack_from(">I", root, 8)[0]
    if tag & 0x01000000:  # Standard GC FST: first byte marks directories.
        standard = True
    elif tag & 1:  # Layout in which the first byte is the low bit of the name offset.
        standard = False
    else:
        raise DiscError("invalid FST root directory entry")
    # Discs in the wild carry either an entry count or a byte size in the root's
    # third word; accept whichever fits the FST the header declared.
    if value * 12 <= size:
        count, table_size = value, value * 12
    elif value % 12 == 0 and value <= size:
        count, table_size = value // 12, value
    else:
        raise DiscError("invalid FST entry table size")
    if count < 1:
        raise DiscError("empty FST")
    disc.seek(offset)
    data = disc.read(size)
    if len(data) != size:
        raise DiscError("truncated FST")
    entries = [struct.unpack_from(">III", data, i * 12) for i in range(count)]
    return entries, data[table_size:], standard


def is_dir(entry, standard):
    return bool(entry[0] & (0x01000000 if standard else 1))


def entry_name(entry, strings, standard):
    offset = entry[0] & 0x00FFFFFF if standard else entry[0] >> 1
    if offset >= len(strings): raise DiscError("invalid name offset in FST")
    end = strings.find(b"\0", offset)
    if end < 0: raise DiscError("unterminated name in FST")
    return strings[offset:end].decode("utf-8", errors="replace")


def dir_end(index, entries, standard):
    end = len(entries) if index == 0 else entries[index][2 if standard else 1]
    if end <= index or end > len(entries): raise DiscError("invalid directory boundary in FST")
    return end


def find_file(path, entries, strings, standard):
    parts = [part for part in path.split("/") if part]
    if not parts or any(part in (".", "..") for part in parts): raise DiscError("path not found on disc: " + path)
    directory, boundary = 0, len(entries)
    for pos, part in enumerate(parts):
        index = directory + 1
        while index < boundary and entry_name(entries[index], strings, standard) != part:
            entry = entries[index]
            index = dir_end(index, entries, standard) if is_dir(entry, standard) else index + 1
        if index == boundary: raise DiscError("path not found on disc: " + path)
        entry = entries[index]
        if pos == len(parts) - 1:
            if is_dir(entry, standard):
                raise DiscError("path is a directory, not a file: " + path)
            return entry
        if not is_dir(entry, standard):
            raise DiscError("path component is not a directory: " + part)
        directory, boundary = index, dir_end(index, entries, standard)


def list_tree(entries, strings, standard):
    def walk(directory, parent_end, prefix):
        index = directory + 1
        while index < parent_end:
            entry = entries[index]
            path = prefix + entry_name(entry, strings, standard)
            if is_dir(entry, standard):
                end = dir_end(index, entries, standard)
                if end > parent_end: raise DiscError("invalid nested directory boundary in FST")
                print(path + "/")
                walk(index, end, path + "/")
                index = end
            else:
                print(path)
                index += 1
    walk(0, len(entries), "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", help="GameCube disc image")
    parser.add_argument("path", nargs="?", help="file path as it appears in the disc FST")
    parser.add_argument("-o", "--output", help="output file")
    parser.add_argument("-l", "--list", action="store_true", help="list the disc file tree")
    args = parser.parse_args()
    if (args.list and (args.path or args.output)) or (not args.list and (not args.path or not args.output)):
        parser.error("use DISC -l, or DISC PATH -o OUTPUT")
    try:
        with open(args.disc, "rb") as disc:
            disc_size = os.fstat(disc.fileno()).st_size
            entries, strings, standard = load_fst(disc, disc_size)
            if args.list:
                list_tree(entries, strings, standard)
            else:
                offset, size = find_file(args.path, entries, strings, standard)[1:]
                if offset + size > disc_size: raise DiscError("file data extends beyond the disc image")
                disc.seek(offset)
                contents = disc.read(size)
                if len(contents) != size: raise DiscError("truncated file data on disc")
                with open(args.output, "wb") as output:
                    output.write(contents)
                print("Extracted {} bytes".format(size))
    except (OSError, DiscError) as error:
        print("error: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
