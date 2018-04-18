#ifndef INCLUDED_SIGMF_READER_UTILS_H
#define INCLUDED_SIGMF_READER_UTILS_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <pmt/pmt.h>

/**
 * Common utilities used by readers of sigmf files
 */
namespace gr {
  namespace sigmf {
    namespace reader_utils {
      boost::posix_time::ptime iso_string_to_ptime(const std::string &str);
      pmt::pmt_t ptime_to_uhd_time(const boost::posix_time::ptime &time);

    }
  } // namespace sigmf
} // namespace gr

#endif /* INCLUDED_SIGMF_READER_UTILS_H */