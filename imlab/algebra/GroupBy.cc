//---------------------------------------------------------------------------
#include "imlab/algebra/GroupBy.h"

#include "imlab/algebra/IU.h"
#include <algorithm>
#include <string>
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
GroupBy::GroupBy(std::unique_ptr<Operator> input, std::vector<const IU*> keys)
    : UnaryOperator(std::move(input)), keys_(std::move(keys)) {}
//---------------------------------------------------------------------------
const IU* GroupBy::addCount(const std::string& name) {
    auto result = std::make_unique<IU>(name, imlab::Type::Integer());
    const IU* raw = result.get();
    aggs_.push_back(Agg{Agg::Kind::CountStar, nullptr, std::move(result)});
    return raw;
}
//---------------------------------------------------------------------------
const IU* GroupBy::addSum(const IU* argument, const std::string& name) {
    auto result = std::make_unique<IU>(name, imlab::Type::Integer());
    const IU* raw = result.get();
    aggs_.push_back(Agg{Agg::Kind::Sum, argument, std::move(result)});
    return raw;
}
//---------------------------------------------------------------------------
int GroupBy::sumSlot(size_t aggIndex) const {
    int slot = 0;
    for (size_t i = 0; i < aggIndex; ++i) {
        if (aggs_[i].kind == Agg::Kind::Sum) {
            ++slot;
        }
    }
    return slot;
}
//---------------------------------------------------------------------------
int GroupBy::sumCount() const {
    int count = 0;
    for (const auto& agg : aggs_) {
        if (agg.kind == Agg::Kind::Sum) {
            ++count;
        }
    }
    return count;
}
//---------------------------------------------------------------------------
std::set<const IU*> GroupBy::collectIUs() {
    std::set<const IU*> ius(keys_.begin(), keys_.end());
    for (const auto& agg : aggs_) {
        ius.insert(agg.result.get());
    }
    return ius;
}
//---------------------------------------------------------------------------
void GroupBy::prepare(const std::set<const IU*>& required, Operator* consumer) {
    (void)required;
    consumer_ = consumer;

    std::set<const IU*> childRequired(keys_.begin(), keys_.end());
    for (const auto& agg : aggs_) {
        if (agg.argument) {
            childRequired.insert(agg.argument);
        }
    }
    input_->prepare(childRequired, this);

    varNames_.clear();
    for (size_t i = 0; i < keys_.size(); ++i) {
        varNames_[keys_[i]] = "gk_" + std::to_string(i);
    }
    for (size_t i = 0; i < aggs_.size(); ++i) {
        varNames_[aggs_[i].result.get()] = "agg_" + std::to_string(i);
    }
}
//---------------------------------------------------------------------------
void GroupBy::emitInt32(std::ostream& code, const std::string& dest, const std::string& expr) const {
    code << "          if (" << expr << " > 2147483647LL || " << expr << " < -2147483648LL) {\n";
    code << "              throw imlab::RuntimeException(\"aggregate overflow\");\n";
    code << "          }\n";
    code << "          auto " << dest << " = imlab::Value(imlab::Integer(static_cast<int32_t>(" << expr << ")));\n";
}
//---------------------------------------------------------------------------
void GroupBy::produce(std::ostream& code) {
    code << "   // start GroupBy::produce\n";
    codegenId_ = Operator::nextCodegenId();
    const int id = codegenId_;
    const int sums = sumCount();

    if (keys_.empty()) {
        const bool needsCount = std::any_of(aggs_.begin(), aggs_.end(), [](const Agg& agg) {
            return agg.kind == Agg::Kind::CountStar;
        });
        if (needsCount) {
            code << "   int64_t count_" << id << " = 0;\n";
        }
        for (int slot = 0; slot < sums; ++slot) {
            code << "   int64_t sum_" << id << "_" << slot << " = 0;\n";
        }
        input_->produce(code);
        for (size_t i = 0; i < aggs_.size(); ++i) {
            const std::string dest = varNames_.at(aggs_[i].result.get());
            if (aggs_[i].kind == Agg::Kind::CountStar) {
                emitInt32(code, dest, "count_" + std::to_string(id));
            } else {
                const std::string expr = "sum_" + std::to_string(id) + "_" + std::to_string(sumSlot(i));
                emitInt32(code, dest, expr);
            }
        }
        if (consumer_) {
            consumer_->consume(code, this);
        }
        code << "   // end GroupBy::produce\n";
        return;
    }

    code << "   struct GroupKey_" << id << " {\n";
    code << "       std::vector<int32_t> parts;\n";
    code << "       bool operator==(const GroupKey_" << id << "& other) const { return parts == other.parts; }\n";
    code << "       bool operator<(const GroupKey_" << id << "& other) const { return parts < other.parts; }\n";
    code << "   };\n";
    code << "   struct GroupKeyHash_" << id << " {\n";
    code << "       size_t operator()(const GroupKey_" << id << "& key) const {\n";
    code << "           size_t hash = 0;\n";
    code << "           for (int32_t part : key.parts) {\n";
    code << "               hash ^= std::hash<int32_t>{}(part) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);\n";
    code << "           }\n";
    code << "           return hash;\n";
    code << "       }\n";
    code << "   };\n";
    code << "   struct AggState_" << id << " {\n";
    code << "       int64_t count = 0;\n";
    if (sums > 0) {
        code << "       int64_t sums[" << sums << "] = {};\n";
    }
    code << "   };\n";
    code << "   std::unordered_map<GroupKey_" << id << ", AggState_" << id << ", GroupKeyHash_" << id << "> groups_" << id << ";\n";

    input_->produce(code);

    code << "   std::vector<GroupKey_" << id << "> ordered;\n";
    code << "   ordered.reserve(groups_" << id << ".size());\n";
    code << "   for (const auto& entry : groups_" << id << ") ordered.push_back(entry.first);\n";
    code << "   std::sort(ordered.begin(), ordered.end());\n";
    code << "   for (const auto& groupKey : ordered) {\n";
    code << "       const auto& slot = groups_" << id << ".at(groupKey);\n";
    for (size_t i = 0; i < keys_.size(); ++i) {
        code << "       auto " << varNames_.at(keys_[i]) << " = imlab::Value(imlab::Integer(groupKey.parts[" << i << "]));\n";
    }
    for (size_t i = 0; i < aggs_.size(); ++i) {
        const std::string dest = varNames_.at(aggs_[i].result.get());
        if (aggs_[i].kind == Agg::Kind::CountStar) {
            emitInt32(code, dest, "slot.count");
        } else {
            emitInt32(code, dest, "slot.sums[" + std::to_string(sumSlot(i)) + "]");
        }
    }
    if (consumer_) {
        consumer_->consume(code, this);
    }
    code << "   }\n";
    code << "   // end GroupBy::produce\n";
}
//---------------------------------------------------------------------------
void GroupBy::consume(std::ostream& code, const Operator* child) {
    (void)child;
    const int id = codegenId_;
    const auto& childVars = input_->getVarNames();
    code << "       {\n";
    if (keys_.empty()) {
        bool counted = false;
        for (const auto& agg : aggs_) {
            if (agg.kind == Agg::Kind::CountStar && !counted) {
                code << "           count_" << id << " += 1;\n";
                counted = true;
            }
        }
        if (!counted && aggs_.empty()) {
            code << "           count_" << id << " += 1;\n";
        }
        for (size_t i = 0; i < aggs_.size(); ++i) {
            if (aggs_[i].kind != Agg::Kind::Sum) {
                continue;
            }
            code << "           sum_" << id << "_" << sumSlot(i) << " += static_cast<int64_t>("
                 << childVars.at(aggs_[i].argument) << ".getInteger().value);\n";
        }
        code << "       }\n";
        return;
    }

    code << "           GroupKey_" << id << " key;\n";
    for (const IU* key : keys_) {
        code << "           key.parts.push_back(" << childVars.at(key) << ".getInteger().value);\n";
    }
    code << "           auto& slot = groups_" << id << "[key];\n";
    code << "           slot.count += 1;\n";
    for (size_t i = 0; i < aggs_.size(); ++i) {
        if (aggs_[i].kind != Agg::Kind::Sum) {
            continue;
        }
        code << "           slot.sums[" << sumSlot(i) << "] += static_cast<int64_t>("
             << childVars.at(aggs_[i].argument) << ".getInteger().value);\n";
    }
    code << "       }\n";
}
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
