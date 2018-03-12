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

#ifndef INCLUDED_SIGMF_SINK_IMPL_H
#define INCLUDED_SIGMF_SINK_IMPL_H

#include <boost/filesystem/path.hpp>
#include <boost/thread/mutex.hpp>

#include <string>
#include <vector>
#include <unordered_map>

#include <sigmf/meta_namespace.h>
#include <sigmf/sink.h>

namespace gr {
  namespace sigmf {

    static const pmt::pmt_t TIME_KEY = pmt::string_to_symbol("rx_time");
    static const pmt::pmt_t RATE_KEY = pmt::string_to_symbol("rx_rate");
    static const pmt::pmt_t FREQ_KEY = pmt::string_to_symbol("rx_freq");
    static const pmt::pmt_t PACKET_LEN_KEY = pmt::string_to_symbol("packet_len");
    static const pmt::pmt_t FILENAME_KEY = pmt::string_to_symbol("filename");

    static const pmt::pmt_t META = pmt::string_to_symbol("meta");
    static const pmt::pmt_t COMMAND = pmt::string_to_symbol("command");

    static const char *SIGMF_VERSION = "0.0.1";

    inline size_t
    type_to_size(const std::string type)
    {
      size_t underscore_pos = type.find("_");
      std::string type_minus_endianness = type.substr(0, underscore_pos);
      if(type_minus_endianness == "cf64") {
        return 16;
      } else if(type_minus_endianness == "cf32") {
        return 8;
      } else if(type_minus_endianness == "ci64") {
        return 16;
      } else if(type_minus_endianness == "ci32") {
        return 8;
      } else if(type_minus_endianness == "ci16") {
        return 4;
      } else if(type_minus_endianness == "ci8") {
        return 2;
      } else if(type_minus_endianness == "rf64") {
        return 8;
      } else if(type_minus_endianness == "rf32") {
        return 4;
      } else if(type_minus_endianness == "ri64") {
        return 8;
      } else if(type_minus_endianness == "ri32") {
        return 4;
      } else if(type_minus_endianness == "ri16") {
        return 2;
      } else if(type_minus_endianness == "ri8") {
        return 1;
      } else if(type_minus_endianness == "cu32") {
        return 8;
      } else if(type_minus_endianness == "cu16") {
        return 4;
      } else if(type_minus_endianness == "ru32") {
        return 4;
      } else if(type_minus_endianness == "ru16") {
        return 2;
      } else {
        std::stringstream s;
        s << "unknown sigmf type " << type << std::endl;
        throw std::runtime_error(s.str());
      }
    }

    inline std::pair<uint64_t, double>
    extract_uhd_time(pmt::pmt_t uhd_time) {
        uint64_t seconds = pmt::to_uint64(pmt::tuple_ref(uhd_time, 0));
        double frac_seconds = pmt::to_double(pmt::tuple_ref(uhd_time, 1));
        return std::make_pair(seconds, frac_seconds);
    }


    class sink_impl : public sink {
      private:
      // current data FILE*
      FILE *d_fp;

      // Replacement data FILE*
      FILE *d_new_fp;

      // True if file should be appended to
      bool d_append;

      // True if there is a new fp to switch to
      bool d_updated;


      bool d_debug;

      // True if the metadata for the current file has been written
      bool d_meta_written;

      // The offset of the start of the current recording from
      // what the block believes 
      uint64_t d_recording_start_offset;

      boost::mutex d_mutex;
      size_t d_itemsize;
      std::vector<tag_t> d_temp_tags;

      // Base type, not full format specifier. We need endianness for that.
      std::string d_type;

      // Stored basic global metadata, we'll need these
      double d_samp_rate;
      std::string d_description;
      std::string d_author;
      std::string d_license;
      std::string d_hardware;

      boost::filesystem::path d_data_path;
      boost::filesystem::path d_meta_path;

      boost::filesystem::path d_new_data_path;
      boost::filesystem::path d_new_meta_path;

      // Note that samp_rate is needed for timekeeping as well, since we might
      // have to start a new capture segment without having a corresponding
      // timestamp.
      meta_namespace d_global;
      std::vector<meta_namespace> d_captures;
      std::vector<meta_namespace> d_annotations;

      pmt::pmt_t d_pre_capture_data = pmt::make_dict();
      // A map of pre_capture_data keys to the sample index of the
      // tag that they were attached to
      std::unordered_map<std::string, uint64_t> d_pre_capture_tag_index;

      void reset_meta();
      void init_meta();

      void on_command_message(pmt::pmt_t msg);

      void write_meta();

      void handle_uhd_tag(const tag_t *tag, meta_namespace &capture_segment);
      void capture_segment_from_tags(const std::vector<tag_t> &tags);
      void handle_tags(const std::vector<tag_t> &tags);
      void handle_tags_not_capturing(const std::vector<tag_t> &tags);
      void do_update();

      std::string iso_8601_ts();
      std::string convert_uhd_time_to_iso8601(pmt::pmt_t uhd_time);

      public:
      sink_impl(std::string type,
                std::string filename,
                bool append,
                bool debug);
      ~sink_impl();

      bool open(const char *filename);

      bool open(const std::string &filename);

      void close();

      std::string get_data_path();
      std::string get_meta_path();

      void set_global_meta(const std::string &key, pmt::pmt_t val);
      void set_global_meta(const std::string &key, double val);
      void set_global_meta(const std::string &key, int64_t val);
      void set_global_meta(const std::string &key, uint64_t val);
      void set_global_meta(const std::string &key, const std::string &val);
      void set_global_meta(const std::string &key, bool val);

      void set_annotation_meta(uint64_t sample_start,
                                       uint64_t sample_count,
                                       std::string key,
                                       pmt::pmt_t val);

      void set_capture_meta(uint64_t index, std::string key, pmt::pmt_t val);

      // Where all the action really happens
      int work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);

      bool stop();
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_SINK_IMPL_H */
