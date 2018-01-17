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

#pragma once
#include <boost/filesystem/path.hpp>
namespace gr {
  namespace sigmf {
    boost::filesystem::path to_data_path(const std::string &filename);
    boost::filesystem::path meta_path_from_data(boost::filesystem::path data_path);
    enum endian_t { LITTLE = 0, BIG = 1 };
    struct format_detail_t {
      bool is_complex;
      std::string type_str; // base type string, so no r or c and no _le or _be
      size_t width;
      endian_t endianness;
    };

    format_detail_t parse_format_str(const std::string &format_str);
    size_t format_str_to_size(const std::string &format_str);
  }
}
