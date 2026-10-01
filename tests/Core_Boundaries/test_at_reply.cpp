// Pure production line reader; compilation/execution remains deferred.
#include <cassert>
#include "Utilities/Loom_ATReply.h"

loomAt::Reply readLine(const char *text) {
    loomAt::ReplyReader reader;
    loomAt::Reply result = loomAt::Reply::Waiting;
    while (*text) {
        result = reader.consume(*text++);
    }
    return result;
}
int main() {
    using loomAt::Reply;
    assert(readLine("OK\r\n") == Reply::Accepted);
    assert(readLine("ERROR\r\n") == Reply::Rejected);
    assert(readLine("+CME ERROR: 10\r\n") == Reply::Rejected);
    assert(readLine("+CMS ERROR: a long detailed modem explanation\r\n") == Reply::Rejected);
    assert(readLine("NOT OK\r\n") == Reply::Waiting);
    assert(readLine("AT+BOOK=1\r\n") == Reply::Waiting);
    assert(readLine("+INFO: ERROR history\r\n") == Reply::Waiting);
    assert(readLine("OK followed by extra non-acknowledgement text\r\n") == Reply::Waiting);
    assert(readLine("OK") == Reply::Waiting); // A partial line does not acknowledge the command.
    loomAt::ReplyReader reader;
    reader.consume('O');
    reader.consume('K');
    reader.consume('\0');
    assert(reader.consume('\n') == Reply::Waiting);
    for (unsigned int i = 0; i < 10000; ++i) {
        assert(reader.consume('x') == Reply::Waiting);
    }
    assert(reader.consume('\n') == Reply::Waiting);
    reader.consume('O');
    reader.consume('K');
    assert(reader.consume('\n') == Reply::Accepted); // A bad/oversized line cannot poison the next.
}
