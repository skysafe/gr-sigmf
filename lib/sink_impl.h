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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>

#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include "resizable_buffer.h"

#include <sigmf/meta_namespace.h>
#include <sigmf/sink.h>

namespace gr {
  namespace sigmf {

    static const pmt::pmt_t FILENAME_KEY = pmt::string_to_symbol("filename");

    static const pmt::pmt_t META = pmt::string_to_symbol("meta");
    static const pmt::pmt_t COMMAND = pmt::string_to_symbol("command");
    static const pmt::pmt_t GPS = pmt::string_to_symbol("gps");
    static const pmt::pmt_t SYSTEM = pmt::string_to_symbol("system");
    static const pmt::pmt_t LATITUDE = pmt::string_to_symbol("latitude");
    static const pmt::pmt_t LONGITUDE = pmt::string_to_symbol("longitude");
    static const pmt::pmt_t ALTITUDE = pmt::string_to_symbol("altitude");

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

      // True if the metadata for the current file has been written
      bool d_meta_written = false;

      // Number of incoming channels in file
      int d_num_channels;

      // Convenience vector for input buffers
      std::vector<char*> d_input_bufs;

      resizable_byte_buffer_t d_interlaced_buffer;

      // The offset of the start of the current recording from
      // what the block believes
      uint64_t d_recording_start_offset;

      format_detail_t d_format_detail;

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

      std::filesystem::path d_data_path;
      std::filesystem::path d_temp_data_path;
      std::filesystem::path d_meta_path;

      std::filesystem::path d_new_data_path;
      std::filesystem::path d_new_temp_data_path;
      std::filesystem::path d_new_meta_path;

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

      // time mode for sink
      sigmf_time_mode d_sink_time_mode;

      bool d_is_first_sample = true;

      boost::posix_time::ptime d_relative_start_ts;
      pmt::pmt_t d_relative_time_at_start = pmt::get_PMT_NIL();

      std::filesystem::path convert_to_temp_path(const std::filesystem::path &path);
      std::string generate_random_string(size_t length);

      std::string add_endianness(const std::string &type);
      void reset_meta();
      void init_meta();

      void on_command_message(pmt::pmt_t msg);
      void on_gps_message(pmt::pmt_t msg);

      void write_meta();
      void move_temp_to_final();

      void handle_uhd_tag(const tag_t *tag, meta_namespace &capture_segment);
      void capture_segment_from_tags(const std::vector<tag_t> &tags);
      void handle_tags(const std::vector<tag_t> &tags);
      void handle_tags_not_capturing(const std::vector<tag_t> &tags);
      void do_update();

      std::string check_dtype_endianness(std::string dtype);

      std::string iso_8601_ts();
      std::string convert_uhd_time_to_iso8601(pmt::pmt_t uhd_time);
      std::string convert_full_fracs_pair_to_iso8601(uint64_t seconds, double frac_seconds);

      void set_geolocation(pmt::pmt_t lon, pmt::pmt_t lat, pmt::pmt_t alt);

      void close_impl();

      public:
      sink_impl(std::string type, std::string filename, sigmf_time_mode time_mode, bool append, int num_channels);
      ~sink_impl();

      void open(const char *filename);

      void open(const std::string &filename);

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
