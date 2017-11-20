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

#include <thread>
#include <chrono>
#include <unordered_map>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>

#include "FairMQLogger.h"

namespace fair
{
namespace mq
{
namespace shmem
{

namespace bipc = boost::interprocess;

struct Region
{
    Region(std::string regionIdStr, uint64_t size, bool remote)
        : fRemote(remote)
        , fRegionIdStr(regionIdStr)
        , fShmemObject()
    {
        if (fRemote)
        {
            fShmemObject = bipc::shared_memory_object(bipc::open_only, regionIdStr.c_str(), bipc::read_write);
        }
        else
        {
            fShmemObject = bipc::shared_memory_object(bipc::create_only, regionIdStr.c_str(), bipc::read_write);
            fShmemObject.truncate(size);
        }
        fRegion = bipc::mapped_region(fShmemObject, bipc::read_write); // TODO: add HUGEPAGES flag here
    }

    Region() = delete;

    Region(const Region&) = default;
    Region(Region&&) = default;

    ~Region()
    {
        if (!fRemote)
        {
            if (bipc::shared_memory_object::remove(fRegionIdStr.c_str()))
            {
                LOG(DEBUG) << "shmem: destroyed region " << fRegionIdStr;
            }
        }
        else
        {
            LOG(DEBUG) << "shmem: region '" << fRegionIdStr << "' is remote, no cleanup necessary.";
        }
    }

    bool fRemote;
    std::string fRegionIdStr;
    bipc::shared_memory_object fShmemObject;
    bipc::mapped_region fRegion;
};

class Manager
{
  public:
    static Manager& Instance()
    {
        static Manager man;
        return man;
    }

    void InitializeSegment(const std::string& op, const std::string& name, const size_t size = 0)
    {
        if (!fSegment)
        {
            try
            {
                if (op == "open_or_create")
                {
                    fSegment = new bipc::managed_shared_memory(bipc::open_or_create, name.c_str(), size);
                }
                else if (op == "create_only")
                {
                    fSegment = new bipc::managed_shared_memory(bipc::create_only, name.c_str(), size);
                }
                else if (op == "open_only")
                {
                    int numTries = 0;
                    bool success = false;

                    do
                    {
                        try
                        {
                            fSegment = new bipc::managed_shared_memory(bipc::open_only, name.c_str());
                            success = true;
                        }
                        catch (bipc::interprocess_exception& ie)
                        {
                            if (++numTries == 5)
                            {
                                LOG(ERROR) << "Could not open shared memory after " << numTries << " attempts, exiting!";
                                exit(EXIT_FAILURE);
                            }
                            else
                            {
                                LOG(DEBUG) << "Could not open shared memory segment on try " << numTries << ". Retrying in 1 second...";
                                LOG(DEBUG) << ie.what();

                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            }
                        }
                    }
                    while (!success);
                }
                else
                {
                    LOG(ERROR) << "Unknown operation when initializing shared memory segment: " << op;
                }
            }
            catch (std::exception& e)
            {
                LOG(ERROR) << "Exception during shared memory segment initialization: " << e.what() << ", application will now exit";
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            LOG(INFO) << "Segment already initialized";
        }
    }

    bipc::managed_shared_memory* Segment() const
    {
        if (fSegment)
        {
            return fSegment;
        }
        else
        {
            LOG(ERROR) << "Segment not initialized";
            exit(EXIT_FAILURE);
        }
    }

    bipc::mapped_region* CreateRegion(const size_t size, const uint64_t regionId)
    {
        auto it = fRegions.find(regionId);
        if (it != fRegions.end())
        {
            LOG(ERROR) << "shmem: Trying to create a region that already exists";
            return nullptr;
        }
        else
        {
            std::string regionIdStr = "fmq_shm_region_" + std::to_string(regionId);
            auto r = fRegions.emplace(regionId, Region{regionIdStr, size, false});

            LOG(DEBUG) << "shmem: created region: " << regionIdStr;

            return &(r.first->second.fRegion);
        }
    }

    bipc::mapped_region* GetRemoteRegion(const uint64_t regionId)
    {
        // remote region could actually be a local one if a message originates from this device (has been sent out and returned)
        auto it = fRegions.find(regionId);
        if (it != fRegions.end())
        {
            return &(it->second.fRegion);
        }
        else
        {
            std::string regionIdStr = "fmq_shm_region_" + std::to_string(regionId);

            auto r = fRegions.emplace(regionId, Region{regionIdStr, 0, true});

            LOG(DEBUG) << "shmem: located remote region: " << regionIdStr;

            return &(r.first->second.fRegion);
        }
    }

    void RemoveRegion(const uint64_t regionId)
    {
        fRegions.erase(regionId);
    }

    void Remove()
    {
        if (bipc::shared_memory_object::remove("fmq_shm_main"))
        {
            LOG(DEBUG) << "shmem: successfully removed \"fmq_shm_main\" segment after the device has stopped.";
        }
        else
        {
            LOG(DEBUG) << "shmem: did not remove \"fmq_shm_main\" segment after the device stopped. Already removed?";
        }

        if (bipc::shared_memory_object::remove("fmq_shm_management"))
        {
            LOG(DEBUG) << "shmem: successfully removed \"fmq_shm_management\" segment after the device has stopped.";
        }
        else
        {
            LOG(DEBUG) << "shmem: did not remove \"fmq_shm_management\" segment after the device stopped. Already removed?";
        }
    }

    bipc::managed_shared_memory& ManagementSegment()
    {
        return fManagementSegment;
    }

  private:
    Manager()
        : fSegment(nullptr)
        , fManagementSegment(bipc::open_or_create, "fmq_shm_management", 65536)
    {}
    Manager(const Manager&) = delete;
    Manager operator=(const Manager&) = delete;

    bipc::managed_shared_memory* fSegment;
    bipc::managed_shared_memory fManagementSegment;
    std::unordered_map<uint64_t, Region> fRegions;
};

} // namespace shmem
} // namespace mq
} // namespace fair

#endif /* FAIRMQSHMMANAGER_H_ */
