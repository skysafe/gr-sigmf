import argparse
import json
import tarfile
import tempfile
import os
import sys
import hashlib
from itertools import groupby
import fnmatch
import shutil

# TODO: Finish archive support

BUF_SIZE = 65536


def hash_file(file_to_hash, length):
    sha512 = hashlib.sha512()
    data_to_read = length
    while True:
        with open(file_to_hash, "r") as f:
            data = f.read(min(data_to_read, BUF_SIZE))
            data_to_read -= len(data)
            if not data:
                break
            sha512.update(data)
    return "{0}".format(sha512.hexdigest())


def find(pattern, path):
    result = []
    for root, dirs, files in os.walk(path):
        for name in files:
            if fnmatch.fnmatch(name, pattern):
                result.append(os.path.join(root, name))
    return result


class Archive(object):
    def __init__(self, archive_path):
        self.archive_path = archive_path

    def check(self):
        # TODO: This assumes a single recording per archive
        # Find the recording members in the tar file
        with tarfile.open(self.archive_path, "r:") as tar:
            data_member = None
            meta_member = None
            for member in tar.getmembers():
                if member.name.endswith("sigmf-data"):
                    data_member = member
                if member.name.endswith("sigmf-meta"):
                    meta_member = member

        # Use the undocumented features of the tarfile
        if meta_member and data_member:
            meta_size = meta_member.size
            meta_offset = meta_member.offset_data
            data_size = data_member.size
            data_offset = data_member.offset_data
            with open(self.archive_path, "r") as archive:
                archive.seek(meta_offset)
                meta_string = archive.read(meta_size)
                meta = json.loads(meta_string)
                try:
                    cur_hash = meta["global"]["core:sha512"]
                except KeyError:
                    print("No hash in file %s" % self.archive_path)
                    return
                archive.seek(data_offset)
                computed_hash = hash_file(archive, data_size)
            if cur_hash == computed_hash:
                print("Hash match")
            else:
                print("Hash doesn't match")
        else:
            print("couldn't find members")
            return

    def update(self):
        # TODO: This assumes a single recording per archive
        # Make a temp folder
        temp_dir = tempfile.mkdtemp()
        # Untar the archive
        archive_tar = tarfile.open(self.archive_path)
        archive_tar.extractall(path=temp_dir)
        meta_file = find("*.sigmf-meta", temp_dir)
        data_file = find("*.sigmf-data", temp_dir)
        # Then reuse the filepair stuff from below
        file_pair = FilePair([meta_file, data_file])
        file_pair.update()
        # retar the archive
        temp_tar_path = os.path.join(temp_dir, "temp.sigmf")
        new_tar = tarfile.open(temp_tar_path, "w:")
        new_tar.add(meta_file)
        new_tar.add(data_file)
        new_tar.close()
        # Copy it over the existing archive
        shutil.copy2(temp_tar_path, self.archive_path)
        # Clean up the temp directory
        shutil.rmtree(temp_dir)


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
            df_size = os.path.getsize(self.data_file)
            computed_hash = hash_file(self.data_file, df_size)
            if cur_hash != computed_hash:
                print(str(cur_hash))
                print(str(computed_hash))
                print("Hash doesn't match")
            else:
                print("Hash match")

    def update(self):
        with open(self.meta_file, "r+") as mf:
            meta = json.load(mf)
            df_size = os.path.getsize(self.data_file)
            computed_hash = hash_file(self.data_file, df_size)
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
