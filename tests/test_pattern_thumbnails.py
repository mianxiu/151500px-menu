import struct
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

from backend.pattern_thumbnails import SIDE, thumbnails


def record(name, mode, rows):
    name = (name+'\0').encode('utf-16be')
    header = struct.pack('>IIHHI', 1, mode, 1, 2, len(name)//2) + name + b'\x01x'
    if mode == 2:
        header += bytes.fromhex('ff00000000ff') + bytes(766)
    frame = struct.pack('>4iI', 0, 0, 1, 2, len(rows))
    for compression, row in rows:
        plane = struct.pack('>I4iHB', 8, 0, 0, 1, 2, 8, compression) + row
        frame += struct.pack('>II', 1, len(plane)) + plane
    frame += bytes(8)
    return header + struct.pack('>II', 3, len(frame)) + frame


class PatternThumbnails(unittest.TestCase):
    def test_matching_names_channels_and_fallback(self):
        patterns = [record('Indexed', 2, [(1,b'\x00\x03\x01\x00\x01')]),
                    record('Gray', 1, [(0,bytes([22,28]))]),
                    record('RGB', 3, [(0,bytes([n,255-n])) for n in (90,50,0)]),
                    record('Broken', 2, [(1,b'\x00\x01\xfa')])]
        patterns += [record('Same',1,[(0,b'\x01\x02')])]*2
        with TemporaryDirectory() as folder, patch.dict('os.environ', APPDATA=folder):
            path = Path(folder)/'Adobe/Adobe Photoshop 2023/Adobe Photoshop 2023 Settings/Patterns.psp'
            path.parent.mkdir(parents=True)
            path.write_bytes(struct.pack('>4sHI',b'8BPT',1,len(patterns))+b''.join(patterns))
            names = ['Indexed','Gray','RGB','Broken','Same','Only in PS']
            result = thumbnails('24.0.1',names)
            self.assertEqual(set(result), {'Indexed','Gray','RGB'})
            self.assertEqual(result['Indexed'][:6], 'ff0000')
            self.assertEqual(result['Indexed'][-6:], '0000ff')
            self.assertEqual(result['Gray'][:6], '161616')
            self.assertEqual(result['RGB'][:6], '5a3200')
            self.assertEqual(len(result['RGB']), SIDE*SIDE*6)
            self.assertEqual(thumbnails('24.0.1',['Gray']), {'Gray':result['Gray']})
            self.assertEqual(thumbnails('24.0.1',['Gray','Gray']), {})
            self.assertEqual(thumbnails('25.0.0',names), {})
            path.write_bytes(b'8BPT')
            self.assertEqual(thumbnails('24.0.1',names), {})
