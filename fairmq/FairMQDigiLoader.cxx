/**
 * FairMQDigiLoader.cxx
 *
 *  @since 2012-04-22
 *  @author A. Rybalchenko
 */

#include "FairMQDigiLoader.h"
#include "FairMQLogger.h"
#include <iostream>


FairMQDigiLoader::FairMQDigiLoader() :
  FairMQSamplerTask("Load TestDetectorDigiPixel from rootfile into FairMQPayload::TestDetectorDigi")
{
}

FairMQDigiLoader::~FairMQDigiLoader()
{
}

void FairMQDigiLoader::Exec(Option_t* opt)
{
  Int_t nTestDetectorDigis = fInput->GetEntriesFast();
  Int_t size = nTestDetectorDigis * sizeof(FairMQPayload::TestDetectorDigi);
  
  void* buffer = operator new[](size);
  FairMQPayload::TestDetectorDigi* ptr = static_cast<FairMQPayload::TestDetectorDigi*>(buffer);

  for (Int_t i = 0; i < nTestDetectorDigis; ++i) {
    FairMQDigi* testDigi = dynamic_cast<FairMQDigi*>(fInput->At(i));
    if (NULL == testDigi)
        continue;
    
    new(&ptr[i]) FairMQPayload::TestDetectorDigi();
    ptr[i] = FairMQPayload::TestDetectorDigi();
    ptr[i].fX = testDigi->GetX();
    ptr[i].fY = testDigi->GetY();
    ptr[i].fZ = testDigi->GetZ();
    ptr[i].fTimeStamp = testDigi->GetTimeStamp();
    //std::cout << "Digi: " << ptr[i].fX << "|" << ptr[i].fY << "|" << ptr[i].fZ << "|" << ptr[i].fTimeStamp << ";" << std::endl;

  }

  fOutput->GetMessage()->rebuild(buffer, size, &FairMQSamplerTask::ClearOutput);

  //std::cout << "Loaded " << fOutput->Size() << " bytes (" << nTestDetectorDigis << " entries)." << std::endl;
}

