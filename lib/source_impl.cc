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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sstream>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <gnuradio/io_signature.h>
#include "sigmf/sigmf_utils.h"
#include "source_impl.h"
#include "type_converter.h"
#include "tag_keys.h"

namespace posix = boost::posix_time;
namespace algo = boost::algorithm;

namespace gr {
  namespace sigmf {

    source::sptr
    source::make(std::string filename, std::string type, bool repeat, bool debug)
    {
      return gnuradio::get_initial_sptr(new source_impl(filename, type, repeat, debug));
    }

    /*
     * The private constructor
     */
    source_impl::source_impl(std::string filename, std::string type, bool repeat, bool debug)
    : gr::sync_block("source",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(float))),
      d_data_fp(0), d_meta_fp(0), d_repeat(repeat), d_debug(debug), d_file_begin(true),
      d_add_begin_tag(pmt::PMT_NIL), d_repeat_count(0),
      d_data_path(to_data_path(filename)), d_meta_path(meta_path_from_data(d_data_path))
    {

      // command message port
      message_port_register_in(COMMAND);
      set_msg_handler(COMMAND, boost::bind(&source_impl::on_command_message, this, _1));

      // metadata message port
      message_port_register_out(META);

      open();
      load_metadata();
      std::string input_datatype = d_global.get_str("core:datatype");

      // Get the input datatype
      format_detail_t input_detail = parse_format_str(input_datatype);

      // and output datatype
      format_detail_t output_detail = parse_format_str(type);

      // Need to divide by 8 to convert to bytes
      d_sample_size = (output_detail.width * (output_detail.is_complex ? 2 : 1)) / 8;

      d_num_samps_to_base = output_detail.is_complex ? 2 : 1;
      d_base_size = output_detail.width / 8;
      d_input_size = input_detail.width / 8;

      std::fseek(d_data_fp, 0, SEEK_END);
      d_num_samples_in_file = std::ftell(d_data_fp) / d_sample_size;

      if(d_debug) {
        std::cout << "Samps in file: " << d_num_samples_in_file;
      }

      std::fseek(d_data_fp, 0, SEEK_SET);
      set_output_signature(gr::io_signature::make(1, 1, d_sample_size));

      d_convert_func = get_convert_function(input_datatype, type);

      std::stringstream ss;
      ss << name() << unique_id();
      d_id = pmt::string_to_symbol(ss.str());
    }

    /*
     * Our virtual destructor.
     */
    source_impl::~source_impl()
    {
    }

    void
    source_impl::on_command_message(pmt::pmt_t msg)
    {
      if(!pmt::is_dict(msg)) {
        GR_LOG_ERROR(d_logger, boost::format("Command message is not a dict: %s") % msg);
        return;
      }
      pmt::pmt_t command_pmt = pmt::dict_ref(msg, COMMAND, pmt::get_PMT_NIL());
      if(pmt::eqv(command_pmt, pmt::get_PMT_NIL())) {
        GR_LOG_ERROR(d_logger, boost::format("Command key not found in dict: %s") % msg);
        return;
      }
      std::string command_str = pmt::symbol_to_string(command_pmt);

      if(command_str == "set_begin_tag") {
        pmt::pmt_t tag = pmt::dict_ref(msg, TAG_KEY, pmt::get_PMT_NIL());
        if(pmt::eqv(tag, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Tag key not found in dict: %s") % msg);
          return;
        }
        set_begin_tag(tag);
      }

      if(d_debug) {
        std::cout << "Received command message" << std::endl;
      }
    }

    posix::ptime
    source_impl::iso_string_to_ptime(const std::string &str)
    {
      boost::posix_time::ptime time;
      std::stringstream ss(str);
      boost::local_time::local_time_input_facet *ifc =
        new boost::local_time::local_time_input_facet();
      ifc->set_iso_extended_format();
      ss.imbue(std::locale(ss.getloc(), ifc));
      boost::local_time::local_date_time zonetime(boost::local_time::not_a_date_time);
      if(ss >> zonetime) {
        time = zonetime.utc_time();
      }
      return time;
    }

    void
    source_impl::add_tags_from_meta_list(const std::vector<meta_namespace> &meta_list)
    {
      for(std::vector<meta_namespace>::const_iterator it = meta_list.begin();
          it != meta_list.end(); it++) {
        meta_namespace ns = *it;
        uint64_t offset;
        std::set<std::string> capture_keys = ns.keys();

        if(capture_keys.count("core:sample_start")) {
          offset = pmt::to_uint64(ns.get("core:sample_start"));

          // remove this key, we don't need it as a tag later
          capture_keys.erase("core:sample_start");

        } else {
          throw std::runtime_error(
            "Invalid metadata, no core:sample_start found for capture_segment");
        }
        for(std::set<std::string>::iterator it = capture_keys.begin();
            it != capture_keys.end(); it++) {
          std::string key = *it;
          tag_t tag;
          tag.offset = offset;
          if (key == "core:frequency") {
            tag.key = FREQ_KEY;
          } else if (key == "core:datetime") {
            tag.key = TIME_KEY;
          } else {
            tag.key = pmt::mp(key);
          }
          if (key == "core:datetime") {
            std::string iso_string = pmt::symbol_to_string(ns.get(key));
            posix::ptime parsed_time = iso_string_to_ptime(iso_string);
            uint64_t seconds = static_cast<uint64_t>(posix::to_time_t(parsed_time));
            auto tod = parsed_time.time_of_day();
            auto tick_seconds = tod.total_seconds() * tod.ticks_per_second();
            auto frac_ticks = tod.ticks() - tick_seconds;
            double frac_seconds = static_cast<double>(frac_ticks) / tod.ticks_per_second();
            tag.value = pmt::make_tuple(pmt::mp(seconds), pmt::mp(frac_seconds));
          } else {
            tag.value = ns.get(key);
          }

          d_tags_to_output.push_back(tag);
        }
      }
    }

    bool
    tag_offset_compare(const tag_t &a, const tag_t &b)
    {
      return a.offset < b.offset;
    }

    void
    source_impl::build_tag_list()
    {
      // Add tags to the send queue from both captures and annotations
      add_tags_from_meta_list(d_captures);
      add_tags_from_meta_list(d_annotations);

      // Sort the tags by their offsets
      std::sort(d_tags_to_output.begin(), d_tags_to_output.end(), tag_offset_compare);

      if(d_debug) {
        std::cout << "tags to out: \n";
        for(size_t i = 0; i < d_tags_to_output.size(); i++) {
          std::cout << "key = " << d_tags_to_output[i].key << ", ";
          std::cout << "val = " << d_tags_to_output[i].value << ", ";
          std::cout << "offset = " << d_tags_to_output[i].offset << ", ";
          std::cout << std::endl;
        }
        std::cout << "End of tags to output" << std::endl;
      }

      // And set the output index
      d_next_tag_index = 0;
    }

    void
    source_impl::load_metadata()
    {
      metafile_namespaces ns = load_metafile(d_meta_fp);
      d_global = ns.global;
      d_captures = ns.captures;
      d_annotations = ns.annotations;

      if(d_debug) {
        std::cout << "num captures: " << d_captures.size() << std::endl;
        for(size_t i = 0; i < d_captures.size(); i++) {
          d_captures[i].print();
        }
      }

      build_tag_list();
    }

    bool
    source_impl::open()
    {
      // hold mutex for duration of this function
      gr::thread::scoped_lock guard(d_open_mutex);

      d_data_fp = fopen(d_data_path.c_str(), "r");
      if(d_data_fp == NULL) {
        std::stringstream s;
        s << "failed to open data file, errno = " << errno << std::endl;
        throw std::runtime_error(s.str());
      }
      d_meta_fp = fopen(d_meta_path.c_str(), "r");
      if(d_meta_fp == NULL) {
        std::stringstream s;
        s << "failed to open meta file, errno = " << errno << std::endl;
        throw std::runtime_error(s.str());
      }

      return true;
    }

    void
    source_impl::set_begin_tag(pmt::pmt_t tag)
    {
      d_add_begin_tag = tag;
    }

    gr::sigmf::meta_namespace &
    source_impl::global_meta()
    {
      return d_global;
    }

    std::vector<gr::sigmf::meta_namespace> &
    source_impl::capture_segments()
    {
      return d_captures;
    }

    int
    source_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      char *output_buf = static_cast<char *>(output_items[0]);
      int items_read;

      // This is in samples
      int size = noutput_items;

      // This is in base units
      int base_size = size * d_num_samps_to_base;

      uint64_t window_start = nitems_written(0);

      if(d_debug) {
        std::cout << "window_start: " << window_start << std::endl;
      }

      uint64_t window_end = window_start + size;
      for(size_t tag_index = d_next_tag_index; tag_index < d_tags_to_output.size(); tag_index++) {
        if(d_tags_to_output[tag_index].offset >= window_start &&
           d_tags_to_output[tag_index].offset < window_end) {

          // Update the offset since the file may have repeated
          tag_t tag_to_add = d_tags_to_output[tag_index];
          tag_to_add.offset = tag_to_add.offset + (d_num_samples_in_file * d_repeat_count);

          if(d_debug) {
            std::cout << "Adding a tag\n";
            std::cout << "key = " << tag_to_add.key << std::endl;
            std::cout << "val = " << tag_to_add.value << std::endl;
            std::cout << "offset = " << tag_to_add.offset << std::endl;
          }

          add_item_tag(0, tag_to_add);

        } else {
          if(tag_index == d_tags_to_output.size() - 1) {
            d_next_tag_index = 0;
          } else {
            d_next_tag_index = tag_index;
          }
          break;
        }
      }

      while(base_size > 0) {

        // Add stream tag whenever the file starts again
        if(d_file_begin) {
          if(d_add_begin_tag != pmt::PMT_NIL) {
            add_item_tag(0, window_start + noutput_items - (base_size / d_num_samps_to_base),
                         d_add_begin_tag, pmt::from_long(d_repeat_count), d_id);
          }
          pmt::pmt_t msg = d_global.get();
          message_port_pub(META, msg);
          // Check if the first capture segment starts at 0 or not
          // NOTE: this may change if the sigmf spec changes
          pmt::pmt_t first_capture_start_position = d_captures[0].get("core:sample_start");
          uint64_t offset_samples = pmt::to_uint64(first_capture_start_position);
          uint64_t offset_bytes = offset_samples * d_sample_size; 
          // If we ever do this when d_data_fp isn't at 0, something is wrong
          assert(std::ftell(d_data_fp) == 0);
          std::fseek(d_data_fp, offset_bytes, SEEK_SET);
          d_file_begin = false;
        }

        // Read as many items as possible
        items_read = d_convert_func(output_buf, d_input_size, base_size, d_data_fp);
        base_size -= items_read;

        // advance output pointer
        output_buf += items_read * d_base_size;

        if(base_size == 0) {
          break;
        }

        // short read, try again
        if(items_read > 0) {
          continue;
        }

        if(!d_repeat) {
          break;
        }

        if(std::fseek((FILE *)d_data_fp, 0, SEEK_SET) == -1) {
          std::fprintf(stderr, "[%s] fseek failed\n", __FILE__);
        }
        if(d_add_begin_tag != pmt::PMT_NIL) {
          d_repeat_count++;
        }
        d_file_begin = true;
      }

      // EOF or error
      if(base_size > 0) {

        // we didn't read anything; say we're done
        if((base_size / d_num_samps_to_base) == noutput_items) {
          return -1;
        }

        // else return partial result
        else {
          return noutput_items - (base_size / d_num_samps_to_base);
        }
      }

      return noutput_items;
    }

  } /* namespace sigmf */
} /* namespace gr */
