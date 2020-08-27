/*
 * FairEveRecoTrack.h
 *
 *  Created on: 23 kwi 2020
 *      Author: Daniel Wielanek
 *		E-mail: daniel.wielanek@gmail.com
 *		Warsaw University of Technology, Faculty of Physics
 */
#ifndef FAIREVERECOTRACK_H_
#define FAIREVERECOTRACK_H_

#include <TEvePointSet.h>
#include <TEveTrackPropagator.h>
#include <TEveCompound.h>
#include <TGeoTrack.h>
#include "FairEveTrack.h"

class FairEveRecoTrack : public TEveCompound{
    TEvePointSet *fHits;
    Int_t fNPoints;
    FairEveTrack *fRecoTrack;
    FairEveTrack *fMCTrack;
public:
    FairEveRecoTrack();
    FairEveRecoTrack(TParticle *t, Int_t label,TEveTrackPropagator *prop=nullptr);
    FairEveTrack *GetRecoTrack()const{return fRecoTrack;};
    FairEveTrack *GetMCTrack()const{return fMCTrack;}
    TEvePointSet *GetHits()const{return fHits;};
    void AddHit(TVector3 hit);
    /**
     * make and draw MC track
     * @param geo
     */
    void MakeMCTrack(const TGeoTrack *geo);
    /** initialize container for MC track, user must manualy add points */
    void InitMCTrack(const TParticle *p);
    void SetMainColor(Color_t color);
    virtual ~FairEveRecoTrack();
    ClassDef(FairEveRecoTrack,0);
};

#endif /* FAIREVERECOTRACK_H_ */