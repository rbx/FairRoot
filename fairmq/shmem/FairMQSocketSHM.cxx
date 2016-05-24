/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
/**
 * FairMQSocketSHM.cxx
 *
 * @since 2016-06-01
 * @author D. Klein, A. Rybalchenko
 */

#include <sstream>

#include "FairMQSocketSHM.h"
#include "FairMQMessageSHM.h"
#include "FairMQLogger.h"

using namespace std;
namespace bipc = boost::interprocess;

FairMQSocketSHM::FairMQSocketSHM(const string& type, const string& name, const int numIoThreads, const string& id /*= ""*/)
    : FairMQSocket(ZMQ_SNDMORE, ZMQ_RCVMORE, ZMQ_DONTWAIT)
    , fSocket(NULL)
    , fId()
    , fBytesTx(0)
    , fBytesRx(0)
    , fMessagesTx(0)
    , fMessagesRx(0)
{
    fId = id + "." + name + "." + type;

    fSocket = zmq_socket(fContext->GetContext(), GetConstant(type));

    if (fSocket == NULL)
    {
        LOG(ERROR) << "Failed creating socket " << fId << ", reason: " << zmq_strerror(errno);
        exit(EXIT_FAILURE);
    }

    LOG(DEBUG) << "created socket " << fId;
}

string FairMQSocketSHM::GetId()
{
    return fId;
}

bool FairMQSocketSHM::Bind(const string& address)
{
    string queueName = address.substr(address.rfind("/") + 1);
    LOG(INFO) << "creating queue \'" << queueName << "\' from " << address;

    try
    {
        bipc::message_queue::remove(queueName.c_str());
        fQueue = new bipc::message_queue(create_only, queueName.c_str(), 1000, sizeof(uint64_t));
    }
    catch (bipc::interprocess_exception& ie)
    {
        LOG(ERROR) << "Failed creating queue \'" << queueName << "\' from " << address << ", reason: " << ie.what();
        exit(EXIT_FAILURE);
    }

    return true;
}

void FairMQSocketSHM::Connect(const string& address)
{
    string queueName = address.substr(address.rfind("/") + 1);
    LOG(INFO) << "opening queue \'" << queueName << "\' from " << address;

    try
    {
        fQueue = new bipc::message_queue(open_only, queueName.c_str());
    }
    catch (bipc::interprocess_exception& ie)
    {
        LOG(ERROR) << "Failed opening queue \'" << queueName << "\' from " << address << ", reason: " << ie.what();
        exit(EXIT_FAILURE);
    }
}

int FairMQSocketSHM::Send(FairMQMessage* msg, const string& flag)
{
    return Send(msg, GetConstant(flag));
}

int FairMQSocketSHM::Send(FairMQMessage* msg, const int flags)
{
    try
    {
        size_t nbytes = msg->GetSize();
        uint64_t id = *(static_cast<uint64_t*>(msg->GetMessage()));

        do
        {
            bpt::ptime sndTill = bpt::microsec_clock::universal_time() + bpt::milliseconds(1000);

            if (fQueue->timed_send(&id, sizeof(id), 0, sndTill))
            {
                // LOG(TRACE) << "Sent meta.";
                fBytesTx += nbytes;
                ++fMessagesTx;
                return nbytes;
            }
            else
            {
                LOG(WARN) << "Did not send anything within 1 second.";
            }
        }
        while (!FairMQShmManager::Instance().Interrupted());

        LOG(DEBUG) << "Transfer interrupted";

        SegmentManager::Instance().Remove(id);

        return -2;
    }
    catch (bipc::interprocess_exception& ie)
    {
        LOG(ERROR) << "Failed sending on socket \'" << fId << "\', reason: " << ie.what();
        return -1;
    }
}

int64_t FairMQSocketSHM::Send(const vector<unique_ptr<FairMQMessage>>& msgVec, const int flags)
{
}

int FairMQSocketSHM::Receive(FairMQMessage* msg, const string& flag)
{
    return Receive(msg, GetConstant(flag));
}

int FairMQSocketSHM::Receive(FairMQMessage* msg, const int flags)
{
    try
    {
        size_t nbytes = 0;
        uint64_t id = 0;
        message_queue::size_type rcvSize;
        unsigned int priority;

        do
        {
            bpt::ptime rcvTill = bpt::microsec_clock::universal_time() + bpt::milliseconds(1000);

            if (fQueue->timed_receive(&id, sizeof(id), rcvSize, priority, rcvTill))
            {
                fBytesRx += nbytes;

                string ownerStr("o" + to_string(id));
                // LOG(TRACE) << "Received message: " << ownerStr;

                // find the shared pointer in shared memory with its ID
                SharedPtrOwner* owner = SegmentManager::Instance().Segment()->find<SharedPtrOwner>(ownerStr.c_str()).first;
                // LOG(TRACE) << "owner use count: " << owner->fSharedPtr.use_count();
                // create a local shared pointer from the received one (increments the reference count)
                msg->SetMessage();
                SharedPtrType localPtr = owner->fSharedPtr;

                ++fMessagesRx;
                return nbytes;
            }




            bpt::ptime sndTill = bpt::microsec_clock::universal_time() + bpt::milliseconds(1000);

            if (fQueue->timed_send(&id, sizeof(id), 0, sndTill))
            {
                // LOG(TRACE) << "Sent meta.";
                fBytesTx += nbytes;
                ++fMessagesTx;
                return nbytes;
            }
            else
            {
                LOG(WARN) << "Did not send anything within 1 second.";
            }
        }
        while (!FairMQShmManager::Instance().Interrupted());

        LOG(DEBUG) << "Transfer interrupted";

        SegmentManager::Instance().Remove(id);

        return -2;
    }
    catch (bipc::interprocess_exception& ie)
    {
        LOG(ERROR) << "Failed sending on socket \'" << fId << "\', reason: " << ie.what();
        return -1;
    }
}

int64_t FairMQSocketSHM::Receive(vector<unique_ptr<FairMQMessage>>& msgVec, const int flags)
{
}

void FairMQSocketSHM::Close()
{
    // LOG(DEBUG) << "Closing socket " << fId;

    if (fSocket == NULL)
    {
        return;
    }

    if (zmq_close(fSocket) != 0)
    {
        LOG(ERROR) << "Failed closing socket " << fId << ", reason: " << zmq_strerror(errno);
    }

    fSocket = NULL;
}

void FairMQSocketSHM::Terminate()
{
    if (zmq_ctx_destroy(fContext->GetContext()) != 0)
    {
        LOG(ERROR) << "Failed terminating context, reason: " << zmq_strerror(errno);
    }
}

void* FairMQSocketSHM::GetSocket() const
{
    return fSocket;
}

int FairMQSocketSHM::GetSocket(int) const
{
    // dummy method to comply with the interface. functionality not possible in zeromq.
    return -1;
}

void FairMQSocketSHM::SetOption(const string& option, const void* value, size_t valueSize)
{
    if (zmq_setsockopt(fSocket, GetConstant(option), value, valueSize) < 0)
    {
        LOG(ERROR) << "Failed setting socket option, reason: " << zmq_strerror(errno);
    }
}

void FairMQSocketSHM::GetOption(const string& option, void* value, size_t* valueSize)
{
    if (zmq_getsockopt(fSocket, GetConstant(option), value, valueSize) < 0)
    {
        LOG(ERROR) << "Failed getting socket option, reason: " << zmq_strerror(errno);
    }
}

unsigned long FairMQSocketSHM::GetBytesTx() const
{
    return fBytesTx;
}

unsigned long FairMQSocketSHM::GetBytesRx() const
{
    return fBytesRx;
}

unsigned long FairMQSocketSHM::GetMessagesTx() const
{
    return fMessagesTx;
}

unsigned long FairMQSocketSHM::GetMessagesRx() const
{
    return fMessagesRx;
}

void FairMQSocketSHM::SetDeviceId(const std::string& id)
{
    fDeviceId = id;
}

void FairMQSocketSHM::Interrupt()
{
    FairMQShmManager::Instance().Interrupt()
}

void FairMQSocketSHM::Resume()
{
    FairMQShmManager::Instance().Resume()
}

bool FairMQSocketSHM::SetSendTimeout(const int timeout, const string& address, const string& method)
{
    if (method == "bind")
    {
        if (zmq_unbind(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed unbinding socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_setsockopt(fSocket, ZMQ_SNDTIMEO, &timeout, sizeof(int)) != 0)
        {
            LOG(ERROR) << "Failed setting option on socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_bind(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed binding socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
    }
    else if (method == "connect")
    {
        if (zmq_disconnect(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed disconnecting socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_setsockopt(fSocket, ZMQ_SNDTIMEO, &timeout, sizeof(int)) != 0)
        {
            LOG(ERROR) << "Failed setting option on socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_connect(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed connecting socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
    }
    else
    {
        LOG(ERROR) << "SetSendTimeout() failed - unknown method provided!";
        return false;
    }

    return true;
}

int FairMQSocketSHM::GetSendTimeout() const
{
    int timeout = -1;
    size_t size = sizeof(timeout);

    if (zmq_getsockopt(fSocket, ZMQ_SNDTIMEO, &timeout, &size) != 0)
    {
        LOG(ERROR) << "Failed getting option 'receive timeout' on socket " << fId << ", reason: " << zmq_strerror(errno);
    }

    return timeout;
}

bool FairMQSocketSHM::SetReceiveTimeout(const int timeout, const string& address, const string& method)
{
    if (method == "bind")
    {
        if (zmq_unbind(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed unbinding socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_setsockopt(fSocket, ZMQ_RCVTIMEO, &timeout, sizeof(int)) != 0)
        {
            LOG(ERROR) << "Failed setting option on socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_bind(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed binding socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
    }
    else if (method == "connect")
    {
        if (zmq_disconnect(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed disconnecting socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_setsockopt(fSocket, ZMQ_RCVTIMEO, &timeout, sizeof(int)) != 0)
        {
            LOG(ERROR) << "Failed setting option on socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
        if (zmq_connect(fSocket, address.c_str()) != 0)
        {
            LOG(ERROR) << "Failed connecting socket " << fId << ", reason: " << zmq_strerror(errno);
            return false;
        }
    }
    else
    {
        LOG(ERROR) << "SetReceiveTimeout() failed - unknown method provided!";
        return false;
    }

    return true;
}

int FairMQSocketSHM::GetReceiveTimeout() const
{
    int timeout = -1;
    size_t size = sizeof(timeout);

    if (zmq_getsockopt(fSocket, ZMQ_RCVTIMEO, &timeout, &size) != 0)
    {
        LOG(ERROR) << "Failed getting option 'receive timeout' on socket " << fId << ", reason: " << zmq_strerror(errno);
    }

    return timeout;
}

int FairMQSocketSHM::GetConstant(const string& constant)
{
    if (constant == "")
        return 0;
    if (constant == "sub")
        return ZMQ_SUB;
    if (constant == "pub")
        return ZMQ_PUB;
    if (constant == "xsub")
        return ZMQ_XSUB;
    if (constant == "xpub")
        return ZMQ_XPUB;
    if (constant == "push")
        return ZMQ_PUSH;
    if (constant == "pull")
        return ZMQ_PULL;
    if (constant == "req")
        return ZMQ_REQ;
    if (constant == "rep")
        return ZMQ_REP;
    if (constant == "dealer")
        return ZMQ_DEALER;
    if (constant == "router")
        return ZMQ_ROUTER;
    if (constant == "pair")
        return ZMQ_PAIR;

    if (constant == "snd-hwm")
        return ZMQ_SNDHWM;
    if (constant == "rcv-hwm")
        return ZMQ_RCVHWM;
    if (constant == "snd-size")
        return ZMQ_SNDBUF;
    if (constant == "rcv-size")
        return ZMQ_RCVBUF;
    if (constant == "snd-more")
        return ZMQ_SNDMORE;
    if (constant == "rcv-more")
        return ZMQ_RCVMORE;

    if (constant == "linger")
        return ZMQ_LINGER;
    if (constant == "no-block")
        return ZMQ_DONTWAIT;
    if (constant == "snd-more no-block")
        return ZMQ_DONTWAIT|ZMQ_SNDMORE;

    return -1;
}

FairMQSocketSHM::~FairMQSocketSHM()
{
}
