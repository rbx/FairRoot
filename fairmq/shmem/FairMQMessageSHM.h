/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQMessageSHM.h
 *
 * @since 2016-06-01
 * @author A. Rybalchenko
 */

#ifndef FAIRMQMESSAGESHM_H_
#define FAIRMQMESSAGESHM_H_

#include "FairMQShmManager.h"
#include "FairMQMessage.h"

#include <atomic>

class FairMQMessageSHM : public FairMQMessage
{
  public:
    FairMQMessageSHM();
    FairMQMessageSHM(const size_t size);
    FairMQMessageSHM(void* data, const size_t size, fairmq_free_fn* ffn, void* hint = nullptr);

    virtual void Rebuild();
    virtual void Rebuild(const size_t size);
    virtual void Rebuild(void* data, const size_t size, fairmq_free_fn* ffn, void* hint = nullptr);

    virtual void* GetMessage();
    virtual void* GetData();
    virtual size_t GetSize();

    virtual void SetMessage(void* data, const size_t size);

    virtual void CloseMessage();
    virtual void Copy(FairMQMessage* msg);
    virtual void Copy(const std::unique_ptr<FairMQMessage>& msg);

    virtual ~FairMQMessageSHM();

  private:
    static std::atomic<uint64_t> fIdCounter;
    uint64_t fMessage;
    size_t fSize;
    static std::string fDeviceId;
    FairMQShmShPtrOwner* fOwner;
    FairMQShmShPtrType fLocalPtr;
};

#endif /* FAIRMQMESSAGESHM_H_ */
