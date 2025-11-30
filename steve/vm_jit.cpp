/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "vm_jit.h"
#include "vm.h" // Include definitions for Instruction and InstructionType
#include "vm_exception.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <processthreadsapi.h>
#endif

namespace steve {
    namespace VM {

        // x86-64 register definitions
        const int RAX = 0;
        const int RCX = 1;
        const int RDX = 2;
        const int RBX = 3;
        const int RSP = 4;
        const int RBP = 5;
        const int RSI = 6;
        const int RDI = 7;
        const int R8 = 8;
        const int R9 = 9;
        const int R10 = 10;
        const int R11 = 11;
        const int R12 = 12;
        const int R13 = 13;
        const int R14 = 14;
        const int R15 = 15;

        JITCompiler::JITCompiler(size_t bufferSize) : bufferSize(bufferSize), codeSize(0), nextReg(0) {
            codeBuffer.resize(bufferSize);
            executableMemory = nullptr;
            regUsed.resize(16, false); // x86-64 has 16 general purpose registers

            // Initialize register usage (skip RSP and RBP, as they have special purposes)
            regUsed[RSP] = true;  // Stack pointer
            regUsed[RBP] = true;  // Base pointer
        }

        JITCompiler::~JITCompiler() {
            if (executableMemory) {
#ifdef _WIN32
                // On Windows, use VirtualFree
                VirtualFree(executableMemory, 0, MEM_RELEASE);
#else
                // On Unix-like systems, use munmap or free
                // For now, just use free as a placeholder
                free(executableMemory);
#endif
            }
        }

        bool JITCompiler::compile(const std::vector<Instruction>& program) {
            // Clear the code buffer
            codeBuffer.clear();
            codeSize = 0;
            nextReg = 0;
            labelOffsets.clear();

            // Function prologue: save registers and set up stack frame
            // push rbp
            emitByte(0x55);
            // mov rbp, rsp
            emitByte(0x48);
            emitByte(0x89);
            emitByte(0xE5);

            // Reserve space for local variables (1024 bytes for example)
            emitByte(0x48); // sub rsp, 1024
            emitByte(0x83);
            emitByte(0xEC);
            emitByte(0x00); // Will be updated to 0x04 (1024/8) later

            // Compile each instruction in the program
            for (size_t i = 0; i < program.size(); i++) {
                const Instruction& instr = program[i];
                // Store instruction index for error reporting
                instructionIndices.push_back(i);

                switch (instr.type) {
                case InstructionType::DEFVAR: {
                    // In JIT, variable definition allocates memory space
                    // We map variables to stack offsets
                    // Actually, we'll generate machine code that allocates memory
                    // Create a local variable area at function start
                    // Here we just record the variable's existence, actual allocation is handled at function start
                    break;
                }

                case InstructionType::LOAD: {
                    // In JIT, Push operation loads value into register
                    // We need a runtime stack to simulate the VM stack
                    int reg = allocateRegister();

                    if (!instr.operands.empty()) {
                        std::string operand = instr.operands[0];

                        // Simulate stack operation: store register value to stack
                        // Use RSP as stack pointer

                        // Load value to register
                        if (operand[0] == '"' && operand.back() == '"') {
                            // If it's immediate value
                            std::string strValue = operand.substr(1, operand.length() - 2);
                            // If it's a number, load to register
                            try {
                                if (operand.find('.') != std::string::npos) {
                                    // Floating point number
                                    double val = std::stod(strValue);
                                    // For simplicity, convert to int64_t directly
                                    emitMovRegImm(reg, *reinterpret_cast<int64_t*>(&val));
                                } else {
                                    // Integer
                                    int64_t val = std::stoll(strValue);
                                    emitMovRegImm(reg, val);
                                }
                            } catch (...) {
                                // If not a number, it might be a string or special value
                                // Handle string (if starts and ends with quotes)
                                // String value, extract content
                                // Simplification: push string length as value to stack
                                emitMovRegImm(reg, static_cast<int64_t>(strValue.length()));
                            }
                        } else if (operand == "true" || operand == "false") {
                            // Boolean value
                            emitMovRegImm(reg, operand == "true" ? 1 : 0);
                        } else if (operand == "null") {
                            // Null value
                            emitMovRegImm(reg, 0);
                        } else {
                            // Variable name (like "x"), load from memory
                            // In our implementation, variables will be stored in the local variable area
                            // First need to know the variable's stack offset
                            // Assume variables are stored at fixed offsets below the base pointer
                            // Actual implementation needs to maintain mapping from variables to offsets
                            emitMovRegMem(reg, RBP, -8); // Example: load variable x from RBP-8
                        }

                        // Push value to VM stack (x86-64 stack)
                        emitPush(reg);
                    }

                    break;
                }

                case InstructionType::STORE: {
                    // Store top stack value to variable
                    if (!instr.operands.empty()) {
                        std::string varName = instr.operands[0];
                        int valueReg = allocateRegister();
                        emitPop(valueReg); // Pop value from VM stack

                        // Store to variable location (simplified)
                        // In real implementation, we would map variable name to stack offset
                        emitMovMemReg(RBP, -8, valueReg); // Example: store to RBP-8 (variable x)
                    }
                    break;
                }

                case InstructionType::IF: {
                    // Pop condition value from stack
                    int condReg = allocateRegister();
                    emitPop(condReg); // Pop value from VM stack

                    // Compare condition value with 0
                    emitCmpRegImm(condReg, 0);

                    // If condition is false (equals 0), jump to else part
                    // Generate conditional jump, need to reserve space first
                    // jne (jump if not equal) to skip else part
                    emitByte(0x75);
                    emitInt(0); // Placeholder offset, needs to be fixed later
                    break;
                }

                case InstructionType::ELSE: {
                    // Generate ELSE structure machine code
                    // Need unconditional jump to skip if part
                    // Generate unconditional jump, skip if part
                    emitByte(0xE9);
                    emitInt(0); // Placeholder offset
                    break;
                }

                case InstructionType::END: {
                    // End control flow structure
                    // Can generate label here but actual label position needs to be determined at compile time
                    break;
                }

                case InstructionType::WHILE: {
                    // WHILE loop start
                    // Need to generate loop label
                    break;
                }

                case InstructionType::DO: {
                    // DO loop body start
                    // Loop body part
                    break;
                }

                case InstructionType::RETURN: {
                    // Return instruction
                    break;
                }

                case InstructionType::PRINT: {
                    // Pop a value from VM stack and print it
                    // First pop value from stack
                    int reg = allocateRegister();
                    emitPop(reg); // Pop value from VM stack

                    // Call print function
                    // Save registers
                    emitPush(RDI); // Save RDI

                    // Set parameters (value in RDI)
                    emitMovRegReg(RDI, reg); // First parameter (value) to RDI

                    // In actual implementation, we need to generate call to a print function
                    // For simplicity, we generate code that sets up parameters
                    // Here we assume there's an address of a print function
                    // To simplify, we generate some code to set parameters

                    emitByte(0x48); // MOV RAX, 0 (system call number)
                    emitByte(0xC7);
                    emitByte(0xC0);
                    emitByte(0x00);
                    emitByte(0x00);
                    emitByte(0x00);
                    emitByte(0x00);

                    // Restore registers
                    emitPop(RDI); // Restore RDI

                    break;
                }

                case InstructionType::CALL: {
                    // Function call implementation
                    // First, pop arguments
                    int argReg = allocateRegister();
                    if (!instr.operands.empty()) {
                        std::string funcName = instr.operands[0];

                        // In actual implementation, we need to generate call to specific function based on name
                        // For print, we've already implemented it
                        emitPush(argReg); // Push argument back on stack

                        // For input, generate special handling
                        if (funcName == "input") {
                            emitMovRegImm(reg, 0); // Placeholder value
                        }

                        // For user-defined functions, we need to look up and call
                        // Simplified handling: generate placeholder call
                    }
                    break;
                }

                case InstructionType::FUNC: {
                    // Generate function entry code
                    // Save base pointer
                    emitByte(0x55); // push rbp
                    emitByte(0x48); // mov rbp, rsp
                    emitByte(0x89);
                    emitByte(0xE5);

                    // Compile all instructions
                    break;
                }

                case InstructionType::BINARY_OP: {
                    // Pop two operands and perform operation
                    int rightReg = allocateRegister();
                    int leftReg = allocateRegister();

                    emitPop(rightReg); // Pop right operand from VM stack
                    emitPop(leftReg);  // Pop left operand from VM stack

                    if (!instr.operands.empty()) {
                        std::string op = instr.operands[0];

                        if (op == "+") {
                            emitAddRegReg(leftReg, rightReg); // leftReg = leftReg + rightReg
                        } else if (op == "-") {
                            emitSubRegReg(leftReg, rightReg); // leftReg = leftReg - rightReg
                        } else if (op == "*") {
                            emitMulRegReg(leftReg, rightReg); // leftReg = leftReg * rightReg
                        } else if (op == "/") {
                            // For division, need to handle division by zero
                            emitCmpRegImm(rightReg, 0);
                            // This is simplified - in real code, we'd add error handling
                            emitDivRegReg(leftReg, rightReg); // leftReg = leftReg / rightReg
                        } else if (op == "%") {
                            emitModRegReg(leftReg, rightReg); // leftReg = leftReg % rightReg
                        } else if (op == "==") {
                            // Compare and set result
                            // Set equal flag to destReg
                            emitCmpRegReg(leftReg, rightReg); // Reverse comparison
                            emitSetEqual(leftReg); // Set result of comparison to register
                            // Extend 8-bit result to 64-bit
                        } else if (op == "!=") {
                            // Compare and set result
                            // Set not equal flag to destReg
                            emitCmpRegReg(leftReg, rightReg); // Reverse comparison
                            emitSetNotEqual(leftReg); // Set result of comparison to register
                            // Extend 8-bit result to 64-bit
                        } else if (op == "<") {
                            emitCmpRegReg(leftReg, rightReg);
                            emitSetLess(leftReg); // Set result to 1 if left < right, 0 otherwise
                        } else if (op == ">") {
                            emitCmpRegReg(leftReg, rightReg);
                            emitSetGreater(leftReg); // Set result to 1 if left > right, 0 otherwise
                        } else if (op == "<=") {
                            emitCmpRegReg(leftReg, rightReg);
                            emitSetLessEqual(leftReg); // Set result to 1 if left <= right, 0 otherwise
                        } else if (op == ">=") {
                            emitCmpRegReg(leftReg, rightReg);
                            emitSetGreaterEqual(leftReg); // Set result to 1 if left >= right, 0 otherwise
                        } else if (op == "and" || op == "&&") {
                            // Bitwise AND
                            emitAndRegReg(leftReg, rightReg);
                        } else if (op == "or" || op == "||") {
                            // Bitwise OR
                            emitOrRegReg(leftReg, rightReg);
                        }
                    }

                    // Push result back to VM stack
                    emitPush(leftReg);
                    break;
                }

                case InstructionType::UNARY_OP: {
                    // Pop operand and perform unary operation
                    int reg = allocateRegister();
                    emitPop(reg); // Pop operand from VM stack

                    if (!instr.operands.empty()) {
                        std::string op = instr.operands[0];

                        if (op == "-") {
                            emitNegReg(reg); // Negate value in register
                        } else if (op == "!" || op == "not") {
                            // For logical NOT, first compare with 0, then set result
                            emitCmpRegImm(reg, 0);
                            // Set zero flag result
                            emitSetZero(reg); // Set result of zero comparison to register
                            // Extend 8-bit result to 64-bit
                        }
                    }

                    // Push result back to VM stack
                    emitPush(reg);
                    break;
                }

                case InstructionType::PUSH: {
                    // If not a number, might be string or identifier
                    if (!instr.operands.empty()) {
                        std::string operand = instr.operands[0];
                        int reg = allocateRegister();

                        // Handle immediate values
                        try {
                            if (operand.find('.') != std::string::npos) {
                                // Floating point number
                                double val = std::stod(operand);
                                emitMovRegImm(reg, *reinterpret_cast<int64_t*>(&val));
                            } else {
                                // Integer
                                int64_t val = std::stoll(operand);
                                emitMovRegImm(reg, val);
                            }
                        } catch (...) {
                            // Variable name or other identifier
                            // For now just push 0
                            emitMovRegImm(reg, 0);
                        }

                        // Push value to VM stack
                        emitPush(reg);
                    }
                    break;
                }

                case InstructionType::POP: {
                    // Pop value from VM stack
                    int reg = allocateRegister();
                    emitPop(reg);
                    // Value is now in register, can be used as needed
                    break;
                }

                case InstructionType::GOTO: {
                    // Jump instruction, using label name instead of number
                    if (!instr.operands.empty()) {
                        std::string labelName = instr.operands[0];

                        // In actual implementation, we need a mapping from label names to addresses
                        // Here we use a virtual label system
                        int labelId = std::hash<std::string>{}(labelName); // Simple hash as label ID
                        // Generate jump to label instruction
                        emitJmp(0); // Placeholder offset, actual offset needs to be calculated at link time
                    }
                    break;
                }

                case InstructionType::TRY: {
                    // Try statement - exception handling
                    // For JIT, we'll implement as a no-op for now
                    break;
                }

                case InstructionType::CATCH: {
                    // Catch statement - exception handling
                    // For JIT, we'll implement as a no-op for now
                    break;
                }

                case InstructionType::BREAK: {
                    // Break statement - exit current loop
                    // This would require more complex control flow management
                    break;
                }

                case InstructionType::CONTINUE: {
                    // Continue statement - continue to next iteration
                    // This would require more complex control flow management
                    break;
                }

                case InstructionType::PASS: {
                    // Pass statement - no operation
                    emitNop();
                    break;
                }

                case InstructionType::PACKAGE: {
                    // Package declaration - no runtime effect
                    if (!instr.operands.empty()) {
                        std::string packageName = instr.operands[0];
                        // Store package info if needed for module system
                    }
                    break;
                }

                case InstructionType::LABEL: {
                    // Label definition
                    if (!instr.operands.empty()) {
                        std::string labelName = instr.operands[0];

                        // In actual implementation, we need to record label position
                        int labelId = std::hash<std::string>{}(labelName); // Simple hash as label ID
                        labelOffsets[labelId] = codeSize; // Record current position as label location
                    }
                    break;
                }

                case InstructionType::GC_NEW:
                case InstructionType::GC_DELETE:
                case InstructionType::GC_RUN:
                case InstructionType::MEM_MALLOC:
                case InstructionType::MEM_FREE: {
                    // Garbage collection and memory management instructions
                    // Need to generate call to corresponding C++ functions

                    // Save registers
                    emitPush(RDI); // Save RDI

                    if (instr.type == InstructionType::MEM_MALLOC ||
                        instr.type == InstructionType::GC_NEW) {
                        // Need one parameter (size)
                        int sizeReg = allocateRegister();
                        if (!emitPop(sizeReg)) { // Get size parameter from stack
                            emitMovRegImm(sizeReg, 1); // Default size
                        }

                        // In actual implementation, need to call corresponding C++ function
                        // Generate placeholder call
                    } else if (instr.type == InstructionType::MEM_FREE ||
                               instr.type == InstructionType::GC_DELETE) {
                        // Need one parameter (pointer)
                        int ptrReg = allocateRegister();
                        emitPop(ptrReg); // Get pointer parameter from stack

                        // Generate placeholder call
                    } else if (instr.type == InstructionType::GC_RUN ||
                               instr.type == InstructionType::MEM_MALLOC ||
                               instr.type == InstructionType::MEM_FREE) {
                        // Run garbage collection, no parameters
                        // Generate placeholder call
                    }

                    // Restore registers
                    emitPop(RDI); // Restore RDI
                    break;
                }

                case InstructionType::NOP: {
                    // No operation
                    emitNop();
                    break;
                }

                default:
                    // Unknown instruction, generate no operation
                    emitNop();
                    break;
                }

                // Update the instruction index mapping
                instructionIndices.back() = i;
            }

            // Function epilogue: restore registers and return
            // add rsp, 1024 (release local variable space)
            emitByte(0x48);
            emitByte(0x83);
            emitByte(0xC4);
            emitByte(0x00); // Will be updated to 0x04 (1024/8) later

            // pop rbp
            emitByte(0x5D);
            // ret
            emitByte(0xC3);

            // Generate function exit code

            // Copy generated machine code to executable memory
            executableMemory = allocateExecutableMemory(codeSize);
            if (!executableMemory) {
                return false;
            }

            std::memcpy(executableMemory, codeBuffer.data(), codeSize);

            return true;
        }

        void* JITCompiler::allocateExecutableMemory(size_t size) {
#ifdef _WIN32
            // On Windows, use VirtualAlloc with PAGE_EXECUTE_READWRITE
            return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
            // On Unix-like systems, use mmap with PROT_READ, PROT_WRITE, PROT_EXEC
            // For now, just use malloc as a placeholder (not executable)
            return malloc(size);
#endif
        }

        void JITCompiler::emitByte(uint8_t byte) {
            if (codeSize < codeBuffer.size()) {
                codeBuffer[codeSize++] = byte;
            }
        }

        void JITCompiler::emitInt(uint32_t value) {
            for (int i = 0; i < 4; i++) {
                emitByte((value >> (i * 8)) & 0xFF);
            }
        }

        void JITCompiler::emitLong(uint64_t value) {
            for (int i = 0; i < 8; i++) {
                emitByte((value >> (i * 8)) & 0xFF);
            }
        }

        void JITCompiler::emitMovRegImm(int reg, int64_t imm) {
            // MOV instruction for 64-bit register with immediate value
            // REX.W + MOV r64, imm64 (48 B8+rd imm64)
            emitByte(0x48); // REX.W prefix
            emitByte(0xB8 + (reg & 0x7)); // MOV reg, imm64
            emitLong(imm);
        }

        void JITCompiler::emitMovRegReg(int destReg, int srcReg) {
            // MOV instruction between registers
            // REX.R + MOV r64, r64 (48 89 /r)
            emitByte(0x48); // REX.W prefix
            emitByte(0x89);
            // ModR/M byte: 11 (direct addressing) + destReg*8 + srcReg
            emitByte(0xC0 | ((destReg & 0x7) << 3) | (srcReg & 0x7));
        }

        void JITCompiler::emitAddRegReg(int destReg, int srcReg) {
            // ADD instruction between registers
            emitByte(0x48); // REX.W prefix
            emitByte(0x01);
            emitByte(0xC0 | ((destReg & 0x7) << 3) | (srcReg & 0x7));
        }

        void JITCompiler::emitSubRegReg(int destReg, int srcReg) {
            // SUB instruction between registers
            emitByte(0x48); // REX.W prefix
            emitByte(0x29);
            emitByte(0xC0 | ((destReg & 0x7) << 3) | (srcReg & 0x7));
        }

        void JITCompiler::emitCmpRegReg(int reg1, int reg2) {
            // CMP instruction between registers
            emitByte(0x48); // REX.W prefix
            emitByte(0x39);
            emitByte(0xC0 | ((reg1 & 0x7) << 3) | (reg2 & 0x7));
        }

        void JITCompiler::emitCmpRegImm(int reg, int64_t imm) {
            // CMP instruction with immediate value
            if (imm >= -128 && imm <= 127) {
                // Use 8-bit immediate
                emitByte(0x48); // REX.W prefix
                emitByte(0x83);
                emitByte(0xF8 | (reg & 0x7));
                emitByte(imm & 0xFF);
            } else {
                // Use 32-bit immediate
                emitByte(0x48); // REX.W prefix
                emitByte(0x81);
                emitByte(0xF8 | (reg & 0x7));
                emitInt(imm & 0xFFFFFFFF);
            }
        }

        void JITCompiler::emitPush(int reg) {
            // PUSH register
            if (reg < 8) {
                emitByte(0x50 + reg); // PUSH reg
            } else {
                emitByte(0x41); // REX.B prefix
                emitByte(0x50 + (reg & 0x7)); // PUSH reg
            }
        }

        bool JITCompiler::emitPop(int reg) {
            // POP register
            if (reg < 8) {
                emitByte(0x58 + reg); // POP reg
                return true;
            } else {
                emitByte(0x41); // REX.B prefix
                emitByte(0x58 + (reg & 0x7)); // POP reg
                return true;
            }
        }

        void JITCompiler::emitNop() {
            // NOP instruction
            emitByte(0x90);
        }

        void JITCompiler::emitJmp(int32_t offset) {
            // JMP instruction with relative offset
            emitByte(0xE9);
            emitInt(offset);
        }

        void JITCompiler::emitSetEqual(int reg) {
            // Set byte if equal (for comparison result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x94); // SETE instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        void JITCompiler::emitSetNotEqual(int reg) {
            // Set byte if not equal (for comparison result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x95); // SETNE instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        int JITCompiler::allocateRegister() {
            // Find and return an available register
            for (size_t i = 0; i < regUsed.size(); i++) {
                if (!regUsed[i]) {
                    regUsed[i] = true;
                    return static_cast<int>(i);
                }
            }
            // If no registers available, try to reuse (simplified)
            // In real implementation, use register spilling
            regUsed[nextReg] = true;
            int result = nextReg;
            nextReg = (nextReg + 1) % regUsed.size();
            return result;
        }

        void JITCompiler::freeRegister(int reg) {
            if (reg >= 0 && static_cast<size_t>(reg) < regUsed.size()) {
                regUsed[reg] = false;
            }
        }

        int64_t JITCompiler::execute() {
            if (!executableMemory) {
                throw RuntimeError("JIT code not compiled");
            }

            // Cast function pointer and execute
            int64_t(*func)() = reinterpret_cast<int64_t(*)()>(executableMemory);
            return func();
        }

        void JITCompiler::emitMulRegReg(int destReg, int srcReg) {
            // IMUL instruction between registers
            emitByte(0x48); // REX.W prefix
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0xAF); // IMUL instruction
            emitByte(0xC0 | ((destReg & 0x7) << 3) | (srcReg & 0x7));
        }

        void JITCompiler::emitDivRegReg(int destReg, int srcReg) {
            // IDIV instruction (divides RAX:RDX by register)
            // This is a simplified implementation, real division is more complex
            emitByte(0x48); // REX.W prefix
            emitByte(0xF7); // DIV/IDIV instruction
            emitByte(0xF8 | (srcReg & 0x7)); // IDIV srcReg
        }

        void JITCompiler::emitModRegReg(int destReg, int srcReg) {
            // For modulo, we perform division and get the remainder from RDX
            emitDivRegReg(destReg, srcReg); // This performs division, remainder ends up in RDX
            emitMovRegReg(destReg, RDX); // Move remainder to destination
        }

        void JITCompiler::emitSetLess(int reg) {
            // Set byte if less (for comparison result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x9C); // SETL instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        void JITCompiler::emitSetGreater(int reg) {
            // Set byte if greater (for comparison result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x9F); // SETG instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        void JITCompiler::emitSetLessEqual(int reg) {
            // Set byte if less or equal (for comparison result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x9E); // SETLE instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        void JITCompiler::emitSetGreaterEqual(int reg) {
            // Set byte if greater or equal (for comparison result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x9D); // SETGE instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        void JITCompiler::emitAndRegReg(int destReg, int srcReg) {
            // AND instruction between registers
            emitByte(0x48); // REX.W prefix
            emitByte(0x21);
            emitByte(0xC0 | ((destReg & 0x7) << 3) | (srcReg & 0x7));
        }

        void JITCompiler::emitOrRegReg(int destReg, int srcReg) {
            // OR instruction between registers
            emitByte(0x48); // REX.W prefix
            emitByte(0x09);
            emitByte(0xC0 | ((destReg & 0x7) << 3) | (srcReg & 0x7));
        }

        void JITCompiler::emitNegReg(int reg) {
            // NEG instruction (negate value in register)
            emitByte(0xF7); // NEG instruction
            emitByte(0xD8 | (reg & 0x7)); // ModR/M byte for NEG reg
        }

        void JITCompiler::emitSetZero(int reg) {
            // Set byte if zero (for logical NOT result)
            emitByte(0x40 | (reg >= 8 ? 0x40 : 0x00)); // REX prefix if needed
            emitByte(0x0F); // 2-byte opcode prefix
            emitByte(0x94); // SETZ instruction
            emitByte(0xC0 | (reg & 0x7)); // ModR/M byte
        }

        void JITCompiler::emitMovRegMem(int destReg, int baseReg, int offset) {
            // MOV register, [baseReg + offset]
            emitByte(0x48); // REX.W prefix (for 64-bit operation)
            emitByte(0x8B); // MOV r32, r/m32 (but with REX.W becomes MOV r64, r/m64)

            if (offset == 0 && baseReg != RBP && baseReg != R13) {
                // If offset is 0 and not using RBP/R13 as base (which would need SIB byte), use simple encoding
                emitByte(0x04 | ((destReg & 0x7) << 3) | (baseReg & 0x7));
            } else if (offset >= -128 && offset <= 127) {
                // Use 8-bit displacement
                emitByte(0x40 | ((destReg & 0x7) << 3) | (baseReg & 0x7)); // ModR/M
                emitByte(offset & 0xFF); // 8-bit displacement
            } else {
                // Use 32-bit displacement
                emitByte(0x80 | ((destReg & 0x7) << 3) | (baseReg & 0x7)); // ModR/M
                emitInt(offset & 0xFFFFFFFF); // 32-bit displacement
            }
        }

        void JITCompiler::emitMovMemReg(int baseReg, int offset, int srcReg) {
            // MOV [baseReg + offset], srcReg
            emitByte(0x48); // REX.W prefix (for 64-bit operation)
            emitByte(0x89); // MOV r/m32, r32 (but with REX.W becomes MOV r/m64, r64)

            if (offset == 0 && baseReg != RBP && baseReg != R13) {
                // If offset is 0 and not using RBP/R13 as base (which would need SIB byte), use simple encoding
                emitByte(0x04 | ((srcReg & 0x7) << 3) | (baseReg & 0x7));
            } else if (offset >= -128 && offset <= 127) {
                // Use 8-bit displacement
                emitByte(0x40 | ((srcReg & 0x7) << 3) | (baseReg & 0x7)); // ModR/M
                emitByte(offset & 0xFF); // 8-bit displacement
            } else {
                // Use 32-bit displacement
                emitByte(0x80 | ((srcReg & 0x7) << 3) | (baseReg & 0x7)); // ModR/M
                emitInt(offset & 0xFFFFFFFF); // 32-bit displacement
            }
        }

        // Additional methods that were declared in header but not implemented

        void JITCompiler::emitBytes(const std::vector<uint8_t>& bytes) {
            for (uint8_t byte : bytes) {
                emitByte(byte);
            }
        }

        void JITCompiler::emitJmpIf(int condition, int offset) {
            // Conditional jump implementation
            // This would emit the appropriate conditional jump instruction based on the condition
            // For now, using a simple placeholder approach
            emitByte(0x0F);  // 2-byte jump opcode prefix
            emitByte(0x80 + condition);  // conditional jump opcode
            emitInt(offset);  // offset
        }

        void JITCompiler::emitReturn() {
            emitByte(0xC3);  // RET instruction
        }

        void JITCompiler::emitCall(int funcAddr) {
            emitByte(0xE8);  // CALL instruction
            emitInt(funcAddr);  // function address
        }

        void JITCompiler::emitNeg(int reg) {
            emitByte(0xF7);  // NEG instruction
            emitByte(0xD8 + (reg & 0x7));  // ModR/M byte for NEG reg
        }

        void JITCompiler::emitNot(int reg) {
            emitByte(0xF7);  // NOT instruction
            emitByte(0xD0 + (reg & 0x7));  // ModR/M byte for NOT reg
        }

        // Generate code for specific operations

        void JITCompiler::compileBinaryOp(const std::string& op, int destReg, int srcReg) {
            if (op == "+") {
                emitAddRegReg(destReg, srcReg);
            } else if (op == "-") {
                emitSubRegReg(destReg, srcReg);
            } else if (op == "*") {
                emitMulRegReg(destReg, srcReg);
            } else if (op == "/") {
                emitDivRegReg(destReg, srcReg);
            } else if (op == "%") {
                emitModRegReg(destReg, srcReg);
            } else if (op == "==") {
                emitCmpRegReg(destReg, srcReg);
                emitSetEqual(destReg);
            } else if (op == "!=") {
                emitCmpRegReg(destReg, srcReg);
                emitSetNotEqual(destReg);
            } else if (op == "<") {
                emitCmpRegReg(destReg, srcReg);
                emitSetLess(destReg);
            } else if (op == ">") {
                emitCmpRegReg(destReg, srcReg);
                emitSetGreater(destReg);
            } else if (op == "<=") {
                emitCmpRegReg(destReg, srcReg);
                emitSetLessEqual(destReg);
            } else if (op == ">=") {
                emitCmpRegReg(destReg, srcReg);
                emitSetGreaterEqual(destReg);
            } else if (op == "and" || op == "&&") {
                emitAndRegReg(destReg, srcReg);
            } else if (op == "or" || op == "||") {
                emitOrRegReg(destReg, srcReg);
            }
        }

        void JITCompiler::compileUnaryOp(const std::string& op, int reg) {
            if (op == "-") {
                emitNegReg(reg);
            } else if (op == "!" || op == "not") {
                emitCmpRegImm(reg, 0);
                emitSetZero(reg);
            } else if (op == "~") {
                emitNot(reg);
            }
        }

        // Compile various instructions
        void JITCompiler::compileDefVar(const std::string& varName) {
            // In JIT, variable definition allocates memory space
            // We map variables to stack offsets
        }

        void JITCompiler::compileLoad(const std::string& operand) {
            int reg = allocateRegister();
            // Try to parse as number
            try {
                if (operand.find('.') != std::string::npos) {
                    // Floating point number
                    double val = std::stod(operand);
                    emitMovRegImm(reg, *reinterpret_cast<int64_t*>(&val));
                } else {
                    // Integer
                    int64_t val = std::stoll(operand);
                    emitMovRegImm(reg, val);
                }
            } catch (...) {
                // If not a number, treat as variable name or other identifier
                emitMovRegImm(reg, 0);
            }

            // Push value to VM stack
            emitPush(reg);
        }

        void JITCompiler::compileStore(const std::string& varName) {
            // Store top stack value to variable
            int valueReg = allocateRegister();
            if (emitPop(valueReg)) {
                // Store to variable location (simplified)
            }
        }

        void JITCompiler::compilePush(int64_t value) {
            int reg = allocateRegister();
            emitMovRegImm(reg, value);
            emitPush(reg);
        }

        void JITCompiler::compilePop() {
            int reg = allocateRegister();
            emitPop(reg);
        }

        void JITCompiler::compileIf(int& endLabel, int& elseLabel) {
            // Pop condition value from stack
            int condReg = allocateRegister();
            emitPop(condReg); // Pop value from VM stack

            // Compare condition value with 0
            emitCmpRegImm(condReg, 0);

            // If condition is false (equals 0), jump to else part
            emitByte(0x75);  // JNE (jump if not equal)
            elseLabel = codeSize;  // Placeholder offset
            emitInt(0);  // Placeholder offset, needs to be fixed later
        }

        void JITCompiler::compileElse(int& endLabel) {
            // Generate unconditional jump to skip if part
            emitByte(0xE9);  // JMP
            endLabel = codeSize;  // Placeholder offset
            emitInt(0);  // Placeholder offset
        }

        void JITCompiler::compileEnd() {
            // End control flow structure - no operation needed
        }

        void JITCompiler::compileWhile() {
            // WHILE loop start - no operation needed
        }

        void JITCompiler::compileDo() {
            // DO loop body start - no operation needed
        }

        void JITCompiler::compileReturn() {
            // Return instruction
            emitReturn();
        }

        void JITCompiler::compilePrint() {
            // Pop a value from VM stack and print it
            int reg = allocateRegister();
            emitPop(reg); // Pop value from VM stack
        }

        void JITCompiler::compileCall(const std::string& funcName) {
            // Function call implementation
        }

        void JITCompiler::compileTry() {
            // Try statement - exception handling
        }

        void JITCompiler::compileCatch() {
            // Catch statement - exception handling
        }

        void JITCompiler::compileBreak() {
            // Break statement - exit current loop
        }

        void JITCompiler::compileContinue() {
            // Continue statement - continue to next iteration
        }

        void JITCompiler::compilePass() {
            // Pass statement - no operation
            emitNop();
        }

        void JITCompiler::compilePackage() {
            // Package declaration - no runtime effect
        }

        // Register management
        int JITCompiler::allocReg() {
            return allocateRegister();
        }

        void JITCompiler::freeReg(int reg) {
            if (reg >= 0 && static_cast<size_t>(reg) < regUsed.size()) {
                regUsed[reg] = false;
            }
        }

        // Reset compiler
        void JITCompiler::reset() {
            codeSize = 0;
            nextReg = 0;
            for (size_t i = 0; i < regUsed.size(); i++) {
                regUsed[i] = false;
            }
            // Initialize register usage (skip RSP and RBP, as they have special purposes)
            regUsed[RSP] = true;  // Stack pointer
            regUsed[RBP] = true;  // Base pointer
        }

        // Get code size
        size_t JITCompiler::getCodeSize() const {
            return codeSize;
        }

        // Generate labels and jumps
        int JITCompiler::newLabel() {
            int labelId = labelCounter++;
            labelPositions[labelId] = 0; // initially unknown position
            return labelId;
        }

        void JITCompiler::placeLabel(int label) {
            labelPositions[label] = codeSize;
        }

        int JITCompiler::getJumpOffset(int label) {
            if (labelPositions.find(label) != labelPositions.end()) {
                return static_cast<int>(labelPositions[label] - codeSize);
            }
            return 0; // default to 0 if label not found
        }

    } // namespace VM
} // namespace steve