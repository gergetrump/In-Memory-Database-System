//---------------------------------------------------------------------------
#include "imlab/algebra/IU.h"
#include "imlab/infra/Types.h"

#include <imlab/runtime/RuntimeException.h>
//---------------------------------------------------------------------------


namespace imlab::algebra {

// add tables and their columns to scope (FROM)
void Scope::addTable(const std::string& tableName, const std::string& alias, const std::vector<const IU*>& columns) {
    std::string effectiveName = alias.empty() ? tableName : alias;

    for (const auto* iuPtr : columns) {
        ius_[effectiveName].push_back({effectiveName, iuPtr});
    }
}

// resolve column references in SELECT and WHERE
const IU* Scope::resolveIU(const std::string& tableName, const std::string& columnName) const {

    if (!tableName.empty()) {
        auto iter = ius_.find(tableName);
        if (iter != ius_.end()) {
            for (auto& iuMap : iter->second) {
                if (iuMap.iu->column_ == columnName) {
                    return iuMap.iu;
                }
            }
        }
    } else {
        const IU* foundIU = nullptr;
        for (auto& [alias, iuMaps] : ius_) {
            for (auto& iuMap : iuMaps) {
                if (iuMap.iu->column_ == columnName) {
                    if (foundIU) {
                        throw RuntimeException("Column '" + columnName + "' is ambiguos");
                    }
                    foundIU = iuMap.iu;
                }
            }
        }
        if (foundIU) {
            return foundIU;
        }
    }
    return nullptr;
}

// retrieve all columns in SELECT *
std::vector<const IU*> Scope::getAllIUs() const {
    std::vector<const IU*> allIUs;
    for (auto& entry : ius_) {
        for (auto& iuMap : entry.second) {
            allIUs.push_back(iuMap.iu);
        }
    }
    return allIUs;
}




}

