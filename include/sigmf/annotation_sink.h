/* -*- c++ -*- */
/*
 * Copyright 2018 Paul Wicks
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


#ifndef INCLUDED_SIGMF_ANNOTATION_SINK_H
#define INCLUDED_SIGMF_ANNOTATION_SINK_H

#include <string>
#include <sigmf/api.h>
#include <sigmf/time_mode.h>
#include <gnuradio/block.h>


namespace gr {
  namespace sigmf {

    enum class annotation_filter_strategy {
      clear_existing,
      keep_existing
    };


    /**
     * Determines how the annotation sink deals
     * with existing annotations. If keep(), then
     * they are all kept. If clear(), then existing
     * annotations are cleared, optionally with a globbing
     * expression as a filter for the annotations to remove
     */
    struct SIGMF_API annotation_mode {
      annotation_filter_strategy filter_strategy;
      std::string filter_key;

      static annotation_mode keep() {
        return annotation_mode(annotation_filter_strategy::keep_existing);
      }

      static annotation_mode clear(std::string filter) {
        return annotation_mode(annotation_filter_strategy::clear_existing, filter);
      }

      protected:
      annotation_mode(annotation_filter_strategy strat, std::string key = "")
      : filter_strategy(strat), filter_key(key) {}
    };

    /*!
     * \brief Sink block for writing annotations to an existing dataset
     * \ingroup sigmf
     *
     */
    class SIGMF_API annotation_sink : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<annotation_sink> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of sigmf::annotation_sink.
       *
       * To avoid accidental use of raw pointers, sigmf::annotation_sink's
       * constructor is in a private implementation
       * class. sigmf::annotation_sink::make is the public interface for
       * creating new instances.
       */
      static sptr make(std::string filename,
                       annotation_mode mode,
                       sigmf_time_mode time_mode = sigmf_time_mode::relative);
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_ANNOTATION_SINK_H */

