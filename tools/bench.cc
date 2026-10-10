#include "imlab/Database.h"
#include "imlab/algebra/InnerJoin.h"
#include "imlab/algebra/Operator.h"
#include "imlab/algebra/TableScan.h"
#include "imlab/parser/ParseContext.h"
#include "imlab/semana/SemanticAnalysis.h"
#include "imlab/statement/QueryStatement.h"
#include "imlab/statement/Statement.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace imlab;

namespace {

class NullBuffer : public std::streambuf {
    protected:
    int overflow(int ch) override { return ch; }
};

class SilenceCout {
    public:
    SilenceCout() : previous_(std::cout.rdbuf(&buffer_)) {}
    ~SilenceCout() { std::cout.rdbuf(previous_); }

    private:
    NullBuffer buffer_;
    std::streambuf* previous_;
};

std::unique_ptr<statement::Statement> build(Database& db, const std::string& sql) {
    std::istringstream in(sql);
    parser::ParseContext context;
    auto trees = context.Parse(in);
    semana::SemanticAnalysis analysis;
    return analysis.buildStatement(*trees.at(0), db);
}

void exec(Database& db, const std::string& sql) {
    SilenceCout silence;
    build(db, sql)->run(db);
}

void fillIntegers(Database& db, const std::string& table, size_t rows, int32_t (*value)(size_t)) {
    for (size_t i = 0; i < rows; ++i) {
        db.insertRow(table, {Value(Integer(value(i)))});
    }
}

void fillPairs(Database& db, const std::string& table, size_t rows, int32_t (*key)(size_t), int32_t (*value)(size_t)) {
    for (size_t i = 0; i < rows; ++i) {
        db.insertRow(table, {Value(Integer(key(i))), Value(Integer(value(i)))});
    }
}

struct Result {
    std::string name;
    size_t rows = 0;
    double compileMs = 0;
    double executeMs = 0;
    bool cacheHit = false;
    std::string note;
};

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
    return dynamic_cast<const algebra::TableScan*>(op);
}

Result measure(Database& db, const std::string& name, size_t rows, const std::string& sql, const std::string& note) {
    auto statement = build(db, sql);
    {
        SilenceCout silence;
        statement->run(db);
    }
    auto* query = dynamic_cast<statement::QueryStatement*>(statement.get());
    Result result;
    result.name = name;
    result.rows = rows;
    result.compileMs = query->profile().compileMs;
    result.executeMs = query->profile().executeMs;
    result.cacheHit = query->profile().cacheHit;
    result.note = note;
    const auto* scan = buildScan(query->getTree());
    if (note.empty() && scan != nullptr) {
        result.note = "build=" + scan->getTableName();
    }
    return result;
}

void printResult(const Result& result) {
    const double seconds = result.executeMs / 1000.0;
    const double rowsPerSec = seconds > 0 ? static_cast<double>(result.rows) / seconds : 0;
    std::cout << std::left << std::setw(18) << result.name
              << std::right << std::setw(10) << result.rows
              << std::setw(14) << std::fixed << std::setprecision(1) << result.compileMs
              << std::setw(14) << result.executeMs
              << std::setw(14) << std::setprecision(0) << rowsPerSec
              << std::setw(8) << (result.cacheHit ? "hit" : "miss")
              << "  " << result.note << '\n';
}

} // namespace

int main() {
    Database db;
    std::vector<Result> results;

    exec(db, "create table s10k (a integer not null);");
    exec(db, "create table s1m (a integer not null);");
    fillIntegers(db, "s10k", 10000, [](size_t i) { return static_cast<int32_t>(i); });
    fillIntegers(db, "s1m", 1000000, [](size_t i) { return static_cast<int32_t>(i); });

    results.push_back(measure(db, "scan", 10000, "select a from s10k;", "cold"));
    results.push_back(measure(db, "scan", 10000, "select a from s10k;", "warm"));
    results.push_back(measure(db, "scan", 1000000, "select a from s1m;", "cold"));
    results.push_back(measure(db, "filter", 10000, "select a from s10k where a = 1;", "cold"));
    results.push_back(measure(db, "filter", 1000000, "select a from s1m where a = 1;", "cold"));

    exec(db, "create table j10k_l (a integer not null);");
    exec(db, "create table j10k_r (b integer not null);");
    exec(db, "create table j100k_l (a integer not null);");
    exec(db, "create table j100k_r (b integer not null);");
    fillIntegers(db, "j10k_l", 10000, [](size_t i) { return static_cast<int32_t>(i); });
    fillIntegers(db, "j10k_r", 10000, [](size_t i) { return static_cast<int32_t>(i); });
    fillIntegers(db, "j100k_l", 100000, [](size_t i) { return static_cast<int32_t>(i); });
    fillIntegers(db, "j100k_r", 100000, [](size_t i) { return static_cast<int32_t>(i); });
    results.push_back(measure(db, "join", 20000, "select a from j10k_l l, j10k_r r where l.a = r.b;", "10k x 10k"));
    results.push_back(measure(db, "join", 200000, "select a from j100k_l l, j100k_r r where l.a = r.b;", "100k x 100k"));

    constexpr size_t kSmall = 2000;
    constexpr size_t kLarge = 200000;
    exec(db, "create table small (a integer not null);");
    exec(db, "create table large (b integer not null);");
    fillIntegers(db, "small", kSmall, [](size_t i) { return static_cast<int32_t>(i); });
    fillIntegers(db, "large", kLarge, [](size_t i) { return static_cast<int32_t>(i); });

    statement::QueryStatement::setBuildSideSelection(false);
    results.push_back(measure(db, "join-large-build", kSmall + kLarge,
                               "select a from large l, small s where l.b = s.a;", "build forced to large"));
    results.push_back(measure(db, "join-small-build", kSmall + kLarge,
                               "select a from small s, large l where s.a = l.b;", "build forced to small"));
    statement::QueryStatement::setBuildSideSelection(true);
    results.push_back(measure(db, "join-auto-build", kSmall + kLarge,
                               "select a from large l, small s where l.b = s.a;", ""));

    exec(db, "create table g (k integer not null, v integer not null);");
    fillPairs(db, "g", 200000,
              [](size_t i) { return static_cast<int32_t>(i % 100); },
              [](size_t) { return static_cast<int32_t>(1); });
    results.push_back(measure(db, "groupby", 200000, "select k, count(*), sum(v) from g group by k;", "100 groups"));

    std::cout << std::left << std::setw(18) << "query"
              << std::right << std::setw(10) << "rows"
              << std::setw(14) << "compile_ms"
              << std::setw(14) << "execute_ms"
              << std::setw(14) << "rows_per_s"
              << std::setw(8) << "cache"
              << "  note\n";
    for (const auto& result : results) {
        printResult(result);
    }
    return 0;
}
