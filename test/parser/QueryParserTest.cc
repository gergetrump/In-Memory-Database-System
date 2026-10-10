// ---------------------------------------------------------------------------
// IMLAB
// ---------------------------------------------------------------------------

#include "imlab/parser/AST.h"
#include "imlab/parser/ParseContext.h"
#include <sstream>
#include <vector>
#include <gtest/gtest.h>

using Type = imlab::Type;
using ParseContext = imlab::parser::ParseContext;

namespace test {

namespace {
void validateBinaryExpression(const imlab::parser::AST* ast, auto leftValidation, auto rightValidation) {
    ASSERT_EQ(ast->getType(), imlab::parser::AST::BinaryExpressionType);
    auto exprPtr = static_cast<const imlab::parser::BinaryExpression*>(ast);
    leftValidation(exprPtr);
    rightValidation(exprPtr);
}

void validateTableRef(const imlab::parser::AST* ast, std::string_view table, std::string_view alias) {
    ASSERT_EQ(ast->getType(), imlab::parser::AST::TableRefType);
    auto tableRefPtr = static_cast<const imlab::parser::TableRef*>(ast);
    ASSERT_EQ(tableRefPtr->getTable(), table);
    ASSERT_EQ(tableRefPtr->getAlias(), alias);
}

void validateColdId(const imlab::parser::AST* ast, std::string_view table, std::string_view column) {
    ASSERT_EQ(ast->getType(), imlab::parser::AST::ColIdType);
    auto colIdPtr = static_cast<const imlab::parser::ColId*>(ast);
    ASSERT_EQ(colIdPtr->getTable(), table);
    ASSERT_EQ(colIdPtr->getColumn(), column);
}

void validateLiteral(const imlab::parser::AST* ast, std::string_view literal) {
    ASSERT_EQ(ast->getType(), imlab::parser::AST::LiteralType);
    auto literalPtr = static_cast<const imlab::parser::Literal*>(ast);
    ASSERT_EQ(literalPtr->getLiteralValue(), literal);
}
} // namespace

TEST(ParserTest, QuerySingleTableNoAlias) {
    std::istringstream in("select * from students;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 0);

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QuerySingleTableNoAliasOneTarget) {
    std::istringstream in("select matrnr from students;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 1);
    validateColdId(queryPtr->getTargets()[0], "", "matrnr");

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QuerySingleTableNoAliasMultipleTargets) {
    std::istringstream in("select matrnr, name from students;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 2);
    validateColdId(queryPtr->getTargets()[0], "", "matrnr");
    validateColdId(queryPtr->getTargets()[1], "", "name");

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QuerySingleTableWithAlias) {
    std::istringstream in("select * from students s;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 0);

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "s");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QuerySingleTableWithAliasOneTarget) {
    std::istringstream in("select s.matrnr from students s;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 1);
    validateColdId(queryPtr->getTargets()[0], "s", "matrnr");

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "s");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QuerySingleTableWithAliasMultipleTargets) {
    std::istringstream in("select s.matrnr, s.name from students s;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 2);
    validateColdId(queryPtr->getTargets()[0], "s", "matrnr");
    validateColdId(queryPtr->getTargets()[1], "s", "name");

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "s");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QueryMultipleTablesNoAlias) {
    std::istringstream in("select * from students, attend, lectures;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 0);

    ASSERT_EQ(queryPtr->getFrom().size(), 3);
    validateTableRef(queryPtr->getFrom()[0], "students", "");
    validateTableRef(queryPtr->getFrom()[1], "attend", "");
    validateTableRef(queryPtr->getFrom()[2], "lectures", "");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QueryMultipleTablesWithAlias) {
    std::istringstream in("select * from students s, attend a, lectures l;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 0);

    ASSERT_EQ(queryPtr->getFrom().size(), 3);
    validateTableRef(queryPtr->getFrom()[0], "students", "s");
    validateTableRef(queryPtr->getFrom()[1], "attend", "a");
    validateTableRef(queryPtr->getFrom()[2], "lectures", "l");

    ASSERT_EQ(queryPtr->getWhere().size(), 0);
}

TEST(ParserTest, QuerySingleTableWithOneSelection) {
    std::istringstream in("select * from students s where s.matrnr = 1234;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 0);

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "s");

    ASSERT_EQ(queryPtr->getWhere().size(), 1);
    validateBinaryExpression(queryPtr->getWhere()[0], [](const imlab::parser::BinaryExpression* ast) { validateColdId(ast->getLeft(), "s", "matrnr"); }, [](const imlab::parser::BinaryExpression* ast) { validateLiteral(ast->getRight(), "1234"); });
}

TEST(ParserTest, QuerySingleTableWithMultipleSelections) {
    std::istringstream in("select * from students s where s.name = 'foo' and s.matrnr = 1234;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    ASSERT_EQ(parseTrees[0]->getType(), imlab::parser::AST::QueryType);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 0);

    ASSERT_EQ(queryPtr->getFrom().size(), 1);
    validateTableRef(queryPtr->getFrom()[0], "students", "s");

    ASSERT_EQ(queryPtr->getWhere().size(), 2);
    validateBinaryExpression(queryPtr->getWhere()[0], [](const imlab::parser::BinaryExpression* ast) { validateColdId(ast->getLeft(), "s", "name"); }, [](const imlab::parser::BinaryExpression* ast) { validateLiteral(ast->getRight(), "foo"); });

    validateBinaryExpression(queryPtr->getWhere()[1], [](const imlab::parser::BinaryExpression* ast) { validateColdId(ast->getLeft(), "s", "matrnr"); }, [](const imlab::parser::BinaryExpression* ast) { validateLiteral(ast->getRight(), "1234"); });
}

TEST(ParserTest, QueryGroupByCountAndSum) {
    std::istringstream in("select k, count(*), sum(v) from t where t.k = 1 group by k;");
    ParseContext spc;
    auto parseTrees = spc.Parse(in);
    ASSERT_EQ(parseTrees.size(), 1);
    auto queryPtr = static_cast<const imlab::parser::Query*>(parseTrees[0].get());

    ASSERT_EQ(queryPtr->getTargets().size(), 3);
    validateColdId(queryPtr->getTargets()[0], "", "k");
    ASSERT_EQ(queryPtr->getTargets()[1]->getType(), imlab::parser::AST::AggregateType);
    auto* count = static_cast<const imlab::parser::Aggregate*>(queryPtr->getTargets()[1]);
    ASSERT_EQ(count->getKind(), imlab::parser::Aggregate::CountStar);
    ASSERT_EQ(queryPtr->getTargets()[2]->getType(), imlab::parser::AST::AggregateType);
    auto* sum = static_cast<const imlab::parser::Aggregate*>(queryPtr->getTargets()[2]);
    ASSERT_EQ(sum->getKind(), imlab::parser::Aggregate::Sum);
    validateColdId(sum->getArgument(), "", "v");

    ASSERT_EQ(queryPtr->getGroupBy().size(), 1);
    validateColdId(queryPtr->getGroupBy()[0], "", "k");
    ASSERT_EQ(queryPtr->getWhere().size(), 1);
}

} // namespace test
