//---------------------------------------------------------------------------
// IMLAB
//---------------------------------------------------------------------------
#ifndef IMLAB_ALGEBRA_GROUPBY_H
#define IMLAB_ALGEBRA_GROUPBY_H
//---------------------------------------------------------------------------
#include "imlab/algebra/Operator.h"
#include <memory>
#include <string>
#include <vector>
//---------------------------------------------------------------------------
namespace imlab::algebra {
//---------------------------------------------------------------------------
class GroupBy final : public UnaryOperator {
    public:
    struct Agg {
        enum class Kind { CountStar, Sum };
        Kind kind;
        const IU* argument = nullptr;
        std::unique_ptr<IU> result;
    };

    GroupBy(std::unique_ptr<Operator> input, std::vector<const IU*> keys);
    ~GroupBy() override = default;

    const IU* addCount(const std::string& name);
    const IU* addSum(const IU* argument, const std::string& name);

    std::set<const IU*> collectIUs() override;
    void prepare(const std::set<const IU*>& required, Operator* consumer) override;
    void produce(std::ostream& code) override;
    void consume(std::ostream& code, const Operator* child) override;

    OperatorType getOperatorType() const override { return OperatorType::GroupBy; }

    private:
    int sumSlot(size_t aggIndex) const;
    int sumCount() const;
    void emitInt32(std::ostream& code, const std::string& dest, const std::string& expr) const;

    std::vector<const IU*> keys_;
    std::vector<Agg> aggs_;
    int codegenId_ = 0;
};
//---------------------------------------------------------------------------
} // namespace imlab::algebra
//---------------------------------------------------------------------------
#endif // IMLAB_ALGEBRA_GROUPBY_H
//---------------------------------------------------------------------------
