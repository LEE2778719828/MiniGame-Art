"""
Static health check for delivered FBX files, run WITHOUT Unreal.

Two separate defects have already reached us in the same art drop, and neither was visible
until import failed (or worse, succeeded and produced empty clips):

  1. Stray padding in the footer. The binary format wants the top-level null record to be
     immediately followed by the 16-byte footer id. One file carried 13 extra zero bytes in
     between, so the SDK read zeros where it expected the id and rejected the whole file as
     corrupted -- within 2ms, before touching any data.

  2. Every key array zlib-compressed. Blender decides per array whether to compress, so a file
     where all of them are compressed did not come straight out of Blender. In that file the
     curves parsed fine with a generic reader but came into Unreal empty.

Both smell like the file was re-serialized by some tool after export, which also copies the
Creator string across, so the origin is not visible in the metadata.

Usage:
  <UE>/Engine/Binaries/ThirdParty/Python3/Win64/python.exe Tools/ValidateArtFbx.py <file.fbx> [...]
"""

import os
import struct
import sys
import zlib

MAGIC = b"Kaydara FBX Binary  \x00"
# The file closes with a 16-byte id whose leading 12 bytes are fixed; the trailing 4 vary with
# the format version, so only the fixed part is worth checking.
FOOTER_MAGIC = bytes([0xF8, 0x5A, 0x8C, 0x6A, 0xDE, 0xF5, 0xD9, 0x7E, 0xEC, 0xE9, 0x0C, 0xE3])

ARRAY_TYPES = {"f": 4, "d": 8, "l": 8, "i": 4, "b": 1}
SCALAR_TYPES = {"Y": 2, "C": 1, "I": 4, "F": 4, "D": 8, "L": 8}

TRACE = os.environ.get("FBXTRACE") == "1"
TRACE_DEPTH = 3


class Reader(object):
    def __init__(self, blob):
        self.blob = blob
        self.pos = 0
        self.version = 0
        self.wide = False
        # Census filled in while walking.
        self.array_encodings = {}
        self.node_count = 0
        self.decompress_failures = 0
        self.stacks = []
        self.curve_nodes = 0

    def u8(self):
        value = self.blob[self.pos]
        self.pos += 1
        return value

    def u32(self):
        value = struct.unpack_from("<I", self.blob, self.pos)[0]
        self.pos += 4
        return value

    def u64(self):
        value = struct.unpack_from("<Q", self.blob, self.pos)[0]
        self.pos += 8
        return value

    def offset(self):
        return self.u64() if self.wide else self.u32()


def read_property(reader):
    kind = chr(reader.u8())

    if kind in SCALAR_TYPES:
        reader.pos += SCALAR_TYPES[kind]
        return

    if kind in ARRAY_TYPES:
        length = reader.u32()
        encoding = reader.u32()
        compressed = reader.u32()
        reader.array_encodings[encoding] = reader.array_encodings.get(encoding, 0) + 1
        payload = reader.blob[reader.pos:reader.pos + compressed]
        reader.pos += compressed
        if encoding == 1:
            try:
                zlib.decompress(payload)
            except zlib.error:
                reader.decompress_failures += 1
        return

    if kind in ("S", "R"):
        # Read the length into a local first: "pos += u32()" would discard the 4 bytes that
        # u32() itself consumed, because the augmented assignment captures pos before the call.
        length = reader.u32()
        reader.pos += length
        return

    raise ValueError("unknown property type {!r} at offset {}".format(kind, reader.pos - 1))


def read_node(reader, problems, depth=0):
    start = reader.pos
    end_offset = reader.offset()
    num_properties = reader.offset()
    property_list_len = reader.offset()
    name_len = reader.u8()
    name = reader.blob[reader.pos:reader.pos + name_len]
    reader.pos += name_len

    if end_offset == 0:
        return None  # null record: end of this sibling list

    reader.node_count += 1
    if name == b"AnimationStack":
        reader.stacks.append(start)
    elif name == b"AnimationCurveNode":
        reader.curve_nodes += 1

    if TRACE and depth < TRACE_DEPTH:
        print("    {}{} @{} end={} props={} plen={}".format(
            "  " * depth, name.decode("ascii", "replace"), start,
            end_offset, num_properties, property_list_len))

    property_start = reader.pos
    for _ in range(num_properties):
        read_property(reader)

    # Trust the declared length over our own property walk: an unknown property type would
    # otherwise desynchronise everything that follows.
    if reader.pos != property_start + property_list_len:
        problems.append("node {!r}: properties ended at +{} but declared +{}".format(
            name.decode("ascii", "replace"), reader.pos - property_start, property_list_len))
        reader.pos = property_start + property_list_len

    # Anything left before EndOffset is a nested list, itself terminated by a null record.
    while reader.pos < end_offset:
        if read_node(reader, problems, depth + 1) is None:
            break

    if reader.pos != end_offset:
        problems.append("node {!r} ends at {} but declared {}".format(
            name.decode("ascii", "replace"), reader.pos, end_offset))
        reader.pos = end_offset

    return name


def validate(path):
    print("=" * 72)
    print(os.path.basename(path))
    problems = []

    with open(path, "rb") as handle:
        blob = handle.read()
    print("  size: {:,} bytes".format(len(blob)))

    if not blob.startswith(MAGIC):
        print("  FAIL: not a binary FBX (header magic missing)")
        return False

    reader = Reader(blob)
    reader.pos = len(MAGIC) + 2
    reader.version = reader.u32()
    reader.wide = reader.version >= 7500
    print("  version: {}".format(reader.version))

    # Walk the top-level record list.
    while True:
        before = reader.pos
        try:
            if read_node(reader, problems) is None:
                break
        except (ValueError, struct.error, IndexError) as err:
            problems.append("parse aborted at offset {}: {}".format(before, err))
            break

    after_null_record = reader.pos
    print("  nodes: {:,}   AnimationStacks: {}   AnimationCurveNodes: {:,}".format(
        reader.node_count, len(reader.stacks), reader.curve_nodes))

    # Defect 1: the footer id must start right here, no padding.
    padding = 0
    while (after_null_record + padding < len(blob)
           and blob[after_null_record + padding] == 0):
        padding += 1
    if padding:
        problems.append(
            "footer padding: {} stray zero bytes between the top-level null record and the "
            "footer id -- the SDK will reject this file as corrupted".format(padding))
    print("  footer padding: {} bytes {}".format(padding, "(BAD)" if padding else "(ok)"))

    if blob[-16:-4] != FOOTER_MAGIC:
        problems.append("file does not end with the expected 16-byte footer id")

    # Defect 2: an all-compressed census means the file was re-serialized.
    total_arrays = sum(reader.array_encodings.values())
    compressed = reader.array_encodings.get(1, 0)
    raw = reader.array_encodings.get(0, 0)
    print("  key/data arrays: {:,} total, {:,} raw, {:,} zlib".format(total_arrays, raw, compressed))
    if total_arrays and raw == 0:
        problems.append(
            "every one of the {} arrays is zlib-compressed; Blender compresses selectively, so "
            "this file was most likely re-serialized by another tool -- curves tend to import "
            "empty".format(total_arrays))
    if reader.decompress_failures:
        problems.append("{} compressed arrays failed to inflate".format(reader.decompress_failures))

    if problems:
        print("  RESULT: FAIL")
        for item in problems:
            print("    - " + item)
        return False

    print("  RESULT: PASS")
    return True


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    return 0 if all([validate(path) for path in argv]) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
