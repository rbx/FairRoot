/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQExampleRegionSampler.cpp
 *
 * @since 2014-10-10
 * @author A. Rybalchenko
 */

#include "FairMQExampleRegionSampler.h"
#include "FairMQLogger.h"
#include "FairMQProgOptions.h" // device->fConfig

using namespace std;

FairMQExampleRegionSampler::FairMQExampleRegionSampler()
    : fMsgSize(10000)
    , fRegion(nullptr)
{
}

void FairMQExampleRegionSampler::InitTask()
{
    fMsgSize = fConfig->GetValue<int>("msg-size");

    fRegion = FairMQUnmanagedRegionPtr(NewUnmanagedRegionFor("data", 0, 10000000));
}

bool FairMQExampleRegionSampler::ConditionalRun()
{
    FairMQMessagePtr msg(NewMessageFor("data", // channel
                                        0, // sub-channel
                                        fRegion, // region
                                        fRegion->GetData(), // ptr within region
                                        fMsgSize // offset from ptr
                                        // [](void* data){  } // callback to be called when buffer no longer needed by transport
                                        ));
    Send(msg, "data", 0);

    return true;
}

FairMQExampleRegionSampler::~FairMQExampleRegionSampler()
{
}
