extern "C" {
void fn_80_A8();
void fn_8022A578(void*);

// Keep definitions in descending retail-address order; MWCC emits in reverse.
void RELMain() { fn_80_A8(); }

void RELExit() { fn_8022A578(0); }
}
