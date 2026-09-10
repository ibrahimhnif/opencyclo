import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import sys
import sqlite3
import json
import hashlib

sys.path.insert(0,str(Path(__file__).parents[1]/'tools'))
from build_indonesia_map import export_columns
from verify_map_pack import verify

spec=importlib.util.spec_from_file_location('builder',Path(__file__).parents[1]/'tools/build_offline_map.py')
builder=importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class MapTests(unittest.TestCase):
    def test_indexed_pack_and_corruption_detection(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE segments(tx INTEGER,ty INTEGER,data BLOB)')
            db.execute('INSERT INTO segments VALUES (8192,8192,?)',(struct.pack('<4HB',0,0,65535,65535,1),))
            report=export_columns(db,root,[-0.01,-0.01,0.01,0.01])
            pack=root/'maps'
            report['sha256']={str(p.relative_to(pack)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (pack/'14').glob('*.ocp')}
            (pack/'manifest.json').write_text(json.dumps(report))
            self.assertEqual(verify(pack)['segments'],1)
            column=pack/'14/8192.ocp';column.write_bytes(b'corrupt')
            with self.assertRaises(ValueError):verify(pack)
    def test_clipping(self):
        self.assertEqual(builder.clip(-1,5,11,5,0,0,10,10),(0,5,10,5))
        self.assertIsNone(builder.clip(-1,-1,-2,-2,0,0,10,10))

    def test_build_cross_tile_road_and_empty_coverage(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            source=root/'test.osm'
            source.write_text('<osm><node id="1" lat="0.001" lon="-0.01"/><node id="2" lat="0.001" lon="0.01"/><way id="1"><nd ref="1"/><nd ref="2"/><tag k="highway" v="cycleway"/></way></osm>')
            report=builder.build(source,root/'out',[-0.02,-0.02,0.02,0.02])
            self.assertGreaterEqual(report['segments'],2)
            files=list((root/'out/maps/14').glob('*/*.ocm'))
            self.assertTrue(files)
            for f in files:
                data=f.read_bytes();magic,count=struct.unpack('<4sI',data[:8])
                self.assertEqual(magic,b'OCM1');self.assertEqual(len(data),8+9*count)
            self.assertTrue((root/'out/maps/manifest.json').exists())

if __name__=='__main__': unittest.main()
