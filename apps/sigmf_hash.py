import argparse
import json
# import tarfile
import os.path
import sys
import hashlib
from itertools import groupby

# TODO: Finish archive support

BUF_SIZE = 65536


def hash_file(file_to_hash):
    sha512 = hashlib.sha512()

    with open(file_to_hash, 'rb') as f:
        while True:
            data = f.read(BUF_SIZE)
            if not data:
                break
            # md5.update(data)
            # sha1.update(data)
    return "{0}".format(sha512.hexdigest())


class Archive(object):
    def __init__(self, archive_path):
        self.archive_path = archive_path

    def check(self):

        # TODO: implement me
        pass

    def update(self):

        # TODO: implement me
        pass


class FilePair(object):
    def __init__(self, files):
        self.data_file = [f for f in files if f.endswith("sigmf-data")][0]
        self.meta_file = [f for f in files if f.endswith("sigmf-meta")][0]

    def check(self):
        with open(self.meta_file, "r") as mf:
            meta = json.load(mf)
            try:
                cur_hash = meta["global"]["core:sha512"]
            except KeyError:
                print("No hash in file %s" % self.data_file)
                return
            computed_hash = hash_file(self.data_file)
            if cur_hash != computed_hash:
                print("Hashe doesn't match")
            else:
                print("Hash match")

    def update(self):
        with open(self.meta_file, "r+") as mf:
            meta = json.load(mf)
            computed_hash = hash_file(self.data_file)
            meta["global"]["core:sha512"] = computed_hash
            mf.seek(0)
            json.dump(meta, mf)
            mf.truncate()


class SigmfHash(object):

    def __init__(self):
        parser = argparse.ArgumentParser(
            description='Check and update hashes on sigmf files',
            usage='''sigmf-hash <command> [<args>]

The available commands are:
   check       Verify the hash of a dataset
   update      Recompute and update the hash of a dataset
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

    def _build_file_list(self, raw_list):
        final_file_list = []
        for k, g in groupby(raw_list, lambda f: os.path.splitext(f)[0]):
            group_list = list(g)
            if (len(group_list) == 1):
                base, ext = os.path.splitext(group_list[0])
                if ext == ".sigmf":
                    final_file_list.append(Archive(group_list[0]))
                    continue
                elif ext == ".sigmf-meta":
                    other_file = base + ".sigmf-data"
                else:
                    other_file = base + ".sigmf-meta"
                group_list.append(other_file)
            final_file_list.append(FilePair(group_list))
        return final_file_list

    def check(self):
        parser = argparse.ArgumentParser(
            description='Check file hashes')
        parser.add_argument('files', nargs="+")

        args = parser.parse_args(sys.argv[2:])
        # file_list = args.files
        final_file_list = self._build_file_list(args.files)
        for f in final_file_list:
            f.check()

    def update(self):
        parser = argparse.ArgumentParser(
            description='Update file hashes')
        parser.add_argument('files', nargs="+")

        args = parser.parse_args(sys.argv[2:])
        # file_list = args.files
        final_file_list = self._build_file_list(args.files)
        for f in final_file_list:
            f.update()


if __name__ == '__main__':
    SigmfHash()
