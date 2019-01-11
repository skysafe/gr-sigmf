import json
import uuid
import tempfile
import os
import shutil
import math
from gnuradio import gr_unittest, gr, analog, blocks
from gr_sigmf import gr_sigmf_swig as sigmf
from test_blocks import (advanced_tag_injector,
                         tag_collector)


def sig_source_c(samp_rate, freq, amp, N):
    t = map(lambda x: float(x) / samp_rate, xrange(N))
    y = map(lambda x: amp * math.cos(2. * math.pi * freq * x) +
            1j * amp * math.sin(2. * math.pi * freq * x), t)
    return y


class qa_source_to_sink(gr_unittest.TestCase):

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

    def test_tag_roundtrip(self):
        # write some data with both capture and annotation data
        freq = 2.4e9
        samp_rate = 100e6
        test_index = 1000
        time = tuple([1222277384, .0625])
        test_a = 22.3125
        test_b = "asdf"
        test_c = True
        test_index_2 = 2000
        test_d = 18.125
        test_e = "jkl;"
        test_f = False
        injector = advanced_tag_injector([
            (0, {"rx_time": time}),
            (0, {"rx_freq": freq}),
            (0, {"rx_rate": samp_rate}),
            (test_index, {"test:a": test_a,
                          "test:b": test_b, "test:c": test_c}),
            (test_index_2, {"test_d": test_d,
                            "test_e": test_e, "test_f": test_f})
        ])
        src = analog.sig_source_c(0, analog.GR_CONST_WAVE, 0, 0, (1 + 1j))
        num_samps = int(1e6)
        head = blocks.head(gr.sizeof_gr_complex, num_samps)
        data_file, json_file = self.temp_file_names()
        file_sink = sigmf.sink("cf32_le",
                               data_file)

        tb = gr.top_block()
        tb.connect(src, head)
        tb.connect(head, injector)
        tb.connect(injector, file_sink)
        tb.start()
        tb.wait()
        # Make sure the data file got written
        self.assertTrue(os.path.exists(data_file), "Data file missing")
        self.assertEqual(os.path.getsize(
            data_file), gr.sizeof_gr_complex * num_samps,
            "Data file incomplete")

        # Ensure that the data exists as we think it should
        with open(json_file, "r") as f:
            meta_str = f.read()
            meta = json.loads(meta_str)
            print(meta)
            self.assertEqual(
                meta["captures"][0]["core:frequency"],
                freq, "Bad metadata, frequency")
            # Should only be one capture segment
            self.assertEqual(len(meta["captures"]), 1)
            self.assertEqual(meta["global"]["core:sample_rate"],
                             samp_rate, "Bad metadata, samp_rate")

            self.assertEqual(meta["annotations"][0]
                             ["test:a"], test_a, "bad test_a value")
            self.assertEqual(meta["annotations"][0]
                             ["test:b"], test_b, "bad test_b value")
            self.assertEqual(meta["annotations"][0]
                             ["test:c"], test_c, "bad test_c value")
            self.assertEqual(
                meta["annotations"][0]["core:sample_start"],
                test_index, "Bad test index")
            self.assertEqual(meta["annotations"][1]
                             ["unknown:test_d"], test_d, "bad test_d value")
            self.assertEqual(meta["annotations"][1]
                             ["unknown:test_e"], test_e, "bad test_e value")
            self.assertEqual(meta["annotations"][1]
                             ["unknown:test_f"], test_f, "bad test_f value")
            self.assertEqual(
                meta["annotations"][1]["core:sample_start"],
                test_index_2, "Bad test index")

        # Read out the data and check that it matches
        file_source = sigmf.source(data_file, "cf32_le")
        collector = tag_collector()
        sink = blocks.vector_sink_c()
        tb = gr.top_block()
        tb.connect(file_source, collector)
        tb.connect(collector, sink)
        tb.start()
        tb.wait()
        collector.assertTagExists(0, "rx_rate", samp_rate)
        collector.assertTagExists(0, "rx_time", time)
        collector.assertTagExists(0, "rx_freq", freq)
        collector.assertTagExists(test_index, "test:a", test_a)
        collector.assertTagExists(test_index, "test:b", test_b)
        collector.assertTagExists(test_index, "test:c", test_c)
        collector.assertTagExists(
            test_index_2, "test_d", test_d)
        collector.assertTagExists(
            test_index_2, "test_e", test_e)
        collector.assertTagExists(
            test_index_2, "test_f", test_f)

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

    def test_roundtrip_offset_initial_capture(self):
        """Test that if the first capture segment has an offset, then
        it gets correctly offset and output when roundtripped from a
        source to a sink"""

        # generate a file
        data, meta_json, filename, meta_file = self.make_file("offset")

        # drop the first 4 samples
        adjust_size = 4
        with open(meta_file, "r+") as f:
            fdata = json.load(f)
            fdata['captures'][0]["core:sample_start"] = adjust_size
            fdata['captures'][0]["core:frequency"] = 2.4e9
            f.seek(0)
            json.dump(fdata, f, indent=4)
            f.truncate()

        data_start_size = os.path.getsize(filename)

        out_data_file, out_json_file = self.temp_file_names()
        file_source = sigmf.source(filename, "cf32_le")
        file_sink = sigmf.sink("cf32_le", out_data_file)
        tagd = blocks.tag_debug(gr.sizeof_gr_complex, "test")
        tb = gr.top_block()
        tb.connect(file_source, file_sink)
        tb.connect(file_source, tagd)
        tb.start()
        tb.wait()

        data_end_size = os.path.getsize(out_data_file)
        # end data size should be smaller
        dropped_samples = adjust_size * 2 * 4
        self.assertEqual(data_start_size - dropped_samples,
                         data_end_size, "Wrong data size")

        with open(out_json_file, "r") as f:
            meta = json.load(f)
            print(meta)
            self.assertEqual(len(meta["annotations"]), 0,
                             "Shouldn't be any annotations in file")
            self.assertEqual(len(meta["captures"]), 1,
                             "Should only be 1 capture segment in file")
            self.assertEqual(
                meta["captures"][0]["core:frequency"],
                2.4e9, "frequency tag is missing")


if __name__ == '__main__':
    gr_unittest.run(qa_source_to_sink, "qa_source_to_sink.xml")
