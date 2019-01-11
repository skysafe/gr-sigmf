from gnuradio import gr, gr_unittest, blocks, analog
import tempfile
import shutil
import os
from gr_sigmf import gr_sigmf_swig as sigmf
import numpy as np
import struct


class qa_type_converter(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()
        self.samp_rate = 32000

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def make_file(self, filename, type, amplitude):

        filename = os.path.join(self.test_dir, filename)

        tb = gr.top_block()

        if type == "rf32":
            head = blocks.head(gr.sizeof_float * 1, self.samp_rate)
            source = analog.sig_source_f(self.samp_rate, analog.GR_COS_WAVE,
                                         1000, amplitude, 0)
            tb.connect(source, head)
        elif type == "ri32":
            head = blocks.head(gr.sizeof_int * 1, self.samp_rate)
            source = analog.sig_source_i(self.samp_rate, analog.GR_COS_WAVE,
                                         1000, amplitude, 0)
            tb.connect(source, head)
        elif type == "ri16":
            head = blocks.head(gr.sizeof_short * 1, self.samp_rate)
            source = analog.sig_source_s(self.samp_rate, analog.GR_COS_WAVE,
                                         1000, amplitude, 0)
            tb.connect(source, head)
        elif type == "ri8":
            head = blocks.head(gr.sizeof_char * 1, self.samp_rate)
            source = analog.sig_source_f(self.samp_rate, analog.GR_COS_WAVE,
                                         1000, amplitude, 0)
            convert = blocks.float_to_char(1, 1)
            tb.connect(source, convert)
            tb.connect(convert, head)

        else:
            head = blocks.head(gr.sizeof_gr_complex * 1, self.samp_rate)
            source = analog.sig_source_c(self.samp_rate, analog.GR_COS_WAVE,
                                         1000, amplitude, 0)
            tb.connect(source, head)

        sigmf_sink = sigmf.sink(type, filename)

        tb.connect(head, sigmf_sink)
        tb.run()
        tb.wait()

        return filename

    def test_rf32_to_ri32(self):

        path = self.make_file("test_source", "rf32", 1)

        # expected
        expected_source = sigmf.source(path, "rf32", False)
        convert = blocks.float_to_int(1, 1)
        expected_sink = blocks.vector_sink_i(1)

        # actual
        actual_source = sigmf.source(path, "ri32", False)
        actual_sink = blocks.vector_sink_i(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_rf32_to_ri16(self):

        path = self.make_file("test_source", "rf32", 1)

        # expected
        expected_source = sigmf.source(path, "rf32", False)
        convert = blocks.float_to_short(1, 1)
        expected_sink = blocks.vector_sink_s(1)

        # actual
        actual_source = sigmf.source(path, "ri16", False)
        actual_sink = blocks.vector_sink_s(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_rf32_to_ri8(self):

        path = self.make_file("test_source", "rf32", 1)

        # expected
        expected_source = sigmf.source(path, "rf32", False)
        convert1 = blocks.float_to_short(1, 1)
        expected_sink = blocks.vector_sink_s(1)

        # actual
        actual_source = sigmf.source(path, "ri8", False)
        convert2 = blocks.char_to_float(1, 1)
        actual_sink = blocks.vector_sink_f(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert1)
        tb1.connect(convert1, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, convert2)
        tb2.connect(convert2, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()

        np.testing.assert_almost_equal(e, a)

    def test_ri32_to_rf32(self):

        path = self.make_file("test_source", "ri32", 1)

        # expected
        expected_source = sigmf.source(path, "ri32", False)
        convert = blocks.int_to_float(1, 2147483647)
        expected_sink = blocks.vector_sink_f(1)

        # actual
        actual_source = sigmf.source(path, "rf32", False)
        actual_sink = blocks.vector_sink_f(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri32_to_ri16(self):

        path = self.make_file("test_source", "ri32", 1)

        # expected
        expected_source = sigmf.source(path, "ri32", False)
        expected_sink = blocks.vector_sink_i(1)

        # actual
        actual_source = sigmf.source(path, "ri16", False)
        actual_sink = blocks.vector_sink_s(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri32_to_ri8(self):

        path = self.make_file("test_source", "ri32", 1)

        # expected
        expected_source = sigmf.source(path, "ri32", False)
        expected_sink = blocks.vector_sink_i(1)

        # actual
        actual_source = sigmf.source(path, "ri8", False)
        convert1 = blocks.char_to_float(1, 1)
        convert2 = blocks.float_to_int(1, 1)
        actual_sink = blocks.vector_sink_i(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, convert1)
        tb2.connect(convert1, convert2)
        tb2.connect(convert2, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri16_to_rf32(self):

        path = self.make_file("test_source", "ri16", 1)

        # expected
        expected_source = sigmf.source(path, "ri16", False)
        convert = blocks.short_to_float(1, 32767)
        expected_sink = blocks.vector_sink_f(1)

        # actual
        actual_source = sigmf.source(path, "rf32", False)
        actual_sink = blocks.vector_sink_f(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri16_to_ri32(self):

        path = self.make_file("test_source", "ri16", 1)

        # expected
        expected_source = sigmf.source(path, "ri16", False)
        expected_sink = blocks.vector_sink_s(1)

        # actual
        actual_source = sigmf.source(path, "ri32", False)
        actual_sink = blocks.vector_sink_i(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri16_to_ri8(self):

        path = self.make_file("test_source", "ri16", 1)

        # expected
        expected_source = sigmf.source(path, "ri16", False)
        convert = blocks.short_to_char(1)
        expected_sink = blocks.vector_sink_b(1)

        # actual
        actual_source = sigmf.source(path, "ri8", False)
        actual_sink = blocks.vector_sink_b(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri8_to_rf32(self):

        path = self.make_file("test_source", "ri8", 1)

        # expected
        expected_source = sigmf.source(path, "ri8", False)
        convert = blocks.char_to_float(1, 127)
        expected_sink = blocks.vector_sink_f(1)

        # actual
        actual_source = sigmf.source(path, "rf32", False)
        actual_sink = blocks.vector_sink_f(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri8_to_ri32(self):

        path = self.make_file("test_source", "ri8", 1)

        # expected
        expected_source = sigmf.source(path, "ri8", False)
        convert1 = blocks.char_to_float(1, 1)
        convert2 = blocks.float_to_int(1, 1)
        expected_sink = blocks.vector_sink_i(1)

        # actual
        actual_source = sigmf.source(path, "ri32", False)
        actual_sink = blocks.vector_sink_i(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert1)
        tb1.connect(convert1, convert2)
        tb1.connect(convert2, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def test_ri8_to_ri16(self):

        path = self.make_file("test_source", "ri8", 1)

        # expected
        expected_source = sigmf.source(path, "ri8", False)
        convert = blocks.char_to_short(1)
        expected_sink = blocks.vector_sink_s(1)

        # actual
        actual_source = sigmf.source(path, "ri16", False)
        actual_sink = blocks.vector_sink_s(1)

        tb1 = gr.top_block()
        tb1.connect(expected_source, convert)
        tb1.connect(convert, expected_sink)
        tb1.run()
        tb1.wait()

        tb2 = gr.top_block()
        tb2.connect(actual_source, actual_sink)
        tb2.run()
        tb2.wait()

        e = expected_sink.data()
        a = actual_sink.data()
        np.testing.assert_almost_equal(e, a)

    def read_complex_data(self, path, dtype, size):
        data = []
        with open(path + ".sigmf-data", "r") as f:
            try:
                while True:
                    real = struct.unpack(dtype, f.read(size))[0]
                    imag = struct.unpack(dtype, f.read(size))[0]
                    data.append(real + (imag * 1j))
            except:
                pass
        return data

    def test_cf32_to_ci32(self):

        path = self.make_file("test_source", "cf32", 10)

        # actual
        filename = os.path.join(self.test_dir, "test_sink")
        actual_source = sigmf.source(path, "ci32", False)
        actual_sink = sigmf.sink("ci32", filename)

        tb = gr.top_block()
        tb.connect(actual_source, actual_sink)
        tb.run()
        tb.wait()

        e = self.read_complex_data(path, 'f', 4)
        a = self.read_complex_data(filename, 'i', 4)
        np.testing.assert_almost_equal(e, a, decimal=0)

    def test_cf32_to_ci16(self):

        path = self.make_file("test_source", "cf32", 10)

        # actual
        filename = os.path.join(self.test_dir, "test_sink")
        actual_source = sigmf.source(path, "ci16", False)
        actual_sink = sigmf.sink("ci16", filename)

        tb = gr.top_block()
        tb.connect(actual_source, actual_sink)
        tb.run()
        tb.wait()

        e = self.read_complex_data(path, 'f', 4)
        a = self.read_complex_data(filename, 'h', 2)
        np.testing.assert_almost_equal(e, a, decimal=0)

    def test_cf32_to_ci8(self):

        path = self.make_file("test_source", "cf32", 10)

        # actual
        filename = os.path.join(self.test_dir, "test_sink")
        actual_source = sigmf.source(path, "ci8", False)
        actual_sink = sigmf.sink("ci8", filename)

        tb = gr.top_block()
        tb.connect(actual_source, actual_sink)
        tb.run()
        tb.wait()

        e = self.read_complex_data(path, 'f', 4)
        a = self.read_complex_data(filename, 'b', 1)
        np.testing.assert_almost_equal(e, a, decimal=0)


if __name__ == '__main__':
    gr_unittest.run(qa_type_converter, "qa_type_converter.xml")
