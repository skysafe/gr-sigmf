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
#include <sigmf/time_mode.h>
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
                       sigmf_time_mode time_mode = sigmf_time_mode::absolute,
                       bool append = false);

      /*!
       * \brief Get the path the the current .sigmf-data file as a string
       *
       * If there is no currently open file, returns empty string
       * @return the path
       */
      virtual std::string get_data_path() = 0;

      /*!
       * \brief Get the path the the current .sigmf-meta file as a string
       *
       * If there is no currently open file, returns empty string
       * @return the path
       */
      virtual std::string get_meta_path() = 0;

      /*!
       * \brief Set a value in the global metadata for this data set.
       * @param key the key for the value
       * @param val the value to store
       */
      virtual void set_global_meta(const std::string &key, pmt::pmt_t val) = 0;

      /*!
       * \brief Set a value in the global metadata for this data set.
       * @param key the key for the value
       * @param val the value to store
       */
      virtual void set_global_meta(const std::string &key, double val) = 0;

      /*!
       * \brief Set a value in the global metadata for this data set.
       * @param key the key for the value
       * @param val the value to store
       */
      virtual void set_global_meta(const std::string &key, int64_t val) = 0;

      /*!
       * \brief Set a value in the global metadata for this data set.
       * @param key the key for the value
       * @param val the value to store
       */
      virtual void set_global_meta(const std::string &key, uint64_t val) = 0;

      /*!
       * \brief Set a value in the global metadata for this data set.
       * @param key the key for the value
       * @param val the value to store
       */
      virtual void set_global_meta(const std::string &key, const std::string &val) = 0;

      /*!
       * \brief Set a value in the global metadata for this data set.
       * @param key the key for the value
       * @param val the value to store
       */
      virtual void set_global_meta(const std::string &key, bool val) = 0;

      /*!
       * \brief Set a value in the annotations metadata for this data set.
       * @param sample_start sample start for the annotation
       * @param sample_count sample count for the annotation
       * @param key the key for the value
       * @param val the value to store
       *
       * If there is an existing annotation with the same sample start and count,
       * then the new value will be added to it. Otherwise, a new annotation will
       * be created.
       */
      virtual void set_annotation_meta(uint64_t sample_start,
                                       uint64_t sample_count,
                                       std::string key,
                                       pmt::pmt_t val) = 0;

      /*!
       * \brief Set a value in the annotations metadata for this data set.
       * @param index the index of the desired capture segment
       * @param key the key for the value
       * @param val the value to store
       *
       * If no capture segment with the given index exists, then the new value is
       * dropped and an error is logged.
       */
      virtual void set_capture_meta(uint64_t index, std::string key, pmt::pmt_t val) = 0;

      /*!
       * \brief Open a new file to start recording to
       * @param filename the file to write to
       *
       * The sigmf sink will coerce the filename to the names of the two files in a
       * SigMF data set
       */
      virtual void open(const char *filename) = 0;

      /*!
       * \brief Stop writing to the current file
       */
      virtual void close() = 0;
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_SINK_H */
