/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQMessageNN.cxx
 *
 * @since 2013-12-05
 * @author A. Rybalchenko
 */

#include <cstring>
#include <stdlib.h>

#include <nanomsg/nn.h>

#include "FairMQMessageNN.h"
#include "FairMQLogger.h"

using namespace std;

FairMQ::Transport FairMQMessageNN::fTransportType = FairMQ::Transport::NN;

FairMQMessageNN::FairMQMessageNN()
    : fMessage(nullptr)
    , fSize(0)
    , fReceiving(false)
    , fRegionPtr(nullptr)
{
    fMessage = nn_allocmsg(0, 0);
    if (!fMessage)
    {
        LOG(ERROR) << "failed allocating message, reason: " << nn_strerror(errno);
    }
}

FairMQMessageNN::FairMQMessageNN(const size_t size)
    : fMessage(nullptr)
    , fSize(0)
    , fReceiving(false)
    , fRegionPtr(nullptr)
{
    fMessage = nn_allocmsg(size, 0);
    if (!fMessage)
    {
        LOG(ERROR) << "failed allocating message, reason: " << nn_strerror(errno);
    }
    fSize = size;
}


/* nanomsg does not offer support for creating a message out of an existing buffer,
 * therefore the following method is using memcpy. For more efficient handling,
 * create FairMQMessage object only with size parameter and fill it with data.
 * possible TODO: make this zero copy (will should then be as efficient as ZeroMQ).
*/
FairMQMessageNN::FairMQMessageNN(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
    : fMessage(nullptr)
    , fSize(0)
    , fReceiving(false)
    , fRegionPtr(nullptr)
{
    fMessage = nn_allocmsg(size, 0);
    if (!fMessage)
    {
        LOG(ERROR) << "failed allocating message, reason: " << nn_strerror(errno);
    }
    else
    {
        memcpy(fMessage, data, size);
        fSize = size;
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

FairMQMessageNN::FairMQMessageNN(FairMQUnmanagedRegionPtr& region, void* data, const size_t size)
    : fMessage(data)
    , fSize(size)
    , fReceiving(false)
    , fRegionPtr(region.get())
{
    // currently nanomsg will copy the buffer (data) inside nn_sendmsg()
}

void FairMQMessageNN::Rebuild()
{
    Clear();
    fReceiving = false;
}

void FairMQMessageNN::Rebuild(const size_t size)
{
    Clear();
    fMessage = nn_allocmsg(size, 0);
    if (!fMessage)
    {
        LOG(ERROR) << "failed allocating message, reason: " << nn_strerror(errno);
    }
    fSize = size;
    fReceiving = false;
}

void FairMQMessageNN::Rebuild(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
{
    Clear();
    fMessage = nn_allocmsg(size, 0);
    if (!fMessage)
    {
        LOG(ERROR) << "failed allocating message, reason: " << nn_strerror(errno);
    }
    else
    {
        memcpy(fMessage, data, size);
        fSize = size;
        fReceiving = false;

        if (ffn)
        {
            ffn(data, hint);
        }
    }
}

void* FairMQMessageNN::GetMessage()
{
    return fMessage;
}

void* FairMQMessageNN::GetData()
{
    return fMessage;
}

size_t FairMQMessageNN::GetSize() const
{
    return fSize;
}

bool FairMQMessageNN::SetUsedSize(const size_t size)
{
    if (size <= fSize)
    {
        // with size smaller than original nanomsg will simply "chop" the data, avoiding reallocation
        fMessage = nn_reallocmsg(fMessage, size);
        fSize = size;
        return true;
    }
    else
    {
        LOG(ERROR) << "FairMQMessageNN::SetUsedSize: cannot set used size higher than original.";
        return false;
    }
}

void FairMQMessageNN::SetMessage(void* data, const size_t size)
{
    fMessage = data;
    fSize = size;
}

FairMQ::Transport FairMQMessageNN::GetType() const
{
    return fTransportType;
}

void FairMQMessageNN::Copy(const unique_ptr<FairMQMessage>& msg)
{
    if (fMessage)
    {
        if (nn_freemsg(fMessage) < 0)
        {
            LOG(ERROR) << "failed freeing message, reason: " << nn_strerror(errno);
        }
    }

    size_t size = msg->GetSize();

    fMessage = nn_allocmsg(size, 0);
    if (!fMessage)
    {
        LOG(ERROR) << "failed allocating message, reason: " << nn_strerror(errno);
    }
    else
    {
        memcpy(fMessage, msg->GetMessage(), size);
        fSize = size;
    }
}

void FairMQMessageNN::Clear()
{
    if (nn_freemsg(fMessage) < 0)
    {
        LOG(ERROR) << "failed freeing message, reason: " << nn_strerror(errno);
    }
    else
    {
        fMessage = nullptr;
        fSize = 0;
    }
}

FairMQMessageNN::~FairMQMessageNN()
{
    if (fReceiving)
    {
        int rc = nn_freemsg(fMessage);
        if (rc < 0)
        {
            LOG(ERROR) << "failed freeing message, reason: " << nn_strerror(errno);
        }
        else
        {
            fMessage = nullptr;
            fSize = 0;
        }
    }
}
