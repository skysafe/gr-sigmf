import tempfile
import shutil
from gnuradio import gr_unittest


class qa_apps(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)
