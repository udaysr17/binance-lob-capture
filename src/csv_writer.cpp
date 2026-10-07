#include "csv_writer.h"

#include <charconv>

namespace bc {

void append_int(std::string& out, std::int64_t v) {
    char buf[24];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, res.ptr);
}

void append_uint(std::string& out, std::uint64_t v) {
    char buf[24];
    const auto res = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, res.ptr);
}

void append_csv_quoted(std::string& out, std::string_view field) {
    out.push_back('"');
    std::size_t start = 0;
    for (std::size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '"') {
            out.append(field.data() + start, i - start + 1);  // includes the quote
            out.push_back('"');
            start = i + 1;
        }
    }
    out.append(field.data() + start, field.size() - start);
    out.push_back('"');
}

}  // namespace bc
