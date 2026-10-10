//------------------------------------------------------------
#include "imlab/parser/AST.h"
#include "imlab/infra/Types.h"
#include <memory>
//------------------------------------------------------------
using namespace std;
//------------------------------------------------------------
namespace imlab::parser {
//------------------------------------------------------------
AST::AST(AST::ASTType type) : type(type)
// Constructor
{
}
//------------------------------------------------------------
AST::ASTType AST::getType() const
// Get the type of the ParseTreeNode
{
    return type;
}
//------------------------------------------------------------
CopyTable::CopyTable(string table, string file, string delimiter) : AST(ASTType::CopyTableType), table(std::move(table)), file(std::move(file)), delimiter(delimiter.empty() ? "|" : std::move(delimiter))
// Constructor
{
}
//------------------------------------------------------------
CreateTable::CreateTable(Table table) : AST(ASTType::CreateTableType), table(std::move(table))
// Constructor
{
}
//------------------------------------------------------------
Query::Query(std::vector<std::unique_ptr<AST>> targets, std::vector<std::unique_ptr<AST>> from, std::vector<std::unique_ptr<AST>> where, std::vector<std::unique_ptr<AST>> groupBy) : AST(ASTType::QueryType), targets(std::move(targets)), from(std::move(from)), where(std::move(where)), groupBy(std::move(groupBy)) {}
//------------------------------------------------------------
static vector<AST*> resolve(const std::vector<std::unique_ptr<AST>>& i) {
    std::vector<AST*> r;
    for (auto& t : i) r.push_back(t.get());
    return r;
}
//------------------------------------------------------------
std::vector<AST*> Query::getTargets() const { return resolve(targets); }
//------------------------------------------------------------
std::vector<AST*> Query::getFrom() const { return resolve(from); }
//------------------------------------------------------------
std::vector<AST*> Query::getWhere() const { return resolve(where); }
//------------------------------------------------------------
std::vector<AST*> Query::getGroupBy() const { return resolve(groupBy); }
//------------------------------------------------------------
Aggregate::Aggregate(Aggregate::Kind kind, std::unique_ptr<AST> argument) : AST(ASTType::AggregateType), kind(kind), argument(std::move(argument)) {}
//------------------------------------------------------------
ColId::ColId(std::string table, std::string column) : AST(ASTType::ColIdType), table(std::move(table)), column(std::move(column)) {}
//------------------------------------------------------------
BinaryExpression::BinaryExpression(BinaryExpression::SubType op, unique_ptr<AST> left, unique_ptr<AST> right) : AST(ASTType::BinaryExpressionType), op(op), left(move(left)), right(move(right)) {}
//------------------------------------------------------------
TableRef::TableRef(std::string table, std::string alias) : AST(ASTType::TableRefType), table(std::move(table)), alias(std::move(alias)) {}

SubqueryRef::SubqueryRef(std::unique_ptr<AST> subquery, std::string alias) : AST(ASTType::SubqueryRefType), subquery(std::move(subquery)), alias(std::move(alias)) {}

//------------------------------------------------------------
Literal::Literal(std::string literal) : AST(ASTType::LiteralType), literal(std::move(literal)) {}
//------------------------------------------------------------
} // namespace imlab::parser
//------------------------------------------------------------