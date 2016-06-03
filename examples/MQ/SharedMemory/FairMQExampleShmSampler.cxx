/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQExampleShmSampler.cpp
 *
 * @since 2016-04-08
 * @author A. Rybalchenko
 */

#include <string>
#include <thread>
#include <chrono>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>

#include "FairMQExampleShmSampler.h"
#include "FairMQProgOptions.h"
#include "FairMQLogger.h"

using namespace std;
using namespace boost::interprocess;

FairMQExampleShmSampler::FairMQExampleShmSampler()
    : fMsgSize(10000)
    , fMsgCounter(0)
    , fMsgRate(1)
    , fBytesOut(0)
    , fMsgOut(0)
    , fBytesOutNew(0)
    , fMsgOutNew(0)
    , fPtrs()
    , fContainerMutex()
    , fAckMutex()
    , fAckCV()
{
    if (shared_memory_object::remove("FairMQSharedMemory"))
    {
        LOG(INFO) << "Successfully removed shared memory upon device start.";
    }
    else
    {
        LOG(INFO) << "Did not remove shared memory upon device start.";
    }
}

FairMQExampleShmSampler::~FairMQExampleShmSampler()
{
    if (shared_memory_object::remove("FairMQSharedMemory"))
    {
        LOG(INFO) << "Successfully removed shared memory after the device has stopped.";
    }
    else
    {
        LOG(INFO) << "Did not remove shared memory after the device stopped. Still in use?";
    }
}

void FairMQExampleShmSampler::Init()
{
    fMsgSize = fConfig->GetValue<int>("msg-size");
    fMsgRate = fConfig->GetValue<int>("msg-rate");

    SegmentManager::Instance().InitializeSegment("open_or_create", "FairMQSharedMemory", 2000000000);
    LOG(INFO) << "Created/Opened shared memory segment of 2,000,000,000 bytes. Available are "
              << SegmentManager::Instance().Segment()->get_free_memory() << " bytes.";
}

void FairMQExampleShmSampler::Run()
{
    // count sent messages (also used in creating ShmChunk container ID)
    static uint64_t numSentMsgs = 0;

    LOG(INFO) << "Starting the benchmark with message size of " << fMsgSize;

    // start rate logger and acknowledgement listener in separate threads
    thread rateLogger(&FairMQExampleShmSampler::Log, this, 1000);
    thread ackListener(&FairMQExampleShmSampler::ListenForAcks, this);
    // thread resetMsgCounter(&FairMQExampleShmSampler::ResetMsgCounter, this);

    // int charnum = 97;

    while (CheckCurrentState(RUNNING))
    {
        // ShmChunk container ID
        string chunkStr = "c" + to_string(numSentMsgs);
        // shared pointer ID
        string ownerStr = "o" + to_string(numSentMsgs);

        SharedPtrOwner* owner = nullptr;

        try
        {
            owner = SegmentManager::Instance().Segment()->construct<SharedPtrOwner>(ownerStr.c_str())(
                make_managed_shared_ptr(SegmentManager::Instance().Segment()->construct<ShmChunk>(chunkStr.c_str())(fMsgSize),
                                        *(SegmentManager::Instance().Segment()))
                );
        }
        catch (bipc::bad_alloc& ba)
        {
            unique_lock<mutex> lock(fAckMutex);
            fAckCV.wait_for(lock, chrono::milliseconds(500));
            continue;
        }

        // move the local shared pointer into the container (to be destroyed after acknowledgement)
        {
            unique_lock<mutex> containerLock(fContainerMutex);
            fPtrs.insert(make_pair(numSentMsgs, move(owner)));
        }

        // LOG(DEBUG) << "Shared pointer constructed at: " << *ownerStr;
        // LOG(DEBUG) << "Chunk constructed at: " << chunkStr;

        void* ptr = owner->fSharedPtr->GetData();

        // write something to memory, otherwise only (incomplete) allocation will be measured
        // memset(ptr, 0, fMsgSize);

        // static_cast<char*>(ptr)[3] = charnum++;
        // if (charnum == 123)
        // {
        //     charnum = 97;
        // }

        // LOG(DEBUG) << "chunk handle: " << owner->fSharedPtr->GetHandle();
        // LOG(DEBUG) << "chunk size: " << owner->fSharedPtr->GetSize();
        // LOG(DEBUG) << "owner use count: " << owner->fSharedPtr.use_count();

        // char* cptr = static_cast<char*>(ptr);
        // LOG(DEBUG) << "check: " << cptr[3];

        FairMQMessagePtr msg(NewSimpleMessage(numSentMsgs));
        fPtrs.at(numSentMsgs)->fRcvCount += 1;

        if (Send(msg, "meta", 0) >= 0)
        {
            // LOG(DEBUG) << "Sent meta.";
            fBytesOutNew += fMsgSize;
            ++fMsgOutNew;
            ++numSentMsgs;
        }
        else
        {
            fPtrs.erase(numSentMsgs);
            SegmentManager::Instance().Segment()->destroy_ptr(owner);
        }

        // --fMsgCounter;
        // while (fMsgCounter == 0)
        // {
        //     this_thread::sleep_for(chrono::milliseconds(1));
        // }
    }

    LOG(INFO) << "Sent " << numSentMsgs << " messages, leaving RUNNING state.";

    ackListener.join();
    rateLogger.join();
    // resetMsgCounter.join();
}

void FairMQExampleShmSampler::ListenForAcks()
{
    while (CheckCurrentState(RUNNING))
    {
        unique_ptr<FairMQMessage> msg(NewMessage());

        if (Receive(msg, "ack") >= 0)
        {
            uint64_t key = *(static_cast<uint64_t*>(msg->GetData()));
            // LOG(DEBUG) << "Received ack for: " << key;
            {
                unique_lock<mutex> containerLock(fContainerMutex);
                if (fPtrs.find(key) != fPtrs.end())
                {
                    fPtrs.at(key)->fRcvCount -= 1;
                    if (fPtrs.at(key)->fRcvCount == 0)
                    {
                        SegmentManager::Instance().Segment()->destroy_ptr(fPtrs.at(key));
                        fPtrs.erase(key);
                    }
                }
                else
                {
                    LOG(WARN) << "Received ack for a key not contained in the container";
                }
                // LOG(DEBUG) << "Number of chunks in the tracking container: " << fPtrs.size();
            }
            fAckCV.notify_all();
        }
    }
}

void FairMQExampleShmSampler::Log(const int intervalInMs)
{
    timestamp_t t0 = get_timestamp();
    timestamp_t t1;
    timestamp_t msSinceLastLog;

    double mbPerSecOut = 0;
    double msgPerSecOut = 0;

    while (CheckCurrentState(RUNNING))
    {
        t1 = get_timestamp();

        msSinceLastLog = (t1 - t0) / 1000.0L;

        mbPerSecOut = (static_cast<double>(fBytesOutNew - fBytesOut) / (1024. * 1024.)) / static_cast<double>(msSinceLastLog) * 1000.;
        fBytesOut = fBytesOutNew;

        msgPerSecOut = static_cast<double>(fMsgOutNew - fMsgOut) / static_cast<double>(msSinceLastLog) * 1000.;
        fMsgOut = fMsgOutNew;

        LOG(DEBUG) << fixed
                   << setprecision(0) << "out: " << msgPerSecOut << " msg ("
                   << setprecision(2) << mbPerSecOut << " MB)\t("
                   << SegmentManager::Instance().Segment()->get_free_memory() / (1024. * 1024.) << " MB free)";

        t0 = t1;
        this_thread::sleep_for(chrono::milliseconds(intervalInMs));
    }
}

void FairMQExampleShmSampler::ResetMsgCounter()
{
    while (CheckCurrentState(RUNNING))
    {
        fMsgCounter = fMsgRate / 100;
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}
