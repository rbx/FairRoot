/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

 #include "Manager.h"
 #include "Common.h"

namespace fair
{
namespace mq
{
namespace shmem
{

using namespace std;
namespace bipc = boost::interprocess;

Manager::Manager(const string& name, size_t size)
    : fName(name)
    , fSegment(bipc::open_or_create, fName.c_str(), size)
    , fManagementSegment(bipc::open_or_create, "fmq_shm_management", 65536)
    , fRegions()
    , fRegionCallbacks()
    , fAckMtx()
    , fRegionControlMtx()
    , fRegionControlCV()
{}

bipc::managed_shared_memory& Manager::Segment()
{
    return fSegment;
}

void Manager::Interrupt()
{
    Region::fRunning = false;
}

void Manager::Resume()
{
    Region::fRunning = true;
    fRegionControlCV.notify_all();
}

bipc::mapped_region* Manager::CreateRegion(const size_t size, const uint64_t id)
{
    auto it = fRegions.find(id);
    if (it != fRegions.end())
    {
        LOG(ERROR) << "shmem: Trying to create a region that already exists";
        return nullptr;
    }
    else
    {
        auto r = fRegions.emplace(id, Region{*this, id, size, false});

        r.first->second.StartReceivingAcks();

        return &(r.first->second.fRegion);
    }
}

bipc::mapped_region* Manager::GetRemoteRegion(const uint64_t id)
{
    // remote region could actually be a local one if a message originates from this device (has been sent out and returned)
    auto it = fRegions.find(id);
    if (it != fRegions.end())
    {
        return &(it->second.fRegion);
    }
    else
    {
        auto r = fRegions.emplace(id, Region{*this, id, 0, true});

        return &(r.first->second.fRegion);
    }
}

void Manager::RemoveRegion(const uint64_t id)
{
    fRegions.erase(id);
}

bipc::message_queue& Manager::GetRegionQueue(const uint64_t id)
{
    return *(fRegions.at(id).fQueue);
}

void Manager::SetRegionCallback(uint64_t id, FairMQRegionCallback callback)
{
    lock_guard<mutex> lock(fAckMtx);
    fRegionCallbacks.emplace(id, callback);
}

void Manager::RemoveSegment()
{
    if (bipc::shared_memory_object::remove(fName.c_str()))
    {
        LOG(DEBUG) << "shmem: successfully removed " << fName << " segment after the device has stopped.";
    }
    else
    {
        LOG(DEBUG) << "shmem: did not remove " << fName << " segment after the device stopped. Already removed?";
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

bipc::managed_shared_memory& Manager::ManagementSegment()
{
    return fManagementSegment;
}

} // namespace shmem
} // namespace mq
} // namespace fair
