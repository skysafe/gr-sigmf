/* -*- c++ -*- */
/*
 * Copyright 2017 Scott Torborg, Paul Wicks, Caitlin Miller
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

#ifndef INCLUDED_SIGMF_SOURCE_IMPL_H
#define INCLUDED_SIGMF_SOURCE_IMPL_H

#include <cstdio>
#include <sigmf/meta_namespace.h>
#include <sigmf/source.h>
#include "type_converter.h"

namespace gr {
  namespace sigmf {

    static const pmt::pmt_t COMMAND = pmt::mp("command");
    static const pmt::pmt_t META = pmt::mp("meta");
    static const pmt::pmt_t TIME_KEY = pmt::string_to_symbol("rx_time");
    static const pmt::pmt_t RATE_KEY = pmt::string_to_symbol("rx_rate");
    static const pmt::pmt_t FREQ_KEY = pmt::string_to_symbol("rx_freq");
    static const pmt::pmt_t TAG_KEY = pmt::string_to_symbol("tag");

    class source_impl : public source {
      private:
      FILE *d_data_fp;
      FILE *d_meta_fp;

      // size of a sample
      size_t d_sample_size;

      // base size of a data, might be item_size / 2
      size_t d_base_size;
      size_t d_input_size;
      int d_num_samps_to_base;

      bool d_repeat;
      bool d_debug;
      bool d_file_begin;

      pmt::pmt_t d_add_begin_tag;
      pmt::pmt_t d_id;

      std::vector<tag_t> d_tags_to_output;
      size_t d_next_tag_index;
      size_t d_num_samples_in_file;

      uint64_t d_repeat_count;

      boost::mutex d_open_mutex;

      boost::filesystem::path d_data_path;
      boost::filesystem::path d_meta_path;

      convert_function_t d_convert_func;

      meta_namespace d_global;
      std::vector<meta_namespace> d_captures;
      std::vector<meta_namespace> d_annotations;

      void on_command_message(pmt::pmt_t msg);

      bool open();
      void load_metadata();
      void build_tag_list();
      void add_tags_from_meta_list(const std::vector<meta_namespace> &meta_list);

      public:
      source_impl(std::string filename, std::string type, bool repeat, bool debug);
      ~source_impl();

      // Where all the action really happens
      int work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);

      void set_begin_tag(pmt::pmt_t tag);

      gr::sigmf::meta_namespace &global_meta();
      std::vector<gr::sigmf::meta_namespace> &capture_segments();
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_SOURCE_IMPL_H */
