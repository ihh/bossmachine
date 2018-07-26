#include "compiler.h"

Compiler::Compiler()
  : showCells (false)
{ }

static const string xvar ("x"), yvar ("y"), paramvar ("p"), buf0var ("buf0"), buf1var ("buf1"), currentvar ("current"), prevvar ("prev"), resultvar ("result"), softplusvar ("sp");
static const string currentcell ("cell"), xcell ("xcell"), ycell ("ycell"), xycell ("xycell");
static const string xidx ("ix"), yidx ("iy"), xmat ("mx"), xvec ("vx"), yvec ("vy");
static const string xsize ("sx"), ysize ("sy");
static const string tab ("  "), tab2 ("    "), tab3 ("      "), tab4 ("      ");

JavaScriptCompiler::JavaScriptCompiler() {
  preamble = string("var ") + softplusvar + " = require('./softplus.js')\n";
  funcKeyword = "function";
  vecRefType = "var";
  constVecRefType = "const";
  arrayRefType = "var";
  cellRefType = "var";
  constCellRefType = "const";
  indexType = "var";
  sizeType = "const";
  sizeMethod = "length";
  weightType = "const";
  logWeightType = "const";
  resultType = "const";
  mathLibrary = "Math.";
  infinity = softplusvar + ".SOFTPLUS_INTLOG_INFINITY";
  realInfinity = "Infinity";
}

string JavaScriptCompiler::declareArray (const string& arrayName, const string& dim1, const string& dim2) const {
  return string("var ") + arrayName + " = new Array(" + dim1 + ").fill(0).map (function() { return new Array (" + dim2 + ").fill(0) });";
}

string JavaScriptCompiler::declareArray (const string& arrayName, const string& dim) const {
  return string("var ") + arrayName + " = new Array(" + dim + ").fill(0);";
}

string JavaScriptCompiler::deleteArray (const string& arrayName) const {
  return string();
}

string JavaScriptCompiler::arrayRowAccessor (const string& arrayName, const string& rowIndex, const string& rowSize) const {
  return arrayName + "[" + rowIndex + "]";
}

string JavaScriptCompiler::binarySoftplus (const string& a, const string& b) const {
  return softplusvar + ".int_logsumexp (" + a + ", " + b + ")";
}

string JavaScriptCompiler::unaryLog (const string& x) const {
  return softplusvar + ".int_log (" + x + ")";
}

string JavaScriptCompiler::boundLog (const string& x) const {
  return softplusvar + ".bound_intlog (" + x + ")";
}

string JavaScriptCompiler::unaryExp (const string& x) const {
  return softplusvar + ".int_exp (" + x + ")";
}

string JavaScriptCompiler::realLog (const string& x) const {
  return softplusvar + ".int_to_log (" + x + ")";
}

string JavaScriptCompiler::warn (const vguard<string>& args) const {
  return string("console.warn (") + join (args, " + ") + ");";
}

string JavaScriptCompiler::makeString (const string& arg) const {
  return arg;
}

string JavaScriptCompiler::toString (const string& arg) const {
  return arg;
}

string JavaScriptCompiler::postamble (const vguard<string>& funcs) const {
  vguard<string> pairs;
  for (const auto& f: funcs)
    pairs.push_back (f + ": " + f);
  return string ("module.exports = { ") + join (pairs, ", ") + " }\n";
}

string JavaScriptCompiler::mapAccessor (const string& obj, const string& key) const {
  return obj + "[\"" + escaped_str(key) + "\"]";
}

string JavaScriptCompiler::constArrayAccessor (const string& obj, const string& key) const {
  return obj + "[" + key + "]";
}

CPlusPlusCompiler::CPlusPlusCompiler() {
  preamble = "#include <vector>\n" "#include <map>\n" "#include <string>\n" "#include <iostream>\n" "using namespace std;\n";
  funcKeyword = "double";
  matrixType = "const vector<vector<double> >& ";
  intVecType = "const vector<int>& ";
  stringType = "const string& ";
  vecRefType = "long long*";
  funcInit = tab + "const SoftPlus " + softplusvar + ";\n";
  constVecRefType = "const long long*";
  paramsType = "const map<string,double>& ";
  arrayRefType = "long long*";
  cellRefType = "long long*";
  constCellRefType = "const long long*";
  indexType = "size_t";
  sizeType = "const size_t";
  sizeMethod = "size()";
  weightType = "const double";
  logWeightType = "const long long";
  resultType = "const double";
  infinity = "SOFTPLUS_INTLOG_INFINITY";
  realInfinity = "numeric_limits<double>::infinity()";
}

Compiler::MachineInfo::MachineInfo (const Compiler& c, const Machine& m)
  : compiler (c),
    wm (m.advancingMachine().ergodicMachine().waitingMachine()),
    eval (wm),
    incoming (wm.nStates())
{
  for (const auto& f_d: wm.defs.defs) {
    const auto f = funcIdx.size();
    funcIdx[f_d.first] = f;
  }
  for (StateIndex s = 0; s < wm.nStates(); ++s) {
    TransIndex t = 0;
    for (const auto& trans: wm.state[s].trans) {
      incoming[trans.dest].push_back (StateTransIndex (s, t));
      ++t;
    }
  }
}

void Compiler::MachineInfo::addTransitions (vguard<string>& exprs, bool withInput, bool withOutput, StateIndex s, InputToken inTok, OutputToken outTok, bool outputWaiting) const {
  if (outputWaiting) {
    if (withInput && !withOutput && !inTok) {
      const string expr = xcell + "[" + to_string (2*s + 1) + "] + " + xvec + "[" + to_string (eval.inputTokenizer.tok2sym.size() - 1) + "]";
      exprs.push_back (expr);
    }
    if (!withInput && !withOutput) {
      const string expr = currentcell + "[" + to_string(2*s) + "]";
      exprs.push_back (expr);
    }
  } else {
    if (withOutput && !withInput && wm.state[s].waits() && !outTok) {
      const string expr = ycell + "[" + to_string(2*s) + "] + " + yvec + "[" + to_string (eval.outputTokenizer.tok2sym.size() - 1) + "]";
      exprs.push_back (expr);
    }
    for (const auto& s_t: incoming[s]) {
      const auto& trans = wm.state[s_t.first].getTransition(s_t.second);
      if (withInput != trans.inputEmpty() && withOutput != trans.outputEmpty()) {
	if (!withInput || !inTok || trans.in == eval.inputTokenizer.tok2sym[inTok])
	  if (!withOutput || !outTok || trans.out == eval.outputTokenizer.tok2sym[outTok]) {
	    string expr = (withOutput ? (withInput ? xycell : ycell) : (withInput ? xcell: currentcell)) + "[" + to_string (2*s_t.first + 1) + "] + " + transVar(s_t.first,s_t.second);
	    if (withInput && !inTok)
	      expr += " + " + xvec + "[" + to_string (eval.inputTokenizer.sym2tok.at(trans.in) - 1) + "]";
	    if (withOutput && !outTok)
	      expr += " + " + yvec + "[" + to_string (eval.outputTokenizer.sym2tok.at(trans.out) - 1) + "]";
	    exprs.push_back (expr);
	  }
      }
    }
  }
}

void Compiler::MachineInfo::storeTransitions (ostream& result, const string& indent, bool withNull, bool withIn, bool withOut, bool withBoth, InputToken inTok, OutputToken outTok, bool start) const {
  for (StateIndex s = 0; s < wm.nStates(); ++s) {
    for (int outputWaiting = 0; outputWaiting < 2; ++outputWaiting) {
      vguard<string> exprs;
      if (start && s == 0 && outputWaiting == 0)
	exprs.push_back ("0");
      if (withIn)
	addTransitions (exprs, true, false, s, inTok, outTok, outputWaiting);
      if (withOut)
	addTransitions (exprs, false, true, s, inTok, outTok, outputWaiting);
      if (withBoth)
	addTransitions (exprs, true, true, s, inTok, outTok, outputWaiting);
      if (withNull)
	addTransitions (exprs, false, false, s, inTok, outTok, outputWaiting);
      result << indent << currentcell << "[" << (2*s + outputWaiting) << "] = " << compiler.logSumExpReduce (exprs, indent + tab, true, outputWaiting) << ";" << endl;
    }
  }
}

string Compiler::MachineInfo::bufRowAccessor (const string& a, const string& r) const {
  return compiler.arrayRowAccessor (a, r, to_string (2*wm.nStates()));
}

string Compiler::MachineInfo::inputRowAccessor (const string& a, const string& r) const {
  return compiler.arrayRowAccessor (a, r, to_string (eval.inputTokenizer.tok2sym.size()));
}

void Compiler::MachineInfo::showCell (ostream& out, const string& indent, bool withInput, bool withOutput) const {
  if (compiler.showCells) {
    vguard<string> desc;
    desc.push_back (string("\"Cell(\""));
    desc.push_back (withInput ? xidx : string("0"));
    desc.push_back (string("\",\""));
    desc.push_back (withOutput ? yidx : string("0"));
    desc.push_back (string("\")\""));
    if (withInput) {
      desc.push_back (string("\" ") + xvec + "(\"");
      const auto& xToks = eval.inputTokenizer.tok2sym;
      for (size_t xTok = 0; xTok < xToks.size(); ++xTok) {
	desc.push_back (string(xTok ? "\" " : "\"") + (xTok == xToks.size() - 1 ? string("-") : escaped_str(xToks[xTok+1])) + ":\"");
	desc.push_back (compiler.valOrInf (xvec + "[" + to_string(xTok) + "]"));
      }
      desc.push_back (string("\")\""));
    }
    if (withOutput) {
      desc.push_back (string("\" ") + yvec + "(\"");
      const auto& yToks = eval.outputTokenizer.tok2sym;
      for (size_t yTok = 0; yTok < yToks.size(); ++yTok) {
	desc.push_back (string(yTok ? "\" " : "\"") + (yTok == yToks.size() - 1 ? string("-") : escaped_str(yToks[yTok+1])) + ":\"");
	desc.push_back (compiler.valOrInf (yvec + "[" + to_string(yTok) + "]"));
      }
      desc.push_back (string("\")\""));
    }
    for (StateIndex s = 0; s < wm.nStates(); ++s) {
      desc.push_back (string("\" ") + escaped_str (wm.state[s].name.dump()) + " go:\"");
      desc.push_back (compiler.valOrInf (currentcell + "[" + to_string(2*s) + "]"));
      desc.push_back (string("\" wait:\""));
      desc.push_back (compiler.valOrInf (currentcell + "[" + to_string(2*s+1) + "]"));
    }
    out << indent << compiler.warn (desc) << endl;
  }
}

string Compiler::valOrInf (const string& arg) const {
  return string("(") + arg + " <= -" + infinity + " ? " + makeString("\"-inf\"") + " : "
    + string("(") + arg + " >= " + infinity + " ? " + makeString("\"inf\"") + " : "
    + toString(arg) + "))";
}

string Compiler::logSumExpReduce (vguard<string>& exprs, const string& lineIndent, bool topLevel, bool alreadyBounded) const {
  const string newLine = string("\n") + lineIndent;
  if (exprs.size() == 0)
    return string("-") + infinity;
  else if (exprs.size() == 1)
    return topLevel ? (alreadyBounded ? exprs[0] : boundLog (exprs[0])) : (newLine + exprs[0]);
  const string lastExpr = exprs.back();
  exprs.pop_back();
  return binarySoftplus (logSumExpReduce (exprs, lineIndent, false, false), newLine + lastExpr);
}

string CPlusPlusCompiler::declareArray (const string& arrayName, const string& dim1, const string& dim2) const {
  return string("long long* ") + arrayName + " = new long long [(" + dim1 + ") * (" + dim2 + ")];";
}

string CPlusPlusCompiler::declareArray (const string& arrayName, const string& dim) const {
  return string("long long* ") + arrayName + " = new long long [" + dim + "];";
}

string CPlusPlusCompiler::deleteArray (const string& arrayName) const {
  return tab + "delete[] " + arrayName + ";\n";
}

string CPlusPlusCompiler::arrayRowAccessor (const string& arrayName, const string& rowIndex, const string& rowSize) const {
  return string("(") + arrayName + " + " + rowSize + " * (" + rowIndex + "))";
}
  
string CPlusPlusCompiler::binarySoftplus (const string& a, const string& b) const {
  return softplusvar + ".int_logsumexp (" + a + ", " + b + ")";
}

string CPlusPlusCompiler::boundLog (const string& x) const {
  return string("SoftPlus::bound_intlog (") + x + ")";
}

string CPlusPlusCompiler::unaryLog (const string& x) const {
  return softplusvar + ".int_log (" + x + ")";
}

string CPlusPlusCompiler::unaryExp (const string& x) const {
  return softplusvar + ".int_exp (" + x + ")";
}

string CPlusPlusCompiler::realLog (const string& x) const {
  return softplusvar + ".int_to_log (" + x + ")";
}

string CPlusPlusCompiler::warn (const vguard<string>& args) const {
  return string("cerr << ") + join (args, " << ") + " << endl;";
}
 
string CPlusPlusCompiler::makeString (const string& arg) const {
  return string("string(") + arg + ")";
}

string CPlusPlusCompiler::toString (const string& arg) const {
  return string("to_string(") + arg + ")";
}

string CPlusPlusCompiler::postamble (const vguard<string>& funcs) const {
  return string();
}

string CPlusPlusCompiler::mapAccessor (const string& obj, const string& key) const {
  return obj + ".at(string(\"" + escaped_str(key) + "\"))";
}

string CPlusPlusCompiler::constArrayAccessor (const string& obj, const string& key) const {
  return obj + ".at(" + key + ")";
}

string Compiler::funcVar (FuncIndex f) { return string("f") + to_string(f+1); }
string Compiler::transVar (StateIndex s, TransIndex t) { return string("t") + to_string(s+1) + "_" + to_string(t+1); }

bool Compiler::isCharAlphabet (const vguard<string>& alph) {
  for (const auto& s: alph)
    if (s.size() != 1)
      return false;
  return true;
}

string Compiler::compileForward (const Machine& m, SeqType xType, SeqType yType, const char* funcName) const {
  Assert (m.nStates() > 0, "Can't compile empty machine");
  Assert (xType != String || isCharAlphabet (m.inputAlphabet()), "Can't use string type for input when input alphabet contains multi-char tokens");
  Assert (yType != String || isCharAlphabet (m.outputAlphabet()), "Can't use string type for output when output alphabet contains multi-char tokens");

  ostringstream out;
  const MachineInfo info (*this, m);
  const Machine& wm (info.wm);

  // header
  out << "// generated automatically by bossmachine, do not edit" << endl;

  // function
  out << preamble;
  out << funcKeyword << " " << funcName << " ("
      << (xType == Profile ? matrixType : (xType == String ? stringType : intVecType)) << xvar << ", "
      << (yType == Profile ? matrixType : (yType == String ? stringType : intVecType)) << yvar << ", "
      << paramsType << paramvar << ") {" << endl;
  out << funcInit;
  
  // sizes, constants
  out << tab << sizeType << " " << xsize << " = " << xvar << "." << sizeMethod << ";" << endl;
  out << tab << sizeType << " " << ysize << " = " << yvar << "." << sizeMethod << ";" << endl;

  // parameters
  const auto params = WeightAlgebra::toposortParams (wm.defs.defs);
  for (const auto& p: params)
    out << tab << weightType << " " << funcVar(info.funcIdx.at(p)) << " = " << info.expr2string(wm.defs.defs.at(p)) << ";" << endl;
  for (StateIndex s = 0; s < wm.nStates(); ++s) {
    TransIndex t = 0;
    for (const auto& trans: wm.state[s].trans) {
      out << tab << logWeightType << " " << transVar(s,t) << " = " << unaryLog (expr2string (trans.weight, info.funcIdx)) << ";" << endl;
      ++t;
    }
  }

  // Declare log-probability matrix (x) & vector (y)
  out << tab << declareArray (xmat, xsize + " + 1", to_string (info.eval.inputTokenizer.tok2sym.size())) << endl;
  out << tab << declareArray (yvec, to_string (info.eval.inputTokenizer.tok2sym.size())) << endl;
  
  // Declare DP matrix arrays
  // Indexing convention: buf[xIndex][2*state + (yWaitFlag ? 1 : 0)]
  out << tab << declareArray (buf0var, xsize + " + 1", to_string (2*info.wm.nStates())) << endl;
  out << tab << declareArray (buf1var, xsize + " + 1", to_string (2*info.wm.nStates())) << endl;
  out << tab << indexType << " " << xidx << " = 0, " << yidx << ";" << endl;

  // Fill DP matrix
  // x=0, y=0
  out << tab << "{" << endl;
  out << tab2 << cellRefType << " " << currentcell << " = " << info.bufRowAccessor (buf0var, "0") << ";" << endl;

  info.storeTransitions (out, tab2, true, false, false, false, 0, 0, true);
  info.showCell (out, tab2, false, false);

  out << tab << "}" << endl;

  // x>0, y=0
  out << tab << "for (" << xidx << " = 1; " << xidx << " <= " << xsize << "; ++" << xidx << ") {" << endl;

  out << tab2 << cellRefType << " " << currentcell << " = " << info.bufRowAccessor (buf0var, xidx) << ";" << endl;
  out << tab2 << constCellRefType << " " << xcell << " = " << info.bufRowAccessor (buf0var, xidx + " - 1") << ";" << endl;

  string xtab;
  switch (xType) {
  case Profile:
    out << tab2 << vecRefType << " " << xvec << " = " << info.inputRowAccessor (xmat, xidx + " - 1") << ";" << endl;
    for (InputToken xTok = 0; xTok < info.eval.inputTokenizer.tok2sym.size(); ++xTok)
      out << tab2 << xvec << "[" << xTok << "] = " << unaryLog (constArrayAccessor (constArrayAccessor (xvar, xidx + " - 1"), to_string(xTok))) << ";" << endl;
    break;
  case IntVec:
  case String:
    xtab = tab2;
    out << tab2 << "switch (" << xvar << "[" << xidx << " - 1]) {" << endl;
    break;
  default:
    Abort ("Unknown sequence type");
  }

  for (InputToken xTok = (xType == Profile ? 0 : 1); xTok < (xType == Profile ? 1 : info.eval.inputTokenizer.tok2sym.size()); ++xTok) {
    if (xType == IntVec)
      out << xtab << tab << "case " << (xTok - 1) << ":" << endl;
    else if (xType == String)
      out << xtab << tab << "case '" << info.eval.inputTokenizer.tok2sym[xTok] << "':" << endl;

    info.storeTransitions (out, xtab + tab2, true, true, false, false, xTok, 0, false);
    info.showCell (out, xtab + tab2, true, false);

    if (xType != Profile)
      out << xtab << tab2 << "break;" << endl;
  }

  if (xType != Profile)
    out << xtab << tab << "default:" << endl
	<< xtab << tab2 << "return " << realInfinity << ";" << endl
	<< xtab << tab2 << "break;" << endl
	<< tab2 << "}" << endl;

  out << tab << "}" << endl;

  // y>0
  out << tab << "for (" << yidx << " = 1; " << yidx << " <= " << ysize << "; ++" << yidx << ") {" << endl;

  out << tab2 << arrayRefType << " " << currentvar << " = " << yidx << " & 1 ? " << buf1var << " : " << buf0var << ";" << endl;
  out << tab2 << arrayRefType << " " << prevvar << " = " << yidx << " & 1 ? " << buf0var << " : " << buf1var << ";" << endl;

  string ytab;
  switch (yType) {
  case Profile:
    for (OutputToken yTok = 0; yTok < info.eval.outputTokenizer.tok2sym.size(); ++yTok)
      out << tab2 << yvec << "[" << yTok << "] = " << unaryLog (constArrayAccessor (constArrayAccessor (yvar, yidx + " - 1"), to_string(yTok))) << ";" << endl;
    break;
  case IntVec:
  case String:
    ytab = tab2;
    out << tab2 << "switch (" << yvar << "[" << yidx << " - 1]) {" << endl;
    break;
  default:
    Abort ("Unknown sequence type");
  }

  for (OutputToken yTok = (yType == Profile ? 0 : 1); yTok < (yType == Profile ? 1 : info.eval.outputTokenizer.tok2sym.size()); ++yTok) {
    if (yType == IntVec)
      out << ytab << tab << "case " << (yTok - 1) << ":" << endl;
    else if (yType == String)
      out << ytab << tab << "case '" << info.eval.outputTokenizer.tok2sym[yTok] << "':" << endl;

    // x=0, y>0
    out << ytab << tab2 << "{" << endl;
    out << ytab << tab3 << cellRefType << " " << currentcell << " = " << info.bufRowAccessor (currentvar, "0") << ";" << endl;
    out << ytab << tab3 << constCellRefType << " " << ycell << " = " << info.bufRowAccessor (prevvar, "0") << ";" << endl;

    info.storeTransitions (out, ytab + tab3, true, false, true, false, 0, yTok, false);
    info.showCell (out, ytab + tab3, false, true);

    out << ytab << tab2 << "}" << endl;

    // x>0, y>0
    out << ytab << tab2 << "for (" << xidx << " = 1; " << xidx << " <= " << xsize << "; ++" << xidx << ") {" << endl;

    out << ytab << tab3 << cellRefType << " " << currentcell << " = " << info.bufRowAccessor (currentvar, xidx) << ";" << endl;
    out << ytab << tab3 << constCellRefType << " " << xcell << " = " << info.bufRowAccessor (currentvar, xidx + " - 1") << ";" << endl;
    out << ytab << tab3 << constCellRefType << " " << ycell << " = " << info.bufRowAccessor (prevvar, xidx) << ";" << endl;
    out << ytab << tab3 << constCellRefType << " " << xycell << " = " << info.bufRowAccessor (prevvar, xidx + " - 1") << ";" << endl;

    string xytab (ytab);
    switch (xType) {
    case Profile:
      out << ytab << tab3 << constVecRefType << " " << xvec << " = " << info.inputRowAccessor (xmat, xidx + " - 1") << ";" << endl;
      break;
    case IntVec:
    case String:
      xytab = ytab + tab2;
      out << ytab << tab3 << "switch (" << xvar << "[" << xidx << " - 1]) {" << endl;
      break;
    default:
      Abort ("Unknown sequence type");
    }

    for (InputToken xTok = (xType == Profile ? 0 : 1); xTok < (xType == Profile ? 1 : info.eval.inputTokenizer.tok2sym.size()); ++xTok) {
      if (xType == IntVec)
	out << xytab << tab2 << "case " << (xTok - 1) << ":" << endl;
      else if (xType == String)
	out << xytab << tab2 << "case '" << info.eval.inputTokenizer.tok2sym[xTok] << "':" << endl;

      info.storeTransitions (out, xytab + tab3, true, true, true, true, xTok, yTok, false);
      info.showCell (out, xytab + tab3, true, true);

      if (xType != Profile)
	out << xytab << tab3 << "break;" << endl;
    }

    if (xType != Profile)
      out << xytab << tab2 << "default:" << endl
	  << xytab << tab3 << "return " << realInfinity << ";" << endl
	  << xytab << tab3 << "break;" << endl
	  << xytab << tab << "}" << endl;

    out << ytab << tab2 << "}" << endl;  // end xidx loop

    if (yType != Profile)
      out << ytab << tab2 << "break;" << endl;
  }

  if (yType != Profile)
    out << ytab << tab << "default:" << endl
	<< ytab << tab2 << "return " << realInfinity << ";" << endl
	<< ytab << tab2 << "break;" << endl
	<< ytab << tab << "}" << endl;

  out << tab << "}" << endl;  // end yidx loop

  // get result
  out << tab << resultType << " " << resultvar << " = " << realLog (info.bufRowAccessor (string("(") + ysize + " & 1 ? " + buf1var + " : " + buf0var + ")", xsize) + "[" + to_string (2*info.wm.nStates() - 1) + "]") << ";" << endl;
  
  // delete
  out << deleteArray (xmat);
  out << deleteArray (yvec);
  out << deleteArray (buf0var);
  out << deleteArray (buf1var);
  
  // return
  out << tab << "return " << resultvar << ";" << endl;
  out << "}" << endl;  // end function

  vguard<string> funcNames;
  funcNames.push_back (string (funcName));
  out << postamble (funcNames);
  
  return out.str();
}

string Compiler::expr2string (const WeightExpr& w, const map<string,FuncIndex>& funcIdx, int parentPrecedence) const {
  ostringstream expr;
  const ExprType op = w->type;
  switch (op) {
  case ExprType::Null: expr << 0; break;
  case ExprType::Int: expr << w->args.intValue; break;
  case ExprType::Dbl: expr << w->args.doubleValue; break;
  case ExprType::Param:
    {
      const string& n (*w->args.param);
      if (funcIdx.count(n))
	expr << funcVar (funcIdx.at (n));
      else
	expr << mapAccessor (paramvar, n);
    }
    break;
  case ExprType::Log:
  case ExprType::Exp:
    expr << mathLibrary << (op == Log ? "log" : "exp") << "(" << expr2string(w->args.arg,funcIdx) << ")";
    break;
  case ExprType::Pow:
    expr << mathLibrary << "pow(" << expr2string(w->args.binary.l,funcIdx) << "," << expr2string(w->args.binary.r,funcIdx) << ")";
    break;
  default:
    // Precedence rules

    // a*b: rank 2
    // a needs () if it's anything except a multiplication or division [parent rank 2]
    // b needs () if it's anything except a multiplication or division [parent rank 2]

    // a/b: rank 2
    // a needs () if it's anything except a multiplication or division [parent rank 2]
    // b needs () if it's anything except a constant/function [parent rank 3]

    // a-b: rank 1
    // a never needs () [parent rank 0]
    // b needs () if it's anything except a multiplication or division [parent rank 2]

    // a+b: rank 1
    // a never needs () [parent rank 0]
    // b never needs () [parent rank 0]

    int p, l, r;
    string opcode;
    if (op == Mul) { p = l = r = 2; opcode = "*"; }
    else if (op == Div) { p = l = 2; r = 3; opcode = "/"; }
    else if (op == Sub) { p = 1; l = 0; r = 2; opcode = "-"; }
    else if (op == Add) { p = 1; l = r = 0; opcode = "+"; }
    expr << (parentPrecedence > p ? "(" : "")
	 << expr2string(w->args.binary.l,funcIdx,l)
	 << opcode
	 << expr2string(w->args.binary.r,funcIdx,r)
	 << (parentPrecedence > p ? ")" : "");
    break;
  }
  return expr.str();
}
