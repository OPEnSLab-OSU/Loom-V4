#pragma once
#include <stddef.h>

// Check the document itself, not a JsonObject view: a view loses the pool's overflow flag.
// Keep this query stateless so additions made after Manager::package() are checked as well.
template <typename Document> bool loomJsonIsComplete(const Document &document) {
    return !document.isNull() && !document.overflowed();
}

// Pool capacity is not a wire-length limit: escaping and referenced strings can expand JSON.
template <typename Document> bool loomJsonFitsWire(const Document &document, size_t limit) {
    return loomJsonIsComplete(document) && measureJson(document) < limit;
}
