#ifndef INCLUDED_SIGMF_TIME_MODE_H
#define INCLUDED_SIGMF_TIME_MODE_H

#include <sigmf/api.h>

namespace gr {
  namespace sigmf {

    // NOTE: We don't actually care about the underlying type
    // of this enum, but if left unspecified, this triggers a bug
    // in versions of gcc < 6 (see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=43407)
    enum class sigmf_time_mode: int SIGMF_API{
      absolute,
      relative
    };
  }
}

#endif /* INCLUDED_SIGMF_TIME_MODE_H */