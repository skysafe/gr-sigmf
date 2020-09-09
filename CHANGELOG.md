# Changelog
All notable changes to gr-sigmf will be documented in this file.
Note that changes before 1.0.2 are not reflected in this file.
## 2.1.0
* Migrated module to GNU Radio 3.8

## 2.0.0

* Renamed module to gr_sigmf to avoid name conflict with official gnuradio sigmf python module
* Fix an error in parsing of certain gps messages in gps_message_source
* Fix a bug in the source block that caused some tags to have bad offsets
* Improved a few tests
* open throws an exception on an error, rather than return false
* Remove `core:length` support, as it was removed from the standard
* Improved error messages for `sigmf-record`
* Added more tests
* Fixed a bug in source block releated to repeats

## 1.1.6
* Add support for writing `core:length` to capture segments
* Update `sigmf-record` to write `core:recorder` field
* Improvements to documentation
* Fixed an error in gps logging
* Sink block will now throw an error if an invalid file is supplied
* Change the default time mode of the sink to be relative
* Handle thread interruption correctly in usrp_gps_message block
* Add a sigmf-crop tool
* Miscellaneous fixes and improvements

## 1.1.5

* Add support for building a pkg-config file when building gr-sigmf
* Fixed flaky test: test_relative_time_mode_initial_closed
* Cleaned up output of blocks to stdout and stderr in preference to standard logging macros
* Update the handler for the close command pmt message to call `do_update` immediately
  This fixes an issue where a lot of time may pass between when the command to
  close is received and when the work function runs again.
* Fixed a couple other flaky tests
* Updates and improvements to the USRP GPS Message Source
* Miscellaneous fixes and improvements

## 1.1.4

* Update the grc file for the annotation sink to correspond to the actual sink
* Add documentation in annotation_sink.h for annotation_mode struct
* Fix an edge case in time tag handling when in relative mode in sink block
* Make --freq a required parameter to sigmf-record
* Add USRP GPS Message Source

## 1.1.3

* Fixed a bug in the logging configuration for gr-sigmf
* Updated .gitignore for gr-sigmf

## 1.1.2

* Fix a bug in the sink grc file

## 1.1.1

* Fix a bug in `sigmf-record` that caused int-N tuning not to get set
* Fix a bug related to setting the subdev spec in  `sigmf-record`. It is now set before all other parameters instead of last
* Improved the wait logic in `sigmf-record` to allow the user to cancel in the case that the duration is specified and it takes longer than expected to receive the needed samples

## 1.1.0

* Add annotation sink to gr-sigmf
* Fix bug that caused deleted keys in namespaces to not actually delete
* Refactor `sink_time_mode` to `sigmf_time_mode` now that it is needed in multiple places
* Add a get method to `meta_namespace` that takes a single pmt object as a key
* Make the `print` method of `meta_namespace` const
* Change how ondisk metadata is loaded to ensure that sample_rate is always loaded as a double

## 1.0.3

* Fixed a bug that caused numerical values over int32 max to not be loaded correctly

## 1.0.2

* Fixed a bug in how offsets were computed for tags in the source block
