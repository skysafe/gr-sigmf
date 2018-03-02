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

#ifndef INCLUDED_SIGMF_SINK_H
#define INCLUDED_SIGMF_SINK_H

#include <gnuradio/sync_block.h>
#include <sigmf/api.h>
#include <sigmf/meta_namespace.h>

namespace gr {
  namespace sigmf {

    /*!
     * \brief Sink block to create SigMF recordings.
     * \ingroup sigmf
     *
     */
    class SIGMF_API sink : virtual public gr::sync_block {
      public:
      typedef boost::shared_ptr<sink> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of sigmf::sink.
       *
       * To avoid accidental use of raw pointers, sigmf::sink's
       * constructor is in a private implementation
       * class. sigmf::sink::make is the public interface for
       * creating new instances.
       */
      static sptr make(std::string type,
                       std::string filename,
                       double samp_rate,
                       std::string description,
                       std::string author,
                       std::string license,
                       std::string hardware,
                       bool append = false,
                       bool debug = false);

      virtual std::string get_data_path() = 0;
      virtual std::string get_meta_path() = 0;

      virtual void set_global_meta(std::string key, pmt::pmt_t val) = 0;
      virtual void set_annotation_meta(uint64_t sample_start,
                                       uint64_t sample_count,
                                       std::string key,
                                       pmt::pmt_t val) = 0;

      virtual void set_capture_meta(uint64_t index, std::string key, pmt::pmt_t val) = 0;

      virtual bool open(const char *filename) = 0;
      virtual void close() = 0;
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_SINK_H */
