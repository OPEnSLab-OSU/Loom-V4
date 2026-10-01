#pragma once
// GPIO/position tests do not need the real Manager's ArduinoJson pool or hardware serial ID.
class Manager {
  public:
    template <typename Value> void addData(const char *, const char *, Value) {}
};
