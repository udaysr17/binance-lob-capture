#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace bc {

// Parses one RFC 4180 record (no trailing line terminator). Quoted fields may
// contain commas, LF and doubled quotes (""), which are unescaped into
// `storage`. On success `fields` holds views into `storage` (valid until the
// next call that reuses `storage`). Returns false and sets `err` on malformed
// input (bare quote inside an unquoted field, text after a closing quote,
// unterminated quote).
bool parse_csv_record(std::string_view record, std::string& storage,
                      std::vector<std::string_view>& fields, std::string& err);

// Streaming RFC 4180 reader over a file. Records end at an LF outside quotes;
// a CR immediately before that LF is tolerated and stripped.
class CsvReader {
public:
    enum class Status { Record, Eof, Error };

    CsvReader() = default;
    ~CsvReader();
    CsvReader(const CsvReader&) = delete;
    CsvReader& operator=(const CsvReader&) = delete;

    bool open(const std::string& path, std::string& err);

    // Fields are valid until the next call to next().
    Status next(std::vector<std::string_view>& fields, std::string& err);

    // 1-based number of the record most recently returned.
    std::uint64_t record_number() const { return record_no_; }

private:
    bool fill();

    std::FILE* file_ = nullptr;
    std::vector<char> chunk_;
    std::size_t pos_ = 0;
    std::size_t len_ = 0;
    bool eof_ = false;
    std::string raw_;
    std::string storage_;
    std::vector<std::size_t> bounds_;
    std::uint64_t record_no_ = 0;
};

}  // namespace bc
