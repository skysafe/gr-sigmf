import os
from subprocess import Popen, PIPE

from gnuradio import gr, blocks, analog

try:
    import gr_sigmf as sigmf
except ImportError:
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    import gr_sigmf as sigmf


SIGMF_METADATA_EXT = ".sigmf-meta"
SIGMF_DATASET_EXT = ".sigmf-data"


class AppRunner:
    def __init__(self, testdir, app="sigmf_archive.py"):
        self.testdir = testdir
        self.app = app

    def run(self, argstr):
        cmdargs = [self.app]
        cmdargs.extend(argstr.split())
        proc = Popen(cmdargs, stdout=PIPE, stderr=PIPE)
        return proc


def run_flowgraph(filename):

    samp_rate = 32000

    head = blocks.head(gr.sizeof_float * 1, samp_rate)
    source = analog.sig_source_f(samp_rate, analog.GR_COS_WAVE, 1000, 1, 0)
    sigmf_sink = sigmf.sink("rf32_le", filename)

    tb = gr.top_block()
    tb.connect(source, head)
    tb.connect(head, sigmf_sink)
    tb.run()
    tb.wait()
