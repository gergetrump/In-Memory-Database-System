//---------------------------------------------------------------------------
#include "imlab/algebra/Select.h"

#include "BinaryExpression.h"
#include "InnerJoin.h"

#include <iostream>
#include <set>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
Select::Select(unique_ptr<Operator> child, unique_ptr<algebra::Expression> expression) : UnaryOperator(std::move(child)), expression_(std::move(expression)) {}
//---------------------------------------------------------------------------
std::set<const IU*> Select::collectIUs() {
    auto childIUs = input_->collectIUs();
    auto exprIUs = expression_->collectIUs();
    childIUs.insert(exprIUs.begin(), exprIUs.end());
    return childIUs;
}
//---------------------------------------------------------------------------
void Select::prepare(const std::set<const IU*>& required, Operator* consumer) {
    consumer_ = consumer;
    requiredIUs_ = required;

    auto exprIUs = expression_->collectIUs();
    requiredIUs_.insert(exprIUs.begin(), exprIUs.end());

    input_->prepare(requiredIUs_, this);

    const auto& childVarNames = input_->getVarNames();
    expression_->assignVarNames(childVarNames);

    varNames_ = childVarNames;
}
//---------------------------------------------------------------------------
void Select::produce(std::ostream& code) {
    code << "   // start Select::produce\n";
    input_->produce(code);
    code << "   // end Select::produce\n";
}
//---------------------------------------------------------------------------
void Select::consume(std::ostream& code, const Operator* child) {
    code << "           // start Select::consume\n";
    code << "           if (";
    expression_->produce(code);
    code << "   ) {\n";

    consumer_->consume(code, this);

    code << "           }\n";
    code << "           // end Select::consume\n";
}

//---------------------------------------------------------------------------



std::optional<size_t> Select::estimateCardinality() const {
    return input_ ? input_->estimateCardinality() : std::nullopt;
}

void Select::optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) {

    UnaryOperator::optimize(thisRef, pass);

    if (pass != OptimizerPass::PredicatePushdown) {
        return;
    }

    if (input_->pushPredicate(expression_, pass)) {
        auto* nested = dynamic_cast<Select*>(input_.get());
        if (nested != nullptr && nested->gavePredicateToChild()) {
            thisRef = nested->releaseInput();
        } else {
            thisRef = std::move(input_);
        }
    }
}

void Select::mergePredicate(std::unique_ptr<Expression> newPredicate) {
    expression_ = std::make_unique<BinaryExpression>(
        BinaryExpression::Op::And,
        std::move(expression_),
        std::move(newPredicate)
    );
}

bool Select::pushPredicate(std::unique_ptr<Expression>& predicate, OptimizerPass pass) {
    mergePredicate(std::move(predicate));
    if (input_->pushPredicate(expression_, pass)) {
        auto* nested = dynamic_cast<Select*>(input_.get());
        if (nested != nullptr && nested->gavePredicateToChild()) {
            input_ = nested->releaseInput();
            gavePredicateToChild_ = true;
        } else if (nested == nullptr) {
            gavePredicateToChild_ = true;
        }
    }
    return true;
}



//---------------------------------------------------------------------------




} // namespace imlab::algebra
//---------------------------------------------------------------------------
