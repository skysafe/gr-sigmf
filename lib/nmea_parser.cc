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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

#include "sigmf/nmea_parser.h"

namespace gr {
  namespace sigmf {

    gprmc_message::gprmc_message(uint32_t timestamp,
                                 std::string date,
                                 std::string time,
                                 bool valid,
                                 double lat,
                                 double lon,
                                 double speed_knots,
                                 double track_angle,
                                 double magnetic_variation) : nmea_message(),
      timestamp(timestamp),
      date(date),
      time(time),
      valid(valid),
      lat(lat),
      lon(lon),
      speed_knots(speed_knots),
      track_angle(track_angle),
      magnetic_variation(magnetic_variation)
    {
    }

    gprmc_message gprmc_message::parse(std::string raw)
    {
      std::string payload = nmea_extract(raw);
      std::vector<std::string> fields = nmea_split(payload);
      std::string name = fields[0];

      if (name != "GPRMC") {
        throw std::invalid_argument("not a GPRMC message");
      }
      if (fields.size() < 12) {
        throw std::invalid_argument("insufficient number of fields for GPRMC");
      }
      std::string time = fields[1];
      bool valid = (fields[2] == "A");
      double lat = nmea_parse_degrees(fields[3], fields[4]);
      double lon = nmea_parse_degrees(fields[5], fields[6]);
      double speed_knots;
      if (fields[7] == "") {
        speed_knots = 0.0;
      } else {
        speed_knots = std::stod(fields[7]);
      }
      double track_angle;
      if (fields[8] == "") {
        track_angle = 0.0;
      } else {
        track_angle = std::stod(fields[8]);
      }
      std::string date = fields[9];
      double magnetic_variation = nmea_parse_magnetic_variation(fields[10], fields[11]);

      std::time_t t = nmea_parse_datetime(date, time);
      uint32_t timestamp = static_cast<uint32_t>(t);

      return gprmc_message(timestamp, date, time, valid, lat, lon,
                           speed_knots, track_angle, magnetic_variation);
    }

    gpgga_message::gpgga_message(std::string time,
                                 double lat,
                                 double lon,
                                 uint32_t fix_quality,
                                 uint32_t num_sats,
                                 double hdop,
                                 double altitude_msl,
                                 double geoid_hae) : nmea_message(),
      time(time),
      lat(lat),
      lon(lon),
      fix_quality(fix_quality),
      num_sats(num_sats),
      hdop(hdop),
      altitude_msl(altitude_msl),
      geoid_hae(geoid_hae)
    {
    }

    gpgga_message gpgga_message::parse(std::string raw)
    {
      std::string payload = nmea_extract(raw);
      std::vector<std::string> fields = nmea_split(payload);
      std::string name = fields[0];

      if (name != "GPGGA") {
        throw std::invalid_argument("not a GPGGA message");
      }
      if (fields.size() < 15) {
        throw std::invalid_argument("insufficient number of fields for GPGGA");
      }
      std::string time = fields[1];
      double lat = nmea_parse_degrees(fields[2], fields[3]);
      double lon = nmea_parse_degrees(fields[4], fields[5]);
      size_t fix_quality = std::stoi(fields[6]);
      size_t num_sats = std::stoi(fields[7]);
      double hdop = std::stod(fields[8]);
      double altitude_msl;
      if (fields[9] == "") {
        altitude_msl = 0.0;
      } else {
        altitude_msl = std::stod(fields[9]);
      }
      // XXX ignoring altitude units (field 10) maybe we should make sure it's M?
      double geoid_hae;
      if (fields[11] == "") {
        geoid_hae = 0.0;
      } else {
        geoid_hae = std::stod(fields[11]);
      }
      // XXX ignoring geoid hae units (field 12) maybe we should make sure it's M?
      // ignoring dgps update age field 13
      // ignoring dgps station id number field 14

      return gpgga_message(time, lat, lon, fix_quality, num_sats,
                           hdop, altitude_msl, geoid_hae);
    }

    std::vector<std::string> nmea_split(std::string s)
    {
      std::vector<std::string> result;
      while (s.size()){
        size_t index = s.find(',');
        if (index != std::string::npos) {
          result.push_back(s.substr(0, index));
          s = s.substr(index + 1);
          if (s.size() == 0) {
            result.push_back(s);
          }
        } else {
          result.push_back(s);
          s = "";
        }
      }
      return result;
    }

    std::string nmea_extract(std::string raw)
    {
      // Find payload start and end sentinels
      size_t start_char = raw.find("$");
      size_t end_char = raw.find("*");
      if (start_char == std::string::npos) {
        throw std::invalid_argument("missing NMEA start sentinel");
      }
      if (end_char == std::string::npos) {
        throw std::invalid_argument("missing NMEA end sentinel");
      }

      // Extract payload
      std::string payload = raw.substr(start_char + 1, end_char - start_char - 1);

      // Compute checksum on message payload
      uint8_t checksum = 0;
      for (char const &c : payload) {
        checksum ^= c;
      }
      std::stringstream stream;
      stream << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(checksum);
      std::string computed(stream.str());

      // Checksum is directly after "*" end sentinel
      std::string received = raw.substr(end_char + 1, 2);

      if (computed.compare(received) != 0) {
        throw std::invalid_argument("invalid NMEA checksum");
      }

      return payload;
    }

    double nmea_parse_degrees(std::string value, std::string dir)
    {
      int sign;
      size_t digits;
      if (value == "") {
        return 0.0;
      } else if (dir == "W") {
        sign = -1;
        digits = 3;
      } else if (dir == "E") {
        sign = 1;
        digits = 3;
      } else if (dir == "N") {
        sign = 1;
        digits = 2;
      } else if (dir == "S") {
        sign = -1;
        digits = 2;
      } else {
        throw std::invalid_argument("invalid direction");
      }
      double degrees = sign * std::stod(value.substr(0, digits));
      double minutes = std::stod(value.substr(digits));
      return degrees + (minutes / 60.0);
    }

    double nmea_parse_magnetic_variation(std::string value, std::string dir)
    {
      int sign;
      if (value == "") {
        return 0.0;
      } else if (dir == "W") {
        sign = -1;
      } else if (dir == "E") {
        sign = 1;
      } else {
        throw std::invalid_argument("invalid direction");
      }
      return sign * std::stod(value);
    }

    std::time_t nmea_parse_datetime(std::string date, std::string time)
    {
      if ((date == "") || (time == "")) {
        return std::time_t();
      }
      int mday = std::stoi(date.substr(0, 2));
      int month = std::stoi(date.substr(2, 2));
      int year = std::stoi(date.substr(4, 2));

      int hour = std::stoi(time.substr(0, 2));
      int minute = std::stoi(time.substr(2, 2));
      int second = std::stoi(time.substr(4, 2));

      std::tm tm = {0};
      tm.tm_mday = mday;
      tm.tm_mon = month - 1;
      tm.tm_year = year;
      tm.tm_hour = hour;
      tm.tm_min = minute;
      tm.tm_sec = second;

      std::time_t t = std::mktime(&tm) - timezone;
      return t;
    }
  }
}
