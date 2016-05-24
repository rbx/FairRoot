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

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>

#include <atomic>

namespace bipc = boost::interprocess;

namespace FairMQ
{

class ShmManager
{
  public:
    static ShmManager& Instance()
    {
        static ShmManager manager;
        return manager;
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

                                boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
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
            fInitialized = true;
        }
    }

    ShmShPtrType CreateMessage(const uint64_t& id, size_t size)
    {
        string chunkStr = "c" + to_string(id);
        string ownerStr = "o" + to_string(id);

        ShmShPtrOwner* owner = nullptr;
        ShmShPtrType localShPtr;

        while (!owner || !fInterrupted)
        {
            try
            {
                // create local shared pointer for the chunk in shared memory
                localShPtr = make_managed_shared_ptr(fSegment->construct<ShmChunk>(chunkStr.c_str())(size), *fSegment)

                // create shared pointer in shared memory
                owner = fSegment->construct<ShmShPtrOwner>(ownerStr.c_str())(localShPtr);
            }
            catch (bipc::bad_alloc& ba)
            {
                LOG(WARN) << "Not enough free memory for new message (" << fSegment->get_free_memory() << " < " << size << ")";

                if (fSegment->find<ShmChunk>(chunkStr.c_str()).first)
                {
                    fSegment->destroy<ShmChunk>(chunkStr.c_str());
                }

                if (fSegment->find<ShmShPtrOwner>(ownerStr.c_str()).first)
                {
                    fSegment->destroy<ShmShPtrOwner>(ownerStr.c_str());
                }

                unique_lock<mutex> lock(fAckMutex);
                fAckCV.wait_for(lock, chrono::milliseconds(500));
                continue;
            }

            {
                unique_lock<mutex> containerLock(fContainerMutex);
                fPtrs.insert(make_pair(id, std::move(owner)));
            }
            fPtrs.at(id)->fRcvCount += 1;
        }

        return localShPtr;
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

    bool Remove(const uint64_t& id)
    {
        fSegment->destroy_ptr(fPtrs.at(id));
        fPtrs.erase(id);
    }

    void Increment(const uint64_t& id)
    {
        fPtrs.at(id)->fRcvCount += 1;
    }

    void ListenForAcks()
    {
        message_queue ackQueue(open_only, "ack_queue");
        unsigned int priority;
        message_queue::size_type rcvSize;

        uint64_t id = 0;

        while (!fTerminated)
        {
            bpt::ptime rcvTill = bpt::microsec_clock::universal_time() + bpt::milliseconds(1000);

            if (ackQueue.timed_receive(&id, sizeof(id), rcvSize, priority, rcvTill))
            {
                // LOG(TRACE) << "Received ack for: " << id;
                {
                    unique_lock<mutex> containerLock(fContainerMutex);
                    if (fPtrs.find(id) != fPtrs.end())
                    {
                        fPtrs.at(id)->fRcvCount -= 1;
                        if (fPtrs.at(id)->fRcvCount == 0)
                        {
                            Remove(id);
                        }
                    }
                    else
                    {
                        LOG(WARN) << "Received ack for an id not contained in the container";
                    }
                    // LOG(TRACE) << fPtrs.size();
                }
                fAckCV.notify_all();
            }
            else
            {
                LOG(WARN) << "Did not receive anything within 1 second.";
            }
        }
        LOG(INFO) << "Ack listener finished.";
    }

    bool Interrupted()
    {
        return fInterrupted;
    }

    void Interrupt()
    {
        fInterrupted = true;
    }

    void Resume()
    {
        fInterrupted = false;
    }

  private:
    ShmManager()
        : fSegment(nullptr)
        , fPtrs()
        , fContainerMutex()
        , fAckMutex()
        , fAckCV()
        , fInterrupted(false)
        , fTerminated(false)
        , fInitialized(false)
        {}

    bipc::managed_shared_memory* fSegment;
    std::unordered_map<uint64_t, ShmShPtrOwner*> fPtrs;

    std::mutex fContainerMutex;
    std::mutex fAckMutex;
    std::condition_variable fAckCV;
    std::atomic<bool> fInterrupted;
    bool fTerminated;
    bool fInitialized;
};

typedef bipc::managed_shared_ptr<ShmChunk, bipc::managed_shared_memory>::type ShmShPtrType;

struct ShmShPtrOwner
{
    ShmShPtrOwner(const ShmShPtrType& otherPtr)
        : fSharedPtr(otherPtr)
        , fRcvCount(0)
        {}

    ShmShPtrOwner(const ShmShPtrOwner& otherOwner)
        : fSharedPtr(otherOwner.fSharedPtr)
        , fRcvCount(0)
        {}

    ShmShPtrType fSharedPtr;
    std::atomic<unsigned int> fRcvCount;
};

class ShmChunk
{
  public:
    ShmChunk(const size_t size)
        : fHandle()
        , fSize(size)
    {
        void* ptr = SegmentManager::Instance().Segment()->allocate(size);
        fHandle = SegmentManager::Instance().Segment()->get_handle_from_address(ptr);
        // LOG(TRACE) << "constructing chunk (" << fHandle << ")";
    }

    ~ShmChunk()
    {
        // LOG(TRACE) << "destroying chunk (" << fHandle << ")";
        SegmentManager::Instance().Segment()->deallocate(SegmentManager::Instance().Segment()->get_address_from_handle(fHandle));
    }

    bipc::managed_shared_memory::handle_t GetHandle() const
    {
        return fHandle;
    }

    void* GetData() const
    {
        return SegmentManager::Instance().Segment()->get_address_from_handle(fHandle);
    }

    size_t GetSize() const
    {
        return fSize;
    }

  private:
    bipc::managed_shared_memory::handle_t fHandle;
    size_t fSize;
};

} // namespace FairMQ

#endif /* FAIRMQSHMMANAGER_H_ */
