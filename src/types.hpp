#ifndef FILE_SEEN_TYPES
#define FILE_SEEN_TYPES

#include <cstdint>
#include <vector>

namespace HerixLib {

using Byte = uint8_t;
using Buffer = std::vector<Byte>;

using FilePosition = size_t;
using AbsoluteFilePosition = size_t;

}

#endif
