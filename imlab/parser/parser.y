// ---------------------------------------------------------------------------------------------------
// IMLAB
// ---------------------------------------------------------------------------------------------------
%skeleton "lalr1.cc"
%require "3.0.4"
// ---------------------------------------------------------------------------------------------------
// Write a parser header file
%defines
// Define the parser class name
%define api.parser.class {Parser}
// Create the parser in our namespace
%define api.namespace { imlab::parser }

// Use C++ variant to store the values and get better type warnings (compared to "union")
%define api.value.type variant
// With variant-based values, symbols are handled as a whole in the scanner
%define api.token.constructor
// Prefix all tokens
%define api.token.prefix {PARSER_}
// Check if variants are constructed and destroyed properly
%define parse.assert
// Trace the parser
%define parse.trace
// Use verbose parser errors
%define parse.error verbose
// Enable location tracking.

%locations
// Pass the compiler as parameter to yylex/yyparse.
%param { imlab::parser::ParseContext &sc }
// ---------------------------------------------------------------------------------------------------
// Added to the header file and parser implementation before bison definitions.
// We include string for string tokens and forward declare the ParseContext.
%code requires {
#include <memory>
#include <vector>
#include <string>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <istream>
#include "imlab/parser/AST.h"
#include "imlab/parser/ParseContext.h"
#include "imlab/infra/Types.h"
#include "imlab/infra/Helper.h"

typedef std::unique_ptr<imlab::parser::AST> ASTPtr;
typedef std::vector<std::unique_ptr<imlab::parser::AST>> ASTPtrVector;
typedef imlab::parser::Column Column;
typedef std::vector<imlab::parser::Column> ColumnVector;
typedef imlab::parser::Table Table;
typedef std::vector<imlab::parser::Table> TableVector;
typedef struct {
        ColumnVector columns;
        ColumnVector primary_key;
        } ColumnListType;
}
// ---------------------------------------------------------------------------------------------------
// Import the compiler header in the implementation file
%code {
imlab::parser::Parser::symbol_type yylex(imlab::parser::ParseContext& sc);
}
// ---------------------------------------------------------------------------------------------------
// Token definitions

%token <int>            INTEGER_VALUE    "integer_value"
%token <std::string>    Sconst           "string_literal"
%token <std::string>    IDENTIFIER       "identifier"
%token LCB              "left_curly_brackets"
%token RCB              "right_curly_brackets"
%token SEMICOLON        "semicolon"
%token INTEGER          "integer"
%token CHAR             "char"
%token COMMA            "comma"
%token EOF 0            "eof"
%token CREATE           "create"
%token TABLE            "table"
%token PRIMARY          "primary"
%token KEY              "key"
%token NOT              "not"
%token NULL             "null"
%token COPY             "copy"
%token FROM             "from"
%token DELIMITER        "delimiter"
%token VARCHAR          "varchar"
%token NUMERIC          "numeric"
%token LPAREN           "left_parenthesis"
%token RPAREN           "right_parenthesis"
%token TIMESTAMP        "timestamp"
%token SELECT           "select"
%token WHERE            "where"
%token AND              "and"
%token GROUP            "group"
%token BY               "by"
%token COUNT            "count"
%token SUM              "sum"
%token EQUALS           "equals"
%token DOT              "dot"
%token STAR             "star"
%token BAR              "bar"

// ---------------------------------------------------------------------------------------------------

%type <ASTPtrVector> SQLStatementList
%type <ASTPtr> SQLStatement
%type <ASTPtr> CreateTable
%type <ASTPtr> CopyTable
%type <imlab::Type> ColumnType
%type <ColumnListType> OptionalColumnListAndPrimaryKeyDefinition
%type <bool> ColumnConstraint
%type <std::string> OptionalDelimiter
%type <ASTPtr> Query
%type <ASTPtrVector> ColIdList
%type <ASTPtr> ColId
%type <ASTPtrVector> TableRefList
%type <ASTPtr> TableRef
%type <ASTPtrVector> ConditionList
%type <ASTPtr> Condition
%type <ASTPtr> Literal
%type <imlab::parser::Column> Column
%type <ASTPtrVector> SelectList
%type <ASTPtrVector> SelectItemList
%type <ASTPtr> SelectItem
%type <ASTPtrVector> OptionalGroupBy
%type <ASTPtrVector> OptionalWhereClause
%type <ColumnListType> ColumnList
%type <ColumnVector> OptionalPrimaryKeyDefinition
%type <ColumnVector> PrimaryKeyList
%type <ASTPtr> Subquery

%start SQLStatementListStart;
// ---------------------------------------------------------------------------------------------------
%%

SQLStatementListStart:
    SQLStatementList
    {
        sc.registerAST(std::move($1));
    }
    ;

SQLStatementList:
    SQLStatementList SQLStatement
    {
        $1.push_back(std::move($2));
        $$ = std::move($1);
    }
  | %empty
    {
        $$ = ASTPtrVector();
    }
    ;

SQLStatement:
    CreateTable
    {
        $$ = std::move($1);
    }
  | CopyTable
    {
        $$ = std::move($1);
    }
  | Query
    {
        $$ = std::move($1);
    }
    ;

CreateTable:
    CREATE TABLE IDENTIFIER LPAREN OptionalColumnListAndPrimaryKeyDefinition RPAREN SEMICOLON
    {
        imlab::parser::Table table;
        table.id = $3;
        table.columns = std::move($5.columns);
        table.primary_key = std::move($5.primary_key);
        $$ = std::make_unique<imlab::parser::CreateTable>(std::move(table));
    }
    ;

OptionalColumnListAndPrimaryKeyDefinition:
    ColumnList
    {
        $$ = $1;
    }
  | ColumnList COMMA OptionalPrimaryKeyDefinition
    {
        $$ = $1;
        $$.primary_key = $3;
    }
  | %empty
    {
        $$ = ColumnListType();
    }
    ;


OptionalPrimaryKeyDefinition:
    PRIMARY KEY LPAREN PrimaryKeyList RPAREN
    {
        $$ = $4;
    }
  | %empty
    {
        $$ = ColumnVector();
    }
    ;

PrimaryKeyList:
    PrimaryKeyList COMMA IDENTIFIER
    {
        imlab::parser::Column col;
        col.id = $3;
        $1.push_back(col);
        $$ = $1;
    }
  | IDENTIFIER
    {
        imlab::parser::Column col;
        col.id = $1;
        $$ = ColumnVector{col};
    }
    ;

ColumnList:
    ColumnList COMMA Column
    {
        $1.columns.push_back($3);
        $$ = $1;
    }
  | Column
    {
        ColumnListType colListType;
        colListType.columns.push_back($1);
        $$ = colListType;
    }

Column:
    IDENTIFIER ColumnType ColumnConstraint
    {
        imlab::parser::Column column;
        column.id = $1;
        column.type = $2;
        column.not_null = $3;
        $$ = column;
    }
    ;


CopyTable:
    COPY IDENTIFIER FROM Sconst OptionalDelimiter SEMICOLON
    {
        std::string filePath = $4;
        if (filePath.size() >= 2 && filePath.front() == '\'' && filePath.back() == '\'') {
            filePath = filePath.substr(1, filePath.size() - 2);
        }

        $$ = std::make_unique<imlab::parser::CopyTable>(std::move($2), std::move(filePath), std::move($5));
    }
    ;

OptionalDelimiter:
    DELIMITER Sconst
    {
        std::string delimiter = $2;
        if (delimiter.size() >= 2 && delimiter.front() == '\'' && delimiter.back() == '\'') {
            delimiter = delimiter.substr(1, delimiter.size() - 2);
        }
        $$ = delimiter;
    }
  | %empty
    {
        $$ = "|";
    }
    ;


ColumnType:
    INTEGER
    {
        $$ = imlab::Type::Integer();
    }
  | VARCHAR LPAREN INTEGER_VALUE RPAREN
    {
        $$ = imlab::Type::Varchar($3);
    }
  | NUMERIC LPAREN INTEGER_VALUE COMMA INTEGER_VALUE RPAREN
    {
        $$ = imlab::Type::Numeric($3, $5);
    }
  | CHAR LPAREN INTEGER_VALUE RPAREN
    {
        $$ = imlab::Type::Char($3);
    }
  | TIMESTAMP
    {
        $$ = imlab::Type::Timestamp();
    }
    ;

ColumnConstraint:
    NOT NULL
    {
        $$ = true;
    }
  | %empty
    {
        $$ = false;
    }
    ;

Query:
    SELECT SelectList FROM TableRefList OptionalWhereClause OptionalGroupBy SEMICOLON
    {
        $$ = std::make_unique<imlab::parser::Query>(std::move($2), std::move($4), std::move($5), std::move($6));
    }
    ;

Subquery:
    SELECT SelectList FROM TableRefList OptionalWhereClause OptionalGroupBy
    {
        $$ = std::make_unique<imlab::parser::Query>(std::move($2), std::move($4), std::move($5), std::move($6));
    }
    ;


SelectList:
    STAR
    {
        $$ = ASTPtrVector();
    }
  | SelectItemList
    {
        $$ = std::move($1);
    }
    ;

SelectItemList:
    SelectItemList COMMA SelectItem
    {
        $1.push_back(std::move($3));
        $$ = std::move($1);
    }
  | SelectItem
    {
        $$ = ASTPtrVector();
        $$.push_back(std::move($1));
    }
    ;

SelectItem:
    ColId
    {
        $$ = std::move($1);
    }
  | COUNT LPAREN STAR RPAREN
    {
        $$ = std::make_unique<imlab::parser::Aggregate>(imlab::parser::Aggregate::CountStar, nullptr);
    }
  | SUM LPAREN ColId RPAREN
    {
        $$ = std::make_unique<imlab::parser::Aggregate>(imlab::parser::Aggregate::Sum, std::move($3));
    }
    ;

OptionalGroupBy:
    GROUP BY ColIdList
    {
        $$ = std::move($3);
    }
  | %empty
    {
        $$ = ASTPtrVector();
    }
    ;

ColIdList:
    ColIdList COMMA ColId
    {
        $1.push_back(std::move($3));
        $$ = std::move($1);
    }
  | ColId
    {
        $$ = ASTPtrVector();
        $$.push_back(std::move($1));
    }
    ;

ColId:
    IDENTIFIER DOT IDENTIFIER
    {
        $$ = std::make_unique<imlab::parser::ColId>($1, $3);
    }
  | IDENTIFIER
    {
        $$ = std::make_unique<imlab::parser::ColId>("", $1);
    }
    ;

TableRefList:
    TableRefList COMMA TableRef
    {
        $1.push_back(std::move($3));
        $$ = std::move($1);
    }
  | TableRef
    {
        $$ = ASTPtrVector();
        $$.push_back(std::move($1));
    }
    ;

TableRef:
    IDENTIFIER IDENTIFIER
    {
        $$ = std::make_unique<imlab::parser::TableRef>($1, $2);
    }
  | IDENTIFIER
    {
        $$ = std::make_unique<imlab::parser::TableRef>($1, "");
    }
  | LPAREN Subquery RPAREN IDENTIFIER
    {
        $$ = std::make_unique<imlab::parser::SubqueryRef>(std::move($2), $4);
    }
  | LPAREN Subquery RPAREN
    {
        $$ = std::make_unique<imlab::parser::SubqueryRef>(std::move($2), "");
    }
    ;

OptionalWhereClause:
    WHERE ConditionList
    {
        $$ = std::move($2);
    }
  | %empty
    {
        $$ = ASTPtrVector();
    }
    ;

ConditionList:
    ConditionList AND Condition
    {
        $1.push_back(std::move($3));
        $$ = std::move($1);
    }
  | Condition
    {
        $$ = ASTPtrVector();
        $$.push_back(std::move($1));
    }
    ;

Condition:
    ColId EQUALS ColId
    {
        $$ = std::make_unique<imlab::parser::BinaryExpression>(
            imlab::parser::BinaryExpression::EQ,
            std::move($1),
            std::move($3)
        );
    }
  | ColId EQUALS Literal
    {
        $$ = std::make_unique<imlab::parser::BinaryExpression>(
            imlab::parser::BinaryExpression::EQ,
            std::move($1),
            std::move($3)
        );
    }
    ;

Literal:
    Sconst
    {
        std::string literalValue = $1;
        if (literalValue.size() >= 2 && literalValue.front() == '\'' && literalValue.back() == '\'') {
            literalValue = literalValue.substr(1, literalValue.size() - 2);
        }
        $$ = std::make_unique<imlab::parser::Literal>(literalValue);
    }
  | INTEGER_VALUE
    {
        $$ = std::make_unique<imlab::parser::Literal>(std::to_string($1));
    }
    ;





%%
// ---------------------------------------------------------------------------------------------------
// Define error function
void imlab::parser::Parser::error(const location_type& l, const std::string& m) {
    sc.Error(l.begin.line, l.begin.column, m);
}
// ---------------------------------------------------------------------------------------------------

