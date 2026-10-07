#include "output_file.h"

#include <cerrno>
#include <cstring>
#include <utility>

namespace bc {

OutputFile::~OutputFile() { close(); }

OutputFile::OutputFile(OutputFile&& other) noexcept
    : file_(std::exchange(other.file_, nullptr)),
      path_(std::move(other.path_)),
      buf_(std::move(other.buf_)),
      rows_(other.rows_),
      write_errors_(other.write_errors_) {}

bool OutputFile::open(const std::string& path, std::string& err) {
    close();
    file_ = std::fopen(path.c_str(), "wb");
    if (file_ == nullptr) {
        err = "cannot open '" + path + "' for writing: " + std::strerror(errno);
        return false;
    }
    std::setvbuf(file_, nullptr, _IONBF, 0);
    path_ = path;
    buf_.clear();
    buf_.reserve(kFlushThreshold + 16 * 1024);
    rows_ = 0;
    return true;
}

bool OutputFile::flush() {
    if (file_ == nullptr || buf_.empty()) return true;
    const std::size_t n = std::fwrite(buf_.data(), 1, buf_.size(), file_);
    const bool ok = (n == buf_.size());
    if (!ok) {
        ++write_errors_;
        std::fprintf(stderr, "[error] write to %s failed: %s\n", path_.c_str(), std::strerror(errno));
    }
    buf_.clear();
    return ok;
}

bool OutputFile::close() {
    if (file_ == nullptr) return true;
    bool ok = flush();
    if (std::fclose(file_) != 0) {
        ++write_errors_;
        ok = false;
    }
    file_ = nullptr;
    return ok;
}

}  // namespace bc
