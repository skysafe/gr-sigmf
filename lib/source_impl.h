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
    static const pmt::pmt_t TAG_KEY = pmt::string_to_symbol("tag");

    class source_impl : public source {
      private:
      FILE *d_data_fp;
      FILE *d_meta_fp;

      // size of a sample of output data
      size_t d_output_sample_size_bytes;

      // base size of output data, might be sample_size / 2 for complex data
      size_t d_output_base_size;
      // base size of input data, might be sample_size / 2 for complex data
      size_t d_input_base_size;
      // factor to convert from number of samples to base units for the output type
      int d_output_num_samps_to_base;

      // Size of a single sample from the input file in bytes
      size_t d_input_file_sample_size_bytes;

      // Buffer to hold data from the file before deinterlacing
      char* d_multichannel_deinterlace_buffer;


      uint64_t d_num_channels;

      bool d_repeat;
      bool d_file_begin;

      pmt::pmt_t d_add_begin_tag;
      pmt::pmt_t d_id;

      // A multimap of tags to output that maps from tag index to tag
      std::multimap<uint64_t, tag_t> d_tags_to_output;
      size_t d_num_samples_in_file;

      uint64_t d_repeat_count;

      boost::mutex d_open_mutex;

      std::filesystem::path d_data_path;
      std::filesystem::path d_meta_path;

      convert_function_t d_convert_func;

      meta_namespace d_global;
      std::vector<meta_namespace> d_captures;
      std::vector<meta_namespace> d_annotations;

      std::vector<char*> d_output_bufs;

      boost::posix_time::ptime iso_string_to_ptime(const std::string &str);

      void on_command_message(pmt::pmt_t msg);

      bool open();
      void load_metadata();
      void build_tag_list();
      void add_global_tags(const meta_namespace &global_segment);
      void add_tags_from_meta_list(const std::vector<meta_namespace> &meta_list, uint64_t shift_amount);
      void emit_tags(uint64_t window_start, int window_length);

      int single_channel_work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
      int multi_channel_work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);

      std::function<int(int, gr_vector_const_void_star&, gr_vector_void_star&)> d_work_func;

      public:
      source_impl(std::string filename, std::string type, bool repeat);
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
