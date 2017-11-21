/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
#ifndef FAIRMQMESSAGESHM_H_
#define FAIRMQMESSAGESHM_H_

#include <cstddef> // size_t
#include <string>
#include <atomic>

#include <zmq.h>

#include <boost/interprocess/mapped_region.hpp>

#include "FairMQMessage.h"
#include "FairMQUnmanagedRegion.h"
#include "FairMQShmManager.h"

class FairMQMessageSHM : public FairMQMessage
{
    friend class FairMQSocketSHM;

  public:
    FairMQMessageSHM();
    FairMQMessageSHM(const size_t size);
    FairMQMessageSHM(void* data, const size_t size, fairmq_free_fn* ffn, void* hint = nullptr);
    FairMQMessageSHM(FairMQUnmanagedRegionPtr& region, void* data, const size_t size);

    FairMQMessageSHM(const FairMQMessageSHM&) = delete;
    FairMQMessageSHM operator=(const FairMQMessageSHM&) = delete;

    bool InitializeChunk(const size_t size);

    void Rebuild() override;
    void Rebuild(const size_t size) override;
    void Rebuild(void* data, const size_t size, fairmq_free_fn* ffn, void* hint = nullptr) override;

    void* GetMessage() override;
    void* GetData() override;
    size_t GetSize() override;

    void SetMessage(void* data, const size_t size) override;

    FairMQ::Transport GetType() const override;

    void Copy(const std::unique_ptr<FairMQMessage>& msg) override;

    void CloseMessage();

    ~FairMQMessageSHM() override;

  private:
    zmq_msg_t fMessage;
    bool fQueued;
    bool fMetaCreated;
    static std::atomic<bool> fInterrupted;
    static FairMQ::Transport fTransportType;
    uint64_t fRegionId;
    boost::interprocess::managed_shared_memory::handle_t fHandle;
    size_t fSize;
    void* fLocalPtr;
};

#endif /* FAIRMQMESSAGESHM_H_ */
