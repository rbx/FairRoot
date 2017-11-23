/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQShmManager.h
 *
 * @since 2016-04-08
 * @author A. Rybalchenko
 */

#ifndef FAIRMQSHMMANAGER_H_
#define FAIRMQSHMMANAGER_H_

#include "FairMQLogger.h"
#include "fairmq/Tools.h"

#include <boost/asio.hpp>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

#include <thread>
#include <chrono>
#include <unordered_map>

namespace fair
{
namespace mq
{
namespace shmem
{

struct RegionBlock
{
    RegionBlock(boost::interprocess::managed_shared_memory::handle_t handle, size_t size)
        : fHandle(handle)
        , fSize(size)
    {}

    RegionBlock() = delete;

    boost::interprocess::managed_shared_memory::handle_t fHandle;
    size_t fSize;
};

struct Region
{
    Region(uint64_t regionId, uint64_t size, bool remote)
        : fRemote(remote)
        , fName("fmq_shm_region_" + std::to_string(regionId))
        , fQueueName("fmq_shm_region_queue_" + std::to_string(regionId))
        , fShmemObject()
        , fQueue(nullptr)
    {
        if (fRemote)
        {
            fShmemObject = boost::interprocess::shared_memory_object(boost::interprocess::open_only, fName.c_str(), boost::interprocess::read_write);
            LOG(DEBUG) << "shmem: located remote region: " << fName;

            fQueue = fair::mq::tools::make_unique<boost::interprocess::message_queue>(boost::interprocess::open_only, fQueueName.c_str());
            LOG(DEBUG) << "shmem: located remote region queue: " << fQueueName;
        }
        else
        {
            fShmemObject = boost::interprocess::shared_memory_object(boost::interprocess::create_only, fName.c_str(), boost::interprocess::read_write);
            LOG(DEBUG) << "shmem: created region: " << fName;
            fShmemObject.truncate(size);

            fQueue = fair::mq::tools::make_unique<boost::interprocess::message_queue>(boost::interprocess::create_only, fQueueName.c_str(), 10000, sizeof(RegionBlock));
            LOG(DEBUG) << "shmem: created region queue: " << fQueueName;
        }
        fRegion = boost::interprocess::mapped_region(fShmemObject, boost::interprocess::read_write); // TODO: add HUGEPAGES flag here
    }

    Region() = delete;

    Region(const Region&) = default;
    Region(Region&&) = default;

    ~Region()
    {
        if (!fRemote)
        {
            if (boost::interprocess::shared_memory_object::remove(fName.c_str()))
            {
                LOG(DEBUG) << "shmem: destroyed region " << fName;
            }

            if (boost::interprocess::message_queue::remove(fQueueName.c_str()))
            {
                LOG(DEBUG) << "shmem: removed region queue " << fName;
            }
        }
        else
        {
            LOG(DEBUG) << "shmem: region '" << fName << "' is remote, no cleanup necessary.";
            LOG(DEBUG) << "shmem: region queue '" << fQueueName << "' is remote, no cleanup necessary";
        }
    }

    bool fRemote;
    std::string fName;
    std::string fQueueName;
    boost::interprocess::shared_memory_object fShmemObject;
    boost::interprocess::mapped_region fRegion;
    std::unique_ptr<boost::interprocess::message_queue> fQueue;
};

class Manager
{
  public:
    Manager(const std::string& name, size_t size)
        : fName(name)
        , fSegment(boost::interprocess::open_or_create, fName.c_str(), size)
        , fManagementSegment(boost::interprocess::open_or_create, "fmq_shm_management", 65536)
        , fRegions()
        // , fIos()
        // , fWork(fair::mq::tools::make_unique<boost::asio::io_service::work>(fIos))
        // , fWorkers()
    {}

    Manager() = delete;

    Manager(const Manager&) = delete;
    Manager operator=(const Manager&) = delete;

    boost::interprocess::managed_shared_memory& Segment()
    {
        return fSegment;
    }

    boost::interprocess::mapped_region* CreateRegion(const size_t size, const uint64_t regionId)
    {
        auto it = fRegions.find(regionId);
        if (it != fRegions.end())
        {
            LOG(ERROR) << "shmem: Trying to create a region that already exists";
            return nullptr;
        }
        else
        {
            auto r = fRegions.emplace(regionId, Region{regionId, size, false});

            // create handler feedback


            return &(r.first->second.fRegion);
        }
    }

    boost::interprocess::mapped_region* GetRemoteRegion(const uint64_t regionId)
    {
        // remote region could actually be a local one if a message originates from this device (has been sent out and returned)
        auto it = fRegions.find(regionId);
        if (it != fRegions.end())
        {
            return &(it->second.fRegion);
        }
        else
        {
            auto r = fRegions.emplace(regionId, Region{regionId, 0, true});

            return &(r.first->second.fRegion);
        }
    }

    void RemoveRegion(const uint64_t regionId)
    {
        fRegions.erase(regionId);
    }

    boost::interprocess::message_queue& GetRegionQueue(const uint64_t regionId)
    {
        return *(fRegions.at(regionId).fQueue);
    }

    void Remove()
    {
        if (boost::interprocess::shared_memory_object::remove(fName.c_str()))
        {
            LOG(DEBUG) << "shmem: successfully removed " << fName << " segment after the device has stopped.";
        }
        else
        {
            LOG(DEBUG) << "shmem: did not remove " << fName << " segment after the device stopped. Already removed?";
        }

        if (boost::interprocess::shared_memory_object::remove("fmq_shm_management"))
        {
            LOG(DEBUG) << "shmem: successfully removed \"fmq_shm_management\" segment after the device has stopped.";
        }
        else
        {
            LOG(DEBUG) << "shmem: did not remove \"fmq_shm_management\" segment after the device stopped. Already removed?";
        }
    }

    boost::interprocess::managed_shared_memory& ManagementSegment()
    {
        return fManagementSegment;
    }

  private:
    std::string fName;
    boost::interprocess::managed_shared_memory fSegment;
    boost::interprocess::managed_shared_memory fManagementSegment;
    std::unordered_map<uint64_t, Region> fRegions;
    // boost::asio::io_service fIos;
    // std::unique_ptr<boost::asio::io_service::work> fWork;
    // std::vector<std::thread> fWorkers;
};

} // namespace shmem
} // namespace mq
} // namespace fair

#endif /* FAIRMQSHMMANAGER_H_ */
