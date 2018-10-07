#include "ctc.h"
#include "logger.h"

PrefixTree::Node::Node() :
  inTok (0),
  parent (NULL),
  nStates (0),
  outLen (0)
{ }

PrefixTree::Node::Node (const PrefixTree& tree, const Node* parent, InputToken inTok) :
  inTok (inTok),
  parent (parent),
  nStates (tree.nStates),
  outLen (tree.outLen)
{ }

void PrefixTree::Node::fill (const PrefixTree& tree)
{
  cellStorage = vector<double> (nCells(), -numeric_limits<double>::infinity());
  logPrefixProb = -numeric_limits<double>::infinity();

  if (!parent)
    seqCell (0, 0) = 0;

  for (OutputIndex outPos = 0; outPos <= outLen; ++outPos) {
    const OutputToken outTok = outPos ? tree.output[outPos-1] : OutputTokenizer::emptyToken();
    for (StateIndex d = 0; d < nStates; ++d) {
      LogThisAt(9,"d="<<d<<": ");
      const EvaluatedMachineState& state = tree.machine.state[d];
      double& ll = seqCell (outPos, d);
      const EvaluatedMachineState::OutStateTransMap* nonAbsorbing = NULL;
      const EvaluatedMachineState::OutStateTransMap* absorbing = NULL;
      if (parent && state.incoming.count (inTok))
	absorbing = &state.incoming.at (inTok);
      if (state.incoming.count (InputTokenizer::emptyToken()))
	nonAbsorbing = &state.incoming.at (InputTokenizer::emptyToken());
      if (absorbing && outPos)
	accumulateSeqCell (ll, *absorbing, *parent, outTok, outPos - 1);
      if (absorbing)
	accumulateSeqCell (ll, *absorbing, *parent, OutputTokenizer::emptyToken(), outPos);
      prefixCell (outPos, d) = ll;
      if (outPos && nonAbsorbing)
	accumulateSeqCell (ll, *nonAbsorbing, *this, outTok, outPos - 1);
      if (nonAbsorbing)
	accumulateSeqCell (ll, *nonAbsorbing, *this, OutputTokenizer::emptyToken(), outPos);
      LogThisAt(8,"seqCell("<<outPos<<","<<d<<")="<<ll<<endl);
    }
    // looping over d AND prevState seems inefficient! Could precompute sumInTrans*outTrans for each outTok
    for (StateIndex d = 0; d < nStates; ++d) {
      double& ll = prefixCell (outPos, d);
      if (outPos) {
	const EvaluatedMachineState& state = tree.machine.state[d];
	for (const auto& i_ostm: state.incoming) {
	  const EvaluatedMachineState::OutStateTransMap& outStateTransMap = i_ostm.second;
	  if (outStateTransMap.count (outTok))
	    for (const auto& st: outStateTransMap.at (outTok)) {
	      const EvaluatedMachineState::Trans& trans = st.second;
	      for (StateIndex prevState = 0; prevState < nStates; ++prevState) {
		const double prevCell = prefixCell (outPos - 1, prevState);
		const double logEmitWeight = prevCell + tree.sumInTrans[prevState][st.first] + trans.logWeight;
		log_accum_exp (ll, logEmitWeight);
		LogThisAt(9,"prefixCell("<<outPos<<","<<d<<") logsum+= "<<prevCell<<" + "<<tree.sumInTrans[prevState][st.first]<<" + "<<trans.logWeight<<" ("<<prevState<<"->"<<st.first<<"->"<<d<<")"<<" ... now "<<ll<<endl);
	      }
	    }
	}
      }
      LogThisAt(8,"prefixCell("<<outPos<<","<<d<<")="<<ll<<endl);
    }
  }

  for (StateIndex d = 0; d < nStates; ++d) {
    log_accum_exp (logPrefixProb, prefixCell(outLen,d) + tree.sumInTrans[d][tree.nStates - 1]);
    LogThisAt(9,"logPrefixProb logsum+= "<<prefixCell(outLen,d)<<" + "<<tree.sumInTrans[d][tree.nStates - 1]<<" ("<<d<<"->end)"<<endl);
  }

  if (parent && logPrefixProb > parent->logPrefixProb)
    Warn ("LogP(%s*)=%g rose from LogP(%s*)=%g",
	  to_string_join(tree.seqTraceback(this),"").c_str(), logPrefixProb,
	  to_string_join(tree.seqTraceback(parent),"").c_str(), parent->logPrefixProb);
}

double PrefixTree::Node::logSeqProb() const {
  return seqCell (outLen, nStates - 1);
}

vguard<InputToken> PrefixTree::Node::traceback() const {
  list<InputToken> result;
  for (const Node* node = this; node->inTok; node = node->parent)
    result.push_front (node->inTok);
  return vguard<InputToken> (result.begin(), result.end());
}

PrefixTree::InputIndex PrefixTree::Node::length() const {
  InputIndex len = 0;
  for (const Node* node = this; node->inTok; node = node->parent)
    ++len;
  return len;
}

PrefixTree::Node* PrefixTree::Node::randomChild (mt19937& mt) const {
  uniform_real_distribution<double> distrib (0, 1);
  const double r0 = distrib (mt);
  auto rc = child.begin();
  size_t nc = 0;
  for (double r = r0; rc != child.end() && (r -= exp ((**rc).logPrefixProb - logPrefixProb)) > 0; ++rc)
    ++nc;
  LogThisAt(5,"Randomly sampled child #" << nc << (rc == child.end() ? " (terminating)" : "") << " with probability " << (exp((rc == child.end() ? logSeqProb() : (**rc).logPrefixProb) - logPrefixProb)) << " (r=" << r0 << ")" << endl);
  return rc == child.end() ? NULL : *rc;
}

PrefixTree::PrefixTree (const EvaluatedMachine& machine, const vguard<OutputSymbol>& outSym) :
  machine (machine),
  sumInTrans (machine.sumInTrans()),
  output (machine.outputTokenizer.tokenize (outSym)),
  outLen (output.size()),
  nStates (machine.nStates()),
  bestSeqNode (NULL),
  bestLogSeqProb (-numeric_limits<double>::infinity())
{
  addNode (NULL, machine.inputTokenizer.emptyToken());
}

vguard<InputSymbol> PrefixTree::doPrefixSearch() {
  while (!nodeQueue.empty()) {
    Node* parent = bestPrefixNode();
    nodeQueue.pop();
    if (parent->logPrefixProb > bestLogSeqProb)
      extendNode (parent);
    else
      break;
  }

  Assert (bestSeqNode, "No valid sequence found");
  return bestSeq();
}

vguard<InputSymbol> PrefixTree::doRandomSearch (mt19937& mt) {
  Node* current = rootNode();
  while (current->logPrefixProb > current->logSeqProb()) {
    extendNode (current);
    Node* next = current->randomChild (mt);
    if (!next)
      break;
    current = next;
  }
  return seqTraceback (current);
}

#define MinBurnSteps 10
vguard<InputSymbol> PrefixTree::doAnnealedSearch (mt19937& mt, int steps) {
  vguard<InputSymbol> current = doRandomSearch (mt);
  const int burnSteps = current.size() + MinBurnSteps;  // arbitrary burn-in phase
  for (int step = 0; step < steps; ++step) {
    const size_t len = current.size();
    // more to go here:
    //  sample type of event (substitution, insertion, deletion) with weight (len, len+1, len)
    //  sample location of event
    //  calculate logSeqProb (new, old, delta)
    //  divide by temperature for Hastings ratio
    //  accept/reject
  }
  return bestSeq();
}

double PrefixTree::logSeqProb (const vguard<InputSymbol>& input) {
  Node* current = rootNode();
  for (const auto& inSym: input) {
    if (!machine.inputTokenizer.sym2tok.count (inSym))
      return -numeric_limits<double>::infinity();
    const InputToken inTok = machine.inputTokenizer.sym2tok.at(inSym);
    current = addNode (current, inTok);
  }
  return current->logSeqProb();
}

void PrefixTree::extendNode (Node* parent) {
  const InputToken inToks = machine.inputTokenizer.tok2sym.size() - 1;
  LogThisAt (5, "Nodes: " << nodeStore.size() << " Extending " << to_string_join(bestPrefix(),"") << "* (logP " << parent->logPrefixProb << ")" << endl);
  double norm = parent->logSeqProb();
  for (InputToken inTok = 1; inTok <= inToks; ++inTok)
    log_accum_exp (norm, addNode(parent,inTok)->logPrefixProb);
  LogThisAt (6, "log(Sum_x(P(Sx*)) / P(S*)) = " << (norm - parent->logPrefixProb) << endl);
}

PrefixTree::Node* PrefixTree::rootNode() {
  return &nodeStore.front();
}

PrefixTree::Node* PrefixTree::addNode (Node* parent, InputToken inTok) {
  if (parent)
    for (const auto& c: parent->child)
      if (c->inTok == inTok)
	return &*c;
  nodeStore.push_back (Node (*this, parent, inTok));
  Node* nodePtr = &nodeStore.back();
  if (parent)
    parent->child.push_back (nodePtr);
  
  LogThisAt (6, "Adding node " << (parent ? to_string_join (seqTraceback (nodePtr), "") : string("<root>")) << endl);

  nodePtr->fill (*this);
  if (nodePtr->logPrefixProb > bestLogSeqProb)
    nodeQueue.push (nodePtr);

  const double logNodeSeqProb = nodePtr->logSeqProb();
  if (logNodeSeqProb > bestLogSeqProb) {
    bestSeqNode = nodePtr;
    bestLogSeqProb = logNodeSeqProb;
    LogThisAt (4, "Nodes: " << nodeStore.size() << " Best sequence so far: " << to_string_join (bestSeq(), "") << " (" << bestLogSeqProb << ")" << endl);
  }
  LogThisAt (7, "logP(seq)=" << logNodeSeqProb << " logP(seq*)=" << nodePtr->logPrefixProb << " seq: " << to_string_join (seqTraceback (nodePtr), "") << endl);

  return nodePtr;
}

vguard<InputSymbol> PrefixTree::seqTraceback (const Node* node) const {
  return machine.inputTokenizer.detokenize (node->traceback());
}
