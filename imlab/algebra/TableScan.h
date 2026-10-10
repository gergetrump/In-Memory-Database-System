//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
#ifndef IMLAB_ALGEBRA_TABLESCAN_H
#define IMLAB_ALGEBRA_TABLESCAN_H
//---------------------------------------------------------------------------
#include "imlab/algebra/IU.h"
#include "imlab/algebra/Operator.h"
#include <vector>
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
class TableScan final : public Operator {
    protected:
    /// Table
    const char* table_;
    /// Id of this table scan
    unsigned id_;

    /// IUs belonging to this TableScan
    std::vector<const IU*> registeredIUs_;

    parser::Table tableSchema_;
    size_t cardinality_ = 0;

    public:
    /// Constructor
    TableScan(const char* table, unsigned id, const parser::Table& tableSchema, const std::vector<const IU*>& ius, size_t cardinality = 0);
    /// Destructor
    ~TableScan() override = default;

    /// Deleted copy operations
    TableScan(const TableScan&) = delete;
    TableScan& operator=(const TableScan&) = delete;
    /// Deleted move operations
    TableScan(TableScan&&) noexcept = delete;
    TableScan& operator=(TableScan&&) noexcept = delete;

    /// Collect all IUs produced by the operator
    std::set<const IU*> collectIUs() override;

    /// Prepare the operator
    void prepare(const std::set<const IU*>& required, Operator* consumer) override;
    /// Produce tuple
    void produce(std::ostream& code) override;
    /// Consume tuple
    void consume(std::ostream& code, const Operator* child) override {}

    /// Get the operator type
    OperatorType getOperatorType() const override { return OperatorType::TableScan; }
    /// Get the operator category
    OperatorCategory getOperatorCategory() const override { return OperatorCategory::Scan; }

    /// Perform optimization
    void optimize(std::unique_ptr<Operator>& thisRef, imlab::OptimizerPass pass) override;

    std::optional<size_t> estimateCardinality() const override { return cardinality_; }

    const std::vector<const IU*>& getRegisteredIUs() const {return registeredIUs_;}

    static const IU* getIUByName(const std::vector<const IU*>& ius, const std::string& name);

    const std::string getTableName() const {
        return tableSchema_.id;
    }


};
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
#endif // IMLAB_ALGEBRA_TABLESCAN_H
//---------------------------------------------------------------------------
