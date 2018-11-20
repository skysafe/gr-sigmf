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

#include <iostream>
#include <regex>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/find.hpp>
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/skiphead.h>
#include <gnuradio/blocks/head.h>
#include <unistd.h>
#include <stdio.h>
#include <sigmf/sigmf_utils.h>
#include <sigmf/sink.h>
#include <sigmf/source.h>


namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace algo = boost::algorithm;

static const auto YELLOW = isatty(fileno(stdin)) ? "\033[1;33m" : "";
static const auto RED = isatty(fileno(stdin)) ? "\033[1;33m" : "";
static const auto NO_COLOR = isatty(fileno(stdin)) ? "\033[0m" : "";


uint64_t
parse_spec(const std::string &spec_str, pmt::pmt_t samp_rate_pmt) {
  std::cout << spec_str << std::endl;
  std::regex spec_regex(R"(^([0-9]+)((\.[0-9]+)?(m|h|s))?$)");
  std::smatch spec_match;
  bool spec_matched = std::regex_match(spec_str, spec_match, spec_regex);
  if (spec_matched) {
    if (spec_match.length(4) > 0) {
      // Then this is a time match, get everything up to unit as double
      std::string time_str(spec_str.begin(), spec_match[4].first);
      double time_part = boost::lexical_cast<double>(time_str);
      if (pmt::is_null(samp_rate_pmt)) {
        // Throw exception here
        throw std::invalid_argument("No sample rate found in source file, can't do time conversion");
      }
      double samp_rate = pmt::to_double(samp_rate_pmt);
      std::string time_unit = spec_match[4];
      // Convert to seconds
      double mult = 1;
      if (time_unit == "m") {
        mult = 60;
      } else if (time_unit == "h") {
        mult = 3600;
      }
      double samps = (time_part * mult) * samp_rate;
      return std::ceil(samps);
    } else {
      // Then this is just samples, parse as uint64_t and return
      return boost::lexical_cast<uint64_t>(spec_str);
    }
  } else {
    throw std::invalid_argument((boost::format("Invalid time spec: %s") % spec_str).str()) ;
  }
}

int
main(int argc, char *argv[])
{
  // crop positions
  std::string start_spec;
  std::string end_spec;
  std::string length_spec;

  // files
  std::string input_filename;
  std::string output_filename;

  bool overwrite;

  po::options_description main_options("Allowed options");
  // Need to tell clang-format to not format this
  // clang-format off
  main_options.add_options()
    ("help,h", "Show help message")
    ("start,s", po::value<std::string>(&start_spec), "Where to start cropping")
    ("end,e", po::value<std::string>(&end_spec), "Where to end cropping")
    ("length,l", po::value<std::string>(&length_spec), "Length of cropped area")
    ("overwrite", po::bool_switch(&overwrite)->default_value(false), "Overwrite input file")
    ("output-file,o", po::value<std::string>(&output_filename)->default_value(""), "File to write to")
    ("input-file", po::value<std::string>(&input_filename)->required(), "File to crop");
  // clang-format on
  po::positional_options_description positional_options;
  positional_options.add("input-file", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(main_options)
              .positional(positional_options)
              .run(),
            vm);

  if(vm.count("help")) {
    std::cout << "Cut out a section from an existing file" << std::endl << std::endl;
    std::cout << boost::format("Usage: %s [options] <filename>") % argv[0] << std::endl << std::endl;
    std::cout << main_options << std::endl;
    return ~0;
  }

  try {
    po::notify(vm);
  }
  catch(const std::exception &e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  // Count initialized specs
  int init_spec_count = 0;
  init_spec_count += start_spec != "" ? 1 : 0;
  init_spec_count += end_spec != "" ? 1 : 0;
  init_spec_count += length_spec != "" ? 1 : 0;
  if (init_spec_count < 2) {
    std::cerr << "Not enough arguments supplied for crop!\n";
    return 1;
  }
  if (init_spec_count > 2) {
    std::cerr << "Too many arguments supplied for crop!\n";
    return 1;
  }

  // Get the size of the data file
  auto data_path = gr::sigmf::to_data_path(input_filename);
  uint64_t file_size = fs::file_size(data_path);

  // Make a source block
  gr::sigmf::source::sptr file_source(
    gr::sigmf::source::make_no_datatype(input_filename));

  // Get the sample_rate
  pmt::pmt_t sample_rate = file_source->global_meta().get("core:sample_rate");

  // Get the item size
  pmt::pmt_t source_output_type_pmt = file_source->global_meta().get("core:datatype");
  std::string source_output_type = pmt::symbol_to_string(source_output_type_pmt);
  auto parsed_format = gr::sigmf::parse_format_str(source_output_type);
  size_t sample_size = ((parsed_format.width * (parsed_format.is_complex ? 2 : 1)) / 8);

  // Convert file size to num samples
  auto &capture_segments = file_source->capture_segments();
  uint64_t file_size_in_samples = file_size / sample_size;
  if (capture_segments.size() > 0) {
    // Get the sample start of the first capture segment
    pmt::pmt_t sample_start_pmt = capture_segments[0].get("core:sample_start");
    uint64_t sample_start = pmt::to_uint64(sample_start_pmt);
    file_size_in_samples = file_size_in_samples - sample_start;
  }

  // Convert strings to normalized numbers
  uint64_t crop_start = 0;
  uint64_t crop_length = 0;
  if (start_spec != "" && end_spec != "") {
    try {
      crop_start = parse_spec(start_spec, sample_rate);
    }
    catch(const std::exception &e) {
      std::cerr << "Failed to parse crop start!\n" << e.what() << std::endl;
      return -1;
    }
    uint64_t parsed_end;
    try {
      parsed_end = parse_spec(end_spec, sample_rate);
    }
    catch(const std::exception &e) {
      std::cerr << "Failed to parse crop end!\n" << e.what() << std::endl;
      return -1;
    }
    if (parsed_end <= crop_start) {
      std::cerr << "End is before start!" << std::endl;
      return -1;
    }
    crop_length = parsed_end - crop_start;

  } else if (start_spec != "" && length_spec != "") {
    try {
      crop_start = parse_spec(start_spec, sample_rate);
    }
    catch(const std::exception &e) {
      std::cerr << "Failed to parse crop start!\n" << e.what() << std::endl;
      return -1;
    }
    try {
      crop_length = parse_spec(length_spec, sample_rate);
    }
    catch(const std::exception &e) {
      std::cerr << RED << "Failed to parse crop length!\n" << e.what() << NO_COLOR << std::endl;
      return -1;
    }
  } else { // end + length

    uint64_t parsed_end;
    try {
      parsed_end = parse_spec(end_spec, sample_rate);
    }
    catch(const std::exception &e) {
      std::cerr << RED << "Failed to parse crop end!\n" << e.what() << NO_COLOR << std::endl;
      return -1;
    }
    try {
      crop_length = parse_spec(length_spec, sample_rate);
    }
    catch(const std::exception &e) {
      std::cerr << RED << "Failed to parse crop length!\n" << e.what() << NO_COLOR << std::endl;
      return -1;
    }
    crop_start = parsed_end - crop_length;
  }
  // Check if length is 0
  if (crop_length == 0) {
    std::cerr << RED << "Crop length must be greater than 0" << NO_COLOR << std::endl;
    return -1;
  }
  // Check if crop start is inside file
  if (crop_start >= file_size_in_samples) {
    std::cerr << RED << "Crop start is outside file" << NO_COLOR << std::endl;
    return -1;
  }

  // Check if length is too long
  uint64_t max_possible_length = file_size_in_samples - crop_start;
  if (crop_length > max_possible_length) {
    // Just warn about this, but let it go
    std::cout << YELLOW << "Warning: specified limits go beyond the extent of the file" << NO_COLOR << std::endl;
  }

  // Build the rest of the blocks
  gr::blocks::skiphead::sptr skip_head_block = gr::blocks::skiphead::make(
    sample_size, crop_start);
  gr::blocks::head::sptr head_block = gr::blocks::head::make(sample_size, crop_length);

  // Make the file sink
  gr::sigmf::sink::sptr file_sink(
    gr::sigmf::sink::make(source_output_type, output_filename));

  // Copy metadata from the global segment
  gr::sigmf::meta_namespace &global_meta = file_source->global_meta();
  std::set<std::string> global_keys = global_meta.keys();
  for(const std::string &key: global_keys) {
    // sha512 will change
    // sink block handles datatype, no need to do it again
    // offset doesn't make any sense after cropping
    if (key == "core:sha512" || key == "core:datatype" || key == "core:offset") {
      continue;
    }
    file_sink->set_global_meta(key, global_meta.get(key));
  }

  // Make the top block and wire everything up
  gr::top_block_sptr tb(gr::make_top_block("sigmf_crop"));
  tb->connect(file_source, 0, skip_head_block, 0);
  tb->connect(skip_head_block, 0, head_block, 0);
  tb->connect(head_block, 0, file_sink, 0);

  // Run it
  tb->start();
  tb->wait();
}
