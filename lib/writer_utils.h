#ifndef INCLUDED_SIGMF_WRITER_UTILS_H
#define INCLUDED_SIGMF_WRITER_UTILS_H

#include <cstdio>
#include <vector>
#include "sigmf/meta_namespace.h"

/**
 * Internal functions shared between blocks that
 * need to write data
 */
namespace gr {
  namespace sigmf {
    namespace writer_utils {
        
      /**
       * Write given metadata data set to file.
       * Assumes file is already open and does not
       * close the file when finished
       */
      void write_meta_to_fp(FILE *fp,
                            const meta_namespace &global,
                            std::vector<meta_namespace> &captures,
                            std::vector<meta_namespace> &annotations);
    }
  } // namespace sigmf
} // namespace gr
#endif /* INCLUDED_SIGMF_WRITER_UTILS_H */