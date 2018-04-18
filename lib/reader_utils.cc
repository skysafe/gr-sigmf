#include "reader_utils.h"
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/conversion.hpp>

namespace posix = boost::posix_time;

namespace gr {
  namespace sigmf {
    namespace reader_utils {
      posix::ptime
      iso_string_to_ptime(const std::string &str)
      {
        boost::posix_time::ptime time;
        std::stringstream ss(str);
        boost::local_time::local_time_input_facet *ifc =
          new boost::local_time::local_time_input_facet();
        ifc->set_iso_extended_format();
        ss.imbue(std::locale(ss.getloc(), ifc));
        boost::local_time::local_date_time zonetime(boost::local_time::not_a_date_time);
        if(ss >> zonetime) {
          time = zonetime.utc_time();
        }
        return time;
      }

      pmt::pmt_t
      ptime_to_uhd_time(const boost::posix_time::ptime &time)
      {
        uint64_t seconds = static_cast<uint64_t>(posix::to_time_t(time));
        auto tod = time.time_of_day();
        auto tick_seconds = tod.total_seconds() * tod.ticks_per_second();
        auto frac_ticks = tod.ticks() - tick_seconds;
        double frac_seconds = static_cast<double>(frac_ticks) / tod.ticks_per_second();
        return pmt::make_tuple(pmt::mp(seconds), pmt::mp(frac_seconds));
      }
    } // namespace reader_utils
  }   // namespace sigmf
} // namespace gr