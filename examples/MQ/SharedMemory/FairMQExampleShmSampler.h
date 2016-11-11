/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQExampleShmSampler.h
 *
 * @since 2016-04-08
 * @author A. Rybalchenko
 */

#ifndef FAIRMQEXAMPLESHMSAMPLER_H_
#define FAIRMQEXAMPLESHMSAMPLER_H_

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include "FairMQDevice.h"
#include "ShmChunk.h"

class FairMQExampleShmSampler : public FairMQDevice
{
  public:
    FairMQExampleShmSampler();
    virtual ~FairMQExampleShmSampler();

    void ListenForAcks();
    void Log(const int intervalInMs);
    void ResetMsgCounter();

  protected:
    unsigned int fMsgSize;
    unsigned int fMsgCounter;
    unsigned int fMsgRate;

    unsigned long long fBytesOut;
    unsigned long long fMsgOut;
    std::atomic<unsigned long long> fBytesOutNew;
    std::atomic<unsigned long long> fMsgOutNew;

    // std::unordered_map<uint64_t, SharedPtrOwner*> fPtrs;
    std::unordered_map<bipc::managed_shared_memory::handle_t, std::unique_ptr<ShmChunk>> fPtrs;

    std::mutex fContainerMutex;
    std::mutex fAckMutex;
    std::condition_variable fAckCV;

    virtual void Init();
    virtual void Run();
};

#endif /* FAIRMQEXAMPLESHMSAMPLER_H_ */
