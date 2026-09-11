#pragma once
// Minimal Arduino compatibility layer for the native simulator build.
// Covers only what the shared sources actually use: millis/delay, Serial,
// and the setup()/loop() entry points (driven by sim_main.cpp).
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// strlcpy is BSD/macOS/ESP-IDF; glibc only exposes it from 2.38.
#if defined(__GLIBC__) && (!defined(__GLIBC_PREREQ) || !__GLIBC_PREREQ(2, 38))
static inline size_t strlcpy(char* dst, const char* src, size_t size) {
    size_t len = strlen(src);
    if (size) {
        size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}
#endif

unsigned long millis(void);
void delay(unsigned long ms);

class SimSerial {
public:
    void begin(unsigned long baud) { (void)baud; }
    int  available(void) { return 0; }
    int  read(void) { return -1; }
    size_t write(const uint8_t* buf, size_t len);
    void flush(void) { fflush(stdout); }
    void print(const char* s) { fputs(s, stdout); }
    void println(const char* s) { puts(s); }
    void println(void) { putchar('\n'); }
    void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
};
extern SimSerial Serial;

void setup(void);
void loop(void);
