/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * ShmChunk.h
 *
 * @since 2016-04-08
 * @author A. Rybalchenko
 */

#include <thread>
#include <chrono>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "FairMQLogger.h"

#ifndef SHMCHUNK_H_
#define SHMCHUNK_H_

namespace bipc = boost::interprocess;

class SegmentManager
{
  public:
    static SegmentManager& Instance()
    {
        static SegmentManager manager;
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
                    fSegment = new bipc::managed_shared_memory(bipc::open_or_create, name.c_str(), size + 100000000);
                }
                else if (op == "create_only")
                {
                    fSegment = new bipc::managed_shared_memory(bipc::create_only, name.c_str(), size + 100000000);

                    fBuffer = static_cast<char*>(fSegment->allocate(size));
                    fCapacity = size;
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
    }

    void* CreateChunk(const size_t size)
    {
        if (fSegment)
        {
            if (size == 0)
            {
                LOG(ERROR) << "Will not create chunk of size 0";
                exit(EXIT_FAILURE);
            }

            bipc::scoped_lock<bipc::named_mutex> lock(fMutex);

            size_t oldWriteIndex = fWriteIndex;

            if (size <= fCapacity - fWriteIndex)
            {
                fWriteIndex += size;
                fSize += size;

                if (fWriteIndex == fCapacity)
                {
                    fWriteIndex = 0;
                }
            }
            else
            {
                // if (fReadIndex - fWriteIndex >= size)
                // {
                    fWriteIndex = 0;
                    oldWriteIndex = fWriteIndex;
                    fWriteIndex += size;
                    fSize += size;
                // }
                // else
                // {
                    // return nullptr;
                // }
            }

            return fBuffer + oldWriteIndex;
        }
        else
        {
            LOG(ERROR) << "Segment not initialized";
            exit(EXIT_FAILURE);
        }
    }

    void DestroyChunk(const size_t size)
    {
        if (fSegment)
        {
            if (size == 0)
            {
                LOG(ERROR) << "Will not destroy chunk of size 0";
                exit(EXIT_FAILURE);
            }

            bipc::scoped_lock<bipc::named_mutex> lock(fMutex);

            if (size <= fCapacity - fReadIndex)
            {
                fReadIndex += size;
                fSize -= size;

                if (fReadIndex == fCapacity)
                {
                    fReadIndex = 0;
                }
            }
            else
            {
                fReadIndex = 0;
                fReadIndex += size;
                fSize -= size;
            }
        }
        else
        {
            LOG(ERROR) << "Segment not initialized";
            exit(EXIT_FAILURE);
        }
    }

    size_t Size() const { return fSize; }
    size_t Capacity() const { return fCapacity; }
    size_t Free() const { return fCapacity - fSize; }

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

  private:
    SegmentManager()
        : fSegment(nullptr)
        , fMutex(bipc::open_or_create, "FairMQNamedMutex")
        , fBuffer(nullptr)
        , fCapacity(0)
        , fSize(0)
        , fReadIndex(0)
        , fWriteIndex(0)
    {}

    bipc::managed_shared_memory* fSegment;
    bipc::named_mutex fMutex;

    char* fBuffer;
    size_t fCapacity;
    size_t fSize;
    size_t fReadIndex;
    size_t fWriteIndex;
};

class ShmChunk
{
  public:
    ShmChunk(const size_t size)
        : fHandle()
        , fSize(size)
        , fSending(true)
    {
        void* ptr = nullptr;
        // while (!ptr)
        // {
            ptr = SegmentManager::Instance().CreateChunk(size);
        // }
        fHandle = SegmentManager::Instance().Segment()->get_handle_from_address(ptr);
    }

    ShmChunk(bipc::managed_shared_memory::handle_t handle, const int i)
        : fHandle(handle)
        , fSize(0)
        , fSending(false)
    {
    }

    ~ShmChunk()
    {
        if (fSending)
        {
            SegmentManager::Instance().DestroyChunk(fSize);
        }
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
    bool fSending;
};

#endif /* SHMCHUNK_H_ */
