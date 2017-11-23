/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
#include <string>
#include <cstdlib>

#include "FairMQMessageSHM.h"
#include "FairMQUnmanagedRegionSHM.h"
#include "FairMQLogger.h"
#include "FairMQShmCommon.h"

using namespace std;
using namespace fair::mq::shmem;

namespace bipc = boost::interprocess;

atomic<bool> FairMQMessageSHM::fInterrupted(false);
FairMQ::Transport FairMQMessageSHM::fTransportType = FairMQ::Transport::SHM;

FairMQMessageSHM::FairMQMessageSHM()
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle(-1)
    , fSize(0)
    , fLocalPtr(nullptr)
{
    if (zmq_msg_init(&fMessage) != 0)
    {
        LOG(ERROR) << "failed initializing message, reason: " << zmq_strerror(errno);
    }
    fMetaCreated = true;
}

FairMQMessageSHM::FairMQMessageSHM(const size_t size)
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle(-1)
    , fSize(0)
    , fLocalPtr(nullptr)
{
    InitializeChunk(size);
}

FairMQMessageSHM::FairMQMessageSHM(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle(-1)
    , fSize(0)
    , fLocalPtr(nullptr)
{
    if (InitializeChunk(size))
    {
        memcpy(fLocalPtr, data, size);
        if (ffn)
        {
            ffn(data, hint);
        }
        else
        {
            free(data);
        }
    }
}

FairMQMessageSHM::FairMQMessageSHM(FairMQUnmanagedRegionPtr& region, void* data, const size_t size)
    : fMessage()
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(static_cast<FairMQUnmanagedRegionSHM*>(region.get())->fRegionId)
    , fHandle(-1)
    , fSize(size)
    , fLocalPtr(data)
{
    fHandle = (bipc::managed_shared_memory::handle_t)(reinterpret_cast<const char*>(data) - reinterpret_cast<const char*>(region->GetData()));

    if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) != 0)
    {
        LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
    }
    else
    {
        MetaHeader header;
        header.fSize = size;
        header.fHandle = fHandle;
        header.fRegionId = fRegionId;
        memcpy(zmq_msg_data(&fMessage), &header, sizeof(MetaHeader));

        fMetaCreated = true;
    }
}

bool FairMQMessageSHM::InitializeChunk(const size_t size)
{
    while (fHandle < 0)
    {
        try
        {
            fLocalPtr = Manager::Instance().Segment()->allocate(size);
        }
        catch (bipc::bad_alloc& ba)
        {
            // LOG(WARN) << "Shared memory full...";
            this_thread::sleep_for(chrono::milliseconds(50));
            if (fInterrupted)
            {
                return false;
            }
            else
            {
                continue;
            }
        }
        fHandle = Manager::Instance().Segment()->get_handle_from_address(fLocalPtr);
    }

    fSize = size;

    if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) != 0)
    {
        LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
        return false;
    }
    MetaHeader header;
    header.fSize = size;
    header.fHandle = fHandle;
    header.fRegionId = fRegionId;
    memcpy(zmq_msg_data(&fMessage), &header, sizeof(MetaHeader));

    fMetaCreated = true;

    return true;
}

void FairMQMessageSHM::Rebuild()
{
    CloseMessage();

    fQueued = false;

    if (zmq_msg_init(&fMessage) != 0)
    {
        LOG(ERROR) << "failed initializing message, reason: " << zmq_strerror(errno);
    }
    fMetaCreated = true;
}

void FairMQMessageSHM::Rebuild(const size_t size)
{
    CloseMessage();

    fQueued = false;

    InitializeChunk(size);
}

void FairMQMessageSHM::Rebuild(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
{
    CloseMessage();

    fQueued = false;

    if (InitializeChunk(size))
    {
        memcpy(fLocalPtr, data, size);
        if (ffn)
        {
            ffn(data, hint);
        }
        else
        {
            free(data);
        }
    }
}

void* FairMQMessageSHM::GetMessage()
{
    return &fMessage;
}

void* FairMQMessageSHM::GetData()
{
    if (fLocalPtr)
    {
        return fLocalPtr;
    }
    else
    {
        if (fRegionId == 0)
        {
            return Manager::Instance().Segment()->get_address_from_handle(fHandle);
        }
        else
        {
            boost::interprocess::mapped_region* region = Manager::Instance().GetRemoteRegion(fRegionId);
            fLocalPtr = reinterpret_cast<char*>(region->get_address()) + fHandle;
            return fLocalPtr;
        }
    }
}

size_t FairMQMessageSHM::GetSize()
{
    return fSize;
}

void FairMQMessageSHM::SetMessage(void*, const size_t)
{
    // dummy method to comply with the interface. functionality not allowed in zeromq.
}

FairMQ::Transport FairMQMessageSHM::GetType() const
{
    return fTransportType;
}

void FairMQMessageSHM::Copy(const unique_ptr<FairMQMessage>& msg)
{
    if (!fHandle)
    {
        bipc::managed_shared_memory::handle_t otherHandle = static_cast<FairMQMessageSHM*>(msg.get())->fHandle;
        if (otherHandle)
        {
            if (InitializeChunk(msg->GetSize()))
            {
                memcpy(GetData(), msg->GetData(), msg->GetSize());
            }
        }
        else
        {
            LOG(ERROR) << "FairMQMessageSHM::Copy() fail: source message not initialized!";
        }
    }
    else
    {
        LOG(ERROR) << "FairMQMessageSHM::Copy() fail: target message already initialized!";
    }
}

void FairMQMessageSHM::CloseMessage()
{
    if (fHandle >= 0 && !fQueued)
    {
        if (fRegionId == 0)
        {
            Manager::Instance().Segment()->deallocate(Manager::Instance().Segment()->get_address_from_handle(fHandle));
            fHandle = 0;
        }
        else
        {
            // send notification back to the receiver
            RegionBlock block(fHandle, fSize);
            if (Manager::Instance().GetRegionQueue(fRegionId).try_send(static_cast<void*>(&block), sizeof(RegionBlock), 0))
            {
                // LOG(INFO) << "true";
            }
        }
    }

    if (fMetaCreated)
    {
        if (zmq_msg_close(&fMessage) != 0)
        {
            LOG(ERROR) << "failed closing message, reason: " << zmq_strerror(errno);
        }
    }
}

FairMQMessageSHM::~FairMQMessageSHM()
{
    CloseMessage();
}
