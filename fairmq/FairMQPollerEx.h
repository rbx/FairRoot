/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef FAIRMQPOLLEREX_H_
#define FAIRMQPOLLEREX_H_

#include "FairMQChannel.h"

#include <boost/asio/io_service.hpp>

class FairMQPollerEx
{
  public:
    FairMQPollerEx();

    void AddChannel(const FairMQChannel& channel);

    virtual ~FairMQPollerEx();

  private:
    boost::asio::io_service fIoService;
};

#endif /* FAIRMQPOLLEREX_H_ */
