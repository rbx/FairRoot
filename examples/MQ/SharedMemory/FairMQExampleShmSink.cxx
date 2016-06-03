/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQExampleShmSink.cxx
 *
 * @since 2016-04-08
 * @author A. Rybalchenko
 */

#include <string>
#include <thread>
#include <chrono>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>

#include "FairMQExampleShmSink.h"
#include "FairMQLogger.h"
#include "ShmChunk.h"

using namespace std;
using namespace boost::interprocess;

FairMQExampleShmSink::FairMQExampleShmSink()
    : fBytesIn(0)
    , fMsgIn(0)
    , fBytesInNew(0)
    , fMsgInNew(0)
{
}

FairMQExampleShmSink::~FairMQExampleShmSink()
{
}

void FairMQExampleShmSink::Init()
{
    SegmentManager::Instance().InitializeSegment("open_only", "FairMQSharedMemory");
    LOG(INFO) << "Opened shared memory segment 'FairMQSharedMemory'. Available are "
              << SegmentManager::Instance().Segment()->get_free_memory() << " bytes.";
}

void FairMQExampleShmSink::Run()
{
    static uint64_t numReceivedMsgs = 0;

    thread rateLogger(&FairMQExampleShmSink::Log, this, 1000);

    while (CheckCurrentState(RUNNING))
    {
        FairMQMessagePtr msg(NewMessage());

        if (Receive(msg, "meta") >= 0)
        {
            // get the shared pointer ID from the received message
            string ownerStr = "o" + to_string(*(static_cast<uint64_t*>(msg->GetData())));
            // LOG(DEBUG) << "Received message: " << ownerStr;

            // find the shared pointer in shared memory with its ID
            SharedPtrOwner* owner = SegmentManager::Instance().Segment()->find<SharedPtrOwner>(ownerStr.c_str()).first;
            // LOG(DEBUG) << "owner use count: " << owner->fSharedPtr.use_count();
            // create a local shared pointer from the received one (increments the reference count)
            SharedPtrType localPtr = owner->fSharedPtr;
            // LOG(DEBUG) << "owner use count: " << owner->fSharedPtr.use_count();

            // reply with the same string as an acknowledgement
            if (Send(msg, "ack") >= 0)
            {
                // LOG(DEBUG) << "Sent acknowledgement.";
            }

            if (localPtr)
            {
                // get memory address from the handle
                void* ptr = localPtr->GetData();

                // LOG(DEBUG) << "chunk handle: " << localPtr->GetHandle();
                // LOG(DEBUG) << "chunk size: " << localPtr->GetSize();

                fBytesInNew += localPtr->GetSize();
                ++fMsgInNew;

                // char* cptr = static_cast<char*>(ptr);
                // LOG(DEBUG) << "check: " << cptr[3];
            }
            else
            {
                LOG(WARN) << "Shared pointer is zero.";
            }

            // LOG(DEBUG) << "destroying local shared pointer";

            ++numReceivedMsgs;
        }
    }

    LOG(INFO) << "Received " << numReceivedMsgs << " messages, leaving RUNNING state.";

    rateLogger.join();
}

void FairMQExampleShmSink::Log(const int intervalInMs)
{
    timestamp_t t0 = get_timestamp();
    timestamp_t t1;
    timestamp_t msSinceLastLog;

    double mbPerSecIn = 0;
    double msgPerSecIn = 0;

    while (CheckCurrentState(RUNNING))
    {
        t1 = get_timestamp();

        msSinceLastLog = (t1 - t0) / 1000.0L;

        mbPerSecIn = (static_cast<double>(fBytesInNew - fBytesIn) / (1024. * 1024.)) / static_cast<double>(msSinceLastLog) * 1000.;
        fBytesIn = fBytesInNew;

        msgPerSecIn = static_cast<double>(fMsgInNew - fMsgIn) / static_cast<double>(msSinceLastLog) * 1000.;
        fMsgIn = fMsgInNew;

        LOG(DEBUG) << fixed
                   << setprecision(0) << "in: " << msgPerSecIn << " msg ("
                   << setprecision(2) << mbPerSecIn << " MB)\t("
                   << SegmentManager::Instance().Segment()->get_free_memory() / (1024. * 1024.) << " MB free)";

        t0 = t1;
        this_thread::sleep_for(chrono::milliseconds(intervalInMs));
    }
}
