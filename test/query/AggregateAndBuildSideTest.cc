//---------------------------------------------------------------------------
#include "gtest/gtest.h"
#include "imlab/Database.h"
#include "imlab/algebra/InnerJoin.h"
#include "imlab/algebra/Operator.h"
#include "imlab/algebra/Print.h"
#include "imlab/algebra/TableScan.h"
#include "imlab/parser/ParseContext.h"
#include "imlab/runtime/RuntimeException.h"
#include "imlab/semana/SemanticAnalysis.h"
#include "imlab/statement/QueryStatement.h"
#include "imlab/statement/Statement.h"
#include <sstream>
#include <string>
//---------------------------------------------------------------------------
using namespace imlab;
//---------------------------------------------------------------------------
namespace {
class CaptureCout {
    public:
    CaptureCout() : previous_(std::cout.rdbuf(buffer_.rdbuf())) {}
    ~CaptureCout() { std::cout.rdbuf(previous_); }
    std::string str() const { return buffer_.str(); }

    private:
    std::stringstream buffer_;
    std::streambuf* previous_;
};

std::unique_ptr<statement::Statement> build(Database& db, const std::string& sql) {
    std::istringstream in(sql);
    parser::ParseContext context;
    auto trees = context.Parse(in);
    semana::SemanticAnalysis analysis;
    return analysis.buildStatement(*trees.at(0), db);
}

std::string run(Database& db, const std::string& sql) {
    CaptureCout capture;
    auto statement = build(db, sql);
    statement->run(db);
    return capture.str();
}

const algebra::TableScan* buildScan(const algebra::Operator* op) {
    while (op != nullptr && op->getOperatorType() != algebra::Operator::OperatorType::TableScan) {
        if (op->getOperatorType() == algebra::Operator::OperatorType::InnerJoin) {
            op = static_cast<const algebra::InnerJoin*>(op)->getLeft();
            continue;
        }
        const auto* unary = dynamic_cast<const algebra::UnaryOperator*>(op);
        if (unary == nullptr) {
            return nullptr;
        }
        op = unary->getInput();
    }
    return static_cast<const algebra::TableScan*>(op);
}
} // namespace
//---------------------------------------------------------------------------
namespace test {
//---------------------------------------------------------------------------
TEST(QueryFeatures, GroupByCountAndSum) {
    Database db;
    build(db, "create table t (k integer not null, v integer not null);")->run(db);
    db.insertRow("t", {Value(Integer(2)), Value(Integer(5))});
    db.insertRow("t", {Value(Integer(1)), Value(Integer(10))});
    db.insertRow("t", {Value(Integer(1)), Value(Integer(20))});

    EXPECT_EQ(run(db, "select k, count(*), sum(v) from t group by k;"),
              "k, count, sum_v\n1, 2, 30\n2, 1, 5\n");
    EXPECT_EQ(run(db, "select count(*), sum(v) from t;"), "count, sum_v\n3, 35\n");
    EXPECT_EQ(run(db, "select k, count(*) from t where k = 1 group by k;"), "k, count\n1, 2\n");
    EXPECT_THROW(build(db, "select k, count(*) from t;"), RuntimeException);
    EXPECT_THROW(build(db, "select count(*) from (select k from t group by k) s;"), RuntimeException);
}
//---------------------------------------------------------------------------
TEST(QueryFeatures, BuildSmallerSide) {
    Database db;
    build(db, "create table large (b integer not null);")->run(db);
    build(db, "create table small (a integer not null);")->run(db);
    db.insertRow("small", {Value(Integer(1))});
    db.insertRow("small", {Value(Integer(2))});
    db.insertRow("large", {Value(Integer(2))});
    db.insertRow("large", {Value(Integer(2))});
    db.insertRow("large", {Value(Integer(9))});
    db.insertRow("large", {Value(Integer(9))});
    db.insertRow("large", {Value(Integer(9))});

    auto statement = build(db, "select * from large l, small s where l.b = s.a;");
    std::string output;
    {
        CaptureCout capture;
        statement->run(db);
        output = capture.str();
    }
    EXPECT_EQ(output, "b, a\n2, 2\n2, 2\n");

    auto* query = dynamic_cast<statement::QueryStatement*>(statement.get());
    ASSERT_NE(query, nullptr);
    const auto* scan = buildScan(query->getTree());
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->getTableName(), "small");
}
//---------------------------------------------------------------------------
TEST(QueryFeatures, MorselScanKeepsInputOrder) {
    Database db;
    build(db, "create table seq (a integer not null);")->run(db);
    for (int32_t value = 1; value <= 8; ++value) {
        db.insertRow("seq", {Value(Integer(value))});
    }
    EXPECT_EQ(run(db, "select a from seq;"), "a\n1\n2\n3\n4\n5\n6\n7\n8\n");
}
//---------------------------------------------------------------------------
TEST(QueryFeatures, PrimaryKeyLookup) {
    Database db;
    build(db, "create table item (id integer not null, v integer not null, primary key (id));")->run(db);
    db.insertRow("item", {Value(Integer(1)), Value(Integer(10))});
    db.insertRow("item", {Value(Integer(2)), Value(Integer(20))});
    db.insertRow("item", {Value(Integer(3)), Value(Integer(30))});
    EXPECT_THROW(db.insertRow("item", {Value(Integer(2)), Value(Integer(99))}), RuntimeException);

    auto statement = build(db, "select v from item where id = 2;");
    std::string output;
    {
        CaptureCout capture;
        statement->run(db);
        output = capture.str();
    }
    EXPECT_EQ(output, "v\n20\n");
    auto* query = dynamic_cast<statement::QueryStatement*>(statement.get());
    ASSERT_NE(query, nullptr);
    auto* print = dynamic_cast<const algebra::Print*>(query->getTree());
    ASSERT_NE(print, nullptr);
    EXPECT_EQ(print->getInput()->getOperatorType(), algebra::Operator::OperatorType::TableScan);

    build(db, "create table pair (a integer not null, b integer not null, v integer not null, primary key (a, b));")->run(db);
    db.insertRow("pair", {Value(Integer(1)), Value(Integer(1)), Value(Integer(10))});
    db.insertRow("pair", {Value(Integer(1)), Value(Integer(2)), Value(Integer(20))});
    auto pairQuery = build(db, "select v from pair where b = 2 and a = 1;");
    EXPECT_EQ(run(db, "select v from pair where b = 2 and a = 1;"), "v\n20\n");
    auto* pairStatement = dynamic_cast<statement::QueryStatement*>(pairQuery.get());
    ASSERT_NE(pairStatement, nullptr);
    pairStatement->optimize(OptimizerPass::PredicatePushdown);
    auto* pairPrint = dynamic_cast<const algebra::Print*>(pairStatement->getTree());
    ASSERT_NE(pairPrint, nullptr);
    EXPECT_EQ(pairPrint->getInput()->getOperatorType(), algebra::Operator::OperatorType::TableScan);
}
//---------------------------------------------------------------------------
} // namespace test
//---------------------------------------------------------------------------
