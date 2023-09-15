#include <iostream>

void movdir64b(void* dest, const void* src) {
    __asm {
        mov rdi, dest
        mov rsi, src
        movdir64b [rdi], rsi
    }
}

int main() {
    alignas(64) char src[64] = "Hello, MOVDIR64B!";
    alignas(64) char dest[64] = {};

    movdir64b(dest, src);

    std::cout << "Destination buffer: " << dest << std::endl;

    return 0;
}