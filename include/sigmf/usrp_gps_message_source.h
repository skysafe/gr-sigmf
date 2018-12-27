/* -*- c++ -*- */
/*
 * Copyright 2018 Scott Torborg, Paul Wicks, Caitlin Miller
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


#ifndef INCLUDED_SIGMF_USRP_GPS_MESSAGE_SOURCE_H
#define INCLUDED_SIGMF_USRP_GPS_MESSAGE_SOURCE_H

#include <sigmf/api.h>
#include <gnuradio/block.h>
#include <uhd/usrp/multi_usrp.hpp>

namespace gr {
  namespace sigmf {

    /*!
     * \brief Emit PMT messages with GPS sensor data from a USRP.
     * \ingroup sigmf
     *
     */
    class SIGMF_API usrp_gps_message_source : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<usrp_gps_message_source> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of sigmf::usrp_gps_message_source.
       * @param uhd_args the args specifiying the usrp to use
       * @param poll_interval the polling interval in seconds
       * To avoid accidental use of raw pointers, sigmf::usrp_gps_message_source's
       * constructor is in a private implementation
       * class. sigmf::usrp_gps_message_source::make is the public interface for
       * creating new instances.
       */
      static sptr make(const ::uhd::device_addr_t &uhd_args, double poll_interval);

      /*!
       * \brief Return a shared_ptr to a new instance of sigmf::usrp_gps_message_source.
       * @param usrp_ptr a pointer to an already open usrp
       * @param poll_interval the polling interval in seconds
       * To avoid accidental use of raw pointers, sigmf::usrp_gps_message_source's
       * constructor is in a private implementation
       * class. sigmf::usrp_gps_message_source::make is the public interface for
       * creating new instances.
       */
      static sptr make(::uhd::usrp::multi_usrp::sptr usrp_ptr, double poll_interval);
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_USRP_GPS_MESSAGE_SOURCE_H */

