"""
Dump the object graph of a binary FBX without Unreal or the FBX SDK.

Answers the questions that decide whether an art drop is usable as a character:
which meshes are in the file, how heavy each one is, whether the meshes carry skin
clusters (bind weights) or are plain static geometry, and which bones exist.

Usage:
    python Tools/InspectFbxStructure.py <file.fbx> [...]
"""

import os
import struct
import sys
import zlib

MAGIC = b"Kaydara FBX Binary  \x00"

ARRAY_TYPES = {"f": 4, "d": 8, "l": 8, "i": 4, "b": 1}
SCALAR_TYPES = {"Y": 2, "C": 1, "I": 4, "F": 4, "D": 8, "L": 8}


class Reader(object):
    def __init__(self, blob):
        self.blob = blob
        self.pos = 0
        self.wide = False

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


class Array(object):
    """An array property: only the element count matters here, not the payload."""

    def __init__(self, kind, length, payload, encoding):
        self.kind = kind
        self.length = length
        self._payload = payload
        self._encoding = encoding

    def values(self):
        raw = self._payload
        if self._encoding == 1:
            raw = zlib.decompress(raw)
        return struct.unpack_from("<{}{}".format(self.length, self.kind), raw, 0)


class Node(object):
    def __init__(self, name):
        self.name = name
        self.props = []
        self.children = []

    def child(self, name):
        for item in self.children:
            if item.name == name:
                return item
        return None

    def kids(self, name):
        return [item for item in self.children if item.name == name]


def read_property(reader):
    kind = chr(reader.u8())

    if kind in SCALAR_TYPES:
        size = SCALAR_TYPES[kind]
        raw = reader.blob[reader.pos:reader.pos + size]
        reader.pos += size
        fmt = {"Y": "<h", "C": "<b", "I": "<i", "F": "<f", "D": "<d", "L": "<q"}[kind]
        return struct.unpack_from(fmt, raw, 0)[0]

    if kind in ARRAY_TYPES:
        length = reader.u32()
        encoding = reader.u32()
        compressed = reader.u32()
        payload = reader.blob[reader.pos:reader.pos + compressed]
        reader.pos += compressed
        return Array(kind, length, payload, encoding)

    if kind in ("S", "R"):
        length = reader.u32()
        raw = reader.blob[reader.pos:reader.pos + length]
        reader.pos += length
        return raw if kind == "R" else raw.decode("utf-8", "replace")

    raise ValueError("unknown property type {!r} at offset {}".format(kind, reader.pos - 1))


def read_node(reader):
    end_offset = reader.offset()
    num_properties = reader.offset()
    property_list_len = reader.offset()
    name_len = reader.u8()
    name = reader.blob[reader.pos:reader.pos + name_len].decode("ascii", "replace")
    reader.pos += name_len

    if end_offset == 0:
        return None

    node = Node(name)
    property_start = reader.pos
    for _ in range(num_properties):
        node.props.append(read_property(reader))
    reader.pos = property_start + property_list_len

    while reader.pos < end_offset:
        child = read_node(reader)
        if child is None:
            break
        node.children.append(child)
    reader.pos = end_offset
    return node


def object_name(node):
    """Object names are stored as "name\\x00\\x01ClassName"."""
    for prop in node.props:
        if isinstance(prop, str) and "\x00\x01" in prop:
            return prop.split("\x00\x01")[0]
    return "?"


def object_subtype(node):
    strings = [p for p in node.props if isinstance(p, str)]
    return strings[-1] if len(strings) >= 2 else ""


def inspect(path):
    print("=" * 78)
    print(os.path.basename(path))

    with open(path, "rb") as handle:
        blob = handle.read()
    if not blob.startswith(MAGIC):
        print("  not a binary FBX")
        return

    reader = Reader(blob)
    reader.pos = len(MAGIC) + 2
    version = reader.u32()
    reader.wide = version >= 7500

    roots = []
    while True:
        node = read_node(reader)
        if node is None:
            break
        roots.append(node)

    creator = ""
    for node in roots:
        if node.name == "Creator" and node.props:
            creator = str(node.props[0])
    print("  version: {}   size: {:,} bytes".format(version, len(blob)))
    print("  creator: {}".format(creator))

    objects = None
    connections = None
    for node in roots:
        if node.name == "Objects":
            objects = node
        elif node.name == "Connections":
            connections = node
    if objects is None:
        print("  no Objects section")
        return

    geometry_by_id = {}
    models = []
    deformers = []
    for node in objects.children:
        if node.name == "Geometry":
            geometry_by_id[node.props[0]] = node
        elif node.name == "Model":
            models.append(node)
        elif node.name == "Deformer":
            deformers.append(node)

    # Geometry -> Model links live in Connections as (child_id, parent_id) pairs.
    geometry_owner = {}
    if connections is not None:
        model_ids = {node.props[0]: node for node in models}
        for link in connections.kids("C"):
            props = link.props
            if len(props) >= 3 and props[0] == "OO":
                child_id, parent_id = props[1], props[2]
                if child_id in geometry_by_id and parent_id in model_ids:
                    geometry_owner[child_id] = object_name(model_ids[parent_id])

    print("\n  meshes ({}):".format(len(geometry_by_id)))
    print("    {:<24} {:>8} {:>9}".format("owner model", "verts", "tris"))
    total_verts = 0
    for geom_id, geom in geometry_by_id.items():
        vertices = geom.child("Vertices")
        indices = geom.child("PolygonVertexIndex")
        verts = vertices.props[0].length // 3 if vertices and vertices.props else 0
        # Negative index marks the last corner of a polygon.
        polys = 0
        if indices and indices.props:
            polys = sum(1 for v in indices.props[0].values() if v < 0)
        total_verts += verts
        print("    {:<24} {:>8,} {:>9,}".format(
            geometry_owner.get(geom_id, object_name(geom)), verts, polys))
    print("    {:<24} {:>8,}".format("TOTAL", total_verts))

    limbs = [object_name(m) for m in models if object_subtype(m) == "LimbNode"]
    nulls = [object_name(m) for m in models if object_subtype(m) == "Null"]
    skins = [d for d in deformers if object_subtype(d) == "Skin"]
    clusters = [d for d in deformers if object_subtype(d) == "Cluster"]
    stacks = [n for n in objects.children if n.name == "AnimationStack"]

    print("\n  rig:")
    print("    bones (LimbNode):   {}".format(len(limbs)))
    print("    Null nodes:         {}  {}".format(len(nulls), nulls[:6]))
    print("    Skin deformers:     {}".format(len(skins)))
    print("    Skin clusters:      {}".format(len(clusters)))
    print("    AnimationStacks:    {}  {}".format(
        len(stacks), [object_name(s) for s in stacks]))

    if limbs:
        print("\n  bone names ({}):".format(len(limbs)))
        for i in range(0, len(limbs), 4):
            print("    " + "  ".join("{:<18}".format(b) for b in limbs[i:i + 4]))

    print("\n  VERDICT: {}".format(
        "SKINNED (usable as skeletal mesh)" if clusters and limbs
        else "STATIC geometry only (no bind weights)"))


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    for path in argv:
        inspect(path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
