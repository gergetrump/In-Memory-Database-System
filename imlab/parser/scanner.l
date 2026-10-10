%{
// ---------------------------------------------------------------------------------------------------
// IMLAB
// ---------------------------------------------------------------------------------------------------
%}

%{
// ---------------------------------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------------------------------
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <istream>
#include "imlab/parser/ParseContext.h"
#include "imlab/parser/AST.h"
#include "parser.h"

namespace imlab {
namespace parser {

// The location of the current token
extern imlab::parser::location loc;
// The input stream of the scanner
extern std::istream *in;

}  // namespace parser
}  // namespace imlab

using namespace imlab::parser;

// Work around an incompatibility in flex (at least versions
// 2.5.31 through 2.5.33): it generates code that does
// not conform to C89.  See Debian bug 333231
// <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
#undef yywrap
#define yywrap() 1

// Declare the yylex function
#define YY_DECL Parser::symbol_type yylex(ParseContext& sc)
// Configure the scanner to use istreams
#define YY_INPUT(buffer, result, max_size)  \
    result = 0; \
    while (true) { \
        int c = in->get(); \
        if (in->eof()) break; \
        buffer[result++] = c; \
        if (result == max_size || c == '\n') break; \
    }
#if __cplusplus > 199711L
    #define register      // Deprecated in C++11.
#endif  // #if __cplusplus > 199711L
%}

%{
// ---------------------------------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------------------------------
%}
%{
// noyywrap:    Disable yywrap (EOF == end of parsing)
// nounput:     Disable manipulation of input stream
// noinput:     Disable explicit fetch of the next character
// batch:       Scanner in batch-mode (vs. interactive)
// debug:       Write debug info to stderr
// caseless:    Case-insensitive pattern matching
%}
%option noyywrap
%option nounput
%option noinput
%option batch
%option debug
%option caseless

%{
// Code run each time a token is matched.
// We just update the location of the token.
#define YY_USER_ACTION  { loc.columns(yyleng); }
%}

%%

%{
// Code runs each time yylex is called.
// Set the beginning of the token to the end of the previous token.
loc.step ();
%}

[ \t]+              { loc.step(); }
"\n"                { loc.lines (yyleng); loc.step (); }

";"                 { return Parser::make_SEMICOLON(loc); }
","                 { return Parser::make_COMMA(loc); }
"{"                 { return Parser::make_LCB(loc); }
"}"                 { return Parser::make_RCB(loc); }
"("                 { return Parser::make_LPAREN(loc); }
")"                 { return Parser::make_RPAREN(loc); }
"|"                 { return Parser::make_BAR(loc); }

"integer"           { return Parser::make_INTEGER(loc); }
"char"              { return Parser::make_CHAR(loc); }
"varchar"           { return Parser::make_VARCHAR(loc); }
"numeric"           { return Parser::make_NUMERIC(loc); }
"timestamp"         { return Parser::make_TIMESTAMP(loc); }

"create"            { return Parser::make_CREATE(loc); }
"table"             { return Parser::make_TABLE(loc); }
"primary"           { return Parser::make_PRIMARY(loc); }
"key"               { return Parser::make_KEY(loc); }
"not"               { return Parser::make_NOT(loc); }
"null"              { return Parser::make_NULL(loc); }
"copy"              { return Parser::make_COPY(loc); }
"from"              { return Parser::make_FROM(loc); }
"delimiter"         { return Parser::make_DELIMITER(loc); }

"="                 { return Parser::make_EQUALS(loc); }
"."                 { return Parser::make_DOT(loc); }
"*"                 { return Parser::make_STAR(loc); }

"select"            { return Parser::make_SELECT(loc); }
"where"             { return Parser::make_WHERE(loc); }
"and"               { return Parser::make_AND(loc); }
"group"             { return Parser::make_GROUP(loc); }
"by"                { return Parser::make_BY(loc); }
"count"             { return Parser::make_COUNT(loc); }
"sum"               { return Parser::make_SUM(loc); }



[A-Za-z_][A-Za-z0-9_]* { return Parser::make_IDENTIFIER(yytext, loc); }

\'(\\\'|[^\'])*\'   { return Parser::make_Sconst(yytext, loc); }

[0-9]+              {
                        uint32_t value = 0;
                        try {
                            value = std::stoul(yytext);
                        } catch(const std::invalid_argument &err) {
                            sc.Error(loc.begin.line, loc.begin.column, "integer is invalid");
                        } catch(const std::out_of_range &err) {
                            sc.Error(loc.begin.line, loc.begin.column, "integer out of range");
                        }
                        return Parser::make_INTEGER_VALUE(value, loc);
                    }

<<EOF>>             { return Parser::make_EOF(loc); }

.                   { sc.Error(loc.begin.line, loc.begin.column, "invalid character"); }


%%

// ---------------------------------------------------------------------------------------------------
// Code  [0-9][.]?[a-z0-9_]*     { return Parser::make_IDENTIFIERSELECT(yytext, loc); }
// ---------------------------------------------------------------------------------------------------

// The input stream
imlab::parser::location imlab::parser::loc;
// The input stream of the scanner
std::istream *imlab::parser::in = nullptr;

// Begin a scan
void imlab::parser::ParseContext::beginScan(std::istream &is) {
    yy_init_globals();
    yy_flex_debug = trace_scanning_;
    in = &is;
}

// End a scan
void imlab::parser::ParseContext::endScan() {
    yylex_destroy();
    in = nullptr;
}

