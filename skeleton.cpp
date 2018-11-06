#include "T3D/skeleton.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "ts/tsShapeInstance.h"
#include "math/mTransform.h"
#include "math/mathUtils.h"


IMPLEMENT_CO_DATABLOCK_V1(Bone);
IMPLEMENT_CO_DATABLOCK_V1(JiggleBone);
IMPLEMENT_CO_DATABLOCK_V1(IKChain);
IMPLEMENT_CO_DATABLOCK_V1(JiggleChain);
IMPLEMENT_CO_DATABLOCK_V1(IKRule);

//=================================================================

bool Bone::onAdd()
{
	if(!Parent::onAdd())
      return false;

	ShapeBaseData *db = ((ShapeBaseData*)mTarget);
	db->mSkeleton->addBone(this);

	return true;
}

void Bone::initPersistFields(){
	Parent::initPersistFields();
}

//=================================================================
bool JiggleBone::onAdd()
{
	if(!Parent::onAdd())
      return false;

	mTarget->mSkeleton->addJiggleBone(this);

	return true;
}

void JiggleBone::initPersistFields(){
	Parent::initPersistFields();
}

void tempJBone::setFromBone(Bone *bone)
{
	boneNode = bone->boneNode;
    children = bone->children;	//storing the nodeID is cleaner and more effecient 
								//than a full Bone ref, usefull for updating non-bone children(like hands at the end of an arm IK)
    parent = bone->parent;
    boneVec = bone->boneVec;	//vectors to our child from this bone
    length = bone->length;		//length of our origin to our child
    boneName = bone->boneName;
    bounds = bone->bounds;

    mTarget = bone->mTarget;	//the object we're trying to set a Bone to

    mDof[0] = bone->mDof[0];	//a min/max for each axis
	mDof[1] = bone->mDof[1];	//a min/max for each axis
}
//=================================================================
bool IKChain::onAdd()
{
	if(!Parent::onAdd())
      return false;

	ShapeBaseData* test = mTarget;

	if(mTarget)
	{
		ShapeBaseData *db = ((ShapeBaseData*)mTarget);
		db->mSkeleton->addIKChain(this);
	}

	return true;
}

void IKChain::initPersistFields()
{
	Parent::initPersistFields();

	addField("mTarget", TYPEID< ShapeBaseData >(), Offset(mTarget, IKChain));
	addField("rootBoneName", TypeString, Offset(rootBoneName, IKChain));
	addField("endBoneName", TypeString, Offset(endBoneName, IKChain));
	addField("tolerance", TypeF32, Offset(tolerance, IKChain));
	addField("dontReach", TypeBool, Offset(dontReach, IKChain));
}

//=================================================================

bool JiggleChain::onAdd()
{
	if(!Parent::onAdd())
      return false;

	mTarget->mSkeleton->addJiggleChain(this);

	return true;
}

void JiggleChain::initPersistFields(){
	Parent::initPersistFields();
}
//=================================================================
// chain IK(arms)
// leg IK(linear to ground)
// look at
bool IKRule::onAdd()
{
	if(!Parent::onAdd())
      return false;

	ShapeBaseData* test = mTarget;

	if(mTarget)
	{
		ShapeBaseData *db = ((ShapeBaseData*)mTarget);
		for(U32 i=0; i< db->mSkeleton->ikChains.size(); i++)
			if(db->mSkeleton->ikChains[i] == targetChain)
			{
				db->mSkeleton->addIKRule(this);
			}
	}

	return true;
}

void IKRule::initPersistFields()
{
	Parent::initPersistFields();

	addField("mTarget", TYPEID< ShapeBaseData >(), Offset(mTarget, IKRule));
	addField("TargetChain", TYPEID< IKChain >(), Offset(targetChain, IKRule));
	addField("animationName", TypeString, Offset(animationName, IKRule));
	addField("goalNodeName", TypeString, Offset(goalNodeName, IKRule));
	addField("blendAmount", TypeF32, Offset(blendAmount, IKRule));
	addField("offset", TypeF32, Offset(offset, IKRule));
	addField("triggerID", TypeS32, Offset(triggerID, IKRule));	
	addField("matchOrientToGoal", TypeBool, Offset(matchOrientToGoal, IKRule));		
}
//=================================================================

Skeleton::Skeleton()
{
}

Skeleton::~Skeleton()
{
}

void Skeleton::CCDIK(TSShapeInstance* shapeInstance, IKChain *ikchain, MatrixF endTrans)
{
	Point3F		rootPos, curEnd, desiredEnd, endPos = endTrans.getPosition();
	VectorF     targetVector, curVector, crossResult;
	double		cosAngle,turnAngle, distCheck;
	S32 tries, link;

	// start at the last link in the chain
	link = ikchain->chain.size();
	link -= 1;
	tries = 0;

	F32 len = 0; F32 dist = 0;
	for(S32 i=0; i < ikchain->chain.size(); i++) {
		len += ikchain->chain[i]->length;		//we can presume(for now) that we're only gunna have linear bone chains
		//only use bones we need to?
		if(len >= dist){
			link = i;
			continue;
		}
	}

	Vector<VectorF> pastCurVectors;
	Vector<VectorF> pastTargetVectors;

	do
	{
		MatrixF boneTrans = *getBoneTrans(shapeInstance, ikchain->chain[link]);

		rootPos = boneTrans.getPosition();

		//MatrixF rootLocal = shapeInstance->smNodeLocalTransforms[ikchain->chain[link]->boneNode];

		//==================================================================
		//End bone position
		curEnd = getBoneEndPoint(shapeInstance, ikchain->chain[ikchain->chain.size()-1]);
		//End bone position
		//==================================================================

		desiredEnd = endPos;
		VectorF endVec = desiredEnd - curEnd;
		double distance = endVec.len();

		// see if i'm already close enough
		if (distance > ikchain->tolerance)
		{
			// create the vector to the current effector pos
			curVector = curEnd - rootPos;
			// create the desired effector position vector
			targetVector = endPos - rootPos;

			curVector.normalize();
			targetVector.normalize();

			cosAngle = mDot(curVector, targetVector);

			F32 ang = mRadToDeg(cosAngle);

			if (cosAngle < 0.9999999)
			{
				/*if(cosAngle < -0.9999999)
				{
					//the 2 vectors are collinear
					
					// check if we can use cross product of source vector and [1, 0, 0]
					crossResult.set(0, curVector.x, -curVector.y);

					if( crossResult.magnitudeSafe() < 1.0e-10 )
					{
						// nope! we need cross product of source vector and [0, 1, 0]
						crossResult.set( -curVector.z, 0, curVector.x ) ;
					}
				}
				else
				{
					// use the cross product to check which way to rotate
					crossResult = mCross(curVector, targetVector);
				}

				crossResult.normalize();
				turnAngle = mAcos(cosAngle);	// get the angle

				if(turnAngle > M_PI / 12) //max angle change
				{
					turnAngle = M_PI / 12;
				}*/

				QuatF rot, tester = QuatF(boneTrans);
				rot.shortestArc(curVector, targetVector);

				MatrixF oldBoneTrans = boneTrans;

				TSTransform::setMatrix(rot, boneTrans.getPosition(), &boneTrans);

				//check dof's
				MatrixF newBoneTrans = CheckDofsRestrictions(ikchain->chain[link], boneTrans);

				//storeBoneTrans(shapeInstance, ikchain->chain[link]->boneNode, newBoneTrans);
				shapeInstance->mNodeTransforms[ikchain->chain[link]->boneNode] = newBoneTrans;
				
				updateChildren(shapeInstance, ikchain->chain[link], ikchain);
			}
			if (--link < 0) 
				link = ikchain->chain.size()-1;	// START OF THE CHAIN, RESTART
		}

		// quit if i am close enough or been running long enough
	} while (++tries < 60 && 
		//curEnd.SquaredDistance(desiredEnd) > ikchain->tolerance);
		VectorF(desiredEnd - getBoneEndPoint(shapeInstance, ikchain->chain[ikchain->chain.size()-1])).len() > ikchain->tolerance);
}

void Skeleton::physicalIK(TSShapeInstance* shapeInstance, IKChain *ikchain, MatrixF endTrans, F32 dt)
{
	//make a temp copy of the ikchain as a jiggle chian
	Vector<tempJBone*> physicalBones;
	Vector<mass*>	   masses;
	mass *m;

	for(U32 i=0; i<ikchain->chain.size(); i++)
	{
		tempJBone* jb = new tempJBone();
		jb->setFromBone(ikchain->chain[i]);
		//we don't need gravity simulation on the physical IK
		jb->specificGravity = 0.f;
		physicalBones.push_front(jb);

		m = new mass();
		m->boneNum = physicalBones[i]->boneNode;
	
		masses.push_front(m);
	}

	//now that we have an impromptu jChain, simulate the end bone towards our goal point
	S32 count = physicalBones.size()-1;
	S32 tries = 0;

	VectorF dir;
	
	do{
		storeBoneTrans(shapeInstance, ikchain->chain[count]->boneNode, endTrans);

		//solve the 'springs' first
		for(U32 x=0; x<count; x++) {
			//solveJiggleBone(shapeInstance, physicalBones[x], masses[x]);
			Point3F rootPos = getBoneTrans(shapeInstance, findBone(physicalBones[x]->parent))->getPosition();
			Point3F endPos = getBoneTrans(shapeInstance,physicalBones[x])->getPosition();

			VectorF springVector = rootPos - endPos;							//vector between the two masses
			VectorF force = VectorF(0,0,0);														//force initially has a zero value
			
			//solve the 'spring' between the two bones
			F32 vecLen = springVector.len();											//distance between the two masses
			
			if (vecLen != 0)																	//to avoid a division by zero check if r is zero
				force += (springVector / vecLen) * (vecLen - physicalBones[x]->length);// * (-physicalBones[x]->stiffness);	//the spring force is added to the force

			VectorF vel = physicalBones[x]->velocity * physicalBones[x]->moveFriction;						//The air friction
			if(x<count-1)
				force += -(vel - physicalBones[x+1]->velocity) * physicalBones[x]->moveFriction.z;						//the friction force is added to the force
			else 
				force += -(vel) * physicalBones[x]->moveFriction.z;						    //with this addition we obtain the net force of the spring

			VectorF b = (VectorF(0,0,-9.81f) * physicalBones[x]->specificGravity) * physicalBones[x]->mass;
			force += b;//(VectorF(0,0,-9.81f) * jB->specificGravity) * jB->mass;			//The gravitational force

			masses[x]->force += force;															//net forces
		}
			
		//then simulate/update the bones
		for(U32 y=0; y<count; y++){
			updateJiggleBone(shapeInstance, physicalBones[y], masses[y], dt);
			updateChildren(shapeInstance, physicalBones[y], ikchain);
		}

		MatrixF boneTrans = *getBoneTrans(shapeInstance, ikchain->chain[count]);
		dir = endTrans.getPosition() - boneTrans.getPosition();
	}
	while(++tries < 3 && dir.len() > ikchain->tolerance);
}
void Skeleton::analiticalIK(TSShapeInstance* shapeInstance, IKChain *ikchain, MatrixF endTrans)
{

/// Local Variables ///////////////////////////////////////////////////////////
	/*float l1,l2;		// BONE LENGTH FOR BONE 1 AND 2
	float ex,ey;		// ADJUSTED TARGET POSITION
	float sin2,cos2;	// SINE AND COSINE OF ANGLE 2
	float angle1,angle2;// ANGLE 1 AND 2 IN RADIANS
	float tan1;			// TAN OF ANGLE 1
///////////////////////////////////////////////////////////////////////////////

	// SUBTRACT THE INITIAL OFFSET FROM THE TARGET POS
	ex = endPos.x - (m_UpArm.trans.x * m_ModelScale);
	ey = endPos.y - (m_UpArm.trans.y * m_ModelScale);

	// MULTIPLY THE BONE LENGTHS BY THE WINDOW SCALE
	l1 = m_LowArm.trans.x * m_ModelScale;
	l2 = m_Effector.trans.x * m_ModelScale;
	
	// CALCULATE THE COSINE OF ANGLE 2
	cos2 = ((ex * ex) + (ey * ey) - (l1 * l1) - (l2 * l2)) / (2 * l1 * l2);

	// IF IT IS NOT IN THIS RANGE, IT IS UNREACHABLE
	if (cos2 >= -1.0 && cos2 <= 1.0)
	{
		angle2 = (float)acos(cos2);	// GET THE ANGLE WITH AN ARCCOSINE
		m_LowArm.rot.z = RADTODEG(angle2);	// CONVERT IT TO DEGREES

		sin2 = (float)sin(angle2);		// CALC THE SINE OF ANGLE 2

		// COMPUTE ANGLE 1
		// HERE IS WHERE THE BUG WAS SEE THE README.TXT FOR MORE INFO
		// CALCULATE THE TAN OF ANGLE 1
		tan1 = (-(l2 * sin2 * ex) + ((l1 + (l2 * cos2)) * ey)) / 
				  ((l2 * sin2 * ey) + ((l1 + (l2 * cos2)) * ex));
		// GET THE ACTUAL ANGLE
		angle1 = atan(tan1);
		m_UpArm.rot.z = RADTODEG(angle1);	// CONVERT IT TO DEGREES
		return TRUE;
	}
	else
		return FALSE;*/
}
MatrixF Skeleton::CheckDofsRestrictions(Bone *bone, MatrixF mat)
{
	EulerF angles = mat.toEuler();

	bool modified = false;

	F32 xMin = mDegToRad(bone->mDof[0].x);
	F32 xMax = mDegToRad(bone->mDof[1].x);
	if(angles.x < xMin)
	{
		angles.x = xMin;
		modified = true;
	}
	else
		if(angles.x > xMax)
		{
			angles.x = xMax;
			modified = true;
		}

	F32 yMin = mDegToRad(bone->mDof[0].y);
	F32 yMax = mDegToRad(bone->mDof[1].y);
	if(angles.y < yMin)
	{
		angles.y = yMin;
		modified = true;
	}
	else
		if(angles.y > yMax)
		{
			angles.y = yMax;
			modified = true;
		}

	F32 zMin = mDegToRad(bone->mDof[0].z);
	F32 zMax = mDegToRad(bone->mDof[1].z);
	if(angles.z < zMin)
	{
		angles.z = zMin;
		modified = true;
	}
	else
		if(angles.z > zMax)
		{
			angles.z = zMax;
			modified = true;
		}

	if(modified)
	{
		QuatF q = QuatF(angles);
		MatrixF moddedXfm;

		TSTransform::setMatrix(q, mat.getPosition(), &moddedXfm);
		return moddedXfm;
	}

	return mat;
}
//helper function to the main IK, takes a direction and the given bone, and we compile out a transform for the destired rotation
MatrixF Skeleton::directionToMatrix(VectorF dir, VectorF up)
{
	MatrixF matr;
	AngAxisF aAng;

	VectorF MatX = mCross(dir, up);	// the x-column of our 3x3 matrix. perpendicular to both %vDirection and %vUp. 
	matr.setColumn(0, MatX);
	matr.setColumn(1, dir);						// the y-column of our 3x3 matrix. we're just saying "be what i want". 
	matr.setColumn(2, mCross(MatX, dir));		// the z-column of our 3x3 matrix. perpendicular to X and Y.
	matr.setPosition(Point3F(0,0,0));

	F32 * mat = matr;

	F32 theta = mAcos((mat[0,0] + mat[1,1] + mat[2,2] - 1) / 2);
	F32 eps = 0.0001;

	if(mAbs(theta) < eps){
		// singularity at zero  
		//return "0 0 0 0 0 1 0";
	}
	else if ( mAbs(3.14159265359 - theta) < eps)
	{
		// ditto at 180, but i leave it to you to grok the whole largest-diagonal thing at that site.  
		//return "0 0 0 0 0 1 0";  
    }

	F32 denom = mSqrt((mat[2,1] - mat[1,2]) * (mat[2,1] - mat[1,2]) +
					  (mat[0,2] - mat[2,0]) * (mat[0,2] - mat[2,0]) +
					  (mat[1,0] - mat[0,1]) * (mat[1,0] - mat[0,1]));

	aAng.axis.x = (mat[2,1] - mat[1,2]) / denom;
	aAng.axis.y = (mat[0,2] - mat[2,0]) / denom;
	aAng.axis.z = (mat[1,0] - mat[0,1]) / denom;
	aAng.angle = theta;

	TransformF mats(Point3F(0,0,0), aAng);

	return mats.getMatrix();
}

void Skeleton::updateChildren(TSShapeInstance* shapeInstance, Bone *current, IKChain *chain)
{
	F32 boneCount = chain->chain.size() - 1;
	//first update our root
	MatrixF parentTrans, curTrans, newTrans;

	for(U32 i = 1; i < boneCount - 1; i++)
	{
		if(i==0)
			 parentTrans = shapeInstance->mNodeTransforms[current->boneNode];
		else
			 parentTrans = getLocalBoneTrans(shapeInstance, chain->chain[i-1]);

		curTrans = getLocalBoneTrans(shapeInstance, chain->chain[i]); //our current

		newTrans.mul(parentTrans, curTrans);

		storeBoneTrans(shapeInstance, chain->chain[i]->boneNode, newTrans);
	}
}

MatrixF Skeleton::setForwardVector(MatrixF *mat, VectorF axisY, VectorF up)
{
   VectorF axisX;  
   VectorF axisZ;  
  
   // Validate and normalize input:  
   F32 lenSq;  
   lenSq = axisY.lenSquared();  
   if (lenSq < 0.000001f)  
   {  
      axisY.set(0.0f, 1.0f, 0.0f);  
      Con::errorf("SceneObject::setForwardVector() - degenerate forward vector");  
   }  
   else  
   {  
      axisY /= mSqrt(lenSq);  
   }  
  
  
   lenSq = up.lenSquared();  
   if (lenSq < 0.000001f)  
   {  
      up.set(0.0f, 0.0f, 1.0f);  
      Con::errorf("SceneObject::setForwardVector() - degenerate up vector - too small");  
   }  
   else  
   {  
      up /= mSqrt(lenSq);  
   }  
  
   if (fabsf(mDot(up, axisY)) > 0.9999f)  
   {  
      Con::errorf("SceneObject::setForwardVector() - degenerate up vector - same as forward");  
      // i haven't really tested this, but i think it generates something which should be not parallel to the previous vector:  
      F32 tmp = up.x;  
      up.x = -up.y;  
      up.y =  up.z;  
      up.z =  tmp;  
   }  
  
   // construct the remaining axes:  
  
   mCross(axisY, up   , &axisX);  
   mCross(axisX, axisY, &axisZ);  
  
   mat->setColumn(0, axisX);  
   mat->setColumn(1, axisY);  
   mat->setColumn(2, axisZ);  
  
   return *mat;
}
void Skeleton::updateChildren(TSShapeInstance* shapeInstance, Bone *parent)
{
	for(S32 i=0; i<parent->children.size(); i++)
	{
		Bone *child = NULL;

		if(isBone(parent->children[i], child))
		{
			MatrixF childTrans = *getBoneTrans(shapeInstance, child);
			
			childTrans.mul(*getBoneTrans(shapeInstance, parent));

			storeBoneTrans(shapeInstance, child->boneNode, childTrans);

			if(meshShape.nodes[child->boneNode].firstChild != -1)
				updateChildren(shapeInstance, child);
		}
	}
}

/*void Skeleton::updateChildren(TSShapeInstance* shapeInstance, Bone *parent)
{
	//parse through any children we may have in our skeleton system
	for(S32 i=0; i<parent->children.size(); i++)
	{
		Bone *child = NULL;
		MatrixF newOrient;
		
		//confirm it's actually a bone
		if(isBone(parent->children[i], child))
		{
			MatrixF *childMat = getBoneTrans(shapeInstance, child);
			MatrixF *parentMat = getBoneTrans(shapeInstance, parent);
			
			VectorF curForVec, newVec, parOrient, testOrient, childOrient;

			Point3F endPos = getBoneEndPoint(shapeInstance, parent);
			curForVec = getBoneForwardVector(shapeInstance, parent);

			//since these never change, use them as a reference
			VectorF homeBoneVec =  meshShape.defaultTranslations[child->boneNode] - 
											meshShape.defaultTranslations[parent->boneNode];

			//take the difference of the default vector and the current vector, and the parent's end position
			//newOrient = MatrixF(homeBoneVec+curForVec, parentMat->getPosition()/*+endPos*/ /*+ meshShape.defaultTranslations[child->boneNode]);
			newOrient = MatrixF(homeBoneVec+curForVec, endPos);

			//store the update, which is applied at the owner's discretion
			//setBoneTrans(child, newOrient);
			storeBoneTrans(child->boneNode, newOrient);

			if(meshShape.nodes[child->boneNode].firstChild != -1)
				updateChildren(shapeInstance, child);
		}
		//it's not? well update it so we don't have wayward nodes
		//else
		//	if(meshShape.nodes[parent->boneNode].firstChild != -1)
		//		updateChildren(shapeInstance, parent->boneNode);
	}

	//check if we have normal nodes as children
	//if(meshShape.nodes[parent->boneNode].firstChild != -1)
		//if we do, then update them
	//	updateChildren(shapeInstance, parent->boneNode);

}*/

//used for updating non-skeleton bone based bone nodes in the mesh(child bones)
void Skeleton::updateChildren(TSShapeInstance* shapeInstance, S32 parent)
{
	S32 childIdx = meshShape.nodes[parent].firstChild;
	//get our first child, then step through the siblings
	
	MatrixF newOrient;
	VectorF curForVec, newVec, parOrient, testOrient, childOrient, forVec;

	shapeInstance->mNodeTransforms[childIdx].getColumn(0,&forVec);
	forVec.normalize();

	Point3F first = meshShape.defaultTranslations[childIdx];
	Point3F second = meshShape.defaultTranslations[parent];

	VectorF vec = second - first;

	Point3F endPoint = forVec * vec.len();

	//since these never change, use them as a reference
	VectorF homeBoneVec =  meshShape.defaultTranslations[childIdx] - 
									meshShape.defaultTranslations[parent];

	newOrient = MatrixF(homeBoneVec+forVec, endPoint);
	
	//store to apply later
	// meshInstance->mNodeTransforms[childIdx] = newOrient;
	storeBoneTrans(shapeInstance, childIdx, newOrient);
  
	if(meshShape.nodes[childIdx].firstChild != -1)
		updateChildren(shapeInstance, childIdx);

	//now walk through the siblings, if we have any
	while (meshShape.nodes[childIdx].nextSibling>=0)
	{
		S32 sib = meshShape.nodes[childIdx].nextSibling;
		                                     
		VectorF curForVec, newVec, parOrient, testOrient, childOrient;

		shapeInstance->mNodeTransforms[sib].getColumn(0,&forVec);
		forVec.normalize();

		Point3F first = meshShape.defaultTranslations[sib];
		Point3F second = meshShape.defaultTranslations[parent];

		VectorF vec = second - first;

		Point3F endPoint = forVec * vec.len();

		//since these never change, use them as a reference
		VectorF homeBoneVec =  meshShape.defaultTranslations[sib] - meshShape.defaultTranslations[parent];

		newOrient = MatrixF(homeBoneVec+forVec, endPoint);
		
		//store to apply later
		// meshInstance->mNodeTransforms[sib] = newOrient;
		storeBoneTrans(shapeInstance, sib, newOrient);

		if(meshShape.nodes[sib].firstChild != -1)
			updateChildren(shapeInstance, sib);
	}
}

void Skeleton::updateChildren(TSShapeInstance* shapeInstance, Bone *parent, MatrixF newTrans)
{
	for(S32 i=0; i<parent->children.size(); i++)
	{
		Bone *child = NULL;
		isBone(parent->children[i], child);

		MatrixF *childMat = getBoneTrans(shapeInstance, child);
		MatrixF *parentMat = getBoneTrans(shapeInstance, parent);

		getBoneTrans(shapeInstance, child)->mul(newTrans);

		updateChildren(shapeInstance, child);
	}
}

void Skeleton::solveJiggleBone(TSShapeInstance* shapeInstance, JiggleBone* jB, mass* m)																	//solve() method: the method where forces can be applied
{
	Point3F rootPos = getBoneTrans(shapeInstance, findBone(jB->parent))->getPosition();
	Point3F endPos = getBoneTrans(shapeInstance,jB)->getPosition();

	VectorF springVector = rootPos - endPos;							//vector between the two masses
	VectorF force = VectorF(0,0,0);														//force initially has a zero value

	JiggleBone* jBParent = NULL;
	
	//solve the 'spring' between the two bones
	F32 vecLen = springVector.len();											//distance between the two masses
	
	if (vecLen != 0){																	//to avoid a division by zero check if r is zero
		VectorF a = (springVector / vecLen) * (vecLen - jB->length) * (-jB->stiffness);
		force += a;//(springVector / vecLen) * (vecLen - jB->length) * (-jB->stiffness);	//the spring force is added to the force
	}

	//solve the rest of the forces
	//repurpose for collision code
	/*if (masses[a]->pos.y < groundHeight)		//Forces from the ground are applied if a mass collides with the ground
	{
		Vector3D v;								//A temporary Vector3D

		v = masses[a]->vel;						//get the velocity
		v.y = 0;								//omit the velocity component in y direction

		//The velocity in y direction is omited because we will apply a friction force to create 
		//a sliding effect. Sliding is parallel to the ground. Velocity in y direction will be used
		//in the absorption effect.
		masses[a]->applyForce(-v * groundFrictionConstant);		//ground friction force is applied

		v = masses[a]->vel;						//get the velocity
		v.x = 0;								//omit the x and z components of the velocity
		v.z = 0;								//we will use v in the absorption effect
		
		//above, we obtained a velocity which is vertical to the ground and it will be used in 
		//the absorption force

		if (v.y < 0)							//let's absorb energy only when a mass collides towards the ground
			masses[a]->applyForce(-v * groundAbsorptionConstant);		//the absorption force is applied
		
		//The ground shall repel a mass like a spring. 
		//By "Vector3D(0, groundRepulsionConstant, 0)" we create a vector in the plane normal direction 
		//with a magnitude of groundRepulsionConstant.
		//By (groundHeight - masses[a]->pos.y) we repel a mass as much as it crashes into the ground.
		Vector3D force = Vector3D(0, groundRepulsionConstant, 0) * 
			(groundHeight - masses[a]->pos.y);

		jB->applyForce(force);			//The ground repulsion force is applied
	}*/

	VectorF vel = jB->velocity * jB->moveFriction;						//The air friction
	if(isJiggleBone(jB->parent, jBParent))
		force += -(vel - jBParent->velocity) * jB->moveFriction.z;						//the friction force is added to the force
	else 
		force += -(vel) * jB->moveFriction.z;						    //with this addition we obtain the net force of the spring

	VectorF b = (VectorF(0,0,-9.81f) * jB->specificGravity) * jB->mass;
	force += b;//(VectorF(0,0,-9.81f) * jB->specificGravity) * jB->mass;			//The gravitational force

	m->force += force;															//net forces
}

//simulate the bone here
void Skeleton::updateJiggleBone(TSShapeInstance* shapeInstance, JiggleBone* jB, mass* m, F32 dt)																	//solve() method: the method where forces can be applied
{
	m->velocity += (m->force / jB->mass) * dt;				// Change in velocity is added to the velocity.
											// The change is proportinal with the acceleration (force / m) and change in time

	MatrixF finMat, mat = *getBoneTrans(shapeInstance, jB);
	Point3F pos = mat.getPosition();

	pos += m->velocity * dt;						// Change in position is added to the position.
											// Change in position is velocity times the change in time

	TSTransform::setMatrix(mat, pos, &finMat);
	//mat.setPosition(pos);

	MatrixF newBoneTrans = CheckDofsRestrictions(jB, finMat);

	storeBoneTrans(shapeInstance, jB->boneNode, newBoneTrans);
}

void Skeleton::accumulateVelocity(TSShapeInstance* shapeInstance)
{
	//what we do here is compare our last know bone and object positions, and then see the difference, 
	//accumulating a velocity out of the distance/rotation that we use to solve the jigglebone animation
	MatrixF nodeTransform, difference;

	//MatrixF globalDifference = shapeInstance->getTransform() - objectTransform;

	for(U32 i=0; i < oldNodeTransforms.size(); i++)
	{
		//obviously we have some transforms
		//so get our current transforms for any jigglebone's parents
		for(U32 x=0; x < jBoneList.size(); x++)
		{
			//looks like we have a hit
			if(oldNodeTransforms[i].bone == jBoneList[x]->parent)
			{
				nodeTransform = shapeInstance->mNodeTransforms[jBoneList[x]->parent];

				//difference = nodeTransform - oldNodeTransforms[i];

				//blahblahmathblah
			}
		}
	}
	
	//now flush our current transforms and store them for the next update
	
}
void Skeleton::addBone(Bone* bone)
{
	bool nameMatch = false, boneMatch = false;

	//check our bone name list
	for(U32 x=0; x < boneNames.size(); x++)
	{
		//we have a hit!
		if(!dStrcmp(boneNames[x].c_str(), bone->boneName))
			nameMatch = true;
	}
	//no name here
	if(!nameMatch)
		boneNames.push_front(bone->getName());
	//check our physical bones list
	for(U32 i=0; i< boneList.size(); i++)
	{
		//yup, it's here
		if(boneList[i]->boneNode == bone->boneNode)
			boneMatch = true;
	}
	//otherwise, we can add it to our list no problem.
	if(!boneMatch)
		boneList.push_front(bone);
}

void Skeleton::addJiggleBone(JiggleBone* jBone)
{
	//jigglebones are distinct from regular bones, so we can have a jigglebone with the same
	//bonenode as a regular bone, but cannot have 2 jigglebones on the same node.
	for(U32 i=0; i< jBoneList.size(); i++)
	{
		//if it already exists, abort adding it.
		if(jBoneList[i]->boneNode == jBone->boneNode)
			return;
	}
	//otherwise, we can add it to our list no problem.
	jBoneNames.push_front(jBone->getName());
	jBoneList.push_front(jBone);
}

void Skeleton::addIKChain(IKChain* ikchain)
{
	bool nameMatch = false, chainMatch = false;

	//first, double check we're not trying to re-add this thing
	for(U32 x=0; x<ikChainNames.size(); x++) 
		if(!dStrcmp(ikChainNames[x].c_str(), ikchain->getName()))
			nameMatch = true;

	for(U32 q=0; q<ikChains.size(); q++)
		if(!dStrcmp(ikChains[q]->rootBoneName, ikchain->rootBoneName) && 
			!dStrcmp(ikChains[q]->endBoneName, ikchain->endBoneName))
			chainMatch = true;
		
	if(!chainMatch)
	{
		S32 rootIdx = /*bodyData->shape->*/meshShape.findNode(ikchain->rootBoneName);
		S32 endIdx = /*bodyData->shape->*/meshShape.findNode(ikchain->endBoneName);
		S32 currIdx = endIdx;
		S32 priorIdx = 0;
		VectorF boneVector;

		S32 test = -1;

		Bone *bone = new Bone();

		if(rootIdx == -1 || endIdx == -1){
			Con::errorf("IKChain::addIKChain - unable to find either the root or end nodes!");
			return;
		}

		do
		{
			if(currIdx == -1)
				break;

			//first, check that we don't already have a bone at each step. If we do, just tweak the bone
			//data to hook in. otherwise, create a new bone and string it together
			bool aBone = isBone(currIdx, bone);
			if(!aBone) 
			{
				bone->boneNode = currIdx;

				if(currIdx != endIdx){
					bone->length = getBoneLength(currIdx, priorIdx);
					bone->children.push_back(priorIdx);
				}
				else
					bone->length = getBoneLength(currIdx, meshShape.nodes[currIdx].firstChild);

				getBoneDefaultTrans(bone)->getColumn(1, &boneVector);
				bone->boneVec = boneVector;

				bone->parent = meshShape.nodes[currIdx].parentIndex;
				String parentName = meshShape.getName(meshShape.nodes[currIdx].parentIndex);

				bone->boneName = meshShape.getName(meshShape.nodes[currIdx].nameIndex);
				bone->bounds = Box3F();

				bone->mTarget = ikchain->mTarget;	//the object we're trying to set a Bone to

				//as we've created an entirely new bone, we need to add it to our skeleton's global bone list
				String temp = bone->getName();
				if(temp.isEmpty())
					bone->assignName(bone->boneName);

				//register this hooooo
				bone->registerObject();

				addBone(bone);
			}
			else
				bone = findBone(currIdx);

			priorIdx = currIdx;
			currIdx = bone->parent;

			//if we can't find the node, bail this one, otherwise, add it in
			if(bone->boneNode != -1)
			{
				//add to the chain
				ikchain->addBone(bone); 
			}
			
			//clear it out for the next iteration
			bone = new Bone();
			
			U32 rootNodeParentVal = meshShape.nodes[rootIdx].parentIndex;
			U32 currentNodeVal = meshShape.nodes[currIdx].parentIndex;

			U32 savethescope = -1;

		}while(currIdx != meshShape.nodes[rootIdx].parentIndex); //if we've reached the parent of our chain, or somehow hit the end of the skeleton, end.

		//now add our completed chain to the skeleton
		ikChains.push_back(ikchain);
	}

	if(!nameMatch)
		ikChainNames.push_back(ikchain->getName());
}

void Skeleton::addIKRule(IKRule* ikrule)
{
	bool nameMatch = false, ruleMatch = false;

	//check our bone name list
	for(U32 x=0; x < ikRuleNames.size(); x++)
	{
		//we have a hit!
		if(!dStrcmp(ikRuleNames[x].c_str(), ikrule->getName()))
			nameMatch = true;
	}
	//no name here
	if(!nameMatch)
		ikRuleNames.push_front(ikrule->getName());
	//check our physical bones list
	for(U32 i=0; i< ikRules.size(); i++)
	{
		//yup, it's here
		if(ikRules[i] == ikrule)
			ruleMatch = true;
	}
	//otherwise, we can add it to our list no problem.
	if(!ruleMatch)
		ikRules.push_back(ikrule);
}

void Skeleton::addJiggleChain(JiggleChain* jchain)
{
	S32 rootIdx = /*bodyData->shape->*/meshShape.findNode(jchain->rootBoneName);
	S32 endIdx = /*bodyData->shape->*/meshShape.findNode(jchain->endBoneName);
	S32 currIdx = endIdx;
	S32 priorIdx = 0;
	VectorF boneVector;

	do
	{
		//first, add our end node, then step backwards up the heirarchy and add those
		JiggleBone *newBone = new JiggleBone();

		newBone->boneNode = currIdx;

		getBoneDefaultTrans(newBone)->getColumn(1, &boneVector); //get the forward vector of the bone instead
		newBone->boneVec = boneVector;

		newBone->parent = /*bodyData->shape->*/meshShape.nodes[currIdx].parentIndex;
		newBone->length = boneVector.len();

		newBone->boneName = /*bodyData->shape->*/meshShape.getName(/*bodyData->shape->*/meshShape.nodes[currIdx].nameIndex);
		newBone->bounds = Box3F();

		newBone->mTarget = jchain->mTarget;	//the object we're trying to set a Bone to

		/*newBone->isFlexible = isFlexible;
		newBone->moveConstraint = moveConstraint;
		newBone->moveFriction = moveFriction;

		newBone->isRigid = isRigid;

		newBone->tip_mass = tip_mass; 
		newBone->specificGravity = specificGravity;

		newBone->stiffness = stiffness;
		newBone->dampening = dampening;
		newBone->mass = mass;
		newBone->active = active;*/

		//add to the chain
		jchain->chain.push_back(newBone);

		priorIdx = currIdx;
		currIdx = newBone->parent;

	}while(currIdx != /*bodyData->shape->*/meshShape.nodes[rootIdx].parentIndex);

	//now add our completed chain to the skeleton
	jChains.push_back(jchain);
}

//this searches the whole skeleton for the bone, this is a god bit slower, hence why we prefer to region-search
//if we don't find it here, bad bad juju.
Bone* Skeleton::findBone(S32 boneID)
{
	for(S32 i=0; i<boneList.size(); i++){
		if(boneID == this->boneList[i]->boneNode){
				return this->boneList[i];
		}
	}
	Con::warnf("Error, findBone found no such Bone in the current skeleton!");
	return NULL;//since all values are -1 on new Bones, we can filter the error post-call 
}

F32 Skeleton::getBoneLength(S32 boneA, S32 boneB)
{
	Point3F first = /*bodyData->shape->*/meshShape.defaultTranslations[boneA];
	Point3F second = /*bodyData->shape->*/meshShape.defaultTranslations[boneB];

	VectorF vec = second - first;

	return vec.len();
}

Point3F Skeleton::getBoneEndPoint(TSShapeInstance* shapeInstance, Bone *bone)
{
	Point3F endPos;
	VectorF forVec;
	MatrixF trans;

	//getBoneTrans(shapeInstance, bone)->getColumn(0,&forVec);
	trans = *getBoneTrans(shapeInstance, bone);
	trans.getColumn(0,&forVec);
	forVec.normalize();

	endPos = forVec * bone->length;

	//and offset that from the current position
	endPos *= trans.getPosition();

	return endPos;
}

VectorF Skeleton::getBoneForwardVector(TSShapeInstance* shapeInstance, Bone *bone)
{
	VectorF forVec;

	shapeInstance->mNodeTransforms[bone->boneNode].getColumn(0,&forVec);
	forVec.normalize();

	return forVec;
}

//returns the vector the bones have using the default translation. good for refernce
VectorF Skeleton::getBoneHomeVector(Bone *boneA, Bone *boneB){
	Point3F first = /*bodyData->shape->*/meshShape.defaultTranslations[boneA->boneNode];
	Point3F second = /*bodyData->shape->*/meshShape.defaultTranslations[boneB->boneNode];

	VectorF vec = second - first;

	return vec;
}

MatrixF* Skeleton::getBoneTrans(TSShapeInstance* shapeInstance, Bone *bone)
{
	/*for(U32 i=0; i<shapeInstance->smNodeStoredTransforms.size(); i++){
		if(shapeInstance->smNodeStoredTransforms[i].bone == bone->boneNode)
			return &shapeInstance->smNodeStoredTransforms[i].trans;
	}*/
	return &shapeInstance->mNodeTransforms[bone->boneNode];
}

MatrixF Skeleton::getLocalBoneTrans(TSShapeInstance* shapeInstance, Bone *bone)
{
	for(U32 i=0; i<shapeInstance->smNodeStoredTransforms.size(); i++){
		if(shapeInstance->smNodeStoredTransforms[i].bone == bone->boneNode)
			return shapeInstance->smNodeStoredTransforms[i].trans;
	}
	return shapeInstance->smNodeLocalTransforms[bone->boneNode];
}

/*MatrixF* Skeleton::getLocalTrans(TSShapeInstance* shapeInstance, Bone *bone)
{
	//basically, this function acts as a impromptu animateNodes. We go through and process what the intended transform would be for a given node,
	//assuming for all the animations that would be playing normally. This gets a default, local transform we can IK from.
   bool rotMatters, transMatters, scaleMaaters;
   bool rotSet, transSet, scaleSet;
   S32 i,j,nodeIndex,a,b,start,end,firstBlend = shapeInstance->mThreadList.size();
   for (i=0; i<shapeInstance->mThreadList.size(); i++)
   {
      TSThread * th = shapeInstance->mThreadList[i];

      if (th->getSequence()->isBlend())
      {
         // blend sequences need default (if not set by other sequence)
         // break rather than continue because the rest will be blends too
         firstBlend = i;
         break;
      }
	  rotMatters = th->getSequence()->rotationMatters.test(bone->boneNode);
      transMatters = th->getSequence()->translationMatters.test(bone->boneNode);
      scaleMaaters = th->getSequence()->scaleMatters.test(bone->boneNode);
   }
   rotSet = shapeInstance->mMaskRotationNodes.test(bone->boneNode);


   TSIntegerSet maskPosNodes = shapeInstance->mMaskPosXNodes;
   maskPosNodes.overlap(shapeInstance->mMaskPosYNodes);
   maskPosNodes.overlap(shapeInstance->mMaskPosZNodes);

   transSet = maskPosNodes.test(bone->boneNode);

   if (rotSet.test(bone->boneNode))
   {
      mShape->defaultRotations[bone->boneNode].getQuatF(&smNodeCurrentRotations[bone->boneNode]);
      shapeInstance->smRotationThreads[bone->boneNode] = NULL;
   }
   if (transSet.test(bone->boneNode))
   {
      shapeInstance->smNodeCurrentTranslations[bone->boneNode] = mShape->defaultTranslations[bone->boneNode];
      shapeInstance->smTranslationThreads[bone->boneNode] = NULL;
   }

   rotSet = false;
   transSet = false;

   // default scale
   //if (scaleCurrentlyAnimated())
   //   handleDefaultScale(a,b,scaleBeenSet);

   // handle non-blend sequences
   for (i=0; i<firstBlend; i++)
   {
      TSThread * th = shapeInstance->mThreadList[i];

      j=0;

      // skip nodes outside of this detail
      if (!rotBeenSet.test(bone->boneNode))
      {
        QuatF q1,q2;
        mShape->getRotation(*th->getSequence(),th->keyNum1,j,&q1);
        mShape->getRotation(*th->getSequence(),th->keyNum2,j,&q2);
		TSTransform::interpolate(q1,q2,th->keyPos,&shapeInstance->smNodeCurrentRotations[bone->boneNode]);
        rotSet = true;
        shapeInstance->smRotationThreads[bone->boneNode] = th;
      }

      j=0;

      if (!tranBeenSet.test(bone->boneNode))
      {
         //if (maskPosNodes.test(nodeIndex))
         //   handleMaskedPositionNode(th,nodeIndex,j);
         //else
         //{
            const Point3F & p1 = mShape->getTranslation(*th->getSequence(),th->keyNum1,j);
            const Point3F & p2 = mShape->getTranslation(*th->getSequence(),th->keyNum2,j);
			TSTransform::interpolate(p1,p2,th->keyPos,&shapeInstance->smNodeCurrentTranslations[bone->boneNode]);
            shapeInstance->smTranslationThreads[bone->boneNode] = th;
         //}
         tranSet = true;
      }

      //if (shapeInstance->scaleCurrentlyAnimated())
      //   shapeInstance->handleAnimatedScale(th,a,b,scaleSet);
   }

   // compute transforms
   TSTransform::setMatrix(shapeInstance->smNodeCurrentRotations[bone->boneNode],
		shapeInstance->smNodeCurrentTranslations[bone->boneNode],&shapeInstance->smNodeLocalTransforms[bone->boneNode]);

   // add scale onto transforms
   //if (shapeInstance->scaleCurrentlyAnimated())
   //   handleNodeScale(a,b);

   // handle blend sequences
   /*for (i=firstBlend; i<mThreadList.size(); i++)
   {
      TSThread * th = mThreadList[i];
      if (th->blendDisabled)
         continue;

      handleBlendSequence(th,a,b);
   }*/

   // transitions...
   /*if (shapeInstance->inTransition())
      shapeInstance->handleTransitionNodes(a,b);

   // multiply transforms...
   S32 parentIdx = mShape->nodes[shapeInstance->].parentIndex;
   if (parentIdx < 0)
      mNodeTransforms[shapeInstance->] = smNodeLocalTransforms[shapeInstance->];
   else
      mNodeTransforms[shapeInstance->].mul(mNodeTransforms[parentIdx],smNodeLocalTransforms[shapeInstance->]);
 }*/
/*void Skeleton::setBoneTrans(TSShapeInstance* shapeInstance, Bone *bone, MatrixF &mat)
{
	shapeInstance->mNodeTransforms[bone->boneNode] = mat;
}*/

void Skeleton::setBoneTrans(TSShapeInstance* shapeInstance, U32 boneNode, MatrixF &mat)
{
	//shapeInstance->mNodeTransforms[boneNode] = mat;
	//set the locals first
	shapeInstance->smNodeIKTransforms[boneNode] = mat;

	//then adjust relative to parent
    /*S32 parentIdx = shapeInstance->getShape()->nodes[boneNode].parentIndex;
    if (parentIdx < 0)
	   shapeInstance->mNodeTransforms[boneNode] = shapeInstance->smNodeLocalTransforms[boneNode];
    else
	   shapeInstance->mNodeTransforms[boneNode].mul(shapeInstance->mNodeTransforms[parentIdx],shapeInstance->smNodeLocalTransforms[boneNode]);*/

}

void Skeleton::storeBoneTrans(TSShapeInstance *shapeInstance, Bone *bone, MatrixF &mat)
{
	bool boneMatch = false;

	for(U32 i=0; i< shapeInstance->smNodeStoredTransforms.size(); i++)
	{
		if(shapeInstance->smNodeStoredTransforms[i].bone == bone->boneNode) {
			shapeInstance->smNodeStoredTransforms[i].trans = mat;
			boneMatch = true;
		}
	}
	if(!boneMatch){
		boneTransform n;
		n.bone = bone->boneNode;
		n.trans = mat;

		shapeInstance->smNodeStoredTransforms.push_front(n);
	}
}

void Skeleton::storeBoneTrans(TSShapeInstance *shapeInstance, U32 boneNode, MatrixF &mat)
{
	bool boneMatch = false;

	for(U32 i=0; i< shapeInstance->smNodeStoredTransforms.size(); i++)
	{
		if(shapeInstance->smNodeStoredTransforms[i].bone == boneNode) {
			MatrixF prev = shapeInstance->smNodeStoredTransforms[i].trans;

			if(prev.getPosition() == mat.getPosition() && prev.getForwardVector() == mat.getForwardVector()
										&& prev.getUpVector() == mat.getUpVector())
				int test = 1;

			shapeInstance->smNodeStoredTransforms[i].trans = mat;
			boneMatch = true;

			MatrixF store = shapeInstance->smNodeStoredTransforms[i].trans;
			MatrixF bone = shapeInstance->smNodeLocalTransforms[boneNode];
			MatrixF bleh;
		}
	}
	if(!boneMatch){
		boneTransform n;
		n.bone = boneNode;
		n.trans = mat;

		shapeInstance->smNodeStoredTransforms.push_front(n);
	}
}

MatrixF Skeleton::getStoredBoneTrans(TSShapeInstance* shapeInstance, U32 boneNode)
{
	for(U32 i=0; i< shapeInstance->smNodeStoredTransforms.size(); i++){
		if(shapeInstance->smNodeStoredTransforms[i].bone == boneNode){
			return shapeInstance->smNodeStoredTransforms[i].trans;
		}
	}

	return MatrixF::Identity;
}

bool Skeleton::isStoredBoneTrans(TSShapeInstance* shapeInstance, U32 boneNode)
{
	for(U32 i=0; i< shapeInstance->smNodeStoredTransforms.size(); i++)
		if(shapeInstance->smNodeStoredTransforms[i].bone == boneNode)
			return true;

	return false;
}

void Skeleton::clearStoredTransforms()
{
	return;
	//mShapeInstance->smNodeStoredTransforms.clear();
}
MatrixF* Skeleton::getBoneDefaultTrans(Bone *bone)
{
	MatrixF *boneMat = new MatrixF();
	//QuatF quatR = /*bodyData->shape->*/meshShape.defaultRotations[bone->boneNode].getQuatF(&QuatF());
	QuatF quatR; 
	meshShape.defaultRotations[bone->boneNode].getQuatF(&quatR);
	//boneMat->set(quatR.QuatToEuler(), /*bodyData->shape->*/meshShape.defaultTranslations[bone->boneNode]);

	TSTransform::setMatrix(quatR,meshShape.defaultTranslations[bone->boneNode],boneMat);

	return boneMat;
}


bool Skeleton::isBone(S32 boneID, Bone *bone)
{
	for(S32 i=0; i<boneList.size(); i++){
		Bone* temp = boneList[i];
		if(boneID == boneList[i]->boneNode){
			bone = boneList[i];
			return true;
		}
	}

	return false;
}

bool Skeleton::isJiggleBone(S32 boneID, Bone *bone)
{
	for(S32 i=0; i < jBoneList.size(); i++){
		if(boneID == this->jBoneList[i]->boneNode){
			bone = this->jBoneList[i];
			return true;
		}
	}

	return false;
}

void Skeleton::clearSkeletalData()
{
	//this function deletes all stored bone/chain/rule info, leaving just the names list
	boneList.clear();
    jBoneList.clear();
    ikChains.clear();
    jChains.clear();
    ikRules.clear();
}

void Skeleton::clearSkeletalNames()
{
	//this function deletes all stored names
	boneNames.clear();
    jBoneNames.clear();
    ikChainNames.clear();
    jChainNames.clear();
    ikRuleNames.clear();
}

