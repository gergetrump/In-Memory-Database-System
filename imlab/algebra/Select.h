//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
#ifndef IMLAB_ALGEBRA_SELECTION_H
#define IMLAB_ALGEBRA_SELECTION_H
//---------------------------------------------------------------------------
#include "../../imlab/algebra/Operator.h"
#include "../../imlab/infra/Types.h"
#include "../../imlab/algebra/Expression.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <ostream>
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
class Select final : public UnaryOperator {
   protected:
   /// Predicate
   std::unique_ptr<algebra::Expression> expression_;

   public:
   /// Constructor
   Select(std::unique_ptr<Operator> child, std::unique_ptr<algebra::Expression> expression);
   /// Destructor
   ~Select() override = default;

   /// Collect all IUs produced by the operator
   std::set<const IU*> collectIUs() override;

   /// Prepare the operator
   void prepare(const std::set<const IU*>& required, Operator* consumer) override;
   /// Produce tuple
   void produce(std::ostream& code) override;
   /// Consume tuple
   void consume(std::ostream& code, const Operator* child) override;

   /// Get the operator type
   OperatorType getOperatorType() const override { return OperatorType::Select; }

   /// Perform optimization
   void optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) override;

   std::optional<size_t> estimateCardinality() const override;

   const Expression* getExpression() const {
      return expression_.get();
   }

   void mergePredicate(std::unique_ptr<Expression> newPredicate);

   bool pushPredicate(std::unique_ptr<Expression>& predicate, OptimizerPass pass) override;

   bool gavePredicateToChild() const { return gavePredicateToChild_; }

   private:
   bool gavePredicateToChild_ = false;
};
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
#endif // IMLAB_ALGEBRA_SELECTION_H
//---------------------------------------------------------------------------
