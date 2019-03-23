import sys
import os
import re
import struct
import exceptions
import json
import math
import time
from datetime import datetime
from time import sleep
import tempfile
import shutil
import uuid
from threading import Event
import numpy
import pmt
from multiprocessing import Process, Queue
from gnuradio import gr, gr_unittest, blocks, analog

from gr_sigmf import gr_sigmf_swig as sigmf

from test_blocks import (simple_tag_injector, sample_counter,
                         sample_producer, msg_sender)


def sig_source_c(samp_rate, freq, amp, N):
    t = map(lambda x: float(x) / samp_rate, xrange(N))
    y = map(lambda x: amp * math.cos(2. * math.pi * freq * x) +
            1j * amp * math.sin(2. * math.pi * freq * x), t)
    return y


def parse_iso_ts(ts):
    # strptime can only handle six digits of fractional seconds
    ts = re.sub(r'\.(\d+)Z',
                lambda m: "." + m.group(1)[:6] + "Z",
                ts)
    return datetime.strptime(
        ts, "%Y-%m-%dT%H:%M:%S.%fZ")


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
        file_sink = sigmf.sink("cf32_le",
                               data_file)

        file_sink.set_global_meta("core:sample_rate", samp_rate)
        file_sink.set_global_meta("core:description", description)
        file_sink.set_global_meta("core:author", author)
        file_sink.set_global_meta("core:sample_rate", author)
        file_sink.set_global_meta("core:license", file_license)
        file_sink.set_global_meta("core:hw", hardware)

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
            assert meta["global"]["core:datatype"] == "cf32_le"
            assert meta["global"]["core:description"] == description
            assert meta["global"]["core:author"] == author
            assert meta["global"]["core:license"] == file_license
            assert meta["global"]["core:hw"] == hardware

            # Check captures meta
            assert meta["captures"][0]["core:sample_start"] == 0

    def test_tags_to_capture_segment(self):
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)

        injector = simple_tag_injector()
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

    def test_tags_to_annotation_segments(self):
        '''Test that tags correctly convert to annotation segments'''
        # FIXME: this test is occasionally flaky, as the flowgraph is shutdown
        # before all the messages get to the sink
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)

        injector = simple_tag_injector()
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
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file_1, json_file_1 = self.temp_file_names()
        data_file_2, json_file_2 = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file_1)

        counter = sample_counter()
        tb = gr.top_block()
        tb.connect(src, counter)
        tb.connect(counter, file_sink)
        tb.start()
        file_sink.set_global_meta("test:a", 1)
        sleep(.2)
        file_sink.open(data_file_2)
        sleep(.2)
        file_sink.set_global_meta("test:b", 2)
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
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)

        injector = simple_tag_injector()
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
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)
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
        file_sink = sigmf.sink("cf32_le",
                               "")
        file_sink.set_global_meta("core:sample_rate", samp_rate)
        file_sink.set_global_meta("core:description", description)
        file_sink.set_global_meta("core:author", author)
        file_sink.set_global_meta("core:sample_rate", author)
        file_sink.set_global_meta("core:license", file_license)
        file_sink.set_global_meta("core:hw", hardware)

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
        file_sink = sigmf.sink("cf32_le",
                               "")
        file_sink.set_global_meta("core:sample_rate", samp_rate)
        file_sink.set_global_meta("core:description", description)
        file_sink.set_global_meta("core:author", author)
        file_sink.set_global_meta("core:sample_rate", author)
        file_sink.set_global_meta("core:license", file_license)
        file_sink.set_global_meta("core:hw", hardware)

        injector = simple_tag_injector()
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
            # Check annotations meta, should be empty, since annotations are
            # meant for specific samples and shouldn't be saved
            assert len(meta["annotations"]) == 0

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
        file_sink = sigmf.sink("cf32_le", "")
        file_sink.set_global_meta("core:sample_rate", samp_rate)
        file_sink.set_global_meta("core:description", description)
        file_sink.set_global_meta("core:author", author)
        file_sink.set_global_meta("core:sample_rate", author)
        file_sink.set_global_meta("core:license", file_license)
        file_sink.set_global_meta("core:hw", hardware)

        injector = simple_tag_injector()
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
        file_sink = sigmf.sink("cf32_le", "")
        file_sink.set_global_meta("core:sample_rate", samp_rate)
        file_sink.set_global_meta("core:description", description)
        file_sink.set_global_meta("core:author", author)
        file_sink.set_global_meta("core:sample_rate", author)
        file_sink.set_global_meta("core:license", file_license)
        file_sink.set_global_meta("core:hw", hardware)

        injector = simple_tag_injector()
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

    def test_ensure_empty_string_global_meta_setting(self):
        '''Ensure empty strings get propagated'''
        N = 1000
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        description = ""
        author = ""
        file_license = ""
        hardware = ""
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le", data_file)

        file_sink.set_global_meta("core:sample_rate", samp_rate)
        file_sink.set_global_meta("core:description", description)
        file_sink.set_global_meta("core:author", author)
        file_sink.set_global_meta("core:sample_rate", author)
        file_sink.set_global_meta("core:license", file_license)
        file_sink.set_global_meta("core:hw", hardware)

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
            assert meta["global"]["core:description"] == ""
            assert meta["global"]["core:author"] == ""
            assert meta["global"]["core:license"] == ""
            assert meta["global"]["core:hw"] == ""

    def test_capture_datetime_on_start(self):
        '''If a file is open to start then it should have a datetime
        set and that datetime should be sort of accurate'''
        N = 1000
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le", data_file)

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
            meta_dt = parse_iso_ts(meta_dt_str)
            print(meta_dt)
            assert (datetime.utcnow() - meta_dt).total_seconds() < 2

    def test_rate_tags_to_global(self):
        '''Test to ensure that rate tags go to the global segment
        and not to the capture segment'''
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()

        file_sink = sigmf.sink("cf32_le", data_file)

        # Set a value that will get overridden by the tags
        file_sink.set_global_meta("core:sample_rate", 20)

        injector = simple_tag_injector()
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()

        samp_rate = 1000.5
        injector.inject_tag = {"rx_rate": samp_rate}
        sleep(.2)
        tb.stop()
        tb.wait()
        meta = json.load(open(json_file, "r"))
        # Samp rate should have been set by the tag
        assert meta["global"]["core:sample_rate"] == samp_rate
        # And should not be in the captures segment
        assert "core:sample_rate" not in meta["captures"][0]

    def test_set_capture_meta_via_message(self):
        '''Test that when we send a message to set some metadata
        it gets set correctly'''
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()

        file_sink = sigmf.sink("cf32_le",
                               data_file)

        sender = msg_sender()
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.msg_connect(sender, "out", file_sink, "command")
        tb.start()
        sleep(.1)
        sender.send_msg({
            "command": "set_capture_meta",
            "index": 0,
            "key": "test:a",
            "val": 84
        })
        sleep(.2)
        tb.stop()
        tb.wait()

        with open(json_file, "r") as f:
            meta = json.load(f)
            print(meta)
            assert meta["captures"][0]["test:a"] == 84

    def test_bad_types_set_global(self):
        '''Make sure that test_global_meta with a non allowed type throws
        an error'''
        data_file, json_file = self.temp_file_names()

        file_sink = sigmf.sink("cf32_le",
                               data_file)
        exception_hit = False
        try:
            file_sink.set_global_meta("core:sample_rate", [234, 2342, 234])
        except:
            exception_hit = True

        assert exception_hit

    def test_rx_time_conversion(self):
        '''Test that rx_time tags are correctly converted to iso8601 strings'''

        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()

        file_sink = sigmf.sink("cf32_le",
                               data_file)
        seconds = 1520551983
        frac_seconds = 0.09375
        frac_seconds_2 = 0.25
        correct_str_1 = datetime.utcfromtimestamp(
            seconds).strftime('%Y-%m-%dT%H:%M:%S')
        correct_str_1 += str(frac_seconds).lstrip('0') + "Z"
        correct_str_2 = datetime.utcfromtimestamp(
            seconds).strftime('%Y-%m-%dT%H:%M:%S')
        correct_str_2 += str(frac_seconds_2).lstrip('0') + "Z"
        injector = simple_tag_injector()
        # first sample should have a rx_time tag
        injector.inject_tag = {"rx_time": (seconds, frac_seconds)}
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()
        sleep(.2)
        # Also test the case where a tag arives while writing
        injector.inject_tag = {"rx_time": (seconds, frac_seconds_2)}
        sleep(.1)
        tb.stop()
        tb.wait()

        with open(json_file, "r") as f:
            meta = json.load(f)
            assert meta["captures"][0]["core:datetime"] == correct_str_1
            assert meta["captures"][1]["core:datetime"] == correct_str_2

    def test_rx_time_update_when_file_not_open(self):
        '''Test that rx_time tags received before recording starts
        get offset correctly for the datetime on the first captures segment'''

        def run_iteration(wait_full, wait_frac):
            limit_event = Event()
            continue_event = Event()
            samp_rate = 10000.0
            limit_samples = (samp_rate * wait_full) + (samp_rate * wait_frac)
            print(limit_samples)
            src = sample_producer(limit_samples, limit_event, continue_event)

            data_file, json_file = self.temp_file_names()
            file_sink = sigmf.sink("cf32_le", "")
            file_sink.set_global_meta("core:sample_rate", samp_rate)

            seconds = 1520551983
            frac_seconds = 0.09375
            end_seconds = seconds + wait_full
            end_frac = frac_seconds + wait_frac
            if (end_frac > 1):
                end_seconds += 1
                end_frac -= 1
            correct_str = datetime.utcfromtimestamp(
                end_seconds).strftime('%Y-%m-%dT%H:%M:%S')
            correct_str += str(end_frac).lstrip('0') + "Z"
            injector = simple_tag_injector()
            # first sample should have a rx_time tag
            injector.inject_tag = {"rx_time": (seconds, frac_seconds)}
            tb = gr.top_block()
            tb.connect(src, injector)
            tb.connect(injector, file_sink)
            tb.start()
            print("waiting")
            limit_event.wait()
            # sleep to let the last samples get to the sink block
            sleep(.1)
            file_sink.open(data_file)
            continue_event.set()
            sleep(.1)
            tb.stop()
            tb.wait()

            with open(json_file, "r") as f:
                meta = json.load(f)
                print(meta)
                assert meta["captures"][0]["core:datetime"] == correct_str

        # just one second of added time
        run_iteration(1, 0)
        # Just a fractional amount
        run_iteration(0, (2 / 32.0))
        # fractional parts overlap
        run_iteration(1, (30 / 32.0))

    def test_relative_time_mode(self):
        # Example of Relative Mode Opertation
        # The following events happen:

        # Sample 0: rx_time: (2, 0.50000) at host time
        # of 2018-03-12T11:36:00.20000
        # 10,000 samples follow
        # Sample 10,000: rx_time: (4, 0.80000)
        # 20,000 samples follow
        # Note that the relative time difference between the two
        # capture segments is precisely 2.3 seconds.
        # This should create two capture segments:

        # Capture Segment 1 core:datetime: 2018-03-12T11:36:00.20000

        # Capture Segment 2 core:datetime: 2018-03-12T11:36:02.50000
        limit_event = Event()
        continue_event = Event()
        samp_rate = 10000.0
        limit_samples = samp_rate
        print(limit_samples)
        src = sample_producer(limit_samples, limit_event, continue_event)

        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le", data_file,
                               sigmf.sigmf_time_mode_relative)
        file_sink.set_global_meta("core:sample_rate", samp_rate)

        injector = simple_tag_injector()
        # first sample should have a rx_time tag
        injector.inject_tag = {"rx_time": (2, 0.500000)}
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()
        print("waiting")
        limit_event.wait()
        # sleep to let the last samples get to the sink block
        sleep(.1)
        # set the rx_time tag for the next section
        injector.inject_tag = {"rx_time": (4, 0.80000)}
        continue_event.set()
        sleep(.1)
        tb.stop()
        tb.wait()

        with open(json_file, "r") as f:
            meta = json.load(f)
            capture_one_dt = parse_iso_ts(meta["captures"][0]["core:datetime"])
            capture_two_dt = parse_iso_ts(meta["captures"][1]["core:datetime"])
            diff_time = capture_two_dt - capture_one_dt
            assert diff_time.seconds == 2
            assert diff_time.microseconds == 300000

    def test_relative_time_mode_initial_closed(self):
        '''Test relative time mode when the sink is initially not recording'''
        # limit_event = Event()
        # continue_event = Event()

        samp_rate = 100e6
        limit_samples = samp_rate
        print(limit_samples)
        # src = sample_producer(limit_samples, limit_event, continue_event)
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))

        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le", "",
                               sigmf.sigmf_time_mode_relative)
        file_sink.set_global_meta("core:sample_rate", samp_rate)

        injector = simple_tag_injector()
        # first sample should have a rx_time tag
        injector.inject_tag = {"rx_time": (65000, 0.500000)}
        tb = gr.top_block()
        tb.connect(src, injector)
        tb.connect(injector, file_sink)
        tb.start()
        # sleep to let some samples get to the sink block
        sleep(.1)
        # set the rx_time tag for the next section
        file_sink.open(data_file)
        # Let some stuff get recorded
        sleep(.1)
        tb.stop()
        tb.wait()

        with open(json_file, "r") as f:
            meta = json.load(f)
            capture_one_dt = parse_iso_ts(meta["captures"][0]["core:datetime"])
            now = datetime.utcnow()
            print(capture_one_dt)
            print(now)
            self.assertEqual(now.year, capture_one_dt.year,
                             "Bad year in first capture segment")
            self.assertEqual(now.month, capture_one_dt.month,
                             "Bad month in first capture segment")
            self.assertEqual(now.day, capture_one_dt.day,
                             "Bad day in first capture segment")
            # capture_two_dt =
            # parse_iso_ts(meta["captures"][1]["core:datetime"])
            # diff_time = capture_two_dt - capture_one_dt
            # assert diff_time.seconds == 2
            # assert diff_time.microseconds == 300000

    def test_endianness_checking(self):
        '''Check that the sink properly converts and errors on
        endianness values in the type argument'''

        if sys.byteorder == "little":
            ending = "_le"
            bad_ending = "_be"
        else:
            ending = "_be"
            bad_ending = "_le"

        def run_check(dtype):
            N = 1000
            samp_rate = 200000
            data_file, json_file = self.temp_file_names()
            data = sig_source_c(samp_rate, 1000, 1, N)
            src = blocks.vector_source_c(data)
            file_sink = sigmf.sink(dtype, data_file)
            tb = gr.top_block()
            tb.connect(src, file_sink)
            tb.run()
            tb.wait()
            with open(json_file, "r") as f:
                meta = json.load(f)
                assert meta["global"]["core:datatype"] == ("cf32" + ending)
        # Try without ending
        run_check("cf32")
        # Try with right ending
        run_check("cf32" + ending)
        # Try with bad ending
        exception_msg = ""
        try:
            file_sink = sigmf.sink("cf32" + bad_ending, "")  # noqa: F841
        except Exception as e:
            exception_msg = str(e)
        assert "endianness" in exception_msg

    def test_inconsistent_data_on_kill(self):
        '''Check that if the sink is killed before it writes out the
        final metadata file, then the data file should have a name like
        '.temp-uuid-<original file name>.sigmf-data'
        '''

        data_file, json_file = self.temp_file_names()

        def process_func():
            src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
            file_sink = sigmf.sink("cf32_le",
                                   data_file)

            tb = gr.top_block()
            tb.connect(src, file_sink)
            tb.start()
            tb.wait()

        p = Process(target=process_func)
        p.start()
        sleep(.2)
        p.terminate()
        try:
            p.join(1)
        except Exception:
            self.fail("Joining subprocess failed")

        # Neither file should exist
        self.assertFalse(os.path.exists(json_file),
                         "metadata file found, but should not be there")
        self.assertFalse(os.path.exists(data_file),
                         "Data file found with wrong name")
        # But there should be a temp data file
        files_dir = os.path.dirname(data_file)
        data_files = [f for f in os.listdir(
            files_dir) if f.endswith(".sigmf-data")]
        self.assertEqual(len(data_files), 1, "More than one data file found!")
        data_temp_name = os.path.basename(data_files[0])
        self.assertTrue(re.match(
            r"\.temp-((\d|\w){16})-(.+)\.sigmf-data",
            data_temp_name) is not None,
            "Bad temp data name")

    def test_gps_annotation(self):
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)

        sender = msg_sender()
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.msg_connect(sender, "out", file_sink, "gps")

        coords = [
            (12.345, -67.89),
            (55.555, -110.111),
            (33.123, 33.123),
        ]

        tb.start()
        for lat, lon in coords:
            sender.send_msg({
                "latitude": lat,
                "longitude": lon,
            })
            sleep(.05)
        tb.stop()
        tb.wait()
        metadata = json.load(open(json_file, "r"))
        # should be 3 annotations
        self.assertEqual(len(metadata["annotations"]), len(coords))
        # And they should be these and in this order
        for ii, point in enumerate(coords):
            lat, lon = point
            annotation = metadata["annotations"][ii]
            self.assertEqual(annotation["core:latitude"], lat)
            self.assertEqual(annotation["core:longitude"], lon)
            self.assertIn("GPS", annotation["core:generator"])

    def test_close_via_pmt(self):
        '''Ensure that a close sent via pmt is handled even if the work
        function isn't ever called again'''
        data_file, json_file = self.temp_file_names()

        class sample_eater(gr.sync_block):
            def __init__(self):
                gr.sync_block.__init__(
                    self,
                    name="sample_counter",
                    in_sig=[numpy.complex64],
                    out_sig=[numpy.complex64],
                )
                self.eat_samples = False

            def work(self, input_items, output_items):
                if self.eat_samples:
                    return 0
                else:
                    output_items[0][:] = input_items[0]
                    return len(output_items[0])

        def process_func(death_queue):
            src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
            file_sink = sigmf.sink("cf32_le",
                                   "")
            sender = msg_sender()
            eater = sample_eater()
            tb = gr.top_block()
            tb.connect(src, eater)
            tb.msg_connect(sender, "out", file_sink, "command")
            tb.connect(eater, file_sink)
            tb.start()
            sleep(.05)
            sender.send_msg({
                "command": "open",
                "filename": data_file
            })
            # ensure it gets set
            while True:
                if file_sink.get_data_path() == "":
                    sleep(.05)
                else:
                    break
            # record some data
            sleep(.05)
            # eat all the samples so the work function
            # never gets called again
            eater.eat_samples = True
            # wait a bit
            sleep(.05)
            # send close, this should close and write the file in
            # the pmt handler
            sender.send_msg({
                "command": "close"
            })
            # signal to outside that this can be killed
            death_queue.put("KILL")
            tb.wait()

        kill_queue = Queue()
        p = Process(target=process_func, args=(kill_queue,))
        p.start()
        # wait for something to be put in the kill queue
        try:
            kill_queue.get(True, 2)
        except Queue.empty:
            self.fail("Subprocess never signalled for death")
        # kill the process
        p.terminate()
        try:
            p.join(1)
        except Exception:
            self.fail("Joining subprocess failed")

        # both files should exist
        self.assertTrue(os.path.exists(json_file),
                        "metadata file not found")
        self.assertTrue(os.path.exists(data_file),
                        "Data file not found")
        self.assertGreater(os.path.getsize(data_file),
                           0, "No data in data file")

    def test_set_capture_meta_before_start(self):
        """Test that if set_capture_meta is called before flowgraph start
        that it is handled correctly and not discarded"""
        N = 1000
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)
        file_sink.set_capture_meta(0, "test:foo", pmt.to_pmt("bar"))

        # build flowgraph here
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.run()
        tb.wait()
        # check that the metadata matches up
        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)

            self.assertEqual(meta["captures"][0]["test:foo"],
                             "bar", "Pre start capture segment data discarded")

    def test_bad_filename(self):
        """Test that if we get a bad filename, then we should get a runtime
        error"""
        dirname = uuid.uuid4().hex
        filename = uuid.uuid4().hex
        # Make a data file with a weird name that doesn't exist
        data_file = os.path.join("/tmp", dirname, filename)
        # Try to instantiate the sink, this should error
        with self.assertRaises(exceptions.RuntimeError):
            sigmf.sink("cf32_le",
                       data_file)

    def test_exception_from_open_via_message(self):
        """Test that if open is called via a message,
        then an exception is thrown"""
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        file_sink = sigmf.sink("cf32_le",
                               "")
        dirname = uuid.uuid4().hex
        filename = uuid.uuid4().hex
        # Make a data file with a weird name that doesn't exist
        data_file = os.path.join("/tmp", dirname, filename)

        sender = msg_sender()
        # eater = sample_eater()
        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.msg_connect(sender, "out", file_sink, "command")

        tb.start()
        sleep(.05)
        # This should result in an exception beting thrown in the block
        sender.send_msg({
            "command": "open",
            "filename": data_file
        })
        # This is a bit of a hack, once there is better exception handling
        # behavior in gnuradio, this test will need to be updated. As is, we
        # can detect that a block has stopped running by checking that it's not
        # reading new items
        sleep(.05)
        items = file_sink.nitems_read(0)
        sleep(.05)
        items2 = file_sink.nitems_read(0)
        diff_items = items2 - items
        self.assertEqual(
            diff_items, 0, "Block didn't die from invalid open message!")
        tb.stop()
        tb.wait()


if __name__ == '__main__':
    gr_unittest.run(qa_sink, "qa_sink.xml")
