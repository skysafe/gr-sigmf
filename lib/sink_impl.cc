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
#include <fcntl.h>
#include <gnuradio/io_signature.h>

#define RAPIDJSON_HAS_STDSTRING 1

#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include "sigmf/sigmf_utils.h"
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

namespace fs = boost::filesystem;
namespace posix = boost::posix_time;
namespace greg = boost::gregorian;

namespace gr {
  namespace sigmf {

    sink::sptr
    sink::make(std::string type,
               std::string filename,
               double samp_rate,
               std::string description,
               std::string author,
               std::string license,
               std::string hardware,
               bool append,
               bool debug)
    {
      return gnuradio::get_initial_sptr(new sink_impl(type, filename, samp_rate, description, author,
                                                      license, hardware, append, debug));
    }

    /*
     * The private constructor
     */
    sink_impl::sink_impl(std::string type,
                         std::string filename,
                         double samp_rate,
                         std::string description,
                         std::string author,
                         std::string license,
                         std::string hardware,
                         bool append,
                         bool debug)
    : gr::sync_block("sink",
                     gr::io_signature::make(1, 1, type_to_size(type)),
                     gr::io_signature::make(0, 0, 0)),
      d_fp(nullptr), d_new_fp(nullptr), d_append(append), d_itemsize(type_to_size(type)),
      d_type(type), d_samp_rate(samp_rate), d_description(description), d_author(author),
      d_license(license), d_hardware(hardware), d_debug(debug), d_meta_written(false),
      d_recording_start_offset(0)
    {
      reset_meta();
      open(filename.c_str());
      d_temp_tags.reserve(32);

      // command message port
      message_port_register_in(COMMAND);
      set_msg_handler(COMMAND, boost::bind(&sink_impl::on_command_message, this, _1));
    }

    /*
     * Our virtual destructor.
     */
    sink_impl::~sink_impl()
    {
    }

    bool
    sink_impl::stop() {
      close();

      if (d_fp) {
        std::fclose(d_fp);
        write_meta();
        d_fp = nullptr;
      }

      return true;
    }

    void
    sink_impl::reset_meta() {
      // std::cout << "reset_meta()" << std::endl;
      d_global = meta_namespace::build_global_object(d_type);
      d_global.set("core:sample_rate", d_samp_rate);
      d_global.set("core:description", d_description);
      d_global.set("core:author", d_author);
      d_global.set("core:license", d_license);
      d_global.set("core:hw", d_hardware);
      d_captures.clear();
      d_captures.push_back(meta_namespace::build_capture_segment(0));
      d_annotations.clear();
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
        close();
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

      } else {
        GR_LOG_ERROR(d_logger,
                     boost::format("Invalid command string received in dict: %s") % msg);
        return;
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
    sink_impl::set_global_meta(std::string key, pmt::pmt_t val) {
      d_global.set(key, val);
    }

    void
    sink_impl::set_capture_meta(uint64_t index, std::string key, pmt::pmt_t val) {
      try {
        auto capture = d_captures.at(index);
        capture.set(key, val);
      } catch (const std::out_of_range &e) {
        GR_LOG_ERROR(d_logger, "Invalid capture index");
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
      d_new_meta_path = meta_path_from_data(d_new_data_path);

      // we use the open system call to get access to the O_LARGEFILE flag.
      int fd;
      int flags;
      if(d_append) {
        flags = O_WRONLY | O_CREAT | O_APPEND | OUR_O_LARGEFILE | OUR_O_BINARY;
      } else {
        flags = O_WRONLY | O_CREAT | O_TRUNC | OUR_O_LARGEFILE | OUR_O_BINARY;
      }
      if((fd = ::open(d_new_data_path.c_str(), flags, 0664)) < 0) {
        GR_LOG_ERROR(d_logger,
                     boost::format("Failed to open file descriptor for path '%s'") % d_new_data_path);
        std::perror(d_new_data_path.c_str());
        return false;
      }

      // if we've already got a new one open, close it
      if(d_new_fp) {
        std::fclose(d_new_fp);
        d_new_fp = 0;
      }

      if((d_new_fp = ::fdopen(fd, "wb")) == NULL) {
        std::perror(d_new_data_path.c_str());

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
        	reset_meta();
        }
        d_recording_start_offset = nitems_read(0);

        // install new file pointer
        d_fp = d_new_fp;
        d_data_path = d_new_data_path;
        d_meta_path = d_new_meta_path;
        d_meta_written = d_new_fp == nullptr ? true : false;

        d_new_fp = nullptr;
        d_updated = false;
      }
    }

    void
    sink_impl::close()
    {
      // hold mutex for duration of this function
      gr::thread::scoped_lock guard(d_mutex);

      if(d_new_fp != nullptr) {
        std::fclose(d_new_fp);
        d_new_fp = nullptr;
      }
      d_updated = true;
    }

    void
    sink_impl::write_meta()
    {
      if(d_debug) {
        // std::cout << "meta_path = " << d_meta_path << std::endl;
      }
      if(d_meta_written) {
        return;
      }

      FILE *fp = std::fopen(d_meta_path.c_str(), "w");
      if (fp == nullptr) {
        std::perror("Error opening d_meta_path");
      }
      char write_buf[65536];
      rapidjson::FileWriteStream file_stream(fp, write_buf, sizeof(write_buf));

      rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(file_stream);
      writer.StartObject();

      writer.String("global");
      d_global.serialize(writer);

      writer.String("captures");
      writer.StartArray();
      for(std::vector<meta_namespace>::iterator it = d_captures.begin();
          it != d_captures.end(); it++) {
        (*it).serialize(writer);
      }
      writer.EndArray();

      // sort annotations
      std::sort(d_annotations.begin(), d_annotations.end(), [](const meta_namespace &a, const meta_namespace &b) {
        // TODO: This may need to become a more complex sort if the spec changes based on https://github.com/gnuradio/SigMF/issues/90
        return pmt::to_uint64(a.get("core:sample_start")) < pmt::to_uint64(b.get("core:sample_start"));
      });

      writer.String("annotations");
      writer.StartArray();
      for(std::vector<meta_namespace>::iterator it = d_annotations.begin();
          it != d_annotations.end(); it++) {
        (*it).serialize(writer);
      }
      writer.EndArray();

      writer.EndObject();
      std::fclose(fp);
      d_meta_written = true;
    }


    void
    sink_impl::add_tag_to_capture_segment(const tag_t *tag, meta_namespace &capture_segment)
    {
      if(pmt::eqv(tag->key, TIME_KEY)) {

        // this is a tuple
        uint64_t seconds = pmt::to_uint64(pmt::tuple_ref(tag->value, 0));
        double frac_seconds = pmt::to_double(pmt::tuple_ref(tag->value, 1));

        // As of 1/17/18, there's no good way to map these values to anything in core
        // For now, we'll just use an extension namespace, but this may change as
        // core expands

        capture_segment.set("usrp:offset_full_secs", pmt::mp(seconds));
        capture_segment.set("usrp:offset_frac_secs", pmt::mp(frac_seconds));

      } else if(pmt::eqv(tag->key, FREQ_KEY)) {

        // frequency as double
        capture_segment.set("core:frequency", tag->value);

      } else if(pmt::eqv(tag->key, RATE_KEY)) {

        // sample_rate as double
        capture_segment.set("core:sample_rate", tag->value);

      } else {
        throw std::runtime_error("invalid key in capture_segment_from_tags");
      }
    }


    bool
    is_capture_tag(const tag_t *tag)
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
        tag_vec_it annotations_begin = std::partition(tag_begin, it->second.end(), &is_capture_tag);
        // Handle any capture tags
        if(std::distance(tag_begin, annotations_begin) > 0) {
          pmt::pmt_t most_recent_segment_start =
            d_captures.back().get("core:sample_start");
          // IF there's already a segment for this sample index, then use that
          if(offset != pmt::to_uint64(most_recent_segment_start)) {
            // otherwise add a new empty segment
            meta_namespace new_capture;
            d_captures.push_back(new_capture);
          }
          meta_namespace &capture_ns = d_captures.back();
          for(tag_vec_it tag_it = tag_begin; tag_it != annotations_begin; tag_it++) {
            // These get added to the capture object
            add_tag_to_capture_segment(*tag_it, capture_ns);
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
              anno_ns.set((*tag_it)->key, (*tag_it)->value);
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

    int
    sink_impl::work(int noutput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
    {
      char *inbuf = (char *)input_items[0];
      int nwritten = 0;

      // Check if a new fp is here and handle the update if so
      do_update();

      // drop output on the floor
      if(!d_fp) {
        return noutput_items;
      }

      get_tags_in_window(d_temp_tags, 0, 0, noutput_items);
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
