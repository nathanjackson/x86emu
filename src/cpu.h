#ifndef X86EMU_CPU_H
#define X86EMU_CPU_H

#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "ram.h"

template<typename T>
T countSetBits(T val)
{
    T count = 0;
    while (val) {
        count += val & 1;
        val >>= 1;
    }
    return count;
}

using X86Reg = uint16_t;
using X86Seg = uint16_t;

struct X86RegisterFile {
    // General Purpose Registers
    union {
        // by name
        struct {
            X86Reg ax;
            X86Reg cx;
            X86Reg dx;
            X86Reg bx;
            X86Reg sp;
            X86Reg bp;
            X86Reg si;
            X86Reg di;
        };
        // by index
        std::array<X86Reg, 8> gp;
    };

    X86Reg ip;
    X86Reg flags;

    // Segments
    union {
        struct {
            X86Seg es;
            X86Seg cs;
            X86Seg ss;
            X86Seg ds;
        };
        // by index
        std::array<X86Seg, 4> seg;
    };
};

const uint16_t SIGN_BIT_MASK = 0x8000;

const uint16_t CF_MASK = 0x0001;
const uint16_t PF_MASK = 0x0004;
const uint16_t ZF_MASK = 0x0040;
const uint16_t SF_MASK = 0x0080;
const uint16_t IF_MASK = 0x0200;
const uint16_t OF_MASK = 0x0800;

uint8_t mod(uint8_t val)
{
    return val >> 6;
}

uint8_t reg(uint8_t val)
{
    return (val >> 0x3) & 0x7;
}

uint8_t rm(uint8_t val)
{
    return val & 0x7;
}

class Operand
{
public:
    virtual ~Operand() = default;

    virtual size_t size() const = 0;

    virtual std::any getPtr(X86RegisterFile& registerFile, RAM& ram) = 0;
};

template<typename T>
class ImmediateOperand : public Operand
{
public:
    ImmediateOperand(T val) : value(val)
    {
    }

    ~ImmediateOperand() = default;

    size_t size() const override
    {
        return sizeof(value);
    }

    std::any getPtr(X86RegisterFile& registerFile, RAM& ram) override
    {
        return &value;
    }

private:
    T value;
};

class RegisterOperand : public Operand
{
public:
    RegisterOperand(size_t sz, uint8_t idx) : sz(sz), index(idx)
    {
    }

    ~RegisterOperand() = default;

    size_t size() const override
    {
        return sz;
    }

    std::any getPtr(X86RegisterFile& registerFile, RAM& ram) override
    {
        return &registerFile.gp.at(index);
    }

private:
    size_t sz;
    uint8_t index;
};

class SegmentOperand : public Operand
{
public:
    SegmentOperand(uint8_t idx) : index(idx)
    {
    }

    ~SegmentOperand() = default;

    size_t size() const override
    {
        return 2;
    }

    std::any getPtr(X86RegisterFile& registerFile, RAM& ram) override
    {
        return &registerFile.seg.at(index);
    }

private:
    uint8_t index;
};


class Instruction
{
public:
    virtual ~Instruction() = default;

    virtual void execute(X86RegisterFile& registerFile, RAM& ram) = 0;
};

class MovInstruction : public Instruction
{
public:
    MovInstruction(size_t len, std::unique_ptr<Operand> dst, std::unique_ptr<Operand> src)
        : length(len), destination(std::move(dst)), source(std::move(src))
    {
    }

    ~MovInstruction() = default;

    void execute(X86RegisterFile &registerFile, RAM& ram) override
    {
        std::cout << "MOV\n";

        auto dstPtr = std::any_cast<uint16_t*>(destination->getPtr(registerFile, ram));
        auto srcPtr = std::any_cast<uint16_t*>(source->getPtr(registerFile, ram));
        *dstPtr = *srcPtr;
        registerFile.ip += length;
    }

private:
    size_t length;
    std::unique_ptr<Operand> destination;
    std::unique_ptr<Operand> source;    
};

class CliInstruction : public Instruction
{
public:
    ~CliInstruction() = default;

    void execute(X86RegisterFile& registerFile, RAM& ram) override
    {
        std::cout << "CLI\n";
        registerFile.flags = registerFile.flags & (~IF_MASK);
        registerFile.ip += 1;
    }

    
};

class XorInstruction : public Instruction
{
public:
    XorInstruction(size_t len, std::unique_ptr<Operand> dst, std::unique_ptr<Operand> src) : length(len), destination(std::move(dst)), source(std::move(src))
    {
    }

    void execute(X86RegisterFile& registerFile, RAM& ram) override
    {
        std::cout << "XOR\n";

        auto dst = std::any_cast<uint16_t*>(destination->getPtr(registerFile, ram));
        auto src = std::any_cast<uint16_t*>(source->getPtr(registerFile, ram));

        *dst ^= *src;
    
        // Clear OF, CF
        X86Reg& flags = registerFile.flags;
        flags = flags & (~(OF_MASK | CF_MASK));
    
        // Set or clear SF
        if (0 > static_cast<int16_t>(*dst)) {
            flags |= SF_MASK;
        } else {
            flags = flags & (~SF_MASK);
        }
    
        // Set or clear ZF
        if (0 == *dst) {
            flags |= ZF_MASK;
        } else {
            flags = flags & (~ZF_MASK);
        }
    
        // Set or clear PF
        if (0 == (countSetBits(*dst) % 2)) {
            flags |= PF_MASK;
        } else {
            flags = flags & (~PF_MASK);
        }

        registerFile.ip += length;
    }

private:
    size_t length;
    std::unique_ptr<Operand> destination;
    std::unique_ptr<Operand> source;
};

class PushInstruction : public Instruction
{
public:
    ~PushInstruction() = default;

    PushInstruction(size_t len, std::unique_ptr<Operand> src) : length(len), source(std::move(src))
    {
    }

    void execute(X86RegisterFile &registerFile, RAM& ram)
    {
        std::cout << "PUSH\n";

        auto val = std::any_cast<uint16_t *>(source->getPtr(registerFile, ram));
        std::vector<uint8_t> buf;
        uint8_t *begin = reinterpret_cast<uint8_t *>(val);
        uint8_t *end = begin + sizeof(val);
        buf.insert(buf.begin(), begin, end);
        ram.write(registerFile.ss, registerFile.sp, buf);
        registerFile.sp -= sizeof(val);

        registerFile.ip += length;
    }
private:
    size_t length;
    std::unique_ptr<Operand> source;
};

class PopInstruction : public Instruction
{
public:
    ~PopInstruction() = default;

    PopInstruction(size_t len, std::unique_ptr<Operand> dst) : length(len), destination(std::move(dst))
    {
    }

    void execute(X86RegisterFile& registerFile, RAM& ram)
    {
        std::cout << "POP\n";

        auto buf = ram.read(registerFile.ss, registerFile.sp, 2);
        auto dstPtr = std::any_cast<uint16_t *>(destination->getPtr(registerFile, ram));
        std::copy(buf.begin(), buf.end(), dstPtr);

        registerFile.ip += length;
    }
private:
    size_t length;
    std::unique_ptr<Operand> destination;
};

class JmpInstruction : public Instruction
{
public:
    JmpInstruction(size_t len, int8_t disp) : length(len), displacement(disp)
    {
    }

    void execute(X86RegisterFile& registerFile, RAM& ram)
    {
        std::cout << "JMP\n";
        registerFile.ip += displacement + length;
    }

private:
    size_t length;
    int8_t displacement;
};

static void decode_modrm_regs(uint8_t modrm, std::unique_ptr<Operand>& dst,
                              std::unique_ptr<Operand>& src)
{
    switch (mod(modrm)) {
    case 0b11: {
        auto dstIdx = reg(modrm);
        auto srcIdx = rm(modrm);
        dst.reset(new RegisterOperand(2, dstIdx));
        src.reset(new RegisterOperand(2, srcIdx));
    } break;
    default: {
        throw std::runtime_error("addressing mode not implemented");
    } break;
    }

    if (nullptr == dst) {
        throw std::runtime_error("bad destination register");
    } else if (nullptr == src) {
        throw std::runtime_error("bad source register");
    }
}

static void decode_modrm_seg_reg(uint8_t modrm, std::unique_ptr<Operand>& dst,
                                 std::unique_ptr<Operand>& src)
{
    switch (mod(modrm)) {
    case 0b11: {
        auto dstIdx = reg(modrm);
        auto srcIdx = rm(modrm);
        dst.reset(new SegmentOperand(dstIdx));
        src.reset(new RegisterOperand(2, srcIdx)); 
    } break;
    default: {
        throw std::runtime_error("addressing mode not implemented");
    } break;
    }

    if (nullptr == dst) {
        throw std::runtime_error("bad destination segment");
    } else if (nullptr == src) {
        throw std::runtime_error("bad source register");
    }
}


std::unique_ptr<Instruction> decode(std::vector<uint8_t>& buffer)
{
    auto opcode = buffer.at(0);
    std::unique_ptr<Operand> dst = nullptr;
    std::unique_ptr<Operand> src = nullptr;
    std::unique_ptr<Instruction> result = nullptr;

    switch (opcode) {
    case 0x07: { // POP
        auto seg = std::make_unique<SegmentOperand>(0);
        result.reset(new PopInstruction(1, std::move(seg)));
    } break;

    case 0x16: { // PUSH
        auto seg = std::make_unique<SegmentOperand>(2);
        result.reset(new PushInstruction(1, std::move(seg)));
    } break;

    case 0x33: {
        uint8_t modrm = buffer.at(1);
        decode_modrm_regs(modrm, dst, src);
        result.reset(new XorInstruction(2, std::move(dst), std::move(src)));
    } break;

    case 0x50: { // PUSH AX
        auto srcIdx = opcode - 0x50;
        src.reset(new RegisterOperand(2, srcIdx));
        result.reset(new PushInstruction(1, std::move(src)));
    } break;


    case 0x8b: {
        uint8_t modrm = buffer.at(1);
        decode_modrm_regs(modrm, dst, src);
        result.reset(new MovInstruction(2, std::move(dst), std::move(src)));
    };

    case 0x8e: { // MOV Sreg, /r
        uint8_t modrm = buffer.at(1);
        decode_modrm_seg_reg(modrm, dst, src);
        result.reset(new MovInstruction(2, std::move(dst), std::move(src)));
    } break;

    case 0xbb: // MOV reg, imm
    case 0xbc: {
        auto dstIdx = opcode - 0xb8;
        dst.reset(new RegisterOperand(2, dstIdx));
        uint16_t imm = 0x0;
        std::copy(buffer.begin() + 1, buffer.begin() + 3, reinterpret_cast<uint8_t *>(&imm));
        src.reset(new ImmediateOperand<uint16_t>(imm)); 
        result.reset(new MovInstruction(3, std::move(dst), std::move(src)));
    } break;

    case 0xeb: {
        auto disp = static_cast<int8_t>(buffer.at(1));
        result.reset(new JmpInstruction(2, disp));
    } break;

    case 0xfa: { // CLI
        result.reset(new CliInstruction);
    } break;

    default: {
        throw std::runtime_error("instruction not implemented");
    } break;

    }

    return std::move(result);
}

class X86Cpu
{
public:
    X86Cpu(RAM& ram)
        : registerFile({
            {
                .ax = 0x0,
                .cx = 0x0,
                .dx = 0x0,
                .bx = 0x0,
                .sp = 0x0,
                .bp = 0x0,
                .si = 0x0,
                .di = 0x0,
            },
            .ip = 0x7c00,
            .flags = 0x0,
            {
                .es = 0x0,
                .cs = 0x0,
                .ss = 0x0,
                .ds = 0x0, 
            }
        }), ram(ram)
        
    {
    }

    void printRegs() const
    {
        std::cout << std::hex
                  << "AX=0x" << registerFile.ax << "\n"
                  << "BX=0x" << registerFile.bx << "\n"
                  << "SP=0x" << registerFile.sp << "\n"
                  << "SI=0x" << registerFile.si << "\n"
                  << "IP=0x" << registerFile.ip << "\n"
                  << "ES=0x" << registerFile.es << "\n"
                  << "CS=0x" << registerFile.cs << "\n"
                  << "SS=0x" << registerFile.ss << "\n"
                  << "FLAGS=0x" << registerFile.flags << "\n"
                  ;
    }

    void step()
    {
        auto buf = ram.read(registerFile.cs, registerFile.ip, 16);
        auto inst = decode(buf);
        inst->execute(registerFile, ram);
    }

private:
    X86RegisterFile registerFile;
    RAM& ram;
};

#endif // X86EMU_CPU_H
