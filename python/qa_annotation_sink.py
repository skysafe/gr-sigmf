#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2018 Paul Wicks
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

from gnuradio import gr, gr_unittest
from test_utils import sig_source_c
from test_blocks import msg_sender
from time import sleep
import tempfile
import pmt
import shutil
import json
from gnuradio import blocks
import os

from gr_sigmf import gr_sigmf_swig as sigmf


class qa_annotation_sink (gr_unittest.TestCase):

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):
        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def make_file(
            self,
            filename,
            annotations=None,
            captures=None,
            global_data=None,
            N=1000,
            type="cf32_le"):
        if (not filename.startswith("/")):
            filename = os.path.join(self.test_dir, filename)
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        file_sink = sigmf.sink(type,
                               filename)
        data_path = file_sink.get_data_path()
        meta_path = file_sink.get_meta_path()

        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.run()
        with open(meta_path, "r+") as f:
            data = json.load(f)
            if annotations is not None:
                for anno in annotations:
                    updated = False
                    for anno_data in data["annotations"]:
                        if anno["core:sample_start"] ==\
                                anno_data["core:sample_start"] and\
                                anno.get("core:sample_count", "NO_COUNT") ==\
                                anno_data.get("core:sample_count", "NO_COUNT"):
                            anno_data.update(anno)
                            updated = True
                            break
                    if not updated:
                        data["annotations"].append(anno)
                data["annotations"].sort(key=lambda a: a["core:sample_start"])
            if captures is not None:
                for capture in captures:
                    updated = False
                    for capture_data in data["captures"]:
                        if capture["core:sample_start"] ==\
                                capture_data["core:sample_start"]:
                            capture_data.update(capture)
                            updated = True
                            break
                    if not updated:
                        data["annotations"].append(anno)
            if global_data:
                data["global"].update(global_data)
            f.seek(0)
            json.dump(data, f, indent=4)
            f.truncate()

        with open(meta_path, "r") as f:
            meta_json = json.load(f)
        return data, meta_json, data_path, meta_path

    def test_annotation_clearing(self):
        '''Test that the annoation filtering logic is working correctly'''

        # things to try
        tests = [
            ("test*", ["blargh:foo"]),
            ("test:foo*", ["blargh:foo", "test:bar", "test:baz"]),
            ("test:ba?", ["blargh:foo", "test:foo", "test:foobar"]),
            ("*:foo", ["test:foobar", "test:bar", "test:baz"]),
            ("*:foo*", ["test:bar", "test:baz"]),
            ("blargh:*", ["test:bar", "test:baz", "test:foobar", "test:foo"])
        ]

        def run_clear_test(filter_str, expected_tags):
            data, json_dict, data_path, json_path = self.make_file(
                "clearing",
                [{
                    "core:sample_start": 0,
                    "test:foo": 1,
                    "test:foobar": 2,
                    "test:bar": 3,
                    "test:baz": 4,
                    "blargh:foo": 5
                }]
            )
            anno_sink = sigmf.annotation_sink(
                data_path, sigmf.annotation_mode_clear(filter_str))
            # Strobe block is just here for the flowgraph to run
            strobe = blocks.message_strobe(pmt.to_pmt("asdf"), 1)
            tb = gr.top_block()
            tb.msg_connect(strobe, "strobe", anno_sink, "annotations")
            tb.start()
            tb.stop()
            tb.wait()
            with open(json_path, "r") as f:
                meta = json.load(f)
                anno_dict = meta["annotations"][0]
                expect_set = set(expected_tags)
                anno_keys = set(anno_dict.keys())
                self.assertIn("core:sample_start", anno_keys)
                anno_keys.remove("core:sample_start")
                self.assertEqual(anno_keys, expect_set,
                                 "Key filtering is incorrect")

        for test in tests:
            run_clear_test(*test)

    def test_annotation_sink_basics(self):
        '''Test of actually using the annotation sink'''
        data, json_dict, data_path, json_path = self.make_file(
            "basics",
            annotations=[{
                "core:sample_start": 0,
                "core:sample_count": 1,
                "test:foo": 1,
                "test:foobar": 2,
                "test:bar": 3,
                "test:baz": 4,
                "blargh:foo": 5
            }]
        )
        anno_sink = sigmf.annotation_sink(
            data_path, sigmf.annotation_mode_clear("blargh:*"))
        sender = msg_sender()
        tb = gr.top_block()
        tb.msg_connect(sender, "out", anno_sink, "annotations")
        tb.start()
        sender.send_msg({
            "core:sample_start": 0,
            "core:sample_count": 1,
            "test:foo": 2
        })
        sender.send_msg({
            "core:sample_start": 0,
            "core:sample_count": 1,
            "test:foo2": "foo2"
        })
        sender.send_msg({
            "core:sample_start": 0,
            "core:sample_count": 3,
            "test:foo3": True
        })
        sender.send_msg({
            "core:sample_start": 1,
            "core:sample_count": 1,
            "test:bing": "asdf"
        })
        # no such thing as head for messages, so have to sleep here :\
        sleep(.1)
        tb.stop()
        tb.wait()

        with open(json_path, "r") as f:
            meta = json.load(f)
            self.assertNotIn(
                "blargh:foo",
                meta["annotations"][0],
                "Clear existing did not work")
            self.assertEqual(meta["annotations"][0]["test:foo"],
                             2, "Replacing existing tag didn't work")
            self.assertEqual(meta["annotations"][0]["test:foo2"],
                             "foo2", "Adding tag to existing segment failure")
            self.assertEqual(meta["annotations"][1]["test:foo3"],
                             True, "New segment create fail")
            self.assertEqual(meta["annotations"][2]["test:bing"],
                             "asdf", "New tag add failure")
            self.assertEqual(meta["annotations"][2]["core:sample_start"],
                             1, "New tag add failure")

    def test_annotation_sink_time_offsets(self):
        '''Test of using the annotation sink with absolute time offsets'''
        data, json_dict, data_path, json_path = self.make_file(
            "time_offsets",
            annotations=[{
                "core:sample_start": 0,
                "core:sample_count": 1,
                "test:foo": 1,
                "test:foobar": 2,
                "test:bar": 3,
                "test:baz": 4,
                "blargh:foo": 5
            }],
            captures=[{
                "core:sample_start": 0,
                "core:sample_count": 100,
                "core:datetime": "2018-04-18T21:41:19"
            }],
            global_data={
                "core:sample_rate": 100
            }
        )
        anno_sink = sigmf.annotation_sink(
            data_path,
            sigmf.annotation_mode_clear("test:foo*"),
            sigmf.sigmf_time_mode_absolute)
        sender = msg_sender()
        tb = gr.top_block()
        tb.msg_connect(sender, "out", anno_sink, "annotations")
        tb.start()
        # one second later
        sender.send_msg({
            "time": (1524087680, 0),
            "duration": (1, 0),
            "test:foo": 2
        })
        # one and one half second later
        sender.send_msg({
            "time": (1524087680, .5),
            "duration": (1, 0),
            "test:foo2": 3
        })
        # no such thing as head for messages, so have to sleep here :\
        sleep(.2)
        tb.stop()
        tb.wait()

        with open(json_path, "r") as f:
            meta = json.load(f)
            self.assertEqual(meta["annotations"][1]
                             ["core:sample_start"], 100, "bad sample start")
            self.assertEqual(meta["annotations"][1]
                             ["core:sample_count"], 100, "bad sample count")
            self.assertEqual(meta["annotations"][1]
                             ["test:foo"], 2, "bad anno value")
            self.assertEqual(meta["annotations"][2]
                             ["core:sample_start"], 150, "bad sample start")
            self.assertEqual(meta["annotations"][2]
                             ["core:sample_count"], 100, "bad sample count")
            self.assertEqual(meta["annotations"][2]
                             ["test:foo2"], 3, "bad anno value")

    def test_annotation_sink_time_offsets_no_timebase(self):
        '''Test of using the annotation sink with time offsets in the case
        where there is no timebase in the first capture segment to base
        things off of'''
        data, json_dict, data_path, json_path = self.make_file(
            "time_offsets_no_timebase",
            [
                {
                    "core:sample_start": 0,
                    "core:sample_count": 1,
                },
                {
                    "core:sample_start": 100,
                    "core:sample_count": 100,
                },
            ],
            [{
                "core:sample_start": 0
            }],
            {
                "core:sample_rate": 100
            }
        )
        anno_sink = sigmf.annotation_sink(
            data_path, sigmf.annotation_mode_keep())
        sender = msg_sender()
        tb = gr.top_block()
        tb.msg_connect(sender, "out", anno_sink, "annotations")
        tb.start()
        sender.send_msg({
            "time": (0, 0),
            "duration": (1, .1),
            "test:foo": 2
        })
        sender.send_msg({
            "time": (1, 0),
            "duration": (1, 0),
            "test:foo2": "foo"
        })
        # no such thing as head for messages, so have to sleep here :\
        sleep(.1)
        tb.stop()
        tb.wait()

        with open(json_path, "r") as f:
            meta = json.load(f)
            print(meta)
            self.assertEqual(len(meta["annotations"]),
                             3, "wrong number of annotations")
            self.assertEqual(len(meta["annotations"][0].keys()), 2,
                             "wrong number of keys in first annotation")
            self.assertEqual(meta["annotations"][1]
                             ["test:foo"], 2, "Wrong value in annotation")
            self.assertEqual(
                meta["annotations"][1]["core:sample_start"],
                0, "Wrong value in annotation")
            self.assertEqual(
                meta["annotations"][1]["core:sample_count"],
                110, "Wrong value in annotation")
            self.assertEqual(
                meta["annotations"][2]["core:sample_count"],
                100, "Wrong value in annotation")
            self.assertEqual(
                meta["annotations"][2]["core:sample_start"],
                100, "Wrong value in annotation")
            self.assertEqual(
                meta["annotations"][2]["test:foo2"],
                "foo", "Wrong value in annotation")


if __name__ == '__main__':
    gr_unittest.run(qa_annotation_sink, "qa_annotation_sink.xml")
