#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;  /* 1=Sun, 2=Mon, ... 7=Sat */
} rtc_time_t;

/* Read current date/time from CMOS RTC */
void rtc_read(rtc_time_t *t);

/* Format time into "HH:MM:SS" buffer (needs 9 chars) */
void rtc_format_time(const rtc_time_t *t, char *buf);

/* Format date into "YYYY-MM-DD" buffer (needs 11 chars) */
void rtc_format_date(const rtc_time_t *t, char *buf);

/* Format day-of-week into 3-letter abbrev (needs 4 chars) */
void rtc_format_weekday(const rtc_time_t *t, char *buf);

#endif
