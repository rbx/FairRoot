/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQTransportFactorySHM.h
 *
 * @since 2016-06-01
 * @author: A. Rybalchenko
 */

#ifndef FAIRMQTRANSPORTFACTORYSHM_H_
#define FAIRMQTRANSPORTFACTORYSHM_H_

#include <vector>

#include "FairMQTransportFactory.h"
#include "FairMQContextSHM.h"
#include "FairMQMessageSHM.h"
#include "FairMQSocketSHM.h"
#include "FairMQPollerSHM.h"

class FairMQTransportFactorySHM : public FairMQTransportFactory
{
  public:
    FairMQTransportFactorySHM();

    virtual FairMQMessage* CreateMessage();
    virtual FairMQMessage* CreateMessage(const size_t size);
    virtual FairMQMessage* CreateMessage(void* data, const size_t size, fairmq_free_fn* ffn, void* hint = NULL);

    virtual FairMQSocket* CreateSocket(const std::string& type, const std::string& name, const int numIoThreads, const std::string& id = "");

    virtual FairMQPoller* CreatePoller(const std::vector<FairMQChannel>& channels);
    virtual FairMQPoller* CreatePoller(const std::unordered_map<std::string, std::vector<FairMQChannel>>& channelsMap, const std::initializer_list<std::string> channelList);
    virtual FairMQPoller* CreatePoller(const FairMQSocket& cmdSocket, const FairMQSocket& dataSocket);

    virtual ~FairMQTransportFactorySHM() {};
};

#endif /* FAIRMQTRANSPORTFACTORYSHM_H_ */
