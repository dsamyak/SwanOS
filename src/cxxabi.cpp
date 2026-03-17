/* ============================================================
 * SwanOS — Minimal C++ ABI Stubs for Bare-Metal
 * Required by g++ even with -fno-exceptions -fno-rtti
 * ============================================================ */

extern "C" {

/* Called when a pure virtual function is invoked (should never happen) */
void __cxa_pure_virtual(void) {
    while (1) { __asm__ volatile("hlt"); }
}

/* Static destructor registration (no-op in bare-metal) */
int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}

/* DSO handle (required by some compilers) */
void *__dso_handle = 0;

} /* extern "C" */
