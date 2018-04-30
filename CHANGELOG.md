# Changelog
All notable changes to gr-sigmf will be documented in this file.
Note that changes before 1.0.2 are not reflected in this file.

## 1.1.4

* Update the grc file for the annotation sink to correspond to the actual sink
* Add documentation in annotation_sink.h for annotation_mode struct
* Fix an edge case in time tag handling when in relative mode in sink block
* Make --freq a required parameter to sigmf-record

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