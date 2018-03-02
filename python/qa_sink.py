import os
import struct
import json
import math
import time
from datetime import datetime
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
        self.injected_offset = None

    def work(self, input_items, output_items):
        output_items[0][:] = input_items[0]
        if self.inject_tag:
            offset = self.nitems_read(0) + 1
            for key, val in self.inject_tag.items():
                self.add_item_tag(
                    0, offset, pmt.to_pmt(key), pmt.to_pmt(val))
            self.injected_offset = offset
            self.inject_tag = None
        return len(output_items[0])


class sample_counter(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="sample_counter",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.count = 0

    def work(self, input_items, output_items):
        output_items[0][:] = input_items[0]
        self.count += len(output_items[0])
        return len(output_items[0])


class msg_sender(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="msg_sender",
            in_sig=None,
            out_sig=None
        )
        self.message_port_register_out(pmt.intern("out"))

    def send_msg(self, msg_to_send):
        self.message_port_pub(pmt.intern("out"), pmt.to_pmt(msg_to_send))


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
        tb.wait()

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
        for i in range(100):
            sleep(.001)
            frac, int_part = math.modf(time.time())
            injector.inject_tag = {
                "test:a": i,
                "test:b": True,
                "test:c": 2.33
            }
        sleep(.5)

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
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file_1, json_file_1 = self.temp_file_names()
        data_file_2, json_file_2 = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               data_file_1,
                               samp_rate,
                               "testing write methods",
                               "me",
                               "No License",
                               "wave source",
                               False)

        counter = sample_counter()
        tb = gr.top_block()
        tb.connect(src, counter)
        tb.connect(counter, file_sink)
        tb.start()
        file_sink.set_global_meta("test:a", pmt.to_pmt(1))
        sleep(.2)
        file_sink.open(data_file_2)
        sleep(.2)
        file_sink.set_global_meta("test:b", pmt.to_pmt(2))
        sleep(.2)
        file_sink.close()
        sleep(.2)
        count_1 = counter.count
        sleep(.1)
        count_2 = counter.count
        tb.stop()
        tb.wait()

        # flow graph should still be running after flle
        # close, but dropping all packets on the floor
        self.assertGreater(count_2, count_1)

        # The metadata of the two files should be different
        meta_1 = json.load(open(json_file_1, "r"))
        meta_2 = json.load(open(json_file_2, "r"))
        self.assertEqual(meta_1["global"]["test:a"], 1)
        self.assertTrue("test:b" not in meta_1["global"])

        # import pdb; pdb.set_trace()
        self.assertEqual(meta_2["global"]["test:b"], 2)
        self.assertTrue("test:a" not in meta_2["global"])

    def test_pmt_to_annotation(self):
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
        sender = msg_sender()
        counter = sample_counter()
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, counter)
        tb.connect(counter, file_sink)
        tb.msg_connect(sender, "out", file_sink, "command")

        tb.start()
        # sleep so the streamed annotation isn't the first one
        sleep(.1)
        # Inject one tag
        injector.inject_tag = {"test:a": 1}
        # Wait again so that we know the tag got processed
        sleep(.1)
        # Then tell it to add 2 more via pmts,
        # one before the injected tag
        sender.send_msg({
            "command": "set_annotation_meta",
            "sample_start": 1,
            "sample_count": 10,
            "key": "test:b",
            "val": 22})
        # and one after
        sender.send_msg({
            "command": "set_annotation_meta",
            "sample_start": counter.count + 1,
            "sample_count": 10,
            "key": "test:c",
            "val": True})
        sleep(.25)
        tb.stop()
        tb.wait()
        metadata = json.load(open(json_file, "r"))
        # should be 3 annotations
        self.assertEqual(len(metadata["annotations"]), 3)
        # And they should be these and in this order
        self.assertEqual(metadata["annotations"][0]["test:b"], 22)
        self.assertEqual(metadata["annotations"][1]["test:a"], 1)
        self.assertEqual(metadata["annotations"][2]["test:c"], True)

    def test_msg_annotation_meta_merging(self):
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
        sender = msg_sender()
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.msg_connect(sender, "out", file_sink, "command")

        tb.start()
        sender.send_msg({
            "command": "set_annotation_meta",
            "sample_start": 1,
            "sample_count": 10,
            "key": "test:a",
            "val": 1})

        sender.send_msg({
            "command": "set_annotation_meta",
            "sample_start": 1,
            "sample_count": 10,
            "key": "test:b",
            "val": 2})

        sender.send_msg({
            "command": "set_annotation_meta",
            "sample_start": 1,
            "sample_count": 100,
            "key": "test:c",
            "val": 3})
        sleep(.25)
        tb.stop()
        tb.wait()
        metadata = json.load(open(json_file, "r"))

        # should be 2 annotations
        self.assertEqual(len(metadata["annotations"]), 2)
        # First should have both test:a and test:b
        self.assertEqual(metadata["annotations"][0]["core:sample_count"], 10)
        self.assertEqual(metadata["annotations"][0]["test:a"], 1)
        self.assertEqual(metadata["annotations"][0]["test:b"], 2)
        # Second should just have c
        self.assertEqual(metadata["annotations"][1]["core:sample_count"], 100)
        self.assertEqual(metadata["annotations"][1]["test:c"], 3)

    def test_initally_empty_file_write(self):
        '''Test that if the file is initially empty and then open is
        called, everything works as expected'''
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))

        description = "This is a test of the sigmf sink."
        author = "Just some person"
        file_license = "CC-0"
        hardware = "Sig Source"
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               "",
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)

        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.start()
        time.sleep(.5)
        file_sink.open(data_file)
        time.sleep(.5)
        tb.stop()
        tb.wait()

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

        # check that data was recorded
        data_size = os.path.getsize(data_file)
        assert data_size > 0

    def test_stream_tags_before_file(self):
        '''Test that stream tags received before a file is opened will
        get correctly set as metadata'''
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))

        description = "This is a test of the sigmf sink."
        author = "Just some person"
        file_license = "CC-0"
        hardware = "Sig Source"
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               "",
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)

        injector = tag_injector()
        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()
        time.sleep(.1)
        injector.inject_tag = {"test:a": 1}
        time.sleep(.1)
        injector.inject_tag = {"rx_freq": 900e6}
        time.sleep(.2)
        file_sink.open(data_file)
        time.sleep(.5)
        tb.stop()
        tb.wait()

        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)

            # Check global meta
            assert meta["global"]["core:description"] == description
            assert meta["global"]["core:author"] == author
            assert meta["global"]["core:license"] == file_license
            assert meta["global"]["core:hw"] == hardware

            print(meta)
            # Check captures meta
            assert meta["captures"][0]["core:frequency"] == 900e6
            # Check annotations meta
            assert meta["annotations"][0]["test:a"] == 1

    def test_not_intially_open_annotation_tag_offsets(self):
        '''Test that if a sink is created without a file initially open,
        and then a file is opened, that annotation stream tags will have the
        correct offsets, i.e. they should be set from when the
        file was opened, not when the flowgraph started'''
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))

        description = "This is a test of the sigmf sink."
        author = "Just some person"
        file_license = "CC-0"
        hardware = "Sig Source"
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               "",
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)

        injector = tag_injector()
        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()
        time.sleep(.1)
        file_sink.open(data_file)
        time.sleep(.1)
        injector.inject_tag = {"test:a": 1}
        time.sleep(.1)
        tb.stop()
        tb.wait()
        injected_offset = injector.injected_offset

        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)

            # Check global meta
            assert meta["global"]["core:description"] == description
            assert meta["global"]["core:author"] == author
            assert meta["global"]["core:license"] == file_license
            assert meta["global"]["core:hw"] == hardware

            # Check annotations meta
            # The sample_start should be less than what it was injected
            # at, since no file was open at first, so the internal offsets
            # were off
            assert (meta["annotations"][0]
                    ["core:sample_start"] < injected_offset)

    def test_not_intially_open_capture_tag_offsets(self):
        '''Test that if a sink is created without a file initially open,
        and then a file is opened, that capture stream tags will have the
        correct offsets, i.e. they should be set from when the
        file was opened, not when the flowgraph started'''
        samp_rate = 200000
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))

        description = "This is a test of the sigmf sink."
        author = "Just some person"
        file_license = "CC-0"
        hardware = "Sig Source"
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               "",
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)

        injector = tag_injector()
        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()
        time.sleep(.1)
        file_sink.open(data_file)
        time.sleep(.1)
        injector.inject_tag = {"rx_freq": 900e6}
        time.sleep(.1)
        tb.stop()
        tb.wait()
        injected_offset = injector.injected_offset

        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)

            # Check global meta
            assert meta["global"]["core:description"] == description
            assert meta["global"]["core:author"] == author
            assert meta["global"]["core:license"] == file_license
            assert meta["global"]["core:hw"] == hardware

            # Check capture meta
            # The sample_start should be less than what it was injected
            # at, since no file was open at first, so the internal offsets
            # were off
            assert (meta["captures"][0]
                    ["core:sample_start"] < injected_offset)

    def test_dont_set_empty_global_meta(self):
        '''Don't set global metadata from args if they're empty'''
        N = 1000
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        description = ""
        author = ""
        file_license = ""
        hardware = ""
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               data_file,
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)
        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.run()
        tb.wait()

        # check that the metadata matches up
        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)

            # Check global meta
            assert "core:description" not in meta["global"]
            assert "core:author" not in meta["global"]
            assert "core:license" not in meta["global"]
            assert "core:hw" not in meta["global"]

    def test_capture_datetime_on_start(self):
        '''If a file is open to start then it should have a datetime
        set and that datetime should be sort of accurate'''
        N = 1000
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        description = ""
        author = ""
        file_license = ""
        hardware = ""
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32",
                               data_file,
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)
        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.run()
        tb.wait()

        # check that the metadata matches up
        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)
            print(meta)
            meta_dt_str = meta["captures"][0]["core:datetime"]
            meta_dt = datetime.strptime(
                meta_dt_str, "%Y-%m-%dT%H:%M:%S.%fZ")
            print(meta_dt)
            assert (datetime.utcnow() - meta_dt).total_seconds() < 2
            # Check global meta
            assert "core:description" not in meta["global"]
            assert "core:author" not in meta["global"]
            assert "core:license" not in meta["global"]
            assert "core:hw" not in meta["global"]

