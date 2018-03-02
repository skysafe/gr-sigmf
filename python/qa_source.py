from gnuradio import gr, gr_unittest
from gnuradio import blocks
import tempfile
import numpy
import shutil
import json
import pmt
import os
import math
from time import sleep
from threading import Timer

from sigmf import sigmf_swig as sigmf


def sig_source_c(samp_rate, freq, amp, N):
    t = map(lambda x: float(x) / samp_rate, xrange(N))
    y = map(lambda x: amp * math.cos(2. * math.pi * freq * x) +
            1j * amp * math.sin(2. * math.pi * freq * x), t)
    return y


class message_generator(gr.basic_block):
    def __init__(self, message):
        gr.basic_block.__init__(self, name="message_generator",
                                in_sig=None, out_sig=None)
        self.message = message
        self.message_port_register_out(pmt.intern('messages'))
        self.timer = None

    def send_msg(self):
        msg = self.message
        if not isinstance(msg, pmt.pmt_swig.swig_int_ptr):
            msg = pmt.to_pmt(msg)
        self.message_port_pub(pmt.intern('messages'), msg)

    def stop(self):
        if self.timer:
            self.timer.cancel()
            self.timer.join()
        return True

    def start(self):
        self.timer = Timer(1, self.send_msg)
        self.timer.start()
        return True


# Block that just collects all tags that pass thorough it
class tag_collector(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="tag_collector",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.tags = []

    def work(self, input_items, output_items):
        tags = self.get_tags_in_window(0, 0, len(input_items[0]))
        if (tags):
            for tag in tags:
                self.tags.append({
                    "key": pmt.to_python(tag.key),
                    "offset": tag.offset,
                    "value": pmt.to_python(tag.value)
                })
        output_items[0][:] = input_items[0]
        return len(output_items[0])

    def assertTagExists(self, offset, key, val):
        assert len([t for t in self.tags if t["offset"] ==
                    offset and t["key"] == key and t["value"] == val]) == 1


class qa_source (gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def make_file(self, filename, N=1000, type="cf32_le"):
        if (not filename.startswith("/")):
            filename = os.path.join(self.test_dir, filename)
        samp_rate = 200000

        data = sig_source_c(samp_rate, 1000, 1, N)
        src = blocks.vector_source_c(data)

        description = "This is a description"
        author = "John Smith"
        file_license = "CC-0"
        hardware = "Vector Source"
        file_sink = sigmf.sink(type,
                               filename,
                               samp_rate,
                               description,
                               author,
                               file_license,
                               hardware,
                               False)
        data_path = file_sink.get_data_path()
        meta_path = file_sink.get_meta_path()

        tb = gr.top_block()
        tb.connect(src, file_sink)
        tb.run()
        with open(meta_path, "r") as f:
            meta_json = json.load(f)
        return data, meta_json, data_path, meta_path

    def test_normal_run(self):

        # generate a file
        data, meta_json, filename, meta_file = self.make_file("normal")

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le", debug=False)
        sink = blocks.vector_sink_c()
        tb = gr.top_block()
        tb.connect(file_source, sink)
        tb.run()

        # check that the data matches
        written_data = sink.data()
        self.assertComplexTuplesAlmostEqual(data, written_data)

        # check that the meta matches
        # TODO: implement this check

    def test_small_file(self):

        # test on a very small file
        data, meta_json, filename, meta_file = self.make_file(
            "small_files", N=1)

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le", debug=False)
        sink = blocks.vector_sink_c()
        tb = gr.top_block()
        tb.connect(file_source, sink)
        tb.run()

        # check that the data matches
        written_data = sink.data()
        self.assertComplexTuplesAlmostEqual(data, written_data)

    def test_repeat(self):

        # Test to ensure that repeat works correctly
        # TODO: This test should check the tags if set
        data, meta_json, filename, meta_file = self.make_file("repeat")
        file_source = sigmf.source(filename, "cf32_le", repeat=True,
                                   debug=False)
        sink = blocks.vector_sink_c()
        tb = gr.top_block()
        tb.connect(file_source, sink)
        tb.start()

        # sleep for a very short amount of time to allow for some repeats
        # to happen
        sleep(.005)
        tb.stop()
        tb.wait()
        data_len = len(data)
        written_data = sink.data()
        written_len = len(written_data)
        num_reps = written_len / data_len
        assert num_reps > 1
        for rep in range(num_reps):
            self.assertComplexTuplesAlmostEqual(
                data, written_data[rep * data_len: (rep + 1) * data_len])
        last_partial = written_data[num_reps * data_len:]
        partial_data = data[:len(last_partial)]
        self.assertComplexTuplesAlmostEqual(last_partial, partial_data)

    def test_bad_metafile(self):
        data, meta_json, filename, meta_file = self.make_file("bad_meta")
        with open(meta_file, "r+") as f:
            f.seek(0)

            # corrupt the first byte
            f.write("A")

        try:
            sigmf.source(filename, "cf32_le", debug=False)
        except:

            # TODO: I should probably throw a better exception here...
            assert True
            return

        assert False

    def test_nonexistant_files(self):
        data, meta_json, filename, meta_file = self.make_file("no_meta")
        os.remove(meta_file)
        caught_exception = False
        try:
            sigmf.source(filename, "cf32_le", debug=False)
        except:
            caught_exception = True
        assert caught_exception

        data, meta_json, filename, meta_file = self.make_file("no_data")
        os.remove(filename)
        caught_exception = False
        try:
            sigmf.source(filename, debug=False)
        except:
            caught_exception = True
        assert caught_exception

    def test_begin_tags(self):
        data, meta_json, filename, meta_file = self.make_file("begin")

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le", debug=False)
        begin_tag = pmt.to_pmt("TEST")
        file_source.set_begin_tag(begin_tag)
        sink = blocks.vector_sink_c()
        collector = tag_collector()
        tb = gr.top_block()
        tb.connect(file_source, collector)
        tb.connect(collector, sink)
        tb.run()

        zero_offset_tags = [t for t in collector.tags if t["offset"] == 0]
        test_tag = [t for t in zero_offset_tags if t["key"] == "TEST"]
        self.assertEqual(len(test_tag), 1)

    def test_capture_segments_to_tags(self):
        data, meta_json, filename, meta_file = self.make_file("capture_segs")

        # Add some capture segments
        with open(meta_file, "r+") as f:
            data = json.load(f)
            data['captures'].append({
                "core:sample_start": 5,
                "core:frequency": 2.4e9,

            })
            data['captures'].append({
                "core:sample_start": 10,
                "core:frequency": 2.44e9,
            })
            f.seek(0)
            json.dump(data, f, indent=4)
            f.truncate()

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le", debug=False)
        begin_tag = pmt.to_pmt("TEST")
        file_source.set_begin_tag(begin_tag)
        sink = blocks.vector_sink_c()
        collector = tag_collector()
        tb = gr.top_block()
        tb.connect(file_source, collector)
        tb.connect(collector, sink)
        tb.run()

        # There should be 3 tags
        print(collector.tags)
        zero_offset_tags = [t for t in collector.tags if t["offset"] == 0]
        test_tag = [t for t in zero_offset_tags if t["key"] == "TEST"]
        self.assertEqual(len(test_tag), 1)
        collector.assertTagExists(5, "core:frequency", 2.4e9)
        collector.assertTagExists(10, "core:frequency", 2.44e9)

    def test_annotations_to_tags(self):
        data, meta_json, filename, meta_file = self.make_file(
            "annotation_tags")

        # Add some annotations
        with open(meta_file, "r+") as f:
            data = json.load(f)
            data['annotations'].append({
                "core:sample_start": 5,
                "test:string": "This is some string data",
                "test:more_data": True,
            })
            data['annotations'].append({
                "core:sample_start": 10,
                "test:rating": 12,
            })

            # Write over f with a version with the new annotations
            f.seek(0)
            json.dump(data, f, indent=4)
            f.truncate()

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le", debug=False)
        sink = blocks.vector_sink_c()
        collector = tag_collector()
        tb = gr.top_block()
        tb.connect(file_source, collector)
        tb.connect(collector, sink)
        tb.run()

        collector.assertTagExists(5, "test:string", "This is some string data")
        collector.assertTagExists(5, "test:more_data", True)
        collector.assertTagExists(10, "test:rating", 12)

    def test_command_message(self):
        data, meta_json, filename, meta_file = self.make_file("begin")

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le", repeat=True,
                                   debug=False)
        msg = {"command": "set_begin_tag", "tag": "test"}
        generator = message_generator(msg)
        sink = blocks.vector_sink_c()
        collector = tag_collector()

        tb = gr.top_block()
        tb.msg_connect((generator, 'messages'), (file_source, 'command'))
        tb.connect(file_source, collector)
        tb.connect(collector, sink)
        tb.start()
        sleep(1)
        tb.stop()
        tb.wait()

        for tag in collector.tags:
            if tag["key"] != "core:datetime":
                self.assertEqual(tag["key"], "test")
