#pragma once

// Project-wide build flags may override these defaults. The LOOM_ prefix avoids collisions
// with sketch macros such as VREF and VBATPIN. Include Arduino before using the A7 pin value.
// Hypnos needs these settings without importing Analog's vector/module implementation.
#ifndef LOOM_ANALOG_BATTERY_PIN
#define LOOM_ANALOG_BATTERY_PIN A7
#endif
#ifndef LOOM_ANALOG_ADC_RESOLUTION_BITS
#define LOOM_ANALOG_ADC_RESOLUTION_BITS 12
#endif
#ifndef LOOM_ANALOG_ADC_REFERENCE_VOLTAGE
#define LOOM_ANALOG_ADC_REFERENCE_VOLTAGE 3.3f
#endif
#ifndef LOOM_ANALOG_BATTERY_DIVIDER_SCALE
#define LOOM_ANALOG_BATTERY_DIVIDER_SCALE 2.0f
#endif
#ifndef LOOM_ANALOG_BATTERY_SAMPLE_COUNT
#define LOOM_ANALOG_BATTERY_SAMPLE_COUNT 8
#endif
#ifndef LOOM_ANALOG_ADC_MAX_READING
#define LOOM_ANALOG_ADC_MAX_READING ((1UL << LOOM_ANALOG_ADC_RESOLUTION_BITS) - 1UL)
#endif
