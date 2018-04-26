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

#ifndef INCLUDED_SIGMF_USRP_GPS_MESSAGE_SOURCE_IMPL_H
#define INCLUDED_SIGMF_USRP_GPS_MESSAGE_SOURCE_IMPL_H

#include <uhd/usrp/multi_usrp.hpp>
#include <sigmf/usrp_gps_message_source.h>

namespace gr {
  namespace sigmf {

    class usrp_gps_message_source_impl : public usrp_gps_message_source
    {
      private:
      double d_poll_interval;
      size_t d_mboard;
      uhd::usrp::multi_usrp::sptr d_usrp;
      gr::thread::thread d_poll_thread;

      void poll_thread();

      public:
      usrp_gps_message_source_impl(const std::string uhd_args, double poll_interval);
      ~usrp_gps_message_source_impl();

      pmt::pmt_t poll_now();
    };

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_USRP_GPS_MESSAGE_SOURCE_IMPL_H */

