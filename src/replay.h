#pragma once

#include <string>

namespace bc {

// Runs the replay mode. Reads market-data from `in_path` and writes order-book rows to `out_path`.
// Returns true on success.
bool run_replay(const std::string& in_path, const std::string& out_path);

} // namespace bc
