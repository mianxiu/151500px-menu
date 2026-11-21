"""Small read-only previews from the running Photoshop version's saved pattern catalog."""
from collections import Counter
from functools import lru_cache
from pathlib import Path
import os
import struct

SIDE = 24
MAX_FILE = 32 * 1024 * 1024


def _row(data, width):
    """Decode a single Photoshop PackBits scanline."""
    result = bytearray()
    pos = 0
    while pos < len(data):
        code = data[pos]
        pos += 1
        if code < 128:
            n = code + 1
            if pos + n > len(data):
                raise ValueError('Short literal')
            result.extend(data[pos:pos+n])
            pos += n
        elif code > 128:
            result.extend(bytes([data[pos]]) * (257-code))
            pos += 1
        if len(result) > width:
            raise ValueError('Overlong scanline')
    if len(result) != width:
        raise ValueError('Short scanline')
    return result


def _samples(data, w, h, compression):
    offsets = [0]
    if compression == 1:
        for length, in struct.iter_unpack('>H', data[:2*h]):
            offsets.append(offsets[-1] + length)
        data = data[2*h:]
        if len(offsets) != h+1 or offsets[-1] != len(data):
            raise ValueError('Invalid row lengths')
    elif compression != 0 or len(data) != w*h:
        raise ValueError('Unsupported channel')
    xs = [min(w-1, (2*x+1)*w//(2*SIDE)) for x in range(SIDE)]
    samples = bytearray()
    for y in range(SIDE):
        line = (2*y+1)*h//(2*SIDE)
        row = _row(data[offsets[line]:offsets[line+1]], w) if compression else data[line*w:(line+1)*w]
        samples.extend(row[x] for x in xs)
    return samples


def _tile(block, mode, palette):
    top, left, h, w, channels = struct.unpack_from('>4iI', block)
    if top or left or not (0 < w <= 8192 and 0 < h <= 8192 and channels <= 32):
        return None
    planes = []
    pos = 20
    for channel in range(channels+2):
        written, = struct.unpack_from('>I', block, pos)
        pos += 4
        if not written:
            continue
        size, = struct.unpack_from('>I', block, pos)
        pos += 4
        if not size:
            continue
        if size < 23 or pos+size > len(block) or channel != len(planes) or channel >= (3 if mode == 3 else 1):
            return None
        depth, y, x, bottom, right, bits, compression = struct.unpack_from('>I4iHB', block, pos)
        if (depth, y, x, bottom, right, bits) != (8, 0, 0, h, w, 8):
            return None
        planes.append(_samples(block[pos+23:pos+size], w, h, compression))
        pos += size
    if mode == 3 and len(planes) == 3:
        return bytes(v for pixel in zip(*planes) for v in pixel).hex()
    if mode in (1, 2) and len(planes) == 1:
        return b''.join(palette[3*v:3*v+3] if mode == 2 else bytes([v])*3 for v in planes[0]).hex()


@lru_cache(maxsize=1)
def _read(path, signature, names):
    with path.open('rb') as stream:
        data = memoryview(stream.read(min(MAX_FILE, signature[0])+1))
    if len(data) != signature[0] or _signature(path) != signature:
        return {}
    magic, version, count = struct.unpack_from('>4sHI', data)
    if (magic, version) != (b'8BPT', 1) or count > 4096:
        return {}
    tiles, seen = {}, set()
    pos = 10
    for _ in range(count):
        version, mode, _, _, chars = struct.unpack_from('>IIHHI', data, pos)
        if version != 1 or chars > 4096:
            return {}
        pos += 16
        name = bytes(data[pos:pos+chars*2]).decode('utf-16be').rstrip('\0')
        pos += chars*2
        pos += 1+data[pos]
        palette = data[pos:pos+768] if mode == 2 else None
        if mode == 2:
            pos += 772
        version, size = struct.unpack_from('>II', data, pos)
        pos += 8
        if pos+size > len(data):
            return {}
        if name in names:
            if name in seen:
                tiles.pop(name, None)
            elif version == 3 and mode in (1, 2, 3):
                try:
                    tile = _tile(data[pos:pos+size], mode, palette)
                except (IndexError, ValueError, struct.error):
                    tile = None
                if tile:
                    tiles[name] = tile
            seen.add(name)
        pos += size
    return tiles


def _signature(path):
    info = path.stat()
    return info.st_size, info.st_mtime_ns


def thumbnails(host_version, names):
    try:
        year = 1999 + int(host_version.split('.')[0])
        product = f'Adobe Photoshop {year}'
        path = Path(os.environ['APPDATA'])/'Adobe'/product/(product+' Settings')/'Patterns.psp'
        signature = _signature(path)
        if signature[0] <= MAX_FILE:
            unique = frozenset(name for name, count in Counter(names).items() if count == 1)
            return _read(path, signature, unique) if unique else {}
    except (AttributeError, IndexError, KeyError, OSError, ValueError, struct.error):
        pass
    return {}
