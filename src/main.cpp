#include <iostream>
#include <vector>

#include "cpu.h"

int main()
{
    RAM ram;

    ram.write(0x0, 0x7c00, { 0xeb, 0x3c, 0x90 });
    ram.write(0x0, 0x7c3e, { 0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x16, 0x07, 0xbb, 0x78, 0x00, 0x36, 0xc5, 0x37 });
    //ram.write(0x0, 0x0, { 0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x16, 0x07, 0xbb, 0x78, 0x00 });

    X86Cpu cpu(ram);

    while (true) {
        cpu.step();
        cpu.printRegs();
    }

    /*X86RegisterFile rf;
    rf.ax = 0x7c00;    

    auto dst = std::make_unique<RegisterOperand>(2, 0);
    auto src = std::make_unique<RegisterOperand>(2, 0);

    XorInstruction inst(std::move(dst), std::move(src)); */

    //inst.execute(rf, ram);

    //printf("%x\n", rf.ax);

/*    while (true) {
        cpu.printRegs();
        cpu.step(ram);
    } */

    //std::vector<uint8_t> data = { 0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x16 };
    //auto inst = decode(data);

    return 0;
}
