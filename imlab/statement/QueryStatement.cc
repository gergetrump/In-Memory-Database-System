//---------------------------------------------------------------------------
#include "imlab/statement/QueryStatement.h"

#include "CodeGenHelper.h"
#include "imlab/algebra/Operator.h"
#include "imlab/statement/Statement.h"
#include <chrono>
#include <sstream>


//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace imlab::statement {
//---------------------------------------------------------------------------
QueryStatement::QueryStatement(unique_ptr<algebra::Operator> tree) : Statement(Statement::Type::QueryStatementType), tree(std::move(tree)) {}
//---------------------------------------------------------------------------
QueryStatement::~QueryStatement() = default;
//---------------------------------------------------------------------------
bool QueryStatement::buildSideSelectionEnabled_ = true;
//---------------------------------------------------------------------------
void QueryStatement::setBuildSideSelection(bool enabled) { buildSideSelectionEnabled_ = enabled; }
//---------------------------------------------------------------------------
bool QueryStatement::buildSideSelection() { return buildSideSelectionEnabled_; }
//---------------------------------------------------------------------------
void QueryStatement::run(Database& db) {
    optimize(imlab::OptimizerPass::PredicatePushdown);
    if (buildSideSelectionEnabled_) {
        optimize(imlab::OptimizerPass::BuildSideSelection);
    }

    std::set<const algebra::IU*> requiredIUs;
    tree->prepare(requiredIUs, nullptr);

    std::ostringstream code;
    code << "// start QueryStatement::run\n";
    code << "#include <algorithm>\n";
    code << "#include <cstdint>\n";
    code << "#include <functional>\n";
    code << "#include <iostream>\n";
    code << "#include <string>\n";
    code << "#include <unordered_map>\n";
    code << "#include <vector>\n";
    code << "#include \"imlab/Database.h\"\n";
    code << "#include \"imlab/infra/HashTable.h\"\n";
    code << "#include \"imlab/infra/Types.h\"\n";
    code << "#include \"imlab/runtime/RuntimeException.h\"\n";
    code << "#include <tbb/parallel_for.h>\n";
    code << "extern \"C\" void execute(imlab::Database& db) {\n";
    algebra::Operator::resetCodegenIds();
    tree->produce(code);
    code << "}\n";
    code << "// end QueryStatement::run\n";

    CodeGenHelper helper(code.str());
    const auto loaded = helper.getExecute();
    profile_.compileMs = loaded.compileMs;
    profile_.cacheHit = loaded.cacheHit;

    using Function = void (*)(Database&);
    auto* func = reinterpret_cast<Function>(loaded.func);
    const auto started = std::chrono::steady_clock::now();
    func(db);
    const auto finished = std::chrono::steady_clock::now();
    profile_.executeMs = std::chrono::duration<double, std::milli>(finished - started).count();
}

//---------------------------------------------------------------------------
void QueryStatement::optimize(imlab::OptimizerPass pass) {
    tree->optimize(tree, pass);
}
//---------------------------------------------------------------------------
} // namespace imlab::statement
//---------------------------------------------------------------------------