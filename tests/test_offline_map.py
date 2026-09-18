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
from build_indonesia_map import export_columns, export_named_columns
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
    def test_named_columns_pack_and_lookup(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',
                       ('Jalan Test',struct.pack('<4H',0,0,65535,65535)))
            export_named_columns(db,root,8192,8192)
            path=root/'maps'/'14'/'8192.ocn'
            self.assertTrue(path.exists())
            data=path.read_bytes()
            magic,index_count,pool_size=struct.unpack_from('<4sII',data,0)
            self.assertEqual(magic,b'OCN1');self.assertEqual(index_count,1)
            y,offset,count=struct.unpack_from('<III',data,12)
            self.assertEqual((y,offset,count),(8192,0,1))
            pool_start=12+index_count*12
            pool=data[pool_start:pool_start+pool_size]
            self.assertEqual(pool[:len(b'Jalan Test')],b'Jalan Test')
            self.assertEqual(pool[len(b'Jalan Test')],0)  # NUL terminator
            seg_start=pool_start+pool_size
            ax,ay,bx,by,name_offset=struct.unpack_from('<4HH',data,seg_start)
            self.assertEqual((ax,ay,bx,by,name_offset),(0,0,65535,65535,0))

    def test_named_columns_multi_row_offset_and_dedup(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            # Row 8192 holds two segments sharing one name; row 8193 a second name.
            db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',('A',struct.pack('<4H',0,0,100,100)))
            db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',('A',struct.pack('<4H',100,100,200,200)))
            db.execute('INSERT INTO named_segments VALUES (8192,8193,?,?)',('B',struct.pack('<4H',5,5,6,6)))
            export_named_columns(db,root,8192,8192)
            data=(root/'maps'/'14'/'8192.ocn').read_bytes()
            magic,index_count,pool_size=struct.unpack_from('<4sII',data,0)
            self.assertEqual(magic,b'OCN1');self.assertEqual(index_count,2)
            self.assertEqual(struct.unpack_from('<III',data,12),(8192,0,2))
            # Second row starts after the first row's two 10-byte segments.
            self.assertEqual(struct.unpack_from('<III',data,24),(8193,20,1))
            pool_start=12+index_count*12
            pool=data[pool_start:pool_start+pool_size]
            # Pool order follows first-seen order while walking rows by ascending ty.
            self.assertEqual(pool,b'A\x00B\x00')
            seg_start=pool_start+pool_size
            first=struct.unpack_from('<4HH',data,seg_start)
            second=struct.unpack_from('<4HH',data,seg_start+10)
            third=struct.unpack_from('<4HH',data,seg_start+20)
            self.assertEqual(first,(0,0,100,100,0));self.assertEqual(second,(100,100,200,200,0))
            self.assertEqual(first[4],second[4])  # within-row dedup: one pool entry
            self.assertEqual(third,(5,5,6,6,2))
            self.assertNotEqual(third[4],first[4])
            self.assertEqual(pool[third[4]:pool.index(b'\x00',third[4])],b'B')

    def test_named_column_integrity_and_corruption_detection(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE segments(tx INTEGER,ty INTEGER,data BLOB)')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            db.execute('INSERT INTO segments VALUES (8192,8192,?)',(struct.pack('<4HB',0,0,65535,65535,1),))
            db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',
                       ('Jalan Test',struct.pack('<4H',0,0,65535,65535)))
            report=export_columns(db,root,[-0.01,-0.01,0.01,0.01])
            report.update(export_named_columns(db,root,8191,8192))
            pack=root/'maps'
            report['sha256']={str(p.relative_to(pack)):hashlib.sha256(p.read_bytes()).hexdigest()
                              for pattern in ('*.ocp','*.ocn') for p in (pack/'14').glob(pattern)}
            (pack/'manifest.json').write_text(json.dumps(report))
            self.assertIn('14/8192.ocn',report['sha256'])
            result=verify(pack)
            self.assertEqual(result['named_columns'],1)
            self.assertEqual(result['named_segments'],1)
            self.assertEqual(result['named_tiles'],1)
            column=pack/'14/8192.ocn';column.write_bytes(column.read_bytes()[:-1])
            with self.assertRaises(ValueError):verify(pack)

    def test_named_columns_skip_oversized_pool(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            for i in range(100):  # 100 distinct ~700-byte names overflow the 65535-byte pool.
                db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',
                           (f'{i:04d}'+'x'*700,struct.pack('<4H',0,0,1,1)))
            report=export_named_columns(db,root,8192,8192)
            self.assertFalse((root/'maps'/'14'/'8192.ocn').exists())
            self.assertEqual(report['skipped_named_columns'],[8192])
            self.assertEqual(report['named_segments'],0);self.assertEqual(report['named_tiles'],0)

    def test_named_columns_skip_oversized_row(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',
                       ('Too Big',struct.pack('<4H',0,0,1,1)*20001))
            db.execute('INSERT INTO named_segments VALUES (8192,8193,?,?)',
                       ('Fine',struct.pack('<4H',2,2,3,3)))
            report=export_named_columns(db,root,8192,8192)
            self.assertEqual(report['skipped_named_tiles'],[[8192,8192]])
            self.assertEqual(report['skipped_named_columns'],[])
            self.assertEqual(report['named_segments'],1);self.assertEqual(report['named_tiles'],1)
            data=(root/'maps'/'14'/'8192.ocn').read_bytes()
            magic,index_count,pool_size=struct.unpack_from('<4sII',data,0)
            self.assertEqual(magic,b'OCN1');self.assertEqual(index_count,1)
            # The skipped row leaves no index entry and consumes no segment bytes.
            self.assertEqual(struct.unpack_from('<III',data,12),(8193,0,1))
            self.assertEqual(len(data),12+12+pool_size+10)

    def test_named_columns_skips_unnamed_columns(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            export_named_columns(db,root,8192,8192)
            self.assertFalse((root/'maps'/'14'/'8192.ocn').exists())

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
