#ifndef BEAM_INCLUDED
#define BEAM_INCLUDED

#include <list>
#include <algorithm>
#include <random>

#include "dpmatrix.h"
#include "logger.h"

struct BeamSearchMatrix {
  typedef Envelope::InputIndex InputIndex;
  typedef Envelope::OutputIndex OutputIndex;

  struct SeqNode {
    typedef SeqNode* SeqNodePtr;
    const InputToken inTok;
    const SeqNodePtr parent;
    vguard<SeqNodePtr> child;
    SeqNode();
    SeqNode (SeqNodePtr p, InputToken t);
  };

  typedef SeqNode::SeqNodePtr SeqNodePtr;
  typedef map<SeqNodePtr,double> Cell;
  
  const EvaluatedMachine& machine;
  const vguard<OutputToken> output;
  const OutputIndex outLen;
  const StateIndex nStates;
  const size_t beamWidth;

  list<SeqNode> seqNodeStore;
  vguard<Cell> cellStore;
  
  inline size_t nCells() const {
    return (outLen + 1) * nStates;
  }
  
  inline size_t cellIndex (OutputIndex outPos, StateIndex state) const {
    return outPos * nStates + state;
  }

  inline Cell& cell (OutputIndex outPos, StateIndex state) {
    return cellStore [cellIndex (outPos, state)];
  }

  inline const Cell& cell (OutputIndex outPos, StateIndex state) const {
    return cellStore [cellIndex (outPos, state)];
  }

  BeamSearchMatrix (const EvaluatedMachine& machine, const vguard<OutputSymbol>& outSym, size_t beamWidth);

  void extendSeq (SeqNodePtr);
  
  vguard<InputSymbol> doBeamSearch();
  vguard<InputSymbol> getSeq (SeqNodePtr) const;
};

#endif /* BEAM_INCLUDED */
