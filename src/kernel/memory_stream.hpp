#pragma once

// Private helper of bendcore_kernel: a read-only, seekable std::streambuf over a memory
// buffer (no copy), and a stream setup that makes OCCT readers stop on bad input.
//
// OCCT readers do not check the stream state everywhere: BRepTools::Read on truncated text
// loops forever in BRepTools_ShapeSet::ReadGeometry once extraction starts failing. With
// failbit and badbit in the exception mask, the first failed extraction throws
// std::ios_base::failure out of the reader instead.

#include <cstddef>
#include <ios>
#include <istream>
#include <streambuf>

namespace bendcore::kernel::detail {

class MemoryStreamBuf : public std::streambuf {
public:
    MemoryStreamBuf(const char* data, std::size_t size) {
        char* begin = const_cast<char*>(data);
        setg(begin, begin, begin + size);
    }

protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        off_type base = 0;
        if (dir == std::ios_base::cur) {
            base = gptr() - eback();
        } else if (dir == std::ios_base::end) {
            base = egptr() - eback();
        }
        return seekpos(pos_type(base + off), which);
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
        const off_type offset = off_type(pos);
        if (!(which & std::ios_base::in) || offset < 0 || offset > egptr() - eback()) {
            return pos_type(off_type(-1));
        }
        setg(eback(), eback() + offset, egptr());
        return pos;
    }
};

inline void throwOnFailedExtraction(std::istream& in) { in.exceptions(std::ios::failbit | std::ios::badbit); }

}  // namespace bendcore::kernel::detail
