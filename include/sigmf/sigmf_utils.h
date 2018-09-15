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

#include <sigmf/api.h>
#include <boost/filesystem/path.hpp>
namespace gr {
  namespace sigmf {

    /*!
     * \brief convert a filename to the path of a .sigmf-data file
     * @param filename the filename
     * @return the path as a boost::filesystem::path object
     *
     * For example, filename is `/foo/bar/baz` then the returned path would
     * be `/foo/bar/bas.sigmf-data`. Any existing extension will be replaced
     */
    boost::filesystem::path to_data_path(const std::string &filename) SIGMF_API;

    /*!
     * \brief convert a data path to the path of a .sigmf-meta file
     * @param data_path path to data
     * @return the path as a boost::filesystem::path object
     */
    boost::filesystem::path meta_path_from_data(boost::filesystem::path data_path) SIGMF_API;

    //! An enum describing endianness
    enum endian_t {
      //! Little endian
      LITTLE = 0,
      //! Big endian
      BIG = 1
    };

    //! struct representing a parsed SigMF format
    struct SIGMF_API format_detail_t {
      //! true if the format is a complex type, false otherwise
      bool is_complex;
      //! base type string, so no r or c and no _le or _be
      std::string type_str;
      //! size of the format in bits
      size_t width;
      //! endinness of the format
      endian_t endianness;
    };

    /*!
     * \brief Parse the dataset format
     * defined by SigMF into a format_detail_t struct.
     * @param format_str format as a sttring
     * @exception std::runtime_error invalid format string
     * @return the parsed struct
     */
    format_detail_t parse_format_str(const std::string &format_str) SIGMF_API;

  }
}
