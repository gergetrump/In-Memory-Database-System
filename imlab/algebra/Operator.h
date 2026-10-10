//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
#ifndef IMLAB_ALGEBRA_OPERATOR_H
#define IMLAB_ALGEBRA_OPERATOR_H
//---------------------------------------------------------------------------
#include "BinaryExpression.h"
#include "Expression.h"
#include "imlab/algebra/IU.h"
#include "imlab/optimizer/OptimizerPass.h"
#include <memory>
#include <optional>
#include <set>
#include "imlab/types/Types.h"
#include "imlab/Database.h"
//---------------------------------------------------------------------------

namespace imlab::algebra {
class Expression;
}

namespace imlab::algebra {


//---------------------------------------------------------------------------
class Operator {
    public:
    enum class OperatorType : unsigned {
        InnerJoin,
        Print,
        Select,
        TableScan,
        SubqueryScan,
        GroupBy,
    };
    enum class OperatorCategory : unsigned {
        Unary,
        Binary,
        Scan,
    };

    /// Consumer
    Operator* consumer_;
    /// The required ius
    std::set<const IU*> requiredIUs_;

    std::map<const IU*, std::string> varNames_;

    /// Constructor
    Operator() = default;
    /// Destructor
    virtual ~Operator() = default;

    /// Collect all IUs produced by the operator
    virtual std::set<const IU*> collectIUs() = 0;

    /// Prepare the operator
    virtual void prepare(const std::set<const IU*>& required, Operator* parent) = 0;
    /// Produce tuple
    virtual void produce(std::ostream& code) = 0;
    /// Consume tuple
    virtual void consume(std::ostream& code, const Operator* child) = 0;

    /// Get the operator type
    virtual OperatorType getOperatorType() const = 0;
    /// Get the operator category
    virtual OperatorCategory getOperatorCategory() const = 0;

    /// Perform optimization
    virtual void optimize(std::unique_ptr<Operator>& thisRef, OptimizerPass pass) = 0;

    /// Known input cardinality, when the operator can see it
    virtual std::optional<size_t> estimateCardinality() const { return std::nullopt; }

    virtual const std::map<const IU*, std::string>& getVarNames() const { return varNames_; }

    static int& codegenSerial() {
        static int value = 0;
        return value;
    }
    static void resetCodegenIds() { codegenSerial() = 0; }
    static int nextCodegenId() { return codegenSerial()++; }

    std::string getOperatorTypeName() const {
        switch (this->getOperatorType()) {
            case OperatorType::TableScan: return "TableScan";
            case OperatorType::Select: return "Select";
            case OperatorType::InnerJoin: return "InnerJoin";
            case OperatorType::Print: return "Print";
            case OperatorType::GroupBy: return "GroupBy";
            default: return "Unknown Operator";
        }
    }

    virtual bool pushPredicate(std::unique_ptr<Expression>& predicate, OptimizerPass pass) {
        return false;
    }

    std::vector<std::unique_ptr<Expression>> splitConditions(std::unique_ptr<Expression> expression) {
        std::vector<std::unique_ptr<Expression>> conditions;

        auto binaryExpr = static_cast<BinaryExpression*>(expression.get());
        if (binaryExpr && binaryExpr->getOp() == BinaryExpression::And) {

            std::unique_ptr<Expression> leftSubExpr = std::move(binaryExpr->left_);
            std::unique_ptr<Expression> rightSubExpr = std::move(binaryExpr->right_);

            auto leftConditions = splitConditions(std::move(leftSubExpr));
            auto rightConditions = splitConditions(std::move(rightSubExpr));

            for (auto& cond : leftConditions) {
                conditions.push_back(std::move(cond));
            }
            for (auto& cond : rightConditions) {
                conditions.push_back(std::move(cond));
            }
        } else {
            conditions.push_back(std::move(expression));
        }
        return conditions;
    }

    std::unique_ptr<Expression> combineConditions(std::vector<std::unique_ptr<Expression>> conditions) {
        if (conditions.empty()) {
            return nullptr;
        }

        if (conditions.size() == 1) {
            return std::move(conditions[0]);
        }

        std::unique_ptr<Expression> combinedConds;
        for (auto& cond : conditions) {
            if (!combinedConds) {
                combinedConds = std::move(cond);
            } else {
                combinedConds = std::make_unique<BinaryExpression>(
                BinaryExpression::Op::And,
                std::move(combinedConds),
                std::move(cond)
            );
            }
        }
        return combinedConds;
    }

};
//---------------------------------------------------------------------------
class UnaryOperator : public Operator {
    public:
    /// Constructor
    explicit UnaryOperator(std::unique_ptr<Operator> input) : Operator(), input_(std::move(input)) {}
    /// Destructor
    ~UnaryOperator() override = default;

    /// Get input ptr
    Operator* getInput() const { return input_.get(); }
    /// Release the input operator
    std::unique_ptr<Operator> releaseInput() { return std::move(input_); }
    /// Get reference to input operator
    std::unique_ptr<Operator>& getInputRef() { return input_; }
    /// Set a new input operator
    void setInput(std::unique_ptr<Operator> in) { input_ = std::move(in); }

    /// Get the operator category
    OperatorCategory getOperatorCategory() const override { return OperatorCategory::Unary; }

    void optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) override {
        if (input_) {
            input_->optimize(input_, pass);
        }
    }

    protected:
    /// The input operator
    std::unique_ptr<Operator> input_;
};
//---------------------------------------------------------------------------
class BinaryOperator : public Operator {
    public:
    /// Constructor
    BinaryOperator(std::unique_ptr<Operator> left, std::unique_ptr<Operator> right) : Operator(), left_(std::move(left)), right_(std::move(right)) {}
    /// Destructor
    ~BinaryOperator() override = default;

    /// Get the ptr to the left operator
    Operator* getLeft() const { return left_.get(); }
    /// Get the ptr to the right operator
    Operator* getRight() const { return right_.get(); }
    /// Release the left ptr
    std::unique_ptr<Operator> releaseLeft() { return std::move(left_); }
    /// Release the right ptr
    std::unique_ptr<Operator> releaseRight() { return std::move(right_); }
    /// Get reference to left operator
    std::unique_ptr<Operator>& getLeftRef() { return left_; }
    /// Get reference to right operator
    std::unique_ptr<Operator>& getRightRef() { return right_; }
    /// Set a new left operator
    void setLeft(std::unique_ptr<Operator> input) { left_ = std::move(input); }
    /// Set a new right operator
    void setRight(std::unique_ptr<Operator> input) { right_ = std::move(input); }

    /// Get the operator category
    OperatorCategory getOperatorCategory() const override { return OperatorCategory::Binary; }

    void optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) override {
        if (left_) {
            left_->optimize(left_, pass);
        }
        if (right_) {
            right_->optimize(right_, pass);
        }
    }

    protected:
    /// The left operator
    std::unique_ptr<Operator> left_;
    /// The right operator
    std::unique_ptr<Operator> right_;
};
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
#endif // IMLAB_ALGEBRA_OPERATOR_H
//---------------------------------------------------------------------------
