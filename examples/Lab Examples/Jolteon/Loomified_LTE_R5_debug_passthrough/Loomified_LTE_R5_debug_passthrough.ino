/** Jolteon Rewire / Feather M0 protocol bench console.
 * Use the Jolteon Rewire carrier and select Loomified Feather M0.
 * Serial Monitor: 115200, newline or CRLF. Run baseline nosd without a card.
 * Baseline enables Hypnos first; separate manual enable/disable commands remain.
 */
#include "Bringup.h"
void setup() { bringupBegin(); }
void loop() { bringupLoop(); }
