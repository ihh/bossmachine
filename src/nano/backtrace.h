#ifndef BACKTRACE_INCLUDED
#define BACKTRACE_INCLUDED

#include "fwdtrace.h"
#include "gcounts.h"

class BackwardTraceMatrix : public TraceDPMatrix {
private:
  vguard<IndexedTrans>::const_reverse_iterator nullTrans_rbegin, nullTrans_rend;
public:
  BackwardTraceMatrix (const EvaluatedMachine& eval, const GaussianModelParams& modelParams, const Trace& trace, const TraceParams& traceParams);
  
  void getMachineCounts (const ForwardTraceMatrix&, MachineCounts&) const;
  void getGaussianCounts (const ForwardTraceMatrix&, vguard<GaussianCounts>&) const;
  void getCounts (const ForwardTraceMatrix&, GaussianModelCounts&) const;
  double logLike() const;
};

#endif /* BACKTRACE_INCLUDED */
