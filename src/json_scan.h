#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bc {

// Minimal, allocation-free JSON scanner for the known Binance message shapes.
// It validates structure while skipping values and never builds a DOM.
// Strings are returned raw (the bytes between the quotes, escapes untouched);
// none of the fields we read (stream names, decimal strings) use escapes.
//
// Object iteration (the caller owns the `first` flag, so nesting is safe):
//   if (!c.consume('{')) fail;
//   bool first = true, done = false; std::string_view key;
//   while (c.next_key(first, key, done) && !done) { read or skip the value }
//   if (!done) fail;
class JsonCursor {
public:
    explicit JsonCursor(std::string_view s) : p_(s.data()), end_(s.data() + s.size()) {}

    void skip_ws() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_;
    }
    bool at_end() {
        skip_ws();
        return p_ >= end_;
    }
    // Consumes `c` (after whitespace) if present.
    bool consume(char c) {
        skip_ws();
        if (p_ < end_ && *p_ == c) {
            ++p_;
            return true;
        }
        return false;
    }
    bool peek(char c) {
        skip_ws();
        return p_ < end_ && *p_ == c;
    }
    const char* pos() const { return p_; }

    bool read_string(std::string_view& out);  // raw contents
    bool read_uint(std::uint64_t& out);       // non-negative integer token
    bool skip_value();                        // any JSON value

    // After '{': reads the next key and its ':'; done=true when '}' consumed.
    bool next_key(bool& first, std::string_view& key, bool& done);
    // After '[': positions at the next element; done=true when ']' consumed.
    bool next_element(bool& first, bool& done);

private:
    bool skip_value_depth(int depth);

    const char* p_;
    const char* end_;
};

// Splits a combined-stream envelope {"stream":"...","data":{...}} into the
// stream name and the raw bytes of the data object. Returns false if the
// message is not a well-formed object or lacks either member (acks, errors).
bool split_envelope(std::string_view msg, std::string_view& stream, std::string_view& data);

// True if `json` contains whitespace outside string literals.
bool has_ws_outside_strings(std::string_view json);

// Copies `json` into `out` without whitespace outside string literals.
void minify_json(std::string_view json, std::string& out);

}  // namespace bc
