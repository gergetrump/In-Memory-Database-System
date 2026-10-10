//---------------------------------------------------------------------------
#ifndef IMLAB_OPTIMIZER_OPTIMIZERPASS_H
#define IMLAB_OPTIMIZER_OPTIMIZERPASS_H
//---------------------------------------------------------------------------
namespace imlab {
//---------------------------------------------------------------------------
/// Known optimizer passes
enum class OptimizerPass : unsigned {
    PredicatePushdown,
    BuildSideSelection,
};
//---------------------------------------------------------------------------
} // namespace imlab
//---------------------------------------------------------------------------
#endif //IMLAB_OPTIMIZER_OPTIMIZERPASS_H
//---------------------------------------------------------------------------