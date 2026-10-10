//---------------------------------------------------------------------------
#include "imlab/algebra/Print.h"
#include "imlab/algebra/IU.h"
#include "imlab/infra/Helper.h"
#include "imlab/algebra/Operator.h"
#include "imlab/algebra/Expression.h"

#include <iostream>
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
Print::Print(std::unique_ptr<Operator> child, std::vector<Output> output) : UnaryOperator(std::move(child)), output_(std::move(output)) {}
//---------------------------------------------------------------------------
std::set<const IU*> Print::collectIUs() {
    std::set<const IU*> ius;
    for (const auto& out : output_) {
        auto exprIUs = out.expr_->collectIUs();
        ius.insert(exprIUs.begin(), exprIUs.end());
    }
    return ius;
}
//---------------------------------------------------------------------------
void Print::prepare(const std::set<const IU*>& required, Operator* consumer) {
    requiredIUs_ = collectIUs();
    requiredIUs_.insert(required.begin(), required.end());

    consumer_ = consumer;

    input_->prepare(requiredIUs_, this);

    const auto& childVarNames = input_->getVarNames();
    for (const auto& out : output_) {
        out.expr_->assignVarNames(childVarNames);
    }
}
//---------------------------------------------------------------------------
void Print::produce(std::ostream& code) {
    code << "   // start Print::produce\n";
    code << "   std::cout << ";

    for (size_t i = 0; i < output_.size(); ++i) {
        code << "\"" << output_[i].name_ << "\"";
        if (i < output_.size() - 1) {
            code << " << \", \" << ";
        }
    }

    code << " << std::endl;\n";

    input_->produce(code);

    code << "   // end Print::produce\n";
}
//---------------------------------------------------------------------------
void Print::consume(std::ostream& code, const Operator* child) {
    code << "               // start Print::consume\n";
    code << "               std::cout << ";

    for (size_t i = 0; i < output_.size(); ++i) {
        const auto& type = output_[i].expr_->getType();
        if (type.getClass() == imlab::Type::kNumeric) {
            code << "imlab::Numeric<" << type.getPrecision() << ", " << type.getScale() << ">(";
            output_[i].expr_->produce(code);
            code << ".getNumericRawValue())";
        } else {
            output_[i].expr_->produce(code);
            code << "." << Expression::getAccessorFunction(type);
        }
        if (i < output_.size() - 1) {
            code << " << \", \" << ";
        }
    }

    code << " << std::endl;\n";
    code << "               // end Print::consume\n";
}
//---------------------------------------------------------------------------
void Print::optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) {

    UnaryOperator::optimize(thisRef, pass);

}
//---------------------------------------------------------------------------



} // namespace imlab::algebra
//---------------------------------------------------------------------------