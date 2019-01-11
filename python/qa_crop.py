import tempfile
from gnuradio import gr_unittest, blocks, gr
import json
import struct
import os
import shutil
from subprocess import PIPE, Popen
import numpy as np
from uuid import uuid4
from test_utils import sig_source_c

from gr_sigmf import gr_sigmf_swig as sigmf


class qa_crop(gr_unittest.TestCase):

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

    def run_crop(self, args, filename):
        cmd = ["sigmf-crop"]
        split_args = args.split()
        cmd.extend(split_args)
        out_file = os.path.join(self.test_dir, str(uuid4()) + ".sigmf-data")
        out_meta = os.path.splitext(out_file)[0] + ".sigmf-meta"
        cmd.append("-o")
        cmd.append(out_file)
        cmd.append(filename)
        p = Popen(cmd, stdout=PIPE, stderr=PIPE)
        std_out, std_err = p.communicate()
        if (p.returncode == 0):
            with open(out_file, "r") as f:
                raw_data = f.read()
            num_samps = len(raw_data) / 8
            s = struct.Struct("ff" * num_samps)
            out_tuple = s.unpack(raw_data)
            out_data = [complex(x, y)
                        for x, y in zip(out_tuple[0::2], out_tuple[1::2])]
            t = 0, std_out, std_err, out_file, out_data, out_meta
        else:
            t = p.returncode, std_out, std_err, out_file, [], out_meta
        return t

    def test_crop1(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 0 -e 100", filename)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        np.testing.assert_almost_equal(data[:100], out_data)

    def test_crop2(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 0 -l 100", filename)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        np.testing.assert_almost_equal(data[:100], out_data)

    def test_crop3(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-e 100 -l 100", filename)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        np.testing.assert_almost_equal(data[:100], out_data)

    def test_crop4(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 100 -e 200", filename)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        np.testing.assert_almost_equal(data[100:200], out_data)

    def test_crop_length_warning(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 100 -l 200000000", filename)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        self.assertIn(
            "Warning: specified limits go beyond the extent of the file",
            out, "No warning for bad length")
        np.testing.assert_almost_equal(data[100:], out_data)

    def test_crop_reversed_stop_start(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 10 -e 2", filename)
        self.assertNotEqual(rc, 0, "Return code equal to 0")
        self.assertFalse(os.path.exists(out_file),
                         "Error, but output file exists")
        self.assertIn("End is before start!", err, "Missing error message")

    def test_crop_global_meta(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        with open(meta_file, "r") as f:
            meta = json.load(f)
            meta["global"]["test:foo"] = "bar"
        with open(meta_file, "w") as f:
            json.dump(meta, f)
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 0 -e 100", filename)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        np.testing.assert_almost_equal(data[:100], out_data)
        with open(out_meta, "r") as f:
            meta = json.load(f)
            self.assertEqual(
                meta["global"]["test:foo"], "bar",
                "Additional global data not copied to output file")

    def test_crop_annotation(self):
        data, meta_json, filename, meta_file = self.make_file("normal")
        # Add an annotation
        with open(meta_file, "r") as f:
            meta = json.load(f)
            meta["annotations"].append({
                "core:sample_start": 50,
                "core:sample_count": 10,
                "core:comment": "Test"
            })
        with open(meta_file, "w") as f:
            json.dump(meta, f)
        rc, out, err, out_file, out_data, out_meta = self.run_crop(
            "-s 25 -l 100", filename)
        print(out)
        print(err)
        self.assertEqual(rc, 0, "Return code not equal to 0")
        np.testing.assert_almost_equal(data[25:125], out_data)
        with open(out_meta, "r") as f:
            meta = json.load(f)
            self.assertEqual(meta["annotations"][0]["core:sample_start"],
                             25, "Annotation has wrong sample start")


if __name__ == '__main__':
    gr_unittest.run(qa_crop, "qa_crop.xml")
