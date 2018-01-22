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

#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/thread/future.hpp>
#include <csignal>
#include <ctime>
#include <gnuradio/blocks/head.h>
#include <gnuradio/top_block.h>
#include <gnuradio/uhd/usrp_source.h>
#include <sigmf/meta_namespace.h>
#include <sigmf/sigmf_utils.h>
#include <sigmf/sink.h>

#include <boost/algorithm/string/find.hpp>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include "app_utils.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace algo = boost::algorithm;
namespace json = rapidjson;

bool
check_valid_uhd_format(const std::string &str)
{
  return (str == "fc64" || str == "fc32" || str == "sc16" || str == "sc8");
}

size_t
format_str_to_size(const std::string &format_str)
{
  boost::regex format_regex("(r|c)(\\w)(\\d +)");
  boost::smatch result;
  if(boost::regex_search(format_str, result, format_regex)) {
    int multiplier = result[1] == "c" ? 2 : 1;
    int num_bits = boost::lexical_cast<int>(result[3]);
    return (num_bits / 8) * multiplier;
  } else {
    return 0;
  }
}

/**
 * Convert a number specifiying an amount in hertz to a nice string
 */
template <typename T>
std::string
hertz_num_to_str(T num, int precision = 2)
{
  std::string unit;
  double divider;

  // then THz
  if(num > 1e12) {
    unit = "THz";
    divider = 1e12;
  }

  // Then GHz
  else if(num > 1e9) {
    unit = "GHz";
    divider = 1e9;
  }

  // Then MHz
  else if(num > 1e6) {
    unit = "MHz";
    divider = 1e6;
  }

  // Then KHz
  else if(num > 1e3) {
    unit = "KHz";
    divider = 1e3;
  }

  // then Hz
  else {
    unit = "Hz";
    divider = 1;
  }

  std::stringstream ss;
  double converted = num / divider;
  std::string formatStr = "%." + boost::lexical_cast<std::string>(precision) + "f";
  ss << boost::format(formatStr) % converted;
  ss << unit;
  return ss.str();
}

std::string
generate_filename(const std::string &sdr_name, double center_freq, double sample_rate, double gain)
{
  std::time_t cur_time = std::time(NULL);
  std::tm *local_time = std::localtime(&cur_time);

  std::stringstream ss;
  ss << "sigmf-" << sdr_name << "-f" << hertz_num_to_str(center_freq) << "-r"
     << hertz_num_to_str(sample_rate) << "-g" << gain << "-" << local_time->tm_year
     << local_time->tm_mon << local_time->tm_mday << local_time->tm_hour
     << local_time->tm_min << local_time->tm_sec << ".sigmf-data";
  return ss.str();
}

std::string
generate_hw_name(const uhd::dict<std::string, std::string> &usrp_info)
{
  std::stringstream ss;
  ss << usrp_info["mboard_id"];
  if(usrp_info.has_key("mboard_serial")) {
    std::string serial = usrp_info["mboard_serial"];
    if(serial != "") {
      ss << "/ " << serial;
    }
  }
  return ss.str();
}

boost::promise<int> quit_promise;
boost::unique_future<int> quit_future = quit_promise.get_future();

void
sig_int_handler(int /* unused */)
{
  // Don't actually care about the value, just abusing future for one-shot signaling
  quit_promise.set_value(0);
}

int
main(int argc, char *argv[])
{
  // USRP arguments
  std::string cpu_format_str;
  std::string wire_format_str;
  std::string device_args;
  std::string antenna;
  std::string subdev_spec;
  double center_freq;
  double sample_rate;
  double gain;
  double normalized_gain;
  double bandwidth;

  // SigMF arguments
  std::string output_filename;
  std::string description;
  std::string author;
  std::string license;
  std::string hardware;

  std::vector<std::string> extra_global_meta;

  // behavioral arguments
  bool overwrite;
  double duration_seconds;

  // TODOs: verbose, gps
  po::options_description main_options("Allowed options");
  // Need to tell clang-format to not format this
  // clang-format off
  main_options.add_options()
    ("help,h", "show help message")
    ("args,a", po::value<std::string>(&device_args)->default_value(""),"Argument string for usrp")
    ("cpu-format", po::value<std::string>(&cpu_format_str)->default_value("fc32"),"Format of saved data")
    ("wire-format", po::value<std::string>(&wire_format_str)->default_value(""),"Format of otw data")
    ("center-freq,f", po::value<double>(&center_freq)->required(),"Center frequency in hertz")
    ("sample-rate,s", po::value<double>(&sample_rate)->required(),"Sample rate in samples/second")
    ("gain,g", po::value<double>(&gain), "Gain in db")
    ("normalized-gain", po::value<double>(&normalized_gain),"Normalized gain")
    ("antenna", po::value<std::string>(&antenna),"Antenna for usrp")
    ("bandwidth", po::value<double>(&bandwidth),"Bandwidth for usrp")
    ("subdev-spec",po::value<std::string>(&subdev_spec),"Subdev spec for usrp")
    ("description", po::value<std::string>(&description)->default_value(""),"Description of this recording")
    ("author", po::value<std::string>(&author),"Author for this recording")
    ("license", po::value<std::string>(&license), "License for this recoridng")
    ("hardware", po::value<std::string>(&hardware)->default_value(""),"Set hardware for this recording (if left empty, then usrp will be queried)")
    ("duration", po::value<double>(&duration_seconds),"If set, then only capture for this many seconds")
    ("overwrite", po::bool_switch(&overwrite)->default_value(false),"Overwrite output file")
    ("global-meta", po::value<std::vector<std::string> >(&extra_global_meta), "Additional global metadata")
    ("output-file", po::value<std::string>(&output_filename)->default_value(""),"File to write to");
  // clang-format on
  po::positional_options_description positional_options;
  positional_options.add("output-file", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(main_options)
              .positional(positional_options)
              .run(),
            vm);
  po::notify(vm);

  if(vm.count("help")) {
    std::cout << main_options << "\n";
    return 1;
  }

  // TODO: DO i need to make this check? or can I just let stuff explode...
  if(!check_valid_uhd_format(cpu_format_str)) {
    std::cerr << "Supplied cpu format str is invalid\n";
    return -1;
  }

  if(vm.count("gain") && vm.count("normalized-gain")) {
    std::cerr << "Can't set gain and normalized gain!\n";
    return -1;
  }
  if(!vm.count("gain") && !vm.count("normalized-gain")) {
    std::cerr << "No gain supplied!\n";
    return -1;
  }


  // Make a usrp source
  uhd::device_addr_t device_addr(device_args);
  uhd::stream_args_t stream_args(cpu_format_str, wire_format_str);
  gr::uhd::usrp_source::sptr usrp_source(gr::uhd::usrp_source::make(device_addr, stream_args));

  // Required params
  usrp_source->set_center_freq(center_freq);
  usrp_source->set_samp_rate(sample_rate);
  if(vm.count("gain")) {
    usrp_source->set_gain(gain);
  } else if(vm.count("normalized-gain")) {
    usrp_source->set_normalized_gain(normalized_gain);
  }

  // Optional params
  if(vm.count("antenna")) {
    usrp_source->set_antenna(antenna);
  }
  if(vm.count("bandwidth")) {
    usrp_source->set_bandwidth(bandwidth);
  }
  if(vm.count("subdev-spec")) {
    usrp_source->set_subdev_spec(subdev_spec);
  }

  uhd::dict<std::string, std::string> usrp_info = usrp_source->get_usrp_info();

  if(output_filename == "") {
    // Then generate a filename
    output_filename = generate_filename(usrp_info["mboard_id"], center_freq, sample_rate, gain);
  }

  std::string sigmf_format = uhd_format_to_sigmf_format(cpu_format_str);

  // make a sink block
  gr::sigmf::sink::sptr file_sink(
    gr::sigmf::sink::make(sigmf_format, output_filename, sample_rate, description, author,
                          license, hardware != "" ? hardware : generate_hw_name(usrp_info)));

  // Have to wait until here to check for existing files because the sink my change the
  // data file name
  if(!overwrite && fs::exists(file_sink->get_data_path())) {
    std::cerr << "Output file exists and overwrite flag is not set";
    return -1;
  }

  // Add any extra global metadata
  for(int i = 0; i < extra_global_meta.size(); i++) {
    std::string meta = extra_global_meta[i];
    boost::iterator_range<std::string::iterator> second_comma = algo::find_nth(meta, ":", 1);
    std::string key = meta.substr(0, second_comma.begin() - meta.begin());
    std::string val =
      meta.substr(second_comma.begin() - meta.begin() + 1, meta.end() - second_comma.begin());
    json::Document document;
    document.Parse(val.c_str());
    if(document.HasParseError()) {
      json::ParseErrorCode err_code = document.GetParseError();
      if(err_code == json::kParseErrorValueInvalid) {
        // Might be a string that needs quotes
        std::string val_with_quotes = "\"" + val + "\"";
        document.Parse(val_with_quotes.c_str());
        if(document.HasParseError()) {
          std::cerr << "Error parsing metadata value\n";
          std::cout << "damn";
          return -1;
        }
      } else {
        std::cerr << "Error parsing metadata value\n";
        return -1;
      }
    }
    if(document.IsString()) {
      std::string str_val(document.GetString());
      file_sink->set_global_meta(key, pmt::mp(str_val));
      std::cout << document.GetString() << std::endl;
    }
  }

  gr::top_block_sptr tb(gr::make_top_block("sigmf_record"));

  // connect blocks
  if(vm.count("duration")) {
    size_t sample_size = format_str_to_size(sigmf_format);
    uint64_t samples_for_duration =
      static_cast<uint64_t>(std::ceil(duration_seconds * sample_rate));
    gr::blocks::head::sptr head_block = gr::blocks::head::make(sample_size, samples_for_duration);

    tb->connect(usrp_source, 0, head_block, 0);
    tb->connect(head_block, 0, file_sink, 0);
  } else {
    tb->connect(usrp_source, 0, file_sink, 0);
  }

  // Handle the interrupt signal
  std::signal(SIGINT, &sig_int_handler);
  std::cout << "Press Ctrl + C to stop streaming..." << std::endl;

  // run the flow graph
  tb->start();
  std::cout << "Start returned" << std::endl;
  quit_future.wait();
  tb->stop();
  tb->wait();
}
