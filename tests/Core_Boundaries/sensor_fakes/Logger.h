#pragma once
// Only logging is removed; tests exercise the production transfer/packaging code.
struct FakeFunctionStart {
    void operator()(const void * = nullptr, const char * = nullptr) const {}
};
inline FakeFunctionStart fakeFunctionStart() { return {}; }
#define FUNCTION_START fakeFunctionStart()
#define FUNCTION_START_OBJECT(object) FUNCTION_START(object)
#define FUNCTION_END
#define LOG(...)
#define LOGF(...)
#define ERROR(...)
#define ERRORF(...)
#define WARNING(...)
#define WARNINGF(...)

#define TRACE_FUNCTION_START_OBJECT(object)
#define TRACE_FUNCTION_START(object)
#define TRACE_FUNCTION_SCOPE(name, object)
