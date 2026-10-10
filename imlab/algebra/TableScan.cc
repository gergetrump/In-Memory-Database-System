//---------------------------------------------------------------------------
#include "imlab/algebra/TableScan.h"
#include "imlab/algebra/Operator.h"

#include <iostream>
//---------------------------------------------------------------------------
namespace imlab::algebra {
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
void TableScan::produce(std::ostream& code) {
    code << "   // start TableScan::produce\n";

    code << "   {\n";
    code << "       auto& table = db.getTable(\"" << tableSchema_.id << "\");\n";
    code << "       auto& rows = table.getRows();\n";
    code << "       auto& columnIndices = table.getColumnIndices();\n";

    code << "       for (size_t i = 0; i < rows.size(); ++i) {\n";
    code << "           const auto& row = rows[i];\n";

    for (const auto* iu : requiredIUs_) {
        std::string varName = varNames_[iu];
        code << "           auto " << varName << " = row[columnIndices.at(\"" << iu->column_ << "\")];\n";
    }

    consumer_->consume(code, this);

    code << "       }\n";
    code << "   }\n";


    code << "   // end TableScan::produce\n";
}

//---------------------------------------------------------------------------
void TableScan::optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) {
    // nothing to do here
}


const IU* TableScan::getIUByName(const std::vector<const IU*>& ius, const std::string& name) {
    for (const auto iu : ius) {
        if (iu->column_ == name) {
            return iu;
        }
    }
    return nullptr;
}

//---------------------------------------------------------------------------
} // namespace imlab::algebra
// ---------------------------------------------------------------------------
