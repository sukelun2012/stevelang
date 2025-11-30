/*
 * Copyright (c) 2024 Kekun Su(苏科纶).
 * 
 * Refer to the LICENSE file for full license information.
 * SPDX-License-Identifier: MIT
 */

#include "vm.h"
#include "language.h"
#include "vm_gc.h"
#include "vm_exception.h"
#include "vm_jit.h"
#include "mem.h"
#include "gc.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>
#include <set>
#include <variant>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <stack>

namespace steve {
    namespace VM {

        VirtualMachine::VirtualMachine() {

            // Initialize state

            state.pc = 0;

            state.running = false;

            state.rax = 0;

            state.rbx = 0;

            state.rcx = 0;

            state.rdx = 0;



            // Initialize scope

            state.scopes.push_back({});


            // Initialize debug state
            debugState.debugging = false;
            debugState.pendingCommand = DebugCommand::NONE;
            debugState.stepOverTarget = 0;
            debugState.isStepping = false;
            debugState.currentCallDepth = 0;


            // Initialize garbage collector

            gc = std::make_unique<VMGarbageCollector>();


            // Initialize JIT compiler

            jitCompiler = std::make_unique<JITCompiler>();

            useJIT = false; // Default is not to use JIT
            

            // Initialize file operation support
            nextFileHandleId = 1000; // Start with high numbers to avoid conflicts


            // Initialize pointer memory management
            nextObjectId = 1;


            // Register built-in functions

            registerBuiltInFunctions();

        }

        VirtualMachine::~VirtualMachine() {

            // Ensure garbage collection runs during destruction

            runGarbageCollection();
            
            // Clean up all open file handles
            for (auto& pair : fileHandles) {
                delete pair.second;
            }
            fileHandles.clear();
            
            // Clean up all managed objects
            for (auto& pair : managedObjects) {
                delete pair.second;
            }
            managedObjects.clear();

        }

        void VirtualMachine::registerBuiltInFunctions() {

            // Register print function

            builtInFunctions["print"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty()) {

                    if (std::holds_alternative<std::string>(args[0])) {

                        std::cout << std::get<std::string>(args[0]);

                    }

                    else if (std::holds_alternative<int>(args[0])) {

                        std::cout << std::get<int>(args[0]);

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        std::cout << std::get<double>(args[0]);

                    }

                    else if (std::holds_alternative<bool>(args[0])) {

                        std::string boolStr = std::get<bool>(args[0]) ? "true" : "false";

                        std::cout << boolStr;

                    }

                    else if (std::holds_alternative<std::nullptr_t>(args[0])) {

                        std::cout << "null";

                    }

                }

                std::cout << std::endl;

                return Value(nullptr);

                };



            // Register input function

            builtInFunctions["input"] = [this](std::vector<Value> args) -> Value {

                std::string input;

                std::getline(std::cin, input);

                return Value(input);

                };



            // Type conversion function - int

            builtInFunctions["int"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty()) {

                    if (std::holds_alternative<std::string>(args[0])) {

                        try {

                            return Value(std::stoi(std::get<std::string>(args[0])));

                        }

                        catch (...) {

                            return Value(0);

                        }

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        return Value(static_cast<int>(std::get<double>(args[0])));

                    }

                    else if (std::holds_alternative<int64_t>(args[0])) {

                        return Value(static_cast<int>(std::get<int64_t>(args[0])));

                    }

                    else if (std::holds_alternative<bool>(args[0])) {

                        return Value(static_cast<int>(std::get<bool>(args[0])));

                    }

                    else if (std::holds_alternative<int>(args[0])) {

                        return args[0];

                    }

                }

                return Value(0);

                };



            // Type conversion function - float

            builtInFunctions["float"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty()) {

                    if (std::holds_alternative<std::string>(args[0])) {

                        try {

                            return Value(std::stod(std::get<std::string>(args[0])));

                        }

                        catch (...) {

                            return Value(0.0);

                        }

                    }

                    else if (std::holds_alternative<int>(args[0])) {

                        return Value(static_cast<double>(std::get<int>(args[0])));

                    }

                    else if (std::holds_alternative<int64_t>(args[0])) {

                        return Value(static_cast<double>(std::get<int64_t>(args[0])));

                    }

                    else if (std::holds_alternative<bool>(args[0])) {

                        return Value(static_cast<double>(std::get<bool>(args[0])));

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        return args[0];

                    }

                }

                return Value(0.0);

                };



            // Type conversion function - string

            builtInFunctions["string"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty()) {

                    if (std::holds_alternative<int>(args[0])) {

                        return Value(std::to_string(std::get<int>(args[0])));

                    }

                    else if (std::holds_alternative<int64_t>(args[0])) {

                        return Value(std::to_string(std::get<int64_t>(args[0])));

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        return Value(std::to_string(std::get<double>(args[0])));

                    }

                    else if (std::holds_alternative<bool>(args[0])) {

                        return Value(std::get<bool>(args[0]) ? "true" : "false");

                    }

                    else if (std::holds_alternative<std::string>(args[0])) {

                        return args[0];

                    }

                    else if (std::holds_alternative<std::nullptr_t>(args[0])) {

                        return Value("null");

                    }

                }

                return Value(std::string(""));

                };



            // Type conversion function - bool

            builtInFunctions["bool"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty()) {

                    if (std::holds_alternative<int>(args[0])) {

                        return Value(std::get<int>(args[0]) != 0);

                    }

                    else if (std::holds_alternative<int64_t>(args[0])) {

                        return Value(std::get<int64_t>(args[0]) != 0);

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        return Value(std::get<double>(args[0]) != 0.0);

                    }

                    else if (std::holds_alternative<std::string>(args[0])) {

                        std::string s = std::get<std::string>(args[0]);

                        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

                        return Value(s != "false" && s != "0" && !s.empty());

                    }

                    else if (std::holds_alternative<bool>(args[0])) {

                        return args[0];

                    }

                    else if (std::holds_alternative<std::nullptr_t>(args[0])) {

                        return Value(false);

                    }

                }

                return Value(false);

                };

            // Pointer related functions

            // new function - creates a new object in memory and returns a pointer
            builtInFunctions["new"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    // For simplicity, we'll create a mock pointer value
                    // In a real implementation, this would allocate memory for the specific type
                    ManagedObject* obj = new ManagedObject(nullptr, "object", sizeof(int)); // Create a managed object
                    PointerValue ptr(obj, "object", false); // Placeholder for now
                    // Add to managed objects for GC
                    int64_t objId = nextObjectId++;
                    managedObjects[objId] = obj;
                    return Value(ptr);
                }
                return Value(PointerValue());
            };

            // type function - returns the type of a variable
            builtInFunctions["type"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    if (std::holds_alternative<int>(args[0])) {
                        return Value(std::string("int"));
                    } else if (std::holds_alternative<double>(args[0])) {
                        return Value(std::string("float")); // Or double
                    } else if (std::holds_alternative<std::string>(args[0])) {
                        return Value(std::string("string"));
                    } else if (std::holds_alternative<bool>(args[0])) {
                        return Value(std::string("bool"));
                    } else if (std::holds_alternative<std::nullptr_t>(args[0])) {
                        return Value(std::string("null"));
                    } else if (std::holds_alternative<int64_t>(args[0])) {
                        return Value(std::string("long"));
                    } else if (std::holds_alternative<PointerValue>(args[0])) {
                        PointerValue ptr = std::get<PointerValue>(args[0]);
                        return Value(ptr.type);
                    }
                }
                return Value(std::string("unknown"));
            };

            // hash function
            builtInFunctions["hash"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    if (std::holds_alternative<std::string>(args[0])) {
                        std::string str = std::get<std::string>(args[0]);
                        return Value(static_cast<int64_t>(std::hash<std::string>{}(str)));
                    } else if (std::holds_alternative<int>(args[0])) {
                        return Value(static_cast<int64_t>(std::hash<int>{}(std::get<int>(args[0]))));
                    } else if (std::holds_alternative<double>(args[0])) {
                        return Value(static_cast<int64_t>(std::hash<double>{}(std::get<double>(args[0]))));
                    } else {
                        // For other types, convert to string and hash
                        std::ostringstream ss;
                        ss << args[0].index(); // Use variant index as a simple approach
                        return Value(static_cast<int64_t>(std::hash<std::string>{}(ss.str())));
                    }
                }
                return Value(static_cast<int64_t>(0));
            };

            // bs function (binary string conversion)
            builtInFunctions["bs"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    if (std::holds_alternative<int>(args[0])) {
                        int val = std::get<int>(args[0]);
                        return Value(static_cast<long long>(val));
                    } else if (std::holds_alternative<int64_t>(args[0])) {
                        int64_t val = std::get<int64_t>(args[0]);
                        return Value(val);
                    } else {
                        return Value(std::string("0"));
                    }
                }
                return Value(std::string("0"));
            };

            // run function (for executing external files)
            builtInFunctions["run"] = [this](std::vector<Value> args) -> Value {
                // For now, this is a placeholder implementation
                // In a real implementation, this would execute external files
                std::cout << "Run function called (not fully implemented)" << std::endl;
                return Value(0);
            };

            // open function for file operations
            builtInFunctions["open"] = [this](std::vector<Value> args) -> Value {
                if (args.size() >= 2) {
                    if (std::holds_alternative<std::string>(args[0]) && std::holds_alternative<std::string>(args[1])) {
                        std::string filename = std::get<std::string>(args[0]);
                        std::string mode = std::get<std::string>(args[1]);
                        
                        FileHandle* handle = new FileHandle(filename, mode);
                        if (handle->isOpen) {
                            int64_t handleId = nextFileHandleId++;
                            fileHandles[handleId] = handle;
                            // Return a pointer-like value representing the file handle
                            ManagedObject* obj = new ManagedObject(reinterpret_cast<void*>(handleId), "file", sizeof(int64_t));
                            PointerValue ptr(obj, "file", false);
                            // Add to managed objects for GC
                            managedObjects[handleId] = obj;
                            return Value(ptr);
                        } else {
                            delete handle; // Clean up if failed to open
                            std::cerr << "Error: Could not open file: " << filename << std::endl;
                            return Value(PointerValue());
                        }
                    }
                } else if (!args.empty() && std::holds_alternative<std::string>(args[0])) {
                    std::string filename = std::get<std::string>(args[0]);
                    std::string defaultMode = "r"; // Default read mode
                    
                    FileHandle* handle = new FileHandle(filename, defaultMode);
                    if (handle->isOpen) {
                        int64_t handleId = nextFileHandleId++;
                        fileHandles[handleId] = handle;
                        ManagedObject* obj = new ManagedObject(reinterpret_cast<void*>(handleId), "file", sizeof(int64_t));
                        PointerValue ptr(obj, "file", false);
                        // Add to managed objects for GC
                        managedObjects[handleId] = obj;
                        return Value(ptr);
                    } else {
                        delete handle; // Clean up if failed to open
                        std::cerr << "Error: Could not open file: " << filename << std::endl;
                        return Value(PointerValue());
                    }
                }
                return Value(PointerValue());
            };

            // close function for file operations
            builtInFunctions["close"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty() && std::holds_alternative<PointerValue>(args[0])) {
                    PointerValue ptrVal = std::get<PointerValue>(args[0]);
                    if (!ptrVal.isNull) {
                        int64_t handleId = reinterpret_cast<int64_t>(ptrVal.ptr);
                        auto it = fileHandles.find(handleId);
                        if (it != fileHandles.end()) {
                            FileHandle* handle = it->second;
                            if (handle->stream && handle->isOpen) {
                                handle->stream->close();
                                handle->isOpen = false;
                            }
                            fileHandles.erase(it); // Remove from handle map
                            // Remove from managed objects
                            for (auto objIt = managedObjects.begin(); objIt != managedObjects.end(); ++objIt) {
                                if (objIt->second->data == reinterpret_cast<void*>(handleId)) {
                                    delete objIt->second;
                                    managedObjects.erase(objIt);
                                    break;
                                }
                            }
                            delete handle; // Clean up the handle
                            return Value(0); // Success
                        } else {
                            std::cerr << "Error: Invalid file handle" << std::endl;
                            return Value(-1); // Error code
                        }
                    }
                }
                std::cerr << "Error: Cannot close null file handle" << std::endl;
                return Value(-1); // Error code
            };

            // Additional file operations
            // write function for file operations
            builtInFunctions["write"] = [this](std::vector<Value> args) -> Value {
                if (args.size() >= 2 && std::holds_alternative<PointerValue>(args[0])) {
                    PointerValue ptrVal = std::get<PointerValue>(args[0]);
                    if (!ptrVal.isNull) {
                        int64_t handleId = reinterpret_cast<int64_t>(ptrVal.ptr);
                        auto it = fileHandles.find(handleId);
                        if (it != fileHandles.end()) {
                            FileHandle* handle = it->second;
                            if (handle->stream && handle->isOpen) {
                                std::string content;
                                if (std::holds_alternative<std::string>(args[1])) {
                                    content = std::get<std::string>(args[1]);
                                } else {
                                    // Convert other types to string
                                    std::ostringstream ss;
                                    ss << args[1].index(); // A simple conversion for now
                                    content = ss.str();
                                }
                                
                                *handle->stream << content;
                                handle->stream->flush(); // Ensure data is written
                                return Value(static_cast<int>(content.length())); // Return number of chars written
                            } else {
                                std::cerr << "Error: File not open for writing" << std::endl;
                                return Value(-1);
                            }
                        }
                    }
                }
                std::cerr << "Error: Invalid file handle for write" << std::endl;
                return Value(-1);
            };

            // read function for file operations
            builtInFunctions["read"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty() && std::holds_alternative<PointerValue>(args[0])) {
                    PointerValue ptrVal = std::get<PointerValue>(args[0]);
                    if (!ptrVal.isNull) {
                        int64_t handleId = reinterpret_cast<int64_t>(ptrVal.ptr);
                        auto it = fileHandles.find(handleId);
                        if (it != fileHandles.end()) {
                            FileHandle* handle = it->second;
                            if (handle->stream && handle->isOpen) {
                                // Read the entire file content
                                std::ostringstream ss;
                                ss << handle->stream->rdbuf();
                                return Value(ss.str());
                            } else {
                                std::cerr << "Error: File not open for reading" << std::endl;
                                return Value(std::string(""));
                            }
                        }
                    }
                }
                std::cerr << "Error: Invalid file handle for read" << std::endl;
                return Value(std::string(""));
            };

            // throw function for exception handling
            builtInFunctions["throw"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    std::string exceptionMsg;
                    if (std::holds_alternative<std::string>(args[0])) {
                        exceptionMsg = std::get<std::string>(args[0]);
                    } else {
                        // Convert other types to string
                        std::ostringstream ss;
                        ss << args[0].index(); // A simple conversion for now
                        exceptionMsg = ss.str();
                    }
                    
                    // Throw an exception with the provided message
                    throw RuntimeError(exceptionMsg);
                } else {
                    // If no argument provided, throw a generic exception
                    throw RuntimeError("Exception thrown");
                }
            };



            // Math function - abs

            builtInFunctions["abs"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty()) {

                    if (std::holds_alternative<int>(args[0])) {

                        return Value(std::abs(std::get<int>(args[0])));

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        return Value(std::abs(std::get<double>(args[0])));

                    }

                    else if (std::holds_alternative<int64_t>(args[0])) {

                        return Value(std::llabs(std::get<int64_t>(args[0])));

                    }

                }

                return Value(0);

                };



            // Math function - pow

            builtInFunctions["pow"] = [this](std::vector<Value> args) -> Value {

                if (args.size() >= 2) {

                    double base = 0.0, exponent = 0.0;



                    // Get base

                    if (std::holds_alternative<int>(args[0])) {

                        base = std::get<int>(args[0]);

                    }

                    else if (std::holds_alternative<double>(args[0])) {

                        base = std::get<double>(args[0]);

                    }

                    else if (std::holds_alternative<int64_t>(args[0])) {

                        base = std::get<int64_t>(args[0]);

                    }



                    // Get exponent

                    if (std::holds_alternative<int>(args[1])) {

                        exponent = std::get<int>(args[1]);

                    }

                    else if (std::holds_alternative<double>(args[1])) {

                        exponent = std::get<double>(args[1]);

                    }

                    else if (std::holds_alternative<int64_t>(args[1])) {

                        exponent = std::get<int64_t>(args[1]);

                    }



                    return Value(std::pow(base, exponent));

                }

                return Value(1.0);

                };



            // String function - len

            builtInFunctions["len"] = [this](std::vector<Value> args) -> Value {

                if (!args.empty() && std::holds_alternative<std::string>(args[0])) {

                    return Value(static_cast<int>(std::get<std::string>(args[0]).length()));

                }

                return Value(0);

                };



            // String function - substr

            builtInFunctions["substr"] = [this](std::vector<Value> args) -> Value {

                if (args.size() >= 2 && std::holds_alternative<std::string>(args[0])) {

                    std::string str = std::get<std::string>(args[0]);

                    int start = 0, length = static_cast<int>(str.length());



                    if (std::holds_alternative<int>(args[1])) {

                        start = std::get<int>(args[1]);

                    }



                    if (args.size() >= 3 && std::holds_alternative<int>(args[2])) {

                        length = std::get<int>(args[2]);

                    }



                    if (start < 0) start = 0;

                    if (start >= static_cast<int>(str.length())) return Value("");

                    if (length < 0) length = 0;

                    if (start + length > static_cast<int>(str.length())) {

                        length = static_cast<int>(str.length()) - start;

                    }



                    return Value(str.substr(start, length));

                }

                return Value("");

                };

            // List/Array function - append
            builtInFunctions["append"] = [this](std::vector<Value> args) -> Value {
                // This would typically be called as list.append(item) in actual implementation
                // For now, this is a placeholder - the actual implementation would need
                // the VM to support list objects directly
                return Value(0);
            };

            // Dictionary function - append (add key-value pair)
            builtInFunctions["dict_append"] = [this](std::vector<Value> args) -> Value {
                // This would typically be called as dict.append(key, value) in actual implementation
                return Value(0);
            };

            // Delete function for removing elements
            builtInFunctions["del"] = [this](std::vector<Value> args) -> Value {
                // Handle deletion of variables, list elements, dict entries, or pointers
                if (!args.empty()) {
                    if (std::holds_alternative<PointerValue>(args[0])) {
                        // Handle pointer deletion
                        PointerValue ptr = std::get<PointerValue>(args[0]);
                        if (!ptr.isNull) {
                            // Remove from managed objects map
                            for (auto it = managedObjects.begin(); it != managedObjects.end(); ++it) {
                                if (it->second == ptr.obj) {
                                    delete it->second;  // This also frees the data
                                    managedObjects.erase(it);
                                    break;
                                }
                            }
                            // Also delete the object if it's directly stored
                            if (ptr.obj) {
                                delete ptr.obj;
                            }
                            return Value(0); // Success
                        }
                    }
                    // For now, just return success for other types
                    return Value(0);
                }
                return Value(-1); // Error
            };

            // Built-in functions for list and dict operations
            
            // Create a new list
            builtInFunctions["list"] = [this](std::vector<Value> args) -> Value {
                std::vector<Value> items;
                // Add all arguments to the list
                for (const Value& arg : args) {
                    items.push_back(arg);
                }
                return Value(ListValue(items));
            };

            // Get length of list/dict
            builtInFunctions["len"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    if (std::holds_alternative<std::string>(args[0])) {
                        return Value(static_cast<int>(std::get<std::string>(args[0]).length()));
                    } else if (std::holds_alternative<ListValue>(args[0])) {
                        const ListValue& list = std::get<ListValue>(args[0]);
                        return Value(static_cast<int>(list.items.size()));
                    } else if (std::holds_alternative<DictValue>(args[0])) {
                        const DictValue& dict = std::get<DictValue>(args[0]);
                        return Value(static_cast<int>(dict.items.size()));
                    }
                }
                return Value(0);
            };
            
            // Append to list
            builtInFunctions["append"] = [this](std::vector<Value> args) -> Value {
                if (args.size() >= 2) {
                    // First argument should be the list, second is the item to add
                    if (std::holds_alternative<ListValue>(args[0])) {
                        ListValue list = std::get<ListValue>(args[0]);
                        Value item = args[1];
                        list.items.push_back(item);
                        return Value(list); // Return modified list
                    }
                }
                return args.empty() ? Value(0) : args[0]; // Return first argument if can't append
            };
            
            // Type checking function
            builtInFunctions["type"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    if (std::holds_alternative<int>(args[0])) {
                        return Value(std::string("int"));
                    } else if (std::holds_alternative<double>(args[0])) {
                        return Value(std::string("float")); // Or double
                    } else if (std::holds_alternative<std::string>(args[0])) {
                        return Value(std::string("string"));
                    } else if (std::holds_alternative<bool>(args[0])) {
                        return Value(std::string("bool"));
                    } else if (std::holds_alternative<std::nullptr_t>(args[0])) {
                        return Value(std::string("null"));
                    } else if (std::holds_alternative<int64_t>(args[0])) {
                        return Value(std::string("long"));
                    } else if (std::holds_alternative<PointerValue>(args[0])) {
                        PointerValue ptr = std::get<PointerValue>(args[0]);
                        return Value(ptr.type);
                    } else if (std::holds_alternative<ListValue>(args[0])) {
                        return Value(std::string("list"));
                    } else if (std::holds_alternative<DictValue>(args[0])) {
                        return Value(std::string("dict"));
                    }
                }
                return Value(std::string("unknown"));
            };
            
            // New function to create pointer objects
            builtInFunctions["new"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty()) {
                    // For now, we'll create a simple managed object based on the type
                    std::string requestedType;
                    if (std::holds_alternative<std::string>(args[0])) {
                        requestedType = std::get<std::string>(args[0]);
                    } else {
                        // If not a string, try to get the type of the value
                        std::ostringstream ss;
                        ss << args[0].index();
                        requestedType = ss.str();
                    }
                    
                    // Allocate memory for the object
                    size_t size = 8; // Default size
                    if (requestedType == "int" || requestedType == "bool") size = sizeof(int);
                    else if (requestedType == "float" || requestedType == "double") size = sizeof(double);
                    else if (requestedType == "string") size = sizeof(std::string);
                    
                    void* data = std::malloc(size);
                    if (data) {
                        // Initialize the data based on type
                        if (requestedType == "int") {
                            *static_cast<int*>(data) = 0;
                        } else if (requestedType == "float" || requestedType == "double") {
                            *static_cast<double*>(data) = 0.0;
                        } else if (requestedType == "bool") {
                            *static_cast<bool*>(data) = false;
                        } else {
                            // For other types, just zero out the memory
                            std::memset(data, 0, size);
                        }
                        
                        // Create a managed object
                        ManagedObject* obj = new ManagedObject(data, requestedType, size);
                        int64_t objId = nextObjectId++;
                        managedObjects[objId] = obj;
                        
                        // Create and return a pointer to the managed object
                        return Value(PointerValue(obj, requestedType, false, false));
                    }
                }
                // Return null pointer if failed
                return Value(PointerValue());
            };
            
            // Dereference function for pointers
            builtInFunctions["deref"] = [this](std::vector<Value> args) -> Value {
                if (!args.empty() && std::holds_alternative<PointerValue>(args[0])) {
                    PointerValue ptr = std::get<PointerValue>(args[0]);
                    if (!ptr.isNull && ptr.obj && ptr.obj->data) {
                        // Return a representation of the data
                        // In a real implementation, this would return the actual value
                        // For now, we'll return a string representation
                        std::ostringstream ss;
                        ss << "[ptr_data:" << ptr.type << "]";
                        return Value(ss.str());
                    }
                }
                return Value(std::string("null"));
            };

        }

        bool VirtualMachine::loadProgram(const std::string& filename) {

            std::ifstream file(filename);

            if (!file) {

                std::cerr << "Error: Cannot open file: " << filename << std::endl;

                return false;

            }



            std::stringstream buffer;



            buffer << file.rdbuf();



            std::string irCode = buffer.str();







            state.program = parseIR(irCode);

            return !state.program.empty();

        }

        std::vector<Instruction> VirtualMachine::parseIR(const std::string& irCode) {
            std::istringstream stream(irCode);
            std::string line;
            int lineNum = 0;
            std::vector<Instruction> instructions; // Local variable, not class member

            while (std::getline(stream, line)) {
                lineNum++;

                // Skip empty lines and comments
                if (line.empty() || line[0] == ';' || line.substr(0, 2) == ";;") {
                    continue;
                }

                // Check IR start and end markers
                if (line.find("# IR BEGIN") != std::string::npos || line.find("IR END") != std::string::npos) {
                    continue;
                }

                // Remove trailing comments
                size_t commentPos = line.find(';');
                if (commentPos != std::string::npos) {
                    line = line.substr(0, commentPos);
                }

                // Trim leading and trailing whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                if (line.empty()) continue;

                // Parse instruction
                std::istringstream lineStream(line);
                std::string instrName;
                lineStream >> instrName;

                if (instrName.empty()) continue;

                Instruction instr;
                instr.line = lineNum;

                // Set type based on instruction name

                if (instrName == "DEFVAR") {

                    instr.type = InstructionType::DEFVAR;

                }

                else if (instrName == "LOAD") {

                    instr.type = InstructionType::LOAD;

                }

                else if (instrName == "STORE") {

                    instr.type = InstructionType::STORE;

                }

                else if (instrName == "FUNC") {

                    instr.type = InstructionType::FUNC;

                }

                else if (instrName == "CALL") {

                    instr.type = InstructionType::CALL;

                }

                else if (instrName == "IF") {

                    instr.type = InstructionType::IF;

                }

                else if (instrName == "ELSE") {

                    instr.type = InstructionType::ELSE;

                }

                else if (instrName == "END") {

                    instr.type = InstructionType::END;

                }

                else if (instrName == "WHILE") {

                    instr.type = InstructionType::WHILE;

                }

                else if (instrName == "DO") {

                    instr.type = InstructionType::DO;

                }

                else if (instrName == "RETURN") {

                    instr.type = InstructionType::RETURN;

                }

                else if (instrName == "IMPORT") {

                    instr.type = InstructionType::IMPORT;

                }

                else if (instrName == "PRINT") {

                    instr.type = InstructionType::PRINT;

                }

                else if (instrName == "INPUT") {

                    instr.type = InstructionType::INPUT;

                }

                else if (instrName == "BINARY_OP") {

                    instr.type = InstructionType::BINARY_OP;

                }

                else if (instrName == "UNARY_OP") {

                    instr.type = InstructionType::UNARY_OP;

                }

                else if (instrName == "PUSH") {

                    instr.type = InstructionType::PUSH;

                }

                else if (instrName == "POP") {

                    instr.type = InstructionType::POP;

                }

                else if (instrName == "GOTO") {

                    instr.type = InstructionType::GOTO;

                }

                else if (instrName == "LABEL") {

                    instr.type = InstructionType::LABEL;

                }

                else if (instrName == "TRY") {

                    instr.type = InstructionType::TRY;

                }

                else if (instrName == "CATCH") {

                    instr.type = InstructionType::CATCH;

                }

                else if (instrName == "BREAK") {

                    instr.type = InstructionType::BREAK;

                }

                else if (instrName == "CONTINUE") {

                    instr.type = InstructionType::CONTINUE;

                }

                else if (instrName == "PASS") {

                    instr.type = InstructionType::PASS;

                }

                else if (instrName == "PACKAGE") {

                    instr.type = InstructionType::PACKAGE;

                }

                else if (instrName == "PTR_new") {

                    instr.type = InstructionType::PTR_NEW;

                }

                else if (instrName == "PTR_DEREF") {

                    instr.type = InstructionType::PTR_DEREF;

                }

                else if (instrName == "THROW") {

                    instr.type = InstructionType::THROW;

                }

                else if (instrName == "GC_new") {

                    instr.type = InstructionType::GC_NEW;

                }

                else if (instrName == "GC_delete") {

                    instr.type = InstructionType::GC_DELETE;

                }

                else if (instrName == "GC_gc") {

                    instr.type = InstructionType::GC_RUN;

                }

                else if (instrName == "MEM_malloc") {

                    instr.type = InstructionType::MEM_MALLOC;

                }

                else if (instrName == "MEM_free") {

                    instr.type = InstructionType::MEM_FREE;

                }

                else {

                    instr.type = InstructionType::NOP;

                }

                // Parse operands

                std::string operand;

                while (lineStream >> operand) {

                    // Handle quoted string operands

                    if (operand.length() >= 2 && operand.front() == '"' && operand.back() == '"') {

                        // Remove quotes

                        operand = operand.substr(1, operand.length() - 2);

                    }

                    else {

                        // Remove possible comma separators

                        if (!operand.empty() && operand.back() == ',') {

                            operand.pop_back();

                        }

                    }

                    

                    instr.operands.push_back(operand);

                }

                instructions.push_back(instr);
            }

            return instructions;
        }

        bool VirtualMachine::execute() {

            if (state.program.empty()) {

                std::cerr << "Internal Error: No program loaded" << std::endl;

                return false;

            }



            // Check if JIT compilation should be used

            if (useJIT && canJITCompile()) {

                try {

                    // Try to compile and execute

                    if (jitCompiler->compile(state.program)) {

                        int64_t result = jitCompiler->execute();

                        std::cout << "JIT execution result: " << result << std::endl;

                        return true;

                    }

                }

                catch (const std::exception& e) {

                    std::cerr << "JIT compilation failed: " << e.what() << ", falling back to interpreter" << std::endl;

                }

            }



            // Execute using interpreter

            state.pc = 0;

            state.running = true;



            try {

                while (state.running && state.pc < state.program.size()) {

                    if (!executeInstruction(state.pc)) {

                        return false;

                    }

                    state.pc++;

                }

            }

            catch (const VMException& e) {

                std::cerr << "VM Exception, PC " << state.pc << ": " << e.what() << std::endl;

                if (e.getLine() > 0) {

                    std::cerr << "  At line " << e.getLine() << std::endl;

                }

                return false;

            }

            catch (const std::exception& e) {

                std::cerr << "Standard Exception, PC " << state.pc << ": " << e.what() << std::endl;

                return false;

            }

            catch (...) {

                std::cerr << "Unknown Exception, PC " << state.pc << std::endl;

                return false;

            }



            return true;

        }

        bool VirtualMachine::canJITCompile() {

            // Simple check to determine if program is suitable for JIT compilation

            if (state.program.size() == 0) {

                return false;

            }



            // Check if it contains complex instructions

            for (const auto& instr : state.program) {

                if (instr.type == InstructionType::FUNC ||

                    instr.type == InstructionType::IF ||

                    instr.type == InstructionType::WHILE ||

                    instr.type == InstructionType::CALL ||

                    instr.type == InstructionType::GOTO) {

                    return false;

                }

            }



            return true;

        }

        bool VirtualMachine::executeInstruction(size_t index) {
            if (index >= state.program.size()) {
                return false;
            }

            const Instruction& instr = state.program[index];
            return decodeAndExecute(instr);
        }

        bool VirtualMachine::decodeAndExecute(const Instruction& instr) {
            try {
                switch (instr.type) {
                case InstructionType::DEFVAR: {

                    if (instr.operands.size() >= 1) {

                        std::string varName = instr.operands[0];

                        // Remove type annotation part (if exists)

                        if (varName.find(':') != std::string::npos) {

                            varName = varName.substr(0, varName.find(':'));

                        }

                        state.variables[varName] = Value(0); // Default value is 0

                    }

                    break;

                }

                case InstructionType::LOAD: {

                    if (instr.operands.size() >= 1) {

                        std::string operand = instr.operands[0];

                        // Check if it's a literal value or variable name

                        if (operand[0] == '"' && operand.back() == '"') {

                            // String literal

                            std::string strValue = operand.substr(1, operand.length() - 2);

                            state.stack.push_back(Value(strValue));

                        }

                        else if (operand == "true" || operand == "false") {

                            // Boolean value

                            state.stack.push_back(Value(operand == "true"));

                        }

                        else if (operand == "null") {

                            // Null value

                            state.stack.push_back(Value(nullptr));

                        }

                        else {

                            // Try to parse as number

                            try {

                                if (operand.find('.') != std::string::npos) {

                                    // Floating point number

                                    state.stack.push_back(Value(std::stod(operand)));

                                }

                                else {

                                    // Integer

                                    state.stack.push_back(Value(std::stoi(operand)));

                                }

                            }

                            catch (...) {

                                // Variable name

                                auto it = state.variables.find(operand);

                                if (it != state.variables.end()) {

                                    state.stack.push_back(it->second);

                                }

                                else {

                                    // Undefined variable defaults to 0

                                    state.stack.push_back(Value(0));

                                }

                            }

                        }

                    }

                    break;

                }

                case InstructionType::STORE: {

                    if (state.stack.empty()) {

                        throw AccessError("Stack underflow during STORE operation", instr.line);

                    }

                    if (instr.operands.size() < 1) {

                        throw AccessError("STORE operation missing variable name", instr.line);

                    }



                    Value val = state.stack.back();

                    state.stack.pop_back();



                    std::string varName = instr.operands[0];

                    state.variables[varName] = val;

                    break;

                }

                case InstructionType::FUNC: {

                    // Function definition, record function position in program

                    if (instr.operands.size() >= 1) {

                        std::string funcName = instr.operands[0];

                        state.functions[funcName] = state.pc;



                        // Create new scope

                        state.scopes.push_back({});

                    }

                    break;

                }

                case InstructionType::CALL: {

                    // Function call

                    if (instr.operands.size() >= 1) {

                        std::string funcName = instr.operands[0];



                        // Check if it's a built-in function

                        auto builtInIt = builtInFunctions.find(funcName);

                        if (builtInIt != builtInFunctions.end()) {

                            // Call built-in function

                            std::vector<Value> args;

                            // Get parameters from stack (if any)

                            if (!state.stack.empty()) {

                                args.push_back(state.stack.back());

                                state.stack.pop_back();

                            }

                            Value result = builtInIt->second(args);

                            state.stack.push_back(result);

                        }

                        else {

                            // Call user-defined function

                            auto funcIt = state.functions.find(funcName);

                            if (funcIt != state.functions.end()) {

                                // Save return address

                                state.stack.push_back(Value(static_cast<int>(state.pc)));

                                // Jump to function

                                state.pc = funcIt->second;

                            }

                            else {

                                throw RuntimeError("Undefined function: " + funcName, instr.line);

                            }

                        }

                    }

                    break;

                }

                            case InstructionType::IF: {

                if (state.stack.empty()) {

                    language::reportError("RuntimeError", "IF: Stack is empty", true);

                }

                Value condition = state.stack.back();

                state.stack.pop_back();

                bool condValue = getBoolValue(condition);

                if (!condValue) {

                    jumpToElseOrEnd();

                }

                break;

            }

                                case InstructionType::WHILE: {

                    // WHILE loop start

                    if (!state.stack.empty()) {

                        Value condition = state.stack.back();

                        state.stack.pop_back();



                        bool condValue = getBoolValue(condition);



                        if (!condValue) {

                            // Condition is false, jump to END

                            jumpToEnd();

                        }

                        else {

                            // Condition is true, record loop start position for later jump back

                            state.stack.push_back(Value(static_cast<int>(state.pc - 1)));

                        }

                    }

                    else {

                        throw AccessError("Stack is empty during WHILE operation", instr.line);

                    }

                    break;

                }

                            case InstructionType::ELSE: {

                jumpToEnd();

                break;

            }

                case InstructionType::END: {

                    // END instruction, check if it's loop end, if so jump back to loop start

                    if (!state.stack.empty() && std::holds_alternative<int>(state.stack.back())) {

                        int loopStart = std::get<int>(state.stack.back());

                        // Check if it's a reasonable loop start position

                        if (loopStart >= 0 && loopStart < static_cast<int>(state.program.size())) {

                            // Jump back to loop start to recheck condition

                            state.pc = loopStart;

                        }

                    }

                    break;

                }

                case InstructionType::DO: {

                    // DO is the start of WHILE loop body, no special handling needed

                    break;

                }

                case InstructionType::RETURN: {

                    // RETURN instruction, handle function return

                    // Return to caller

                    if (!state.stack.empty() && std::holds_alternative<int>(state.stack.back())) {

                        int returnAddr = std::get<int>(state.stack.back());

                        state.stack.pop_back();

                        state.pc = returnAddr;



                        // Restore scope

                        if (state.scopes.size() > 1) {

                            state.scopes.pop_back();

                        }

                    }

                    else {

                        // No return address, stop execution

                        state.running = false;

                    }

                    break;

                }

                case InstructionType::PRINT: {

                    // PRINT instruction

                    if (!state.stack.empty()) {

                        Value val = state.stack.back();

                        state.stack.pop_back();



                        if (std::holds_alternative<std::string>(val)) {

                            std::cout << std::get<std::string>(val);

                        }

                        else if (std::holds_alternative<int>(val)) {

                            std::cout << std::get<int>(val);

                        }

                        else if (std::holds_alternative<double>(val)) {

                            std::cout << std::get<double>(val);

                        }

                        else if (std::holds_alternative<bool>(val)) {

                            std::cout << (std::get<bool>(val) ? "true" : "false");

                        }

                        else if (std::holds_alternative<std::nullptr_t>(val)) {

                            std::cout << "null";

                        }

                        else if (std::holds_alternative<int64_t>(val)) {

                            std::cout << std::get<int64_t>(val);

                        }

                        std::cout << std::endl;

                    }

                    break;

                }

                case InstructionType::INPUT: {

                    // INPUT instruction

                    std::string input;

                    std::getline(std::cin, input);

                    state.stack.push_back(Value(input));

                    break;

                }

                case InstructionType::GC_NEW: {

                    // Garbage collection new operation - allocate a mock object

                    int64_t size = 1; // Default size

                    if (!state.stack.empty()) {

                        size = getInt64Value(state.stack.back());

                        state.stack.pop_back(); // Remove size parameter

                    }



                    // In actual implementation, this would allocate a garbage-collected managed object

                    // For now return a mock object reference

                    state.stack.push_back(Value(size)); // Return allocated size as reference

                    break;

                }

                case InstructionType::GC_DELETE: {

                    // Garbage collection delete operation

                    if (!state.stack.empty()) {

                        Value objRef = state.stack.back();

                        state.stack.pop_back(); // Remove object reference

                        // In actual implementation, this would mark the object as recyclable

                        // For now just ignore it

                    }

                    break;

                }

                case InstructionType::GC_RUN: {

                    // Run garbage collection

                    runGarbageCollection();

                    state.stack.push_back(Value(0)); // Return number of collected objects

                    break;

                }

                case InstructionType::MEM_MALLOC: {

                    // Memory allocation

                    if (!state.stack.empty()) {

                        Value sizeVal = state.stack.back();

                        state.stack.pop_back();



                        size_t size = 0;

                        if (std::holds_alternative<int>(sizeVal)) {

                            size = static_cast<size_t>(std::get<int>(sizeVal));

                        }

                        else if (std::holds_alternative<int64_t>(sizeVal)) {

                            size = static_cast<size_t>(std::get<int64_t>(sizeVal));

                        }



                        void* ptr = allocateMemory(size);

                        // Convert pointer to integer and store in stack

                        state.stack.push_back(Value(reinterpret_cast<int64_t>(ptr)));

                    }

                    break;

                }

                case InstructionType::MEM_FREE: {

                    // Memory deallocation

                    if (!state.stack.empty()) {

                        Value ptrVal = state.stack.back();

                        state.stack.pop_back();



                        void* ptr = nullptr;

                        if (std::holds_alternative<int64_t>(ptrVal)) {

                            ptr = reinterpret_cast<void*>(std::get<int64_t>(ptrVal));

                        }

                        else if (std::holds_alternative<int>(ptrVal)) {

                            ptr = reinterpret_cast<void*>(static_cast<int64_t>(std::get<int>(ptrVal)));

                        }



                        if (ptr) {

                            deallocateMemory(ptr);

                        }

                    }

                    break;

                }

                case InstructionType::BINARY_OP: {

                    // Binary operation

                    if (state.stack.size() < 2) {

                        throw AccessError("Stack underflow during BINARY_OP operation", instr.line);

                    }

                    if (instr.operands.size() < 1) {

                        throw AccessError("BINARY_OP operation missing operator", instr.line);

                    }



                    Value right = state.stack.back();

                    state.stack.pop_back();

                    Value left = state.stack.back();

                    state.stack.pop_back();



                    Value result = performBinaryOperation(left, right, instr.operands[0], instr.line);

                    state.stack.push_back(result);

                    break;

                }

                case InstructionType::UNARY_OP: {

                    // Unary operation

                    if (state.stack.empty()) {

                        throw AccessError("Stack underflow during UNARY_OP operation", instr.line);

                    }

                    if (instr.operands.size() < 1) {

                        throw AccessError("UNARY_OP operation missing operator", instr.line);

                    }



                    Value operand = state.stack.back();

                    state.stack.pop_back();



                    Value result = performUnaryOperation(operand, instr.operands[0], instr.line);

                    state.stack.push_back(result);

                    break;

                }

                case InstructionType::PUSH: {

                    // PUSH instruction

                    if (instr.operands.size() >= 1) {

                        std::string operand = instr.operands[0];

                        // Try to parse as number

                        try {

                            if (operand.find('.') != std::string::npos) {

                                state.stack.push_back(Value(std::stod(operand)));

                            }

                            else {

                                state.stack.push_back(Value(std::stoi(operand)));

                            }

                        }

                        catch (...) {

                            // If not a number, treat as string

                            state.stack.push_back(Value(operand));

                        }

                    }

                    break;

                }

                case InstructionType::POP: {

                    // POP instruction

                    if (!state.stack.empty()) {

                        state.stack.pop_back();

                    }

                    break;

                }

                case InstructionType::GOTO: {

                    // Jump instruction

                    if (instr.operands.size() >= 1) {

                        std::string label = instr.operands[0];

                        // Find label position

                        for (size_t i = 0; i < state.program.size(); i++) {

                            if (state.program[i].type == InstructionType::LABEL &&

                                state.program[i].operands.size() >= 1 &&

                                state.program[i].operands[0] == label) {

                                state.pc = i; // Jump directly to label position

                                return true;

                            }

                        }

                        throw RuntimeError("Undefined label: " + label, instr.line);

                    }

                    break;

                }

                case InstructionType::LABEL: {

                    // Label definition, no special handling needed

                    break;

                }

                case InstructionType::TRY: {

                    // Try statement - implement basic exception handling

                    // For now, just continue execution (no exception occurred)

                    break;

                }

                case InstructionType::CATCH: {

                    // Catch statement - handle exceptions

                    // In a full implementation, this would handle caught exceptions

                    break;

                }

                case InstructionType::BREAK: {

                    // Break statement - exit current loop

                    // This would require tracking loop contexts in a more sophisticated implementation

                    // For now, just break out of current context by finding matching END

                    break;

                }

                case InstructionType::CONTINUE: {

                    // Continue statement - skip to loop condition check

                    // This would require tracking loop contexts in a more sophisticated implementation

                    // For now, just continue to next iteration

                    break;

                }

                case InstructionType::PASS: {

                    // Pass statement - no operation

                    break;

                }

                case InstructionType::PACKAGE: {

                    // Package declaration - no runtime effect

                    if (instr.operands.size() >= 1) {

                        std::string packageName = instr.operands[0];

                        // Store package info if needed for module system

                    }

                    break;

                }

                case InstructionType::PTR_NEW: {

                    // Create new pointer - allocate memory for an object and return a pointer to it

                    if (!state.stack.empty()) {

                        Value sizeVal = state.stack.back();

                        state.stack.pop_back(); // Remove size parameter (for now, we'll ignore it)



                        // Create a new pointer value (for now, it's a placeholder)

                        PointerValue newPtr(nullptr, "object", false); // In real implementation, this would allocate memory

                        state.stack.push_back(Value(newPtr));

                    } else {

                        // Default: create a null pointer if no size specified

                        PointerValue nullPtr(nullptr, "object", false);

                        state.stack.push_back(Value(nullPtr));

                    }

                    break;

                }

                case InstructionType::PTR_DEREF: {

                    // Dereference pointer - get value from pointer address

                    if (!state.stack.empty()) {

                        Value top = state.stack.back();

                        state.stack.pop_back();
                        
                        if (std::holds_alternative<PointerValue>(top)) {

                            PointerValue ptr = std::get<PointerValue>(top);

                            if (!ptr.isNull) {

                                // In a real implementation, this would dereference the actual memory address

                                // For now, we'll just return a placeholder value

                                state.stack.push_back(Value(0)); // Placeholder for dereferenced value

                            } else {

                                throw RuntimeError("Cannot dereference null pointer", instr.line);

                            }

                        } else {

                            // If not a pointer, treat as error or return the value as-is

                            state.stack.push_back(top);

                        }

                    }

                    break;

                }

                case InstructionType::THROW: {

                    // Throw exception - get message from stack and throw it

                    if (!state.stack.empty()) {

                        Value exceptionValue = state.stack.back();

                        state.stack.pop_back();



                        std::string exceptionMsg;



                        if (std::holds_alternative<std::string>(exceptionValue)) {

                            exceptionMsg = std::get<std::string>(exceptionValue);

                        } else {

                            exceptionMsg = "Unknown exception occurred";

                        }



                        // Throw a RuntimeError exception with the message

                        throw RuntimeError(exceptionMsg, instr.line);

                    } else {

                        // If no value on stack, throw a generic exception

                        throw RuntimeError("Exception thrown", instr.line);

                    }

                    break;

                }

                case InstructionType::IMPORT: {

                    // Import instruction, simple handling for now

                    if (instr.operands.size() >= 1) {

                        std::string moduleName = instr.operands[0];

                        // In actual implementation, this should load and execute the module

                        // For now just simple logging

                        std::cout << "Importing module: " << moduleName << std::endl;

                    }

                    break;

                }

                case InstructionType::NOP: {

                    // No operation, do nothing

                    break;

                }

                default:

                    // Unknown instruction type

                    std::cerr << "Warning: Unknown instruction type at line " << instr.line << std::endl;

                    break;

                }
            }

            catch (const VMException& e) {

                throw; // Re-throw VM exception

            }

            catch (const std::exception& e) {

                throw RuntimeError(std::string("Standard exception: ") + e.what(), instr.line);

            }



            return true;

        }

        // Helper function: Get boolean value

        bool VirtualMachine::getBoolValue(const Value& value) {

            if (std::holds_alternative<int>(value)) {

                return std::get<int>(value) != 0;

            }

            else if (std::holds_alternative<int64_t>(value)) {

                return std::get<int64_t>(value) != 0;

            }

            else if (std::holds_alternative<double>(value)) {

                return std::get<double>(value) != 0.0;

            }

            else if (std::holds_alternative<bool>(value)) {

                return std::get<bool>(value);

            }

            else if (std::holds_alternative<std::string>(value)) {

                return !std::get<std::string>(value).empty();

            }

            else if (std::holds_alternative<std::nullptr_t>(value)) {

                return false;

            }

            else if (std::holds_alternative<ListValue>(value)) {

                const ListValue& list = std::get<ListValue>(value);

                return !list.items.empty(); // List is true if not empty

            }

            else if (std::holds_alternative<DictValue>(value)) {

                const DictValue& dict = std::get<DictValue>(value);

                return !dict.items.empty(); // Dict is true if not empty

            }

            return false;

        }

                // Helper function: Jump to ELSE or END

        void VirtualMachine::jumpToElseOrEnd() {

            int depth = 1;

            size_t current = state.pc + 1;

            while (current < state.program.size() && depth > 0) {

                const Instruction& nextInstr = state.program[current];

                if (nextInstr.type == InstructionType::IF || nextInstr.type == InstructionType::WHILE) {

                    depth++;

                }

                else if (nextInstr.type == InstructionType::END) {

                    depth--;

                }

                            else {

                throw TypeError("Unsupported unary operator: " + op, line);

            }

            // This line should not be reached due to exceptions above, but added for compiler safety
            return Value(0);
        }

        // Helper function: Perform binary operation

                    // Found same depth ELSE

                    state.pc = current;

                    return;

                }

                if (depth == 0) {

                    // Found matching END

                    state.pc = current;

                    return;

                }

                current++;

            }

            // If no matching END is found, jump to end of program

            state.pc = state.program.size() - 1;

        }

        // Helper function: Jump to END

        void VirtualMachine::jumpToEnd() {

            int depth = 1;

            size_t current = state.pc + 1;

            while (current < state.program.size() && depth > 0) {

                const Instruction& nextInstr = state.program[current];

                if (nextInstr.type == InstructionType::IF || nextInstr.type == InstructionType::WHILE) {

                    depth++;

                }

                else if (nextInstr.type == InstructionType::END) {

                    depth--;

                }

                if (depth == 0) {

                    state.pc = current;

                    return;

                }

                current++;

            }

            // If no matching END is found, jump to end of program

            state.pc = state.program.size() - 1;

        }

                // Helper function: Perform binary operation

        Value VirtualMachine::performBinaryOperation(const Value& left, const Value& right,

            const std::string& op, int line) {

            // Determine result type, try to convert to most appropriate type

            if (std::holds_alternative<double>(left) || std::holds_alternative<double>(right)) {

                // If either operand is double, convert to double calculation

                double leftVal = getDoubleValue(left);

                double rightVal = getDoubleValue(right);



                if (op == "+") return Value(leftVal + rightVal);

                else if (op == "-") return Value(leftVal - rightVal);

                else if (op == "*") return Value(leftVal * rightVal);

                else if (op == "/") {

                    if (rightVal == 0.0) {

                        throw RuntimeError("Division by zero error", line);

                    }

                    return Value(leftVal / rightVal);

                }

                else if (op == "==") return Value(leftVal == rightVal);

                else if (op == "!=") return Value(leftVal != rightVal);

                else if (op == "<") return Value(leftVal < rightVal);

                else if (op == ">") return Value(leftVal > rightVal);

                else if (op == "<=") return Value(leftVal <= rightVal);

                else if (op == ">=") return Value(leftVal >= rightVal);

                else if (op == "and" || op == "&&") return Value(static_cast<bool>(leftVal) && static_cast<bool>(rightVal));

                else if (op == "or" || op == "||") return Value(static_cast<bool>(leftVal) || static_cast<bool>(rightVal));

                else {

                    throw TypeError("Unsupported operator for floating point: " + op, line);

                }

            }

            else if ((std::holds_alternative<int>(left) || std::holds_alternative<int64_t>(left)) &&

                (std::holds_alternative<int>(right) || std::holds_alternative<int64_t>(right))) {

                // Integer arithmetic

                int64_t leftVal = getInt64Value(left);

                int64_t rightVal = getInt64Value(right);



                if (op == "+") return Value(leftVal + rightVal);

                else if (op == "-") return Value(leftVal - rightVal);

                else if (op == "*") return Value(leftVal * rightVal);

                else if (op == "/") {

                    if (rightVal == 0) {

                        throw RuntimeError("Division by zero error", line);

                    }

                    return Value(leftVal / rightVal);

                }

                else if (op == "%") {

                    if (rightVal == 0) {

                        throw RuntimeError("Modulo by zero error", line);

                    }

                    return Value(leftVal % rightVal);

                }

                else if (op == "==") return Value(leftVal == rightVal);
                else if (op == "!=") return Value(leftVal != rightVal);
                else if (op == "<") return Value(leftVal < rightVal);
                else if (op == ">") return Value(leftVal > rightVal);
                else if (op == "<=") return Value(leftVal <= rightVal);
                else if (op == ">=") return Value(leftVal >= rightVal);
                else if (op == "and" || op == "&&") return Value(leftVal != 0 && rightVal != 0);
                else if (op == "or" || op == "||") return Value(leftVal != 0 || rightVal != 0);
                else {

                    throw TypeError("Unsupported operator for integer: " + op, line);

                }

            }

            else if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {

                // String arithmetic

                if (op == "+") {

                    return Value(std::get<std::string>(left) + std::get<std::string>(right));

                }

                else if (op == "==") {

                    return Value(std::get<std::string>(left) == std::get<std::string>(right));

                }

                else if (op == "!=") {

                    return Value(std::get<std::string>(left) != std::get<std::string>(right));

                }

                else {

                    throw TypeError("Unsupported operator for string: " + op, line);

                }

            }

            else if (std::holds_alternative<PointerValue>(left) || std::holds_alternative<PointerValue>(right)) {

                // Pointer operations

                if (op == "==") {

                    if (std::holds_alternative<PointerValue>(left) && std::holds_alternative<PointerValue>(right)) {

                        const PointerValue& lptr = std::get<PointerValue>(left);

                        const PointerValue& rptr = std::get<PointerValue>(right);

                        return Value(lptr.ptr == rptr.ptr);

                    } else {

                        // Compare pointer to null

                        bool leftIsNull = std::holds_alternative<PointerValue>(left) ? std::get<PointerValue>(left).isNull : true;

                        bool rightIsNull = std::holds_alternative<PointerValue>(right) ? std::get<PointerValue>(right).isNull : true;

                        return Value(leftIsNull && rightIsNull); // Both are null, so equal

                    }

                }

                else if (op == "!=") {

                    if (std::holds_alternative<PointerValue>(left) && std::holds_alternative<PointerValue>(right)) {

                        const PointerValue& lptr = std::get<PointerValue>(left);

                        const PointerValue& rptr = std::get<PointerValue>(right);

                        return Value(lptr.ptr != rptr.ptr);

                    } else {

                        // Compare pointer to null

                        bool leftIsNull = std::holds_alternative<PointerValue>(left) ? std::get<PointerValue>(left).isNull : true;

                        bool rightIsNull = std::holds_alternative<PointerValue>(right) ? std::get<PointerValue>(right).isNull : true;

                        return Value(leftIsNull != rightIsNull); // Only one is null, so not equal

                    }

                }

                else if (op == "=") {

                    // Assignment of pointer to another pointer

                    if (std::holds_alternative<PointerValue>(right)) {

                        return right; // Return the right pointer value

                    }

                }

                throw TypeError("Unsupported operator for pointer: " + op, line);

            }

            else if (std::holds_alternative<ListValue>(left) && std::holds_alternative<ListValue>(right) && op == "+") {

                // List concatenation

                ListValue leftList = std::get<ListValue>(left);

                ListValue rightList = std::get<ListValue>(right);

                std::vector<Value> resultItems;

                

                // Add items from left list

                for (const Value& item : leftList.items) {

                    resultItems.push_back(item);

                }

                // Add items from right list

                for (const Value& item : rightList.items) {

                    resultItems.push_back(item);

                }

                

                return Value(ListValue(resultItems));

            }

            else if (std::holds_alternative<ListValue>(left) && op == "*") {

                // List repetition (list * number)

                if (std::holds_alternative<int>(right) || std::holds_alternative<int64_t>(right)) {

                    int64_t repetitions = std::holds_alternative<int>(right) ? std::get<int>(right) : std::get<int64_t>(right);

                    ListValue list = std::get<ListValue>(left);

                    std::vector<Value> resultItems;

                    

                    for (int64_t i = 0; i < repetitions; ++i) {

                        for (const Value& item : list.items) {

                            resultItems.push_back(item);

                        }

                    }

                    

                    return Value(ListValue(resultItems));

                }

            }

            else if (std::holds_alternative<DictValue>(left) && op == "==") {

                // Dictionary equality comparison

                if (std::holds_alternative<DictValue>(right)) {

                    const DictValue& leftDict = std::get<DictValue>(left);

                    const DictValue& rightDict = std::get<DictValue>(right);

                    

                    if (leftDict.items.size() != rightDict.items.size()) {

                        return Value(false);

                    }

                    

                    for (const auto& pair : leftDict.items) {

                        auto it = rightDict.items.find(pair.first);

                        if (it == rightDict.items.end() || it->second != pair.second) {

                            return Value(false);

                        }

                    }

                    

                    return Value(true);

                }

            }

            else {

                throw TypeError("Binary operation type mismatch", line);

            }

            // This line should not be reached due to exceptions above, but added for compiler safety
            return Value(0);
        }

        // Helper function: Perform unary operation

        Value VirtualMachine::performUnaryOperation(const Value& operand, const std::string& op, int line) {

            if (op == "-") {

                if (std::holds_alternative<int>(operand)) {

                    return Value(-std::get<int>(operand));

                }

                else if (std::holds_alternative<int64_t>(operand)) {

                    return Value(-std::get<int64_t>(operand));

                }

                else if (std::holds_alternative<double>(operand)) {

                    return Value(-std::get<double>(operand));

                }

                else {

                    throw TypeError("Invalid operand type for unary minus", line);

                }

            }

            else if (op == "!" || op == "not") {

                return Value(!getBoolValue(operand));

            }

            else {

                throw TypeError("Unsupported unary operator: " + op, line);

            }

        }

        // Helper function: Get double value

        double VirtualMachine::getDoubleValue(const Value& value) {

            if (std::holds_alternative<double>(value)) {

                return std::get<double>(value);

            }

            else if (std::holds_alternative<int>(value)) {

                return static_cast<double>(std::get<int>(value));

            }

            else if (std::holds_alternative<int64_t>(value)) {

                return static_cast<double>(std::get<int64_t>(value));

            }

            else if (std::holds_alternative<bool>(value)) {

                return static_cast<double>(std::get<bool>(value));

            }

            else if (std::holds_alternative<ListValue>(value)) {

                const ListValue& list = std::get<ListValue>(value);

                return static_cast<double>(list.items.size()); // Return list length as double

            }

            else if (std::holds_alternative<DictValue>(value)) {

                const DictValue& dict = std::get<DictValue>(value);

                return static_cast<double>(dict.items.size()); // Return dict size as double

            }

            return 0.0;

        }

                // Helper function: Get int64_t value

        int64_t VirtualMachine::getInt64Value(const Value& value) {

            if (std::holds_alternative<int64_t>(value)) {

                return std::get<int64_t>(value);

            }

            else if (std::holds_alternative<int>(value)) {

                return static_cast<int64_t>(std::get<int>(value));

            }

            else if (std::holds_alternative<bool>(value)) {

                return static_cast<int64_t>(std::get<bool>(value));

            }

            else if (std::holds_alternative<PointerValue>(value)) {
                // Return some identifier for the pointer, for now just a hash of the pointer address
                const PointerValue& ptr = std::get<PointerValue>(value);
                return reinterpret_cast<int64_t>(ptr.ptr);
            }

            else if (std::holds_alternative<ListValue>(value)) {
                const ListValue& list = std::get<ListValue>(value);
                return static_cast<int64_t>(list.items.size()); // Return list length as int64_t
            }

            else if (std::holds_alternative<DictValue>(value)) {
                const DictValue& dict = std::get<DictValue>(value);
                return static_cast<int64_t>(dict.items.size()); // Return dict size as int64_t
            }

            return 0;

        }

        void* VirtualMachine::allocateMemory(size_t size) {
            return steve::malloc(size);
        }

        void VirtualMachine::deallocateMemory(void* ptr) {
            if (ptr) {
                steve::free(ptr);
            }
        }

        void VirtualMachine::runGarbageCollection() {

            // Call the virtual machine's garbage collector

            if (gc) {

                gc->collect();

            }

            // Also call Steve's garbage collector

            steve::gc.collect();

        }

        Value VirtualMachine::getVariable(const std::string& name) {
            auto it = state.variables.find(name);
            if (it != state.variables.end()) {
                return it->second;
            }
            return Value(0);
        }

        void VirtualMachine::setVariable(const std::string& name, const Value& value) {
            state.variables[name] = value;
        }

        void VirtualMachine::printStack() {
            std::cout << "Stack (" << state.stack.size() << " elements): ";
            for (size_t i = 0; i < state.stack.size(); ++i) {
                if (std::holds_alternative<int>(state.stack[i])) {
                    std::cout << std::get<int>(state.stack[i]) << " ";
                }
                else if (std::holds_alternative<int64_t>(state.stack[i])) {
                    std::cout << std::get<int64_t>(state.stack[i]) << " ";
                }
                else if (std::holds_alternative<double>(state.stack[i])) {
                    std::cout << std::get<double>(state.stack[i]) << " ";
                }
                else if (std::holds_alternative<std::string>(state.stack[i])) {
                    std::cout << """ << std::get<std::string>(state.stack[i]) << "" ";
                }
                else if (std::holds_alternative<bool>(state.stack[i])) {
                    std::cout << (std::get<bool>(state.stack[i]) ? "true" : "false") << " ";
                }
                else if (std::holds_alternative<std::nullptr_t>(state.stack[i])) {
                    std::cout << "null ";
                }
                else if (std::holds_alternative<PointerValue>(state.stack[i])) {
                    const PointerValue& ptr = std::get<PointerValue>(state.stack[i]);
                    if (ptr.isNull) {
                        std::cout << "null_ptr ";
                    } else {
                        std::cout << "ptr(" << ptr.type << ") ";
                    }
                }
                else if (std::holds_alternative<ListValue>(state.stack[i])) {
                    const ListValue& list = std::get<ListValue>(state.stack[i]);
                    std::cout << "[list:" << list.items.size() << "] ";
                }
                else if (std::holds_alternative<DictValue>(state.stack[i])) {
                    const DictValue& dict = std::get<DictValue>(state.stack[i]);
                    std::cout << "{dict:" << dict.items.size() << "} ";
                }
                else {
                    std::cout << "unknown ";
                }
            }
            std::cout << std::endl;
        }

        size_t VirtualMachine::getPC() const {
            return state.pc;
        }

        void VirtualMachine::setPC(size_t pc) {
            state.pc = pc;
        }

        bool VirtualMachine::isRunning() const {
            return state.running;
        }

        void VirtualMachine::reset() {
            // Reset the virtual machine to its initial state
            state.pc = 0;
            state.running = false;
            state.rax = 0;
            state.rbx = 0;
            state.rcx = 0;
            state.rdx = 0;
            
            // Clear all scopes and create a new one
            state.scopes.clear();
            state.scopes.push_back({});
            
            // Clear the stack
            state.stack.clear();
            
            // Clear variables and functions maps
            state.variables.clear();
            state.functions.clear();
            
            // Clear the program
            state.program.clear();
            
            // Re-register built-in functions
            registerBuiltInFunctions();
        }

        std::vector<Instruction> VirtualMachine::parseSource(const std::string& source) {
            // This method is currently not used as we're parsing IR directly
            // But implementing it for completeness
            std::vector<Instruction> instructions;
            
            // For now, return empty vector
            // In a full implementation, this would parse source code to IR
            return instructions;
        }

        int VirtualMachine::findLabel(int start, int targetId) {
            // Find a label with the given target ID starting from the start position
            for (size_t i = start; i < state.program.size(); i++) {
                if (state.program[i].type == InstructionType::LABEL) {
                    // In our implementation, labels are identified by their operand
                    if (state.program[i].operands.size() > 0) {
                        // Try to convert the operand to an integer ID
                        try {
                            int labelId = std::stoi(state.program[i].operands[0]);
                            if (labelId == targetId) {
                                return static_cast<int>(i);
                            }
                        } catch (...) {
                            // If conversion fails, continue searching
                            continue;
                        }
                    }
                }
            }
            return -1; // Not found
        }

        bool VirtualMachine::executeWithJIT() {
            // Execute using JIT compilation
            if (!useJIT || !jitCompiler) {
                return false;
            }

            try {
                // Try to compile and execute
                if (jitCompiler->compile(state.program)) {
                    int64_t result = jitCompiler->execute();
                    std::cout << "JIT execution result: " << result << std::endl;
                    return true;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "JIT execution failed: " << e.what() << std::endl;
                // Fall back to interpreter
                return execute();
            }

            return false;
        }

        // Debug control methods implementation

        void VirtualMachine::setDebugging(bool enabled) {
            debugState.debugging = enabled;
        }

        void VirtualMachine::step() {
            debugState.pendingCommand = DebugCommand::STEP;
            debugState.isStepping = true;
        }

        void VirtualMachine::stepOver() {
            debugState.pendingCommand = DebugCommand::STEP_OVER;
            debugState.isStepping = true;
            // Set the step over target to the next instruction after the current call
            debugState.stepOverTarget = state.pc + 1;
        }

        void VirtualMachine::stepInto() {
            debugState.pendingCommand = DebugCommand::STEP_INTO;
            debugState.isStepping = true;
        }

        void VirtualMachine::stepOut() {
            debugState.pendingCommand = DebugCommand::STEP_OUT;
            debugState.isStepping = true;
            // Set target to return from current function depth
            // We'll need to track call depth properly
        }

        void VirtualMachine::continueExecution() {
            debugState.pendingCommand = DebugCommand::CONTINUE;
            debugState.isStepping = false;
        }

        void VirtualMachine::addBreakpoint(int line, size_t pc) {
            debugState.breakpoints.emplace_back(line, pc);
        }

        void VirtualMachine::addBreakpoint(int line, size_t pc, const std::string& condition) {
            debugState.breakpoints.emplace_back(line, pc, condition);
        }

        void VirtualMachine::removeBreakpoint(int line) {
            debugState.breakpoints.erase(
                std::remove_if(debugState.breakpoints.begin(), debugState.breakpoints.end(),
                    [line](const Breakpoint& bp) { return bp.line == line && !bp.temporary; }),
                debugState.breakpoints.end());
        }

        void VirtualMachine::removeBreakpointByPC(size_t pc) {
            debugState.breakpoints.erase(
                std::remove_if(debugState.breakpoints.begin(), debugState.breakpoints.end(),
                    [pc](const Breakpoint& bp) { return bp.pc == pc && !bp.temporary; }),
                debugState.breakpoints.end());
        }

        void VirtualMachine::enableBreakpoint(int line) {
            for (auto& bp : debugState.breakpoints) {
                if (bp.line == line) {
                    bp.enabled = true;
                }
            }
        }

        void VirtualMachine::disableBreakpoint(int line) {
            for (auto& bp : debugState.breakpoints) {
                if (bp.line == line) {
                    bp.enabled = false;
                }
            }
        }

        bool VirtualMachine::shouldPauseAt(size_t pc, int line) {
            // Check if there's an active breakpoint at this PC or line
            for (const auto& bp : debugState.breakpoints) {
                if (bp.enabled && (bp.pc == pc || (line != -1 && bp.line == line))) {
                    if (bp.condition.empty()) {
                        return true; // Unconditional breakpoint
                    } else if (checkBreakpointCondition(bp)) {
                        return true; // Conditional breakpoint met
                    }
                }
            }

            // Check debug commands
            if (debugState.isStepping) {
                if (debugState.pendingCommand == DebugCommand::STEP) {
                    return true; // Always pause when stepping
                } else if (debugState.pendingCommand == DebugCommand::STEP_OVER) {
                    // For step over, we pause only if we're not inside a function call
                    // (i.e., when we've returned to the stepOverTarget depth or beyond)
                    return false; // Actual step-over logic will be handled differently
                } else if (debugState.pendingCommand == DebugCommand::STEP_OUT) {
                    // For step out, we pause when we've returned from the current function
                    // This will be handled by tracking the call stack depth
                    return false; // Actual step-out logic will be handled differently
                }
            }

            return false;
        }

        bool VirtualMachine::checkBreakpointCondition(const Breakpoint& bp) {
            // For now, just return true (in a full implementation, this would evaluate the condition)
            // In a real implementation, we'd evaluate the condition against the current scope
            return true;
        }

        bool VirtualMachine::handleDebugCommand() {
            if (debugState.pendingCommand == DebugCommand::BREAK ||
                debugState.pendingCommand == DebugCommand::STEP ||
                shouldPauseAt(state.pc)) {
                
                // In a real debugger, this is where we'd pause and wait for user input
                std::cout << "DEBUGGER PAUSED at PC: " << state.pc << std::endl;
                
                // For now, just reset the debug command to continue execution
                debugState.pendingCommand = DebugCommand::NONE;
                debugState.isStepping = false;
                return true; // Indicate that we paused
            }

            return false;
        }

        bool VirtualMachine::executeDebugInstruction(size_t index) {
            if (index >= state.program.size()) {
                return false;
            }

            const Instruction& instr = state.program[index];
            
            // Check if we should pause before executing this instruction
            if (shouldPauseAt(index, instr.line)) {
                handleDebugCommand();
            }

            // Track function calls for step-into/step-over/step-out functionality
            if (instr.type == InstructionType::CALL) {
                debugState.callStack.push_back(index); // Record the call
                debugState.currentCallDepth++;
            } else if (instr.type == InstructionType::RETURN) {
                if (!debugState.callStack.empty()) {
                    debugState.callStack.pop_back(); // Remove the call
                    if (debugState.currentCallDepth > 0) {
                        debugState.currentCallDepth--;
                    }
                }
            }

            // Execute the instruction
            bool result = decodeAndExecute(instr);
            state.pc = index + 1;
            return result;
        }

        bool VirtualMachine::executeDebug() {
            if (debugState.breakpoints.empty() && !debugState.isStepping) {
                // If no breakpoints and not stepping, just execute normally
                return execute();
            }

            state.running = true;
            while (state.running && state.pc < state.program.size()) {
                if (!executeDebugInstruction(state.pc)) {
                    break;
                }

                // Allow for interruption by debug commands
                if (debugState.pendingCommand != DebugCommand::NONE && debugState.pendingCommand != DebugCommand::CONTINUE) {
                    handleDebugCommand();
                }
            }

            return !state.running;
        }

    } // namespace VM
} // namespace steve