// -------------------------------------------------------------------------
// -----   FairTutorialDetHitProducerIdealMissallign source file       -----
// -----                  Created 11.02.13  by F. Uhlig                -----
// -------------------------------------------------------------------------
#include "FairTutorialDetHitProducerIdealMissallign.h"

#include "FairTutorialDetHit.h"
#include "FairTutorialDetPoint.h"
#include "FairTutorialDetMissallignPar.h"

#include "FairRootManager.h"
#include "FairRunAna.h"
#include "FairRuntimeDb.h"

#include "TClonesArray.h"

// -----   Default constructor   -------------------------------------------
FairTutorialDetHitProducerIdealMissallign::FairTutorialDetHitProducerIdealMissallign()
  : FairTask("Missallign Hit Producer for the TutorialDet"),
    fPointArray(NULL),
    fHitArray(NULL),
    fShiftX(),
    fShiftY(),
    fDigiPar(NULL)
{
}

// -----   Destructor   ----------------------------------------------------
FairTutorialDetHitProducerIdealMissallign::~FairTutorialDetHitProducerIdealMissallign() { }

// --------------------------------------------------
void FairTutorialDetHitProducerIdealMissallign::SetParContainers()
{

  LOG(INFO)<< "Set tutdet missallign parameters"<<FairLogger::endl;
  // Get Base Container
  FairRunAna* ana = FairRunAna::Instance();
  FairRuntimeDb* rtdb=ana->GetRuntimeDb();

  fDigiPar = (FairTutorialDetMissallignPar*)
             (rtdb->getContainer("FairTutorialDetMissallignPar"));

}
// --------------------------------------------------------------------
InitStatus FairTutorialDetHitProducerIdealMissallign::ReInit()
{

  // Get Base Container
  FairRunAna* ana = FairRunAna::Instance();
  FairRuntimeDb* rtdb=ana->GetRuntimeDb();

  fDigiPar = (FairTutorialDetMissallignPar*)
             (rtdb->getContainer("FairTutorialDetMissallignPar"));

  fShiftX=fDigiPar->GetShiftX();
  fShiftY=fDigiPar->GetShiftY();
}

// -----   Public method Init   --------------------------------------------
InitStatus FairTutorialDetHitProducerIdealMissallign::Init()
{

  // Get RootManager
  FairRootManager* ioman = FairRootManager::Instance();
  if ( ! ioman ) {
    LOG(FATAL) << "RootManager not instantised!" << FairLogger::endl;
    return kFATAL;
  }

  // Get input array
  fPointArray = (TClonesArray*) ioman->GetObject("TutorialDetPoint");
  if ( ! fPointArray ) {
    LOG(FATAL)<<"No TutorialDetPoint array!" << FairLogger::endl;
    return kFATAL;
  }

  // Create and register output array
  fHitArray = new TClonesArray("FairTutorialDetHit");
  ioman->Register("TutorialDetHit", "TutorialDet", fHitArray, kTRUE);

  LOG(INFO)<< "HitProducerIdealMissallign: Initialisation successfull"
           << FairLogger::endl;


  fShiftX=fDigiPar->GetShiftX();
  fShiftY=fDigiPar->GetShiftY();
  /*
    Int_t num = fDigiPar->GetNrOfDetectors();
    Int_t size = fShiftX.GetSize();
    LOG(INFO)<<"Array has a size of "<< size << "elements"<<FairLogger::endl;
    for (Int_t i=0; i< num; ++i) {
      LOG(INFO)<< i <<": "<<fShiftX.At(i)<<FairLogger::endl;
    }
  */
  return kSUCCESS;

}
// -----   Public method Exec   --------------------------------------------
void FairTutorialDetHitProducerIdealMissallign::Exec(Option_t* opt)
{

  // Reset output array
  if ( ! fHitArray ) { LOG(FATAL)<<"No TutorialDetHitArray"<<FairLogger::endl; }

  fHitArray->Clear();

  // Declare some variables
  FairTutorialDetPoint* point = NULL;
  Int_t detID   = 0;        // Detector ID
  Int_t trackID = 0;        // Track index
  Double_t x, y, z;         // Position
  Double_t dx = 0.1;        // Position error
  Double_t tof = 0.;        // Time of flight
  TVector3 pos, dpos;       // Position and error vectors

  // Loop over TofPoints
  Int_t nHits = 0;
  Int_t nPoints = fPointArray->GetEntriesFast();
  for (Int_t iPoint=0; iPoint<nPoints; iPoint++) {
    point = (FairTutorialDetPoint*) fPointArray->At(iPoint);
    if ( ! point) { continue; }

    // Detector ID
    detID = point->GetDetectorID();

    // MCTrack ID
    trackID = point->GetTrackID();

    // Determine hit position
    x  = point->GetX()-fShiftX.At(detID);
    y  = point->GetY()-fShiftX.At(detID);
    z  = point->GetZ();

    LOG(DEBUG2)<<"Missallign hit by "<<fShiftX.At(detID)<<" cm in x- and "
               << fShiftY.At(detID)<<" cm in y-direction."<<FairLogger::endl;

    // Time of flight
    tof = point->GetTime();

    // Create new hit
    pos.SetXYZ(x,y,z);
    dpos.SetXYZ(dx, dx, 0.);
    new ((*fHitArray)[nHits]) FairTutorialDetHit(detID, iPoint, pos, dpos);
    nHits++;
  }   // Loop over MCPoints

  // Event summary
  LOG(INFO)<< "Create " << nHits << " TutorialDetHits out of "
           << nPoints << " TutorilaDetPoints created." << FairLogger::endl;

}
// -------------------------------------------------------------------------



ClassImp(FairTutorialDetHitProducerIdealMissallign)
