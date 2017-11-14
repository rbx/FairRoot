/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef FAIRMQUNMANAGEDREGIONSHM_H_
#define FAIRMQUNMANAGEDREGIONSHM_H_

#include "FairMQUnmanagedRegion.h"
#include "FairMQLogger.h"

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <cstddef> // size_t
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

class FairMQUnmanagedRegionSHM : public FairMQUnmanagedRegion
{
    friend class FairMQSocketSHM;
    friend class FairMQMessageSHM;

  public:
    FairMQUnmanagedRegionSHM(const size_t size);

    virtual void* GetData() const override;
    virtual size_t GetSize() const override;

    virtual ~FairMQUnmanagedRegionSHM();

  private:
    static std::atomic<bool> fInterrupted;
    boost::interprocess::mapped_region* fRegion;
    uint64_t fRegionId;
};

#endif /* FAIRMQUNMANAGEDREGIONSHM_H_ */