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
{
}

void FairMQExampleRegionSampler::InitTask()
{
    fMsgSize = fConfig->GetValue<int>("msg-size");
}

void FairMQExampleRegionSampler::Run()
{
    FairMQChannel& dataOutChannel = fChannels.at("data").at(0);

    FairMQUnmanagedRegionPtr region(NewUnmanagedRegionFor("data", 0, 10000000));

    while (CheckCurrentState(RUNNING))
    {
        FairMQMessagePtr msg(NewMessageFor("data", // channel
                                           0, // sub-channel
                                           region, // region
                                           region->GetData(), // ptr within region
                                           fMsgSize // offset from ptr
                                           // [](void* data){  } // callback to be called when buffer no longer needed by transport
                                           ));
        dataOutChannel.Send(msg);
    }
}

FairMQExampleRegionSampler::~FairMQExampleRegionSampler()
{
}
