## \file
## \ingroup tutorial_tmva_keras
## \notebook -nodraw
## This tutorial shows how to do multiclass classification in TMVA with neural
## networks trained with keras.
##
## \macro_code
##
## \date 2017
## \author TMVA Team

from ROOT import TMVA, TFile, TCut, gROOT
from os.path import isfile

from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense
from tensorflow.keras.optimizers import SGD


def create_model():
    # Define model
    model = Sequential()
    model.add(Dense(32, activation='relu', input_dim=4))
    model.add(Dense(4, activation='softmax'))

    # Set loss and optimizer
    model.compile(loss='categorical_crossentropy', optimizer=SGD(
        learning_rate=0.01), weighted_metrics=['accuracy',])

    # Store model to file
    model.save('modelMultiClass.h5')
    model.summary()


def run():
    with TFile.Open('TMVA.root', 'RECREATE') as output, TFile.Open('tmva_example_multiple_background.root') as data:
        factory = TMVA.Factory('TMVAClassification', output,
                               '!V:!Silent:Color:DrawProgressBar:Transformations=D,G:AnalysisType=multiclass')

        signal = data.Get('TreeS')
        background0 = data.Get('TreeB0')
        background1 = data.Get('TreeB1')
        background2 = data.Get('TreeB2')

        dataloader = TMVA.DataLoader('dataset')
        for branch in signal.GetListOfBranches():
            dataloader.AddVariable(branch.GetName())

        dataloader.AddTree(signal, 'Signal')
        dataloader.AddTree(background0, 'Background_0')
        dataloader.AddTree(background1, 'Background_1')
        dataloader.AddTree(background2, 'Background_2')
        dataloader.PrepareTrainingAndTestTree(TCut(''),
                                              'SplitMode=Random:NormMode=NumEvents:!V')

        # Book methods
        factory.BookMethod(dataloader, TMVA.Types.kFisher, 'Fisher',
                           '!H:!V:Fisher:VarTransform=D,G')
        factory.BookMethod(dataloader, TMVA.Types.kPyKeras, 'PyKeras',
                           'H:!V:VarTransform=D,G:FilenameModel=modelMultiClass.h5:FilenameTrainedModel=trainedModelMultiClass.h5:NumEpochs=20:BatchSize=32')

        # Run TMVA
        factory.TrainAllMethods()
        factory.TestAllMethods()
        factory.EvaluateAllMethods()


if __name__ == "__main__":
    # Generate model
    create_model()

    # Setup TMVA
    TMVA.Tools.Instance()
    TMVA.PyMethodBase.PyInitialize()

    # Load data
    if not isfile('tmva_example_multiple_background.root'):
        createDataMacro = str(gROOT.GetTutorialDir()) + '/machine_learning/createData.C'
        print(createDataMacro)
        gROOT.ProcessLine('.L {}'.format(createDataMacro))
        gROOT.ProcessLine('create_MultipleBackground(4000)')

    # Run TMVA
    run()
