#include "writer_utils.h"

namespace gr {
  namespace sigmf {
    namespace writer_utils {
      void
      write_meta_to_fp(FILE *fp,
                       const meta_namespace &global,
                       const std::vector<meta_namespace> &captures, 
                       const std::vector<meta_namespace> &annotations)
      {
        json meta_record;
        meta_record["global"] = global;
        meta_record["captures"] = captures;
        // Make a copy of annotations so we can sort them
        std::vector<meta_namespace> anno_copy{annotations};
        // sort annotations
        std::sort(anno_copy.begin(), anno_copy.end(),
                  [](const meta_namespace &a, const meta_namespace &b) {
                    return pmt::to_uint64(a.get("core:sample_start")) <
                      pmt::to_uint64(b.get("core:sample_start"));
                  });
        meta_record["annotations"] = anno_copy;
        std::string json_str = meta_record.dump(4);
        std::fwrite(json_str.data(), 1, json_str.length(), fp);
      }
    } // namespace writer_utils
  } // namespace sigmf
} // namespace gr
