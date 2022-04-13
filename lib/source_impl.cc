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
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <gnuradio/io_signature.h>
#include "sigmf/sigmf_utils.h"
#include "source_impl.h"
#include "type_converter.h"
#include "reader_utils.h"
#include "tag_keys.h"

namespace posix = boost::posix_time;
namespace algo = boost::algorithm;

#define D(v) std::cout << __LINE__ << " " <<  #v << " = " << v << std::endl;

namespace gr {
  namespace sigmf {

    source::sptr
    source::make(std::string filename, std::string type, bool repeat)
    {
      return gnuradio::get_initial_sptr(new source_impl(filename, type, repeat));
    }

    source::sptr
    source::make_no_datatype(std::string filename, bool repeat)
    {
      return gnuradio::get_initial_sptr(new source_impl(filename, "", repeat));
    }

    /*
     * The private constructor
     */
    source_impl::source_impl(std::string filename, std::string type, bool repeat)
    : gr::sync_block("source",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(float))), // This get's overwritten below
      d_data_fp(0), d_meta_fp(0), d_repeat(repeat), d_file_begin(true),
      d_add_begin_tag(pmt::PMT_NIL), d_repeat_count(0),
      d_data_path(to_data_path(filename)), d_meta_path(meta_path_from_data(d_data_path))
    {

      // command message port
      message_port_register_in(COMMAND);
      set_msg_handler(COMMAND, [this](pmt::pmt_t msg) { this->on_command_message(msg); });

      // metadata message port
      message_port_register_out(META);

      open();
      std::string input_datatype = d_global.get_str("core:datatype");
      if (type == "") {
        type = input_datatype;
      }

      // Get the input datatype
      format_detail_t input_detail = parse_format_str(input_datatype);

      // and output datatype
      format_detail_t output_detail = parse_format_str(type);

      // Need to divide by 8 to convert to bytes
      d_output_sample_size_bytes = (output_detail.width * (output_detail.is_complex ? 2 : 1)) / 8;

      d_output_num_samps_to_base = output_detail.is_complex ? 2 : 1;
      d_output_base_size = output_detail.width / 8;
      d_input_base_size = input_detail.width / 8;

      d_input_file_sample_size_bytes = (input_detail.width * (input_detail.is_complex ? 2 : 1)) / 8;

      std::fseek(d_data_fp, 0, SEEK_END);
      d_num_samples_in_file = std::ftell(d_data_fp) / d_input_file_sample_size_bytes;
      D(d_num_samples_in_file);
      

      // GR_LOG_DEBUG(d_logger, "Samps in file: " << d_num_samples_in_file);

      std::fseek(d_data_fp, 0, SEEK_SET);

      d_num_channels = d_global.get_uint64_t("core:num_channels", 1);
      set_output_signature(gr::io_signature::make(1, d_num_channels, d_output_sample_size_bytes));

      if (d_num_channels == 1) {
        d_work_func = std::bind(&source_impl::single_channel_work, this, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3);
      } else {
        d_work_func = std::bind(&source_impl::multi_channel_work, this, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3);
      }

      d_output_bufs.resize(d_num_channels);

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
        GR_LOG_WARN(d_logger, boost::format("Command message is not a dict: %s") % msg);
        return;
      }
      pmt::pmt_t command_pmt = pmt::dict_ref(msg, COMMAND, pmt::get_PMT_NIL());
      if(pmt::eqv(command_pmt, pmt::get_PMT_NIL())) {
        GR_LOG_WARN(d_logger, boost::format("Command key not found in dict: %s") % msg);
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

      GR_LOG_DEBUG(d_logger, "Received command message");
    }


    void
    source_impl::add_tags_from_meta_list(const std::vector<meta_namespace> &meta_list, uint64_t shift_amount)
    {
      for(std::vector<meta_namespace>::const_iterator it = meta_list.begin();
          it != meta_list.end(); it++) {
        meta_namespace ns = *it;
        uint64_t offset;
        std::set<std::string> capture_keys = ns.keys();

        if(capture_keys.count("core:sample_start")) {
          offset = pmt::to_uint64(ns.get("core:sample_start"));
          offset -= shift_amount;
          // remove this key, we don't need it as a tag later
          capture_keys.erase("core:sample_start");

        } else {
          throw std::runtime_error(
            "Invalid metadata, no core:sample_start found for segment");
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
          } else if (algo::starts_with(key, UNKNOWN_PREFIX)) {
            std::string unknown_key = key.substr(UNKNOWN_PREFIX.length());
            tag.key = pmt::mp(unknown_key);
          } else {
            tag.key = pmt::mp(key);
          }
          if (key == "core:datetime") {
            std::string iso_string = pmt::symbol_to_string(ns.get(key));
            posix::ptime parsed_time = reader_utils::iso_string_to_ptime(iso_string);
            tag.value = reader_utils::ptime_to_uhd_time(parsed_time);
          } else {
            tag.value = ns.get(key);
          }
          d_tags_to_output.insert({offset, tag});
        }
      }
    }

    void source_impl::add_global_tags(const meta_namespace &global_segment) {
      if (global_segment.has("core:sample_rate")) {
          tag_t tag;
          tag.offset = 0;
          tag.key = RATE_KEY;
          tag.value = global_segment.get("core:sample_rate");
          d_tags_to_output.insert({0, tag});
      }
    }

    void
    source_impl::build_tag_list()
    {
      // Add known tags from the global object
      add_global_tags(d_global);

      uint64_t offset = 0;
      if (d_captures.size() > 0) {
        offset = pmt::to_uint64(d_captures[0].get("core:sample_start"));
      }

      // Add tags to the send queue from both captures and annotations
      add_tags_from_meta_list(d_captures, offset);
      add_tags_from_meta_list(d_annotations, offset);

      GR_LOG_DEBUG(d_logger, "tags to output: ");
      for(auto it = d_tags_to_output.begin(); it != d_tags_to_output.end(); it++) {
        auto key = it->first;
        auto tag_to_output = it->second;
        // GR_LOG_DEBUG(d_logger, "key = " << key << ", ");
        // GR_LOG_DEBUG(d_logger, "val = " << tag_to_output.value << ", ");
        // GR_LOG_DEBUG(d_logger, "offset = " << tag_to_output.offset << ", ");
      }
      GR_LOG_DEBUG(d_logger, "End of tags to output");
    }

    void
    source_impl::load_metadata()
    {
      metafile_namespaces ns = load_metafile(d_meta_fp);
      d_global = ns.global;
      d_captures = ns.captures;
      d_annotations = ns.annotations;

      build_tag_list();
    }

    bool
    source_impl::open()
    {
      // hold mutex for duration of this function
      gr::thread::scoped_lock guard(d_open_mutex);

      D(d_meta_path.string());
      d_meta_fp = fopen(d_meta_path.c_str(), "r");
      if(d_meta_fp == NULL) {
        std::stringstream s;
        s << "failed to open meta file, errno = " << errno << std::endl;
        throw std::runtime_error(s.str());
      }

      load_metadata();
      if (d_global.has("metadata_only")) {
        throw std::runtime_error(d_meta_path.string() + " is a metadata only dataset, nothing to stream!");
      }
      d_data_fp = fopen(d_data_path.c_str(), "r");
      if(d_data_fp == NULL) {
        std::stringstream s;
        s << "failed to open data file, errno = " << errno << std::endl;
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

    void
    source_impl::emit_tags(uint64_t start_offset_abs, int length) {
      // how much window we have left to send out tags for
      int window_remaining = length;
      // where we are starting to get tags from
      uint64_t start = start_offset_abs;
      while(window_remaining > 0) {
        // The lower bound for tags
        uint64_t tag_start = start % d_num_samples_in_file;
        // amount to adjust tag offsets by
        uint64_t offset_adjust = start - tag_start;
        // distance to the end of the file from where the current tags started
        uint64_t distance_to_file_end = (d_num_samples_in_file - tag_start);
        // the size of the chunk of the window that we are getting tags for
        uint64_t window_chunk = std::min(distance_to_file_end, static_cast<uint64_t>(window_remaining));
        // Upper bound for tags
        uint64_t tag_end = tag_start + window_chunk;
        auto start_it = d_tags_to_output.lower_bound(tag_start);
        auto end_it = d_tags_to_output.upper_bound(tag_end);
        for(auto it = start_it; it != end_it; it++) {
          auto tag_to_output = it->second;
          tag_to_output.offset = tag_to_output.offset + offset_adjust;
          add_item_tag(0, tag_to_output);
        }
        window_remaining -= window_chunk;
        start += window_chunk;
      }
    }

    int
    source_impl::multi_channel_work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      D(noutput_items);
      GR_LOG_DEBUG(d_logger, "MULTI!!");
      // Copy the output bufs for convenience
      for(size_t i = 0; i < d_num_channels; i++) {
        d_output_bufs[i] = static_cast<char *>(output_items[i]);
      }

      char *output_buf = static_cast<char *>(output_items[0]);
      int items_read;

      // This is in samples
      int output_size_samples = noutput_items;

      // This is in base units
      int output_base_remaining = output_size_samples * d_output_num_samps_to_base;

      uint64_t start_offset_abs = nitems_written(0);

      emit_tags(start_offset_abs, output_size_samples);
      // TODO: This should be done smareter so we aren't calling malloc on every work call, but that's an optimization for later
      d_multichannel_deinterlace_buffer = static_cast<char*>(std::malloc(output_size_samples * d_input_file_sample_size_bytes * d_num_channels));

      while(output_base_remaining > 0) {

        // Add stream tag whenever the file starts again
        if(d_file_begin) {
          if(d_add_begin_tag != pmt::PMT_NIL) {
            add_item_tag(0, start_offset_abs + noutput_items - (output_base_remaining / d_output_num_samps_to_base),
                         d_add_begin_tag, pmt::from_long(d_repeat_count), d_id);
          }
          pmt::pmt_t msg = d_global.get();
          message_port_pub(META, msg);
          // Check if the first capture segment starts at 0 or not
          // NOTE: this may change if the sigmf spec changes
          pmt::pmt_t first_capture_start_position = d_captures[0].get("core:sample_start");
          uint64_t offset_samples = pmt::to_uint64(first_capture_start_position);
          uint64_t offset_bytes = offset_samples * d_output_sample_size_bytes;
          // If we ever do this when d_data_fp isn't at 0, something is wrong
          assert(std::ftell(d_data_fp) == 0);
          std::fseek(d_data_fp, offset_bytes, SEEK_SET);
          d_file_begin = false;
        }

        // Read input samples from file * the number of output bufs, recording the number of samples we actually read
        // int samples_read = std::fread(d_multichannel_deinterlace_buffer, d_input_file_sample_size_bytes,
        //                               output_size * d_num_channels, d_data_fp);

        // // Deinterlace the input into some contiguous buffers so we can do conversion
        // for(size_t i = 0; i < samples_read; i++) {
        //   int target_deint_buf = i % d_num_channels;
        //   int target_deint_index = i / d_num_channels;
        //   std::memcpy(d_deinterlaced_bufs[target_deint_buf][target_deint_index], bytes_from_file[i * d_input_size_bytes], d_input_size_bytes);
        // }

        // // Do the conversion from the input to output
        // for(size_t channel_num = 0; channel_num < d_num_channels; channel_num++) {
        //   d_convert_func(d_output_bufs[channel_num], d_input_size, base_size, d_deinterlaced_bufs[channel_num]);
        //   // advance output pointers
        //   d_output_bufs[channel_num] += samples_read * d_output_base_size_bytes * d_output_num_samps_to_bytes;
        // }
        // output_base_size -= samples_read * d_input_num_samps_to_base;

	D(output_base_remaining);
        // Read as many items as possible and convert them before deinterlacing
        items_read =
          d_convert_func(d_multichannel_deinterlace_buffer, d_input_base_size, output_base_remaining, d_data_fp);

	std::cout << "items read: " << items_read << std::endl;
        output_base_remaining -= items_read;
        for(size_t item_index = 0; item_index < items_read; item_index++) {
          int target_output_buf = item_index % d_num_channels;
          int target_output_index = item_index / d_num_channels;
          std::memcpy(d_output_bufs[target_output_buf], &d_multichannel_deinterlace_buffer[item_index * d_output_sample_size_bytes], d_output_sample_size_bytes);
          d_output_bufs[target_output_buf] += d_output_sample_size_bytes;
        }

        // advance output pointer
        output_buf += items_read * d_output_base_size;

        if(output_base_remaining == 0) {
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
        d_repeat_count++;
        d_file_begin = true;
      }

      // EOF or error
      if(output_base_remaining > 0) {

        // we didn't read anything; say we're done
        if((output_base_remaining / d_output_num_samps_to_base) == noutput_items) {
          return -1;
        }

        // else return partial result
        else {
	  D(noutput_items - (output_base_remaining / d_output_num_samps_to_base));

          return (noutput_items - (output_base_remaining / d_output_num_samps_to_base)) / d_num_channels;
        }
      }
      D(noutput_items);
      return noutput_items / d_num_channels;
    }

    int
    source_impl::single_channel_work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {

      GR_LOG_DEBUG(d_logger, "SINGLE!!");
      char *output_buf = static_cast<char *>(output_items[0]);
      int items_read;

      // This is in samples
      int size = noutput_items;

      // This is in base units
      int base_size = size * d_output_num_samps_to_base;

      uint64_t start_offset_abs = nitems_written(0);

      emit_tags(start_offset_abs, size);

      while(base_size > 0) {

        // Add stream tag whenever the file starts again
        if(d_file_begin) {
          if(d_add_begin_tag != pmt::PMT_NIL) {
            add_item_tag(0, start_offset_abs + noutput_items - (base_size / d_output_num_samps_to_base),
                         d_add_begin_tag, pmt::from_long(d_repeat_count), d_id);
          }
          pmt::pmt_t msg = d_global.get();
          message_port_pub(META, msg);
          // Check if the first capture segment starts at 0 or not
          // NOTE: this may change if the sigmf spec changes
          pmt::pmt_t first_capture_start_position = d_captures[0].get("core:sample_start");
          uint64_t offset_samples = pmt::to_uint64(first_capture_start_position);
          uint64_t offset_bytes = offset_samples * d_output_sample_size_bytes;
          // If we ever do this when d_data_fp isn't at 0, something is wrong
          assert(std::ftell(d_data_fp) == 0);
          std::fseek(d_data_fp, offset_bytes, SEEK_SET);
          d_file_begin = false;
        }

        // Read as many items as possible
        items_read = d_convert_func(output_buf, d_input_base_size, base_size, d_data_fp);
        base_size -= items_read;

        // advance output pointer
        output_buf += items_read * d_output_base_size;

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
        d_repeat_count++;
        d_file_begin = true;
      }

      // EOF or error
      if(base_size > 0) {

        // we didn't read anything; say we're done
        if((base_size / d_output_num_samps_to_base) == noutput_items) {
          return -1;
        }

        // else return partial result
        else {
          return noutput_items - (base_size / d_output_num_samps_to_base);
        }
      }

      return noutput_items;
    }

    int
    source_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      return d_work_func(noutput_items, input_items, output_items);
    }

  } /* namespace sigmf */
} /* namespace gr */
