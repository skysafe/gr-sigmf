#pragma once

namespace gr::sigmf {
    namespace detail {
      template<typename T>
      class resizable_buffer {
        private:
        T *data_ptr;
        size_t size;

        public:
        resizable_buffer(size_t initial_size)
        : data_ptr(static_cast<T *>(std::malloc(initial_size * sizeof(T)))), size(initial_size)
        {
          if (data_ptr == nullptr) {
            throw std::runtime_error("Failed to allocate initial memory amoung");
          }
        }

        ~resizable_buffer()
        {
          std::free(data_ptr);
        }

        T *
        data()
        {
          return data_ptr;
        }

        bool
        ensure_size(size_t new_size)
        {
          if(new_size <= size) {
            return true;
          } else {
            T *new_data_ptr = static_cast<T *>(std::realloc(data_ptr, new_size * sizeof(T)));
            if(new_data_ptr == nullptr) {
              return false;
            }
            data_ptr = new_data_ptr;
            size = new_size;
            return true;
          }
        }
      };
    }
    using resizable_byte_buffer_t = detail::resizable_buffer<char>;
}