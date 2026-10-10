#ifndef CODEGENHELPER_H
#define CODEGENHELPER_H

#include "imlab/runtime/RuntimeException.h"
#include "imlab/statement/CodeGenConfig.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

class CodeGenHelper {
public:
    struct LoadedFunction {
        void* func = nullptr;
        double compileMs = 0;
        bool cacheHit = false;
    };

    explicit CodeGenHelper(const std::string& code)
        : code_(code) {}

    LoadedFunction getExecute() {
        auto found = cache().find(code_);
        if (found != cache().end()) {
            return LoadedFunction{found->second.func, 0, true};
        }

        const auto started = std::chrono::steady_clock::now();
        const std::string stem = "/tmp/imlab-" + hexHash(code_);
        const std::string sourcePath = stem + ".cc";
        const std::string libraryPath = stem + ".so";
        writeFile("/tmp/code.cc", code_);
        writeFile(sourcePath, code_);

        const std::string compileCommand =
            std::string(IMLAB_CXX_COMPILER) +
            " -shared -fPIC -std=c++20 " +
            IMLAB_CODEGEN_CXX_FLAGS +
            " -o \"" + libraryPath + "\" \"" + sourcePath + "\"" +
            " -I\"" + std::string(IMLAB_SOURCE_DIR) + "\"" +
            " -L\"" + std::string(IMLAB_LIBRARY_DIR) + "\"" +
            " -limlab " +
            IMLAB_CODEGEN_LINK_FLAGS;
        if (std::system(compileCommand.c_str()) != 0) {
            throw imlab::RuntimeException("Compilation failed");
        }

        void* handle = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            throw imlab::RuntimeException("Failed to load shared library: " + std::string(dlerror()));
        }
        void* func = dlsym(handle, "execute");
        if (!func) {
            const std::string error = dlerror();
            dlclose(handle);
            throw imlab::RuntimeException("Failed to load function 'execute': " + error);
        }
        cache().emplace(code_, CacheEntry{handle, func});

        const auto finished = std::chrono::steady_clock::now();
        const double compileMs = std::chrono::duration<double, std::milli>(finished - started).count();
        return LoadedFunction{func, compileMs, false};
    }

    static void clearCache() {
        for (auto& entry : cache()) {
            if (entry.second.handle) {
                dlclose(entry.second.handle);
            }
        }
        cache().clear();
    }

private:
    struct CacheEntry {
        void* handle = nullptr;
        void* func = nullptr;
    };

    static std::unordered_map<std::string, CacheEntry>& cache() {
        static std::unordered_map<std::string, CacheEntry> entries;
        return entries;
    }

    static void writeFile(const std::string& path, const std::string& contents) {
        std::ofstream outFile(path);
        if (!outFile) {
            throw imlab::RuntimeException("Failed to open file for writing: " + path);
        }
        outFile << contents;
    }

    static std::string hexHash(const std::string& text) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char ch : text) {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        std::ostringstream out;
        out << std::hex << hash;
        return out.str();
    }

    std::string code_;
};

#endif //CODEGENHELPER_H
