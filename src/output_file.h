#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace bc {

// Append-only output file with an explicit user-space buffer.
// Callers append one complete row to buffer() and then call row_done();
// the buffer is written out only at row boundaries: when it exceeds the
// threshold, on flush() (periodic), and on close(). The FILE* itself is
// unbuffered so there is exactly one buffering layer.
class OutputFile {
public:
    static constexpr std::size_t kFlushThreshold = 64 * 1024;

    OutputFile() = default;
    ~OutputFile();
    OutputFile(const OutputFile&) = delete;
    OutputFile& operator=(const OutputFile&) = delete;
    OutputFile(OutputFile&& other) noexcept;
    OutputFile& operator=(OutputFile&&) = delete;

    // Creates or truncates `path`.
    bool open(const std::string& path, std::string& err);
    bool is_open() const { return file_ != nullptr; }

    std::string& buffer() { return buf_; }
    void row_done() {
        ++rows_;
        if (buf_.size() >= kFlushThreshold) flush();
    }

    bool flush();
    bool close();

    std::uint64_t rows() const { return rows_; }
    std::uint64_t write_errors() const { return write_errors_; }
    const std::string& path() const { return path_; }

private:
    std::FILE* file_ = nullptr;
    std::string path_;
    std::string buf_;
    std::uint64_t rows_ = 0;
    std::uint64_t write_errors_ = 0;
};

}  // namespace bc
