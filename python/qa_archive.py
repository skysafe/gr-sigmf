import tempfile
from gnuradio import gr_unittest
import os
import shutil
import apps_test_helper
import tarfile
import numpy as np


class qa_archive(gr_unittest.TestCase):

    def setUp(self):

        # Create a temporary directory
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):

        # Remove the directory after the test
        shutil.rmtree(self.test_dir)

    def test_archive(self):

        runner = apps_test_helper.AppRunner(self.test_dir, "sigmf-archive")

        filename = "temp"

        file_path = os.path.join(self.test_dir, filename)
        data_file = file_path + apps_test_helper.SIGMF_DATASET_EXT
        archive_file = file_path + ".sigmf"

        apps_test_helper.run_flowgraph(data_file)

        # archive
        proc = runner.run("archive " + data_file)
        out, err = proc.communicate()

        sigmf_tarfile = tarfile.open(archive_file,
                                     mode="r", format=tarfile.PAX_FORMAT)
        files = sigmf_tarfile.getmembers()

        file_extensions = {apps_test_helper.SIGMF_DATASET_EXT,
                           apps_test_helper.SIGMF_METADATA_EXT}
        for f in files:

            # layout
            assert tarfile.TarInfo.isfile(f)
            f_dir = os.path.split(f.name)[0]

            # names and extensions
            assert f_dir == filename
            f_name, f_ext = os.path.splitext(f.name)
            assert f_ext in file_extensions
            if f.name.endswith(apps_test_helper.SIGMF_METADATA_EXT):
                m_file = f
            elif f.name.endswith(apps_test_helper.SIGMF_DATASET_EXT):
                d_file = f
            assert os.path.split(f_name)[1] == f_dir

            # permissions
            # assert f.mode == 0o644

        # type
        assert sigmf_tarfile.format == tarfile.PAX_FORMAT

        # extract
        proc = runner.run("extract " + archive_file)
        out, err = proc.communicate()

        # content
        meta_expected = open(os.path.join(
                             self.test_dir,
                             filename + apps_test_helper.SIGMF_METADATA_EXT),
                             "r")
        meta_actual = open(os.path.join(self.test_dir, m_file.name), "r")
        assert meta_expected.read() == meta_actual.read()

        data_expected = open(os.path.join(
                             self.test_dir,
                             filename + apps_test_helper.SIGMF_DATASET_EXT),
                             "r")
        data_actual = open(os.path.join(self.test_dir, d_file.name), "r")
        de = np.fromstring(data_expected.read()).all()
        da = np.fromstring(data_actual.read()).all()
        assert de == da


if __name__ == '__main__':
    gr_unittest.run(qa_archive, "qa_archive.xml")
