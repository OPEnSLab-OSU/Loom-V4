// Deferred regression cases: no compiler was run during the source pass.
#include "../../src/Utilities/Loom_LoRaReceiveState.h"
#include <assert.h>

int main() {
    loomLoRa::BatchReceiveState batch;
    assert(batch.remaining() == 0);
    batch.start(7, 3, 100);
    batch.start(7, 3, 200);
    assert(batch.remaining() == 3); // Repeated header must not double the outstanding count.
    batch.complete(8, 300);
    assert(batch.remaining() == 3); // Another node cannot acknowledge node 7's data.
    batch.complete(7, 400);
    assert(batch.remaining() == 2);
    assert(!batch.expire(10399, 10000));
    assert(batch.expire(10400, 10000));
    assert(batch.remaining() == 0);
    batch.start(9, 2, UINT32_MAX - 99);
    assert(!batch.expire(9899, 10000));
    assert(batch.expire(9900, 10000)); // millis() rollover must not retain a dead batch.
    batch.start(1, 1, 0);
    batch.complete(1, 1);
    batch.complete(1, 2);
    assert(batch.remaining() == 0); // No unsigned underflow after the last packet.
    batch.start(2, 4, 3);
    batch.cancel();
    assert(batch.remaining() == 0); // Blocking failure releases the example's do/while loop.
    return 0;
}
