/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include "FairMQPollerEx.h"
#include "FairMQLogger.h"

#include <boost/asio/posix/stream_descriptor.hpp>

FairMQPollerEx::FairMQPollerEx()
    : fIoService()
{

}

void FairMQPollerEx::AddChannel(const FairMQChannel& channel)
{
    boost::asio::posix::stream_descriptor::native_handle_type fd = channel.GetSocket().GetFileDescriptor();
    boost::asio::posix::stream_descriptor sd(fIoService, fd);

    LOG(WARN) << fd;

    sd.release();
}

FairMQPollerEx::~FairMQPollerEx()
{

}
