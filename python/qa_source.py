from gnuradio import gr, gr_unittest
from gnuradio import blocks
import array
import tempfile
import shutil
import json
import pmt
import os
import math
from time import sleep
from test_blocks import message_generator, tag_collector

from sigmf import sigmf_swig as sigmf


def sig_source_c(samp_rate, freq, amp, N):
    t = map(lambda x: float(x) / samp_rate, xrange(N))
    y = map(lambda x: amp * math.cos(2. * math.pi * freq * x) +
            1j * amp * math.sin(2. * math.pi * freq * x), t)
    return y


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

        file_sink = sigmf.sink(type,
                               filename)
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
        file_source = sigmf.source(filename, "cf32_le")
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
        file_source = sigmf.source(filename, "cf32_le")
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
        file_source = sigmf.source(filename, "cf32_le", repeat=True)
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
            sigmf.source(filename, "cf32_le")
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
            sigmf.source(filename, "cf32_le")
        except:
            caught_exception = True
        assert caught_exception

        data, meta_json, filename, meta_file = self.make_file("no_data")
        os.remove(filename)
        caught_exception = False
        try:
            sigmf.source(filename)
        except:
            caught_exception = True
        assert caught_exception

    def test_begin_tags(self):
        data, meta_json, filename, meta_file = self.make_file("begin")

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le")
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
        file_source = sigmf.source(filename, "cf32_le")
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
        collector.assertTagExists(5, "rx_freq", 2.4e9)
        collector.assertTagExists(10, "rx_freq", 2.44e9)

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
        file_source = sigmf.source(filename, "cf32_le")
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
        file_source = sigmf.source(filename, "cf32_le", repeat=True)
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
            if tag["key"] not in ["rx_time", "core:length"]:
                self.assertEqual(tag["key"], "test")

    def test_first_capture_segment_non_zero_start(self):
        '''Test to check that if the first capture segment
        has a non-zero start index, then we should skip that part
        of the file'''

        filename_data = os.path.join(
            self.test_dir, "capture_not_zero.sigmf-data")
        filename_meta = os.path.join(
            self.test_dir, "capture_not_zero.sigmf-meta")
        skip_samples = 500
        normal_samples = 500
        a = array.array("f")
        for i in range(skip_samples * 2):
            a.append(1)
        for i in range(normal_samples * 2):
            a.append(2)
        with open(filename_data, "w") as f:
            a.tofile(f)
        metadata = {
            "global": {
                "core:datatype": "cf32_le",
                "core:version": "0.0.1"
            },
            "captures": [
                {
                    "core:sample_start": skip_samples
                }
            ]
        }
        with open(filename_meta, "w") as f:
            json.dump(metadata, f)

        file_source = sigmf.source(filename_data, "cf32_le",
                                   repeat=False)

        sink = blocks.vector_sink_c()
        tb = gr.top_block()
        tb.connect(file_source, sink)
        tb.start()
        tb.wait()
        written_data = sink.data()
        for i in range(len(written_data)):
            assert(written_data[i] == (2 + 2j))

    def test_json_types(self):

        # generate a file
        data, meta_json, filename, meta_file = self.make_file("json_types")

        # Add annotations with all types
        with open(meta_file, "r+") as f:
            data = json.load(f)
            data['annotations'].append({
                "core:sample_start": 1,
                "core:sample_count": 2,
                "test:int": -2,
                "test:int64": 278202993021,
                "test:uint": 2,
                "test:uint2": 2**32 + 2,
                "test:double": 2.2,
                "test:neg_double": -2.2,
                "test:bool1": True,
                "test:bool2": False,
                "test:null": None,
                "test:string": "foo",
            })
            f.seek(0)
            json.dump(data, f, indent=4)
            f.truncate()

        # run through the flowgraph
        file_source = sigmf.source(filename, "cf32_le")
        sink = blocks.vector_sink_c()
        collector = tag_collector()
        tb = gr.top_block()
        tb.connect(file_source, collector)
        tb.connect(collector, sink)
        tb.start()
        tb.wait()

        # Check that all the types got read correctly
        collector.assertTagExists(1, "test:int", -2)
        collector.assertTagExists(1, "test:int64", 278202993021)
        collector.assertTagExists(1, "test:uint", 2)
        collector.assertTagExists(1, "test:uint2", 2**32 + 2)
        collector.assertTagExists(1, "test:double", 2.2)
        collector.assertTagExists(1, "test:neg_double", -2.2)
        collector.assertTagExists(1, "test:bool1", True)
        collector.assertTagExists(1, "test:bool2", False)
        collector.assertTagExists(1, "test:null", None)
        collector.assertTagExists(1, "test:string", "foo")


if __name__ == '__main__':
    gr_unittest.run(qa_source, "qa_source.xml")
