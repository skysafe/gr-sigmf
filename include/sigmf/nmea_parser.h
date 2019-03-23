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

#ifndef INCLUDED_SIGMF_NMEA_PARSER_H
#define INCLUDED_SIGMF_NMEA_PARSER_H

#include <string>
#include <vector>
#include <sigmf/api.h>

namespace gr {
  namespace sigmf {

    class SIGMF_API nmea_message {
    };

    class SIGMF_API gprmc_message : public nmea_message {
      public:
        uint32_t timestamp;
        std::string date;
        std::string time;
        bool valid;
        double lat;
        double lon;
        double speed_knots;
        double track_angle;
        double magnetic_variation;

        gprmc_message(uint32_t timestamp,
                      std::string date,
                      std::string time,
                      bool valid,
                      double lat,
                      double lon,
                      double speed_knots,
                      double track_angle,
                      double magnetic_variation);

        static gprmc_message parse(std::string raw);
    };

    class SIGMF_API gpgga_message : public nmea_message {
      public:
        std::string time;
        double lat;
        double lon;
        uint32_t fix_quality;
        uint32_t num_sats;
        double hdop;
        double altitude_msl;
        double geoid_hae;

        gpgga_message(std::string time,
                      double lat,
                      double lon,
                      uint32_t fix_quality,
                      uint32_t num_sats,
                      double hdop,
                      double altitude_msl,
                      double geoid_hae);

        static gpgga_message parse(std::string raw);
    };

    SIGMF_API std::vector<std::string> nmea_split(std::string s);

    SIGMF_API std::string nmea_extract(std::string raw);

    SIGMF_API double nmea_parse_degrees(std::string value, std::string dir);

    SIGMF_API double nmea_parse_magnetic_variation(std::string value, std::string dir);

    SIGMF_API std::time_t nmea_parse_datetime(std::string date, std::string time);

  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_NMEA_PARSER_H */

