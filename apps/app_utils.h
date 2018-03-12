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

#include <string>
#include <boost/endian/conversion.hpp>

std::string
uhd_format_to_sigmf_format(const std::string &format)
{
  std::string ending;
  if (boost::endian::order::native == boost::endian::order::little) {
    ending = "_le";
  } else {
    ending = "_be";
  }
  if(format == "fc64") {
    return "cf64" + ending;
  } else if(format == "fc32") {
    return "cf32" + ending;
  } else if(format == "sc16") {
    return "ci16" + ending;
  } else if(format == "sc8") {
    return "ci8" + ending;
  }
  return format;
}
