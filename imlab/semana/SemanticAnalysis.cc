//------------------------------------------------------------
#include "imlab/semana/SemanticAnalysis.h"
#include "imlab/algebra/Expression.h"
#include "imlab/algebra/Operator.h"
#include "imlab/algebra/SubqueryScan.h"
#include "imlab/runtime/RuntimeException.h"
#include "imlab/statement/CopyTableStatement.h"
#include "imlab/statement/CreateTableStatement.h"
#include "imlab/statement/Statement.h"
#include <memory>
#include <fmt/format.h>
#include <imlab/algebra/BinaryExpression.h>
#include <imlab/algebra/Const.h>
#include <imlab/algebra/GroupBy.h>
#include <imlab/algebra/InnerJoin.h>
#include <imlab/algebra/IURef.h>
#include <imlab/algebra/Print.h>
#include <imlab/algebra/Select.h>
#include <imlab/algebra/TableScan.h>
#include <set>
#include <imlab/statement/QueryStatement.h>
#include "imlab/parser/AST.h"
//------------------------------------------------------------
namespace imlab::semana {
//---------------------------------------------------------------------------
std::unique_ptr<statement::Statement> SemanticAnalysis::buildStatement(const imlab::parser::AST& input, imlab::Database& db) {
    switch (input.getType()) {
        case parser::AST::ASTType::CreateTableType: {
            auto* createTable = static_cast<const parser::CreateTable*>(&input);
            if (!createTable) {
                throw RuntimeException("Invalid AST type for CreateTable");
            }
            return analyzeCreateTable(*createTable, db);
        }
        case parser::AST::ASTType::CopyTableType: {
            auto* copyTable = static_cast<const parser::CopyTable*>(&input);
            if (!copyTable) {
                throw RuntimeException("Invalid AST type for CopyTable");
            }
            return analyzeCopyTable(*copyTable, db);
        }
        case parser::AST::ASTType::QueryType: {
            auto* query = static_cast<const parser::Query*>(&input);
            if (!query) {
                throw RuntimeException("Invalid AST type for Query");
            }
            return analyzeSelectStmt(*query, db);
        }
        default:
            throw RuntimeException("SemanticAnalysis::buildStatement: Unknown AST");
    }
}
//------------------------------------------------------------
std::unique_ptr<statement::Statement> SemanticAnalysis::analyzeCreateTable(const parser::CreateTable& ast, Database& db) {
    if (db.tableExists(ast.table.id)) {
        throw imlab::RuntimeException(fmt::format("table {} already exists", ast.table.id));
    }

    std::map<std::string, Type> nameTypeMap;
    for (auto& c : ast.table.columns) {

        // check if column already exists
        if (nameTypeMap.find(c.id) != nameTypeMap.end()) {
            throw imlab::RuntimeException(fmt::format("Duplicate column name in CREATE statement: '{}'", c.id));
        }
        // check column is declared not null
        if (!c.not_null) {
            throw imlab::RuntimeException(fmt::format("Column must be declared not null: '{}'", c.id));
        }

        nameTypeMap.insert({c.id, c.type});
    }

    return std::make_unique<statement::CreateTableStatement>(ast.table);
}
//------------------------------------------------------------
std::unique_ptr<statement::Statement> SemanticAnalysis::analyzeCopyTable(const parser::CopyTable& ast, Database& db) {

    if (!db.tableExists(ast.table)) {
        throw RuntimeException(fmt::format("table {} does not exist", ast.table));
    }
    if (ast.delimiter.empty()) {
        throw RuntimeException("The delimiter should not be empty");
    }

    return std::make_unique<statement::CopyTableStatement>(ast.table, ast.file, ast.delimiter);
}
//------------------------------------------------------------
namespace {
bool queryUsesAggregation(const parser::Query& query) {
    if (!query.getGroupBy().empty()) {
        return true;
    }
    for (const auto* target : query.getTargets()) {
        if (target->getType() == parser::AST::AggregateType) {
            return true;
        }
    }
    return false;
}

std::string uniqueOutputName(const std::string& base, std::set<std::string>& used) {
    std::string name = base;
    int suffix = 2;
    while (used.count(name) != 0) {
        name = base + "_" + std::to_string(suffix++);
    }
    used.insert(name);
    return name;
}
} // namespace

std::unique_ptr<statement::Statement> SemanticAnalysis::analyzeAggregateStmt(const parser::Query& query, Database& db) {
    if (query.getTargets().empty()) {
        throw RuntimeException("SELECT * is not supported with GROUP BY or aggregates");
    }

    algebra::Scope queryScope;
    auto tree = analyzeFrom(query.getFrom(), db, queryScope);
    tree = analyzeWhere(query.getWhere(), db, queryScope, std::move(tree));

    std::vector<const algebra::IU*> keys;
    std::set<const algebra::IU*> keySet;
    for (const auto* item : query.getGroupBy()) {
        if (item->getType() != parser::AST::ColIdType) {
            throw RuntimeException("GROUP BY supports integer columns only");
        }
        auto expr = analyzeColId(static_cast<const parser::ColId&>(*item), db, queryScope);
        const auto* iu = static_cast<algebra::IURef*>(expr.get())->getIU();
        if (iu->type_.getClass() != Type::kInteger) {
            throw RuntimeException("GROUP BY supports integer columns only");
        }
        if (!keySet.insert(iu).second) {
            throw RuntimeException("Duplicate GROUP BY column '" + iu->column_ + "'");
        }
        keys.push_back(iu);
    }

    auto groupBy = std::make_unique<algebra::GroupBy>(std::move(tree), std::move(keys));
    std::vector<algebra::Print::Output> outputCols;
    std::set<std::string> usedNames;

    for (const auto* item : query.getTargets()) {
        if (item->getType() == parser::AST::ColIdType) {
            const auto& colId = static_cast<const parser::ColId&>(*item);
            auto expr = analyzeColId(colId, db, queryScope);
            const auto* iu = static_cast<algebra::IURef*>(expr.get())->getIU();
            if (keySet.find(iu) == keySet.end()) {
                throw RuntimeException("Column '" + iu->column_ + "' must appear in GROUP BY or inside an aggregate");
            }
            const std::string name = uniqueOutputName(colId.getColumn(), usedNames);
            outputCols.push_back({name, std::make_unique<algebra::IURef>(iu)});
        } else if (item->getType() == parser::AST::AggregateType) {
            const auto& aggregate = static_cast<const parser::Aggregate&>(*item);
            if (aggregate.getKind() == parser::Aggregate::CountStar) {
                const std::string name = uniqueOutputName("count", usedNames);
                const auto* result = groupBy->addCount(name);
                outputCols.push_back({name, std::make_unique<algebra::IURef>(result)});
            } else {
                const auto* argument = aggregate.getArgument();
                if (!argument || argument->getType() != parser::AST::ColIdType) {
                    throw RuntimeException("SUM requires an integer column");
                }
                auto expr = analyzeColId(static_cast<const parser::ColId&>(*argument), db, queryScope);
                const auto* iu = static_cast<algebra::IURef*>(expr.get())->getIU();
                if (iu->type_.getClass() != Type::kInteger) {
                    throw RuntimeException("SUM requires an integer column");
                }
                const std::string name = uniqueOutputName("sum_" + iu->column_, usedNames);
                const auto* result = groupBy->addSum(iu, name);
                outputCols.push_back({name, std::make_unique<algebra::IURef>(result)});
            }
        } else {
            throw RuntimeException("Unsupported select item");
        }
    }

    auto printOp = std::make_unique<algebra::Print>(std::move(groupBy), std::move(outputCols));
    return std::make_unique<statement::QueryStatement>(std::move(printOp));
}

std::unique_ptr<statement::Statement> SemanticAnalysis::analyzeSelectStmt(const parser::Query& query, Database& db) {
    if (queryUsesAggregation(query)) {
        return analyzeAggregateStmt(query, db);
    }

    algebra::Scope queryScope;
    auto tree = analyzeSubquery(query, db, queryScope);

    std::vector<algebra::Print::Output> outputCols;

    if (query.getTargets().empty()) {
        // case: SELECT *
        auto colsInScope = queryScope.getAllIUs();
        for (const auto* column : colsInScope) {
            // create an IURef for each IU (column)
            auto iuRef = std::make_unique<algebra::IURef>(column);
            outputCols.push_back({column->column_, std::move(iuRef)});
        }
    } else {
        // case: SELECT col1, col2, ...
        for (const auto& selectItem : query.getTargets()) {
            // analyze expression (e.g. 'col1 + col2')
            auto exprRef = analyzeExpression(*selectItem, db, queryScope);

            // determine name of output column
            std::string outputColName;
            if (selectItem->getType() == parser::AST::ColIdType) {
                // simple col reference -> get col name
                auto& colId = static_cast<const parser::ColId&>(*selectItem);
                outputColName = colId.getColumn();
            } else {
                // expression -> use placeholder name
                outputColName = "expression reference";
            }

            outputCols.push_back({outputColName, std::move(exprRef)});
        }
    }

    auto printOp = std::make_unique<algebra::Print>(std::move(tree), std::move(outputCols));
    return std::make_unique<statement::QueryStatement>(std::move(printOp));
}
//------------------------------------------------------------
std::unique_ptr<algebra::Operator> SemanticAnalysis::analyzeSubquery(const parser::Query& query, Database& db, algebra::Scope& scope) {
    if (queryUsesAggregation(query)) {
        throw RuntimeException("GROUP BY and aggregates are not supported in subqueries");
    }

    auto tree = analyzeFrom(query.getFrom(), db, scope);

    tree = analyzeWhere(query.getWhere(), db, scope, std::move(tree));

    return tree;
}
//------------------------------------------------------------





std::unique_ptr<algebra::Operator> SemanticAnalysis::analyzeFrom(const std::vector<parser::AST*>& fromList, Database& db, algebra::Scope& scope) {
    if (fromList.empty()) {
        throw RuntimeException("FROM clause can't be empty");
    }

    std::unique_ptr<algebra::Operator> fromTree;

    for (const auto* fromItem : fromList) {
        std::unique_ptr<algebra::Operator> currentScan;

        if (fromItem->getType() == parser::AST::ASTType::TableRefType) {
            auto tableRef = static_cast<const parser::TableRef*>(fromItem);
            currentScan = analyzeTableRef(*tableRef, db, scope);
        } else if (fromItem->getType() == parser::AST::ASTType::SubqueryRefType) {
            auto subqueryRef = static_cast<const parser::SubqueryRef*>(fromItem);
            currentScan = analyzeSubqueryRef(*subqueryRef, db, scope);
        } else {
            throw RuntimeException("Invalid table reference in FROM clause");
        }

        if (!fromTree) {
            // only executed in first loop
            fromTree = std::move(currentScan);
        } else {
            // subsequent loops
            fromTree = std::make_unique<algebra::InnerJoin>(
                std::move(fromTree),
                std::move(currentScan),
                nullptr
            );
        }
    }

    return fromTree;
}
//------------------------------------------------------------


std::unique_ptr<algebra::Operator> SemanticAnalysis::analyzeWhere(const std::vector<parser::AST*>& whereClause, Database& db, algebra::Scope& scope, std::unique_ptr<algebra::Operator> tree) {

    if (whereClause.empty()) {
        return tree;
    }

    for (const auto* condition : whereClause) {


        if (condition->getType() == parser::AST::BinaryExpressionType) {
            const auto* binaryExpr = static_cast<const parser::BinaryExpression*>(condition);

            if (binaryExpr->getOp() == parser::BinaryExpression::AND) {
                tree = analyzeWhere({binaryExpr->getLeft()}, db, scope, std::move(tree));
                tree = analyzeWhere({binaryExpr->getRight()}, db, scope, std::move(tree));
                continue;
            }

            auto expr = analyzeBinaryExpression(*binaryExpr, db, scope);

            tree = std::make_unique<algebra::Select>(std::move(tree), std::move(expr));
        } else {
            throw RuntimeException("Unsupported AST node type in WHERE clause");
        }
    }

    return tree;
}

//------------------------------------------------------------
std::unique_ptr<algebra::Operator> SemanticAnalysis::analyzeTableRef(const parser::TableRef& tableRef, Database& db, algebra::Scope& scope) {

    const auto tableName = static_cast<std::string>(tableRef.getTable());
    const auto alias = static_cast<std::string>(tableRef.getAlias().empty() ? tableName : tableRef.getAlias());

    auto& table = db.getTable(tableName);
    std::vector<const algebra::IU*> iuPtrs;
    for (auto& iuPtr : table.getColumnsIUs()) {
        iuPtrs.emplace_back(iuPtr.get());
    }

    scope.addTable(tableName, alias, iuPtrs);
    auto tableScan = std::make_unique<algebra::TableScan>(tableName.c_str(), 0, table.getSchema(), iuPtrs, table.getRows().size());

    return tableScan;
}

std::unique_ptr<algebra::Operator> SemanticAnalysis::analyzeSubqueryRef(const parser::SubqueryRef& subqueryRef, Database& db, algebra::Scope& parentScope) {
    if (!subqueryRef.getSubquery()) {
        throw RuntimeException("SubqueryRef contains no subquery");
    }

    const auto* queryNode = static_cast<const parser::Query*>(subqueryRef.getSubquery());
    if (!queryNode) {
        throw RuntimeException("SubqueryRef does not contain a Query node");
    }

    algebra::Scope subqueryScope;

    auto subqueryOp = analyzeSubquery(*queryNode, db, subqueryScope);


    if (!subqueryRef.getAlias().empty()) {
        auto subqueryIUs = subqueryOp->collectIUs();
        parentScope.addTable(subqueryRef.getAlias(), "", std::vector(subqueryIUs.begin(), subqueryIUs.end()));
    }

    return std::make_unique<algebra::SubqueryScan>(std::move(subqueryOp), subqueryRef.getAlias());
}


//------------------------------------------------------------
std::unique_ptr<algebra::Expression> SemanticAnalysis::analyzeBinaryExpression(const parser::BinaryExpression& binExpr, Database& db, const algebra::Scope& scope) {

    auto leftExpr = analyzeExpression(*binExpr.getLeft(), db, scope);
    auto rightExpr = analyzeExpression(*binExpr.getRight(), db, scope);

    if (leftExpr->getExpressionType() != algebra::Expression::ExpressionType::IURef) {
        throw RuntimeException("Left expression must be a column reference");
    }
    auto iuRef = static_cast<algebra::IURef*>(leftExpr.get());
    Type expectedType = iuRef->getIU()->type_;

    if (rightExpr->getExpressionType() == algebra::Expression::ExpressionType::Const) {
        auto constExpr = static_cast<algebra::Const*>(rightExpr.get());
        if (!constExpr->hasType()) {
            rightExpr = convertConstToType(*constExpr, expectedType);
        }
    }

    algebra::BinaryExpression::Op op;
    switch (binExpr.getOp()) {
        case parser::BinaryExpression::EQ:
            op = algebra::BinaryExpression::Op::Equal;
            break;
        case parser::BinaryExpression::AND:
            op = algebra::BinaryExpression::Op::And;
            break;
        default:
            throw RuntimeException("Unsupported binary expression operation");
    }

    return std::make_unique<algebra::BinaryExpression>(op, std::move(leftExpr), std::move(rightExpr));
}

// helper function
std::unique_ptr<algebra::Expression> SemanticAnalysis::convertConstToType(
    const algebra::Const& constExpr, const Type& expectedType) {

    const std::string& literalVal = constExpr.getConstant();
    const char* str = literalVal.c_str();
    uint32_t strLen = literalVal.length();

    try {
        auto typedValue = Value(Integer(0));
        switch (expectedType.getClass()) {
            case Type::kInteger: {
                auto intValue = Integer::castString(str, strLen);
                typedValue = Value(intValue);
                break;
            }
            case Type::kNumeric: {
                unsigned len = expectedType.getPrecision();
                unsigned precision = expectedType.getScale();
                int64_t rawValue = parseNumericString(literalVal, len, precision);
                typedValue = Value(rawValue, expectedType);
                break;
            }
            case Type::kTimestamp: {
                auto timestampValue = Timestamp::castString(str, strLen);
                typedValue = Value(timestampValue);
                break;
            }
            case Type::kVarchar:
            case Type::kChar: {
                unsigned maxLength = expectedType.getLength();
                if (literalVal.length() > maxLength) {
                    throw RuntimeException("String value exceeds maximum length");
                }
                typedValue = Value(literalVal, expectedType);
                break;
            }
            default:
                throw RuntimeException("Unsupported expected type in constant conversion");
        }

        return std::make_unique<algebra::Const>(typedValue, expectedType);

    } catch (const std::exception& e) {
        throw RuntimeException(fmt::format("Failed to convert constant '{}' to expected type: {}", literalVal, e.what()));
    }
}

// helper function
int64_t SemanticAnalysis::parseNumericString(const std::string& str, unsigned len, unsigned precision) {
    // Parse string to double
    double value = std::stod(str);

    // Scale value according to precision
    double scaleFactor = std::pow(10, precision);
    int64_t scaledValue = static_cast<int64_t>(value * scaleFactor);

    // Calculate max allowed value based on len and precision
    int64_t maxValue = static_cast<int64_t>(std::pow(10, len) - 1);

    if (std::abs(scaledValue) > maxValue) {
        throw RuntimeException("Numeric value out of range");
    }

    return scaledValue;
}

// helper function
std::unique_ptr<algebra::Expression> SemanticAnalysis::analyzeExpression(
    const parser::AST& expr, Database& db, const algebra::Scope& scope) {

    if (expr.getType() == parser::AST::ASTType::ColIdType) {
        return analyzeColId(*static_cast<const parser::ColId*>(&expr), db, scope);
    }

    if (expr.getType() == parser::AST::ASTType::LiteralType) {
        return analyzeLiteral(*static_cast<const parser::Literal*>(&expr), db, scope);
    }

    if (expr.getType() == parser::AST::ASTType::BinaryExpressionType) {
        return analyzeBinaryExpression(static_cast<const parser::BinaryExpression&>(expr), db, scope);
    }

    throw RuntimeException("Unsupported expression type");
}

//------------------------------------------------------------
std::unique_ptr<algebra::Expression> SemanticAnalysis::analyzeColId(const parser::ColId& colId, Database& db, const algebra::Scope& scope) {
    const algebra::IU* iu;

    if (!colId.getTable().empty()) {
        // table name, column
        iu = scope.resolveIU(colId.getTable(), colId.getColumn());
        if (!iu) {
            throw RuntimeException(fmt::format("Column {} not found in Table {}", colId.getColumn(), colId.getTable()));
        }
    } else {
        // column
        iu = scope.resolveIU("", colId.getColumn());
        if (!iu) {
            throw RuntimeException(fmt::format("Column {} not found in any Table", colId.getColumn()));
        }
    }

    return std::make_unique<algebra::IURef>(iu);
}
//------------------------------------------------------------
std::unique_ptr<algebra::Expression> SemanticAnalysis::analyzeLiteral(const parser::Literal& literal, Database& db, const algebra::Scope& scope) {
    auto literalVal = static_cast<std::string>(literal.getLiteralValue());

    return std::make_unique<algebra::Const>(literalVal);
}
//------------------------------------------------------------


} // namespace imlab::semana