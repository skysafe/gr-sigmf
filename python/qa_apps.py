import tempfile
from gnuradio import gr, gr_unittest, blocks, analog
from subprocess import Popen, PIPE
import os
import fnmatch
import shutil
from sigmf import sigmf_swig as sigmf


def locate(pattern, root=os.curdir):
    for path, dirs, files in os.walk(os.path.abspath(root)):
        for filename in fnmatch.filter(files, pattern):
            return os.path.join(path, filename)


class AppRunner:
    def __init__(self, testdir, app="sigmf_archive.py"):
        self.testdir = testdir
        self.app = app

    def run(self, argstr):

        sigmf_location = locate(self.app)
        if not sigmf_location:
            raise Exception("Can't find " + self.app + " binary")

        cmdargs = ["python", sigmf_location]
        cmdargs.extend(argstr.split())
        # print(" ".join(cmdargs))

        proc = Popen(cmdargs, stdout=PIPE, stderr=PIPE)
        return proc


class qa_apps(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def run_flowgraph(self, filename):

        samp_rate = 32000

        head = blocks.head(gr.sizeof_float * 1, samp_rate)
        source = analog.sig_source_f(samp_rate, analog.GR_COS_WAVE, 1000, 1, 0)
        sigmf_sink = sigmf.sink("rf32", filename, samp_rate, 'QA test data',
                                'Cate Miller', 'CC-BY-SA', 'Signal Source',
                                False)

        tb = gr.top_block()
        tb.connect(source, head)
        tb.connect(head, sigmf_sink)
        tb.run()
        tb.wait()

    def test_archive(self):

        runner = AppRunner(self.test_dir, "sigmf_archive.py")

        filename = os.path.join(self.test_dir, "temp")
        data_file = filename + ".sigmf-data"
        archive_file = filename + ".sigmf"

        self.run_flowgraph(data_file)

        # archive
        proc = runner.run("archive " + data_file)
        out, err = proc.communicate()

        # extract
        proc = runner.run("extract " + archive_file)
        out, err = proc.communicate()

    def test_hash(self):

        runner = AppRunner(self.test_dir, "sigmf_hash.py")

        filename = os.path.join(self.test_dir, "temp")
        data_file = filename + ".sigmf-data"

        self.run_flowgraph(data_file)

        # update
        proc = runner.run("update " + data_file)
        out, err = proc.communicate()

        # check
        proc = runner.run("check " + data_file)
        out, err = proc.communicate()
