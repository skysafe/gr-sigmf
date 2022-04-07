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

#include "sigmf/meta_namespace.h"
#include <boost/format.hpp>
#include <regex>
#include <stdexcept>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gr {
  namespace sigmf {

    metafile_namespaces
    load_metafile(FILE *fp)
    {
      metafile_namespaces meta_ns;
      json meta_file;
      try {
        meta_file = json::parse(fp);
      } catch (const json::parse_error &e){

        throw std::runtime_error((boost::format("Meta namespace parse error '%1%' at byte %1%") % e.what() % e.byte).str());
      }

      meta_ns.global = meta_file["global"];
      size_t num_captures = meta_file["captures"].size();

      for(size_t i = 0; i < num_captures; i++) {
        meta_ns.captures.push_back(meta_file["captures"][i]);
      }
      size_t num_annotations;
      if(meta_file.contains("annotations")) {
        num_annotations = meta_file["annotations"].size();
      } else {
        num_annotations = 0;
      }

      for(size_t i = 0; i < num_annotations; i++) {
        meta_ns.annotations.push_back(meta_file["annotations"][i]);
      }
      return meta_ns;
    }

    meta_namespace
    meta_namespace::build_global_object(std::string datatype, std::string version)
    {
      meta_namespace ns;
      ns.set("core:datatype", datatype);
      ns.set("core:version", version);
      return ns;
    }

    meta_namespace
    meta_namespace::build_capture_segment(uint64_t sample_start)
    {
      meta_namespace ns;
      ns.set("core:sample_start", sample_start);
      return ns;
    }

    meta_namespace
    meta_namespace::build_annotation_segment(uint64_t sample_start, uint64_t sample_count)
    {
      meta_namespace ns;
      ns.set("core:sample_start", sample_start);
      ns.set("core:sample_count", sample_count);
      return ns;
    }

    meta_namespace::meta_namespace(pmt::pmt_t data) : d_data(data)
    {
    }

    meta_namespace::meta_namespace()
    {
      d_data = pmt::make_dict();
    }

    meta_namespace::~meta_namespace()
    {
    }

    pmt::pmt_t
    meta_namespace::data()
    {
      return d_data;
    }

    void
    meta_namespace::set(const std::string &key, pmt::pmt_t val)
    {
      if(!validate_key(key)) {
        throw std::invalid_argument("key format is invalid:'" + key + "'");
      }
      d_data = pmt::dict_add(d_data, pmt::mp(key), val);
    }

    pmt::pmt_t
    meta_namespace::get(const std::string &key) const
    {
      return get(pmt::mp(key));
    }

    pmt::pmt_t
    meta_namespace::get(const std::string &key, pmt::pmt_t default_val) const
    {
      return get(pmt::mp(key), default_val);
    }

    pmt::pmt_t
    meta_namespace::get(pmt::pmt_t key, pmt::pmt_t default_val) const {
      return pmt::dict_ref(d_data, key, default_val);
    }

    pmt::pmt_t
    meta_namespace::get(pmt::pmt_t key) const
    {
      return get(key, pmt::get_PMT_NIL());
    }

    pmt::pmt_t
    meta_namespace::get() const
    {
      return d_data;
    }

    std::string
    meta_namespace::get_str(const std::string &key) const
    {
      pmt::pmt_t ref = pmt::dict_ref(d_data, pmt::mp(key), pmt::get_PMT_NIL());
      if(pmt::eqv(ref, pmt::get_PMT_NIL())) {
        throw std::runtime_error("key not found");
      } else if(!pmt::is_symbol(ref)) {
        throw std::runtime_error("val is not str");
      }
      return pmt::symbol_to_string(ref);
    }

    bool
    meta_namespace::has(const std::string &key) const
    {
      return pmt::dict_has_key(d_data, pmt::mp(key));
    }

    std::set<std::string>
    meta_namespace::keys() const
    {
      pmt::pmt_t keys_pmt = pmt::dict_keys(d_data);
      std::set<std::string> keys;
      size_t num_keys = pmt::length(keys_pmt);
      for(size_t i = 0; i < num_keys; i++) {
        keys.insert(pmt::symbol_to_string(pmt::nth(i, keys_pmt)));
      }
      return keys;
    }

    bool
    meta_namespace::validate_key(const std::string &key)
    {
      std::regex key_regex("(^\\w+:\\w+$)");
      return std::regex_match(key, key_regex);
    }

    void
    meta_namespace::del(const std::string &key)
    {
      d_data = pmt::dict_delete(d_data, pmt::mp(key));
    }

    void
    meta_namespace::print() const
    {
      pmt::print(d_data);
    }

    void to_json(json& j, const meta_namespace& ns) {
      j = ns.d_data;
    }
    void from_json(const json& j, meta_namespace& ns) {
      ns.d_data = j;
    }
  } // namespace sigmf
} // namespace gr

namespace pmt {
  void
  to_json(json &j, const pmt::pmt_t &pmt_data)
  {
    if(pmt::is_dict(pmt_data)) {
      pmt::pmt_t item_keys = pmt::dict_keys(pmt_data);
      size_t num_items = pmt::length(item_keys);
      for(size_t i = 0; i < num_items; i++) {
        pmt::pmt_t item_key = pmt::nth(i, item_keys);
        pmt::pmt_t val_for_key = pmt::dict_ref(pmt_data, item_key, pmt::get_PMT_NIL());
        ::std::string key_str = pmt::symbol_to_string(item_key);
        j[key_str] = val_for_key;
      }
    } else if(pmt::is_bool(pmt_data)) {
      j = pmt::to_bool(pmt_data);
    } else if(pmt::is_integer(pmt_data)) {
      j = pmt::to_long(pmt_data);
    } else if(pmt::is_real(pmt_data)) {
      j = pmt::to_double(pmt_data);
    } else if(pmt::is_vector(pmt_data)) {
      size_t num_items = pmt::length(pmt_data);
      for(size_t i = 0; i < num_items; i++) {
        pmt::pmt_t item = pmt::vector_ref(pmt_data, i);
        j[i] = item;
      }
    } else if(pmt::is_symbol(pmt_data)) {
      j = pmt::symbol_to_string(pmt_data);
    } else if(pmt::is_uint64(pmt_data)) {
      j = pmt::to_uint64(pmt_data);
    } else {
      throw ::std::runtime_error("Unhandled pmt value in to_json");
    }
  }
  void from_json(const json& j, pmt_t& pmt_data) {
    if(j.is_object()) {
      pmt_data = pmt::make_dict();
      for(auto itr = j.begin(); itr != j.end(); ++itr) {
        std::string key_str = itr.key();
        pmt::pmt_t key = pmt::string_to_symbol(key_str);
        if(key_str == "core:sample_rate") {
          // Coerce this to a double to prevent badness
          pmt::pmt_t val_for_key = pmt::from_double(itr.value().get<double>());
          pmt_data = pmt::dict_add(pmt_data, key, val_for_key);
        } else {
          pmt::pmt_t val_for_key = itr.value();
          pmt_data = pmt::dict_add(pmt_data, key, val_for_key);
        }
      }
    } else if(j.is_array()) {
      pmt_data = pmt::make_vector(j.size(), pmt::get_PMT_NIL());
      size_t index = 0;
      for(auto itr = j.begin(); itr != j.end(); ++itr) {
        pmt::vector_set(pmt_data, index, *itr);
        index++;
      }
    } else if(j.is_boolean()) {
      pmt_data = pmt::from_bool(j.get<bool>());
    } else if(j.is_number_unsigned()) {
      pmt_data = pmt::from_uint64(j.get<uint64_t>());
    } else if(j.is_number_integer()) {
      pmt_data = pmt::from_long(j.get<int64_t>());
    } else if(j.is_number_float()) {
      pmt_data = pmt::from_double(j.get<double>());
    } else if(j.is_null()) {
      pmt_data = pmt::get_PMT_NIL();
    } else if(j.is_string()) {
      pmt_data = pmt::string_to_symbol(j.get<std::string>());
    }
  }
}
