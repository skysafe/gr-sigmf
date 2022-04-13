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
#include <filesystem>
#include <regex>
#include <string>

namespace fs = std::filesystem;

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

      std::regex format_regex("(r|c)((f|i|u)(8|16|32|64))(_(le|be))?");
      std::smatch result;

      if(std::regex_match(format_str, result, format_regex)) {
        format_detail_t detail;
        auto is_complex = result[1] == "c";
        auto type_str = result[2];
        size_t width;
        try {
          width = std::stol(result[4]);
        } catch (const std::invalid_argument& e) {
          throw std::runtime_error("bad format str");
        } catch (const std::out_of_range& e) {
          throw std::runtime_error("bad format str");
        }
        endian_t endianness;
        if(result[6].matched) {
          endianness = result[6] == "le" ? LITTLE : BIG;
        }
        return format_detail_t{is_complex, type_str, width, endianness, (width / 8) * (is_complex ? 2 : 1)};
      } else {
        throw std::runtime_error("bad format str");
      }
    }
  } // namespace sigmf
} // namespace gr
