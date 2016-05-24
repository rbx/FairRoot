/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQTransportFactorySHM.cxx
 *
 * @since 2016-06-01
 * @author: A. Rybalchenko
 */

#include "FairMQTransportFactorySHM.h"

using namespace std;

FairMQTransportFactorySHM::FairMQTransportFactorySHM()
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    LOG(DEBUG) << "Using ZeroMQ library, version: " << major << "." << minor << "." << patch;
}

FairMQMessage* FairMQTransportFactorySHM::CreateMessage()
{
    return new FairMQMessageZMQ();
}

FairMQMessage* FairMQTransportFactorySHM::CreateMessage(const size_t size)
{
    return new FairMQMessageZMQ(size);
}

FairMQMessage* FairMQTransportFactorySHM::CreateMessage(void* data, const size_t size, fairmq_free_fn* ffn, void* hint)
{
    return new FairMQMessageZMQ(data, size, ffn, hint);
}

FairMQSocket* FairMQTransportFactorySHM::CreateSocket(const string& type, const std::string& name, const int numIoThreads, const std::string& id /*= ""*/)
{
    return new FairMQSocketZMQ(type, name, numIoThreads, id);
}

FairMQPoller* FairMQTransportFactorySHM::CreatePoller(const vector<FairMQChannel>& channels)
{
    return new FairMQPollerZMQ(channels);
}

FairMQPoller* FairMQTransportFactorySHM::CreatePoller(const std::unordered_map<std::string, std::vector<FairMQChannel>>& channelsMap, const std::initializer_list<std::string> channelList)
{
    return new FairMQPollerZMQ(channelsMap, channelList);
}

FairMQPoller* FairMQTransportFactorySHM::CreatePoller(const FairMQSocket& cmdSocket, const FairMQSocket& dataSocket)
{
    return new FairMQPollerZMQ(cmdSocket, dataSocket);
}
