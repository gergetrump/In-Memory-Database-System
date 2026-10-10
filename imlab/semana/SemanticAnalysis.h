//------------------------------------------------------------
#ifndef IMLAB_SEMANA_SEMANTICANALYSIS_H
#define IMLAB_SEMANA_SEMANTICANALYSIS_H
//------------------------------------------------------------
#include "../../imlab/parser/AST.h"

#include <imlab/algebra/Const.h>
//------------------------------------------------------------
namespace imlab {
//------------------------------------------------------------
class Database;
//------------------------------------------------------------
namespace algebra {
class Scope;
class Operator;
class Expression;
} // namespace algebra
//------------------------------------------------------------
namespace statement {
class Statement;
} // namespace statement
//------------------------------------------------------------
namespace semana {
//------------------------------------------------------------
class SemanticAnalysis {
    public:
    std::unique_ptr<statement::Statement> buildStatement(const parser::AST&, Database&);

    std::unique_ptr<statement::Statement> analyzeCreateTable(const parser::CreateTable&, Database&);

    std::unique_ptr<statement::Statement> analyzeCopyTable(const parser::CopyTable&, Database&);

    std::unique_ptr<statement::Statement> analyzeSelectStmt(const parser::Query&, Database&);

    std::unique_ptr<statement::Statement> analyzeAggregateStmt(const parser::Query&, Database&);

    std::unique_ptr<algebra::Operator> analyzeSubquery(const parser::Query& query, Database& db, algebra::Scope& scope);

    std::unique_ptr<algebra::Operator> analyzeSubqueryRef(const parser::SubqueryRef& subqueryRef, Database& db, algebra::Scope& parentScope);

    std::unique_ptr<algebra::Operator> analyzeFrom(const std::vector<parser::AST*>&, Database&, algebra::Scope&);

    std::unique_ptr<algebra::Operator> analyzeWhere(const std::vector<parser::AST*>&, Database&, algebra::Scope&, std::unique_ptr<algebra::Operator>);

    std::unique_ptr<algebra::Operator> analyzeTableRef(const parser::TableRef&, Database&, algebra::Scope&);

    std::unique_ptr<algebra::Expression> analyzeBinaryExpression(const parser::BinaryExpression&, Database&, const algebra::Scope&);

    std::unique_ptr<algebra::Expression>convertConstToType(const algebra::Const&constExpr, const Type&expectedType);

    std::unique_ptr<algebra::Expression> analyzeColId(const parser::ColId&, Database&, const algebra::Scope&);

    std::unique_ptr<algebra::Expression> analyzeLiteral(const parser::Literal&, Database&, const algebra::Scope&);

    std::unique_ptr<algebra::Expression> analyzeExpression(const parser::AST& expr, Database& db, const algebra::Scope& scope);

    int64_t parseNumericString(const std::string& str, unsigned len, unsigned precision);
};
//------------------------------------------------------------
} // namespace semana
//------------------------------------------------------------
} // namespace imlab
//------------------------------------------------------------
#endif //IMLAB_SEMANA_SEMANTICANALYSIS_H
