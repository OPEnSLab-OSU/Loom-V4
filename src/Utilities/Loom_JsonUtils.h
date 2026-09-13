#pragma once

// Check the document itself, not a JsonObject view: a view loses the pool's overflow flag.
// Keep this query stateless so additions made after Manager::package() are checked as well.
template <typename Document> bool loomJsonIsComplete(const Document &document) {
    return !document.isNull() && !document.overflowed();
}
