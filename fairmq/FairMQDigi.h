/*
 * FairMQDigi.h
 *
 *  Created on: 20.07.2012
 *      Author: stockman
 */

#ifndef FAIRMQDIGI_H_
#define FAIRMQDIGI_H_

#include "FairTimeStamp.h"              // for FairTimeStamp

#include "Riosfwd.h"                    // for ostream
#include "Rtypes.h"                     // for Int_t, etc

#include <iostream>                     // for operator<<, basic_ostream, etc

class FairMQDigi : public FairTimeStamp
{
  public:
    FairMQDigi();
    FairMQDigi(Int_t x, Int_t y, Int_t z, Double_t timeStamp);
    virtual ~FairMQDigi();

    void SetXYZ(Int_t x, Int_t y, Int_t z) { SetX(x); SetY(y); SetZ(z);}
    void SetX(Int_t x)  { fX=x; }
    void SetY(Int_t y)  { fY=y; }
    void SetZ(Int_t z)  { fZ=z; }

    Int_t GetX() const  {return fX;}
    Int_t GetY() const  {return fY;}
    Int_t GetZ() const  {return fZ;}

    virtual bool equal(FairTimeStamp* data) {
      FairMQDigi* myDigi = dynamic_cast <FairMQDigi*> (data);
      if (myDigi != 0) {
        if (fX == myDigi->GetX())
          if (fY == myDigi->GetY())
            if (fZ == myDigi->GetZ()) {
              return true;
            }
      }
      return false;
    }

    virtual bool operator<(const FairMQDigi& myDigi) const {
      if (fX < myDigi.GetX()) { return true; }
      else if (fX > myDigi.GetX()) { return false; }
      if (fY < myDigi.GetY()) { return true; }
      else if (fY > myDigi.GetY()) { return false; }
      if (fZ < myDigi.GetZ()) { return true; }
      else if (fZ > myDigi.GetZ()) { return false; }
      return false;
    }

    friend std::ostream& operator<<(std::ostream& out, FairMQDigi& digi) {
      out << "FairMQDigi at:"
          << " (" << digi.GetX() << "/" << digi.GetY() << "/" << digi.GetZ() << ") "
          << " with TimeStamp: " << digi.GetTimeStamp()
          << std::endl;
      return out;
    }

  private:
    Int_t fX;
    Int_t fY;
    Int_t fZ;


    ClassDef(FairMQDigi,1);
};

#endif /* FAIRMQDIGI_H */
