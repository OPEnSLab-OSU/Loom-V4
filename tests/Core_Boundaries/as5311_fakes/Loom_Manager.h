#pragma once
#include <string.h>

class Manager {
  public:
    void addData(const char *, const char *, const char *value) { alignment = value; }
    void addData(const char *, const char *key, int value) {
        if (strcmp(key, "pos_raw") == 0) {
            raw = value;
        } else if (strcmp(key, "pos_avg") == 0) {
            average = value;
        } else if (strcmp(key, "mag") == 0) {
            magnitude = value;
        }
    }
    void addData(const char *, const char *, float value) { displacement = value; }

    const char *alignment = "";
    int raw = 0, average = 0, magnitude = 0;
    float displacement = 0;
};
