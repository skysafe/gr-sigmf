# Changelog
All notable changes to gr-sigmf will be documented in this file.
Note that changes before 1.0.2 are not reflected in this file.

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