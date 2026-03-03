/* ============================================================
 * SwanOS — CMOS Real-Time Clock Driver
 * Reads date/time from the hardware RTC chip (ports 0x70/0x71).
 * ============================================================ */

#include "rtc.h"
#include "ports.h"
#include "string.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

/* CMOS register indices */
#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x02
#define RTC_HOURS    0x04
#define RTC_WEEKDAY  0x06
#define RTC_DAY      0x07
#define RTC_MONTH    0x08
#define RTC_YEAR     0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    io_wait();
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void rtc_read(rtc_time_t *t) {
    /* Wait for update-in-progress flag to clear */
    while (cmos_read(RTC_STATUS_A) & 0x80);

    uint8_t sec  = cmos_read(RTC_SECONDS);
    uint8_t min  = cmos_read(RTC_MINUTES);
    uint8_t hr   = cmos_read(RTC_HOURS);
    uint8_t wday = cmos_read(RTC_WEEKDAY);
    uint8_t day  = cmos_read(RTC_DAY);
    uint8_t mon  = cmos_read(RTC_MONTH);
    uint8_t yr   = cmos_read(RTC_YEAR);

    uint8_t status_b = cmos_read(RTC_STATUS_B);

    /* Convert BCD to binary if not already binary mode */
    if (!(status_b & 0x04)) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hr   = bcd_to_bin(hr & 0x7F) | (hr & 0x80); /* preserve AM/PM bit */
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        yr   = bcd_to_bin(yr);
        wday = bcd_to_bin(wday);
    }

    /* Convert 12-hour to 24-hour if needed */
    if (!(status_b & 0x02) && (hr & 0x80)) {
        hr = ((hr & 0x7F) + 12) % 24;
    }

    t->second  = sec;
    t->minute  = min;
    t->hour    = hr;
    t->day     = day;
    t->month   = mon;
    t->year    = 2000 + yr;  /* assume 2000s */
    t->weekday = wday;
}

static void two_digit(uint8_t val, char *buf) {
    buf[0] = '0' + (val / 10);
    buf[1] = '0' + (val % 10);
}

void rtc_format_time(const rtc_time_t *t, char *buf) {
    two_digit(t->hour, buf);
    buf[2] = ':';
    two_digit(t->minute, buf + 3);
    buf[5] = ':';
    two_digit(t->second, buf + 6);
    buf[8] = '\0';
}

void rtc_format_date(const rtc_time_t *t, char *buf) {
    /* YYYY-MM-DD */
    buf[0] = '0' + (t->year / 1000) % 10;
    buf[1] = '0' + (t->year / 100) % 10;
    buf[2] = '0' + (t->year / 10) % 10;
    buf[3] = '0' + t->year % 10;
    buf[4] = '-';
    two_digit(t->month, buf + 5);
    buf[7] = '-';
    two_digit(t->day, buf + 8);
    buf[10] = '\0';
}

void rtc_format_weekday(const rtc_time_t *t, char *buf) {
    static const char *days[] = {
        "???", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    int idx = t->weekday;
    if (idx < 1 || idx > 7) idx = 0;
    strcpy(buf, days[idx]);
}
