//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
#ifndef IMLAB_ALGEBRA_INNERJOIN_H
#define IMLAB_ALGEBRA_INNERJOIN_H
//---------------------------------------------------------------------------
#include "imlab/algebra/Expression.h"
#include "imlab/algebra/IU.h"
#include "imlab/algebra/Operator.h"
#include <memory>
//---------------------------------------------------------------------------
namespace imlab::algebra {class BinaryExpression;
//---------------------------------------------------------------------------
class InnerJoin final : public BinaryOperator {
    public:
    /// Constructor
    InnerJoin(std::unique_ptr<Operator> left, std::unique_ptr<Operator> right, std::unique_ptr<algebra::Expression> condition);
    /// Destructor
    ~InnerJoin() override = default;

    /// Collect all IUs produced by the operator
    std::set<const IU*> collectIUs() override;

    /// Prepare the operator
    void prepare(const std::set<const IU*>& required, Operator* consumer) override;
    /// Produce tuple
    void produce(std::ostream& code) override;
    /// Consume tuple
    void consume(std::ostream& code, const Operator* child) override;

    /// Get the operator type
    OperatorType getOperatorType() const override { return OperatorType::InnerJoin; }

    /// Perform optimization
    void optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) override;


    const std::map<const IU*, std::string>& getVarNames() const override { return varNames_; }

    void appendCondition(std::unique_ptr<Expression> newCondition);

    /// Condition
    std::unique_ptr<Expression> condition_;
    std::map<const IU*, Value> currentTuple_;
    std::vector<std::map<const IU*, Value>> rightTuples_;
    bool leftTupleValid_ = false;
    const IU* leftJoinIU_;
    const IU* rightJoinIU_;
    int codegenId_ = 0;

    std::map<const IU*, Value> combineTuples(
      const std::map<const IU*, Value>& leftTuple,
      const std::map<const IU*, Value>& rightTuple) const;

    std::string getConditionsAsString() const {
      if (condition_) {
        return condition_->toString();
      }
      return "No Conditions";
    }

    bool pushPredicate(std::unique_ptr<Expression>& predicate, OptimizerPass pass) override;
};
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
#endif // IMLAB_ALGEBRA_INNERJOIN_H
//---------------------------------------------------------------------------
