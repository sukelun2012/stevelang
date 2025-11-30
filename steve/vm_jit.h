/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_VM_JIT_H
#define STEVE_VM_JIT_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stack>
#include <cstddef> // for size_t
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

// Forward declaration
namespace steve {
    namespace VM {
        struct Instruction;
        enum class InstructionType;
    }
}

namespace steve {
    namespace VM {

        // Simplified JIT compiler
        class JITCompiler {

        private:
            std::vector<uint8_t> codeBuffer;  // buffer for machine code
            size_t codeSize;                  // current code size
            size_t bufferSize;                // buffer size
            void* executableMemory;           // executable memory area

            // Virtual machine stack simulation
            std::stack<int64_t> vmStack;

            // Variable storage (simulation)
            std::unordered_map<std::string, int64_t> variables;

            // Register allocation (simulation)
            std::unordered_map<std::string, int> variableToReg;

            // Register usage
            std::vector<bool> regUsed;
            int nextReg;

        public:
            JITCompiler(size_t bufferSize = 1024 * 1024); // default 1MB
            ~JITCompiler();

            // Compile IR instructions to machine code
            bool compile(const std::vector<Instruction>& instructions);

            // Execute compiled code
            int64_t execute();

            // Add a machine instruction
            void emitByte(uint8_t byte);
            void emitInt(int32_t value);
            void emitLong(int64_t value);
            void emitBytes(const std::vector<uint8_t>& bytes);

            void emitMovRegReg(int dest, int src);      // MOV register to register
            void emitMovRegImm(int dest, int64_t imm);  // MOV immediate to register
            void emitMovRegMem(int dest, int baseReg, int offset); // MOV [base+offset], reg
            void emitMovMemReg(int baseReg, int offset, int src); // MOV reg, [base+offset]
            void emitAddRegReg(int dest, int src);      // ADD register addition
            void emitSubRegReg(int dest, int src);      // SUB register subtraction
            void emitMulRegReg(int dest, int src);      // MUL register multiplication
            void emitDivRegReg(int dest, int src);      // DIV register division
            void emitCmpRegReg(int reg1, int reg2);     // CMP compare registers
            void emitJmp(int offset);                   // JMP unconditional jump
            void emitJmpIf(int condition, int offset);  // conditional jump
            void emitPush(int reg);                     // PUSH register
            void emitPop(int reg);                      // POP register

            void emitReturn();                          // return instruction

            void emitCall(int funcAddr);                // call function

            void emitNeg(int reg);                      // NEG negate

            void emitNot(int reg);                      // NOT bitwise NOT

            void emitNegReg(int reg);                   // NEG register

            // Additional emit functions used in implementation

            void emitSetEqual(int reg);

            void emitSetNotEqual(int reg);

            void emitSetLess(int reg);

            void emitSetGreater(int reg);

            void emitSetLessEqual(int reg);

            void emitSetGreaterEqual(int reg);

            void emitAndRegReg(int destReg, int srcReg);

            void emitOrRegReg(int destReg, int srcReg);

            void emitModRegReg(int destReg, int srcReg);

            void emitSetZero(int reg);

            // Generate code for specific operations

            void compileBinaryOp(const std::string& op, int destReg, int srcReg);

            void compileUnaryOp(const std::string& op, int reg);

            // Compile various instructions
            void compileDefVar(const std::string& varName);
            void compileLoad(const std::string& operand);
            void compileStore(const std::string& varName);
            void compilePush(int64_t value);
            void compilePop();
            void compileIf(int& endLabel, int& elseLabel);
            void compileElse(int& endLabel);
            void compileEnd();
            void compileWhile();

            void compileDo();

            void compileReturn();

            void compilePrint();

            void compileCall(const std::string& funcName);

            void compileTry();

            void compileCatch();

            void compileBreak();

            void compileContinue();

            void compilePass();

            void compilePackage();

            // Register management

            int allocReg();

            void freeReg(int reg);

            int allocateRegister();  // Added to fix compilation error

            // Reset compiler
            void reset();

            // Get code size
            size_t getCodeSize() const;

            // Generate labels and jumps
            int newLabel();
            void placeLabel(int label);
            int getJumpOffset(int label);

        private:

            std::unordered_map<int, size_t> labelPositions;

            int labelCounter;

            std::unordered_map<int, size_t> labelOffsets;  // Added to fix compilation error

            std::vector<size_t> instructionIndices;        // Added to fix compilation error
        };

    } // namespace VM
} // namespace steve

#endif // STEVE_VM_JIT_H