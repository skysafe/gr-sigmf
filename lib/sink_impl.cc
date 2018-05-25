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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/endian/conversion.hpp>
#include <random>
#include <algorithm>
#include <fcntl.h>
#include <gnuradio/io_signature.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include "sigmf/sigmf_utils.h"
#include "tag_keys.h"
#include "writer_utils.h"
#include "pmt_utils.h"
#include "sink_impl.h"

// win32 (mingw/msvc) specific
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef O_BINARY
#define OUR_O_BINARY O_BINARY
#else
#define OUR_O_BINARY 0
#endif

// should be handled via configure
#ifdef O_LARGEFILE
#define OUR_O_LARGEFILE O_LARGEFILE
#else
#define OUR_O_LARGEFILE 0
#endif

#define PVAR(v) std::cout << #v << " = " << v << std::endl;

namespace fs = boost::filesystem;
namespace posix = boost::posix_time;
namespace greg = boost::gregorian;
namespace endian = boost::endian;
namespace algo = boost::algorithm;

namespace gr {
  namespace sigmf {

    sink::sptr
    sink::make(std::string type,
               std::string filename,
               sigmf_time_mode time_mode,
               bool append)
    {
      return gnuradio::get_initial_sptr(new sink_impl(type, filename, time_mode, append));
    }

    /*
     * The private constructor
     */
    sink_impl::sink_impl(std::string type,
                         std::string filename,
                         sigmf_time_mode time_mode,
                         bool append)
    : gr::sync_block("sink",
                     gr::io_signature::make(1, 1, type_to_size(type)),
                     gr::io_signature::make(0, 0, 0)),
      d_fp(nullptr), d_new_fp(nullptr), d_append(append), d_itemsize(type_to_size(type)),
      d_type(add_endianness(type)), d_sink_time_mode(time_mode)
    {
      init_meta();
      open(filename.c_str());
      d_temp_tags.reserve(32);

      // command message port
      message_port_register_in(COMMAND);
      set_msg_handler(COMMAND, boost::bind(&sink_impl::on_command_message, this, _1));
      message_port_register_in(GPS);
      set_msg_handler(GPS, boost::bind(&sink_impl::on_gps_message, this, _1));

      message_port_register_out(SYSTEM);
    }

    /*
     * Our virtual destructor.
     */
    sink_impl::~sink_impl()
    {
    }

    std::string
    sink_impl::add_endianness(const std::string &type) {
      std::string correct_ending;
      std::string incorrect_ending;
      if (endian::order::native == endian::order::big) {
        correct_ending = "_be";
        incorrect_ending = "_le";
      } else {
        correct_ending = "_le";
        incorrect_ending = "_be";
      }
      if (algo::ends_with(type, correct_ending)) {
        return type;
      } else if (algo::ends_with(type, incorrect_ending)) {
        throw std::invalid_argument(
          "endianness of type does not match system endianness");
      } else {
        return type + correct_ending;
      }
    }

    std::string
    sink_impl::generate_random_string(size_t length) {
      std::array<char, 62> char_set = { '0', '1', '2', '3', '4', '5', '6', '7', '8',
                                        '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
                                        'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                                        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
                                        'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                                        's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };
      std::default_random_engine rng(std::random_device{}());
      std::uniform_int_distribution<> dist(0, char_set.size() - 1);
      auto rand_char = [&char_set, &dist, &rng]() { return char_set[dist(rng)]; };
      std::string rand_string(length, 0);
      std::generate_n(rand_string.begin(), length, rand_char);
      return rand_string;
    }

    boost::filesystem::path
    sink_impl::convert_to_temp_path(const boost::filesystem::path &path){
      auto filename = path.filename();
      auto file_dir = path.parent_path();
      std::string filename_str = filename.native();
      std::string random_string = generate_random_string(16);
      std::string new_filename = ".temp-" + random_string + "-" + filename_str;
      return file_dir / new_filename;
    }

    void
    sink_impl::move_temp_to_final() {
      fs::rename(d_temp_data_path, d_data_path);
    }

    bool
    sink_impl::stop() {
      close();

      if (d_fp) {
        std::fclose(d_fp);
        write_meta();
        move_temp_to_final();
        d_fp = nullptr;
      }

      return true;
    }

    std::string
    sink_impl::iso_8601_ts() {
      posix::ptime t = posix::microsec_clock::universal_time();
      return posix::to_iso_extended_string(t) + "Z";
    }

    void 
    sink_impl::init_meta() {
      reset_meta();
      d_captures.push_back(meta_namespace::build_capture_segment(0));
    }

    void
    sink_impl::reset_meta() {
      // std::cout << "reset_meta()" << std::endl;

      pmt::pmt_t samp_rate = d_global.get("core:sample_rate", pmt::get_PMT_NIL());
      pmt::pmt_t description = d_global.get("core:description",pmt::get_PMT_NIL());
      pmt::pmt_t author = d_global.get("core:author", pmt::get_PMT_NIL());
      pmt::pmt_t license = d_global.get("core:license",pmt::get_PMT_NIL());
      pmt::pmt_t hw = d_global.get("core:hw", pmt::get_PMT_NIL());

      d_global = meta_namespace::build_global_object(d_type);
      if (!pmt::eqv(pmt::get_PMT_NIL(), samp_rate)) {
        d_global.set("core:sample_rate", samp_rate);
      }
      if (!pmt::eqv(pmt::get_PMT_NIL(), description)) {
        d_global.set("core:description", description);
      }
      if (!pmt::eqv(pmt::get_PMT_NIL(), author)) {
        d_global.set("core:author", author);
      }
      if (!pmt::eqv(pmt::get_PMT_NIL(), license)) {
        d_global.set("core:license", license);
      }
      if (!pmt::eqv(pmt::get_PMT_NIL(), hw)) {
        d_global.set("core:hw", hw);
      }
      d_annotations.clear();
      // We don't clear the captures here, as there is some extra
      // work that must be done to avoid data loss since captures
      // apply to every sample going forward
  }

    void
    sink_impl::on_command_message(pmt::pmt_t msg)
    {
      if(!pmt::is_dict(msg)) {
        GR_LOG_ERROR(d_logger, boost::format("Command message is not a dict: %s") % msg);
        return;
      }

      // Look for a command
      pmt::pmt_t command_pmt = pmt::dict_ref(msg, COMMAND, pmt::get_PMT_NIL());
      if(pmt::eqv(command_pmt, pmt::get_PMT_NIL())) {
        GR_LOG_ERROR(d_logger, boost::format("Command key not found in dict: %s") % msg);
        return;
      }
      std::string command_str = pmt::symbol_to_string(command_pmt);

      // Handle each command
      if(command_str == "open") {
        pmt::pmt_t filename_pmt = pmt::dict_ref(msg, FILENAME_KEY, pmt::PMT_NIL);
        if(pmt::is_symbol(filename_pmt)) {
          open(pmt::symbol_to_string(filename_pmt));
        } else {
          GR_LOG_ERROR(d_logger,
                       boost::format("Invalid filename for open command in dict: %s") % msg);
          return;
        }
      } else if(command_str == "close") {
        // We can call the impl and update directly here, since the update handler
        // is always run seperately from the work function
        close_impl();
        do_update();
      } else if(command_str == "set_annotation_meta") {
        // Need to get sample_start, sample_count, key, and value
        auto sample_start = pmt::dict_ref(msg, pmt::mp("sample_start"), pmt::get_PMT_NIL());
        auto sample_count = pmt::dict_ref(msg, pmt::mp("sample_count"), pmt::get_PMT_NIL());
        auto key = pmt::dict_ref(msg, pmt::mp("key"), pmt::get_PMT_NIL());
        auto val = pmt::dict_ref(msg, pmt::mp("val"), pmt::get_PMT_NIL());

        if (pmt::eqv(sample_start, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Sample start key not found in dict: %s") % msg);
          return;
        } else if (pmt::eqv(sample_count, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Sample count key not found in dict: %s") % msg);
          return;
        } else if (pmt::eqv(key, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Data key not found in dict: %s") % msg);
          return;
        }

        set_annotation_meta(pmt::to_uint64(sample_start), pmt::to_uint64(sample_count), pmt::symbol_to_string(key), val);

      } else if(command_str == "set_global_meta") {
        // Just need key and value
        auto key = pmt::dict_ref(msg, pmt::mp("key"), pmt::get_PMT_NIL());
        auto val = pmt::dict_ref(msg, pmt::mp("val"), pmt::get_PMT_NIL());

        if (pmt::eqv(key, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Data key not found in dict: %s") % msg);
          return;
        }

        set_global_meta(pmt::symbol_to_string(key), val);

      } else if (command_str == "set_capture_meta") {

        auto index = pmt::dict_ref(msg, pmt::mp("index"), pmt::get_PMT_NIL());
        auto key = pmt::dict_ref(msg, pmt::mp("key"), pmt::get_PMT_NIL());
        auto val = pmt::dict_ref(msg, pmt::mp("val"), pmt::get_PMT_NIL());

        if (pmt::eqv(key, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Data key not found in dict: %s") % msg);
          return;
        }
        if (pmt::eqv(index, pmt::get_PMT_NIL())) {
          GR_LOG_ERROR(d_logger, boost::format("Index key not found in dict: %s") % msg);
          return;
        }
        uint64_t index_int = pmt::to_uint64(index);
        GR_LOG_INFO(d_logger, "setting capture meta(" << index_int << "," << key << ", " << val << ")");
        set_capture_meta(index_int, pmt::symbol_to_string(key), val);

      }else {
        GR_LOG_ERROR(d_logger,
                     boost::format("Invalid command string received in dict: %s") % msg);
        return;
      }
    }

    void
    sink_impl::on_gps_message(pmt::pmt_t msg)
    {
      // Instant in time that corresponds roughly to where we are now.
      uint64_t sample_start = nitems_read(0);
      uint64_t sample_count = 0;

      if(pmt::dict_has_key(msg, LATITUDE) && pmt::dict_has_key(msg, LONGITUDE)) {
        pmt::pmt_t lat = pmt::dict_ref(msg, LATITUDE, pmt::PMT_NIL);
        pmt::pmt_t lon = pmt::dict_ref(msg, LONGITUDE, pmt::PMT_NIL);
        set_annotation_meta(sample_start, sample_count, "core:latitude", lat);
        set_annotation_meta(sample_start, sample_count, "core:longitude", lon);
        set_annotation_meta(sample_start, sample_count, "core:generator", pmt::string_to_symbol("USRP GPS Message"));
      }
    }

    std::string
    sink_impl::get_data_path()
    {
      if(d_fp) {
        return d_data_path.string();
      } else if(d_new_fp) {
        return d_new_data_path.string();
      }
      return "";
    }

    std::string
    sink_impl::get_meta_path()
    {
      if(d_fp) {
        return d_meta_path.string();
      } else if(d_new_fp) {
        return d_new_meta_path.string();
      }
      return "";
    }

    void
    sink_impl::set_global_meta(const std::string &key, pmt::pmt_t val)
    {
      d_global.set(key, val);
    }
    void
    sink_impl::set_global_meta(const std::string &key, double val)
    {
      d_global.set(key, pmt::from_double(val));
    }

    void
    sink_impl::set_global_meta(const std::string &key, int64_t val)
    {
      d_global.set(key, pmt::from_long(val));
    }

    void
    sink_impl::set_global_meta(const std::string &key, uint64_t val)
    {
      d_global.set(key, pmt::from_uint64(val));
    }

    void
    sink_impl::set_global_meta(const std::string &key, const std::string &val)
    {
      d_global.set(key, pmt::string_to_symbol(val));
    }

    void
    sink_impl::set_global_meta(const std::string &key, bool val)
    {
      d_global.set(key, pmt::from_bool(val));
    }

    void
    sink_impl::set_capture_meta(uint64_t index, std::string key, pmt::pmt_t val)
    {
      // If there's no current fp being written to, then put this in d_pre_capture_data
      if (d_fp == nullptr) {
        d_pre_capture_data = pmt::dict_add(d_pre_capture_data, pmt::mp(key), val);
      } else {
        // otherwise it goes in the relavant capture segment
        try {
          auto &capture = d_captures.at(index);
          capture.set(key, val);
        } catch (const std::out_of_range &e) {
          GR_LOG_ERROR(d_logger, "Invalid capture index");
        }
      }
    }

    void
    sink_impl::set_annotation_meta(uint64_t sample_start, uint64_t sample_count, std::string key, pmt::pmt_t val)
    {
      auto existing_annotation = std::find_if(d_annotations.begin(), d_annotations.end(), [sample_start, sample_count](const meta_namespace &ns) -> bool {
        return ns.has("core:sample_start") &&
          pmt::to_uint64(ns.get("core:sample_start")) == sample_start &&
          ns.has("core:sample_count") &&
          pmt::to_uint64(ns.get("core:sample_count")) == sample_count;
      });
      if (existing_annotation == d_annotations.end()) {
        // then make a new one
        auto new_ns = meta_namespace::build_annotation_segment(sample_start, sample_count);
        new_ns.set(key, val);
        // This may cause the annotations list to become unordered, but we'll make sure we sort it before serialization
        d_annotations.push_back(new_ns);
      } else {
        // use that one
        existing_annotation->set(key, val);
      }
    }

    bool
    sink_impl::open(const std::string &filename)
    {
      return open(filename.c_str());
    }

    bool
    sink_impl::open(const char *filename)
    {
      gr::thread::scoped_lock guard(d_mutex); // hold mutex for duration of this function

      if ((filename != nullptr) && (filename[0] == '\0')) {
        // Then it's empty string and we can just return now
        return false;
      }

      d_new_data_path = to_data_path(filename);
      d_new_temp_data_path = convert_to_temp_path(d_new_data_path);
      d_new_meta_path = meta_path_from_data(d_new_data_path);

      // we use the open system call to get access to the O_LARGEFILE flag.
      int fd;
      int flags;
      if(d_append) {
        flags = O_WRONLY | O_CREAT | O_APPEND | OUR_O_LARGEFILE | OUR_O_BINARY;
      } else {
        flags = O_WRONLY | O_CREAT | O_TRUNC | OUR_O_LARGEFILE | OUR_O_BINARY;
      }
      if((fd = ::open(d_new_temp_data_path.c_str(), flags, 0664)) < 0) {
        GR_LOG_ERROR(d_logger,
                     boost::format("Failed to open file descriptor for path '%s'") % d_new_temp_data_path);
        std::perror(d_new_temp_data_path.c_str());
        return false;
      }

      // if we've already got a new one open, close it
      if(d_new_fp) {
        std::fclose(d_new_fp);
        d_new_fp = 0;
      }

      if((d_new_fp = ::fdopen(fd, "wb")) == NULL) {
        std::perror(d_new_temp_data_path.c_str());

        // don't leak file descriptor if fdopen fails
        ::close(fd);
      }

      d_updated = true;
      return d_new_fp != nullptr;
    }

    void
    sink_impl::do_update()
    {
      if(d_updated) {

        // hold mutex for duration of this block
        gr::thread::scoped_lock guard(d_mutex);

        if(d_fp){
          std::fclose(d_fp);
          write_meta();
          move_temp_to_final();
          reset_meta();
        }

        d_recording_start_offset = nitems_read(0);

        // install new file pointer
        d_fp = d_new_fp;
        d_data_path = d_new_data_path;
        d_temp_data_path = d_new_temp_data_path;
        d_meta_path = d_new_meta_path;
        d_meta_written = d_new_fp == nullptr ? true : false;

        // If a new file has been opened
        if (d_fp != nullptr) {
          // Need to check if we've received any capture
          // metadata in the meantime
          meta_namespace first_segment = meta_namespace::build_capture_segment(0);
          // Iterate through the keys of d_pre_capture_data
          // if any of them match the known keys we need to handle
          // then deal with it
          auto capture_keys = pmt::dict_keys(d_pre_capture_data);
          size_t num_keys = pmt::length(capture_keys);
          for(size_t i = 0; i < num_keys; i++) {
            auto capture_key = pmt::nth(i, capture_keys);
            auto capture_val = pmt::dict_ref(d_pre_capture_data, capture_key, pmt::get_PMT_NIL());

            if (pmt::eqv(capture_key, TIME_KEY)) {
              uint64_t received_sample_index = d_pre_capture_tag_index[pmt::symbol_to_string(capture_key)];
              double current_sample_rate = -1;
              pmt::pmt_t sample_rate_pmt = pmt::dict_ref(d_pre_capture_data, RATE_KEY, pmt::get_PMT_NIL());

              if (pmt::eqv(sample_rate_pmt, pmt::get_PMT_NIL())) {
                // Check if it's in the global segment
                if (d_global.has("core:sample_rate")) {
                  pmt::pmt_t samp_rate_pmt = d_global.get("core:sample_rate");
                  current_sample_rate = pmt::to_double(samp_rate_pmt);
                } 
              } else {
                current_sample_rate = pmt::to_double(sample_rate_pmt);
              }
              // If we found a sample rate in the global segment or in the received data
              // Then we can compute a new time offset
              if (current_sample_rate != -1) {
                uint64_t total_samples_read = nitems_read(0);
                // Use the number of samples read since the last time we got a time
                // combined with the sample rate to compute the new time
                uint64_t samples_since_time_received = total_samples_read - received_sample_index;
                // Compute the full seconds and fractional seconds since we last received a time
                // based on the number of samples and the sample rate
                uint64_t full_seconds_since_time =
                  std::floor(samples_since_time_received / current_sample_rate);
                double frac_seconds_since_time = 
                  (samples_since_time_received / current_sample_rate) - (full_seconds_since_time);

                uint64_t capture_val_full_seconds;
                double capture_val_frac_seconds;
                std::tie(capture_val_full_seconds, capture_val_frac_seconds) =
                  pmt_utils::extract_uhd_time(capture_val);

                // Handle the relative case
                if (d_sink_time_mode == sigmf_time_mode::relative) {
                  // First correct for the offset from the first tag
                  uint64_t start_full_seconds = 0;
                  double start_frac_seconds = 0;
                  if (!pmt::eqv(d_relative_time_at_start, pmt::get_PMT_NIL())) {
                      std::tie(start_full_seconds, start_frac_seconds) =
                        pmt_utils::extract_uhd_time(capture_val);
                  }
                  capture_val_full_seconds = capture_val_full_seconds - start_full_seconds;
                  capture_val_frac_seconds = capture_val_frac_seconds - start_frac_seconds;
                  // Then handle adding in the time recorded in d_relative_start_ts
                  posix::ptime epoch(boost::gregorian::date(1970, 1, 1));
                  auto duration = d_relative_start_ts - epoch;
                  uint64_t seconds_adjust = duration.total_seconds();
                  uint64_t frac_seconds = duration.fractional_seconds();
                  uint64_t ticks_per_second = duration.ticks_per_second();
                  double frac_seconds_adjust = (static_cast<double>(frac_seconds) / ticks_per_second);

                  capture_val_full_seconds += seconds_adjust;
                  capture_val_frac_seconds += frac_seconds_adjust;
                  if (capture_val_frac_seconds >= 1) {
                    capture_val_full_seconds += 1;
                    capture_val_frac_seconds -= 1;
                  }
                }
                // Add the values computed above to the values from the tag to get the current value
                uint64_t final_full_seconds = full_seconds_since_time + capture_val_full_seconds;
                double final_frac_seconds = frac_seconds_since_time + capture_val_frac_seconds;
                if (final_frac_seconds >= 1) {
                  final_full_seconds += 1;
                  final_frac_seconds -= 1;
                }
                std::string final_computed_time =
                  convert_full_fracs_pair_to_iso8601(final_full_seconds, final_frac_seconds);
                first_segment.set("core:datetime", final_computed_time);
              }
            } else if (pmt::eqv(capture_key, FREQ_KEY)) {
              first_segment.set("core:frequency", capture_val);
            } else if (pmt::eqv(capture_key, RATE_KEY)) {
              d_global.set("core:sample_rate", capture_val);
            } else {
              // any other data just goes in the first capture_segment
              first_segment.set(capture_key, capture_val);
            }
          }
          // If no datetime set
          if (!first_segment.has("core:datetime")) {
            // Then set one
            GR_LOG_INFO(d_logger, "No core:datetime found, using host ts instead")
            first_segment.set("core:datetime", iso_8601_ts());
          } 
          // clear pre_capture_data
          d_pre_capture_data = pmt::make_dict();
          d_pre_capture_tag_index.clear();
          d_captures.clear();
          d_captures.push_back(first_segment);
        }

        d_new_fp = nullptr;
        d_updated = false;
      }
    }

    void
    sink_impl::close_impl()
    {
      if(d_new_fp != nullptr) {
        std::fclose(d_new_fp);
        d_new_fp = nullptr;
      }
      d_updated = true;
    }

    void
    sink_impl::close()
    {
      // hold mutex for duration of this function
      gr::thread::scoped_lock guard(d_mutex);
      close_impl();
    }

    void
    sink_impl::write_meta()
    {
      if(d_meta_written) {
        return;
      }

      FILE *fp = std::fopen(d_meta_path.c_str(), "w");
      if (fp == nullptr) {
        std::perror("Error opening d_meta_path");
      }
      writer_utils::write_meta_to_fp(fp, d_global, d_captures, d_annotations);
      std::fclose(fp);
      d_meta_written = true;
    }

    std::string
    sink_impl::convert_full_fracs_pair_to_iso8601(uint64_t seconds, double frac_seconds) {
        time_t secs_time(seconds);
        posix::ptime seconds_since_epoch = posix::from_time_t(secs_time);
        std::string seconds_iso = posix::to_iso_extended_string(seconds_since_epoch);
        std::string frac_seconds_str = boost::lexical_cast<std::string>(frac_seconds);
        boost::replace_all(frac_seconds_str, "0.", ".");
        return seconds_iso + frac_seconds_str + "Z";
    }

    std::string
    sink_impl::convert_uhd_time_to_iso8601(pmt::pmt_t uhd_time) {
        uint64_t seconds = pmt::to_uint64(pmt::tuple_ref(uhd_time, 0));
        double frac_seconds = pmt::to_double(pmt::tuple_ref(uhd_time, 1));
        return convert_full_fracs_pair_to_iso8601(seconds, frac_seconds);
    }

    void
    sink_impl::handle_uhd_tag(const tag_t *tag, meta_namespace &capture_segment)
    {
      if(pmt::eqv(tag->key, TIME_KEY)) {
        switch(d_sink_time_mode) {
          case (sigmf_time_mode::relative):
          {
            // In relative mode, we need to add this to the time we stored 
            // for the first sample received
            // Check if we got a relative time on the first sample
            uint64_t start_full_seconds = 0;
            double start_frac_seconds = 0;
            if (!pmt::eqv(d_relative_time_at_start, pmt::get_PMT_NIL())) {
                std::tie(start_full_seconds, start_frac_seconds) =
                  pmt_utils::extract_uhd_time(d_relative_time_at_start);
            }
            // Subtract initial time offset
            uint64_t tag_full_seconds;
            double tag_frac_seconds;
            std::tie(tag_full_seconds, tag_frac_seconds) = pmt_utils::extract_uhd_time(tag->value);
            tag_full_seconds -= start_full_seconds;
            tag_frac_seconds -= start_frac_seconds;
            // Add tag seconds to initial timestamp
            posix::ptime adjusted_time =
              d_relative_start_ts +
              posix::seconds(tag_full_seconds) + 
              posix::nanoseconds(std::floor(tag_frac_seconds * 1000000000));
            // And set the adjusted time as the new time
            std::string ts_iso = posix::to_iso_extended_string(adjusted_time) + "Z";
            capture_segment.set("core:datetime", ts_iso);
            break;
          }
          case (sigmf_time_mode::absolute):
            // In absolute mode, we store these as is
            capture_segment.set("core:datetime", convert_uhd_time_to_iso8601(tag->value));
            break;
        }
      } else if(pmt::eqv(tag->key, FREQ_KEY)) {
        // frequency as double
        capture_segment.set("core:frequency", tag->value);

      } else if(pmt::eqv(tag->key, RATE_KEY)) {

        // sample_rate as double
        // Sample rate is special, it goes to the global segment
        d_global.set("core:sample_rate", tag->value);

      } else {
        throw std::runtime_error("invalid key in handle_uhd_tag");
      }
    }


    bool
    is_capture_or_global_tag(const tag_t *tag)
    {
      return pmt::eqv(tag->key, TIME_KEY) || pmt::eqv(tag->key, RATE_KEY) ||
        pmt::eqv(tag->key, FREQ_KEY);
    }

    void
    sink_impl::handle_tags(const std::vector<tag_t> &tags)
    {

      typedef std::vector<const tag_t *> tag_ptr_vector_t;
      typedef tag_ptr_vector_t::iterator tag_vec_it;
      typedef std::map<uint64_t, tag_ptr_vector_t> tag_map_t;
      tag_map_t tag_map;
      // Make a map from offset to list of tag pointers
      for(std::vector<tag_t>::const_iterator it = tags.begin(); it != tags.end(); it++) {
        tag_map[(*it).offset].push_back(&(*it));
      }

      for(tag_map_t::iterator it = tag_map.begin(); it != tag_map.end(); it++) {
        uint64_t offset = it->first;
        uint64_t adjusted_offset = offset - d_recording_start_offset;
        tag_vec_it tag_begin = it->second.begin();
        tag_vec_it tag_end = it->second.end();
        // split the list into capture tags and annotation tags
        tag_vec_it annotations_begin = std::partition(tag_begin, it->second.end(), &is_capture_or_global_tag);

        // Handle any capture tags
        if(std::distance(tag_begin, annotations_begin) > 0) {
          pmt::pmt_t most_recent_segment_start =
            d_captures.back().get("core:sample_start");

          // If there's already a segment for this sample index, then use that
          if(offset != pmt::to_uint64(most_recent_segment_start)) {
            // otherwise add a new empty segment
            meta_namespace new_capture;
            d_captures.push_back(new_capture);
          }

          meta_namespace &capture_ns = d_captures.back();
          for(tag_vec_it tag_it = tag_begin; tag_it != annotations_begin; tag_it++) {
            // These tags are handles specially, since they do not
            // go to an annotation segment
            handle_uhd_tag(*tag_it, capture_ns);
          }

          // And add the sample_start for this capture_segment
          capture_ns.set("core:sample_start", offset);
        }

        // handle any annotation tags
        if(std::distance(annotations_begin, tag_end) > 0) {

          // Make an annotation object here
          meta_namespace anno_ns;
          bool found_packet_len = false;
          for(tag_vec_it tag_it = annotations_begin; tag_it != tag_end; tag_it++) {
            // These get added to the annoation object
            if(pmt::eqv((*tag_it)->key, PACKET_LEN_KEY)) {
              found_packet_len = true;
              anno_ns.set("core:sample_count", (*tag_it)->value);
            } else {
              std::string key_str = pmt::symbol_to_string((*tag_it)->key);
              if (meta_namespace::validate_key(key_str)) {
                anno_ns.set((*tag_it)->key, (*tag_it)->value);
              } else {
                std::string unknown_ns_key = "unknown:" + key_str;
                anno_ns.set(unknown_ns_key, (*tag_it)->value);
              }
            }
          }
          if(!found_packet_len) {
            anno_ns.set("core:sample_count", 0);
          }
          anno_ns.set("core:sample_start", adjusted_offset);

          // add the annotation object to the list
          d_annotations.push_back(anno_ns);
        }
      }
    }

    void
    sink_impl::handle_tags_not_capturing(const std::vector<tag_t> &tags) {
      for(auto &tag: tags) {
        if (is_capture_or_global_tag(&tag)) {
          d_pre_capture_data = pmt::dict_add(d_pre_capture_data, tag.key, tag.value);
          d_pre_capture_tag_index[pmt::symbol_to_string(tag.key)] = tag.offset;
        }
      }
    }

    int
    sink_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      char *inbuf = (char *)input_items[0];
      int nwritten = 0;

      // Check if a new fp is here and handle the update if so
      do_update();

      // Stream tags should always get handled, even if d_fp is nullptr
      get_tags_in_window(d_temp_tags, 0, 0, noutput_items);

      if (d_sink_time_mode == sigmf_time_mode::relative && d_is_first_sample) {
        // Use the most accurate system clock to get a timestamp for start
        d_relative_start_ts = posix::microsec_clock::universal_time();
        // Check if we got an rx_time and store it if so
        for(tag_t tag: d_temp_tags) {
          if (tag.offset == 0 && pmt::eqv(tag.key, TIME_KEY)) {
            d_relative_time_at_start = tag.value;
            break;
          }
        }
        d_is_first_sample = false;
      }

      // drop output on the floor
      if(!d_fp) {
        handle_tags_not_capturing(d_temp_tags);
        return noutput_items;
      }

      if(d_temp_tags.size() > 0) {
        handle_tags(d_temp_tags);
      }

      while(nwritten < noutput_items) {
        int count = std::fwrite(inbuf, d_itemsize, noutput_items - nwritten, d_fp);
        if(count == 0) {
          if(std::ferror(d_fp)) {
            std::stringstream s;
            s << "sigmf_sink write failed with error " << fileno(d_fp) << std::endl;
            throw std::runtime_error(s.str());
          }
          // is EOF
          else {
            break;
          }
        }

        nwritten += count;
        inbuf += count * d_itemsize;
      }

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace sigmf */
} /* namespace gr */
