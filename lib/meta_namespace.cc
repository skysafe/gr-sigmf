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
#include <boost/regex.hpp>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/reader.h>
#include <rapidjson/writer.h>

using namespace rapidjson;
namespace gr {
  namespace sigmf {

    metafile_namespaces
    load_metafile(FILE *fp)
    {
      char buffer[65536];
      metafile_namespaces meta_ns;
      FileReadStream file_stream(fp, buffer, sizeof(buffer));
      Document doc;
      doc.ParseStream<0, UTF8<>, FileReadStream>(file_stream);

      if(doc.HasParseError()) {
        throw std::runtime_error("Meta namespace parse error - invalid metadata.");
      }

      meta_ns.global = json_value_to_pmt(doc["global"]);
      size_t num_captures = doc["captures"].Size();

      for(size_t i = 0; i < num_captures; i++) {
        meta_ns.captures.push_back(json_value_to_pmt(doc["captures"][i]));
      }
      size_t num_annotations = doc["annotations"].Size();

      for(size_t i = 0; i < num_annotations; i++) {
        meta_ns.annotations.push_back(json_value_to_pmt(doc["annotations"][i]));
      }
      return meta_ns;
    }

    pmt::pmt_t
    json_value_to_pmt(const Value &val)
    {
      if(val.IsObject()) {
        pmt::pmt_t obj = pmt::make_dict();
        for(Value::ConstMemberIterator itr = val.MemberBegin(); itr != val.MemberEnd(); ++itr) {
          std::string key_str = itr->name.GetString();
          pmt::pmt_t key = pmt::string_to_symbol(key_str);
          if (key_str == "core:sample_rate") {
            // Coerce this to a double to prevent badness
            pmt::pmt_t val_for_key = pmt::from_double(itr->value.GetDouble());
            obj = pmt::dict_add(obj, key, val_for_key);
          } else {
            pmt::pmt_t val_for_key = json_value_to_pmt(itr->value);
            obj = pmt::dict_add(obj, key, val_for_key);
          }
        }
        return obj;
      } else if(val.IsArray()) {
        pmt::pmt_t array = pmt::make_vector(val.Size(), pmt::get_PMT_NIL());
        size_t index = 0;
        for(Value::ConstValueIterator itr = val.Begin(); itr != val.End(); ++itr) {
          pmt::vector_set(array, index, json_value_to_pmt(*itr));
          index++;
        }
        return array;
      } else if(val.IsBool()) {
        return pmt::from_bool(val.GetBool());
      } else if(val.IsUint64()) {
        return pmt::from_uint64(val.GetUint64());
      } else if(val.IsInt64()) {
        return pmt::from_long(val.GetInt64());
      } else if (val.IsInt()) {
        return pmt::from_long(val.GetInt());
      } else if(val.IsDouble()) {
        return pmt::from_double(val.GetDouble());
      } else if(val.IsNull()) {
        return pmt::get_PMT_NIL();
      } else if(val.IsString()) {
        return pmt::string_to_symbol(val.GetString());
      } else {
        std::cerr << "Invalid type in json" << std::endl;
        return NULL;
      }
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
      boost::regex key_regex("(^\\w+:\\w+$)");
      return boost::regex_match(key, key_regex);
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

  } // namespace sigmf
} // namespace gr
