#include "writer_utils.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

namespace gr {
  namespace sigmf {
    namespace writer_utils {
      void
      write_meta_to_fp(FILE *fp,
                       const meta_namespace &global,
                       std::vector<meta_namespace> &captures, // TODO: These should
                                                              // probably be const
                       std::vector<meta_namespace> &annotations)
      {
        char write_buf[65536];
        rapidjson::FileWriteStream file_stream(fp, write_buf, sizeof(write_buf));

        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(file_stream);
        writer.StartObject();

        writer.String("global");
        global.serialize(writer);

        writer.String("captures");
        writer.StartArray();
        for(std::vector<meta_namespace>::iterator it = captures.begin(); it != captures.end(); it++) {
          (*it).serialize(writer);
        }
        writer.EndArray();

        
        // sort annotations
        std::sort(annotations.begin(), annotations.end(),
                  [](const meta_namespace &a, const meta_namespace &b) {
                    // TODO: This may need to become a more complex sort if the spec
                    // changes based on https://github.com/gnuradio/SigMF/issues/90
                    return pmt::to_uint64(a.get("core:sample_start")) <
                      pmt::to_uint64(b.get("core:sample_start"));
                  });

        writer.String("annotations");
        writer.StartArray();
        for(std::vector<meta_namespace>::iterator it = annotations.begin();
            it != annotations.end(); it++) {
          (*it).serialize(writer);
        }
        writer.EndArray();

        writer.EndObject();
      }
    } // namespace writer_utils
  } // namespace sigmf
} // namespace gr