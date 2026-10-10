//---------------------------------------------------------------------------
#include "imlab/algebra/TableScan.h"

#include "imlab/algebra/BinaryExpression.h"
#include "imlab/algebra/Const.h"
#include "imlab/algebra/IURef.h"
#include "imlab/algebra/Operator.h"

#include <map>
#include <string>
//---------------------------------------------------------------------------
namespace imlab::algebra {
namespace {

std::string escaped(const std::string& text) {
    std::string out;
    for (char ch : text) {
        if (ch == '\\' || ch == '"') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

void emitKeyValue(std::ostream& code, const Const& constant) {
    const Type& type = constant.getType();
    switch (type.getClass()) {
        case Type::kInteger:
            code << "imlab::Value(imlab::Integer(" << constant.getValue().getInteger().value << "))";
            break;
        case Type::kTimestamp:
            code << "imlab::Value(imlab::Timestamp(" << constant.getValue().getTimestamp().value << "))";
            break;
        case Type::kNumeric:
            code << "imlab::Value(static_cast<int64_t>(" << constant.getValue().getNumericRawValue()
                 << "), imlab::Type::Numeric(" << type.getPrecision() << ", " << type.getScale() << "))";
            break;
        case Type::kChar:
            code << "imlab::Value(std::string(\"" << escaped(constant.getValue().getStringValue())
                 << "\"), imlab::Type::Char(" << type.getLength() << "))";
            break;
        case Type::kVarchar:
            code << "imlab::Value(std::string(\"" << escaped(constant.getValue().getStringValue())
                 << "\"), imlab::Type::Varchar(" << type.getLength() << "))";
            break;
        default:
            throw std::runtime_error("Unsupported primary-key type");
    }
}

const IU* columnIU(const std::vector<const IU*>& ius, const std::string& name) {
    for (const IU* iu : ius) {
        if (iu->column_ == name) {
            return iu;
        }
    }
    return nullptr;
}

bool collectEqualities(Expression* expr, const std::vector<const IU*>& ius, std::map<const IU*, const Const*>& found) {
    auto* binary = dynamic_cast<BinaryExpression*>(expr);
    if (binary == nullptr) {
        return false;
    }
    if (binary->getOp() == BinaryExpression::Op::And) {
        return collectEqualities(binary->left_.get(), ius, found) && collectEqualities(binary->right_.get(), ius, found);
    }
    if (binary->getOp() != BinaryExpression::Op::Equal) {
        return false;
    }

    IURef* column = dynamic_cast<IURef*>(binary->left_.get());
    Const* constant = dynamic_cast<Const*>(binary->right_.get());
    if (column == nullptr || constant == nullptr) {
        column = dynamic_cast<IURef*>(binary->right_.get());
        constant = dynamic_cast<Const*>(binary->left_.get());
    }
    if (column == nullptr || constant == nullptr || !constant->hasType()) {
        return false;
    }
    bool belongsToScan = false;
    for (const IU* iu : ius) {
        if (iu == column->getIU()) {
            belongsToScan = true;
        }
    }
    if (!belongsToScan) {
        return false;
    }
    return found.emplace(column->getIU(), constant).second;
}

} // namespace
//---------------------------------------------------------------------------
TableScan::TableScan(const char* table, unsigned int id, const parser::Table& tableSchema, const std::vector<const IU*>& ius, size_t cardinality) : Operator(), table_(table), id_(id), tableSchema_(tableSchema), registeredIUs_(ius), cardinality_(cardinality) {}
//---------------------------------------------------------------------------
std::set<const IU*> TableScan::collectIUs() {
    std::set<const IU*> ius;
    for (auto iu : registeredIUs_) {
        ius.insert(iu);
    }
    return ius;
}
//---------------------------------------------------------------------------
void TableScan::prepare(const std::set<const IU*>& required, Operator* consumer) {
    consumer_ = consumer;
    requiredIUs_.clear();
    varNames_.clear();

    for (auto iu : registeredIUs_) {
        if (required.find(iu) != required.end()) {
            requiredIUs_.insert(iu);

            std::string varName = tableSchema_.id + "_" + iu->column_;
            varNames_[iu] = varName;
        }
    }
}

//---------------------------------------------------------------------------
std::optional<size_t> TableScan::estimateCardinality() const {
    if (indexPredicate_) {
        return 1;
    }
    return cardinality_;
}
//---------------------------------------------------------------------------
bool TableScan::pushPredicate(std::unique_ptr<Expression>& predicate, OptimizerPass pass) {
    if (pass != OptimizerPass::PredicatePushdown || !predicate || indexPredicate_ || tableSchema_.primary_key.empty()) {
        return false;
    }
    std::map<const IU*, const Const*> found;
    if (!collectEqualities(predicate.get(), registeredIUs_, found)) {
        return false;
    }
    if (found.size() != tableSchema_.primary_key.size()) {
        return false;
    }
    std::vector<const Const*> ordered;
    ordered.reserve(tableSchema_.primary_key.size());
    for (const auto& column : tableSchema_.primary_key) {
        const IU* iu = columnIU(registeredIUs_, column.id);
        auto key = iu == nullptr ? found.end() : found.find(iu);
        if (key == found.end()) {
            return false;
        }
        ordered.push_back(key->second);
    }
    indexPredicate_ = std::move(predicate);
    indexKeyOrder_ = std::move(ordered);
    return true;
}
//---------------------------------------------------------------------------
void TableScan::produce(std::ostream& code) {
    code << "   // start TableScan::produce\n";
    code << "   {\n";
    code << "       auto& table = db.getTable(\"" << tableSchema_.id << "\");\n";
    code << "       auto& rows = table.getRows();\n";
    code << "       auto& columnIndices = table.getColumnIndices();\n";

    std::vector<const IU*> columns(requiredIUs_.begin(), requiredIUs_.end());

    if (indexPredicate_) {
        code << "       std::vector<imlab::Value> key;\n";
        for (const Const* constant : indexKeyOrder_) {
            code << "       key.push_back(";
            emitKeyValue(code, *constant);
            code << ");\n";
        }
        code << "       auto found = table.findPrimaryKey(key);\n";
        code << "       if (found) {\n";
        code << "           const auto& row = rows[*found];\n";
        for (const IU* iu : columns) {
            code << "           auto " << varNames_.at(iu) << " = row[columnIndices.at(\"" << iu->column_ << "\")];\n";
        }
        if (consumer_ != nullptr) {
            consumer_->consume(code, this);
        }
        code << "       }\n";
        code << "   }\n";
        code << "   // end TableScan::produce\n";
        return;
    }

    const int scanId = Operator::nextCodegenId();
    code << "       const size_t rowCount = rows.size();\n";
    code << "       const size_t morselRows = std::min<size_t>(1024, std::max<size_t>(1, rowCount / 2));\n";
    code << "       const size_t morselCount = rowCount == 0 ? 0 : (rowCount + morselRows - 1) / morselRows;\n";
    code << "       struct MorselRow_" << scanId << " {\n";
    for (size_t i = 0; i < columns.size(); ++i) {
        code << "           imlab::Value f" << i << ";\n";
    }
    code << "       };\n";
    code << "       std::vector<std::vector<MorselRow_" << scanId << ">> morsels(morselCount);\n";
    code << "       tbb::parallel_for(size_t(0), morselCount, [&](size_t morsel) {\n";
    code << "           const size_t begin = morsel * morselRows;\n";
    code << "           const size_t end = std::min(rowCount, begin + morselRows);\n";
    code << "           auto& chunk = morsels[morsel];\n";
    code << "           chunk.reserve(end - begin);\n";
    code << "           for (size_t i = begin; i < end; ++i) {\n";
    code << "               const auto& row = rows[i];\n";
    code << "               MorselRow_" << scanId << " record;\n";
    for (size_t i = 0; i < columns.size(); ++i) {
        code << "               record.f" << i << " = row[columnIndices.at(\"" << columns[i]->column_ << "\")];\n";
    }
    code << "               chunk.push_back(std::move(record));\n";
    code << "           }\n";
    code << "       });\n";
    code << "       for (size_t morsel = 0; morsel < morselCount; ++morsel) {\n";
    code << "           for (const auto& record : morsels[morsel]) {\n";
    for (size_t i = 0; i < columns.size(); ++i) {
        code << "               auto " << varNames_.at(columns[i]) << " = record.f" << i << ";\n";
    }
    if (consumer_ != nullptr) {
        consumer_->consume(code, this);
    }
    code << "           }\n";
    code << "       }\n";
    code << "   }\n";
    code << "   // end TableScan::produce\n";
}
//---------------------------------------------------------------------------
void TableScan::optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) {
    (void)thisRef;
    (void)pass;
}

const IU* TableScan::getIUByName(const std::vector<const IU*>& ius, const std::string& name) {
    return columnIU(ius, name);
}

//---------------------------------------------------------------------------
} // namespace imlab::algebra
// ---------------------------------------------------------------------------
