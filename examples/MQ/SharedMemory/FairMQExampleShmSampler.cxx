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

#include <thread>
#include <chrono>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#include "FairMQExampleShmSampler.h"
#include "FairMQProgOptions.h"
#include "FairMQLogger.h"

using namespace std;
using namespace boost::interprocess;
namespace bpt = boost::posix_time;

FairMQExampleShmSampler::FairMQExampleShmSampler()
    : fMsgSize(10000)
    , fMsgCounter(0)
    , fMsgRate(1)
    , fBytesOut(0)
    , fMsgOut(0)
    , fBytesOutNew(0)
    , fMsgOutNew(0)
    , fPtrs()
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
    if (message_queue::remove("meta_queue1"))
    {
        LOG(INFO) << "Successfully removed meta_queue1 after the device has stopped.";
    }
    else
    {
        LOG(INFO) << "Did not remove meta_queue1 after the device stopped. Still in use?";
    }
    // if (message_queue::remove("meta_queue2"))
    // {
    //     LOG(INFO) << "Successfully removed meta_queue2 after the device has stopped.";
    // }
    // else
    // {
    //     LOG(INFO) << "Did not remove meta_queue2 after the device stopped. Still in use?";
    // }
    // if (message_queue::remove("meta_queue3"))
    // {
    //     LOG(INFO) << "Successfully removed meta_queue3 after the device has stopped.";
    // }
    // else
    // {
    //     LOG(INFO) << "Did not remove meta_queue3 after the device stopped. Still in use?";
    // }
    // if (message_queue::remove("meta_queue4"))
    // {
    //     LOG(INFO) << "Successfully removed meta_queue4 after the device has stopped.";
    // }
    // else
    // {
    //     LOG(INFO) << "Did not remove meta_queue4 after the device stopped. Still in use?";
    // }

    if (message_queue::remove("ack_queue"))
    {
        LOG(INFO) << "Successfully removed ack_queue after the device has stopped.";
    }
    else
    {
        LOG(INFO) << "Did not remove ack_queue after the device stopped. Still in use?";
    }

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

    LOG(INFO) << "Boost version: "
              << BOOST_VERSION / 100000 << "." 
              << BOOST_VERSION / 100 % 1000 << "."
              << BOOST_VERSION % 100;

    SegmentManager::Instance().InitializeSegment("create_only", "FairMQSharedMemory", 2000000000);
    LOG(INFO) << "Created/Opened shared memory segment of 2,000,000,000 bytes. Available are "
              << SegmentManager::Instance().Segment()->get_free_memory() << " bytes.";

    message_queue::remove("meta_queue1");
    message_queue metaQueue1(create_only, "meta_queue1", 1000, sizeof(uint64_t));
    // message_queue::remove("meta_queue2");
    // message_queue metaQueue2(create_only, "meta_queue2", 1000, sizeof(uint64_t));
    // message_queue::remove("meta_queue3");
    // message_queue metaQueue3(create_only, "meta_queue3", 1000, sizeof(uint64_t));
    // message_queue::remove("meta_queue4");
    // message_queue metaQueue4(create_only, "meta_queue4", 1000, sizeof(uint64_t));

    message_queue::remove("ack_queue");
    message_queue ackQueue(create_only, "ack_queue", 1000, sizeof(uint64_t));
}

void FairMQExampleShmSampler::Run()
{
    static uint64_t numSentMsgs = 0;

    LOG(INFO) << "Starting the benchmark with message size of " << fMsgSize;

    thread rateLogger(&FairMQExampleShmSampler::Log, this, 1000);
    thread ackListener(&FairMQExampleShmSampler::ListenForAcks, this);
    // thread resetMsgCounter(&FairMQExampleShmSampler::ResetMsgCounter, this);

    message_queue metaQueue1(open_only, "meta_queue1");
    // message_queue metaQueue2(open_only, "meta_queue2");
    // message_queue metaQueue3(open_only, "meta_queue3");
    // message_queue metaQueue4(open_only, "meta_queue4");

    // int charnum = 97;

    while (CheckCurrentState(RUNNING))
    {
        string chunkStr = "c" + to_string(numSentMsgs);
        string ownerStr = "o" + to_string(numSentMsgs);

        SharedPtrOwner* owner = nullptr;

        try
        {
            // create chunk in shared memory of configured size and manage it through a shared memory shared pointer with ID
            owner = SegmentManager::Instance().Segment()->construct<SharedPtrOwner>(ownerStr.c_str())(
                make_managed_shared_ptr(SegmentManager::Instance().Segment()->construct<ShmChunk>(chunkStr.c_str())(fMsgSize),
                                        *(SegmentManager::Instance().Segment()))
                );
        }
        catch (bipc::bad_alloc& ba)
        {
            // LOG(TRACE) << "Not enough free memory for new message ("
            //            << SegmentManager::Instance().Segment()->get_free_memory()
            //            << " < "
            //            << fMsgSize
            //            << ")...";

            // if (SegmentManager::Instance().Segment()->find<SharedPtrOwner>(ownerStr.c_str()).first)
            // {
            //     SegmentManager::Instance().Segment()->destroy<SharedPtrOwner>(ownerStr.c_str());
            // }

            // if (SegmentManager::Instance().Segment()->find<ShmChunk>(chunkStr.c_str()).first)
            // {
            //     SegmentManager::Instance().Segment()->destroy<ShmChunk>(chunkStr.c_str());
            // }

            unique_lock<mutex> lock(fAckMutex);
            fAckCV.wait_for(lock, chrono::milliseconds(500));
            continue;
        }

        {
            unique_lock<mutex> containerLock(fContainerMutex);
            fPtrs.insert(make_pair(numSentMsgs, move(owner)));
        }

        // LOG(TRACE) << "Shared pointer constructed at: " << ownerStr;
        // LOG(TRACE) << "Chunk constructed at: " << chunkStr;

        void* ptr = owner->fSharedPtr->GetData();

        // write something to memory, otherwise only allocation will be measured
        // memset(ptr, 0, fMsgSize);

        // static_cast<char*>(ptr)[3] = charnum++;
        // if (charnum == 123)
        // {
        //     charnum = 97;
        // }
        // char* cptr = static_cast<char*>(ptr);
        // LOG(TRACE) << "check: " << cptr[3];

        // LOG(TRACE) << "chunk handle: " << owner->fSharedPtr->GetHandle();
        // LOG(TRACE) << "chunk size: " << owner->fSharedPtr->GetSize();
        // LOG(TRACE) << "owner use count: " << owner->fSharedPtr.use_count();

        fPtrs.at(numSentMsgs)->fRcvCount += 1;
        // fPtrs.at(numSentMsgs)->fRcvCount += 1;
        // fPtrs.at(numSentMsgs)->fRcvCount += 1;
        // fPtrs.at(numSentMsgs)->fRcvCount += 1;

        bpt::ptime sndTill = bpt::microsec_clock::universal_time() + bpt::milliseconds(1000);

        if (metaQueue1.timed_send(&numSentMsgs, sizeof(numSentMsgs), 0, sndTill))
        {
            // LOG(TRACE) << "Sent meta.";
            fBytesOutNew += fMsgSize;
            ++fMsgOutNew;
        }
        else
        {
            LOG(WARN) << "Did not send anything within 1 second.";
            fPtrs.erase(numSentMsgs);
            SegmentManager::Instance().Segment()->destroy_ptr(owner);
        }

        // if (metaQueue2.timed_send(&numSentMsgs, sizeof(numSentMsgs), 0, sndTill))
        // {
        //     // LOG(TRACE) << "Sent meta.";
        //     fBytesOutNew += fMsgSize;
        //     ++fMsgOutNew;
        // }
        // else
        // {
        //     LOG(WARN) << "Did not send anything within 1 second.";
        //     fPtrs.erase(numSentMsgs);
        //     SegmentManager::Instance().Segment()->destroy_ptr(owner);
        // }

        // if (metaQueue3.timed_send(&numSentMsgs, sizeof(numSentMsgs), 0, sndTill))
        // {
        //     // LOG(TRACE) << "Sent meta.";
        //     fBytesOutNew += fMsgSize;
        //     ++fMsgOutNew;
        // }
        // else
        // {
        //     LOG(WARN) << "Did not send anything within 1 second.";
        //     fPtrs.erase(numSentMsgs);
        //     SegmentManager::Instance().Segment()->destroy_ptr(owner);
        // }

        // if (metaQueue4.timed_send(&numSentMsgs, sizeof(numSentMsgs), 0, sndTill))
        // {
        //     // LOG(TRACE) << "Sent meta.";
        //     fBytesOutNew += fMsgSize;
        //     ++fMsgOutNew;
        // }
        // else
        // {
        //     LOG(WARN) << "Did not send anything within 1 second.";
        //     fPtrs.erase(numSentMsgs);
        //     SegmentManager::Instance().Segment()->destroy_ptr(owner);
        // }

        ++numSentMsgs;

        // --fMsgCounter;
        // while (fMsgCounter == 0)
        // {
        //     this_thread::sleep_for(chrono::milliseconds(1));
        // }
    }

    LOG(INFO) << "Sent " << numSentMsgs << " messages, leaving RUNNING state.";
    LOG(INFO) << "Sending time: ";

    ackListener.join();
    rateLogger.join();
    // resetMsgCounter.join();
}

void FairMQExampleShmSampler::ListenForAcks()
{
    message_queue ackQueue(open_only, "ack_queue");
    unsigned int priority;
    message_queue::size_type rcvSize;

    uint64_t id = 0;

    while (CheckCurrentState(RUNNING))
    {
        bpt::ptime rcvTill = bpt::microsec_clock::universal_time() + bpt::milliseconds(1000);

        if (ackQueue.timed_receive(&id, sizeof(id), rcvSize, priority, rcvTill))
        {
            // LOG(TRACE) << "Received ack for: " << id;
            {
                unique_lock<mutex> containerLock(fContainerMutex);
                if (fPtrs.find(id) != fPtrs.end())
                {
                    fPtrs.at(id)->fRcvCount -= 1;
                    if (fPtrs.at(id)->fRcvCount == 0)
                    {
                        SegmentManager::Instance().Segment()->destroy_ptr(fPtrs.at(id));
                        fPtrs.erase(id);
                    }
                }
                else
                {
                    LOG(WARN) << "Received ack for an id not contained in the container";
                }
                // LOG(TRACE) << fPtrs.size();
            }
            fAckCV.notify_all();
        }
        else
        {
            LOG(WARN) << "Did not receive anything within 1 second.";
        }
    }
    LOG(INFO) << "Ack listener finished.";
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
