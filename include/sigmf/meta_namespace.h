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

#ifndef INCLUDED_META_NAMESPACE_H
#define INCLUDED_META_NAMESPACE_H

#include <iostream>
#include <pmt/pmt.h>

#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif

#include <rapidjson/document.h>

#ifdef RAPIDJSON_HAS_STDSTRING
#undef RAPIDJSON_HAS_STDSTRING
#endif

#include <set>
#include <sigmf/api.h>
#include <string>

namespace gr {
  namespace sigmf {

    static const char *SIGMF_VERSION = "0.0.1";

    class SIGMF_API meta_namespace {
      public:

      static meta_namespace
      build_global_object(std::string datatype, std::string version = SIGMF_VERSION);

      static meta_namespace build_capture_segment(uint64_t sample_start);
      static meta_namespace build_annotation_segment(uint64_t sample_start, uint64_t sample_count);

      meta_namespace(pmt::pmt_t data);
      meta_namespace();
      ~meta_namespace();

      pmt::pmt_t data();

      template <typename ValType>
      void
      set(const std::string &key, ValType val)
      {
        if(!validate_key(key)) {
          throw std::invalid_argument("key format is invalid:'" + key + "'");
        }
        d_data = pmt::dict_add(d_data, pmt::mp(key), pmt::mp(val));
      }

      void
      set(const pmt::pmt_t &key, const pmt::pmt_t &val)
      {
        std::string key_str = pmt::symbol_to_string(key);
        if(!validate_key(key_str)) {
          throw std::invalid_argument("key format is invalid:'" + key_str + "'");
        }
        d_data = pmt::dict_add(d_data, key, val);
      }

      // TODO: This should probably be a specialization of the template function above,
      // not an overload...
      void set(const std::string &key, pmt::pmt_t val);

      // TODO: refactor this with some templates...
      pmt::pmt_t get(const std::string &key) const;
      pmt::pmt_t get(const std::string &key, pmt::pmt_t default_val) const;
      pmt::pmt_t get(pmt::pmt_t key, pmt::pmt_t default_val) const;
      pmt::pmt_t get() const;

      std::string get_str(const std::string &key) const;

      bool has(const std::string &key) const;

      void del(const std::string &key);

      std::set<std::string> keys() const;
      std::set<pmt::pmt_t> pmt_keys() const;

      template <typename Writer>
      void
      serialize(Writer &writer) const
      {
        serialize_impl(writer, d_data);
      }

      void print();

      private:
      bool validate_key(const std::string &key);

      template <typename Writer>
      void
      serialize_impl(Writer &writer, pmt::pmt_t pmt_data) const
      {
        if(pmt::is_dict(pmt_data)) {
          writer.StartObject();
          pmt::pmt_t item_keys = pmt::dict_keys(pmt_data);
          size_t num_items = pmt::length(item_keys);
          for(size_t i = 0; i < num_items; i++) {
            pmt::pmt_t item_key = pmt::nth(i, item_keys);
            pmt::pmt_t val_for_key = pmt::dict_ref(pmt_data, item_key, pmt::get_PMT_NIL());
            std::string key_str = pmt::symbol_to_string(item_key);
            writer.String(key_str.c_str());
            serialize_impl(writer, val_for_key);
          }
          writer.EndObject();
        } else if(pmt::is_bool(pmt_data)) {
          writer.Bool(pmt::to_bool(pmt_data));
        } else if(pmt::is_integer(pmt_data)) {
          writer.Int64(pmt::to_long(pmt_data));
        } else if(pmt::is_real(pmt_data)) {
          writer.Double(pmt::to_double(pmt_data));
        } else if(pmt::is_vector(pmt_data)) {
          writer.StartArray();
          size_t num_items = pmt::length(pmt_data);
          for(size_t i = 0; i < num_items; i++) {
            pmt::pmt_t item = pmt::vector_ref(pmt_data, i);
            serialize_impl(writer, item);
          }
          writer.EndArray();
        } else if(pmt::is_symbol(pmt_data)) {
          std::string str = pmt::symbol_to_string(pmt_data);
          writer.String(str.c_str());
        } else if(pmt::is_uint64(pmt_data)) {
          writer.Uint64(pmt::to_uint64(pmt_data));
        } else {
          throw std::runtime_error("Unhandled pmt value in serialize_impl");
        }
      }
      pmt::pmt_t d_data;
    };

    struct SIGMF_API metafile_namespaces {
      meta_namespace global;
      std::vector<meta_namespace> captures;
      std::vector<meta_namespace> annotations;
    };

    metafile_namespaces load_metafile(FILE *fp) SIGMF_API;
    pmt::pmt_t json_value_to_pmt(const rapidjson::Value &val) SIGMF_API;
  }
}

#endif /* INCLUDED_META_NAMESPACE_H */
