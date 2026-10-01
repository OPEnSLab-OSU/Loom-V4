These two native_recorder files were emitted by the production Loom_Trace serializers during
the native test_trace.cpp SD harness run on 2026-09-30. The fake clock and allocator are not
real board measurements. Paired fixtures test direct Chrome JSON decoding against the original
NDJSON event history, including escaping, copied object names and allocation/function records.
