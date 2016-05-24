/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQMessageSHM.cxx
 *
 * @since 2016-06-01
 * @author D. Klein, A. Rybalchenko, N. Winckler
 */

#include <cstring>
#include <cstdlib>

#include <boost/interprocess/managed_shared_memory.hpp>

#include "FairMQShmManager.h"
#include "FairMQShmSharedPtrOwner.h"
#include "FairMQShmChunk.h"
#include "FairMQLogger.h"

#include "FairMQMessageSHM.h"

using namespace std;
namespace bipc = boost::interprocess;

FairMQMessageSHM::fIdCounter = 0;

FairMQMessageSHM::FairMQMessageSHM()
    : fMessage(0)
    , fOwner(nullptr)
    , fSize(0)
    , fLocalPtr()
{
    // empty message for receiving or (zero-)copying into.
}

FairMQMessageSHM::FairMQMessageSHM(const size_t size)
    : fMessage(0)
    , fOwner(nullptr)
    , fSize(size)
    , fLocalPtr()
{
    fMessage = ++fIdCounter;

    fOwner = CreateMessage(fMessage, size);

    // LOG(TRACE) << "Shared pointer constructed at: " << ownerStr;
    // LOG(TRACE) << "Chunk constructed at: " << chunkStr;

    // fHandle = fOwner->fSharedPtr->GetHandle();
    // fSize = fOwner->fSharedPtr->GetSize();
}

FairMQMessageSHM::FairMQMessageSHM(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
    : fMessage(0)
    , fOwner(nullptr)
    , fSize(size)
    , fLocalPtr()
{
    fMessage = fIdCounter++;
    string chunkStr = "c" + to_string(fMessage);
    string ownerStr = "o" + to_string(fMessage);

    fOwner = CreateMessage(fMessage, ownerStr, chunkStr, size);

    memcpy(fOwner->fSharedPtr->GetData(), data, size);
    if (ffn)
    {
        ffn(data, hint);
    }
    else
    {
        free(data);
    }
}

void FairMQMessageSHM::Rebuild()
{
    CloseMessage();
    if (zmq_msg_init(&fMessage) != 0)
    {
        LOG(ERROR) << "failed initializing message, reason: " << zmq_strerror(errno);
    }
}

void FairMQMessageSHM::Rebuild(const size_t size)
{
    CloseMessage();
    if (zmq_msg_init_size(&fMessage, size) != 0)
    {
        LOG(ERROR) << "failed initializing message with size, reason: " << zmq_strerror(errno);
    }
}

void FairMQMessageSHM::Rebuild(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
{
    CloseMessage();
    if (zmq_msg_init_data(&fMessage, data, size, ffn, hint) != 0)
    {
        LOG(ERROR) << "failed initializing message with data, reason: " << zmq_strerror(errno);
    }
}

void* FairMQMessageSHM::GetMessage()
{
    return &fMessage;
}

void* FairMQMessageSHM::GetData()
{
    if (fOwner)
    {
        return fOwner->fSharedPtr->GetData();
    }
    else
    {
        LOG(ERROR) << "FairMQMessageSHM::GetData(): Accessing uninitialized message!";
        exit(EXIT_FAILURE);
    }
}

size_t FairMQMessageSHM::GetSize()
{
    return fSize;
}

void FairMQMessageSHM::SetMessage(void*, const size_t)
{
    // dummy method to comply with the interface. functionality not available for shared memory.
}

void FairMQMessageSHM::Copy(FairMQMessage* msg)
{
    // DEPRECATED: Use Copy(const unique_ptr<FairMQMessage>&)
}

void FairMQMessageSHM::Copy(const unique_ptr<FairMQMessage>& msg)
{
    // todo: increment RcvCount
    if (fOwner)
    {
        // cleanup the old owner
    }
    fMessage = *(static_cast<uint64_t*>(msg->GetMessage()));
    fSize = msg->GetSize();
    FairMQShmManager::Instance().Increment(fMessage);
}

inline void FairMQMessageSHM::CloseMessage()
{
    if (zmq_msg_close(&fMessage) != 0)
    {
        LOG(ERROR) << "failed closing message, reason: " << zmq_strerror(errno);
    }
}

FairMQMessageSHM::~FairMQMessageSHM()
{
    if (zmq_msg_close(&fMessage) != 0)
    {
        LOG(ERROR) << "failed closing message with data, reason: " << zmq_strerror(errno);
    }
}
