/*
 * FairMQBalancedStandaloneSplitter.cxx
 *
 *  Created on: Dec 6, 2012
 *      Author: dklein
 */

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "FairMQLogger.h"
#include "FairMQBalancedStandaloneSplitter.h"

FairMQBalancedStandaloneSplitter::FairMQBalancedStandaloneSplitter()
{
}

FairMQBalancedStandaloneSplitter::~FairMQBalancedStandaloneSplitter()
{
}

void FairMQBalancedStandaloneSplitter::Run()
{
  FairMQLogger::GetInstance()->Log(FairMQLogger::INFO, ">>>>>>> Run <<<<<<<");

  //boost::thread rateLogger(boost::bind(&FairMQDevice::LogSocketRates, this));
  boost::thread rateLogger(&FairMQDevice::LogSocketRates, this);
  // Initialize poll set
  zmq_pollitem_t items[] = {
    { *(fPayloadInputs->at(0)->GetSocket()), 0, ZMQ_POLLIN, 0 }
  };

  Bool_t received = false;
  Int_t direction = 0;

  while ( fState == RUNNING ) {
    FairMQMessage msg;

    zmq_poll(items, 1, 100);

    if (items[0].revents & ZMQ_POLLIN) {
      received = fPayloadInputs->at(0)->Receive(&msg);
    }

    if (received) {
      fPayloadOutputs->at(direction)->Send(&msg);
      direction++;
      if (direction >= fNumOutputs) {
        direction = 0;
      }
      received = false;
    }//if received
  }

  rateLogger.interrupt();
  rateLogger.join();
}
