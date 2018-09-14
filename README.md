# GNU Radio SigMF Blocks

This module contains blocks to read from and write to SigMF recordings in GNU
Radio.

Gr-sigmf is currently a *work in progress*. We welcome any feature requests or bug reports.
Pull requests are fine too, but will be met with more success if you make an issue first.

Ultimately we'd be happy if gr-sigmf became the canonical implementation of SigMF in GNURadio.

## Goals

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

This is a (rough) planned sequence of development.

### Completed so far

* Basic SigMF Sink with core namespace fields as parameters
* Factor out classes for handling metadata
* SigMF Recorder tool (sigmf-record)
* Basic SigMF Source with complex float 32 only
* SigMF Sink correctly converts uhd stream tags to new capture segments
* SigMF Source correctly converts capture segments and annotations to stream tags
* Convert stream tags to annotations in sink block
* SigMF Player tool (sigmf-play)
* Automatic sample format conversion in Source
* Handle annotations via messages (with sample offset or timestamp)
* Python tools to accomplish basic tasks (e.g. updating checksum, handling archives)

### Up next

* Output metadata via message (for support in applications like 'such samples')
* Make sure that UHD generates/reads stream tags that are nicely handled by
  SigMF blocks.
* Make waterfall, frequency, and time sinks interact well with stream tags as
  translated from metadata.
* Build stream tag converter block to convert uhd stream tags to tags with keys that match sigmf keys

### Farther future

* Dynamic control over recording state via message
* Tar archive support
* Multi-channel recording support, if it is introduced into the main spec
* Automatic file rotation?

### Bare minimum level of usability for gathering/manipulating data

* SigMF Recorder (as a GNU Radio flowgraph, with UHD)
* SigMF Player (as a GNU Radio flowgraph, with UHD)
* SigMF Source

## Notes

To guarantee valid data in the event of a crash, write an initial minimal metadata file at launch, then write a completed file on a clean shutdown.


## Testing

Any new feature added to gr-sigmf should be accompannied by a unit test.