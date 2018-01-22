import os
import struct
import json
import math
import time
from time import sleep
import tempfile
import shutil
import uuid
import numpy
import pmt
from gnuradio import gr, gr_unittest, blocks, analog

from sigmf import sigmf_swig as sigmf


def sig_source_c(samp_rate, freq, amp, N):
    t = map(lambda x: float(x) / samp_rate, xrange(N))
    y = map(lambda x: amp * math.cos(2. * math.pi * freq * x) +
            1j * amp * math.sin(2. * math.pi * freq * x), t)
    return y


class tag_injector(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="tag_injector",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.inject_tag = None

    def work(self, input_items, output_items):
        output_items[0][:] = input_items[0]
        if self.inject_tag:
            offset = self.nitems_read(0) + 1
            for key, val in self.inject_tag.items():
                self.add_item_tag(
                    0, offset, pmt.to_pmt(key), pmt.to_pmt(val))
            self.inject_tag = None
        return len(output_items[0])


class qa_sink(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def temp_file_names(
            self, ending_one="sigmf-data", ending_two="sigmf-meta"):
        name = uuid.uuid4().hex
        if ending_one:
            name_one = name + "." + ending_one
        if ending_two:
            name_two = name + "." + ending_two
        return os.path.join(self.test_dir, name_one), \
            os.path.join(self.test_dir, name_two)

    def test_normal_write(self):
        N = 1000
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        description = "This is a test of the sigmf sink."
        author = "Just some person"
        file_license = "CC-0"
        hardware = "Vector Source"
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               data_file,
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)

        # Make sure the get_data_path method works
        self.assertEqual(data_file, file_sink.get_data_path())

        # And get_meta_path
        self.assertEqual(json_file, file_sink.get_meta_path())

        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.run()

        # check that data file equals data
        read_data = []
        with open(data_file, "r") as f:
            try:
                while True:
                    real = struct.unpack('f', f.read(4))[0]
                    imag = struct.unpack('f', f.read(4))[0]
                    read_data.append(real + (imag * 1j))
            except:
                pass
        self.assertComplexTuplesAlmostEqual(read_data, data)

        # check that the metadata matches up
        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)

            # Check global meta
            assert meta["global"]["core:description"] == description
            assert meta["global"]["core:author"] == author
            assert meta["global"]["core:license"] == file_license
            assert meta["global"]["core:hw"] == hardware

            # Check captures meta
            assert meta["captures"][0]["core:sample_start"] == 0

    def test_tags_to_capture_segment(self):
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               data_file,
                               samp_rate,
                               "testing capture segment tags",
                               "me",
                               "No License",
                               "wave source",
                               False)

        injector = tag_injector()
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()

        # Inject a bunch of tags that should make capture segments
        for i in range(100):
            sleep(.01)
            frac, int_part = math.modf(time.time())
            injector.inject_tag = {
                "rx_freq": i * 1000,
                "rx_rate": i * 500,
                "rx_time": (int(int_part), frac)
            }
        tb.stop()
        tb.wait()
        metadata = json.load(open(json_file, "r"))

        # There should be 100 capture segments
        self.assertEqual(len(metadata["captures"]), 100)

        # And the data in them should match the tags we created
        for i in range(99):
            self.assertEqual(
                metadata["captures"][i + 1]["core:frequency"],
                i * 1000)
            self.assertEqual(
                metadata["captures"][i + 1]["core:sample_rate"],
                i * 500)

    # NOTE: This test fails every once in a while
    def test_tags_to_annotation_segments(self):
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               data_file,
                               samp_rate,
                               "testing annotation segment tags",
                               "me",
                               "No License",
                               "wave source",
                               False)

        injector = tag_injector()
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()

        # Inject a bunch of tags that should make annotation segments
        for i in range(101):
            sleep(.001)
            frac, int_part = math.modf(time.time())
            injector.inject_tag = {
                "test:a": i,
                "test:b": True,
                "test:c": 2.33
            }

        tb.stop()
        tb.wait()
        metadata = json.load(open(json_file, "r"))

        # There should be 100 annotation segments
        self.assertEqual(len(metadata["annotations"]), 100)
        for i in range(100):
            self.assertEqual(metadata["annotations"][i]["test:a"], i)
            self.assertEqual(metadata["annotations"][i]["test:b"], True)
            self.assertEqual(metadata["annotations"][i]["test:c"], 2.33)

    def test_write_methods(self):
        pass

        # TODO: write a test to make sure that calling the open and close
        # methods works correctly

    def test_append(self):
        pass

        # TODO: write a test to ensure that the append option in the ctor
        # does the right thing
