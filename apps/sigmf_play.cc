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
#include <csignal>
#include <gnuradio/blocks/head.h>
#include <gnuradio/top_block.h>
#include <gnuradio/uhd/usrp_sink.h>
#include <iostream>
#include <sigmf/source.h>
#include "app_utils.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

gr::top_block_sptr running_tb;
void
sig_int_handler(int /* unused */)
{
  if(running_tb) {
    running_tb->stop();
  }
}

int
main(int argc, char *argv[])
{
  // USRP arguments
  std::string wire_format_str;
  std::string device_args;
  std::string antenna;
  std::string subdev_spec;
  double gain;
  double normalized_gain;
  double bandwidth;
  double sample_rate;
  double center_freq;

  // SigMF arguments
  std::string input_filename;

  // behavioral arguments
  double delay;
  bool repeat;

  // TODO: These args are from tx_samples_from_file, do we need any of them?
  //         ("spb", po::value<size_t>(&spb)->default_value(10000), "samples per buffer")
  //         ("rate", po::value<double>(&rate), "rate of outgoing samples")
  //         ("lo_off", po::value<double>(&lo_off), "Offset for frontend LO in Hz
  //         (optional)")
  //         ("ref", po::value<std::string>(&ref)->default_value("internal"), "reference
  //         source (internal, external, mimo)")
  //         ("int-n", "tune USRP with integer-n tuning")

  po::options_description main_options("Allowed options");
  // clang-format doesn't handle boost-program-options well at all
  // clang-format off
  main_options.add_options()
    ("help,h", "show help message")
    ("args,a", po::value<std::string>(&device_args)->default_value(""), "Argument string for usrp")
    ("wire-format", po::value<std::string>(&wire_format_str)->default_value(""), "Format of otw data")
    ("sample-rate,s", po::value<double>(&sample_rate), "Sample rate in samples/second, only used if not provided in file")
    ("freq,f", po::value<double>(&center_freq), "Center frequency in hertz, only used if not provided in file")
    ("int-n", "Tune USRP LO in integer-N PLL mode")
    ("gain,g", po::value<double>(&gain), "Gain in db")
    ("normalized-gain", po::value<double>(&normalized_gain), "Normalized gain")
    ("antenna", po::value<std::string>(&antenna),"Antenna for usrp")
    ("bandwidth", po::value<double>(&bandwidth), "Bandwidth for usrp")
    ("subdev-spec", po::value<std::string>(&subdev_spec), "Subdev spec for usrp")
    ("delay", po::value<double>(&delay)->default_value(0.0), "Specify a delay between repeated transmission of file")
    ("repeat", po::bool_switch(&repeat)->default_value(false), "Repeatedly transmit file")
    ("input-file", po::value<std::string>(&input_filename)->default_value(""), "File to read from");
  // clang-format on
  po::positional_options_description positional_options;
  positional_options.add("input-file", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(main_options)
              .positional(positional_options)
              .run(),
            vm);
  po::notify(vm);

  if(vm.count("help")) {
    std::cout << "Play back a SigMF recording via a UHD device." << std::endl << std::endl;
    std::cout << boost::format("Usage: %s [options] <filename>") % argv[0] << std::endl << std::endl;
    std::cout << main_options << std::endl;
    return ~0;
  }

  if(vm.count("gain") && vm.count("normalized-gain")) {
    std::cerr << "Can't set gain and normalized gain!\n";
    return -1;
  }
  if(!vm.count("gain") && !vm.count("normalized-gain")) {
    std::cerr << "No gain supplied!\n";
    return -1;
  }

  // Make a usrp sink
  uhd::device_addr_t device_addr(device_args);
  std::string cpu_format_str = "fc32";
  uhd::stream_args_t stream_args(cpu_format_str, wire_format_str);
  gr::uhd::usrp_sink::sptr usrp_sink(gr::uhd::usrp_sink::make(device_addr, stream_args));

  // Make a file source
  std::string sigmf_format = uhd_format_to_sigmf_format(cpu_format_str);
  gr::sigmf::source::sptr file_source(
    gr::sigmf::source::make(input_filename, sigmf_format, repeat));

  // Look up sample rate in the metadata
  pmt::pmt_t sample_rate_pmt = file_source->global_meta().get("core:sample_rate");
  if(pmt::is_null(sample_rate_pmt)) {
    if(vm.count("sample-rate") == 0) {
      std::cerr << "Error: no sample rate found in metadata and none provided on command line.\n";
      return -1;
    }
  } else {
    sample_rate = pmt::to_double(sample_rate_pmt);
  }
  usrp_sink->set_samp_rate(sample_rate);

  // And the same for center_frequency
  pmt::pmt_t frequency_pmt =
    file_source->capture_segments().front().get("core:frequency");
  if(pmt::is_null(frequency_pmt)) {
    if(vm.count("freq") == 0) {
      std::cerr << "Error: no frequency found in metadata and none provided on command line.\n";
      return -1;
    }
  } else {
    center_freq = pmt::to_double(frequency_pmt);
  }

  std::cout << boost::format("Setting TX Freq: %f MHz...") % (center_freq/1e6) << std::endl;
  uhd::tune_request_t tune_request(center_freq);
  if(vm.count("int-n")) {
    std::cout << "Configuring PLL in integer-N mode..." << std::endl;
    tune_request.args = uhd::device_addr_t("mode_n=integer");
  }
  usrp_sink->set_center_freq(center_freq);
  std::cout << boost::format("Actual TX Freq: %f MHz...") % (usrp_sink->get_center_freq()/1e6) << std::endl << std::endl;

  // This gets set from the args
  if(vm.count("gain")) {
    usrp_sink->set_gain(gain);
  } else if(vm.count("normalized-gain")) {
    usrp_sink->set_normalized_gain(normalized_gain);
  }

  gr::top_block_sptr tb(gr::make_top_block("sigmf_play"));
  tb->connect(file_source, 0, usrp_sink, 0);

  // Handle the interrupt signal
  std::signal(SIGINT, &sig_int_handler);
  std::cout << "Press Ctrl + C to stop streaming..." << std::endl;

  // run the flow graph
  running_tb = tb;
  tb->start();
  tb->wait();
}
