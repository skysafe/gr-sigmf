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

#ifndef INCLUDED_SIGMF_ANNOTATION_SINK_IMPL_H
#define INCLUDED_SIGMF_ANNOTATION_SINK_IMPL_H

#include <sigmf/annotation_sink.h>
#include <pmt/pmt.h>
#include <regex>

namespace gr {
  namespace sigmf {

    pmt::pmt_t SAMPLE_START_KEY = pmt::mp("core:sample_start");
    pmt::pmt_t SAMPLE_COUNT_KEY = pmt::mp("core:sample_count");
    pmt::pmt_t ANNO_TIME_KEY = pmt::mp("time");
    pmt::pmt_t ANNO_DURATION_KEY = pmt::mp("duration");

    class annotation_sink_impl : public annotation_sink {
      private:
      annotation_filter_strategy d_filter_strategy;
      std::regex d_filter_key_regex;

      boost::filesystem::path d_data_path;
      boost::filesystem::path d_meta_path;
      FILE *d_meta_fp;

      meta_namespace d_global;
      std::vector<meta_namespace> d_captures;
      std::vector<meta_namespace> d_annotations;

      double d_sample_rate;

      sigmf_time_mode d_time_mode;

      // Start time as uhd time tuple
      pmt::pmt_t d_start_time;

      std::regex glob_to_regex(const std::string &filter_glob);

      void add_annotation(pmt::pmt_t annotation_msg);

      void write_metadata();
      bool open();
      void load_metadata();

      public:
      annotation_sink_impl(std::string filename, annotation_mode mode, sigmf_time_mode time_mode);
      ~annotation_sink_impl() = default;

      bool stop();
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_ANNOTATION_SINK_IMPL_H */
