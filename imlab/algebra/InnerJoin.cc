//---------------------------------------------------------------------------
#include "imlab/algebra/InnerJoin.h"

#include "BinaryExpression.h"
#include "IURef.h"
#include "Select.h"
#include <utility>
//---------------------------------------------------------------------------
namespace imlab::algebra {
namespace {
bool alignJoinPredicate(Expression& expr, const std::set<const IU*>& leftIUs) {
    auto* binary = dynamic_cast<BinaryExpression*>(&expr);
    if (!binary || binary->getOp() != BinaryExpression::Op::Equal) {
        return false;
    }
    bool keyIsOnLeft = false;
    for (const IU* iu : binary->left_->collectIUs()) {
        keyIsOnLeft = leftIUs.find(iu) != leftIUs.end();
        if (!keyIsOnLeft) {
            break;
        }
    }
    if (!keyIsOnLeft) {
        std::swap(binary->left_, binary->right_);
    }
    return true;
}
} // namespace
//---------------------------------------------------------------------------
InnerJoin::InnerJoin(std::unique_ptr<Operator> left, std::unique_ptr<Operator> right, std::unique_ptr<algebra::Expression> condition) : BinaryOperator(std::move(left), std::move(right)), condition_(std::move(condition)) {}
//---------------------------------------------------------------------------
std::set<const IU*> InnerJoin::collectIUs() {
    auto leftIUs = left_->collectIUs();
    auto rightIUs = right_->collectIUs();
    leftIUs.insert(rightIUs.begin(), rightIUs.end());
    return leftIUs;
}
//---------------------------------------------------------------------------
void InnerJoin::prepare(const std::set<const IU*>& required, Operator* consumer) {
    consumer_ = consumer;

    auto conditionIUs = condition_->collectIUs();

    requiredIUs_.insert(required.begin(), required.end());
    requiredIUs_.insert(conditionIUs.begin(), conditionIUs.end());

    left_->prepare(requiredIUs_, this);
    right_->prepare(requiredIUs_, this);

    const auto& leftVarNames = left_->getVarNames();
    const auto& rightVarNames = right_->getVarNames();
    varNames_ = leftVarNames;
    varNames_.insert(rightVarNames.begin(), rightVarNames.end());

    auto binExpr = static_cast<BinaryExpression*>(condition_.get());
    if (!binExpr || binExpr->getOp() != BinaryExpression::Op::Equal) {
        throw RuntimeException("Only InnerJoin with Equal supported");
    }
    alignJoinPredicate(*condition_, left_->collectIUs());

    auto leftExpr = binExpr->left_.get();
    auto rightExpr = binExpr->right_.get();

    auto leftIURef = static_cast<IURef*>(leftExpr);
    auto rightIURef = static_cast<IURef*>(rightExpr);


    if (!leftIURef || !rightIURef) {
        throw RuntimeException("InnerJoin only works with Columns");
    }

    leftJoinIU_ = leftIURef->getIU();
    rightJoinIU_ = rightIURef->getIU();

    std::map<const IU*, std::string> varNames = leftVarNames;
    varNames.insert(rightVarNames.begin(), rightVarNames.end());

    condition_->assignVarNames(varNames);

    varNames_ = varNames;
}
//---------------------------------------------------------------------------
void InnerJoin::produce(std::ostream& code) {
    code << "   // start InnerJoin::produce\n";

    codegenId_ = Operator::nextCodegenId();
    code << "   LazyMultiMap<Key<imlab::Value>, std::vector<imlab::Value>> hashTable_" << codegenId_ << ";\n";

    left_->produce(code);

    code << "   hashTable_" << codegenId_ << ".finalize();\n";

    right_->produce(code);

    code << "   // end InnerJoin::produce\n";
}
//---------------------------------------------------------------------------
void InnerJoin::consume(std::ostream& code, const Operator* child) {
    code << "       // start InnerJoin::consume\n";

    if (child == left_.get()) {
        const auto& leftVarNames = left_->getVarNames();

        code << "       {\n";
        code << "          Key<imlab::Value> key(" << varNames_[leftJoinIU_] << ");\n";
        code << "          std::vector<imlab::Value> values;\n";

        for (const auto& [iu, varName] : leftVarNames) {
            code << "          values.push_back(" << varName << ");\n";
        }
        code << "          hashTable_" << codegenId_ << ".insert({key, values});\n";
        code << "       }\n";

    } else if (child == right_.get()) {

        code << "       {\n";
        code << "          Key<imlab::Value> key(" << varNames_[rightJoinIU_] << ");\n";
        code << "          auto matches = hashTable_" << codegenId_ << ".equal_range(key);\n";
        code << "          for (auto match = matches.first; match != matches.second; ++match) {\n";

        const auto& leftVarNames = left_->getVarNames();
        int idx = 0;
        for (const auto& [iu, varName] : leftVarNames) {
            code << "              auto " << varName << " = match->value_[" << idx++ << "];\n";
        }

        code << "              if (";
        condition_->produce(code);
        code << ") {\n";

        const auto& rightVarNames = right_->getVarNames();
        for (const auto& [iu, varName] : rightVarNames) {
            varNames_[iu] = varName;
        }

        consumer_->consume(code, this);

        code << "              }\n";
        code << "          }\n";
        code << "       }\n";

    }

    code << "       // end InnerJoin::consume\n";
}


std::map<const IU*, Value> InnerJoin::combineTuples(
      const std::map<const IU*, Value>& leftTuple,
      const std::map<const IU*, Value>& rightTuple) const {

    std::map<const IU*, Value> combinedTuple;

    combinedTuple.insert(leftTuple.begin(), leftTuple.end());
    combinedTuple.insert(rightTuple.begin(), rightTuple.end());

    return combinedTuple;
}

void InnerJoin::appendCondition(std::unique_ptr<Expression> newCondition){
    if (!condition_) {
        condition_ = std::move(newCondition);
    } else {
        condition_ = std::make_unique<BinaryExpression>(
            BinaryExpression::Op::And,
            std::move(condition_),
            std::move(newCondition)
        );
    }
}

bool InnerJoin::pushPredicate(std::unique_ptr<Expression>& predicate, OptimizerPass pass) {
    auto conditions = splitConditions(std::move(predicate));
    std::vector<std::unique_ptr<Expression>> leftConds;
    std::vector<std::unique_ptr<Expression>> rightConds;
    std::vector<std::unique_ptr<Expression>> joinConds;

    auto leftIUs = left_->collectIUs();
    auto rightIUs = right_->collectIUs();

    for (auto& cond : conditions) {
        auto ius = cond->collectIUs();
        bool onlyLeft = true;
        bool onlyRight = true;
        for (const IU* iu : ius) {
            if (leftIUs.find(iu) == leftIUs.end()) {
                onlyLeft = false;
            }
            if (rightIUs.find(iu) == rightIUs.end()) {
                onlyRight = false;
            }
            if (!onlyLeft && !onlyRight) break;
        }
        if (onlyLeft) {
            leftConds.push_back(std::move(cond));
        } else if (onlyRight) {
            rightConds.push_back(std::move(cond));
        } else {
            joinConds.push_back(std::move(cond));
        }
    }

    bool absorbed = false;
    if (!leftConds.empty()) {
        auto combinedLeft = combineConditions(std::move(leftConds));
        if (left_->getOperatorType() == OperatorType::Select) {
            auto leftSelect = static_cast<Select*>(left_.get());
            leftSelect->mergePredicate(std::move(combinedLeft));
            left_->optimize(left_, pass);
        } else {
            auto newSelect = std::make_unique<Select>(std::move(left_), std::move(combinedLeft));
            left_ = std::move(newSelect);
            left_->optimize(left_, pass);
        }
        absorbed = true;
    }
    if (!rightConds.empty()) {
        auto combinedRight = combineConditions(std::move(rightConds));
        if (right_->getOperatorType() == OperatorType::Select) {
            auto rightSelect = static_cast<Select*>(right_.get());
            rightSelect->mergePredicate(std::move(combinedRight));
            right_->optimize(right_, pass);
        } else {
            auto newSelect = std::make_unique<Select>(std::move(right_), std::move(combinedRight));
            right_ = std::move(newSelect);
            right_->optimize(right_, pass);
        }
        absorbed = true;
    }
    if (!joinConds.empty()) {
        auto combinedJoin = combineConditions(std::move(joinConds));
        appendCondition(std::move(combinedJoin));
        absorbed = true;
    }
    return absorbed;
}


//---------------------------------------------------------------------------
void InnerJoin::optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) {
    BinaryOperator::optimize(thisRef, pass);
    if (pass != imlab::OptimizerPass::BuildSideSelection || !condition_ || !left_ || !right_) {
        return;
    }
    const auto leftRows = left_->estimateCardinality();
    const auto rightRows = right_->estimateCardinality();
    if (!leftRows || !rightRows || *rightRows >= *leftRows) {
        return;
    }
    std::swap(left_, right_);
}
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
