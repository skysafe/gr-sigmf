#pragma once
#include <pmt/pmt.h>

namespace gr {
  namespace sigmf {

    static const pmt::pmt_t TIME_KEY = pmt::string_to_symbol("rx_time");
    static const pmt::pmt_t RATE_KEY = pmt::string_to_symbol("rx_rate");
    static const pmt::pmt_t FREQ_KEY = pmt::string_to_symbol("rx_freq");
    static const pmt::pmt_t PACKET_LEN_KEY = pmt::string_to_symbol("packet_len");

  } // namespace sigmf
} // namespace gr