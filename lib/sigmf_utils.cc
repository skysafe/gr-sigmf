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

#include "sigmf/sigmf_utils.h"
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

namespace fs = boost::filesystem;

namespace gr {
  namespace sigmf {
    fs::path
    to_data_path(const std::string &filename)
    {
      fs::path data_path(filename);
      data_path.replace_extension(".sigmf-data");
      return data_path;
    }

    fs::path
    meta_path_from_data(fs::path data_path)
    {
      fs::path meta_path(data_path);
      meta_path.replace_extension(".sigmf-meta");
      return meta_path;
    }

    format_detail_t
    parse_format_str(const std::string &format_str)
    {

      boost::regex format_regex("(r|c)((f|i|u)(8|16|32))(_(le|be))?");
      boost::smatch result;

      if(boost::regex_match(format_str, result, format_regex)) {
        format_detail_t detail;
        detail.is_complex = result[1] == "c";
        detail.type_str = result[2];
        detail.width = boost::lexical_cast<size_t>(result[4]);
        if(result[6].matched) {
          detail.endianness = result[6] == "le" ? LITTLE : BIG;
        }
        return detail;
      } else {
        throw std::runtime_error("bad format str");
      }
    }
  } // namespace sigmf
} // namespace gr
