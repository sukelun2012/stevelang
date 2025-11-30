/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#ifndef STEVE_VM_H
#define STEVE_VM_H
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>
#include <stack>
#include <functional>
#include <cstddef>
#include <iostream>
#include <fstream>  // For FileHandle
#include <bitset>   // For bs function implementation

// Forward declaration
namespace steve {
    namespace VM {
        class VMGarbageCollector;
        class JITCompiler;
    }
}

namespace steve {
    namespace VM {

        // Forward declarations for Value type
        struct ListValue;
        struct DictValue;
        struct PointerValue;
        
        // Value type definition
        using Value = std::variant<int, double, bool, std::string, std::nullptr_t, int64_t, PointerValue, ListValue, DictValue>;
        
        // Memory-managed object structure for pointer system
        struct ManagedObject {
            void* data;
            std::string type;
            size_t size;
            bool marked;  // For garbage collection
            
            ManagedObject(void* d, const std::string& t, size_t s) 
                : data(d), type(t), size(s), marked(false) {}
            
            ~ManagedObject() {
                if (data) {
                    std::free(data);  // Free the data
                    data = nullptr;
                }
            }
        };
        
        // Pointer structure definition
        struct PointerValue {
            ManagedObject* obj;  // Pointer to managed object
            void* ptr;           // Raw pointer value
            std::string type;    // The type of object being pointed to
            bool isNull;
            bool isWeak;         // Whether this is a weak pointer
            bool isRef;          // Whether this is a reference (cannot be null)
            
            PointerValue() : obj(nullptr), ptr(nullptr), type(""), isNull(true), isWeak(false), isRef(false) {}
            PointerValue(ManagedObject* o, const std::string& t, bool weak = false, bool ref = false) 
                : obj(o), ptr(o ? o->data : nullptr), type(t), isNull(o == nullptr), isWeak(weak), isRef(ref) {}
            PointerValue(void* p, const std::string& t, bool weak = false, bool ref = false)
                : obj(nullptr), ptr(p), type(t), isNull(p == nullptr), isWeak(weak), isRef(ref) {}
            
            void* getPointer() const { 
                return obj ? obj->data : ptr; 
            }
            
            std::string getObjectType() const {
                return obj ? obj->type : type;
            }
        };
        
        // List/Array structure definition
        struct ListValue {
            std::vector<Value> items;
            
            ListValue() {}
            ListValue(const std::vector<Value>& v) : items(v) {}
        };
        
        // Dictionary structure definition
        struct DictValue {
            std::unordered_map<std::string, Value> items;
            
            DictValue() {}
            DictValue(const std::unordered_map<std::string, Value>& m) : items(m) {}
        };

        // Debug command enum
        enum class DebugCommand {
            NONE,       // No debug command
            STEP,       // Step one instruction
            STEP_OVER,  // Step over function call
            STEP_INTO,  // Step into function call
            STEP_OUT,   // Step out of current function
            CONTINUE,   // Continue execution until next breakpoint
            BREAK       // Break execution
        };

        // Breakpoint structure
        struct Breakpoint {
            int line;
            size_t pc;  // program counter position
            bool enabled;
            std::string condition;  // conditional breakpoint
            bool temporary;         // temporary breakpoint for step-over, etc.
            
            Breakpoint(int l, size_t p) : line(l), pc(p), enabled(true), temporary(false) {}
            Breakpoint(int l, size_t p, const std::string& cond) : line(l), pc(p), enabled(true), condition(cond), temporary(false) {}
        };

        enum class InstructionType {
            DEFVAR,     // Define variable
            LOAD,       // Load variable
            STORE,      // Store variable
            FUNC,       // Function definition
            CALL,       // Function call
            IF,         // If statement start
            ELSE,       // Else statement
            END,        // End statement
            WHILE,      // While loop start
            DO,         // Do-while loop start
            RETURN,     // Return statement
            IMPORT,     // Import statement
            PRINT,      // Print statement
            INPUT,      // Input statement
            BINARY_OP,  // Binary operation
            UNARY_OP,   // Unary operation
            PUSH,       // Push to stack
            POP,        // Pop from stack
            GOTO,       // Goto statement
            LABEL,      // Label statement
            GC_NEW,     // Create GC object
            GC_DELETE,  // Delete GC object
            GC_RUN,     // Run garbage collection
            MEM_MALLOC, // Memory allocation
            MEM_FREE,   // Memory deallocation
            TRY,        // Try statement start
            CATCH,      // Catch statement start
            BREAK,      // Break statement
            CONTINUE,   // Continue statement
            PASS,       // Pass statement
            PACKAGE,    // Package declaration
            PTR_NEW,    // Create new pointer
            PTR_DEREF,  // Dereference pointer
            THROW,      // Throw exception
            NOP,        // No operation
            DEBUG       // Debug instruction
        };

        // Instruction structure
        struct Instruction {
            InstructionType type;
            std::string operand1;
            std::string operand2;
            std::string operand3;
            Value literal;
            int line;
            std::vector<std::string> operands;

            Instruction() : type(InstructionType::NOP), literal(nullptr), line(-1) {
                operands = std::vector<std::string>(3);
            }
        };

        // Machine state structure
        struct MachineState {
            size_t pc;
            bool running;
            int rax, rbx, rcx, rdx;
            std::vector<std::unordered_map<std::string, Value>> scopes;
            std::vector<Instruction> program;
            std::vector<Value> stack;
            std::unordered_map<std::string, Value> variables;
            std::unordered_map<std::string, size_t> functions;

            MachineState() : pc(0), running(false), rax(0), rbx(0), rcx(0), rdx(0) {}
        };

        // Debug state structure
        struct DebugState {
            bool debugging;              // Whether debugger is active
            DebugCommand pendingCommand; // Pending debug command
            std::vector<Breakpoint> breakpoints; // Active breakpoints
            std::vector<size_t> callStack; // Function call stack for step-out
            size_t stepOverTarget;       // Target PC for step-over
            bool isStepping;             // Whether we're currently stepping
            size_t currentCallDepth;     // Current function call depth
            
            DebugState() : debugging(false), pendingCommand(DebugCommand::NONE), stepOverTarget(0), isStepping(false), currentCallDepth(0) {}
        };

        // File handle structure to keep track of open files
        struct FileHandle {
            std::fstream* stream;
            std::string filename;
            std::string mode;
            bool isOpen;
            
            FileHandle() : stream(nullptr), isOpen(false) {}
            FileHandle(const std::string& fname, const std::string& md) : filename(fname), mode(md), isOpen(false) {
                // Open the file based on the mode
                std::ios::openmode openMode = std::ios::in; // default read mode
                
                if (md.find('w') != std::string::npos) openMode = std::ios::out;
                else if (md.find('a') != std::string::npos) openMode = std::ios::app;
                if (md.find('+') != std::string::npos) openMode |= std::ios::in | std::ios::out;
                
                stream = new std::fstream(fname, openMode);
                isOpen = stream->is_open();
            }
            
            ~FileHandle() {
                if (stream && isOpen) {
                    stream->close();
                }
                delete stream;
            }
        };

        // Virtual machine class
        class VirtualMachine {
        private:
            MachineState state;
            DebugState debugState;       // Debug state
            std::unordered_map<std::string, std::function<Value(std::vector<Value>)>> builtInFunctions;
            std::unique_ptr<VMGarbageCollector> gc;
            std::unique_ptr<JITCompiler> jitCompiler;
            bool useJIT;
            
            // File operation support
            std::unordered_map<int64_t, FileHandle*> fileHandles; // Global file handle storage
            int64_t nextFileHandleId;
            
            // Pointer memory management
            std::unordered_map<int64_t, ManagedObject*> managedObjects; // Managed objects for pointers
            int64_t nextObjectId;

        public:
            VirtualMachine();
            ~VirtualMachine();

            // Load program
            bool loadProgram(const std::string& filename);

            // Execute program
            bool execute();

            // Debug execution methods
            bool executeDebug();         // Execute in debug mode
            void setDebugging(bool enabled); // Enable/disable debugging mode

            // Debug control methods
            void step();                 // Step one instruction
            void stepOver();             // Step over function calls
            void stepInto();             // Step into function calls
            void stepOut();              // Step out of current function
            void continueExecution();    // Continue execution until next breakpoint

            // Breakpoint management
            void addBreakpoint(int line, size_t pc);                    // Add breakpoint at line/PC
            void addBreakpoint(int line, size_t pc, const std::string& condition); // Add conditional breakpoint
            void removeBreakpoint(int line);                            // Remove breakpoint at line
            void removeBreakpointByPC(size_t pc);                       // Remove breakpoint by PC
            void enableBreakpoint(int line);                            // Enable breakpoint
            void disableBreakpoint(int line);                           // Disable breakpoint

            // Reset virtual machine
            void reset();

            // Get machine state
            const MachineState& getState() const { return state; }

            // Get debug state
            const DebugState& getDebugState() const { return debugState; }

            // Run garbage collection manually
            void runGarbageCollection();

        private:
            // Register built-in functions
            void registerBuiltInFunctions();

            // Parse source code
            std::vector<Instruction> parseSource(const std::string& source);

            // Parse IR code (implemented in vm.cpp)
            std::vector<Instruction> parseIR(const std::string& irCode);

            // Check if JIT compilation is possible (implemented in vm.cpp)
            bool canJITCompile();

            // Execute single instruction
            bool executeInstruction(size_t index);

            // Execute single instruction with debug support
            bool executeDebugInstruction(size_t index);

            // Decode and execute instruction
            bool decodeAndExecute(const Instruction& instr);

            // Check if execution should pause for debugging
            bool shouldPauseAt(size_t pc, int line = -1);

            // Check if condition is met for conditional breakpoint
            bool checkBreakpointCondition(const Breakpoint& bp);

            // Handle debug commands
            bool handleDebugCommand();

            // Find label
            int findLabel(int start, int targetId);

            // JIT compile and execute
            bool executeWithJIT();
            
            // Helper function declarations
            bool getBoolValue(const Value& value);
            void jumpToElseOrEnd();
            void jumpToEnd();
            Value performBinaryOperation(const Value& left, const Value& right, const std::string& op, int line);
            Value performUnaryOperation(const Value& operand, const std::string& op, int line);
            double getDoubleValue(const Value& value);
            void* allocateMemory(size_t size);
            void deallocateMemory(void* ptr);
            Value getVariable(const std::string& name);
            void setVariable(const std::string& name, const Value& value);
            void printStack();
            size_t getPC() const;
            void setPC(size_t pc);
            bool isRunning() const;
            int64_t getInt64Value(const Value& value);
        };

    } // namespace VM
} // namespace steve

#endif // STEVE_VM_H