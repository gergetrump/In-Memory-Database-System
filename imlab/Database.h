//---------------------------------------------------------------------------
#ifndef IMLAB_DATABASE_H
#define IMLAB_DATABASE_H
//---------------------------------------------------------------------------
#include "../imlab/parser/AST.h"
#include "../imlab/parser/ParseContext.h"
#include "../imlab/types/Types.h"
#include "../imlab/algebra/IU.h"
#include <optional>
#include <unordered_map>
#include <vector>

//---------------------------------------------------------------------------
namespace imlab {namespace algebra {
class IU;}
//---------------------------------------------------------------------------
namespace semana {
class SemanticalAnalysis;
} // namespace semana
//---------------------------------------------------------------------------
struct PrimaryKeyHash {
    size_t operator()(const std::vector<Value>& key) const {
        size_t hash = 0;
        for (const Value& part : key) {
            hash ^= part.hash() + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

class Table {

    parser::Table tableSchema_;

    std::vector<std::vector<Value>> tableRows_;
    std::unordered_map<std::string, size_t> tableColumnIndices_;
    std::unordered_map<std::vector<Value>, size_t, PrimaryKeyHash> primaryKeyIndex_;

    std::vector<std::unique_ptr<algebra::IU>> tableColumnsIUs_;

public:

    explicit Table(const parser::Table& tableSchema);

    void insertRow(const std::vector<Value>& newRow);

    std::optional<size_t> findPrimaryKey(const std::vector<Value>& key) const;

    const parser::Table& getSchema() const {
      return tableSchema_;
    }

    const std::unordered_map<std::string, size_t>& getColumnIndices() const {
      return tableColumnIndices_;
    }

    const std::vector<std::vector<Value>>& getRows() const {
      return tableRows_;
    }

    const std::vector<std::unique_ptr<algebra::IU>>& getColumnsIUs() const {
      return tableColumnsIUs_;
    }
};
//---------------------------------------------------------------------------
class Database {
    friend semana::SemanticalAnalysis;

    /// the tables
    std::unordered_map<std::string, Table> dbTables_;

public:

    Database() = default;

    void createTable(const parser::Table& tableSchema);

    void insertRow(const std::string& tableName, const std::vector<Value>& newRow);

    Table& getTable(const std::string& tableName);

    bool tableExists(const std::string& tableName) const;

};
//---------------------------------------------------------------------------
} // namespace imlab
//---------------------------------------------------------------------------
#endif //IMLAB_DATABASE_H
