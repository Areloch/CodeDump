//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

//normal stuff here
#include "skeleton.h"

//normal stuff here
class skeleton;

//----------------------------------------------------------------------------

struct PlayerData: public ShapeBaseData {
   typedef ShapeBaseData Parent;
   //normal stuff here

   skeleton *mSkeleton; //we have a skeleton! joyus dayyyyyyy
   StringTableEntry     skeletonBoneList; //text file containing all our bones and config data :o

  //normal stuff here
};


//----------------------------------------------------------------------------

class Player: public ShapeBase
{
   //normal stuff here
   F32 getFarAng(F32 c, F32 a, F32 b); //move to math util later
   void setIK(S32 region, bool set);
};

#endif
