/* -*- c++ -*- */
/* 
 * Copyright 2018 Paul Wicks
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <boost/algorithm/string.hpp>
#include "sigmf/sigmf_utils.h"
#include "sigmf/meta_namespace.h"
#include "writer_utils.h"
#include "reader_utils.h"
#include "pmt_utils.h"
#include "annotation_sink_impl.h"

namespace posix = boost::posix_time;

namespace gr {
  namespace sigmf {

    annotation_sink::sptr
    annotation_sink::make(std::string filename, annotation_mode mode, sigmf_time_mode time_mode)
    {
      return gnuradio::get_initial_sptr
        (new annotation_sink_impl(filename, mode, time_mode));
    }

    /*
     * The private constructor
     */
    annotation_sink_impl::annotation_sink_impl(std::string filename, annotation_mode mode, sigmf_time_mode time_mode)
    : gr::block("annotation_sink",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(0, 0, 0)),
      d_filter_strategy(mode.filter_strategy), d_filter_key_regex(glob_to_regex(mode.filter_key)),
      d_data_path(to_data_path(filename)), d_meta_path(meta_path_from_data(d_data_path)),
      d_sample_rate(-1), d_time_mode(time_mode)
    {
      message_port_register_in(pmt::mp("annotations"));
      set_msg_handler(pmt::mp("annotations"),
                      boost::bind(&annotation_sink_impl::add_annotation, this, _1));
      // Open and load metadata here
      open();
      load_metadata();
      // TODO: This is a bit gross...
      std::fclose(d_meta_fp);
    }

    bool
    annotation_sink_impl::open()
    {
      d_meta_fp = std::fopen(d_meta_path.c_str(), "r");
      if(d_meta_fp == NULL) {
        std::stringstream s;
        s << "failed to open meta file, errno = " << errno << std::endl;
        throw std::runtime_error(s.str());
      }
      return true;
    }

    void
    annotation_sink_impl::add_annotation(pmt::pmt_t annotation_msg)
    {
      pmt::pmt_t sample_start_pmt = pmt::dict_ref(annotation_msg, SAMPLE_START_KEY, pmt::get_PMT_NIL());
      pmt::pmt_t sample_count_pmt = pmt::dict_ref(annotation_msg, SAMPLE_COUNT_KEY, pmt::get_PMT_NIL());
      pmt::pmt_t time_pmt = pmt::dict_ref(annotation_msg, ANNO_TIME_KEY, pmt::get_PMT_NIL());
      pmt::pmt_t duration_pmt = pmt::dict_ref(annotation_msg, ANNO_DURATION_KEY, pmt::get_PMT_NIL());

      // Check if there is a time and duration key
      if(!pmt::eqv(time_pmt, pmt::get_PMT_NIL()) &&
                !pmt::eqv(duration_pmt, pmt::get_PMT_NIL())) {

          uint64_t time_secs;
          double time_frac_secs;
          std::tie(time_secs, time_frac_secs) = pmt_utils::extract_uhd_time(time_pmt);

          uint64_t duration_secs;
          double duration_frac_secs;
          std::tie(duration_secs, duration_frac_secs) = pmt_utils::extract_uhd_time(duration_pmt);

        if (d_time_mode == sigmf_time_mode::relative) {
          // We just convert these straight to sample counts via the sample_rate
          if(d_sample_rate > 0) {
            uint64_t sample_start = (time_secs * d_sample_rate) + (time_frac_secs * d_sample_rate);
            uint64_t sample_count =
              (duration_secs * d_sample_rate) + (duration_frac_secs * d_sample_rate);
            sample_start_pmt = pmt::from_uint64(sample_start);
            sample_count_pmt = pmt::from_uint64(sample_count);
            // Adjust the dict for this annotation
            annotation_msg = pmt::dict_delete(annotation_msg, ANNO_TIME_KEY);
            annotation_msg = pmt::dict_delete(annotation_msg, ANNO_DURATION_KEY);
            annotation_msg = pmt::dict_add(annotation_msg, SAMPLE_START_KEY, sample_start_pmt);
            annotation_msg = pmt::dict_add(annotation_msg, SAMPLE_COUNT_KEY, sample_count_pmt);
          }
        } else {
          // Get seconds and frac seconds and subtract
          uint64_t start_secs;
          double start_frac_secs;
          std::tie(start_secs, start_frac_secs) = pmt_utils::extract_uhd_time(d_start_time);
          uint64_t secs_diff = (time_secs - start_secs);
          double frac_secs_diff = time_frac_secs - start_frac_secs;
          if (frac_secs_diff < 0) {
            frac_secs_diff = +frac_secs_diff;
            secs_diff -= 1;
          }
          // then convert all of it to sample counts
          uint64_t sample_start = (secs_diff * d_sample_rate) + (frac_secs_diff * d_sample_rate);

          GR_LOG_DEBUG(d_logger, "Annotation sample start is :" << sample_start);
          uint64_t sample_count =
            (duration_secs * d_sample_rate) + (duration_frac_secs * d_sample_rate);
          sample_start_pmt = pmt::from_uint64(sample_start);
          sample_count_pmt = pmt::from_uint64(sample_count);
        }
        // Adjust the dict for this annotation
        annotation_msg = pmt::dict_delete(annotation_msg, ANNO_TIME_KEY);
        annotation_msg = pmt::dict_delete(annotation_msg, ANNO_DURATION_KEY);
        annotation_msg = pmt::dict_add(annotation_msg, SAMPLE_START_KEY, sample_start_pmt);
        annotation_msg = pmt::dict_add(annotation_msg, SAMPLE_COUNT_KEY, sample_count_pmt);
      }
      // Check if there is a sample start and count
      if(!pmt::eqv(sample_start_pmt, pmt::get_PMT_NIL()) &&
         !pmt::eqv(sample_count_pmt, pmt::get_PMT_NIL())) {
        bool found_match = false;
        for(auto &anno : d_annotations) {

          // sample_counts and sample_starts received in messages might be integers even though
          // they should be uint64s, so we have to handle that case here
          bool start_equal = false;
          bool count_equal = false;
          if (pmt::is_integer(sample_start_pmt)) {
            int64_t temp = pmt::to_long(sample_start_pmt);
            if (temp >= 0) {
              uint64_t native_start = pmt::to_uint64(anno.get(SAMPLE_START_KEY));
              start_equal = static_cast<uint64_t>(temp) == native_start;
            }
          } else {
            start_equal = pmt::eqv(anno.get(SAMPLE_START_KEY), sample_start_pmt);
          }
          if (pmt::is_integer(sample_count_pmt)) {
            int64_t temp = pmt::to_long(sample_count_pmt);
            if (temp >= 0) {
              uint64_t native_count = pmt::to_uint64(anno.get(SAMPLE_COUNT_KEY));
              count_equal = static_cast<uint64_t>(temp) == native_count;
            }
          } else {
            count_equal = pmt::eqv(anno.get(SAMPLE_COUNT_KEY), sample_count_pmt);
          }

          if(start_equal && count_equal) {
            found_match = true;
            // Get all the keys from the message
            pmt::pmt_t keys = pmt::dict_keys(annotation_msg);
            for (size_t i = 0; i < pmt::length(keys); i++) {
              pmt::pmt_t key(pmt::nth(i, keys));
              pmt::pmt_t val(pmt::dict_ref(annotation_msg, key, pmt::PMT_NIL));
              anno.set(key, val); 
            }
            break;
          }
        }
        // If we didn't merge it with an existing one, just add it
        if (!found_match) {
          d_annotations.push_back(annotation_msg);
        }
      }
    }

    void
    annotation_sink_impl::load_metadata()
    {
      metafile_namespaces ns = load_metafile(d_meta_fp);
      d_global = ns.global;
      d_captures = ns.captures;
      d_annotations = ns.annotations;
      
      if (d_filter_strategy == annotation_filter_strategy::clear_existing) {
        for(meta_namespace &anno_ns: d_annotations) {
          auto keys = anno_ns.keys();
          std::smatch m;
          for(const auto &key: keys) {
            // Check if key matches regex
            if (std::regex_match(key, m, d_filter_key_regex)) {
              // then we keep will drop it 
              anno_ns.del(key);
            }
          }
        }
      }
      // time handling can require this
      if (d_global.has("core:sample_rate")) {
        d_sample_rate = pmt::to_double(d_global.get("core:sample_rate"));
      }

      if (d_time_mode == sigmf_time_mode::absolute) {
        // need to get start time
        pmt::pmt_t start_time = d_captures[0].get("core:datetime", pmt::get_PMT_NIL());
        if (pmt::is_null(start_time)) {
          throw std::runtime_error("Can't use absolute mode if datetime not set!");
        } else {
          std::string start_time_str = pmt::symbol_to_string(start_time);
          // Parse this and convert it to a uhd time...
          posix::ptime parsed_time = reader_utils::iso_string_to_ptime(start_time_str);
          d_start_time = reader_utils::ptime_to_uhd_time(parsed_time);
        }
      }
    }

    void
    annotation_sink_impl::write_metadata() {
      // Blow away whatever was there
      d_meta_fp = std::fopen(d_meta_path.c_str(), "w");
      // Write over it with our new meta
      writer_utils::write_meta_to_fp(d_meta_fp, d_global, d_captures, d_annotations);
      std::fclose(d_meta_fp);
    }

    std::regex
    annotation_sink_impl::glob_to_regex(const std::string &filter_glob)
    {
      if (filter_glob == "") {
        // match everything if empty string
        return std::regex(".*");
      }

      std::string regex_str = boost::replace_all_copy(filter_glob, ".", "\\.");
      boost::replace_all(regex_str, "*", ".*");
      boost::replace_all(regex_str, "?", ".");
      return std::regex(regex_str);
    }

    bool
    annotation_sink_impl::stop() {
      write_metadata();
      return true;
    }
  } /* namespace sigmf */
} /* namespace gr */

