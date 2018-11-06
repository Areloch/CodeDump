#ifndef _SKELETON_H_
#define _SKELETON_H_

#ifndef _SHAPEBASE_H_
#include "T3D/shapeBase.h"
#endif
#ifndef _GAMEBASE_H_
#include "T3D/gameBase/gameBase.h"
#endif
#ifndef _STRINGUNIT_H_
#include "core/strings/stringUnit.h"
#endif

class Skeleton;
class JiggleBone;
class tempJBone;
struct ShapeBaseData;
class TSShapeInstance;

//currently, Bones will only support a single child untill i can figure out a good way to have multiple-inheritant IK

class Bone : public SimObject
{
	typedef SimObject Parent;
	friend Skeleton;
	friend JiggleBone; //<-stupid compiler of stupidness. >:(
	friend tempJBone;

private:
	S32					boneNode;
	Vector<S32>			children;	//storing the nodeID is cleaner and more effecient 
									//than a full Bone ref, usefull for updating non-bone children(like hands at the end of an arm IK)
	S32					parent;
	VectorF				boneVec;	//vectors to our child from this bone
	F32					length;		//length of our origin to our child
public:

   StringTableEntry		boneName;
   Box3F				bounds;

   ShapeBaseData*		mTarget;	//the object we're trying to set a Bone to

   EulerF				mDof[1];	//a min/max for each axis

   F32					mass;

   bool onAdd();

	Bone(){
		length = 0.1f;
		bounds = Box3F();
		parent = -1;
		boneNode = -1;
		boneVec = VectorF(0,0,0);
		boneName = "";
		bounds = Box3F(0.f);
		mDof[0] = EulerF(-120, -120, -120); //min
		mDof[1] = EulerF(120, 120, 120); //max
	}

	~Bone(){}
	static void initPersistFields();

	U32 getBoneNode() { return boneNode; }
    DECLARE_CONOBJECT(Bone);
};

class JiggleBone : public Bone
{
	typedef Bone Parent;

public:

	//below is used to generate the spring's data	

	//flexible
	//the bone has full range of motion, forward/back, left/right and up/down.
		bool isFlexible;

		Point3F moveConstraint;
		Point3F moveFriction;

	//rigid
	//the bone stays a set length away from the base, but moves around freely left/right and up/down
		bool isRigid;

	//universal properties
		F32 tip_mass; 
		F32 specificGravity;

		F32 stiffness;
		Point3F dampening;
		F32 mass;
		bool active;

	VectorF force; //force of the spring!
	VectorF velocity;

	void applyForce(VectorF f)
	{
		force += f;					// The external force is added to the force of the mass
	}

	/*
	  void simulate(float dt) method calculates the new velocity and new position of 
	  the mass according to change in time (dt). Here, a simulation method called
	  "The Euler Method" is used. The Euler Method is not always accurate, but it is 
	  simple. It is suitable for most of physical simulations that we know in common 
	  computer and video games.
	*/

	bool onAdd();

	JiggleBone(){
		bounds = Box3F();
		parent = -1;

		force = Point3F(0,0,0);
		velocity = Point3F(0,0,0);

		length = 1.f; 						
		tip_mass = 1.f; 
		specificGravity = 1.f;

		stiffness = 1.f;
		dampening = Point3F(1,1,1);

		mass = 1.f;

		moveConstraint = Point3F(1,1,1);
		moveFriction = Point3F(1,1,1);
	}
	~JiggleBone(){}
	static void initPersistFields();

	DECLARE_CONOBJECT(JiggleBone); 
};

class tempJBone : public JiggleBone
{
	typedef JiggleBone Parent;

public:

	bool onAdd(){return true;}

	tempJBone(){
		bounds = Box3F();
		parent = -1;

		force = Point3F(0,0,0);

		length = 1.f; 						
		tip_mass = 1.f; 

		stiffness = 1.f;
		dampening = Point3F(1,1,1);
	}
	~tempJBone(){}
	void setFromBone(Bone *bone);
};

struct mass
{
	S32 boneNum;
	VectorF force;
	VectorF velocity;

	mass(){
		boneNum = -1;
		force = VectorF(0,0,0);
		velocity = VectorF(0,0,0);
	}
};
//===============================================================

//esentially a special container class for a list of bones
//the number is arbitrary, you define a start and end of the chain, but it has to be more than 1 bone.
class IKChain : public SimObject
{
	typedef SimObject Parent;
	friend Skeleton;
public:

    StringTableEntry		rootBoneName;
    StringTableEntry		endBoneName;
	ShapeBaseData*			mTarget;
	S32						targetID;
	F32						tolerance;	//how tolerant are we of being away from our end goal
	bool					dontReach; //if the chain isn't long enough, should we at least reach for it anyways?
    Vector<Bone*>			chain;

    bool onAdd();

	IKChain()
	{
		mTarget = NULL;
		targetID = 0;
		tolerance = 0.1f;
		dontReach = true;
		rootBoneName = "";
		endBoneName = "";
	}
	~IKChain(){}
	static void initPersistFields();

	void addBone(Bone *bone) { 
		U32 node = bone->getBoneNode();
		for(U32 i=0; i<chain.size(); i++){
			//if it already exists, abort adding it.
			if(chain[i]->getBoneNode() == node)
				return;
		}
		//otherwise, we can add it to our list no problem.
		chain.push_front(bone);
	}

	S32 getBoneIndex(S32 boneNode)
	{
		for(U32 i=0; i<chain.size(); i++){
			if(chain[i]->getBoneNode() == boneNode)
				return i;
		}
		return -1;
	}

   DECLARE_CONOBJECT(IKChain); 
};

class JiggleChain : public IKChain
{
	typedef IKChain Parent;
	friend Skeleton;
public:

	//NOTE THESE SETTINGS APPLY AUTOMATICALLY TO ALL BONES CREATED BY THE CHAIN
	//IF THE BONE ALREADY EXISTED, IT WILL USE THE SETTINGS IT HAS
	//flexible
	//the bone has full range of motion, forward/back, left/right and up/down.
		bool isFlexible;

		Point3F moveConstraint;
		Point3F moveFriction;

		Vector<JiggleBone*>			chain;
	

	//rigid
	//the bone stays a set length away from the base, but moves around freely left/right and up/down
		bool isRigid;

	//universal properties
		F32 tip_mass; 
		F32 specificGravity;

		F32 stiffness;
		Point3F dampening;
		F32 mass;
		bool active;


	bool onAdd();

	JiggleChain(){}
	~JiggleChain(){}
	static void initPersistFields();

   DECLARE_CONOBJECT(JiggleChain); 
};

class IKRule : public SimObject
{
	typedef SimObject Parent;
	friend Skeleton;
public:

    IKChain*			targetChain;
	ShapeBaseData*		mTarget;

	StringTableEntry	animationName;
	StringTableEntry	goalNodeName;

	S32					triggerID;
	F32					blendAmount;
	Point3F				offset;

	bool				matchOrientToGoal;			//do we rotate our endbone to match the goal node's rotation? (useful for grabbing)

    bool onAdd();

	IKRule()
	{
		targetChain = NULL;
		mTarget = NULL;

		animationName = "";		//during what animation should we do the IK?
		goalNodeName = "";

		triggerID = -1;			//what trigger number should start/stop the IK effect(useful for walking animations)
		blendAmount = 0.f;		//how much do we blend between the IK and the original animation
		offset = Point3F(0,0,0);
	}
	~IKRule(){}
	static void initPersistFields();

   DECLARE_CONOBJECT(IKRule); 
};

struct boneTransform
{
	S32 bone;
	MatrixF trans;
};

//===============================================================

class Skeleton {
	friend class ShapeBase;
	friend struct ShapeBaseData;

private:
	TSShape			 meshShape;				//the static model information


	Vector<Bone*> boneList;				//this is the entire list of bones for the skeleton, it's then furrther
										//broken into IK chains if needed

	Vector<String> boneNames;
	Vector<String> jBoneNames;
	Vector<String> ikChainNames;
	Vector<String> jChainNames;
	Vector<String> ikRuleNames;

public:
	Vector<IKChain*>		ikChains;	//chains are separated much like the bones are. IK chains are for limbs,
	Vector<JiggleChain*>	jChains;	//jigglechains are for clothing(capes, cloaks, etc) or rope/chains.

	Vector<JiggleBone*>     jBoneList;		//jigglebones are handled separately from normal bones/IKChains. 
										//the benifit of this is you can define IK-capable bones and jigglebones
										//on the same mesh bone

	Vector<IKRule*>			ikRules;

	//Vector<boneTransform> newTransQueue;			//our updated bone transform information.

	//old transforms, used for jiggleBone accumulation
	Vector<boneTransform>	 oldNodeTransforms;
	MatrixF					 objectTransform;

	Skeleton();
	~Skeleton();

	void CCDIK(TSShapeInstance* shapeInstance, IKChain *ikchain, MatrixF endTrans);
	void physicalIK(TSShapeInstance* shapeInstance, IKChain *ikchain, MatrixF endTrans, F32 dt);
	void analiticalIK(TSShapeInstance* shapeInstance, IKChain *ikchain, MatrixF endTrans);

	MatrixF directionToMatrix(VectorF dir, VectorF up); //MATH!

	MatrixF CheckDofsRestrictions(Bone *bone, MatrixF mat);

	void updateChildren(TSShapeInstance* shapeInstance, Bone *parent);
	void updateChildren(TSShapeInstance* shapeInstance, Bone *parent, MatrixF newTrans);
	void updateChildren(TSShapeInstance* shapeInstance, S32 parent);

	void updateChildren(TSShapeInstance* shapeInstance, Bone *current, IKChain *chain);
	MatrixF setForwardVector(MatrixF *mat, VectorF axisY, VectorF up = VectorF(0,0,1));

	void solveJiggleBone(TSShapeInstance* shapeInstance, JiggleBone* jB, mass *m);
	void updateJiggleBone(TSShapeInstance* shapeInstance, JiggleBone* jB, mass *m, F32 dt);
	void accumulateVelocity(TSShapeInstance* shapeInstance);

	void addBone(Bone* bone);
	void addJiggleBone(JiggleBone* jBone);
	void addIKChain(IKChain* ikchain);
	void addJiggleChain(JiggleChain* jchain);

	void addIKRule(IKRule* ikrule);

	bool isBone(S32 boneID, Bone *bone);
	bool isJiggleBone(S32 boneID, Bone *bone);

	//this searches the whole skeleton for the bone, this is a god bit slower, hence why we prefer to region-search
	//if we don't find it here, bad bad juju.
	Bone* findBone(S32 boneID);
	F32 getBoneLength(S32 boneA, S32 boneB);
	MatrixF* getBoneTrans(TSShapeInstance* shapeInstance, Bone *bone);
	MatrixF  getLocalBoneTrans(TSShapeInstance* shapeInstance, Bone *bone);
	//void setBoneTrans(TSShapeInstance* shapeInstance, Bone *bone, MatrixF &mat);
	void setBoneTrans(TSShapeInstance* shapeInstance, U32 boneNode, MatrixF &mat);
	MatrixF* getBoneDefaultTrans(Bone *bone);
	VectorF getBoneEndPoint(TSShapeInstance* shapeInstance, Bone *bone);

	void storeBoneTrans(TSShapeInstance *shapeInstance, U32 boneNode, MatrixF &mat);
	void storeBoneTrans(TSShapeInstance *shapeInstance, Bone *bone, MatrixF &mat);		//we store our new transforms here, and then once we're done, the target will
														//grab the updated transforms and apply them
	void clearStoredTransforms();						//afterwards, we'll clear the stored transforms via this function

	MatrixF getStoredBoneTrans(TSShapeInstance* shapeInstance, U32 boneNode);
	bool isStoredBoneTrans(TSShapeInstance* shapeInstance, U32 boneNode);

	//returns the vector the bones have using the default translation. good for refernce
	VectorF getBoneHomeVector(Bone *boneA, Bone *boneB);

	//returns our current forward vector
	VectorF getBoneForwardVector(TSShapeInstance* shapeInstance, Bone *bone);

	void clearSkeletalData();
	void clearSkeletalNames();
};

#endif