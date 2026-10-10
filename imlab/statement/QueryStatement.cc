//---------------------------------------------------------------------------
#include "imlab/statement/QueryStatement.h"

#include "CodeGenHelper.h"
#include "imlab/algebra/Operator.h"
#include "imlab/statement/Statement.h"
#include <sstream>
#include <fstream>
#include <dlfcn.h>


//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace imlab::statement {
//---------------------------------------------------------------------------
QueryStatement::QueryStatement(unique_ptr<algebra::Operator> tree) : Statement(Statement::Type::QueryStatementType), tree(std::move(tree)) {}
//---------------------------------------------------------------------------
QueryStatement::~QueryStatement() = default;
//---------------------------------------------------------------------------
void QueryStatement::run(Database& db) {
    optimize(imlab::OptimizerPass::PredicatePushdown);

    std::set<const algebra::IU*> requiredIUs;
    tree->prepare(requiredIUs, nullptr);

    std::ostringstream code;
    code << "// start QueryStatement::run\n";
    code << "#include <iostream>\n";
    code << "#include \"imlab/Database.h\"\n";
    code << "#include \"imlab/infra/Types.h\"\n";
    code << "#include \"imlab/infra/HashTable.h\"\n";
    code << "#include <tbb/parallel_for.h>\n";
    code << "#include <vector>\n";
    code << "#include <string>\n";
    code << "extern \"C\" void execute(imlab::Database& db) {\n";
    tree->produce(code);
    code << "}\n";
    code << "// end QueryStatement::run\n";


    CodeGenHelper helper(code.str());
    helper.compile();
    using Function = void(*)(Database&);
    Function func = reinterpret_cast<Function>(helper.loadFunction());
    func(db);
}

//---------------------------------------------------------------------------
void QueryStatement::optimize(imlab::OptimizerPass pass) {
    tree->optimize(tree, pass);
}
//---------------------------------------------------------------------------
} // namespace imlab::statement
//---------------------------------------------------------------------------