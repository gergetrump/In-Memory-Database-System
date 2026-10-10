//---------------------------------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------------------------------
#include "ParseContext.h"
#include "../infra/Defer.h"
#include "fmt/format.h"
#include "gen/parser.h"
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <imlab/runtime/RuntimeException.h>
//---------------------------------------------------------------------------------------------------
namespace imlab::parser {
//---------------------------------------------------------------------------------------------------
// Constructor
ParseContext::ParseContext(bool trace_scanning, bool trace_parsing) : trace_scanning_(trace_scanning),
                                                                      trace_parsing_(trace_parsing) {}
//---------------------------------------------------------------------------------------------------
// Destructor
ParseContext::~ParseContext() = default;
//---------------------------------------------------------------------------------------------------
// Parse a string
std::vector<std::unique_ptr<AST>> ParseContext::Parse(std::istream& in) {
    beginScan(in);
    // make sure that endScan is called even if an exception is thrown
    Defer d([&]() { endScan(); });

    Parser parser(*this);
    parser.set_debug_level(trace_parsing_);
    parser.parse();

    return std::move(parseTrees);
}
//---------------------------------------------------------------------------------------------------
// Yield an error
void ParseContext::Error(const std::string& m) {
    throw imlab::RuntimeException(m);
}
//---------------------------------------------------------------------------------------------------
// Yield an error
void ParseContext::Error(uint32_t line, uint32_t column, const std::string& err) {
    throw imlab::RuntimeException(fmt::format("[ l={0} c={1} ] {2}", line, column, err));
}
//---------------------------------------------------------------------------------------------------
// Register the parsed asts
void ParseContext::registerAST(std::vector<std::unique_ptr<AST>>&& trees) {
    parseTrees = std::move(trees);
}

//---------------------------------------------------------------------------------------------------
} // namespace imlab::parser
//---------------------------------------------------------------------------------------------------