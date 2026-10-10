//---------------------------------------------------------------------------
#include "imlab/algebra/BinaryExpression.h"

#include "Const.h"

#include <iostream>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
BinaryExpression::BinaryExpression(Op op, unique_ptr<Expression>&& left, unique_ptr<Expression>&& right) : Expression(), left_(std::move(left)), right_(std::move(right)), op_(op) {
    if (left_->getType().getClass() != right_->getType().getClass()) {
        throw std::runtime_error("left_ and right_ of a BinaryExpression must have the same type");
    }
}
//---------------------------------------------------------------------------
std::set<const IU*> BinaryExpression::collectIUs() const {
    if (!left_) {
        throw std::runtime_error("BinaryExpression::collectIUs: left_ is null");
    }
    if (!right_) {
        throw std::runtime_error("BinaryExpression::collectIUs: right_ is null");
    }

    std::set<const IU*> iusLeft = left_->collectIUs();
    std::set<const IU*> iusRight = right_->collectIUs();

    iusLeft.insert(iusRight.begin(), iusRight.end());

    return iusLeft;
}
//---------------------------------------------------------------------------
void BinaryExpression::produce(ostream& out) const {
    auto emit = [&](const Expression& expr) {
        expr.produce(out);
        if (expr.getExpressionType() == ExpressionType::IURef) {
            out << "." << getAccessorFunction(expr.getType());
        }
    };
    emit(*left_);
    out << " " << opToString(op_) << " ";
    emit(*right_);
}

void BinaryExpression::assignVarNames(const std::map<const IU*, std::string>& varNames) {
    left_->assignVarNames(varNames);
    right_->assignVarNames(varNames);
}

std::string BinaryExpression::opToString(Op op)  {
    switch (op) {
        case Equal:
            return "==";
        case And:
            return "&&";
        default:
            return "unknown op";
    }
}


//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
