#ifndef INCLUDED_SIGMF_PMT_UTILS_H
#define INCLUDED_SIGMF_PMT_UTILS_H

#include <utility>
#include <cstdint>
#include <pmt/pmt.h>

namespace gr {
  namespace sigmf {
    namespace pmt_utils {
      inline std::pair<uint64_t, double>
      extract_uhd_time(pmt::pmt_t uhd_time)
      {
        uint64_t seconds = pmt::to_uint64(pmt::tuple_ref(uhd_time, 0));
        double frac_seconds = pmt::to_double(pmt::tuple_ref(uhd_time, 1));
        return std::make_pair(seconds, frac_seconds);
      }
    } // namespace pmt_utils
  } // namespace sigmf
} // namespace gr
#endif /* INCLUDED_SIGMF_PMT_UTILS_H */