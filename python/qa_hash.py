import tempfile
from gnuradio import gr_unittest
import os
import shutil
import apps_test_helper
import json


class qa_hash(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def test_hash(self):

        runner = apps_test_helper.AppRunner(self.test_dir, "sigmf-hash")

        filename = os.path.join(self.test_dir, "temp")
        data_file = filename + apps_test_helper.SIGMF_DATASET_EXT
        meta_file = filename + apps_test_helper.SIGMF_METADATA_EXT

        apps_test_helper.run_flowgraph(data_file)

        # update
        proc = runner.run("update " + data_file)
        out, err = proc.communicate()

        # check
        proc = runner.run("check " + data_file)
        out, err = proc.communicate()
        assert out == "Hash match\n"

        meta = open(meta_file, "r")
        data = json.loads(meta.read())

        assert 'core:sha512' in data['global']


if __name__ == '__main__':
    gr_unittest.run(qa_hash, "qa_hash.xml")
