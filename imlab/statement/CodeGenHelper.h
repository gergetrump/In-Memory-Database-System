#ifndef CODEGENHELPER_H
#define CODEGENHELPER_H

#include "imlab/runtime/RuntimeException.h"
#include "imlab/statement/CodeGenConfig.h"

#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <string>

class CodeGenHelper {
public:
    explicit CodeGenHelper(const std::string& code)
        : code_(code), tempFileName_("/tmp/code.cc"), sharedLibName_("/tmp/code.so"), handle_(nullptr) {}

    void compile() {
        writeToFile();
        const std::string compileCommand =
            std::string(IMLAB_CXX_COMPILER) +
            " -shared -fPIC -std=c++20 " +
            IMLAB_CODEGEN_CXX_FLAGS +
            " -o \"" + sharedLibName_ + "\" \"" + tempFileName_ + "\"" +
            " -I\"" + std::string(IMLAB_SOURCE_DIR) + "\"" +
            " -L\"" + std::string(IMLAB_LIBRARY_DIR) + "\"" +
            " -limlab " +
            IMLAB_CODEGEN_LINK_FLAGS;
        const int compileResult = system(compileCommand.c_str());
        if (compileResult != 0) {
            throw imlab::RuntimeException("Compilation failed");
        }
    }

    void* loadFunction() {
        handle_ = dlopen(sharedLibName_.c_str(), RTLD_NOW);
        if (!handle_) {
            throw imlab::RuntimeException("Failed to load shared library: " + std::string(dlerror()));
        }
        void* func = dlsym(handle_, "execute");
        if (!func) {
            dlclose(handle_);
            throw imlab::RuntimeException("Failed to load function 'execute': " + std::string(dlerror()));
        }
        return func;
    }


    ~CodeGenHelper() {
        if (handle_) {
            dlclose(handle_);
        }
    }

private:
    void writeToFile() {
        std::ofstream outFile(tempFileName_);
        if (!outFile) {
            throw imlab::RuntimeException("Failed to open file for writing: " + tempFileName_);
        }
        outFile << code_;
        outFile.close();
    }

    std::string code_;
    std::string tempFileName_;
    std::string sharedLibName_;
    void* handle_;
};



#endif //CODEGENHELPER_H
