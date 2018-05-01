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
#include <boost/function.hpp>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <volk/volk.h>
#include "sigmf/sigmf_utils.h"

static const int64_t MAX_INT = 2147483647;  //  (2^31)-1
static const int64_t MIN_INT = -2147483647; // -(2^31)-1

static const int64_t MAX_SHORT = 32767;		//  (2^15)-1
static const int64_t MIN_SHORT = -32767;	// -(2^15)-1

static const int64_t MAX_CHAR = 127;		//  (2^7)-1
static const int64_t MIN_CHAT = -127;		// -(2^7)-1

namespace gr {
  namespace sigmf {

    // Whenever the types agree, this function should be used
    size_t
    read_same(char *buf, size_t item_size, size_t num_items, FILE *fp)
    {
      return std::fread(buf, item_size, num_items, fp);
    }

    class _converter_base {
      private:
      size_t d_allocated_size;

      protected:
      char *d_temp_buf;

      _converter_base() : d_allocated_size(1024)
      {
        d_temp_buf = (char *)volk_malloc(d_allocated_size, volk_get_alignment());
      };

      _converter_base(const _converter_base &obj) : d_allocated_size(obj.d_allocated_size)
      {
        d_temp_buf = (char *)volk_malloc(d_allocated_size, volk_get_alignment());
      }

      ~_converter_base()
      {
        if(d_temp_buf) {
          volk_free(d_temp_buf);
        }
      };

      void
      reserve(size_t size)
      {
        if(size > d_allocated_size) {
          volk_free(d_temp_buf);
          d_allocated_size = size;
          d_temp_buf = (char *)volk_malloc(size, volk_get_alignment());
        }
      }
    };

    struct f32_to_i32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        /*volk_32f_s32f_convert_32i(reinterpret_cast<int32_t *>(buf),
                                  reinterpret_cast<const float *>(d_temp_buf), 1,
           items_read);*/

        int32_t *out = reinterpret_cast<int32_t *>(buf);
        const float *in = reinterpret_cast<const float *>(d_temp_buf);

        for(unsigned int i = 0; i < count; i++) {
          int64_t r = llrintf(in[i]);
          if(r < MIN_INT) {
            r = MIN_INT;
          } else if(r > MAX_INT) {
            r = MAX_INT;
          }
          out[i] = static_cast<int32_t>(r);
        }

        return items_read;
      }
    };

    struct f32_to_u32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct f32_to_i16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_32f_s32f_convert_16i(reinterpret_cast<int16_t *>(buf),
                                  reinterpret_cast<const float *>(d_temp_buf), 1, items_read);
        return items_read;
      }
    };

    struct f32_to_u16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct f32_to_i8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_32f_s32f_convert_8i(reinterpret_cast<int8_t *>(buf),
                                 reinterpret_cast<const float *>(d_temp_buf), 1, items_read);
        return items_read;
      }
    };

    struct f32_to_u8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i32_to_f32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_32i_s32f_convert_32f(reinterpret_cast<float *>(buf),
                                  reinterpret_cast<const int32_t *>(d_temp_buf), MAX_INT, items_read);
        return items_read;
      }
    };

    struct i32_to_u32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i32_to_i16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {

        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);

        int16_t *out = reinterpret_cast<int16_t *>(buf);
        const int32_t *in = reinterpret_cast<const int32_t *>(d_temp_buf);
        for(unsigned int i = 0; i < count; i++) {
          *out++ = ((int16_t)(*in++));
        }

        return items_read;
      }
    };

    struct i32_to_u16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i32_to_i8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {

        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);

        int8_t *out = reinterpret_cast<int8_t *>(buf);
        const int32_t *in = reinterpret_cast<const int32_t *>(d_temp_buf);
        for(unsigned int i = 0; i < count; i++) {
          *out++ = ((int8_t)(*in++));
        }

        return items_read;
      }
    };

    struct i32_to_u8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u32_to_f32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u32_to_i32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u32_to_i16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u32_to_u16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u32_to_i8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u32_to_u8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };


    struct i16_to_f32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_16i_s32f_convert_32f(reinterpret_cast<float *>(buf),
                                  reinterpret_cast<const int16_t *>(d_temp_buf), MAX_SHORT, items_read);
        return items_read;
      }
    };

    struct i16_to_i32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {

        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);

        int32_t *out = reinterpret_cast<int32_t *>(buf);
        const int16_t *in = reinterpret_cast<const int16_t *>(d_temp_buf);
        for(unsigned int i = 0; i < count; i++) {
          *out++ = ((int8_t)(*in++));
        }

        return items_read;
      }
    };

    struct i16_to_u32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i16_to_u16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i16_to_i8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_16i_convert_8i(reinterpret_cast<int8_t *>(buf),
                            reinterpret_cast<const int16_t *>(d_temp_buf), items_read);
        return items_read;
      }
    };

    struct i16_to_u8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u16_to_f32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u16_to_i32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u16_to_u32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u16_to_i16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u16_to_i8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u16_to_u8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i8_to_f32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_8i_s32f_convert_32f(reinterpret_cast<float *>(buf),
                                 reinterpret_cast<const int8_t *>(d_temp_buf), MAX_CHAR, items_read);
        return items_read;
      }
    };

    struct i8_to_i32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {

        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);

        int32_t *out = reinterpret_cast<int32_t *>(buf);
        const int8_t *in = reinterpret_cast<const int8_t *>(d_temp_buf);
        for(unsigned int i = 0; i < count; i++) {
          *out++ = ((int8_t)(*in++));
        }

        return items_read;
      }
    };

    struct i8_to_u32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i8_to_i16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        reserve(item_size * count);
        size_t items_read = std::fread(d_temp_buf, item_size, count, fp);
        volk_8i_convert_16i(reinterpret_cast<int16_t *>(buf),
                            reinterpret_cast<const int8_t *>(d_temp_buf), items_read);
        return items_read;
      }
    };

    struct i8_to_u16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct i8_to_u8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u8_to_f32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u8_to_i32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u8_to_u32 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u8_to_i16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u8_to_u16 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    struct u8_to_i8 : _converter_base {
      size_t
      operator()(char *buf, size_t item_size, size_t count, FILE *fp)
      {
        throw std::runtime_error("Unsigned integer type conversions not yet implemented");
        return 0;
      }
    };

    typedef boost::function<size_t(char *, size_t, size_t, FILE *)> convert_function_t;
    typedef std::map<std::pair<std::string, std::string>, convert_function_t> function_map_t;


    function_map_t
    build_map()
    {
      function_map_t map;
      map[std::make_pair("f32", "i32")] = f32_to_i32();
      map[std::make_pair("f32", "u32")] = f32_to_u32();
      map[std::make_pair("f32", "i16")] = f32_to_i16();
      map[std::make_pair("f32", "u16")] = f32_to_u16();
      map[std::make_pair("f32", "i8")] = f32_to_i8();
      map[std::make_pair("f32", "u8")] = f32_to_u8();
      map[std::make_pair("i32", "f32")] = i32_to_f32();
      map[std::make_pair("i32", "u32")] = i32_to_u32();
      map[std::make_pair("i32", "i16")] = i32_to_i16();
      map[std::make_pair("i32", "u16")] = i32_to_u16();
      map[std::make_pair("i32", "i8")] = i32_to_i8();
      map[std::make_pair("i32", "u8")] = i32_to_u8();
      map[std::make_pair("u32", "f32")] = u32_to_f32();
      map[std::make_pair("u32", "i32")] = u32_to_i32();
      map[std::make_pair("u32", "i16")] = u32_to_i16();
      map[std::make_pair("u32", "u16")] = u32_to_u16();
      map[std::make_pair("u32", "i8")] = u32_to_i8();
      map[std::make_pair("u32", "u8")] = u32_to_u8();
      map[std::make_pair("i16", "f32")] = i16_to_f32();
      map[std::make_pair("i16", "i32")] = i16_to_i32();
      map[std::make_pair("i16", "u32")] = i16_to_u32();
      map[std::make_pair("i16", "u16")] = i16_to_u16();
      map[std::make_pair("i16", "i8")] = i16_to_i8();
      map[std::make_pair("i16", "u8")] = i16_to_u8();
      map[std::make_pair("u16", "f32")] = u16_to_f32();
      map[std::make_pair("u16", "i32")] = u16_to_i32();
      map[std::make_pair("u16", "u32")] = u16_to_u32();
      map[std::make_pair("u16", "i16")] = u16_to_i16();
      map[std::make_pair("u16", "i8")] = u16_to_i8();
      map[std::make_pair("u16", "u8")] = u16_to_u8();
      map[std::make_pair("i8", "f32")] = i8_to_f32();
      map[std::make_pair("i8", "i32")] = i8_to_i32();
      map[std::make_pair("i8", "u32")] = i8_to_u32();
      map[std::make_pair("i8", "i16")] = i8_to_i16();
      map[std::make_pair("i8", "u16")] = i8_to_u16();
      map[std::make_pair("i8", "u8")] = i8_to_u8();
      map[std::make_pair("u8", "f32")] = u8_to_f32();
      map[std::make_pair("u8", "i32")] = u8_to_i32();
      map[std::make_pair("u8", "u32")] = u8_to_u32();
      map[std::make_pair("u8", "i16")] = u8_to_i16();
      map[std::make_pair("u8", "u16")] = u8_to_u16();
      map[std::make_pair("u8", "i8")] = u8_to_i8();
      return map;
    }

    function_map_t function_map = build_map();

    convert_function_t
    get_convert_function(std::string from_type, std::string to_type)
    {
      if(from_type == to_type) {
        return &read_same;
      }

      // Check if one is real and the other is complex
      format_detail_t from_detail = parse_format_str(from_type);
      format_detail_t to_detail = parse_format_str(to_type);

      if(from_detail.is_complex != to_detail.is_complex) {
        throw std::runtime_error("Can't make types work together");
      }

      // If the types are the same, use read_same
      if(from_detail.type_str == to_detail.type_str) {
        return convert_function_t(read_same);
      }

      // Make a key for the map
      std::pair<std::string, std::string> type_pair =
        std::make_pair(from_detail.type_str, to_detail.type_str);
      if(function_map.count(type_pair)) {
        return function_map[type_pair];
      } else {
        throw std::runtime_error("Not yet implemented!");
      }
    }
  } // namespace sigmf
} // namespace gr
