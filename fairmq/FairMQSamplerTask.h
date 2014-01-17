/*
 * FairMQSamplerTask.h
 *
 *  Created on: Nov 22, 2012
 *      Author: dklein
 */

#ifndef FAIRMQSAMPLERTASK_H_
#define FAIRMQSAMPLERTASK_H_

#include "FairTask.h"
#include <vector>
#include "TClonesArray.h"
#include <string>
#include "FairMQMessage.h"
#include "TString.h"


class FairMQSamplerTask: public FairTask
{
  public:
    FairMQSamplerTask();
    FairMQSamplerTask(const Text_t* name, Int_t iVerbose=1);
    virtual ~FairMQSamplerTask();
    virtual InitStatus Init();
    virtual void Exec(Option_t* opt);
    void SetBranch(TString branch);
    FairMQMessage* GetOutput();
    static void ClearOutput(void* data, void* hint);
  protected:
    TClonesArray* fInput;
    TString fBranch;
    FairMQMessage* fOutput;
};

#endif /* FAIRMQSAMPLERTASK_H_ */
