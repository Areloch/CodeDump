//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "game/player.h"

PlayerData::PlayerData()
{
   //normal datablock definitions here

   skeletonBoneList = NULL;
}

bool PlayerData::preload(bool server, char errorBuffer[256])
{
   //normal stuff

   //IK-ready the skeleton before any animations
   mSkeleton = new skeleton();
   mSkeleton->bodyData = this; //get our datablock firstly
}

void PlayerData::initPersistFields()
{
   //normal stuff here

   addField( "skeletonBoneList", TypeFilename,     Offset( skeletonBoneList, PlayerData ) );
}

void PlayerData::packData(BitStream* stream)
{
  //normal stuff here

   stream->writeString(skeletonBoneList);

}

void PlayerData::unpackData(BitStream* stream)
{
   //normal stuff here

   skeletonBoneList = stream->readSTString();
}

bool Player::onNewDataBlock(GameBaseData* dptr)
{
   //normal stuff here

   	mDataBlock->mSkeleton->body = mShapeInstance; //set up our shapefile data to the skeleton
}

void Player::updateLookAnimation()
{
	//we're doing the arm IK stuff!!
   if(mDataBlock->mSkeleton->activeRegions.size() != 0)
   {
	   //exit if we're not really doing IK

	   //our two end points
	   Point3F leftHand, rightHand, null;
	   leftHand = rightHand = null = Point3F(0,0,0);
	   if(weaponSlot == -1)  //for now, bail if we have no weapon
		   return;
       MountedImage& image = mMountedImageList[weaponSlot];
	   MatrixF worldMat, rHand, lHand, rHTemp, lHTemp, objMat, temp;

	   //we have a point for the left arm to do IK with
	   for(S32 i=0; i<mDataBlock->mSkeleton->activeRegions.size(); i++){
		   if(mDataBlock->mSkeleton->activeRegions[i] == 1) //are we doing the left arm
		   {
			   //older
				MatrixF lHandNodeT = getMountedObjectNodeTransform("leftHand", weaponSlot);
				if (image.dataBlock) {
					ShapeBaseImageData& data = *image.dataBlock;

					 //get all valid positioning/rotation data and pull us into world space
					 getRenderMountTransform(data.mountPoint,&worldMat); //Returns mount point to world space transform

					 //move the rear sight node's transform into worldspace
					 temp.mul(worldMat, lHandNodeT);
					 lHTemp.mul(mWorldToObj, temp);
					 //take the world-space sight transform and apply it against the mount transform
					 //(get the relative distance in worldspace)
					 lHand.mul(lHTemp,data.mountTransform);

					 //origin point for proper rotation
					 lHand.mulP(null);

					 //do the left arm's IK 
					 mDataBlock->mSkeleton->CCDIK(1, lHand);
				}
				//older
		   }
		   if(mDataBlock->mSkeleton->activeRegions[i] == 2) //are we doing the right arm
		   {
			   //this was the main way of setting it now, the left hand method was the older one
				S32 rHand = mDataBlock->shape->findNode("rHandMount");
				MatrixF rHandNodeT = mShapeInstance->mNodeTransforms[rHand];
				
				mDataBlock->mSkeleton->CCDIK(2, rHandNodeT);
		   }
	   }
   }

   //normal stuff here
}

void Player::updateAnimationTree(bool firstPerson)
{
	S32 mode = 0;
   
	//replace old stuff here

	for (U32 i = 0; i < mDataBlock->mSkeleton->NumSpineNodes; i++)
      if (mDataBlock->spineNode[i] != -1)
         mShapeInstance->setNodeAnimationState(mDataBlock->spineNode[i],mode);
}

//C then A then B
//comes back as degrees
F32 Player::getFarAng(F32 c, F32 a, F32 b)
{
   F32 angRad = mAcos(((a*a+b*b-c*c)/(2*a*b)));
   return mRadToDeg(angRad); 
}

ConsoleMethod(Player, setIK, void, 4, 4, "Setting the arms and/or legs to use IK 1 = left arm, 2 = right arm 5 = left leg 6 = right leg")
{
	Con::printf("Called setIK! Region is %i and is set to %d", dAtoi(argv[2]), dAtob(argv[3]));
	object->setIK(dAtoi(argv[2]), dAtob(argv[3]));
}

void Player::setIK(S32 region, bool set)
{
	mDataBlock->mSkeleton->setIK(region, set);
}