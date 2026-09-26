/* Time source: DS3231 RTC if present, else internal time from build stamp.
 *
 * envclock_init() scans the candidate I2C pairs for a DS3231 at 0x68. If found
 * with a valid year, chip time rules (battery keeps it across power loss).
 * If found but invalid, the build timestamp is written to it once. With no
 * chip, time starts at the firmware build moment and free-runs on the
 * internal clock — fine for a demo, drifts minutes per day, resets to the
 * build stamp on every power-up. envclock_source() reports which is active.
 */
#pragma once

#include <stdbool.h>
#include <time.h>

void envclock_init(void);
bool envclock_now(struct tm *out);
const char *envclock_source(void);
