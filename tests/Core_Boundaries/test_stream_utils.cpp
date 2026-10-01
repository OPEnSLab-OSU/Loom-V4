#include <cassert>
#include <string>
#include "Utilities/Loom_StreamUtils.h"

struct Input {
    std::string text = "abc";
    size_t offset = 0;
    bool continuous = false, badPeek = false;
    int reads = 0;
    int available() { return continuous ? 1 : static_cast<int>(text.size() - offset); }
    int peek() { return badPeek ? -1 : continuous ? 'x' : text[offset]; }
    int read() {
        ++reads;
        return continuous ? 'x' : text[offset++];
    }
};
struct Output {
    std::string text;
    bool busy = false;
    size_t write(uint8_t value) {
        if (busy) {
            return 0;
        }
        text.push_back(static_cast<char>(value));
        return 1;
    }
};
int main() {
    Input source;
    Output destination;
    assert(loomStream::forwardAvailable(source, destination, 2) == 2);
    assert(destination.text == "ab" && source.offset == 2);
    destination.busy = true;
    assert(loomStream::forwardAvailable(source, destination, 2) == 0);
    assert(source.offset == 2 && source.reads == 2);
    destination.busy = false;
    assert(loomStream::forwardAvailable(source, destination, 2) == 1 && destination.text == "abc");
    Input endless;
    endless.continuous = true;
    assert(loomStream::forwardAvailable(endless, destination, 64) == 64 && endless.reads == 64);
    assert(loomStream::forwardAvailable(endless, destination, 0) == 0 && endless.reads == 64);
    endless.badPeek = true;
    assert(loomStream::forwardAvailable(endless, destination, 64) == 0 && endless.reads == 64);
}
