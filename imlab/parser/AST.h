//------------------------------------------------------------
#ifndef IMLAB_PARSER_AST_H
#define IMLAB_PARSER_AST_H
//------------------------------------------------------------
#include "../../imlab/infra/Types.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>
//------------------------------------------------------------
namespace imlab {
//------------------------------------------------------------
namespace semana {
class SemanticAnalysis;
}; // namespace semana
//------------------------------------------------------------
namespace parser {
//------------------------------------------------------------
class AST;
class ColId;
class TableRef;
class BinaryExpression;
//------------------------------------------------------------
class AST {
    friend semana::SemanticAnalysis;

    public:
    /// the different ParseTreeNode types
    enum ASTType : uint8_t {
        CreateTableType,
        CopyTableType,
        QueryType,
        ColIdType,
        BinaryExpressionType,
        TableRefType,
        LiteralType,
        SubqueryRefType,
        AggregateType,
    };

    protected:
    /// Constructor
    explicit AST(ASTType type);

    public:
    /// Destructor
    virtual ~AST() = default;

    /// Get the type of the ParseTreeNode
    ASTType getType() const;

    private:
    /// type of the ParseTreeNode
    ASTType type;
};
//---------------------------------------------------------------------------
// A single column
// Required by test/parser/ParserTest.cc
struct Column {
    // Name of the column
    std::string id;
    // Type of the column
    Type type;

    bool not_null = false;
};
//---------------------------------------------------------------------------
// A single table
// Required by test/parser/ParserTest.cc
struct Table {
    // Name of the table
    std::string id;
    // Columns
    std::vector<Column> columns;
    // Primary key
    std::vector<Column> primary_key;
};
//------------------------------------------------------------
class CopyTable : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor ('|' is the delimiter in the tbl files)
    CopyTable(std::string table, std::string file, std::string delimiter);
    /// Destructor
    ~CopyTable() override = default;

    /// Get the table
    std::string_view getTable() const { return table; }
    /// Get the file
    std::string_view getFile() const { return file; }
    /// Get the delimiter
    std::string_view getDelimiter() const { return delimiter; }

    private:
    std::string table;
    std::string file;
    std::string delimiter;
};
//------------------------------------------------------------
class CreateTable : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor
    explicit CreateTable(Table table);
    /// Destructor
    ~CreateTable() override = default;

    /// Get the parsed table
    const Table& getTable() const { return table; }

    private:
    /// the parsed table
    Table table;
};
//------------------------------------------------------------
class Query : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor
    Query(std::vector<std::unique_ptr<AST>> targets, std::vector<std::unique_ptr<AST>> from, std::vector<std::unique_ptr<AST>> where, std::vector<std::unique_ptr<AST>> groupBy);
    /// Destructor
    ~Query() override = default;

    /// Get the targets
    std::vector<AST*> getTargets() const;
    /// Get the tables
    std::vector<AST*> getFrom() const;
    /// get conditions
    std::vector<AST*> getWhere() const;
    /// Get the GROUP BY columns
    std::vector<AST*> getGroupBy() const;

    private:
    /// the targets of the query (class ColId or Aggregate)
    std::vector<std::unique_ptr<AST>> targets;
    /// the tables of the query (class TableRef)
    std::vector<std::unique_ptr<AST>> from;
    /// the conditions of the query (class BinaryExpression)
    std::vector<std::unique_ptr<AST>> where;
    /// the GROUP BY columns (class ColId)
    std::vector<std::unique_ptr<AST>> groupBy;
};

//------------------------------------------------------------
class Aggregate : public AST {
    friend semana::SemanticAnalysis;

    public:
    enum Kind { CountStar, Sum };

    Aggregate(Kind kind, std::unique_ptr<AST> argument);
    ~Aggregate() override = default;

    Kind getKind() const { return kind; }
    AST* getArgument() const { return argument.get(); }

    private:
    Kind kind;
    std::unique_ptr<AST> argument;
};
//------------------------------------------------------------
class ColId : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor
    ColId(std::string table, std::string column);
    /// Destructor
    ~ColId() override = default;

    /// Get the table
    std::string getTable() const { return table; }
    /// Get the column
    std::string getColumn() const { return column; }

    private:
    /// table
    std::string table;
    /// column
    std::string column;
};
//------------------------------------------------------------
class BinaryExpression : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Type of the BinaryExpression
    enum SubType { EQ, AND };

    /// Constructor
    BinaryExpression(SubType op, std::unique_ptr<AST> left, std::unique_ptr<AST> right);
    /// Destructor
    ~BinaryExpression() override = default;

    /// Get the type of the BinaryExpression
    SubType getOp() const { return op; }
    /// Get the left expression
    AST* getLeft() const { return left.get(); }
    /// Get the right expression
    AST* getRight() const { return right.get(); }

    private:
    /// type of the BinaryExpression
    SubType op;
    /// left expression (class ColId)
    std::unique_ptr<AST> left;
    /// right expression (either class ColId or class Literal)
    std::unique_ptr<AST> right;
};
//------------------------------------------------------------
class TableRef : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor
    TableRef(std::string table, std::string alias);
    /// Destructor
    ~TableRef() override = default;

    /// Get the table name
    std::string_view getTable() const { return table; }
    /// Get the alias
    std::string_view getAlias() const { return alias; }

    private:
    /// value
    std::string table;
    /// alias
    std::string alias;
};

class SubqueryRef : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor
    SubqueryRef(std::unique_ptr<AST> subquery, std::string alias);
    /// Destructor
    ~SubqueryRef() override = default;

    /// Get the subquery
    AST* getSubquery() const { return subquery.get(); }
    /// Get the alias
    std::string getAlias() const { return alias; }

    private:
    /// subquery AST node
    std::unique_ptr<AST> subquery;
    /// alias
    std::string alias;
};

//------------------------------------------------------------
class Literal : public AST {
    friend semana::SemanticAnalysis;

    public:
    /// Constructor
    explicit Literal(std::string literal);
    /// Destructor
    ~Literal() override = default;

    /// Get the text value of the literal
    std::string_view getLiteralValue() const { return literal; }

    private:
    /// the text format of the literal
    std::string literal;
};
//------------------------------------------------------------
} // namespace parser
//------------------------------------------------------------
} // namespace imlab
//------------------------------------------------------------
#endif //IMLAB_PARSER_AST_H
//------------------------------------------------------------
