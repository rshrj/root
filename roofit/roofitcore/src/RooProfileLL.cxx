 /*****************************************************************************
  * Project: RooFit                                                           *
  *                                                                           *
  * Copyright (c) 2000-2005, Regents of the University of California          *
  *                          and Stanford University. All rights reserved.    *
  *                                                                           *
  * Redistribution and use in source and binary forms,                        *
  * with or without modification, are permitted according to the terms        *
  * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
  *****************************************************************************/

/**
\file RooProfileLL.cxx
\class RooProfileLL
\ingroup Roofitcore

Implements the profile likelihood estimator for
a given likelihood and set of parameters of interest. The value return by
RooProfileLL is the input likelihood nll minimized w.r.t all nuisance parameters
(which are all parameters except for those listed in the constructor) minus
the -log(L) of the best fit. Note that this function is slow to evaluate
as a MIGRAD minimization step is executed for each function evaluation
**/

#include "Riostream.h"

#include "RooProfileLL.h"
#include "RooAbsReal.h"
#include "RooMinimizer.h"
#include "RooMsgService.h"
#include "RooRealVar.h"



////////////////////////////////////////////////////////////////////////////////
/// Constructor of profile likelihood given input likelihood nll w.r.t
/// the given set of variables. The input log likelihood is minimized w.r.t
/// to all other variables of the likelihood at each evaluation and the
/// value of the global log likelihood minimum is always subtracted.

RooProfileLL::RooProfileLL(const char *name, const char *title,
            RooAbsReal& nllIn, const RooArgSet& observables) :
  RooAbsReal(name,title),
  _nll("input","-log(L) function",this,nllIn),
  _obs("paramOfInterest","Parameters of interest",this),
  _par("nuisanceParam","Nuisance parameters",this,false,false)
{
  // Determine actual parameters and observables
  nllIn.getObservables(&observables, _obs) ;
  nllIn.getParameters(&observables, _par) ;
}



////////////////////////////////////////////////////////////////////////////////
/// Copy constructor

RooProfileLL::RooProfileLL(const RooProfileLL &other, const char *name)
   : RooAbsReal(other, name),
     _nll("input", this, other._nll),
     _obs(this, other._obs),
     _par(this, other._par),
     _startFromMin(other._startFromMin),
     _paramFixed(other._paramFixed)
{
  _paramAbsMin.addClone(other._paramAbsMin) ;
  _obsAbsMin.addClone(other._obsAbsMin) ;

}


////////////////////////////////////////////////////////////////////////////////

const RooArgSet& RooProfileLL::bestFitParams() const
{
  validateAbsMin() ;
  return _paramAbsMin ;
}


////////////////////////////////////////////////////////////////////////////////

const RooArgSet& RooProfileLL::bestFitObs() const
{
  validateAbsMin() ;
  return _obsAbsMin ;
}




////////////////////////////////////////////////////////////////////////////////
/// Optimized implementation of createProfile for profile likelihoods.
/// Return profile of original function in terms of stated parameters
/// of interest rather than profiling recursively.

RooFit::OwningPtr<RooAbsReal> RooProfileLL::createProfile(const RooArgSet& paramsOfInterest)
{
  return nll().createProfile(paramsOfInterest) ;
}




////////////////////////////////////////////////////////////////////////////////

void RooProfileLL::initializeMinimizer() const
{
  coutI(Minimization) << "RooProfileLL::evaluate(" << GetName() << ") Creating instance of MINUIT" << std::endl;

  bool smode = RooMsgService::instance().silentMode() ;
  RooMsgService::instance().setSilentMode(true) ;
  _minimizer = std::make_unique<RooMinimizer>(const_cast<RooAbsReal&>(_nll.arg())) ;
  if (!smode) RooMsgService::instance().setSilentMode(false) ;

}



////////////////////////////////////////////////////////////////////////////////
/// Evaluate profile likelihood by minimizing likelihood w.r.t. all
/// parameters that are not considered observables of this profile
/// likelihood object.

double RooProfileLL::evaluate() const
{
  // Instantiate minimizer if we haven't done that already
  if (!_minimizer) {
    initializeMinimizer() ;
  }

  // Save current value of observables
  RooArgSet obsSetOrig;
  _obs.snapshot(obsSetOrig) ;

  validateAbsMin() ;


  // Set all observables constant in the minimization
  const_cast<RooSetProxy&>(_obs).setAttribAll("Constant",true) ;
  ccoutP(Eval) << "." ; ccoutP(Eval).flush() ;

  // If requested set initial parameters to those corresponding to absolute minimum
  if (_startFromMin) {
    _par.assign(_paramAbsMin) ;
  }

  _minimizer->zeroEvalCount() ;

  _minimizer->migrad() ;
  _neval = _minimizer->evalCounter() ;

  // Restore original values and constant status of observables
  for(auto const& arg : obsSetOrig) {
    assert(dynamic_cast<RooRealVar*>(arg));
    auto var = static_cast<RooRealVar*>(arg);
    auto target = static_cast<RooRealVar*>(_obs.find(var->GetName())) ;
    target->setVal(var->getVal()) ;
    target->setConstant(var->isConstant()) ;
  }

  return _nll - _absMin ;
}



////////////////////////////////////////////////////////////////////////////////
/// Check that parameters and likelihood value for 'best fit' are still valid. If not,
/// because the best fit has never been calculated, or because constant parameters have
/// changed value or parameters have changed const/float status, the minimum is recalculated

void RooProfileLL::validateAbsMin() const
{
  // Check if constant status of any of the parameters have changed
  if (_absMinValid) {
    for(auto const& par : _par) {
      if (_paramFixed[par->GetName()] != par->isConstant()) {
   cxcoutI(Minimization) << "RooProfileLL::evaluate(" << GetName() << ") constant status of parameter " << par->GetName() << " has changed from "
            << (_paramFixed[par->GetName()]?"fixed":"floating") << " to " << (par->isConstant()?"fixed":"floating")
            << ", recalculating absolute minimum" << std::endl;
   _absMinValid = false ;
   break ;
      }
    }
  }


  // If we don't have the absolute minimum w.r.t all observables, calculate that first
  if (!_absMinValid) {

    cxcoutI(Minimization) << "RooProfileLL::evaluate(" << GetName() << ") determining minimum likelihood for current configurations w.r.t all observable" << std::endl;


    if (!_minimizer) {
      initializeMinimizer() ;
    }

    // Save current values of non-marginalized parameters
    RooArgSet obsStart;
    _obs.snapshot(obsStart, false) ;

    // Start from previous global minimum
    if (!_paramAbsMin.empty()) {
      const_cast<RooSetProxy&>(_par).assignValueOnly(_paramAbsMin) ;
    }
    if (!_obsAbsMin.empty()) {
      const_cast<RooSetProxy&>(_obs).assignValueOnly(_obsAbsMin) ;
    }

    // Find minimum with all observables floating
    const_cast<RooSetProxy&>(_obs).setAttribAll("Constant",false) ;

    _minimizer->migrad() ;

    // Save value and remember
    _absMin = _nll ;
    _absMinValid = true ;

    // Save parameter values at abs minimum as well
    _paramAbsMin.removeAll() ;

    // Only store non-constant parameters here!
    _paramAbsMin.addClone(*std::unique_ptr<RooArgSet>{_par.selectByAttrib("Constant", false)});

    _obsAbsMin.addClone(_obs) ;

    // Save constant status of all parameters
    for(auto const& par : _par) {
      _paramFixed[par->GetName()] = par->isConstant() ;
    }

    if (dologI(Minimization)) {
      cxcoutI(Minimization) << "RooProfileLL::evaluate(" << GetName() << ") minimum found at (" ;

      bool first=true ;
      for(auto const& arg : _obs) {
        ccxcoutI(Minimization) << (first?"":", ") << arg->GetName() << "="
                               << static_cast<RooAbsReal const*>(arg)->getVal() ;
        first=false ;
      }
      ccxcoutI(Minimization) << ")" << std::endl;
    }

    // Restore original parameter values
    _obs.assign(obsStart) ;

  }
}



////////////////////////////////////////////////////////////////////////////////

bool RooProfileLL::redirectServersHook(const RooAbsCollection& newServerList, bool mustReplaceAll,
                                       bool nameChange, bool isRecursive)
{
  _minimizer.reset();
  return RooAbsReal::redirectServersHook(newServerList, mustReplaceAll, nameChange, isRecursive);
}
