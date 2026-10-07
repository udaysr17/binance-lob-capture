#include <cstdio>
#include <cstring>

#include "test_framework.h"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (const auto& t : bt::registry()) {
        if (filter != nullptr && std::strstr(t.name, filter) == nullptr) continue;
        const int before = bt::failures();
        t.fn();
        ++run;
        std::printf("%s %s\n", bt::failures() == before ? "[ OK ]" : "[FAIL]", t.name);
    }
    std::printf("%d tests, %d failed checks\n", run, bt::failures());
    return bt::failures() == 0 ? 0 : 1;
}
