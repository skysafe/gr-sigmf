import argparse
import tarfile
import re
import os.path
import sys
from itertools import groupby

# TODO: Add options for output name on archive
# TODO: Add tests


class SigmfArchive(object):

    def __init__(self):
        parser = argparse.ArgumentParser(
            description='Convert sigmf files to and from archives',
            usage='''sigmf-archive <command> [<args>]

The available commands are:
   archive     Put files in archive
   extract     Extract files from archive
''')
        parser.add_argument('command', help='Subcommand to run')

        # parse_args defaults to [1:] for args, but you need to
        # exclude the rest of the args too, or validation will fail
        args = parser.parse_args(sys.argv[1:2])
        if not hasattr(self, args.command):
            print 'Unrecognized command'
            parser.print_help()
            exit(1)
        getattr(self, args.command)()

    def archive(self):
        parser = argparse.ArgumentParser(
            description='Archive files')
        parser.add_argument('files', nargs="+")
        parser.add_argument('--remove-files', action='store_true')

        args = parser.parse_args(sys.argv[2:])
        file_list = args.files
        final_file_list = []

        # Determine file names to archive
        for k, g in groupby(file_list, lambda f: os.path.splitext(f)[0]):
            group_list = list(g)
            if (len(group_list) == 1):
                base, ext = os.path.splitext(group_list[0])
                if ext == ".sigmf-meta":
                    other_file = base + ".sigmf-data"
                else:
                    other_file = base + ".sigmf-meta"
                final_file_list.append(other_file)
            final_file_list.extend(group_list)

        # then we can just use the name of that file
        if len(final_file_list) == 2:
            output_file = os.path.splitext(final_file_list[0])[0]

        output_file += ".sigmf"

        # Archive the files
        tfile = tarfile.TarFile(
            name=output_file, mode="w", format=tarfile.PAX_FORMAT)
        for f in final_file_list:
            base_name = os.path.split(f)[1]
            base_name_sans_ext, ext = os.path.splitext(base_name)
            tfile_path = "{0}/{0}{1}".format(base_name_sans_ext, ext)
            tfile.add(f, tfile_path)

        # Remove the originals if requested
        if (args.remove_files):
            for f in final_file_list:
                os.remove(f)

        # TODO: Figure out how to name files when there is more than 1 data set

    def extract(self):
        parser = argparse.ArgumentParser(
            description='Extract files')
        parser.add_argument('files', nargs="+")
        parser.add_argument('--remove-files', action='store_true')
        args = parser.parse_args(sys.argv[2:])
        file_list = args.files

        # Extract files
        for tar_file in file_list:
            tf = tarfile.TarFile(tar_file)
            members = tf.getmembers()
            for k, g in groupby(
                    members, lambda m: re.search(
                        r"([^\/]+)\/\1\.sigmf-(meta|data)", m.name)):
                for f in g:
                    tf.extract(f, path=os.path.split(tar_file)[0])

        # Delete archive if asked
        if (args.remove_files):
            for f in file_list:
                os.remove(f)


if __name__ == '__main__':
    SigmfArchive()
