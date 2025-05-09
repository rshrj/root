// @(#)root/treeviewer:$Id$
// Author: Bastien Dalla Piazza  02/08/2007

/*************************************************************************
 * Copyright (C) 1995-2007, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TParallelCoord.h"
#include "TParallelCoordVar.h"
#include "TParallelCoordRange.h"

#include <cfloat>
#include <iostream>

#include "TROOT.h"
#include "TVirtualPad.h"
#include "TPolyLine.h"
#include "TGraph.h"
#include "TPaveText.h"
#include "TStyle.h"
#include "TEntryList.h"
#include "TFrame.h"
#include "TTree.h"
#include "TTreePlayer.h"
#include "TSelectorDraw.h"
#include "TTreeFormula.h"
#include "TView.h"
#include "TRandom.h"
#include "TCanvas.h"
#include "TGaxis.h"
#include "TFile.h"

ClassImp(TParallelCoord);

/** \class TParallelCoord
Parallel Coordinates class.

The multidimensional system of Parallel coordinates is a common way of studying
high-dimensional geometry and visualizing multivariate problems. It has first
been proposed by A. Inselberg in 1981.

To show a set of points in an n-dimensional space, a backdrop is drawn
consisting of n parallel lines. A point in n-dimensional space is represented as
a polyline with vertices on the parallel axes; the position of the vertex on the
i-th axis corresponds to the i-th coordinate of the point.

This tool comes with a rather large gui in the editor. It is necessary to use
this editor in order to explore a data set, as explained below.

### Reduce cluttering:

The main issue for parallel coordinates is the very high cluttering of the
output when dealing with large data set. Two techniques have been implemented to
bypass that so far:

  - Draw doted lines instead of plain lines with an adjustable dots spacing. A
    slider to adjust the dots spacing is available in the editor.
  - Sort the entries to display with  a "weight cut". On each axis is drawn a
    histogram describing the distribution of the data on the corresponding
    variable. The "weight" of an entry is the sum of the bin content of each bin
    the entry is going through. An entry going through the histograms peaks will
    have a big weight wether an entry going randomly through the histograms will
    have a rather small weight. Setting a cut on this weight allows to draw only
    the most representative entries. A slider set the cut is also available in
    the gui.

## Selections:

Selections of specific entries can be defined over the data se using parallel
coordinates. With that representation, a selection is an ensemble of ranges
defined on the axes. Ranges defined on the same axis are conjugated with OR
(an entry must be in one or the other ranges to be selected). Ranges on
different axes are are conjugated with AND (an entry must be in all the ranges
to be selected). Several selections can be defined with different colors. It is
possible to generate an entry list from a given selection and apply it to the
tree using the editor ("Apply to tree" button).

## Axes:

Options can be defined each axis separately using the right mouse click. These
options can be applied to every axes using the editor.

  - Axis width: If set to 0, the axis is simply a line. If higher, a color
    histogram is drawn on the axis.
  - Axis histogram height: If not 0, a usual bar histogram is drawn on the plot.

The order in which the variables are drawn is essential to see the clusters. The
axes can be dragged to change their position. A zoom is also available. The
logarithm scale is also available by right clicking on the axis.

## Candle chart:

TParallelCoord can also be used to display a candle chart. In that mode, every
variable is drawn in the same scale. The candle chart can be combined with the
parallel coordinates mode, drawing the candle sticks over the axes.

~~~ {.cpp}
{
   TCanvas *c1 = new TCanvas("c1");
   TFile *f = TFile::Open("cernstaff.root");
   TTree *T = (TTree*)f->Get("T");
   T->Draw("Age:Grade:Step:Cost:Division:Nation","","para");
   TParallelCoord* para = (TParallelCoord*)gPad->GetListOfPrimitives()->FindObject("ParaCoord");
   TParallelCoordVar* grade = (TParallelCoordVar*)para->GetVarList()->FindObject("Grade");
   grade->AddRange(new TParallelCoordRange(grade,11.5,14));
   para->AddSelection("less30");
   para->GetCurrentSelection()->SetLineColor(kViolet);
   TParallelCoordVar* age = (TParallelCoordVar*)para->GetVarList()->FindObject("Age");
   age->AddRange(new TParallelCoordRange(age,21,30));
}
~~~

### Some references:

  - Alfred Inselberg's Homepage <http://www.math.tau.ac.il/~aiisreal>, with
    Visual Tutorial, History, Selected Publications and Applications.
  - Almir Olivette Artero, Maria Cristina Ferreira de Oliveira, Haim Levkowitz,
    "Uncovering Clusters in Crowded Parallel Coordinates Visualizations,"
    infovis, pp. 81-88,  IEEE Symposium on Information Visualization
    (INFOVIS'04),  2004.
*/

////////////////////////////////////////////////////////////////////////////////
/// Default constructor.

TParallelCoord::TParallelCoord()
   :TNamed()
{
   Init();
}

////////////////////////////////////////////////////////////////////////////////
/// Constructor without a reference to a tree,
/// the datas must be added afterwards with
/// TParallelCoord::AddVariable(Double_t*,const char*).

TParallelCoord::TParallelCoord(Long64_t nentries)
{
   Init();
   fNentries = nentries;
   fCurrentN = fNentries;
   fVarList = new TList();
   fSelectList = new TList();
   fCurrentSelection = new TParallelCoordSelect();
   fSelectList->Add(fCurrentSelection);
}

////////////////////////////////////////////////////////////////////////////////
/// Normal constructor, the datas must be added afterwards
/// with TParallelCoord::AddVariable(Double_t*,const char*).

TParallelCoord::TParallelCoord(TTree* tree, Long64_t nentries)
   :TNamed("ParaCoord","ParaCoord")
{
   Init();
   Int_t estimate = tree->GetEstimate();
   if (nentries>estimate) {
      Warning("TParallelCoord","Call tree->SetEstimate(tree->GetEntries()) to display all the tree variables");
      fNentries = estimate;
   } else {
      fNentries = nentries;
   }
   fCurrentN = fNentries;
   fTree = tree;
   fTreeName = fTree->GetName();
   if (fTree->GetCurrentFile()) fTreeFileName = fTree->GetCurrentFile()->GetName();
   else fTreeFileName = "";
   fVarList = new TList();
   fSelectList = new TList();
   fCurrentSelection = new TParallelCoordSelect();
   fSelectList->Add(fCurrentSelection);
}

////////////////////////////////////////////////////////////////////////////////
/// Destructor.

TParallelCoord::~TParallelCoord()
{
   if (fInitEntries != fCurrentEntries && fCurrentEntries != nullptr) delete fCurrentEntries;
   if (fVarList) {
      fVarList->Delete();
      delete fVarList;
   }
   if (fSelectList) {
      fSelectList->Delete();
      delete fSelectList;
   }
   if (fCandleAxis) delete fCandleAxis;
   SetDotsSpacing(0);
}

////////////////////////////////////////////////////////////////////////////////
/// Add a variable.

void TParallelCoord::AddVariable(Double_t* val, const char* title)
{
   ++fNvar;
   fVarList->Add(new TParallelCoordVar(val,title,fVarList->GetSize(),this));
   SetAxesPosition();
}

////////////////////////////////////////////////////////////////////////////////
/// Add a variable from an expression.

void TParallelCoord::AddVariable(const char* varexp)
{
   if(!fTree) return; // The tree from which one will get the data must be defined.

   // Select in the only the entries of this TParallelCoord.
   TEntryList *list = GetEntryList(false);
   fTree->SetEntryList(list);

   // ensure that there is only one variable given:

   TString exp = varexp;

   if (exp.Contains(':') || exp.Contains(">>") || exp.Contains("<<")) {
      Warning("AddVariable","Only a single variable can be added at a time.");
      return;
   }
   if (exp == ""){
      Warning("AddVariable","Nothing to add");
      return;
   }

   Long64_t en = fTree->Draw(varexp,"","goff");
   if (en<0) {
      Warning("AddVariable","%s could not be evaluated",varexp);
      return;
   }

   AddVariable(fTree->GetV1(),varexp);
}

////////////////////////////////////////////////////////////////////////////////
/// Add a selection.

void TParallelCoord::AddSelection(const char* title)
{
   TParallelCoordSelect *sel = new TParallelCoordSelect(title);
   fSelectList->Add(sel);
   fCurrentSelection = sel;
}

////////////////////////////////////////////////////////////////////////////////
/// Apply the current selection to the tree.

void  TParallelCoord::ApplySelectionToTree()
{
   if(!fTree) return;
   if(fSelectList) {
      if(fSelectList->GetSize() == 0) return;
      if(fCurrentSelection == nullptr) fCurrentSelection = (TParallelCoordSelect*)fSelectList->First();
   }
   fCurrentEntries = GetEntryList();
   fNentries = fCurrentEntries->GetN();
   fCurrentFirst = 0;
   fCurrentN = fNentries;
   fTree->SetEntryList(fCurrentEntries);
   TString varexp;
   TIter next(fVarList);
   while (auto var = (TParallelCoordVar*)next())
      varexp.Append(TString::Format(":%s",var->GetTitle()));
   varexp.Remove(TString::kLeading,':');
   TSelectorDraw* selector = (TSelectorDraw*)((TTreePlayer*)fTree->GetPlayer())->GetSelector();
   fTree->Draw(varexp.Data(),"","goff");
   next.Reset();
   Int_t i = 0;
   while (auto var = (TParallelCoordVar*)next())
      var->SetValues(fNentries, selector->GetVal(i++));
   if (fSelectList) {           // FIXME It would be better to update the selections by deleting
      fSelectList->Delete();    // the meaningless ranges (selecting everything or nothing for example)
      fCurrentSelection = nullptr;    // after applying a new entrylist to the tree.
   }
   gPad->Modified();
   gPad->Update();
}

////////////////////////////////////////////////////////////////////////////////
/// Call constructor and add the variables.

void  TParallelCoord::BuildParallelCoord(TSelectorDraw* selector, bool candle)
{
   TParallelCoord* pc = new TParallelCoord(selector->GetTree(),selector->GetNfill());
   pc->SetBit(kCanDelete);
   selector->SetObject(pc);
   TString varexp;
   for(Int_t i=0;i<selector->GetDimension();++i) {
      if (selector->GetVal(i)) {
         if (selector->GetVar(i)) {
            pc->AddVariable(selector->GetVal(i),selector->GetVar(i)->GetTitle());
            varexp.Append(TString::Format(":%s",selector->GetVar(i)->GetTitle()));
         }
      }
   }
   varexp.Remove(TString::kLeading,':');
   if (selector->GetSelect())
      varexp.Append(TString::Format("{%s}",selector->GetSelect()->GetTitle()));
   pc->SetTitle(varexp.Data());
   if (!candle) pc->Draw();
   else pc->Draw("candle");
}

////////////////////////////////////////////////////////////////////////////////
/// Clean up the selections from the ranges which could have been deleted
/// when a variable has been deleted.

void TParallelCoord::CleanUpSelections(TParallelCoordRange* range)
{
   TIter next(fSelectList);
   TParallelCoordSelect* select;
   while ((select = (TParallelCoordSelect*)next())){
      if(select->Contains(range)) select->Remove(range);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Delete a selection.

void TParallelCoord::DeleteSelection(TParallelCoordSelect * sel)
{
   fSelectList->Remove(sel);
   delete sel;
   if(fSelectList->GetSize() == 0) fCurrentSelection = nullptr;
   else fCurrentSelection = (TParallelCoordSelect*)fSelectList->At(0);
}

////////////////////////////////////////////////////////////////////////////////
/// Compute the distance from the TParallelCoord.

Int_t TParallelCoord::DistancetoPrimitive(Int_t px, Int_t py)
{
   if(!gPad) return 9999;

   TFrame *frame = gPad->GetFrame();

   Double_t x1,x2,y1,y2,xx,yy;

   x1 = frame->GetX1()+0.01;
   x2 = frame->GetX2()-0.01;
   y2 = frame->GetY2()-0.01;
   y1 = frame->GetY1()+0.01;

   xx = gPad->AbsPixeltoX(px);
   yy = gPad->AbsPixeltoY(py);

   if(xx>x1 && xx<x2 && yy>y1 && yy<y2) return 0;
   else                                 return 9999;
}

////////////////////////////////////////////////////////////////////////////////
/// Draw the parallel coordinates graph.

void TParallelCoord::Draw(Option_t* option)
{
   if (!GetTree()) return;
   if (!fCurrentEntries) fCurrentEntries = fInitEntries;
   bool optcandle = false;
   TString opt = option;
   opt.ToLower();
   if(opt.Contains("candle")) {
      optcandle = true;
      opt.ReplaceAll("candle","");
   }
   if(optcandle) {
      SetBit(kPaintEntries,false);
      SetBit(kCandleChart,true);
      SetGlobalScale(true);
   }

   if (gPad) {
      if (!gPad->IsEditable()) gROOT->MakeDefCanvas();
   } else gROOT->MakeDefCanvas();
   TView *view = gPad->GetView();
   if(view){
      delete view;
      gPad->SetView(nullptr);
   }
   gPad->Clear();
   if (!optcandle) {
      if (gPad && gPad->IsA() == TCanvas::Class()
         && !((TCanvas*)gPad)->GetShowEditor()) {
         ((TCanvas*)gPad)->ToggleEditor();
         ((TCanvas*)gPad)->ToggleEventStatus();
      }
   }

   gPad->SetBit(TGraph::kClipFrame,true);

   TFrame *frame = new TFrame(0.1,0.1,0.9,0.9);
   frame->SetBorderSize(0);
   frame->SetBorderMode(0);
   frame->SetFillStyle(0);
   frame->SetLineColor(gPad->GetFillColor());
   frame->Draw();
   AppendPad(option);
   TPaveText *title = new TPaveText(0.05,0.95,0.35,1);
   title->AddText(GetTitle());
   title->Draw();
   SetAxesPosition();
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      if(optcandle) {
         var->SetBoxPlot(true);
         var->SetHistogramHeight(0.5);
         var->SetHistogramLineWidth(0);
      }
   }

   if (optcandle) {
      if (TestBit(kVertDisplay)) fCandleAxis = new TGaxis(0.05,0.1,0.05,0.9,GetGlobalMin(),GetGlobalMax());
      else                       fCandleAxis = new TGaxis(0.1,0.05,0.9,0.05,GetGlobalMin(),GetGlobalMax());
      fCandleAxis->Draw();
   }

   if (gPad && gPad->IsA() == TCanvas::Class())
      ((TCanvas*)gPad)->Selected(gPad,this,1);
}

////////////////////////////////////////////////////////////////////////////////
/// Execute the corresponding entry.

void TParallelCoord::ExecuteEvent(Int_t /*entry*/, Int_t /*px*/, Int_t /*py*/)
{
   if (!gPad) return;
   gPad->SetCursor(kHand);
}

////////////////////////////////////////////////////////////////////////////////
/// Return the selection currently being edited.

TParallelCoordSelect* TParallelCoord::GetCurrentSelection()
{
   if (!fSelectList) return nullptr;
   if (!fCurrentSelection) {
      fCurrentSelection = (TParallelCoordSelect*)fSelectList->First();
   }
   return fCurrentSelection;
}

////////////////////////////////////////////////////////////////////////////////
/// Get the whole entry list or one for a selection.

TEntryList* TParallelCoord::GetEntryList(bool sel)
{
   if(!sel || fCurrentSelection->GetSize() == 0){ // If no selection is specified, return the entry list of all the entries.
      return fInitEntries;
   } else { // return the entry list corresponding to the current selection.
      TEntryList *enlist = new TEntryList(fTree);
      TIter next(fVarList);
      for (Long64_t li=0;li<fNentries;++li) {
         next.Reset();
         bool inrange=true;
         TParallelCoordVar* var;
         while((var = (TParallelCoordVar*)next())){
            if(!var->Eval(li,fCurrentSelection)) inrange = false;
         }
         if(!inrange) continue;
         enlist->Enter(fCurrentEntries->GetEntry(li));
      }
      return enlist;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// return the global maximum.

Double_t TParallelCoord::GetGlobalMax()
{
   Double_t gmax=-DBL_MAX;
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      if (gmax < var->GetCurrentMax()) gmax = var->GetCurrentMax();
   }
   return gmax;
}

////////////////////////////////////////////////////////////////////////////////
/// return the global minimum.

Double_t TParallelCoord::GetGlobalMin()
{
   Double_t gmin=DBL_MAX;
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      if (gmin > var->GetCurrentMin()) gmin = var->GetCurrentMin();
   }
   return gmin;
}

////////////////////////////////////////////////////////////////////////////////
/// get the binning of the histograms.

Int_t TParallelCoord::GetNbins()
{
   return ((TParallelCoordVar*)fVarList->First())->GetNbins();
}

////////////////////////////////////////////////////////////////////////////////
/// Get a selection from its title.

TParallelCoordSelect* TParallelCoord::GetSelection(const char* title)
{
   TIter next(fSelectList);
   TParallelCoordSelect* sel;
   while ((sel = (TParallelCoordSelect*)next()) && strcmp(title,sel->GetTitle())) { }
   return sel;
}

////////////////////////////////////////////////////////////////////////////////
/// return the tree if fTree is defined. If not, the method try to load the tree
/// from fTreeFileName.

TTree* TParallelCoord::GetTree()
{
   if (fTree) return fTree;
   if (fTreeFileName=="" || fTreeName=="") {
      Error("GetTree","Cannot load the tree: no tree defined!");
      return nullptr;
   }
   TFile *f = TFile::Open(fTreeFileName.Data());
   if (!f) {
      Error("GetTree","Tree file name : \"%s\" does not exist (Are you in the correct directory?).",fTreeFileName.Data());
      return nullptr;
   } else if (f->IsZombie()) {
      Error("GetTree","while opening \"%s\".",fTreeFileName.Data());
      return nullptr;
   } else {
      fTree = (TTree*)f->Get(fTreeName.Data());
      if (!fTree) {
         Error("GetTree","\"%s\" not found in \"%s\".", fTreeName.Data(), fTreeFileName.Data());
         return nullptr;
      } else {
         fTree->SetEntryList(fCurrentEntries);
         TString varexp;
         TIter next(fVarList);
         while (auto var = (TParallelCoordVar*)next())
            varexp.Append(TString::Format(":%s",var->GetTitle()));
         varexp.Remove(TString::kLeading,':');
         fTree->Draw(varexp.Data(),"","goff");
         TSelectorDraw* selector = (TSelectorDraw*)((TTreePlayer*)fTree->GetPlayer())->GetSelector();
         next.Reset();
         Int_t i = 0;
         while (auto var = (TParallelCoordVar*)next())
            var->SetValues(fNentries, selector->GetVal(i++));
         return fTree;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Get the variables values from its title.

Double_t* TParallelCoord::GetVariable(const char* vartitle)
{
   TIter next(fVarList);
   TParallelCoordVar* var = nullptr;
   while(((var = (TParallelCoordVar*)next()) != nullptr) && (var->GetTitle() != vartitle)) { }
   if(!var) return nullptr;
   else     return var->GetValues();
}

////////////////////////////////////////////////////////////////////////////////
/// Get the variables values from its index.

Double_t* TParallelCoord::GetVariable(Int_t i)
{
   if(i<0 || (UInt_t)i>fNvar) return nullptr;
   else return ((TParallelCoordVar*)fVarList->At(i))->GetValues();
}

////////////////////////////////////////////////////////////////////////////////
/// Initialise the data members of TParallelCoord.

void TParallelCoord::Init()
{
   fNentries = 0;
   fVarList = nullptr;
   fSelectList = nullptr;
   SetBit(kVertDisplay,true);
   SetBit(kCurveDisplay,false);
   SetBit(kPaintEntries,true);
   SetBit(kLiveUpdate,false);
   SetBit(kGlobalScale,false);
   SetBit(kCandleChart,false);
   SetBit(kGlobalLogScale,false);
   fTree = nullptr;
   fCurrentEntries = nullptr;
   fInitEntries = nullptr;
   fCurrentSelection = nullptr;
   fNvar = 0;
   fDotsSpacing = 0;
   fCurrentFirst = 0;
   fCurrentN = 0;
   fCandleAxis = nullptr;
   fWeightCut = 0;
   fLineWidth = 1;
   fLineColor = kGreen-8;
   fTreeName = "";
   fTreeFileName = "";
}

////////////////////////////////////////////////////////////////////////////////
/// Paint the parallel coordinates graph.

void TParallelCoord::Paint(Option_t* /*option*/)
{
   if (!GetTree()) return;
   gPad->Range(0,0,1,1);
   TFrame *frame = gPad->GetFrame();
   frame->SetLineColor(gPad->GetFillColor());
   SetAxesPosition();
   if(TestBit(kPaintEntries)){
      PaintEntries(nullptr);
      TIter next(fSelectList);
      TParallelCoordSelect* sel;
      while((sel = (TParallelCoordSelect*)next())) {
         if(sel->GetSize()>0 && sel->TestBit(TParallelCoordSelect::kActivated)) {
            PaintEntries(sel);
         }
      }
   }
   gPad->RangeAxis(0,0,1,1);

   TIter nextVar(fVarList);
   TParallelCoordVar* var=nullptr;
   while((var = (TParallelCoordVar*)nextVar())) {
      var->Paint();
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Loop over the entries and paint them.

void TParallelCoord::PaintEntries(TParallelCoordSelect* sel)
{
   if (fVarList->GetSize() < 2) return;
   Int_t i=0;
   Long64_t n=0;

   Double_t *x = new Double_t[fNvar];
   Double_t *y = new Double_t[fNvar];

   TGraph    *gr     = nullptr;
   TPolyLine *pl     = nullptr;
   TAttLine  *evline = nullptr;

   if (TestBit (kCurveDisplay)) {gr = new TGraph(fNvar); evline = (TAttLine*)gr;}
   else                       {pl = new TPolyLine(fNvar); evline = (TAttLine*)pl;}

   if (fDotsSpacing == 0) evline->SetLineStyle(1);
   else                   evline->SetLineStyle(11);
   if (!sel){
      evline->SetLineWidth(GetLineWidth());
      evline->SetLineColor(GetLineColor());
   } else {
      evline->SetLineWidth(sel->GetLineWidth());
      evline->SetLineColor(sel->GetLineColor());
   }
   TParallelCoordVar *var;

   TFrame *frame = gPad->GetFrame();
   Double_t lx   = ((frame->GetX2() - frame->GetX1())/(fNvar-1));
   Double_t ly   = ((frame->GetY2() - frame->GetY1())/(fNvar-1));
   Double_t a,b;
   TRandom r;

   for (n=fCurrentFirst; n<fCurrentFirst+fCurrentN; ++n) {
      TListIter next(fVarList);
      bool inrange = true;
      // Loop to check whenever the entry must be painted.
      if (sel) {
         while ((var = (TParallelCoordVar*)next())){
            if (!var->Eval(n,sel)) inrange = false;
         }
      }
      if (fWeightCut > 0) {
         next.Reset();
         Int_t entryweight = 0;
         while ((var = (TParallelCoordVar*)next())) entryweight+=var->GetEntryWeight(n);
         if (entryweight/(Int_t)fNvar < fWeightCut) inrange = false;
      }
      if(!inrange) continue;
      i = 0;
      next.Reset();
      // Loop to set the polyline points.
      while ((var = (TParallelCoordVar*)next())) {
         var->GetEntryXY(n,x[i],y[i]);
         ++i;
      }
      // beginning to paint the first point at a random distance
      // to avoid artefacts when increasing the dots spacing.
      if (fDotsSpacing != 0) {
         if (TestBit(kVertDisplay)) {
            a    = (y[1]-y[0])/(x[1]-x[0]);
            b    = y[0]-a*x[0];
            x[0] = x[0]+lx*r.Rndm();
            y[0] = a*x[0]+b;
         } else {
            a    = (x[1]-x[0])/(y[1]-y[0]);
            b    = x[0]-a*y[0];
            y[0] = y[0]+ly*r.Rndm();
            x[0] = a*y[0]+b;
         }
      }
      if (pl) pl->PaintPolyLine(fNvar,x,y);
      else    gr->PaintGraph(fNvar,x,y,"C");
   }

   if (pl) delete pl;
   if (gr) delete gr;
   delete [] x;
   delete [] y;
}

////////////////////////////////////////////////////////////////////////////////
/// Delete a variable from the graph.

void TParallelCoord::RemoveVariable(TParallelCoordVar *var)
{
   fVarList->Remove(var);
   fNvar = fVarList->GetSize();
   SetAxesPosition();
}

////////////////////////////////////////////////////////////////////////////////
/// Delete the variable "vartitle" from the graph.

bool TParallelCoord::RemoveVariable(const char* vartitle)
{
   TIter next(fVarList);
   TParallelCoordVar* var=nullptr;
   while((var = (TParallelCoordVar*)next())) {
      if (!strcmp(var->GetTitle(),vartitle)) break;
   }
   if(!var) {
      Error("RemoveVariable","\"%s\" not a variable",vartitle);
      return false;
   } else {
      RemoveVariable(var);
      delete var;
      return true;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Reset the tree entry list to the initial one..

void TParallelCoord::ResetTree()
{
   if(!fTree) return;
   fTree->SetEntryList(fInitEntries);
   fCurrentEntries = fInitEntries;
   fNentries = fCurrentEntries->GetN();
   fCurrentFirst = 0;
   fCurrentN = fNentries;
   TString varexp = "";
   TIter next(fVarList);
   while (auto var = (TParallelCoordVar*)next())
      varexp.Append(TString::Format(":%s",var->GetTitle()));
   varexp.Remove(TString::kLeading,':');
   fTree->Draw(varexp.Data(),"","goff");
   next.Reset();
   TSelectorDraw* selector = (TSelectorDraw*)((TTreePlayer*)fTree->GetPlayer())->GetSelector();
   Int_t i = 0;
   while (auto var = (TParallelCoordVar*)next()) {
      var->SetValues(fNentries, selector->GetVal(i++));
   }
   if (fSelectList) {           // FIXME It would be better to update the selections by deleting
      fSelectList->Delete();    // the meaningless ranges (selecting everything or nothing for example)
      fCurrentSelection = nullptr;    // after applying a new entrylist to the tree.
   }
   gPad->Modified();
   gPad->Update();
}

////////////////////////////////////////////////////////////////////////////////
/// Save the entry lists in a root file "filename.root".

void TParallelCoord::SaveEntryLists(const char* filename, bool overwrite)
{
   TString sfile = filename;
   if (sfile.IsNull())
      sfile.Form("%s_parallelcoord_entries.root", fTree->GetName());

   TDirectory *savedir = gDirectory;
   TFile *f = TFile::Open(sfile.Data());
   if (f) {
      Warning("SaveEntryLists", "%s already exists.", sfile.Data());
      if (!overwrite)
         return;
      Warning("SaveEntryLists", "Overwriting.");
      f = new TFile(sfile.Data(), "RECREATE");
   } else {
      f = new TFile(sfile.Data(), "CREATE");
   }
   gDirectory = f;
   fInitEntries->Write("initentries");
   fCurrentEntries->Write("currententries");
   f->Close();
   delete f;
   gDirectory = savedir;
   Info("SaveEntryLists", "File \"%s\" written.", sfile.Data());
}

////////////////////////////////////////////////////////////////////////////////
/// Save the TParallelCoord in a macro.

void TParallelCoord::SavePrimitive(std::ostream & out, Option_t* option)
{
   //  Save the entrylists.
   TString filename = TString::Format("%s_parallelcoord_entries.root", fTree->GetName());
   SaveEntryLists(filename, true); // FIXME overwriting by default.
   SaveTree(fTreeFileName, true);  // FIXME overwriting by default.

   if (!gROOT->ClassSaved(Class())) {
      out << "   TFile *para_f = nullptr, *para_entries = nullptr;\n";
      out << "   TTree* para_tree = nullptr;\n";
      out << "   TEntryList *para_currententries = nullptr;\n";
      out << "   TParallelCoordSelect *para_sel = nullptr;\n";
      out << "   TParallelCoordVar* para_var = nullptr;\n";
      out << "   TSelectorDraw *para_selector = nullptr;\n";
      out << "   TParallelCoord *para = nullptr;\n";
   }
   out << "   // Create a TParallelCoord.\n";
   out << "   para_f = TFile::Open(\"" << TString(fTreeFileName).ReplaceSpecialCppChars() << "\");\n";
   out << "   para_tree = (TTree *)para_f->Get(\"" << fTreeName << "\");\n";
   out << "   para = new TParallelCoord(para_tree, " << fNentries << ");\n";
   out << "   // Load the entrylists.\n";
   out << "   para_entries = TFile::Open(\"" << TString(filename).ReplaceSpecialCppChars() << "\");\n";
   out << "   para_currententries = (TEntryList *)para_entries->Get(\"currententries\");\n";
   out << "   para_tree->SetEntryList(para_currententries);\n";
   out << "   para->SetInitEntries((TEntryList*)para_entries->Get(\"initentries\"));\n";
   out << "   para->SetCurrentEntries(para_currententries);\n";
   TIter next(fSelectList);
   out << "   para->GetSelectList()->Delete();\n";
   while (auto sel = (TParallelCoordSelect *)next()) {
      out << "   para->AddSelection(\"" << TString(sel->GetTitle()).ReplaceSpecialCppChars() << "\");\n";
      out << "   para_sel = (TParallelCoordSelect*)para->GetSelectList()->Last();\n";
      sel->SaveLineAttributes(out, "para_sel", -1, -1, 1);
   }
   TIter nextbis(fVarList);
   TString varexp;
   while (auto var = (TParallelCoordVar *)nextbis()) {
      if (!varexp.IsNull())
         varexp.Append(":");
      varexp.Append(var->GetTitle());
   }
   out << "   para_tree->Draw(\"" << varexp.ReplaceSpecialCppChars() << "\", \"\", \"goff\");\n";
   out << "   para_selector = (TSelectorDraw *)((TTreePlayer *)para_tree->GetPlayer())->GetSelector();\n";
   nextbis.Reset();
   Int_t i = 0;
   while (auto var = (TParallelCoordVar *)nextbis()) {
      out << "   //***************************************\n";
      out << "   // Create the axis \"" << var->GetTitle() << "\".\n";
      out << "   para->AddVariable(para_selector->GetVal(" << i++ << "), \"" << TString(var->GetTitle()).ReplaceSpecialCppChars() << "\");\n";
      out << "   para_var = (TParallelCoordVar *)para->GetVarList()->Last();\n";
      var->SavePrimitive(out, "pcalled");
   }
   out << "   //***************************************\n";
   out << "   // Set the TParallelCoord parameters.\n";
   out << "   para->SetCurrentFirst(" << fCurrentFirst << ");\n";
   out << "   para->SetCurrentN(" << fCurrentN << ");\n";
   out << "   para->SetWeightCut(" << fWeightCut << ");\n";
   out << "   para->SetDotsSpacing(" << fDotsSpacing << ");\n";
   out << "   para->SetLineColor(" << TColor::SavePrimitiveColor(GetLineColor()) << ");\n";
   out << "   para->SetLineWidth(" << GetLineWidth() << ");\n";
   out << "   para->SetBit(TParallelCoord::kVertDisplay, " << TestBit(kVertDisplay) << ");\n";
   out << "   para->SetBit(TParallelCoord::kCurveDisplay, " << TestBit(kCurveDisplay) << ");\n";
   out << "   para->SetBit(TParallelCoord::kPaintEntries, " << TestBit(kPaintEntries) << ");\n";
   out << "   para->SetBit(TParallelCoord::kLiveUpdate, " << TestBit(kLiveUpdate) << ");\n";
   out << "   para->SetBit(TParallelCoord::kGlobalLogScale, " << TestBit(kGlobalLogScale) << ");\n";
   if (TestBit(kGlobalScale))
      out << "   para->SetGlobalScale(true);\n";
   if (TestBit(kCandleChart))
      out << "   para->SetCandleChart(true);\n";
   if (TestBit(kGlobalLogScale))
      out << "   para->SetGlobalLogScale(true);\n";
   out << "   \n";
   SavePrimitiveDraw(out, "para", option);
}

////////////////////////////////////////////////////////////////////////////////
/// Save the tree in a file if fTreeFileName == "".

void TParallelCoord::SaveTree(const char* filename, bool overwrite)
{
   if (!fTreeFileName.IsNull())
      return;
   TString sfile = filename;
   if (sfile.IsNull())
      sfile.Form("%s.root",fTree->GetName());

   TFile* f = TFile::Open(sfile.Data());
   if (f) {
      Warning("SaveTree","%s already exists.", sfile.Data());
      if (!overwrite) return;
      else Warning("SaveTree","Overwriting.");
      f = new TFile(sfile.Data(),"RECREATE");
   } else {
      f = new TFile(sfile.Data(),"CREATE");
   }
   gDirectory = f;
   fTree->Write(fTreeName.Data());
   fTreeFileName = sfile;
   Info("SaveTree", "File \"%s\" written.",sfile.Data());
}

////////////////////////////////////////////////////////////////////////////////
/// Update the position of the axes.

void TParallelCoord::SetAxesPosition()
{
   if(!gPad) return;
   bool vert          = TestBit (kVertDisplay);
   TFrame *frame        = gPad->GetFrame();
   if (fVarList->GetSize() > 1) {
      if (vert) {
         frame->SetX1(1.0/((Double_t)fVarList->GetSize()+1));
         frame->SetX2(1-frame->GetX1());
         frame->SetY1(0.1);
         frame->SetY2(0.9);
         gPad->RangeAxis(1.0/((Double_t)fVarList->GetSize()+1),0.1,1-frame->GetX1(),0.9);
      } else {
         frame->SetX1(0.1);
         frame->SetX2(0.9);
         frame->SetY1(1.0/((Double_t)fVarList->GetSize()+1));
         frame->SetY2(1-frame->GetY1());
         gPad->RangeAxis(0.1,1.0/((Double_t)fVarList->GetSize()+1),0.9,1-frame->GetY1());
      }

      Double_t horSpace    = (frame->GetX2() - frame->GetX1())/(fNvar-1);
      Double_t verSpace   = (frame->GetY2() - frame->GetY1())/(fNvar-1);
      Int_t i=0;
      TIter next(fVarList);

      TParallelCoordVar* var;
      while((var = (TParallelCoordVar*)next())){
         if (vert) var->SetX(gPad->GetFrame()->GetX1() + i*horSpace,TestBit(kGlobalScale));
         else      var->SetY(gPad->GetFrame()->GetY1() + i*verSpace,TestBit(kGlobalScale));
         ++i;
      }
   } else if (fVarList->GetSize()==1) {
      frame->SetX1(0.1);
      frame->SetX2(0.9);
      frame->SetY1(0.1);
      frame->SetY2(0.9);
      if (vert) ((TParallelCoordVar*)fVarList->First())->SetX(0.5,TestBit(kGlobalScale));
      else      ((TParallelCoordVar*)fVarList->First())->SetY(0.5,TestBit(kGlobalScale));
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the same histogram axis binning for all axis.

void TParallelCoord::SetAxisHistogramBinning(Int_t n)
{
   TIter next(fVarList);
   TParallelCoordVar *var;
   while((var = (TParallelCoordVar*)next())) var->SetHistogramBinning(n);
}

////////////////////////////////////////////////////////////////////////////////
/// Set the same histogram axis height for all axis.

void TParallelCoord::SetAxisHistogramHeight(Double_t h)
{
   TIter next(fVarList);
   TParallelCoordVar *var;
   while((var = (TParallelCoordVar*)next())) var->SetHistogramHeight(h);
}

////////////////////////////////////////////////////////////////////////////////
/// All axes in log scale.

void TParallelCoord::SetGlobalLogScale(bool lt)
{
   if (lt == TestBit(kGlobalLogScale)) return;
   SetBit(kGlobalLogScale,lt);
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) var->SetLogScale(lt);
   if (TestBit(kGlobalScale)) SetGlobalScale(true);
}

////////////////////////////////////////////////////////////////////////////////
/// Constraint all axes to the same scale.

void TParallelCoord::SetGlobalScale(bool gl)
{
   SetBit(kGlobalScale,gl);
   if (fCandleAxis) {
      delete fCandleAxis;
      fCandleAxis = nullptr;
   }
   if (gl) {
      Double_t min,max;
      min = GetGlobalMin();
      max = GetGlobalMax();
      if (TestBit(kGlobalLogScale) && min<=0) min = 0.00001*max;
      if (TestBit(kVertDisplay)) {
         if (!TestBit(kGlobalLogScale)) fCandleAxis = new TGaxis(0.05,0.1,0.05,0.9,min,max);
         else                           fCandleAxis = new TGaxis(0.05,0.1,0.05,0.9,min,max,510,"G");
      } else {
         if (!TestBit(kGlobalLogScale)) fCandleAxis = new TGaxis(0.1,0.05,0.9,0.05,min,max);
         else                           fCandleAxis = new TGaxis(0.1,0.05,0.9,0.05,min,max,510,"G");
      }
      fCandleAxis->Draw();
      SetGlobalMin(min);
      SetGlobalMax(max);
      TIter next(fVarList);
      TParallelCoordVar* var;
      while ((var = (TParallelCoordVar*)next())) var->GetHistogram();
   }
   gPad->Modified();
   gPad->Update();
}

////////////////////////////////////////////////////////////////////////////////
/// Set the same histogram axis line width for all axis.

void TParallelCoord::SetAxisHistogramLineWidth(Int_t lw)
{
   TIter next(fVarList);
   TParallelCoordVar *var;
   while((var = (TParallelCoordVar*)next())) var->SetHistogramLineWidth(lw);
}

////////////////////////////////////////////////////////////////////////////////
/// Set a candle chart display.

void TParallelCoord::SetCandleChart(bool can)
{
   SetBit(kCandleChart,can);
   SetGlobalScale(can);
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      var->SetBoxPlot(can);
      var->SetHistogramLineWidth(0);
   }
   if (fCandleAxis) delete fCandleAxis;
   fCandleAxis = nullptr;
   SetBit(kPaintEntries,!can);
   if (can) {
      if (TestBit(kVertDisplay)) fCandleAxis = new TGaxis(0.05,0.1,0.05,0.9,GetGlobalMin(),GetGlobalMax());
      else                       fCandleAxis = new TGaxis(0.1,0.05,0.9,0.05,GetGlobalMin(),GetGlobalMax());
      fCandleAxis->Draw();
   } else {
      if (fCandleAxis) {
         delete fCandleAxis;
         fCandleAxis = nullptr;
      }
   }
   gPad->Modified();
   gPad->Update();
}

////////////////////////////////////////////////////////////////////////////////
/// Set the first entry to be displayed.

void TParallelCoord::SetCurrentFirst(Long64_t f)
{
   if(f<0 || f>fNentries) return;
   fCurrentFirst = f;
   if(fCurrentFirst + fCurrentN > fNentries) fCurrentN = fNentries-fCurrentFirst;
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      var->GetMinMaxMean();
      var->GetHistogram();
      if (var->TestBit(TParallelCoordVar::kShowBox)) var->GetQuantiles();
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the number of entry to be displayed.

void TParallelCoord::SetCurrentN(Long64_t n)
{
   if(n<=0) return;
   if(fCurrentFirst+n>fNentries) fCurrentN = fNentries-fCurrentFirst;
   else fCurrentN = n;
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      var->GetMinMaxMean();
      var->GetHistogram();
      if (var->TestBit(TParallelCoordVar::kShowBox)) var->GetQuantiles();
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the selection being edited.

TParallelCoordSelect* TParallelCoord::SetCurrentSelection(const char* title)
{
   if (fCurrentSelection && fCurrentSelection->GetTitle() == title) return fCurrentSelection;
   TIter next(fSelectList);
   TParallelCoordSelect* sel;
   while((sel = (TParallelCoordSelect*)next()) && strcmp(sel->GetTitle(),title))
   if (sel) fCurrentSelection = sel;
   return sel;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the selection being edited.

void TParallelCoord::SetCurrentSelection(TParallelCoordSelect* sel)
{
   if (fCurrentSelection == sel) return;
   fCurrentSelection = sel;
}

////////////////////////////////////////////////////////////////////////////////
/// Set dots spacing. Modify the line style 11.
/// If the canvas support transparency dot spacing is ignored.

void TParallelCoord::SetDotsSpacing(Int_t s)
{
   if (gPad->GetCanvas()->SupportAlpha()) return;
   if (s == fDotsSpacing) return;
   fDotsSpacing = s;
   gStyle->SetLineStyleString(11, TString::Format("%d %d",4,s*8));
}

////////////////////////////////////////////////////////////////////////////////
/// Set the entry lists of "para".

void TParallelCoord::SetEntryList(TParallelCoord* para, TEntryList* enlist)
{
   para->SetCurrentEntries(enlist);
   para->SetInitEntries(enlist);
}

////////////////////////////////////////////////////////////////////////////////
/// Force all variables to adopt the same max.

void TParallelCoord::SetGlobalMax(Double_t max)
{
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      var->SetCurrentMax(max);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Force all variables to adopt the same min.

void TParallelCoord::SetGlobalMin(Double_t min)
{
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      var->SetCurrentMin(min);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// If true, the pad is updated while the motion of a dragged range.

void TParallelCoord::SetLiveRangesUpdate(bool on)
{
   SetBit(kLiveUpdate,on);
   TIter next(fVarList);
   TParallelCoordVar* var;
   while((var = (TParallelCoordVar*)next())) var->SetLiveRangesUpdate(on);
}

////////////////////////////////////////////////////////////////////////////////
/// Set the vertical or horizontal display.

void TParallelCoord::SetVertDisplay(bool vert)
{
   if (vert == TestBit (kVertDisplay)) return;
   SetBit(kVertDisplay,vert);
   if (!gPad) return;
   TFrame* frame = gPad->GetFrame();
   if (!frame) return;
   UInt_t ui = 0;
   Double_t horaxisspace = (frame->GetX2() - frame->GetX1())/(fNvar-1);
   Double_t veraxisspace = (frame->GetY2() - frame->GetY1())/(fNvar-1);
   TIter next(fVarList);
   TParallelCoordVar* var;
   while ((var = (TParallelCoordVar*)next())) {
      if (vert) var->SetX(frame->GetX1() + ui*horaxisspace,TestBit(kGlobalScale));
      else      var->SetY(frame->GetY1() + ui*veraxisspace,TestBit(kGlobalScale));
      ++ui;
   }
   if (TestBit(kCandleChart)) {
      if (fCandleAxis) delete fCandleAxis;
      if (TestBit(kVertDisplay)) fCandleAxis = new TGaxis(0.05,0.1,0.05,0.9,GetGlobalMin(),GetGlobalMax());
      else                       fCandleAxis = new TGaxis(0.1,0.05,0.9,0.05,GetGlobalMin(),GetGlobalMax());
      fCandleAxis->Draw();
   }
   gPad->Modified();
   gPad->Update();
}

////////////////////////////////////////////////////////////////////////////////
/// Unzoom all variables.

void TParallelCoord::UnzoomAll()
{
   TIter next(fVarList);
   TParallelCoordVar* var;
   while((var = (TParallelCoordVar*)next())) var->Unzoom();
}
