#include "csv_reader.h"

#include <cerrno>
#include <cstring>

namespace bc {

bool parse_csv_record(std::string_view record, std::string& storage,
                      std::vector<std::string_view>& fields, std::string& err) {
    fields.clear();
    storage.clear();
    // Unescaped output is never longer than the input, so after this reserve
    // no append can reallocate and the views taken below stay valid.
    storage.reserve(record.size());

    const std::size_t n = record.size();
    std::size_t i = 0;
    while (true) {
        const std::size_t field_start = storage.size();
        if (i < n && record[i] == '"') {
            ++i;
            bool closed = false;
            while (i < n) {
                const char c = record[i];
                if (c == '"') {
                    if (i + 1 < n && record[i + 1] == '"') {
                        storage.push_back('"');
                        i += 2;
                        continue;
                    }
                    ++i;
                    closed = true;
                    break;
                }
                storage.push_back(c);
                ++i;
            }
            if (!closed) {
                err = "unterminated quoted field";
                return false;
            }
            if (i < n && record[i] != ',') {
                err = "unexpected character after closing quote";
                return false;
            }
        } else {
            while (i < n && record[i] != ',') {
                if (record[i] == '"') {
                    err = "quote inside unquoted field";
                    return false;
                }
                storage.push_back(record[i]);
                ++i;
            }
        }
        fields.emplace_back(storage.data() + field_start, storage.size() - field_start);
        if (i >= n) break;
        ++i;  // skip ','
    }
    return true;
}

CsvReader::~CsvReader() {
    if (file_ != nullptr) std::fclose(file_);
}

bool CsvReader::open(const std::string& path, std::string& err) {
    file_ = std::fopen(path.c_str(), "rb");
    if (file_ == nullptr) {
        err = "cannot open '" + path + "': " + std::strerror(errno);
        return false;
    }
    chunk_.resize(1 << 16);
    raw_.reserve(1 << 12);
    return true;
}

bool CsvReader::fill() {
    if (eof_) return false;
    len_ = std::fread(chunk_.data(), 1, chunk_.size(), file_);
    pos_ = 0;
    if (len_ == 0) {
        eof_ = true;
        return false;
    }
    return true;
}

CsvReader::Status CsvReader::next(std::vector<std::string_view>& fields, std::string& err) {
    if (file_ == nullptr) {
        err = "reader not open";
        return Status::Error;
    }
    if (std::ferror(file_)) {
        err = "read error";
        return Status::Error;
    }
    raw_.clear();
    bool in_quotes = false;
    bool got_any = false;
    bool terminated = false;
    while (!terminated) {
        if (pos_ >= len_ && !fill()) break;
        got_any = true;
        std::size_t i = pos_;
        for (; i < len_; ++i) {
            const char c = chunk_[i];
            if (c == '"') {
                in_quotes = !in_quotes;  // "" toggles twice: net no change
            } else if (c == '\n' && !in_quotes) {
                terminated = true;
                break;
            }
        }
        raw_.append(chunk_.data() + pos_, i - pos_);
        pos_ = terminated ? i + 1 : i;
    }
    if (std::ferror(file_)) {
        err = "read error";
        return Status::Error;
    }
    if (!got_any || (!terminated && raw_.empty())) return Status::Eof;
    ++record_no_;
    if (in_quotes) {
        err = "unterminated quoted field at end of file";
        return Status::Error;
    }
    if (!raw_.empty() && raw_.back() == '\r') raw_.pop_back();
    if (!parse_csv_record(raw_, storage_, fields, err)) return Status::Error;
    return Status::Record;
}

}  // namespace bc
