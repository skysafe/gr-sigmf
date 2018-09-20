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
#include <csignal>
#include <ctime>
#include <chrono>
#include <future>
#include <thread>
#include <gnuradio/blocks/head.h>
#include <gnuradio/top_block.h>
#include <gnuradio/uhd/usrp_source.h>
#include <sigmf/meta_namespace.h>
#include <sigmf/sigmf_utils.h>
#include <sigmf/sink.h>
#include <sigmf/usrp_gps_message_source.h>

#include <boost/algorithm/string/find.hpp>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include "app_utils.h"
#include "sigmf/sigmf_utils.h"

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
  boost::regex format_regex("(r|c)(\\w)(\\d+)_(le|be)");
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
  ss << "Ettus ";
  ss << usrp_info["mboard_id"];
  std::vector<std::string> info_keys = usrp_info.keys();
  for(std::string &key: info_keys) {
    if (key == "mboard_id") {
      continue;
    }
    ss << " / " << key << ": " << usrp_info[key];
  }
  return ss.str();
}

std::promise<int> quit_promise;
std::future<int> quit_future = quit_promise.get_future();

static const int PROMISE_FROM_INT = 0;
static const int PROMISE_FROM_THREAD = 1;

void
sig_int_handler(int /* unused */)
{
  try {
    quit_promise.set_value(PROMISE_FROM_INT);
  }
  catch (const std::future_error &e) {
    // Swallow this exception, since there's nothing that can be done if the promise
    // has an error and it probably just means that the flowgraph is slow to
    // shutdown
  }
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
    ("help,h", "Show help message")
    ("args,a", po::value<std::string>(&device_args)->default_value(""), "Argument string for UHD")
    ("cpu-format", po::value<std::string>(&cpu_format_str)->default_value("sc16"), "Format of saved data")
    ("wire-format", po::value<std::string>(&wire_format_str)->default_value(""), "Format of OTW data")
    ("freq,f", po::value<double>(&center_freq)->required(), "Center frequency in hertz")
    ("int-n", "Tune USRP LO in integer-N PLL mode")
    ("skip-gps", "Skip attempting to sync to GPS")
    ("sample-rate,s", po::value<double>(&sample_rate)->default_value(100e6/16), "Sample rate in samples/second")
    ("gain,g", po::value<double>(&gain)->default_value(0), "Gain in db")
    ("normalized-gain", po::value<double>(&normalized_gain), "Normalized gain")
    ("antenna", po::value<std::string>(&antenna), "Antenna to select on USRP")
    ("bandwidth", po::value<double>(&bandwidth), "Bandwidth to select on USRP")
    ("subdev-spec",po::value<std::string>(&subdev_spec), "Subdev spec for USRP")
    ("description", po::value<std::string>(&description)->default_value(""), "Description of this recording")
    ("author", po::value<std::string>(&author), "Author for this recording")
    ("license", po::value<std::string>(&license), "License for this recoridng")
    ("hardware", po::value<std::string>(&hardware)->default_value(""), "Set hardware for this recording (if left empty, then USRP will be queried)")
    ("duration", po::value<double>(&duration_seconds), "If set, then only capture for this many seconds")
    ("overwrite", po::bool_switch(&overwrite)->default_value(false), "Overwrite output file")
    ("global-meta", po::value<std::vector<std::string> >(&extra_global_meta), "Additional global metadata")
    ("output-file", po::value<std::string>(&output_filename)->default_value(""), "File to write to");
  // clang-format on
  po::positional_options_description positional_options;
  positional_options.add("output-file", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(main_options)
              .positional(positional_options)
              .run(),
            vm);

  if(vm.count("help")) {
    std::cout << "Capture a SigMF recording via a UHD device." << std::endl << std::endl;
    std::cout << boost::format("Usage: %s [options] <filename>") % argv[0] << std::endl << std::endl;
    std::cout << main_options << std::endl;
    return ~0;
  }

  try {
    po::notify(vm);
  } catch(po::error &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }



  // TODO: DO i need to make this check? or can I just let stuff explode...
  if(!check_valid_uhd_format(cpu_format_str)) {
    std::cerr << "Supplied cpu format str is invalid\n";
    return -1;
  }
  // Have to use the vars to check since gain has a default value
  auto gain_var = vm["gain"];
  auto normalized_gain_var = vm["normalized-gain"];
  if ((!gain_var.empty() && !gain_var.defaulted()) && !normalized_gain_var.empty()) {
    std::cerr << "Can't set gain and normalized gain at the same time!\n";
    return -1;
  }

  std::cout << std::endl;
  std::cout << boost::format("Creating the usrp device with: %s...") % device_args << std::endl;
  uhd::device_addr_t device_addr(device_args);
  uhd::stream_args_t stream_args(cpu_format_str, wire_format_str);
  gr::uhd::usrp_source::sptr usrp_source(gr::uhd::usrp_source::make(device_addr, stream_args));

  // subdev setting has to be the first thing we do or it leads to bugs
  if(vm.count("subdev-spec")) {
    std::cout << boost::format("Setting subdev spec to: %s") % subdev_spec << std::endl << std::endl;
    usrp_source->set_subdev_spec(subdev_spec);
  }

  std::cout << boost::format("Setting RX Rate: %f MSps...") % (sample_rate/1e6) << std::endl;
  usrp_source->set_samp_rate(sample_rate);
  std::cout << boost::format("Actual RX Rate: %f MSps...") % (usrp_source->get_samp_rate()/1e6) << std::endl << std::endl;

  std::cout << boost::format("Setting RX Freq: %f MHz...") % (center_freq/1e6) << std::endl;
  uhd::tune_request_t tune_request(center_freq);
  if(vm.count("int-n")) {
    std::cout << "Configuring PLL in integer-N mode..." << std::endl;
    tune_request.args = uhd::device_addr_t("mode_n=integer");
  }
  usrp_source->set_center_freq(tune_request);
  std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp_source->get_center_freq()/1e6) << std::endl << std::endl;

  if(vm.count("gain")) {
    std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
    usrp_source->set_gain(gain);
    std::cout << boost::format("Actual RX Gain: %f dB...") % usrp_source->get_gain() << std::endl << std::endl;
  } else if(vm.count("normalized-gain")) {
    std::cout << boost::format("Setting RX Normalized Gain: %f dB...") % normalized_gain << std::endl;
    usrp_source->set_normalized_gain(normalized_gain);
    std::cout << boost::format("Actual RX Normalized Gain: %f dB...") % usrp_source->get_normalized_gain() << std::endl << std::endl;
  }

  // Optional params
  if(vm.count("antenna")) {
    std::cout << boost::format("Setting antenna to: %s") % antenna << std::endl << std::endl;
    usrp_source->set_antenna(antenna);
  }
  if(vm.count("bandwidth")) {
    std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bandwidth/1e6) << std::endl;
    usrp_source->set_bandwidth(bandwidth);
    std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp_source->get_bandwidth()/1e6) << std::endl << std::endl;
  }

  bool do_gps_sync = false;
  if(!vm.count("skip-gps")) {
    std::cout << "Looking for GPS sensor..." << std::endl;
    std::vector<std::string> sensor_names = usrp_source->get_mboard_sensor_names(0);
    if(std::find(sensor_names.begin(), sensor_names.end(), "gps_locked") != sensor_names.end()) {
      bool gps_locked = usrp_source->get_mboard_sensor("gps_locked", 0).to_bool();
      if(gps_locked) {
        std::cout << "GPS locked, attempting to sync time to GPS..." << std::endl;
        do_gps_sync = true;
      } else {
        std::cout << "GPS not locked, skipping GPS time sync." << std::endl << std::endl;
      }
    } else {
      std::cout << "No GPS sensor found, skipping GPS time sync." << std::endl << std::endl;
    }
  } else {
    std::cout << "Skipping GPS time sync." << std::endl << std::endl;
  }

  if(do_gps_sync) {
    std::cout << "Setting clock and time source to GPSDO." << std::endl;
    usrp_source->set_clock_source("gpsdo");
    usrp_source->set_time_source("gpsdo");

    uhd::time_spec_t gps_time = uhd::time_spec_t(time_t(usrp_source->get_mboard_sensor("gps_time", 0).to_int()));
    std::cout << "Got GPS time: " << (boost::format("%0.9f") % gps_time.get_real_secs()) << std::endl;
    usrp_source->set_time_next_pps(gps_time + 1.0);

    std::cout << "Waiting for GPS PPS edge." << std::endl;
    boost::this_thread::sleep(boost::posix_time::seconds(2));

    std::cout << "Checking times to see if sync was successful..." << std::endl;
    gps_time = uhd::time_spec_t(time_t(usrp_source->get_mboard_sensor("gps_time", 0).to_int()));
    uhd::time_spec_t time_last_pps = usrp_source->get_time_last_pps(0);

    std::cout << "New USRP time: " << (boost::format("%0.9f") % time_last_pps.get_real_secs()) << std::endl;
    std::cout << "New GPSDO time: " << (boost::format("%0.9f") % gps_time.get_real_secs()) << std::endl;
    if (gps_time.get_real_secs() == time_last_pps.get_real_secs()) {
      std::cout << std::endl << "USRP time successfully synchronized to GPS time." << std::endl << std::endl;
    } else {
      std::cerr << std::endl << "ERROR: Failed to synchronize USRP time to GPS time." << std::endl << std::endl;
      return -1;
    }
  }

  gr::sigmf::usrp_gps_message_source::sptr gps_source(
    gr::sigmf::usrp_gps_message_source::make(device_addr, 1.0));

  uhd::dict<std::string, std::string> usrp_info = usrp_source->get_usrp_info();

  if(output_filename == "") {
    // Then generate a filename
    output_filename = generate_filename(usrp_info["mboard_id"], center_freq, sample_rate, gain);
  }

  std::string sigmf_format = uhd_format_to_sigmf_format(cpu_format_str);

  if(!overwrite && fs::exists(gr::sigmf::to_data_path(output_filename))) {
    std::cerr << "Error: specified output file already exists. To overwrite it, set the --overwrite flag." << std::endl;
    return -1;
  }

  // make a sink block
  gr::sigmf::sink::sptr file_sink(
    gr::sigmf::sink::make(sigmf_format, output_filename));

  file_sink->set_global_meta("core:sample_rate", pmt::mp(sample_rate));
  file_sink->set_global_meta("core:description", pmt::mp(description));
  file_sink->set_global_meta("core:author", pmt::mp(author));
  file_sink->set_global_meta("core:license", pmt::mp(license));
  file_sink->set_global_meta("core:recorder", pmt::mp("sigmf_record"));
  file_sink->set_global_meta("core:hw", pmt::mp(hardware != "" ? hardware : generate_hw_name(usrp_info)));

  // Check what the gain is and set that on the first capture segment
  double gain_at_start = usrp_source->get_gain();
  file_sink->set_capture_meta(0, "uhd:rx_gain", pmt::mp(gain_at_start));

  std::cout << "Writing SigMF recording to:" << std::endl;
  std::cout << "  Samples: " << file_sink->get_data_path() << std::endl;
  std::cout << "  Metadata: " << file_sink->get_meta_path() << std::endl;

  // Add any extra global metadata
  for(size_t i = 0; i < extra_global_meta.size(); i++) {
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
    }
  }

  gr::top_block_sptr tb(gr::make_top_block("sigmf_record"));

  // connect blocks
  uint64_t samples_for_duration;
  if(vm.count("duration")) {
    size_t sample_size = format_str_to_size(sigmf_format);
    samples_for_duration =
      static_cast<uint64_t>(std::ceil(duration_seconds * sample_rate));
    std::cout << "samples_for_duration: " << samples_for_duration << std::endl;
    std::cout << "sample_size: " << sample_size << std::endl;
    gr::blocks::head::sptr head_block = gr::blocks::head::make(sample_size, samples_for_duration);

    tb->connect(usrp_source, 0, head_block, 0);
    tb->connect(head_block, 0, file_sink, 0);
  } else {
    tb->connect(usrp_source, 0, file_sink, 0);
  }

  tb->msg_connect(gps_source, "out", file_sink, "gps");
  // This allows the flowgraph to terminate when the Sink is done writing data,
  // by notifying the GPS Source that it should stop.
  tb->msg_connect(file_sink, "system", gps_source, "system");

  // Handle the interrupt signal
  std::signal(SIGINT, &sig_int_handler);
  std::cout << std::endl << "Press Ctrl + C to stop streaming..." << std::endl;

  std::thread tb_thread([&tb](){
    tb->start();
    tb->wait();
    try {
      quit_promise.set_value(PROMISE_FROM_THREAD);
    } catch (const std::future_error &e) {
      // Nothing to do here
    }
  });
  if (vm.count("duration")) {
    // We wait in two phases here
    // first we wait for the expected time + 1
    auto status = quit_future.wait_for(std::chrono::duration<double>(duration_seconds + 1));
    std::string seconds = duration_seconds == 1 ? "second" : "seconds";
    if (status == std::future_status::ready) {
      int quit_int = quit_future.get();
      if (quit_int == PROMISE_FROM_INT) {
        std::cout << "User requested early exit, stopping flowgraph..." << std::endl;
        tb->stop();
      }
    } else if (status == std::future_status::timeout) {
      // Second phase, just wait now
      std::cout << "\033[1;33m" << "Warning: Receiving " << samples_for_duration
                << " samples should have taken " << duration_seconds << " " << seconds
                << ", but still waiting for samples!" << "\033[0m" <<  std::endl;
      quit_future.wait();
      tb->stop();
    }
  } else {
    quit_future.wait();
    tb->stop();
  }
  tb_thread.join();
}
