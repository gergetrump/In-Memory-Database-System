//---------------------------------------------------------------------------
#ifndef IMLAB_STATEMENT_QUERYSTATEMENT_H
#define IMLAB_STATEMENT_QUERYSTATEMENT_H
//---------------------------------------------------------------------------
#include "../../imlab/statement/Statement.h"
#include <memory>
//---------------------------------------------------------------------------
namespace imlab {
//---------------------------------------------------------------------------
namespace algebra {
class Operator;
class Expression;
} // namespace algebra
//---------------------------------------------------------------------------
namespace statement {
//---------------------------------------------------------------------------
class QueryStatement : public Statement {
    public:
    explicit QueryStatement(std::unique_ptr<algebra::Operator>);
    ~QueryStatement() override;

    struct Profile {
        double compileMs = 0;
        double executeMs = 0;
        bool cacheHit = false;
    };

    void run(Database& db) override;

    const algebra::Operator* getTree() const { return tree.get(); }
    const Profile& profile() const { return profile_; }

    void optimize(imlab::OptimizerPass pass) override;

    static void setBuildSideSelection(bool enabled);
    static bool buildSideSelection();

    private:
    std::unique_ptr<algebra::Operator> tree;
    Profile profile_;
    static bool buildSideSelectionEnabled_;
};
//---------------------------------------------------------------------------
} // namespace statement
//---------------------------------------------------------------------------
} // namespace imlab
//---------------------------------------------------------------------------
#endif //IMLAB_STATEMENT_QUERYSTATEMENT_H
//---------------------------------------------------------------------------
