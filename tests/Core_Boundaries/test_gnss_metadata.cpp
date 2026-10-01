#include <cassert>
#include <limits>
#include "Utilities/Loom_GnssMetadata.h"
int main() {
    loomGnss::Fix fix;
    fix.year = 2024;
    fix.month = 2;
    fix.day = 29;
    fix.hour = 0;
    fix.minute = 0;
    fix.second = 0;
    fix.latitude = 44.5;
    fix.longitude = -123.25;
    const uint32_t utc = 1709164800;
    assert(loomTime::unixSeconds(2024, 2, 29, 0, 0, 0) == utc);
    assert(loomTime::unixSeconds(2000, 1, 1, 0, 0, 0) == 946684800);
    assert(loomTime::unixSeconds(2024, 13, 1, 0, 0, 0) == 0);
    assert(loomTime::unixSeconds(2100, 1, 1, 0, 0, 0) == 0);
    assert(loomTime::unixSeconds(2023, 2, 29, 0, 0, 0) == 0);
    StaticJsonDocument<512> document;
    JsonObject metadata = document.to<JsonObject>();
    assert(loomGnss::addMetadata(metadata, fix, utc + 300));
    assert(metadata["location"]["time_utc"] == "2024-02-29T00:00:00Z");
    assert(metadata["location"]["Latitude"].as<double>() == 44.5);
    assert(!loomGnss::addMetadata(metadata, fix, utc + 301));
    assert(!loomGnss::addMetadata(metadata, fix, utc - 1));
    fix.latitude = std::numeric_limits<double>::quiet_NaN();
    assert(!loomGnss::addMetadata(metadata, fix, utc));
    assert(metadata["location"]["Latitude"].as<double>() == 44.5); // Reject before mutation.
    fix.latitude = 90.1;
    assert(!loomGnss::addMetadata(metadata, fix, utc));
    fix.latitude = 44.5;
    fix.day = 30;
    assert(!loomGnss::addMetadata(metadata, fix, utc));
    fix.day = 29;
    StaticJsonDocument<16> tiny;
    assert(!loomGnss::addMetadata(tiny.to<JsonObject>(), fix, utc));

    StaticJsonDocument<512> sample;
    JsonObject columns = sample.to<JsonObject>();
    assert(!loomGnss::addCsvLocation(columns, nullptr, utc));
    assert(columns.size() == 4 && columns.containsKey("Latitude"));
    assert(columns["Latitude"].isNull()); // First sample with no fix still defines the schema.
    assert(loomGnss::addCsvLocation(columns, &fix, utc + 300));
    assert(columns.size() == 4 && columns["LocationMethod"] == "GPS");
    assert(columns["Longitude"].as<double>() == -123.25);
    assert(columns["time_utc"] == "2024-02-29T00:00:00Z");
    assert(!loomGnss::addCsvLocation(columns, &fix, utc + 301));
    assert(columns.size() == 4 && columns["Latitude"].isNull()); // Expiry clears old values.
    assert(!loomGnss::addCsvLocation(columns, &fix, utc - 1));
    assert(columns["time_utc"].isNull());
}
