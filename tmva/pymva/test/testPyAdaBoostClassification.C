#include <iostream>

#include "TString.h"
#include "TFile.h"
#include "TTree.h"
#include "TSystem.h"
#include "TROOT.h"
#include "TMVA/Factory.h"
#include "TMVA/Reader.h"
#include "TMVA/DataLoader.h"
#include "TMVA/PyMethodBase.h"

int testPyAdaBoostClassification(){
   // Get data file
   std::cout << "Get test data..." << std::endl;
   TString fname = gROOT->GetTutorialDir() + "/machine_learning/data/tmva_class_example.root";
   TFile *input = TFile::Open(fname);
   if (!input) {
      std::cout << "ERROR: could not open data file " << fname << std::endl;
      return 1;
   }

   // Setup PyMVA and factory
   std::cout << "Setup TMVA..." << std::endl;
   TMVA::PyMethodBase::PyInitialize();
   TFile* outputFile = TFile::Open("ResultsTestPyAdaBoostClassification.root", "RECREATE");
   TMVA::Factory *factory = new TMVA::Factory("testPyAdaBoostClassification", outputFile,
      "!V:Silent:Color:!DrawProgressBar:AnalysisType=Classification");

   // Load data
   TMVA::DataLoader *dataloader = new TMVA::DataLoader("datasetTestPyAdaBoostClassification");

   TTree *signal = (TTree*)input->Get("TreeS");
   TTree *background = (TTree*)input->Get("TreeB");
   dataloader->AddSignalTree(signal);
   dataloader->AddBackgroundTree(background);

   dataloader->AddVariable("var1");
   dataloader->AddVariable("var2");
   dataloader->AddVariable("var3");
   dataloader->AddVariable("var4");

   dataloader->PrepareTrainingAndTestTree("",
      "SplitMode=Random:NormMode=NumEvents:!V");

   // Book and train method
   factory->BookMethod(dataloader, TMVA::Types::kPyAdaBoost, "PyAdaBoost",
      "H:V:NEstimators=50");
   std::cout << "Train classifier..." << std::endl;
   factory->TrainAllMethods();

   // Clean-up
   delete factory;
   delete dataloader;
   delete outputFile;

   // Setup reader
   UInt_t numEvents = 100;
   std::cout << "Run reader and classify " << numEvents << " events..." << std::endl;
   TMVA::Reader *reader = new TMVA::Reader("Color:Silent");
   Float_t vars[4];
   reader->AddVariable("var1", vars+0);
   reader->AddVariable("var2", vars+1);
   reader->AddVariable("var3", vars+2);
   reader->AddVariable("var4", vars+3);
   reader->BookMVA("PyAdaBoost", "datasetTestPyAdaBoostClassification/weights/testPyAdaBoostClassification_PyAdaBoost.weights.xml");

   // Get mean response of method on signal and background events
   signal->SetBranchAddress("var1", vars+0);
   signal->SetBranchAddress("var2", vars+1);
   signal->SetBranchAddress("var3", vars+2);
   signal->SetBranchAddress("var4", vars+3);

   background->SetBranchAddress("var1", vars+0);
   background->SetBranchAddress("var2", vars+1);
   background->SetBranchAddress("var3", vars+2);
   background->SetBranchAddress("var4", vars+3);

   Float_t meanMvaSignal = 0;
   Float_t meanMvaBackground = 0;
   for(UInt_t i=0; i<numEvents; i++){
      signal->GetEntry(i);
      meanMvaSignal += reader->EvaluateMVA("PyAdaBoost");
      background->GetEntry(i);
      meanMvaBackground += reader->EvaluateMVA("PyAdaBoost");
   }
   meanMvaSignal = meanMvaSignal/float(numEvents);
   meanMvaBackground = meanMvaBackground/float(numEvents);

   // Check whether the response is obviously better than guessing

   // NOTE: The scikit-learn AdaBoost classifier returns probability values near
   // to 0.5, thought the separation is quite good. You can check this by looking at
   // the ROC using TMVAGui, which achieves good AUC values.
   // Because of this, the thresholds for passing this test are relaxed to greater or less
   // 0.5 for signal and background respectively.

   std::cout << "Mean MVA response on signal: " << meanMvaSignal << std::endl;
   if(meanMvaSignal < 0.5){
      std::cout << "[ERROR] Mean response on signal is " << meanMvaSignal << " (<0.5)" << std::endl;
      return 1;
   }
   std::cout << "Mean MVA response on background: " << meanMvaBackground << std::endl;
   if(meanMvaBackground > 0.5){
      std::cout << "[ERROR] Mean response on background is " << meanMvaBackground << " (>0.5)" << std::endl;
      return 1;
   }

   return 0;
}

int main(){
   int err = testPyAdaBoostClassification();
   return err;
}
