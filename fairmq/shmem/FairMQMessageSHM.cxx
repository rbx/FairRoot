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
#include "FairMQRegionSHM.h"
#include "FairMQLogger.h"
#include "FairMQShmCommon.h"

using namespace std;
using namespace fair::mq::shmem;

namespace bipc = boost::interprocess;

// string FairMQMessageSHM::fDeviceID = string();
atomic<bool> FairMQMessageSHM::fInterrupted(false);
FairMQ::Transport FairMQMessageSHM::fTransportType = FairMQ::Transport::SHM;

FairMQMessageSHM::FairMQMessageSHM()
    : fMessage()
    , fShPtr(nullptr)
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle()
    , fSize(0)
    , fLocalPtr(nullptr)
    , fRemoteRegion(nullptr)
{
    if (zmq_msg_init(&fMessage) != 0)
    {
        LOG(ERROR) << "failed initializing message, reason: " << zmq_strerror(errno);
    }
    fMetaCreated = true;
}

FairMQMessageSHM::FairMQMessageSHM(const size_t size)
    : fMessage()
    , fShPtr(nullptr)
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle()
    , fSize(0)
    , fLocalPtr(nullptr)
    , fRemoteRegion(nullptr)
{
    InitializeChunk(size);
}

FairMQMessageSHM::FairMQMessageSHM(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
    : fMessage()
    , fShPtr(nullptr)
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(0)
    , fHandle()
    , fSize(0)
    , fLocalPtr(nullptr)
    , fRemoteRegion(nullptr)
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

FairMQMessageSHM::FairMQMessageSHM(FairMQRegionPtr& region, void* data, const size_t size)
    : fMessage()
    , fShPtr(nullptr)
    , fQueued(false)
    , fMetaCreated(false)
    , fRegionId(static_cast<FairMQRegionSHM*>(region.get())->fRegionId)
    , fHandle()
    , fSize(size)
    , fLocalPtr(data)
    , fRemoteRegion(nullptr)
{
    fHandle = (bipc::managed_shared_memory::handle_t)(reinterpret_cast<const char*>(data) - reinterpret_cast<const char*>(region->GetData()));

    if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) != 0)
    {
        LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
    }
    else
    {
        MetaHeader* metaPtr = new(zmq_msg_data(&fMessage)) MetaHeader();
        metaPtr->fSize = size;
        metaPtr->fHandle = fHandle;
        metaPtr->fRegionId = fRegionId;

        fMetaCreated = true;
    }
}

bool FairMQMessageSHM::InitializeChunk(const size_t size)
{
    while (!fHandle)
    {
        try
        {
            fShPtr = Manager::Instance().Segment()->construct<ShmShPtr>(bipc::anonymous_instance)(
                make_managed_shared_ptr(Manager::Instance().Segment()->construct<Chunk>(bipc::anonymous_instance)(size),
                    *(Manager::Instance().Segment())));
            fLocalPtr = fShPtr->get()->GetData();
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
        fHandle = Manager::Instance().Segment()->get_handle_from_address(fShPtr);
    }

    fSize = size;

    if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) != 0)
    {
        LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
        return false;
    }
    MetaHeader* metaPtr = new(zmq_msg_data(&fMessage)) MetaHeader();
    metaPtr->fSize = size;
    metaPtr->fHandle = fHandle;
    metaPtr->fRegionId = fRegionId;

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
            fLocalPtr = fShPtr->get()->GetData();
            return fLocalPtr;
        }
        else
        {
            if (!fRemoteRegion)
            {
                fRemoteRegion = FairMQRegionPtr(new FairMQRegionSHM(fRegionId, true));
            }
            fLocalPtr = reinterpret_cast<char*>(fRemoteRegion->GetData()) + fHandle;
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

void FairMQMessageSHM::SetDeviceId(const string& /*deviceId*/)
{
    // fDeviceID = deviceId;
}

FairMQ::Transport FairMQMessageSHM::GetType() const
{
    return fTransportType;
}

void FairMQMessageSHM::Copy(const unique_ptr<FairMQMessage>& msg)
{
    // if (!fHandle)
    // {
    //     bipc::managed_shared_memory::handle_t otherHandle = static_cast<FairMQMessageSHM*>(msg.get())->fHandle;
    //     if (otherHandle)
    //     {
    //         if (InitializeChunk(msg->GetSize()))
    //         {
    //             memcpy(GetData(), msg->GetData(), msg->GetSize());
    //         }
    //     }
    //     else
    //     {
    //         LOG(ERROR) << "FairMQMessageSHM::Copy() fail: source message not initialized!";
    //     }


    // version with sharing the sent data
    if (!fShPtr)
    {
        if (static_cast<FairMQMessageSHM*>(msg.get())->fShPtr)
        {
            while (!fHandle)
            {
                try
                {
                    fShPtr = Manager::Instance().Segment()->construct<ShmShPtr>(bipc::anonymous_instance)(*(static_cast<FairMQMessageSHM*>(msg.get())->fShPtr));
                    fLocalPtr = fShPtr->get()->GetData();
                }
                catch (bipc::bad_alloc& ba)
                {
                    // LOG(WARN) << "Shared memory full...";
                    this_thread::sleep_for(chrono::milliseconds(50));
                    if (fInterrupted)
                    {
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                fHandle = Manager::Instance().Segment()->get_handle_from_address(fShPtr);
            }

            fSize = static_cast<FairMQMessageSHM*>(msg.get())->fSize;

            if (zmq_msg_init_size(&fMessage, sizeof(MetaHeader)) == 0)
            {
                MetaHeader* metaPtr = new(zmq_msg_data(&fMessage)) MetaHeader();
                metaPtr->fSize = fSize;
                metaPtr->fHandle = fHandle;
                metaPtr->fRegionId = fRegionId;

                fMetaCreated = true;
            }
            else
            {
                LOG(ERROR) << "failed initializing meta message, reason: " << zmq_strerror(errno);
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
    if (fRegionId == 0)
    {
        if (fHandle && !fQueued)
        {
            Manager::Instance().Segment()->destroy_ptr(fShPtr);
            fShPtr = nullptr;
            fHandle = 0;
            // LOG(WARN) << "Destroying unsent message";
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
