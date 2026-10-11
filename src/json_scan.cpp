#include "json_scan.h"

namespace bc {

namespace {
constexpr int kMaxDepth = 32;

inline bool is_ws(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
} // namespace

bool JsonCursor::read_string(std::string_view &out) {
  skip_ws();
  if (p_ >= end_ || *p_ != '"')
    return false;
  ++p_;
  const char *start = p_;
  while (p_ < end_) {
    const char c = *p_;
    if (c == '"') {
      out = std::string_view(start, static_cast<std::size_t>(p_ - start));
      ++p_;
      return true;
    }
    if (c == '\\') {
      if (p_ + 1 >= end_)
        return false;
      p_ += 2;
      continue;
    }
    if (static_cast<unsigned char>(c) < 0x20)
      return false; // raw control char
    ++p_;
  }
  return false;
}

bool JsonCursor::read_uint(std::uint64_t &out) {
  skip_ws();
  const char *start = p_;
  std::uint64_t v = 0;
  while (p_ < end_ && *p_ >= '0' && *p_ <= '9') {
    std::uint64_t t = 0;
    if (__builtin_mul_overflow(v, std::uint64_t{10}, &t))
      return false;
    if (__builtin_add_overflow(t, static_cast<std::uint64_t>(*p_ - '0'), &t))
      return false;
    v = t;
    ++p_;
  }
  if (p_ == start)
    return false;
  if (p_ < end_ &&
      (*p_ == '.' || *p_ == 'e' || *p_ == 'E' || *p_ == '-' || *p_ == '+')) {
    return false; // not an integer
  }
  out = v;
  return true;
}

bool JsonCursor::next_key(bool &first, std::string_view &key, bool &done) {
  skip_ws();
  if (p_ >= end_)
    return false;
  if (*p_ == '}') {
    ++p_;
    done = true;
    return true;
  }
  if (!first) {
    if (*p_ != ',')
      return false;
    ++p_;
  }
  first = false;
  done = false;
  if (!read_string(key))
    return false;
  return consume(':');
}

bool JsonCursor::next_element(bool &first, bool &done) {
  skip_ws();
  if (p_ >= end_)
    return false;
  if (*p_ == ']') {
    ++p_;
    done = true;
    return true;
  }
  if (!first) {
    if (*p_ != ',')
      return false;
    ++p_;
  }
  first = false;
  done = false;
  return true;
}

bool JsonCursor::skip_value() { return skip_value_depth(0); }

bool JsonCursor::skip_value_depth(int depth) {
  if (depth > kMaxDepth)
    return false;
  skip_ws();
  if (p_ >= end_)
    return false;
  const char c = *p_;
  if (c == '"') {
    std::string_view ignored;
    return read_string(ignored);
  }
  if (c == '{') {
    ++p_;
    bool first = true, done = false;
    std::string_view key;
    while (next_key(first, key, done)) {
      if (done)
        return true;
      if (!skip_value_depth(depth + 1))
        return false;
    }
    return false;
  }
  if (c == '[') {
    ++p_;
    bool first = true, done = false;
    while (next_element(first, done)) {
      if (done)
        return true;
      if (!skip_value_depth(depth + 1))
        return false;
    }
    return false;
  }
  auto literal = [this](std::string_view lit) {
    if (static_cast<std::size_t>(end_ - p_) < lit.size())
      return false;
    if (std::string_view(p_, lit.size()) != lit)
      return false;
    p_ += lit.size();
    return true;
  };
  if (c == 't')
    return literal("true");
  if (c == 'f')
    return literal("false");
  if (c == 'n')
    return literal("null");
  if (c == '-' || (c >= '0' && c <= '9')) {
    const char *start = p_;
    while (p_ < end_) {
      const char d = *p_;
      if ((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.' ||
          d == 'e' || d == 'E') {
        ++p_;
      } else {
        break;
      }
    }
    return p_ > start;
  }
  return false;
}

bool split_envelope(std::string_view msg, std::string_view &stream,
                    std::string_view &data) {
  JsonCursor c(msg);
  if (!c.consume('{'))
    return false;
  bool have_stream = false, have_data = false;
  bool first = true, done = false;
  std::string_view key;
  while (c.next_key(first, key, done)) {
    if (done)
      break;
    if (key == "stream") {
      if (!c.read_string(stream))
        return false;
      have_stream = true;
    } else if (key == "data") {
      c.skip_ws();
      if (!c.peek('{'))
        return false;
      const char *start = c.pos();
      if (!c.skip_value())
        return false;
      data = std::string_view(start, static_cast<std::size_t>(c.pos() - start));
      have_data = true;
    } else {
      if (!c.skip_value())
        return false;
    }
  }
  if (!done || !c.at_end())
    return false;
  return have_stream && have_data;
}

bool has_ws_outside_strings(std::string_view json) {
  bool in_str = false;
  for (std::size_t i = 0; i < json.size(); ++i) {
    const char c = json[i];
    if (in_str) {
      if (c == '\\') {
        ++i;
      } else if (c == '"') {
        in_str = false;
      }
    } else if (c == '"') {
      in_str = true;
    } else if (is_ws(c)) {
      return true;
    }
  }
  return false;
}

void minify_json(std::string_view json, std::string &out) {
  out.clear();
  bool in_str = false;
  for (std::size_t i = 0; i < json.size(); ++i) {
    const char c = json[i];
    if (in_str) {
      out.push_back(c);
      if (c == '\\' && i + 1 < json.size()) {
        out.push_back(json[++i]);
      } else if (c == '"') {
        in_str = false;
      }
    } else if (c == '"') {
      in_str = true;
      out.push_back(c);
    } else if (!is_ws(c)) {
      out.push_back(c);
    }
  }
}

} // namespace bc
