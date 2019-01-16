# gr-sigmf: GNU Radio SigMF Blocks

This module contains blocks to read from and write to SigMF (the Signal
Metadata Format) recordings in GNU Radio.

Currently gr-sigmf is best described as alpha software. Basic interactions
work, but features are not complete and the API should be considered somewhat
unstable. We welcome any feature requests or bug reports. Pull requests are
fine too, but will be met with more success if you make an issue first.

Data correctness issues will be prioritized over reliability issues, which will
in turn be prioritized over new feature development.

## Quick Start

Dependencies required:

* GNU Radio
* RapidJSON
* Swig (for Python support)
* UHD (for USRP recording and playback tools)

To install dependencies on Ubuntu 18.04 LTS:

    $ sudo apt install rapidjson-dev swig gnuradio libuhd-dev

To install from source:

    $ git clone git@github.com:skysafe/gr-sigmf.git
    $ cd gr-sigmf
    $ mkdir build; cd build
    $ cmake ..
    $ make
    $ sudo make install

To make a SigMF recording using an Ettus Research USRP:

    $ sigmf-record --sample-rate 10e6 --freq 88.5e6 --gain 30 example.sigmf

Note that the gr-sigmf python module is named `gr_sigmf` to avoid conflicts with the
[official GNURadio sigmf module](https://github.com/gnuradio/SigMF).

## Design Principles

* Correctly and completely implement the [SigMF Specification](https://github.com/gnuradio/SigMF/blob/master/sigmf-spec.md).
* Be a "good GNU Radio citizen", and interact in useful ways with the existing core blocks, existing OOT blocks, and hardware interfaces such as ``gr-uhd``.

## Contents

* A set of blocks for reading and writing SigMF datasets
  * sink: Create SigMF datasets
  * source: Read SigMF datasets
  * annotation_sink: Write SigMF metadata only
  * usrp_gps_message_source: Surface gps metadata from a usrp as messages so it can easily be used by a SigMF sink.

* A set of command line tools for creating/working with SigMF datasets. They include:
  * sigmf-record: Create SigMF datasets from Ettus radios
  * sigmf-play: Replay SigMF datasets to Ettus radios
  * sigmf-archive: Convert SigMF datasets to and from archive files
  * sigmf-crop: Extract subsections from SigMF datasets
  * sigmf-hash: Verify and calculate hashes for SigMF datasets

## Roadmap

### Near Future

* Output metadata via message (for support in applications like 'such samples')
* Make sure that UHD generates/reads stream tags that are nicely handled by
  SigMF blocks.
* Make waterfall, frequency, and time sinks interact well with stream tags as
  translated from metadata.
* Build stream tag converter block to convert uhd stream tags to tags with keys that match sigmf keys

### Farther Future

* Dynamic control over recording state via message
* Tar archive support
* Multi-channel recording support, if it is introduced into the main spec
* Automatic file rotation?
