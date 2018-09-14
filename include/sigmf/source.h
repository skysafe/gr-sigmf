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

#ifndef INCLUDED_SIGMF_SOURCE_H
#define INCLUDED_SIGMF_SOURCE_H

#include <gnuradio/sync_block.h>
#include <sigmf/api.h>
#include <sigmf/meta_namespace.h>

namespace gr {
  namespace sigmf {

    /*!
     * \brief Source Block to read from SigMF recordings.
     * \ingroup sigmf
     *
     */
    class SIGMF_API source : virtual public gr::sync_block {
      public:
      typedef boost::shared_ptr<source> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of sigmf::source.
       *
       * To avoid accidental use of raw pointers, sigmf::source's
       * constructor is in a private implementation
       * class. sigmf::source::make is the public interface for
       * creating new instances.
       */
      static sptr
      make(std::string filename, std::string output_datatype, bool repeat = false);

      /*!
       * \brief Return a shared_ptr to a new instance of sigmf::source.
       *
       * This constructor will build a sink that will always use the
       * native datatype of the input file as the output datatype.
       */
      static sptr
      make_no_datatype(std::string filename, bool repeat = false);

      /*!
       * \brief Add a stream tag to the first sample of the file if true
       * @param val the tag to add
       */
      virtual void set_begin_tag(pmt::pmt_t val) = 0;

      /*!
       * \brief retrieve the global metadata for this source
       */
      virtual gr::sigmf::meta_namespace &global_meta() = 0;

      /*!
       * \brief retrieve the capture segments for this source
       */
      virtual std::vector<gr::sigmf::meta_namespace> &capture_segments() = 0;
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_SOURCE_H */
