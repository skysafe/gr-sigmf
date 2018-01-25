import tempfile
from gnuradio import gr_unittest
import os
import shutil
import apps_test_helper


class qa_archive(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def test_archive(self):

        runner = apps_test_helper.AppRunner(self.test_dir, "sigmf_archive.py")

        filename = os.path.join(self.test_dir, "temp")
        data_file = filename + ".sigmf-data"
        archive_file = filename + ".sigmf"

        apps_test_helper.run_flowgraph(data_file)

        # archive
        proc = runner.run("archive " + data_file)
        out, err = proc.communicate()

        # extract
        proc = runner.run("extract " + archive_file)
        out, err = proc.communicate()
