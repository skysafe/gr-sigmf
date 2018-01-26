import tempfile
from gnuradio import gr_unittest
import os
import shutil
import apps_test_helper


class qa_hash(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def test_hash(self):

        runner = apps_test_helper.AppRunner(self.test_dir, "sigmf_hash.py")

        filename = os.path.join(self.test_dir, "temp")
        data_file = filename + apps_test_helper.SIGMF_DATASET_EXT

        apps_test_helper.run_flowgraph(data_file)

        # update
        proc = runner.run("update " + data_file)
        out, err = proc.communicate()

        # check
        proc = runner.run("check " + data_file)
        out, err = proc.communicate()
        assert out == "Hash match\n"
