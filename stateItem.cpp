//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/StateItem.h"

#include "core/stream/bitStream.h"
#include "math/mMath.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "sim/netConnection.h"
#include "collision/boxConvex.h"
#include "collision/earlyOutPolyList.h"
#include "collision/extrudedPolyList.h"
#include "math/mPolyhedron.h"
#include "math/mathIO.h"
#include "lighting/lightInfo.h"
#include "lighting/lightManager.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsBody.h"
#include "T3D/physics/physicsCollision.h"
#include "ts/tsShapeInstance.h"
#include "console/engineAPI.h"

//-JR
#include "core/resourceManager.h"
#include "console/consoleInternal.h"
#include "console/engineAPI.h"
#include "T3D/fx/particleEmitter.h"
#include "T3D/projectile.h"
#include "T3D/gameBase/gameConnection.h"
#include "T3D/debris.h"
#include "math/mathUtils.h"
#include "sim/netObject.h"
#include "sfx/sfxTrack.h"
#include "sfx/sfxSource.h"
#include "sfx/sfxSystem.h"
#include "sfx/sfxTypes.h"
#include "scene/sceneManager.h"
#include "scene/sceneRenderState.h"
#include "core/stream/fileStream.h"
//-JR


const F32 sRotationSpeed = 6.0f;        // Secs/Rotation
const F32 sAtRestVelocity = 0.15f;      // Min speed after collision
const S32 sCollisionTimeout = 15;       // Timout value in ticks

// Client prediction
static F32 sMinWarpTicks = 0.5 ;        // Fraction of tick at which instant warp occures
static S32 sMaxWarpTicks = 3;           // Max warp duration in ticks

F32 StateItem::mGravity = -20.0f;

const U32 sClientCollisionMask = (TerrainObjectType     |
                                  InteriorObjectType    |  StaticShapeObjectType |
                                  VehicleObjectType     |  PlayerObjectType      | 
                                  StaticObjectType);

const U32 sServerCollisionMask = (sClientCollisionMask |
                                  TriggerObjectType); //-JR this was added back in, because I want it to track in triggers. may be useful

const S32 StateItem::csmAtRestTimer = 64;

static const U32 sgAllowedDynamicTypes = DynamicShapeObjectType;//DamagableStateItemObjectType; ?? -JR

//-JR
//Advanced StateItem Support

StateItemData* InvalidImagePtr = (StateItemData*) 1;

ImplementEnumType( StateItemLoadedState,
   "The loaded state of this ShapeBase\n"
   "@ingroup gameObjects\n\n")
   { StateItemData::StateData::IgnoreLoaded, "Ignore", "Ignore the loaded state.\n" },
   { StateItemData::StateData::Loaded,       "Loaded", "ShapeBaseImage is loaded.\n" },
   { StateItemData::StateData::NotLoaded,    "Empty", "ShapeBaseImage is not loaded.\n" },
EndImplementEnumType;

ImplementEnumType( StateItemSpinState,
   "How the spin animation should be played.\n"
   "@ingroup gameobjects\n\n")
   { StateItemData::StateData::IgnoreSpin,"Ignore", "No changes to the spin sequence.\n" },
   { StateItemData::StateData::NoSpin,    "Stop", "Stops the spin sequence at its current position\n" },
   { StateItemData::StateData::SpinUp,    "SpinUp", "Increase spin sequence timeScale from 0 (on state entry) to 1 (after stateTimeoutValue seconds).\n" },
   { StateItemData::StateData::SpinDown,  "SpinDown", "Decrease spin sequence timeScale from 1 (on state entry) to 0 (after stateTimeoutValue seconds).\n" },
   { StateItemData::StateData::FullSpin,  "FullSpeed", "Resume the spin sequence playback at its current position with timeScale = 1.\n"},
EndImplementEnumType;

ImplementEnumType( StateItemRecoilState,
   "What kind of recoil this ShapeBaseImage should emit when fired.\n"
   "@ingroup gameobjects\n\n")
   { StateItemData::StateData::NoRecoil,     "NoRecoil", "No recoil occurs.\n" },
   { StateItemData::StateData::LightRecoil,  "LightRecoil", "A light recoil occurs.\n" },
   { StateItemData::StateData::MediumRecoil, "MediumRecoil", "A medium recoil occurs.\n" },
   { StateItemData::StateData::HeavyRecoil,  "HeavyRecoil", "A heavy recoil occurs.\n" },
EndImplementEnumType;

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

IMPLEMENT_CALLBACK( StateItemData, onMount, void, ( ShapeBase* obj, S32 slot, F32 dt ), ( obj, slot, dt ),
   "Called when the Image is first mounted to the object.\n"
   "@param obj object that this Image has been mounted to\n"
   "@param slot Image mount slot\n"
   "@param dt time remaining in this Image update\n" );

IMPLEMENT_CALLBACK( StateItemData, onUnmount, void, ( ShapeBase* obj, S32 slot, F32 dt ), ( obj, slot, dt ),
   "Called when the Image is unmounted from the object.\n"
   "@param obj object that this Image has been unmounted from\n"
   "@param slot Image mount slot\n"
   "@param dt time remaining in this Image update\n" );

StateItemData::StateData::StateData()
{
   name = 0;
   transition.loaded[0] = transition.loaded[1] = -1;
   transition.ammo[0] = transition.ammo[1] = -1;
   transition.target[0] = transition.target[1] = -1;
   transition.trigger[0] = transition.trigger[1] = -1;
   transition.altTrigger[0] = transition.altTrigger[1] = -1;
   transition.wet[0] = transition.wet[1] = -1;
   transition.motion[0] = transition.motion[1] = -1;
   transition.timeout = -1;
   waitForTimeout = false; //set to false for better weapon states support -JR
   timeoutValue = 0;
   fire = false;
   altFire = false;
   reload = false;
   energyDrain = 0;
   allowImageChange = true;
   loaded = IgnoreLoaded;
   spin = IgnoreSpin;
   recoil = NoRecoil;
   sound = 0;
   emitter = NULL;
   script = 0;
   ignoreLoadedForReady = false;
   
   ejectShell = false;
   scaleAnimation = false;
   sequenceTransitionIn = false;
   sequenceTransitionOut = false;
   sequenceNeverTransition = false;
   sequenceTransitionTime = 0;
   direction = false;
   emitterTime = 0.0f;

   sequence = -1;
   sequenceVis = -1;
   flashSequence = false;
   emitterNode = -1;
}

static StateItemData::StateData gDefaultStateData;
//-JR

//----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(StateItemData);

ConsoleDocClass( StateItemData,
   "@brief Stores properties for an individual StateItem type.\n\n"   

   "Critical references include a DTS that represents the mesh of the StateItem and the physics data for its movement.\n"
   "The rest of the datablock refers to any lights that we wish for this StateItem object to emit.\n\n"
    "@tsexample\n"
	"datablock StateItemData(HealthKitSmall)\n"
	"	{\n"
	"		category =\"Health\";\n"
	"		className = \"HealthPatch\";\n"
	"		shapeFile = \"art/shapes/StateItems/kit/healthkit.dts\";\n"
	"		gravityMod = \"1.0\";\n"
	"		mass = 2;\n"
	"		friction = 1;\n"
	"		elasticity = 0.3;\n"
    "		density = 2;\n"
	"		drag = 0.5;\n"
	"		maxVelocity = \"10.0\";\n"
	"		emap = true;\n"
	"		sticky = false;\n"
	"		dynamicType = \"0\"\n;"
	"		lightOnlyStatic = false;\n"
	"		lightType = \"NoLight\";\n"
	"		lightColor = \"1.0 1.0 1.0 1.0\";\n"
	"		lightTime = 1000;\n"
	"		lightRadius = 10.0;\n"
   "   simpleServerCollision = true;"
   "     // Dynamic properties used by the scripts\n\n"
   "		pickupName = \"a small health kit\";\n"
	"		repairAmount = 50;\n"
	"	};\n"
    "@endtsexample\n"
   "@ingroup gameObjects\n"
);


StateItemData::StateItemData()
{
   shadowEnable = true;

   friction = 0;
   elasticity = 0;

   sticky = false;
   gravityMod = 1.0;
   maxVelocity = -1;

   density = 2;
   drag = 0.5;

   dynamicTypeField     = 0;

   lightOnlyStatic = false;
   lightType = StateItem::NoLight;
   lightColor.set(1.f,1.f,1.f,1.f);
   lightTime = 1000;
   lightRadius = 10.f; 
   
   simpleServerCollision = true;

   //-JR
   //advanced StateItem support
   emap = false;

   mountPoint = 0;
   mountOffset.identity();
   eyeOffset.identity();
   correctMuzzleVector = true;
   correctMuzzleVectorTP = true;
   firstPerson = true;
   useEyeOffset = false;
   useEyeNode = false;
   mass = 0;

   usesEnergy = false;
   minEnergy = 2;
   accuFire = false;

   projectile = NULL;

   cloakable = true;

   lightDuration = 1000;
   lightBrightness = 1.0f;

   mountTransform.identity();
   shapeName = "";
   itemAnimPrefix = "";
   fireState = -1;
   altFireState = -1;
   reloadState = -1;
   computeCRC = false;

   animateOnServer = false;

   scriptAnimTransitionTime = 0.25f;

   //
   for (int i = 0; i < MaxStates; i++) {
      stateName[i] = 0;
      stateTransitionLoaded[i] = 0;
      stateTransitionNotLoaded[i] = 0;
      stateTransitionAmmo[i] = 0;
      stateTransitionNoAmmo[i] = 0;
      stateTransitionTarget[i] = 0;
      stateTransitionNoTarget[i] = 0;
      stateTransitionWet[i] = 0;
      stateTransitionNotWet[i] = 0;
      stateTransitionMotion[i] = 0;
      stateTransitionNoMotion[i] = 0;
      stateTransitionTriggerUp[i] = 0;
      stateTransitionTriggerDown[i] = 0;
      stateTransitionAltTriggerUp[i] = 0;
      stateTransitionAltTriggerDown[i] = 0;
      stateTransitionTimeout[i] = 0;

	  stateTransitionGeneric0In[i] = 0;
      stateTransitionGeneric0Out[i] = 0;
      stateTransitionGeneric1In[i] = 0;
      stateTransitionGeneric1Out[i] = 0;
      stateTransitionGeneric2In[i] = 0;
      stateTransitionGeneric2Out[i] = 0;
      stateTransitionGeneric3In[i] = 0;
      stateTransitionGeneric3Out[i] = 0;

      stateWaitForTimeout[i] = false;
      stateTimeoutValue[i] = 0;
      stateFire[i] = false;
      stateAlternateFire[i] = false;
      stateReload[i] = false;
      stateEjectShell[i] = false;
      stateEnergyDrain[i] = 0;
      stateAllowImageChange[i] = true;
      stateScaleAnimation[i] = true;
      stateSequenceTransitionIn[i] = false;
      stateSequenceTransitionOut[i] = false;
      stateSequenceNeverTransition[i] = false;
      stateSequenceTransitionTime[i] = 0.25f;
      stateDirection[i] = true;
      stateLoaded[i] = StateData::IgnoreLoaded;
      stateSpin[i] = StateData::IgnoreSpin;
      stateRecoil[i] = StateData::NoRecoil;
      stateSequence[i] = 0;
      stateSequenceRandomFlash[i] = false;

      stateShapeSequence[i] = 0;
      stateScaleShapeSequence[i] = false;

      stateSound[i] = 0;
      stateScript[i] = 0;
      stateEmitter[i] = 0;
      stateEmitterTime[i] = 0;
      stateEmitterNode[i] = 0;
      stateIgnoreLoadedForReady[i] = false;
   }
   statesLoaded = false;
   
   maxConcurrentSounds = 0;

   useRemainderDT = false;

   casing = NULL;
   casingID = 0;
   shellExitDir.set( 1.0, 0.0, 1.0 );
   shellExitDir.normalize();
   shellExitVariance = 20.0;
   shellVelocity = 1.0;
   
   fireStateName = NULL;
   mCRC = U32_MAX;
   retractNode = -1;
   muzzleNode = -1;
   ejectNode = -1;
   emitterNode = -1;
   eyeMountNode = -1;
   eyeNode = -1;
   spinSequence = -1;
   ambientSequence = -1;
   isAnimated = false;
   hasFlash = false;

   shakeCamera = false;
   camShakeFreq = Point3F::Zero;
   camShakeAmp = Point3F::Zero;
   //-JR
}

//-JR
//advanced StateItem support
bool StateItemData::onAdd()
{
   if (!Parent::onAdd())
      return false;

   // Copy state data from the scripting arrays into the
   // state structure array. If we have state data already,
   // we are on the client and need to leave it alone.
   for (U32 i = 0; i < MaxStates; i++) {
	  StateData& s = state[i];
	  if (statesLoaded == false) {
		 s.name = stateName[i];
		 s.transition.loaded[0] = lookupState(stateTransitionNotLoaded[i]);
		 s.transition.loaded[1] = lookupState(stateTransitionLoaded[i]);
		 s.transition.ammo[0] = lookupState(stateTransitionNoAmmo[i]);
		 s.transition.ammo[1] = lookupState(stateTransitionAmmo[i]);
		 s.transition.target[0] = lookupState(stateTransitionNoTarget[i]);
		 s.transition.target[1] = lookupState(stateTransitionTarget[i]);
		 s.transition.wet[0] = lookupState(stateTransitionNotWet[i]);
		 s.transition.wet[1] = lookupState(stateTransitionWet[i]);
		 s.transition.motion[0] = lookupState(stateTransitionNoMotion[i]);
		 s.transition.motion[1] = lookupState(stateTransitionMotion[i]);
		 s.transition.trigger[0] = lookupState(stateTransitionTriggerUp[i]);
		 s.transition.trigger[1] = lookupState(stateTransitionTriggerDown[i]);

		 s.transition.genericTrigger[0][0] = lookupState(stateTransitionGeneric0Out[i]);
		 s.transition.genericTrigger[0][1] = lookupState(stateTransitionGeneric0In[i]);
		 s.transition.genericTrigger[1][0] = lookupState(stateTransitionGeneric1Out[i]);
		 s.transition.genericTrigger[1][1] = lookupState(stateTransitionGeneric1In[i]);
		 s.transition.genericTrigger[2][0] = lookupState(stateTransitionGeneric2Out[i]);
		 s.transition.genericTrigger[2][1] = lookupState(stateTransitionGeneric2In[i]);
		 s.transition.genericTrigger[3][0] = lookupState(stateTransitionGeneric3Out[i]);
		 s.transition.genericTrigger[3][1] = lookupState(stateTransitionGeneric3In[i]);

		 s.transition.altTrigger[0] = lookupState(stateTransitionAltTriggerUp[i]);
		 s.transition.altTrigger[1] = lookupState(stateTransitionAltTriggerDown[i]);
		 s.transition.timeout = lookupState(stateTransitionTimeout[i]);
		 s.waitForTimeout = stateWaitForTimeout[i];
		 s.timeoutValue = stateTimeoutValue[i];
		 s.fire = stateFire[i];
		 s.altFire = stateAlternateFire[i];
		 s.reload = stateReload[i];
		 s.ejectShell = stateEjectShell[i];
		 s.energyDrain = stateEnergyDrain[i];
		 s.allowImageChange = stateAllowImageChange[i];
		 s.scaleAnimation = stateScaleAnimation[i];
		 s.sequenceTransitionIn = stateSequenceTransitionIn[i];
		 s.sequenceTransitionOut = stateSequenceTransitionOut[i];
		 s.sequenceNeverTransition = stateSequenceNeverTransition[i];
		 s.sequenceTransitionTime = stateSequenceTransitionTime[i];
		 s.direction = stateDirection[i];
		 s.loaded = stateLoaded[i];
		 s.spin = stateSpin[i];
		 s.recoil = stateRecoil[i];
		 s.sequence    = -1; // Sequence is resolved in load
		 s.shapeSequence = stateShapeSequence[i];
		 s.shapeSequenceScale = stateScaleShapeSequence[i];
		 s.sequenceVis = -1; // Vis Sequence is resolved in load
		 s.sound = stateSound[i];
		 s.script = stateScript[i];
		 s.emitter = stateEmitter[i];
		 s.emitterTime = stateEmitterTime[i];
		 s.emitterNode = -1; // Sequnce is resolved in load
	  }

	  // The first state marked as "fire" is the state entered on the
	  // client when it recieves a fire event.
	  if (s.fire && fireState == -1)
		 fireState = i;

	  // The first state marked as "alternateFire" is the state entered on the
	  // client when it recieves an alternate fire event.
	  if (s.altFire && altFireState == -1)
		 altFireState = i;

	  // The first state marked as "reload" is the state entered on the
	  // client when it recieves a reload event.
	  if (s.reload && reloadState == -1)
		 reloadState = i;
   }

   // Always preload images, this is needed to avoid problems with
   // resolving sequences before transmission to a client.
   return true;
}

bool StateItemData::preload(bool server, String &errorStr)
{
   if (!Parent::preload(server, errorStr))
      return false;

   // Resolve objects transmitted from server
   if (!server) {
      if (projectile)
         if (Sim::findObject(SimObjectId(projectile), projectile) == false)
            Con::errorf(ConsoleLogEntry::General, "Error, unable to load projectile for StateItemData");

	  for (U32 i = 0; i < MaxStates; i++) {
		 if (state[i].emitter)
			if (!Sim::findObject(SimObjectId(state[i].emitter), state[i].emitter))
			   Con::errorf(ConsoleLogEntry::General, "Error, unable to load emitter for image datablock");
	           
		 String str;
		 if( !sfxResolve( &state[ i ].sound, str ) )
			Con::errorf( ConsoleLogEntry::General, str.c_str() );
	   }
   }

   // Use the first person eye offset if it's set.
   useEyeOffset = !eyeOffset.isIdentity();

   if (shapeName && shapeName[0]) {
      // Resolve shapename
      mShape = ResourceManager::get().load(shapeName);
      if (!bool(mShape)) {
         errorStr = String::ToString("Unable to load shape: %s", shapeName);
         return false;
      }
      if(computeCRC)
      {
         Con::printf("Validation required for shape: %s", shapeName);

         Torque::FS::FileNodeRef    fileRef = Torque::FS::GetFileNode(mShape.getPath());

         if (!fileRef)
            return false;

         if(server)
            mCRC = fileRef->getChecksum();
         else if(mCRC != fileRef->getChecksum())
         {
            errorStr = String::ToString("Shape \"%s\" does not match version on server.",shapeName);
            return false;
         }
      }

      // Resolve nodes & build mount transform
	  eyeMountNode = mShape->findNode("eyeMount");
      eyeNode = mShape->findNode("eye");
      ejectNode = mShape->findNode("ejectPoint");
      muzzleNode = mShape->findNode("muzzlePoint");
      retractNode = mShape->findNode("retractionPoint");
      mountTransform = mountOffset;
      S32 node = mShape->findNode("mountPoint");
      if (node != -1) {
         MatrixF total(1);
         do {
            MatrixF nmat;
            QuatF q;
            TSTransform::setMatrix(mShape->defaultRotations[node].getQuatF(&q),mShape->defaultTranslations[node],&nmat);
            total.mul(nmat);
            node = mShape->nodes[node].parentIndex;
         }
         while(node != -1);
         total.inverse();
         mountTransform.mul(total);
      }

      // Resolve state sequence names & emitter nodes
      isAnimated = false;
      hasFlash = false;
	  for (U32 i = 0; i < MaxStates; i++) {
		 StateData& s = state[i];
		 if (stateSequence[i] && stateSequence[i][0])
			s.sequence = mShape->findSequence(stateSequence[i]);
		 if (s.sequence != -1)
		 {
			isAnimated = true;
		 }

		 if (stateSequence[i] && stateSequence[i][0] && stateSequenceRandomFlash[i]) {
			char bufferVis[128];
			dStrncpy(bufferVis, stateSequence[i], 100);
			dStrcat(bufferVis, "_vis");
			s.sequenceVis = mShape->findSequence(bufferVis);
		 }
		 if (s.sequenceVis != -1)
		 {
			s.flashSequence = true;
			hasFlash = true;
		 }
		 s.ignoreLoadedForReady = stateIgnoreLoadedForReady[i];

		 if (stateEmitterNode[i] && stateEmitterNode[i][0])
			s.emitterNode = mShape->findNode(stateEmitterNode[i]);
		 if (s.emitterNode == -1)
			s.emitterNode = muzzleNode;
	  }
      ambientSequence = mShape->findSequence("ambient");
      spinSequence = mShape->findSequence("spin");
   }
   else {
      errorStr = "Bad Datablock from server";
      return false;
   }

   if( !casing && casingID != 0 )
   {
      if( !Sim::findObject( SimObjectId( casingID ), casing ) )
      {
         Con::errorf( ConsoleLogEntry::General, "StateItemData::preload: Invalid packet, bad datablockId(casing): 0x%x", casingID );
      }
   }


   /*TSShapeInstance* pDummy = new TSShapeInstance(shape, !server);
   delete pDummy;*/
   return true;
}

S32 StateItemData::lookupState(const char* name)
{
   if (!name || !name[0])
      return -1;
   for (U32 i = 0; i < MaxStates; i++)
      if (stateName[i] && !dStricmp(name,stateName[i]))
         return i;
   Con::errorf(ConsoleLogEntry::General,"StateItemData:: Could not resolve state \"%s\" for image \"%s\"",name,getName());
   return 0;
}

ImplementEnumType( StateItemLightType,
   "The type of light to attach to this ShapeBase\n"
   "@ingroup gameObjects\n\n")
	{ StateItem::NoLight,           "NoLight", "No light is attached.\n" },
   { StateItem::ConstantLight,     "ConstantLight", "A constant emitting light is attached.\n" },
   { StateItem::SpotLight,         "SpotLight", "A spotlight is attached.\n" },
   { StateItem::PulsingLight,      "PulsingLight", "A pusling light is attached.\n" },
   { StateItem::WeaponFireLight,   "WeaponFireLight", "Light emits when the weapon is fired the dissipates.\n" }
EndImplementEnumType;

/*ImplementEnumType( StateItemLightType,
   "The type of light that this StateItem has\n"
   "@ingroup gameobjects\n\n")
   { StateItem::NoLight,           "NoLight", "The StateItem has no light attached.\n" },
   { StateItem::ConstantLight,     "ConstantLight", "The StateItem has a constantly emitting light attached.\n" },
   { StateItem::PulsingLight,      "PulsingLight", "The StateItem has a pulsing light attached.\n" }
EndImplementEnumType;*/
//-JR

void StateItemData::initPersistFields()
{
   addField("friction",          TypeF32,       Offset(friction,           StateItemData), "A floating-point value specifying how much velocity is lost to impact and sliding friction.");
   addField("elasticity",        TypeF32,       Offset(elasticity,         StateItemData), "A floating-point value specifying how 'bouncy' this StateItemData is.");
   addField("sticky",            TypeBool,      Offset(sticky,             StateItemData), "If true, StateItemData will 'stick' to any surface it collides with.");
   addField("gravityMod",        TypeF32,       Offset(gravityMod,         StateItemData), "Floating point value to multiply the existing gravity with for just this StateItemData.");
   addField("maxVelocity",       TypeF32,       Offset(maxVelocity,        StateItemData), "Maximum velocity that this StateItemData is able to move.");
   addField("dynamicType",       TypeS32,       Offset(dynamicTypeField,   StateItemData), "An integer value which, if speficied, is added to the value retured by getType().");

   addField("lightType",         TYPEID< StateItem::LightType >(),      Offset(lightType, StateItemData), "Type of light to apply to this StateItemData. Options are NoLight, ConstantLight, PulsingLight. Default is NoLight." );
   addField("lightColor",        TypeColorF,    Offset(lightColor,         StateItemData), "Color value to make this light. Example: \"1.0,1.0,1.0\"");
   addField("lightTime",         TypeS32,       Offset(lightTime,          StateItemData), "Time value for the light of this StateItemData, used to control the pulse speed of the PulsingLight LightType.");
   addField("lightRadius",       TypeF32,       Offset(lightRadius,        StateItemData), "Distance from the center point of this StateItemData for the light to affect");
   addField("lightOnlyStatic",   TypeBool,      Offset(lightOnlyStatic,    StateItemData), "If true, this StateItemData will only cast a light if the StateItem for this StateItemData has a Static value of true.");

   addField("simpleServerCollision",   TypeBool,  Offset(simpleServerCollision,    StateItemData), 
      "@brief Determines if only simple server-side collision will be used (for pick ups).\n\n"
      "If set to true then only simple, server-side collision detection will be used.  This is often the case "
      "if the StateItem is used for a pick up object, such as ammo.  If set to false then a full collision volume "
      "will be used as defined by the shape.  The default is true.\n"
      "@note Only applies when using a physics library.\n"
      "@see TurretShape and ProximityMine for examples that should set this to false to allow them to be "
      "shot by projectiles.\n");

   //-JR
   //advanced StateItem support
   addField( "emap", TypeBool, Offset(emap, StateItemData),
      "Whether to enable environment mapping on this " );

   addField( "shapeFile", TypeShapeFilename, Offset(shapeName, StateItemData),
      "The DTS or DAE model to use for this Image." );

   addField( "itemAnimPrefix", TypeCaseString, Offset(itemAnimPrefix, StateItemData),
      "@brief Passed along to the mounting shape to modify animation sequences played in third person. [optional]\n\n" );

   addField( "animateOnServer", TypeBool, Offset(animateOnServer, StateItemData),
      "@brief Indicates that the image should be animated on the server.\n\n"
      "In most cases you'll want this set if you're using useEyeNode.  You may also want to "
      "set this if the muzzlePoint is animated while it shoots.  You can set this "
      "to false even if these previous cases are true if the image's shape is set "
      "up in the correct position and orientation in the 'root' pose and none of "
      "the nodes are animated at key times, such as the muzzlePoint essentially "
      "remaining at the same position at the start of the fire state (it could "
      "animate just fine after the projectile is away as the muzzle vector is only "
      "calculated at the start of the state).\n\n"
      "You'll also want to set this to true if you're animating the camera using the "
      "image's 'eye' node -- unless the movement is very subtle and doesn't need to "
      "be reflected on the server.\n\n"
      "@note Setting this to true causes up to four animation threads to be advanced on the server "
      "for each instance in use, although for most images only one or two are actually defined.\n\n"
      "@see useEyeNode\n");

   addField( "scriptAnimTransitionTime", TypeF32, Offset(scriptAnimTransitionTime, StateItemData),
      "@brief The amount of time to transition between the previous sequence and new sequence when the script prefix has changed.\n\n"
      "When setImageScriptAnimPrefix() is used on a ShapeBase that has this image mounted, the image "
      "will attempt to switch to the new animation sequence based on the given script prefix.  This is "
      "the amount of time it takes to transition from the previously playing animation sequence to"
      "the new script prefix-based animation sequence.\n"
      "@see ShapeBase::setImageScriptAnimPrefix()");

   addField( "projectile", TYPEID< ProjectileData >(), Offset(projectile, StateItemData),
      "The projectile fired by this Image" );

   addField( "cloakable", TypeBool, Offset(cloakable, StateItemData),
      "Whether this Image can be cloaked.\nCurrently unused." );

   addField( "mountPoint", TypeS32, Offset(mountPoint, StateItemData),
      "Mount node # to mount this Image to.\nThis should correspond to a mount# "
      "node on the ShapeBase derived object we are mounting to." );
   addField( "offset", TypeMatrixPosition, Offset(mountOffset, StateItemData),
      "\"X Y Z\" translation offset from this Image's <i>mountPoint</i> node to "
      "attach to.\nDefaults to \"0 0 0\". ie. attach this Image's "
      "<i>mountPoint</i> node to the ShapeBase model's mount# node." );
   addField( "rotation", TypeMatrixRotation, Offset(mountOffset, StateItemData),
      "\"X Y Z ANGLE\" rotation offset from this Image's <i>mountPoint</i> node "
      "to attach to.\nDefaults to \"0 0 0\". ie. attach this Image's "
      "<i>mountPoint</i> node to the ShapeBase model's mount# node." );
   addField( "eyeOffset", TypeMatrixPosition, Offset(eyeOffset, StateItemData),
      "\"X Y Z\" translation offset from the ShapeBase model's eye node.\n"
      "Only affects 1st person POV." );
   addField( "eyeRotation", TypeMatrixRotation, Offset(eyeOffset, StateItemData),
      "\"X Y Z ANGLE\" rotation offset from the ShapeBase model's eye node.\n"
      "Only affects 1st person POV." );

   addField( "useEyeNode", TypeBool, Offset(useEyeNode, StateItemData),
      "@brief Mount image using image's eyeMount node and place the camera at the image's eye node (or "
      "at the eyeMount node if the eye node is missing).\n\n"
      "When in first person view, if an 'eyeMount' node is present in the image's shape, this indicates "
      "that the image should mount eyeMount node to Player eye node for image placement.  The "
      "Player's camera should also mount to the image's eye node to inherit any animation (or the eyeMount "
      "node if the image doesn't have an eye node).\n\n"
      "@note Used instead of eyeOffset.\n\n"
      "@note Read about the animateOnServer field as you may want to set it to true if you're using useEyeNode.\n\n"
      "@see eyeOffset\n\n"
      "@see animateOnServer\n\n");

   addField( "correctMuzzleVector", TypeBool,  Offset(correctMuzzleVector, StateItemData),
      "Flag to adjust the aiming vector to the eye's LOS point.\n\n"
      "@see getMuzzleVector" );

   addField( "correctMuzzleVectorTP", TypeBool,  Offset(correctMuzzleVectorTP, StateItemData),
      "@brief Flag to adjust the aiming vector to the camera's LOS point when in 3rd person view.\n\n"
      "@see ShapeBase::getMuzzleVector()" );

   addField( "firstPerson", TypeBool, Offset(firstPerson, StateItemData),
      "This flag must be set for the adjusted LOS muzzle vector to be computed.\n\n"
      "@see getMuzzleVector" );
   addField( "mass", TypeF32, Offset(mass, StateItemData),
      "Mass of this \nThis is added to the total mass of the ShapeBase "
      "object." );

   addField( "usesEnergy", TypeBool, Offset(usesEnergy,StateItemData),
      "Flag indicating whether this Image uses energy instead of ammo." );
   addField( "minEnergy", TypeF32, Offset(minEnergy, StateItemData),
      "Minimum Image energy for it to be operable." );
   addField( "accuFire", TypeBool, Offset(accuFire, StateItemData),
      "Flag to control whether the Image's aim is automatically converged with "
      "the crosshair.\nCurrently unused." );

   addField( "lightColor", TypeColorF, Offset(lightColor, StateItemData),
      "The color of light this Image emits." );
   addField( "lightDuration", TypeS32, Offset(lightDuration, StateItemData),
      "Duration in SimTime of Pulsing and WeaponFire type lights." );
   addField( "lightRadius", TypeF32, Offset(lightRadius, StateItemData),
      "Radius of the light this Image emits." );
   addField( "lightBrightness", TypeF32, Offset(lightBrightness, StateItemData),
      "Brightness of the light this Image emits.\nOnly valid for WeaponFireLight." );

   addField( "shakeCamera", TypeBool, Offset(shakeCamera, StateItemData),
      "Flag indicating whether the camera should shake when this Image fires.\n"
      "@note Camera shake only works properly if the player is in control of "
      "the one and only shapeBase object in the scene which fires an Image that "
      "uses camera shake." );
   addField( "camShakeFreq", TypePoint3F, Offset(camShakeFreq, StateItemData),
      "Frequency of the camera shaking effect.\n\n@see shakeCamera" );
   addField( "camShakeAmp", TypePoint3F, Offset(camShakeAmp, StateItemData),
      "Amplitude of the camera shaking effect.\n\n@see shakeCamera" );

   addField( "casing", TYPEID< DebrisData >(), Offset(casing, StateItemData),
      "DebrisData datablock to use for ejected casings.\n\n@see stateEjectShell" );
   addField( "shellExitDir", TypePoint3F, Offset(shellExitDir, StateItemData),
      "Vector direction to eject shell casings." );
   addField( "shellExitVariance", TypeF32, Offset(shellExitVariance, StateItemData),
      "Variance (in degrees) from the shellExitDir vector." );
   addField( "shellVelocity", TypeF32, Offset(shellVelocity, StateItemData),
      "Speed at which to eject casings." );

   // State arrays
   addArray( "States", MaxStates );

      addField( "stateName", TypeCaseString, Offset(stateName, StateItemData), MaxStates,
         "Name of this state." );
      addField( "stateTransitionOnLoaded", TypeString, Offset(stateTransitionLoaded, StateItemData), MaxStates,
         "Name of the state to transition to when the loaded state of the Image "
         "changes to 'Loaded'." );
      addField( "stateTransitionOnNotLoaded", TypeString, Offset(stateTransitionNotLoaded, StateItemData), MaxStates,
         "Name of the state to transition to when the loaded state of the Image "
         "changes to 'Empty'." );
      addField( "stateTransitionOnAmmo", TypeString, Offset(stateTransitionAmmo, StateItemData), MaxStates,
         "Name of the state to transition to when the ammo state of the Image "
         "changes to true." );
      addField( "stateTransitionOnNoAmmo", TypeString, Offset(stateTransitionNoAmmo, StateItemData), MaxStates,
         "Name of the state to transition to when the ammo state of the Image "
         "changes to false." );
      addField( "stateTransitionOnTarget", TypeString, Offset(stateTransitionTarget, StateItemData), MaxStates,
         "Name of the state to transition to when the Image gains a target." );
      addField( "stateTransitionOnNoTarget", TypeString, Offset(stateTransitionNoTarget, StateItemData), MaxStates,
         "Name of the state to transition to when the Image loses a target." );
      addField( "stateTransitionOnWet", TypeString, Offset(stateTransitionWet, StateItemData), MaxStates,
         "Name of the state to transition to when the Image enters the water." );
      addField( "stateTransitionOnNotWet", TypeString, Offset(stateTransitionNotWet, StateItemData), MaxStates,
         "Name of the state to transition to when the Image exits the water." );
      addField( "stateTransitionOnMotion", TypeString, Offset(stateTransitionMotion, StateItemData), MaxStates,
         "Name of the state to transition to when the Player moves." );
      addField( "stateTransitionOnNoMotion", TypeString, Offset(stateTransitionNoMotion, StateItemData), MaxStates,
         "Name of the state to transition to when the Player stops moving." );
      addField( "stateTransitionOnTriggerUp", TypeString, Offset(stateTransitionTriggerUp, StateItemData), MaxStates,
         "Name of the state to transition to when the trigger state of the Image "
         "changes to true (fire button down)." );
      addField( "stateTransitionOnTriggerDown", TypeString, Offset(stateTransitionTriggerDown, StateItemData), MaxStates,
         "Name of the state to transition to when the trigger state of the Image "
         "changes to false (fire button released)." );
      addField( "stateTransitionOnAltTriggerUp", TypeString, Offset(stateTransitionAltTriggerUp, StateItemData), MaxStates,
         "Name of the state to transition to when the alt trigger state of the "
         "Image changes to true (alt fire button down)." );
      addField( "stateTransitionOnAltTriggerDown", TypeString, Offset(stateTransitionAltTriggerDown, StateItemData), MaxStates,
         "Name of the state to transition to when the alt trigger state of the "
         "Image changes to false (alt fire button up)." );
      addField( "stateTransitionOnTimeout", TypeString, Offset(stateTransitionTimeout, StateItemData), MaxStates,
         "Name of the state to transition to when we have been in this state "
         "for stateTimeoutValue seconds." );

      addField( "stateTransitionGeneric0In", TypeString, Offset(stateTransitionGeneric0In, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 0 state "
         "changes to true." );
      addField( "stateTransitionGeneric0Out", TypeString, Offset(stateTransitionGeneric0Out, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 0 state "
         "changes to false." );
      addField( "stateTransitionGeneric1In", TypeString, Offset(stateTransitionGeneric1In, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 1 state "
         "changes to true." );
      addField( "stateTransitionGeneric1Out", TypeString, Offset(stateTransitionGeneric1Out, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 1 state "
         "changes to false." );
      addField( "stateTransitionGeneric2In", TypeString, Offset(stateTransitionGeneric2In, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 2 state "
         "changes to true." );
      addField( "stateTransitionGeneric2Out", TypeString, Offset(stateTransitionGeneric2Out, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 2 state "
         "changes to false." );
      addField( "stateTransitionGeneric3In", TypeString, Offset(stateTransitionGeneric3In, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 3 state "
         "changes to true." );
      addField( "stateTransitionGeneric3Out", TypeString, Offset(stateTransitionGeneric3Out, StateItemData), MaxStates,
         "Name of the state to transition to when the generic trigger 3 state "
         "changes to false." );

      addField( "stateTimeoutValue", TypeF32, Offset(stateTimeoutValue, StateItemData), MaxStates,
         "Time in seconds to wait before transitioning to stateTransitionOnTimeout." );
      addField( "stateWaitForTimeout", TypeBool, Offset(stateWaitForTimeout, StateItemData), MaxStates,
         "If false, this state ignores stateTimeoutValue and transitions "
         "immediately if other transition conditions are met." );
      addField( "stateFire", TypeBool, Offset(stateFire, StateItemData), MaxStates,
         "The first state with this set to true is the state entered by the "
         "client when it receives the 'fire' event." );

      addField( "stateAlternateFire", TypeBool, Offset(stateAlternateFire, StateItemData), MaxStates,
         "The first state with this set to true is the state entered by the "
         "client when it receives the 'altFire' event." );
      addField( "stateReload", TypeBool, Offset(stateReload, StateItemData), MaxStates,
         "The first state with this set to true is the state entered by the "
         "client when it receives the 'reload' event." );

      addField( "stateEjectShell", TypeBool, Offset(stateEjectShell, StateItemData), MaxStates,
         "If true, a shell casing will be ejected in this state." );
      addField( "stateEnergyDrain", TypeF32, Offset(stateEnergyDrain, StateItemData), MaxStates,
         "Amount of energy to subtract from the Image in this state.\n"
         "Energy is drained at stateEnergyDrain units/sec as long as we are in "
         "this state." );
      addField( "stateAllowImageChange", TypeBool, Offset(stateAllowImageChange, StateItemData), MaxStates,
         "If false, other Images will temporarily be blocked from mounting "
         "while the state machine is executing the tasks in this state.\n"
         "For instance, if we have a rocket launcher, the player shouldn't "
         "be able to switch out <i>while</i> firing. So, you'd set "
         "stateAllowImageChange to false in firing states, and true the rest "
         "of the time." );
      addField( "stateDirection", TypeBool, Offset(stateDirection, StateItemData), MaxStates,
         "Direction of the animation to play in this state.\nTrue is forward, "
         "false is backward." );
      addField( "stateLoadedFlag", TYPEID< StateItemData::StateData::LoadedState >(), Offset(stateLoaded, StateItemData), MaxStates,
         "Set the loaded state of the \n"
         "<ul><li>IgnoreLoaded: Don't change Image loaded state.</li>"
         "<li>Loaded: Set Image loaded state to true.</li>"
         "<li>NotLoaded: Set Image loaded state to false.</li></ul>" );
      addField( "stateSpinThread", TYPEID< StateItemData::StateData::SpinState >(), Offset(stateSpin, StateItemData), MaxStates,
         "Controls how fast the 'spin' animation sequence will be played in "
         "this state.\n"
         "<ul><li>Ignore: No change to the spin sequence.</li>"
         "<li>Stop: Stops the spin sequence at its current position.</li>"
         "<li>SpinUp: Increase spin sequence timeScale from 0 (on state entry) "
         "to 1 (after stateTimeoutValue seconds).</li>"
         "<li>SpinDown: Decrease spin sequence timeScale from 1 (on state entry) "
         "to 0 (after stateTimeoutValue seconds).</li>"
         "<li>FullSpeed: Resume the spin sequence playback at its current "
         "position with timeScale=1.</li></ul>" );
      addField( "stateRecoil", TYPEID< StateItemData::StateData::RecoilState >(), Offset(stateRecoil, StateItemData), MaxStates,
         "Type of recoil sequence to play on the ShapeBase object on entry to "
         "this state.\n"
         "<ul><li>NoRecoil: Do not play a recoil sequence.</li>"
         "<li>LightRecoil: Play the light_recoil sequence.</li>"
         "<li>MediumRecoil: Play the medium_recoil sequence.</li>"
         "<li>HeavyRecoil: Play the heavy_recoil sequence.</li></ul>" );
      addField( "stateSequence", TypeString, Offset(stateSequence, StateItemData), MaxStates,
         "Name of the sequence to play on entry to this state." );
      addField( "stateSequenceRandomFlash", TypeBool, Offset(stateSequenceRandomFlash, StateItemData), MaxStates,
         "If true, a random frame from the muzzle flash sequence will be "
         "displayed each frame.\n"
         "The name of the muzzle flash sequence is the same as stateSequence, "
         "with \"_vis\" at the end." );
      addField( "stateScaleAnimation", TypeBool, Offset(stateScaleAnimation, StateItemData), MaxStates,
         "If true, the timeScale of the stateSequence animation will be adjusted "
         "such that the sequence plays for stateTimeoutValue seconds. " );
      addField( "stateSequenceTransitionIn", TypeBool, Offset(stateSequenceTransitionIn, StateItemData), MaxStates,
         "Do we transition to the state's sequence when we enter the state?" );
      addField( "stateSequenceTransitionOut", TypeBool, Offset(stateSequenceTransitionOut, StateItemData), MaxStates,
         "Do we transition to the new state's sequence when we leave the state?" );
      addField( "stateSequenceNeverTransition", TypeBool, Offset(stateSequenceNeverTransition, StateItemData), MaxStates,
         "Never allow a transition to this sequence.  Often used for a fire sequence." );
      addField( "stateSequenceTransitionTime", TypeF32, Offset(stateSequenceTransitionTime, StateItemData), MaxStates,
         "The time to transition in or out of a sequence." );

      addField( "stateShapeSequence", TypeString, Offset(stateShapeSequence, StateItemData), MaxStates,
         "Name of the sequence that is played on the mounting shape." );
      addField( "stateScaleShapeSequence", TypeBool, Offset(stateScaleShapeSequence, StateItemData), MaxStates,
         "Indicates if the sequence to be played on the mounting shape should be scaled to the length of the state." );


      addField( "stateSound", TypeSFXTrackName, Offset(stateSound, StateItemData), MaxStates,
         "Sound to play on entry to this state." );
      addField( "stateScript", TypeCaseString, Offset(stateScript, StateItemData), MaxStates,
         "Method to execute on entering this state.\n"
         "Scoped to this image class name, then StateItemData. The script "
         "callback function takes the same arguments as the onMount callback." );
      addField( "stateEmitter", TYPEID< ParticleEmitterData >(), Offset(stateEmitter, StateItemData), MaxStates,
         "Emitter to generate particles in this state (from muzzle point or "
         "specified node).\n\n@see stateEmitterNode" );
      addField( "stateEmitterTime", TypeF32, Offset(stateEmitterTime, StateItemData), MaxStates,
         "How long (in seconds) to emit particles on entry to this state." );
      addField( "stateEmitterNode", TypeString, Offset(stateEmitterNode, StateItemData), MaxStates,
         "Name of the node to emit particles from.\n\n@see stateEmitter" );
      addField( "stateIgnoreLoadedForReady", TypeBool, Offset(stateIgnoreLoadedForReady, StateItemData), MaxStates,
         "If set to true, and both ready and loaded transitions are true, the "
         "ready transition will be taken instead of the loaded transition.\n"
         "A state is 'ready' if pressing the fire trigger in that state would "
         "transition to the fire state." );

   endArray( "States" );

   addField( "computeCRC", TypeBool, Offset(computeCRC, StateItemData),
      "If true, verify that the CRC of the client's Image matches the server's "
      "CRC for the Image when loaded by the client." );

   addField( "maxConcurrentSounds", TypeS32, Offset(maxConcurrentSounds, StateItemData),
      "Maximum number of sounds this Image can play at a time.\n"
      "Any value <= 0 indicates that it can play an infinite number of sounds." );

   addField( "useRemainderDT", TypeBool, Offset(useRemainderDT, StateItemData), 
      "If true, allow multiple timeout transitions to occur within a single "
      "tick (useful if states have a very small timeout)." );
   //-JR

   Parent::initPersistFields();
}

void StateItemData::packData(BitStream* stream)
{
   Parent::packData(stream);
   stream->writeFloat(friction, 10);
   stream->writeFloat(elasticity, 10);
   stream->writeFlag(sticky);
   if(stream->writeFlag(gravityMod != 1.0))
      stream->writeFloat(gravityMod, 10);
   if(stream->writeFlag(maxVelocity != -1))
      stream->write(maxVelocity);

   if(stream->writeFlag(lightType != StateItem::NoLight))
   {
      AssertFatal(StateItem::NumLightTypes < (1 << 2), "StateItemData: light type needs more bits");
      stream->writeInt(lightType, 2);
      stream->writeFloat(lightColor.red, 7);
      stream->writeFloat(lightColor.green, 7);
      stream->writeFloat(lightColor.blue, 7);
      stream->writeFloat(lightColor.alpha, 7);
      stream->write(lightTime);
      stream->write(lightRadius);
      stream->writeFlag(lightOnlyStatic);
   }
   
   stream->writeFlag(simpleServerCollision);

   //-JR
   //advanced StateItem support
   if(stream->writeFlag(computeCRC))
      stream->write(mCRC);

   stream->writeString(itemAnimPrefix);

   stream->writeString(shapeName);
   stream->write(mountPoint);
   if (!stream->writeFlag(mountOffset.isIdentity()))
      stream->writeAffineTransform(mountOffset);
   if (!stream->writeFlag(eyeOffset.isIdentity()))
      stream->writeAffineTransform(eyeOffset);

   stream->writeFlag(animateOnServer);

   stream->write(scriptAnimTransitionTime);

   stream->writeFlag(useEyeNode);

   stream->writeFlag(correctMuzzleVector);
   stream->writeFlag(correctMuzzleVectorTP);
   stream->writeFlag(firstPerson);
   stream->write(mass);
   stream->writeFlag(usesEnergy);
   stream->write(minEnergy);
   stream->writeFlag(hasFlash);
   // Client doesn't need accuFire

   // Write the projectile datablock
   if (stream->writeFlag(projectile))
      stream->writeRangedU32(packed? SimObjectId(projectile):
                             projectile->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);

   stream->writeFlag(cloakable);

   if ( stream->writeFlag( shakeCamera ) )
   {      
      mathWrite( *stream, camShakeFreq );
      mathWrite( *stream, camShakeAmp );
   }

   mathWrite( *stream, shellExitDir );
   stream->write(shellExitVariance);
   stream->write(shellVelocity);

   if( stream->writeFlag( casing ) )
   {
      stream->writeRangedU32(packed? SimObjectId(casing):
         casing->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);
   }

   for (U32 i = 0; i < MaxStates; i++)
	  if (stream->writeFlag(state[i].name && state[i].name[0])) 
	  {
		 StateData& s = state[i];
		 // States info not needed on the client:
		 //    s.allowImageChange
		 //    s.scriptNames
		 // Transitions are inc. one to account for -1 values
		 stream->writeString(state[i].name);

		 stream->writeInt(s.transition.loaded[0]+1,NumStateBits);
		 stream->writeInt(s.transition.loaded[1]+1,NumStateBits);
		 stream->writeInt(s.transition.ammo[0]+1,NumStateBits);
		 stream->writeInt(s.transition.ammo[1]+1,NumStateBits);
		 stream->writeInt(s.transition.target[0]+1,NumStateBits);
		 stream->writeInt(s.transition.target[1]+1,NumStateBits);
		 stream->writeInt(s.transition.wet[0]+1,NumStateBits);
		 stream->writeInt(s.transition.wet[1]+1,NumStateBits);
		 stream->writeInt(s.transition.trigger[0]+1,NumStateBits);
		 stream->writeInt(s.transition.trigger[1]+1,NumStateBits);
		 stream->writeInt(s.transition.altTrigger[0]+1,NumStateBits);
		 stream->writeInt(s.transition.altTrigger[1]+1,NumStateBits);
		 stream->writeInt(s.transition.timeout+1,NumStateBits);

        // Most states don't make use of the motion transition.
        if (stream->writeFlag(s.transition.motion[0] != -1 || s.transition.motion[1] != -1))
        {
			// This state does
			stream->writeInt(s.transition.motion[0]+1,NumStateBits);
			stream->writeInt(s.transition.motion[1]+1,NumStateBits);
		 }

		 // Most states don't make use of the generic trigger transitions.  Don't transmit
		 // if that is the case here.
		 for (U32 j=0; j<MaxGenericTriggers; ++j)
		 {
			if (stream->writeFlag(s.transition.genericTrigger[j][0] != -1 || s.transition.genericTrigger[j][1] != -1))
			{
			   stream->writeInt(s.transition.genericTrigger[j][0]+1,NumStateBits);
			   stream->writeInt(s.transition.genericTrigger[j][1]+1,NumStateBits);
			}
		 }

		 if(stream->writeFlag(s.timeoutValue != gDefaultStateData.timeoutValue))
			stream->write(s.timeoutValue);

		 stream->writeFlag(s.waitForTimeout);
		 stream->writeFlag(s.fire);
         stream->writeFlag(s.altFire);
         stream->writeFlag(s.reload);
		 stream->writeFlag(s.ejectShell);
		 stream->writeFlag(s.scaleAnimation);
		 stream->writeFlag(s.direction);
         stream->writeFlag(s.sequenceTransitionIn);
		 stream->writeFlag(s.sequenceTransitionOut);
		 stream->writeFlag(s.sequenceNeverTransition);
		 if(stream->writeFlag(s.sequenceTransitionTime != gDefaultStateData.sequenceTransitionTime))
			stream->write(s.sequenceTransitionTime);

		 stream->writeString(s.shapeSequence);
		 stream->writeFlag(s.shapeSequenceScale);
		 if(stream->writeFlag(s.energyDrain != gDefaultStateData.energyDrain))
			stream->write(s.energyDrain);

		 stream->writeInt(s.loaded,StateData::NumLoadedBits);
		 stream->writeInt(s.spin,StateData::NumSpinBits);
		 stream->writeInt(s.recoil,StateData::NumRecoilBits);
		 if(stream->writeFlag(s.sequence != gDefaultStateData.sequence))
			stream->writeSignedInt(s.sequence, 16);

		 if(stream->writeFlag(s.sequenceVis != gDefaultStateData.sequenceVis))
			stream->writeSignedInt(s.sequenceVis,16);
		 stream->writeFlag(s.flashSequence);
		 stream->writeFlag(s.ignoreLoadedForReady);

		 if (stream->writeFlag(s.emitter)) {
			stream->writeRangedU32(packed? SimObjectId(s.emitter):
								   s.emitter->getId(),DataBlockObjectIdFirst,DataBlockObjectIdLast);
			stream->write(s.emitterTime);
			stream->write(s.emitterNode);
		 }

		 sfxWrite( stream, s.sound );
	  }
   stream->write(maxConcurrentSounds);
   stream->writeFlag(useRemainderDT);
   //-JR
}

void StateItemData::unpackData(BitStream* stream)
{
   Parent::unpackData(stream);
   friction = stream->readFloat(10);
   elasticity = stream->readFloat(10);
   sticky = stream->readFlag();
   if(stream->readFlag())
      gravityMod = stream->readFloat(10);
   else
      gravityMod = 1.0;

   if(stream->readFlag())
      stream->read(&maxVelocity);
   else
      maxVelocity = -1;

   if(stream->readFlag())
   {
      lightType = stream->readInt(2);
      lightColor.red = stream->readFloat(7);
      lightColor.green = stream->readFloat(7);
      lightColor.blue = stream->readFloat(7);
      lightColor.alpha = stream->readFloat(7);
      stream->read(&lightTime);
      stream->read(&lightRadius);
      lightOnlyStatic = stream->readFlag();
   }
   else
      lightType = StateItem::NoLight;

   simpleServerCollision = stream->readFlag();

   //-JR
   //advanced StateItem support
   computeCRC = stream->readFlag();
   if(computeCRC)
      stream->read(&mCRC);

   itemAnimPrefix = stream->readSTString();

   shapeName = stream->readSTString();
   stream->read(&mountPoint);
   if (stream->readFlag())
      mountOffset.identity();
   else
      stream->readAffineTransform(&mountOffset);
   if (stream->readFlag())
      eyeOffset.identity();
   else
      stream->readAffineTransform(&eyeOffset);

   animateOnServer = stream->readFlag();

   stream->read(&scriptAnimTransitionTime);

   useEyeNode = stream->readFlag();

   correctMuzzleVector = stream->readFlag();
   correctMuzzleVectorTP = stream->readFlag();
   firstPerson = stream->readFlag();
   stream->read(&mass);
   usesEnergy = stream->readFlag();
   stream->read(&minEnergy);
   hasFlash = stream->readFlag();

   projectile = (stream->readFlag() ?
                 (ProjectileData*)stream->readRangedU32(DataBlockObjectIdFirst,
                                                        DataBlockObjectIdLast) : 0);

   cloakable = stream->readFlag();

   shakeCamera = stream->readFlag();
   if ( shakeCamera )
   {
      mathRead( *stream, &camShakeFreq );
      mathRead( *stream, &camShakeAmp );      
   }

   mathRead( *stream, &shellExitDir );
   stream->read(&shellExitVariance);
   stream->read(&shellVelocity);

   if(stream->readFlag())
   {
      casingID = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);
   }

   for (U32 i = 0; i < MaxStates; i++) {
	  if (stream->readFlag()) {
		 StateData& s = state[i];
		 // States info not needed on the client:
		 //    s.allowImageChange
		 //    s.scriptNames
		 // Transitions are dec. one to restore -1 values
		 s.name = stream->readSTString();

		 s.transition.loaded[0] = stream->readInt(NumStateBits) - 1;
		 s.transition.loaded[1] = stream->readInt(NumStateBits) - 1;
		 s.transition.ammo[0] = stream->readInt(NumStateBits) - 1;
		 s.transition.ammo[1] = stream->readInt(NumStateBits) - 1;
		 s.transition.target[0] = stream->readInt(NumStateBits) - 1;
		 s.transition.target[1] = stream->readInt(NumStateBits) - 1;
		 s.transition.wet[0] = stream->readInt(NumStateBits) - 1;
		 s.transition.wet[1] = stream->readInt(NumStateBits) - 1;
		 s.transition.trigger[0] = stream->readInt(NumStateBits) - 1;
		 s.transition.trigger[1] = stream->readInt(NumStateBits) - 1;
		 s.transition.altTrigger[0] = stream->readInt(NumStateBits) - 1;
		 s.transition.altTrigger[1] = stream->readInt(NumStateBits) - 1;
		 s.transition.timeout = stream->readInt(NumStateBits) - 1;

         // Motion trigger
		 if (stream->readFlag())
		 {
			s.transition.motion[0] = stream->readInt(NumStateBits) - 1;
			s.transition.motion[1] = stream->readInt(NumStateBits) - 1;
		 }
		 else
		 {
			s.transition.motion[0] = -1;
			s.transition.motion[1] = -1;
		 }

		 // Generic triggers
		 for (U32 j=0; j<MaxGenericTriggers; ++j)
		 {
			if (stream->readFlag())
			{
			   s.transition.genericTrigger[j][0] = stream->readInt(NumStateBits) - 1;
			   s.transition.genericTrigger[j][1] = stream->readInt(NumStateBits) - 1;
			}
			else
			{
			   s.transition.genericTrigger[j][0] = -1;
			   s.transition.genericTrigger[j][1] = -1;
			}
		 }

		 if(stream->readFlag())
			stream->read(&s.timeoutValue);
		 else
			s.timeoutValue = gDefaultStateData.timeoutValue;

		 s.waitForTimeout = stream->readFlag();
		 s.fire = stream->readFlag();
         s.altFire = stream->readFlag();
         s.reload = stream->readFlag();
		 s.ejectShell = stream->readFlag();
		 s.scaleAnimation = stream->readFlag();
		 s.direction = stream->readFlag();

		 s.sequenceTransitionIn = stream->readFlag();
		 s.sequenceTransitionOut = stream->readFlag();
		 s.sequenceNeverTransition = stream->readFlag();
		 if (stream->readFlag())
			stream->read(&s.sequenceTransitionTime);
		 else
			s.sequenceTransitionTime = gDefaultStateData.sequenceTransitionTime;

		 s.shapeSequence = stream->readSTString();
		 s.shapeSequenceScale = stream->readFlag();

		 if(stream->readFlag())
			stream->read(&s.energyDrain);
		 else
			s.energyDrain = gDefaultStateData.energyDrain;

		 s.loaded = (StateData::LoadedState)stream->readInt(StateData::NumLoadedBits);
		 s.spin = (StateData::SpinState)stream->readInt(StateData::NumSpinBits);
		 s.recoil = (StateData::RecoilState)stream->readInt(StateData::NumRecoilBits);
		 if(stream->readFlag())
			s.sequence = stream->readSignedInt(16);
		 else
			s.sequence = gDefaultStateData.sequence;

		 if(stream->readFlag())
			s.sequenceVis = stream->readSignedInt(16);
		 else
			s.sequenceVis = gDefaultStateData.sequenceVis;

		 s.flashSequence = stream->readFlag();
		 s.ignoreLoadedForReady = stream->readFlag();

		 if (stream->readFlag()) {
			s.emitter = (ParticleEmitterData*) stream->readRangedU32(DataBlockObjectIdFirst,
																	 DataBlockObjectIdLast);
			stream->read(&s.emitterTime);
			stream->read(&s.emitterNode);
		 }
		 else
			s.emitter = 0;
            
		 sfxRead( stream, &s.sound );
	  }
   }
   
   stream->read(&maxConcurrentSounds);
   useRemainderDT = stream->readFlag();

   statesLoaded = true;
   //-JR
}

//-JR
//advanced StateItem support
void StateItemData::inspectPostApply()
{
   Parent::inspectPostApply();

   addField( "emap", TypeBool, Offset(emap, StateItemData),
      "Whether to enable environment mapping on this Image." );

   addField( "shapeFile", TypeShapeFilename, Offset(shapeName, StateItemData),
      "The DTS or DAE model to use for this Image." );

   addField( "projectile", TYPEID< ProjectileData >(), Offset(projectile, StateItemData),
      "The projectile fired by this Image" );

   addField( "cloakable", TypeBool, Offset(cloakable, StateItemData),
      "Whether this Image can be cloaked.\nCurrently unused." );

   addField( "mountPoint", TypeS32, Offset(mountPoint, StateItemData),
      "Mount node # to mount this Image to.\nThis should correspond to a mount# "
      "node on the ShapeBase derived object we are mounting to." );
   addField( "offset", TypeMatrixPosition, Offset(mountOffset, StateItemData),
      "\"X Y Z\" translation offset from this Image's <i>mountPoint</i> node to "
      "attach to.\nDefaults to \"0 0 0\". ie. attach this Image's "
      "<i>mountPoint</i> node to the ShapeBase model's mount# node." );
   addField( "rotation", TypeMatrixRotation, Offset(mountOffset, StateItemData),
      "\"X Y Z ANGLE\" rotation offset from this Image's <i>mountPoint</i> node "
      "to attach to.\nDefaults to \"0 0 0\". ie. attach this Image's "
      "<i>mountPoint</i> node to the ShapeBase model's mount# node." );
   addField( "eyeOffset", TypeMatrixPosition, Offset(eyeOffset, StateItemData),
      "\"X Y Z\" translation offset from the ShapeBase model's eye node.\n"
      "Only affects 1st person POV." );
   addField( "eyeRotation", TypeMatrixRotation, Offset(eyeOffset, StateItemData),
      "\"X Y Z ANGLE\" rotation offset from the ShapeBase model's eye node.\n"
      "Only affects 1st person POV." );
   addField( "correctMuzzleVector", TypeBool,  Offset(correctMuzzleVector, StateItemData),
      "Flag to adjust the aiming vector to the eye's LOS point.\n\n"
      "@see getMuzzleVector" );
   addField( "firstPerson", TypeBool, Offset(firstPerson, StateItemData),
      "This flag must be set for the adjusted LOS muzzle vector to be computed.\n\n"
      "@see getMuzzleVector" );
   addField( "mass", TypeF32, Offset(mass, StateItemData),
      "Mass of this Image.\nThis is added to the total mass of the ShapeBase "
      "object." );

   addField( "usesEnergy", TypeBool, Offset(usesEnergy,StateItemData),
      "Flag indicating whether this Image uses energy instead of ammo." );
   addField( "minEnergy", TypeF32, Offset(minEnergy, StateItemData),
      "Minimum Image energy for it to be operable." );
   addField( "accuFire", TypeBool, Offset(accuFire, StateItemData),
      "Flag to control whether the Image's aim is automatically converged with "
      "the crosshair.\nCurrently unused." );

   /*addField( "lightType", TYPEID< StateItemData::LightType >(), Offset(lightType, StateItemData),
      "The type of light this Image emits." );*/
   addField( "lightColor", TypeColorF, Offset(lightColor, StateItemData),
      "The color of light this Image emits." );
   addField( "lightDuration", TypeS32, Offset(lightDuration, StateItemData),
      "Duration in SimTime of Pulsing and WeaponFire type lights." );
   addField( "lightRadius", TypeF32, Offset(lightRadius, StateItemData),
      "Radius of the light this Image emits." );
   addField( "lightBrightness", TypeF32, Offset(lightBrightness, StateItemData),
      "Brightness of the light this Image emits.\nOnly valid for WeaponFireLight." );

   addField( "shakeCamera", TypeBool, Offset(shakeCamera, StateItemData),
      "Flag indicating whether the camera should shake when this Image fires.\n"
      "@note Camera shake only works properly if the player is in control of "
      "the one and only shapeBase object in the scene which fires an Image that "
      "uses camera shake." );
   addField( "camShakeFreq", TypePoint3F, Offset(camShakeFreq, StateItemData),
      "Frequency of the camera shaking effect.\n\n@see shakeCamera" );
   addField( "camShakeAmp", TypePoint3F, Offset(camShakeAmp, StateItemData),
      "Amplitude of the camera shaking effect.\n\n@see shakeCamera" );

   addField( "casing", TYPEID< DebrisData >(), Offset(casing, StateItemData),
      "DebrisData datablock to use for ejected casings.\n\n@see stateEjectShell" );
   addField( "shellExitDir", TypePoint3F, Offset(shellExitDir, StateItemData),
      "Vector direction to eject shell casings." );
   addField( "shellExitVariance", TypeF32, Offset(shellExitVariance, StateItemData),
      "Variance (in degrees) from the shellExitDir vector." );
   addField( "shellVelocity", TypeF32, Offset(shellVelocity, StateItemData),
      "Speed at which to eject casings." );

   // State arrays
   addArray( "States", MaxStates );

      addField( "stateName", TypeCaseString, Offset(stateName, StateItemData), MaxStates,
         "Name of this state." );
      addField( "stateTransitionOnLoaded", TypeString, Offset(stateTransitionLoaded, StateItemData), MaxStates,
         "Name of the state to transition to when the loaded state of the Image "
         "changes to 'Loaded'." );
      addField( "stateTransitionOnNotLoaded", TypeString, Offset(stateTransitionNotLoaded, StateItemData), MaxStates,
         "Name of the state to transition to when the loaded state of the Image "
         "changes to 'Empty'." );
      addField( "stateTransitionOnAmmo", TypeString, Offset(stateTransitionAmmo, StateItemData), MaxStates,
         "Name of the state to transition to when the ammo state of the Image "
         "changes to true." );
      addField( "stateTransitionOnNoAmmo", TypeString, Offset(stateTransitionNoAmmo, StateItemData), MaxStates,
         "Name of the state to transition to when the ammo state of the Image "
         "changes to false." );
      addField( "stateTransitionOnTarget", TypeString, Offset(stateTransitionTarget, StateItemData), MaxStates,
         "Name of the state to transition to when the Image gains a target." );
      addField( "stateTransitionOnNoTarget", TypeString, Offset(stateTransitionNoTarget, StateItemData), MaxStates,
         "Name of the state to transition to when the Image loses a target." );
      addField( "stateTransitionOnWet", TypeString, Offset(stateTransitionWet, StateItemData), MaxStates,
         "Name of the state to transition to when the Image enters the water." );
      addField( "stateTransitionOnNotWet", TypeString, Offset(stateTransitionNotWet, StateItemData), MaxStates,
         "Name of the state to transition to when the Image exits the water." );
      addField( "stateTransitionOnTriggerUp", TypeString, Offset(stateTransitionTriggerUp, StateItemData), MaxStates,
         "Name of the state to transition to when the trigger state of the Image "
         "changes to true (fire button down)." );
      addField( "stateTransitionOnTriggerDown", TypeString, Offset(stateTransitionTriggerDown, StateItemData), MaxStates,
         "Name of the state to transition to when the trigger state of the Image "
         "changes to false (fire button released)." );
      addField( "stateTransitionOnAltTriggerUp", TypeString, Offset(stateTransitionAltTriggerUp, StateItemData), MaxStates,
         "Name of the state to transition to when the alt trigger state of the "
         "Image changes to true (alt fire button down)." );
      addField( "stateTransitionOnAltTriggerDown", TypeString, Offset(stateTransitionAltTriggerDown, StateItemData), MaxStates,
         "Name of the state to transition to when the alt trigger state of the "
         "Image changes to false (alt fire button up)." );
      addField( "stateTransitionOnTimeout", TypeString, Offset(stateTransitionTimeout, StateItemData), MaxStates,
         "Name of the state to transition to when we have been in this state "
         "for stateTimeoutValue seconds." );
      addField( "stateTimeoutValue", TypeF32, Offset(stateTimeoutValue, StateItemData), MaxStates,
         "Time in seconds to wait before transitioning to stateTransitionOnTimeout." );
      addField( "stateWaitForTimeout", TypeBool, Offset(stateWaitForTimeout, StateItemData), MaxStates,
         "If false, this state ignores stateTimeoutValue and transitions "
         "immediately if other transition conditions are met." );
      addField( "stateFire", TypeBool, Offset(stateFire, StateItemData), MaxStates,
         "The first state with this set to true is the state entered by the "
         "client when it receives the 'fire' event." );
      addField( "stateEjectShell", TypeBool, Offset(stateEjectShell, StateItemData), MaxStates,
         "If true, a shell casing will be ejected in this state." );
      addField( "stateEnergyDrain", TypeF32, Offset(stateEnergyDrain, StateItemData), MaxStates,
         "Amount of energy to subtract from the Image in this state.\n"
         "Energy is drained at stateEnergyDrain units/sec as long as we are in "
         "this state." );
      addField( "stateAllowImageChange", TypeBool, Offset(stateAllowImageChange, StateItemData), MaxStates,
         "If false, other Images will temporarily be blocked from mounting "
         "while the state machine is executing the tasks in this state.\n"
         "For instance, if we have a rocket launcher, the player shouldn't "
         "be able to switch out <i>while</i> firing. So, you'd set "
         "stateAllowImageChange to false in firing states, and true the rest "
         "of the time." );
      addField( "stateDirection", TypeBool, Offset(stateDirection, StateItemData), MaxStates,
         "Direction of the animation to play in this state.\nTrue is forward, "
         "false is backward." );
      addField( "stateLoadedFlag", TYPEID< StateItemData::StateData::LoadedState >(), Offset(stateLoaded, StateItemData), MaxStates,
         "Set the loaded state of the Image.\n"
         "<ul><li>IgnoreLoaded: Don't change Image loaded state.</li>"
         "<li>Loaded: Set Image loaded state to true.</li>"
         "<li>NotLoaded: Set Image loaded state to false.</li></ul>" );
      addField( "stateSpinThread", TYPEID< StateItemData::StateData::SpinState >(), Offset(stateSpin, StateItemData), MaxStates,
         "Controls how fast the 'spin' animation sequence will be played in "
         "this state.\n"
         "<ul><li>Ignore: No change to the spin sequence.</li>"
         "<li>Stop: Stops the spin sequence at its current position.</li>"
         "<li>SpinUp: Increase spin sequence timeScale from 0 (on state entry) "
         "to 1 (after stateTimeoutValue seconds).</li>"
         "<li>SpinDown: Decrease spin sequence timeScale from 1 (on state entry) "
         "to 0 (after stateTimeoutValue seconds).</li>"
         "<li>FullSpeed: Resume the spin sequence playback at its current "
         "position with timeScale=1.</li></ul>" );
      addField( "stateRecoil", TYPEID< StateItemData::StateData::RecoilState >(), Offset(stateRecoil, StateItemData), MaxStates,
         "Type of recoil sequence to play on the ShapeBase object on entry to "
         "this state.\n"
         "<ul><li>NoRecoil: Do not play a recoil sequence.</li>"
         "<li>LightRecoil: Play the light_recoil sequence.</li>"
         "<li>MediumRecoil: Play the medium_recoil sequence.</li>"
         "<li>HeavyRecoil: Play the heavy_recoil sequence.</li></ul>" );
      addField( "stateSequence", TypeString, Offset(stateSequence, StateItemData), MaxStates,
         "Name of the sequence to play on entry to this state." );
      addField( "stateSequenceRandomFlash", TypeBool, Offset(stateSequenceRandomFlash, StateItemData), MaxStates,
         "If true, a random frame from the muzzle flash sequence will be "
         "displayed each frame.\n"
         "The name of the muzzle flash sequence is the same as stateSequence, "
         "with \"_vis\" at the end." );
      addField( "stateScaleAnimation", TypeBool, Offset(stateScaleAnimation, StateItemData), MaxStates,
         "If true, the timeScale of the stateSequence animation will be adjusted "
         "such that the sequence plays for stateTimeoutValue seconds. " );
      addField( "stateSound", TypeSFXTrackName, Offset(stateSound, StateItemData), MaxStates,
         "Sound to play on entry to this state." );
      addField( "stateScript", TypeCaseString, Offset(stateScript, StateItemData), MaxStates,
         "Method to execute on entering this state.\n"
         "Scoped to this image class name, then StateItemData. The script "
         "callback function takes the same arguments as the onMount callback." );
      addField( "stateEmitter", TYPEID< ParticleEmitterData >(), Offset(stateEmitter, StateItemData), MaxStates,
         "Emitter to generate particles in this state (from muzzle point or "
         "specified node).\n\n@see stateEmitterNode" );
      addField( "stateEmitterTime", TypeF32, Offset(stateEmitterTime, StateItemData), MaxStates,
         "How long (in seconds) to emit particles on entry to this state." );
      addField( "stateEmitterNode", TypeString, Offset(stateEmitterNode, StateItemData), MaxStates,
         "Name of the node to emit particles from.\n\n@see stateEmitter" );
      addField( "stateIgnoreLoadedForReady", TypeBool, Offset(stateIgnoreLoadedForReady, StateItemData), MaxStates,
         "If set to true, and both ready and loaded transitions are true, the "
         "ready transition will be taken instead of the loaded transition.\n"
         "A state is 'ready' if pressing the fire trigger in that state would "
         "transition to the fire state." );

   endArray( "States" );

   addField( "computeCRC", TypeBool, Offset(computeCRC, StateItemData),
      "If true, verify that the CRC of the client's Image matches the server's "
      "CRC for the Image when loaded by the client." );

   addField( "maxConcurrentSounds", TypeS32, Offset(maxConcurrentSounds, StateItemData),
      "Maximum number of sounds this Image can play at a time.\n"
      "Any value <= 0 indicates that it can play an infinite number of sounds." );

   addField( "useRemainderDT", TypeBool, Offset(useRemainderDT, StateItemData), 
      "If true, allow multiple timeout transitions to occur within a single "
      "tick (useful if states have a very small timeout)." );

   // This does not do a very good job of applying changes to states
   // which may have occured in the editor, but at least we can do this...
   useEyeOffset = !eyeOffset.isIdentity();   
}
//-JR

//----------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(StateItem);

ConsoleDocClass( StateItem,
   "@brief Base StateItem class. Uses the StateItemData class for properties of individual StateItem objects.\n\n"   

   "@tsexample\n"
   "// This is the \"health patch\" dropped by a dying player.\n"
   "datablock StateItemData(HealthKitPatch)\n"
   "{\n"
   "   // Mission editor category, this datablock will show up in the\n"
   "   // specified category under the \"shapes\" root category.\n"
   "   category = \"Health\";\n\n"
   "   className = \"HealthPatch\";\n\n"
   "   // Basic StateItem properties\n"
   "   shapeFile = \"art/shapes/StateItems/patch/healthpatch.dts\";\n"
   "   mass = 2;\n"
   "   friction = 1;\n"
   "   elasticity = 0.3;\n"
   "   emap = true;\n\n"
   "   // Dynamic properties used by the scripts\n"
   "   pickupName = \"a health patch\";\n"
   "   repairAmount = 50;\n"
   "};\n\n"

	"%obj = new StateItem()\n"
	"{\n"
	"	dataBlock = HealthKitSmall;\n"
	"	parentGroup = EWCreatorWindow.objectGroup;\n"
	"	static = true;\n"
	"	rotate = true;\n"
	"};\n"
	"@endtsexample\n\n"

   "@ingroup gameObjects\n"
);

IMPLEMENT_CALLBACK( StateItem, onStickyCollision, void, ( const char* objID ),( objID ),
   "Informs the StateItem object that it is now sticking to another object.\n"
   "@param objID Object ID this StateItem object.\n"
   "@see StateItem, StateItemData\n"
);

IMPLEMENT_CALLBACK( StateItem, onEnterLiquid, void, ( const char* objID, const char* waterCoverage, const char* liquidType ),( objID, waterCoverage, liquidType ),
   "Informs an StateItem object that it has entered liquid, along with information about the liquid type.\n"
   "@param objID Object ID for this StateItem object.\n"
   "@param waterCoverage How much coverage of water this StateItem object has.\n"
   "@param liquidType The type of liquid that this StateItem object has entered.\n"
   "@see StateItem, StateItemData, waterObject\n"
);

IMPLEMENT_CALLBACK( StateItem, onLeaveLiquid, void, ( const char* objID, const char* liquidType ),( objID, liquidType ),
   "Informs an StateItem object that it has left a liquid, along with information about the liquid type.\n"
   "@param objID Object ID for this StateItem object.\n"
   "@param liquidType The type of liquid that this StateItem object has left.\n"
   "@see StateItem, StateItemData, waterObject\n"
);


StateItem::StateItem()
{
   mTypeMask |= ItemObjectType;
   mDataBlock = 0;
   mStatic = false;
   mRotate = false;
   mVelocity = VectorF(0,0,0);
   mAtRest = true;
   mAtRestCounter = 0;
   mInLiquid = false;
   delta.warpTicks = 0;
   delta.dt = 1;
   mCollisionObject = 0;
   mCollisionTimeout = 0;
   mPhysicsRep = NULL;

   mConvex.init(this);
   mWorkingQueryBox.minExtents.set(-1e9, -1e9, -1e9);
   mWorkingQueryBox.maxExtents.set(-1e9, -1e9, -1e9);

   mLight = NULL;

   mSubclassItemHandlesScene = true;

   //Advanced StateItem Support -JR
   state = nextState = NULL;
   loaded = false;
   nextLoaded = false;
   delayTime = 0.0f;
   fireCount = 0;
   altFireCount = 0;
   reloadCount = 0;
   triggerDown = altTriggerDown = false;
   ambientThread=visThread=animThread=flashThread=spinThread = NULL;
   lightStart = 0;
   animLoopingSound = false;
   ammo = false;
   target = false;
   wet = false;
   motion = false;

   for (U32 i=0; i<StateItemData::MaxGenericTriggers; ++i)
   {
      genericTrigger[i] = false;
   }
   //-JR
}

StateItem::~StateItem()
{
   SAFE_DELETE(mLight);
}


//----------------------------------------------------------------------------

bool StateItem::onAdd()
{
   if (!Parent::onAdd() || !mDataBlock)
      return false;

   addToScene();

   if (isServerObject())
      scriptOnAdd();

   if(mDataBlock->state[0].name)
      state = &mDataBlock->state[0];

   return true;
}

void StateItem::_updatePhysics()
{
   SAFE_DELETE( mPhysicsRep );

   if ( !PHYSICSMGR )
      return;

   if (mDataBlock->simpleServerCollision)
   {
      // We only need the trigger on the server.
      if ( isServerObject() )
      {
         PhysicsCollision *colShape = PHYSICSMGR->createCollision();
         colShape->addBox( mObjBox.getExtents() * 0.5f, MatrixF::Identity );

         PhysicsWorld *world = PHYSICSMGR->getWorld( isServerObject() ? "server" : "client" );
         mPhysicsRep = PHYSICSMGR->createBody();
         mPhysicsRep->init( colShape, 0, PhysicsBody::BF_TRIGGER | PhysicsBody::BF_KINEMATIC, this, world );
         mPhysicsRep->setTransform( getTransform() );
      }
   }
   else
   {
      if ( !mShapeInstance )
         return;

      PhysicsCollision* colShape = mShapeInstance->getShape()->buildColShape( false, getScale() );

      if ( colShape )
      {
         PhysicsWorld *world = PHYSICSMGR->getWorld( isServerObject() ? "server" : "client" );
         mPhysicsRep = PHYSICSMGR->createBody();
         mPhysicsRep->init( colShape, 0, PhysicsBody::BF_KINEMATIC, this, world );
         mPhysicsRep->setTransform( getTransform() );
      }
   }
}

bool StateItem::onNewDataBlock( GameBaseData *dptr, bool reload )
{
   mDataBlock = dynamic_cast<StateItemData*>(dptr);
   if (!mDataBlock || !Parent::onNewDataBlock(dptr,reload))
      return false;

   scriptOnNewDataBlock();

   if ( isProperlyAdded() )
      _updatePhysics();

   return true;
}

void StateItem::onRemove()
{
   scriptOnRemove();
   removeFromScene();

   Parent::onRemove();
}

/*void StateItem::onDeleteNotify( SimObject *obj )
{
   if ( obj == mCollisionObject ) 
   {
      mCollisionObject = NULL;
      mCollisionTimeout = 0;
   }

   Parent::onDeleteNotify( obj );
}*/

// Lighting: -----------------------------------------------------------------

void StateItem::registerLights(LightManager * lightManager, bool lightingScene)
{
   if(lightingScene)
      return;

   if(mDataBlock->lightOnlyStatic && !mStatic)
      return;

   F32 intensity;
   switch(mDataBlock->lightType)
   {
      case ConstantLight:
         intensity = mFadeVal;
         break;

      case PulsingLight:
      {
         S32 delta = Sim::getCurrentTime() - mDropTime;
         intensity = 0.5f + 0.5f * mSin(M_PI_F * F32(delta) / F32(mDataBlock->lightTime));
         intensity = 0.15f + intensity * 0.85f;
         intensity *= mFadeVal;  // fade out light on flags
         break;
      }

      default:
         return;
   }

   // Create a light if needed
   if (!mLight)
   {
      mLight = lightManager->createLightInfo();
   }   
   mLight->setColor( mDataBlock->lightColor * intensity );
   mLight->setType( LightInfo::Point );
   mLight->setRange( mDataBlock->lightRadius );
   mLight->setPosition( getBoxCenter() );

   lightManager->registerGlobalLight( mLight, this );
}


//----------------------------------------------------------------------------

Point3F StateItem::getVelocity() const
{
   return mVelocity;
}

void StateItem::setVelocity(const VectorF& vel)
{
   mVelocity = vel;

   // Clamp against the maximum velocity.
   if ( mDataBlock->maxVelocity > 0 )
   {
      F32 len = mVelocity.magnitudeSafe();
      if ( len > mDataBlock->maxVelocity )
      {
         Point3F excess = mVelocity * ( 1.0f - (mDataBlock->maxVelocity / len ) );
         mVelocity -= excess;
      }
   }
   setMaskBits(PositionMask);
   mAtRest = false;
   mAtRestCounter = 0;
}

void StateItem::applyImpulse(const Point3F&,const VectorF& vec)
{
   // StateItems ignore angular velocity
   VectorF vel;
   vel.x = vec.x / mDataBlock->mass;
   vel.y = vec.y / mDataBlock->mass;
   vel.z = vec.z / mDataBlock->mass;
   setVelocity(vel);
}

void StateItem::setCollisionTimeout(ShapeBase* obj)
{
	Parent::setCollisionTimeout(obj);
}


//----------------------------------------------------------------------------

void StateItem::processTick(const Move* move)
{
   Parent::processTick(move);

   /*if(!isMounted())
   {
	   // Warp to catch up to server
	   if (delta.warpTicks > 0)
	   {
		  delta.warpTicks--;

		  // Set new pos.
		  MatrixF mat = mObjToWorld;
		  mat.getColumn(3,&delta.pos);
		  delta.pos += delta.warpOffset;
		  mat.setColumn(3,delta.pos);
		  Parent::setTransform(mat);

		  // Backstepping
		  delta.posVec.x = -delta.warpOffset.x;
		  delta.posVec.y = -delta.warpOffset.y;
		  delta.posVec.z = -delta.warpOffset.z;
	   }
	   else
	   {
		  //-JR
		  /*if (isMounted()) {
			  MatrixF mat;
			  mMount.object->getMountTransform( mMount.node, mMount.xfm, &mat );

			  //apply offset
			  mat.mul(mDataBlock->mountOffset);

			  Parent::setTransform(mat);
			  Parent::setRenderTransform(mat);
		  }
		  else/*-JR*//* if (isServerObject() && mAtRest && (mStatic == false && mDataBlock->sticky == false))
		  {
			 if (++mAtRestCounter > csmAtRestTimer)
			 {
				mAtRest = false;
				mAtRestCounter = 0;
				setMaskBits(PositionMask);
			 }
		  }

		  if (!mStatic && !mAtRest && isHidden() == false)
		  {
			 updateVelocity(TickSec);
			 updateWorkingCollisionSet(isGhost() ? sClientCollisionMask : sServerCollisionMask, TickSec);
			 updatePos(isGhost() ? sClientCollisionMask : sServerCollisionMask, TickSec);
		  }
		  else
		  {
			 // Need to clear out last updatePos or warp interpolation
			 delta.posVec.set(0,0,0);
		  }
	   }
   }
   else{
	  MatrixF mat;
	  mMount.object->getMountTransform( mMount.node, mMount.xfm, &mat );
	  getRenderMountTransform( 0.0f, data.mountPoint, MatrixF::Identity, &nmat );

	  //apply offset
      mat.mul(mDataBlock->mountOffset);

	  Parent::setTransform(mat);
	  //Parent::setRenderTransform(mat);
   }*/
}

void StateItem::interpolateTick(F32 dt)
{
   Parent::interpolateTick(dt);

   // Client side interpolation
   //-JR
   /*Point3F pos = delta.pos + delta.posVec * dt;
   MatrixF mat = mRenderObjToWorld;
   mat.setColumn(3,pos);
   if(!isMounted()){
	   setRenderTransform(mat);
	   delta.dt = dt;
   }
   if ( isMounted() )
   {
      // Fetch Mount Transform.
      MatrixF mat;
      mMount.object->getRenderMountTransform( dt, mMount.node, mMount.xfm, &mat );

	  //apply offset
	  mat.mul(mDataBlock->mountOffset);

      // Apply.
	  setTransform(mat);
      setRenderTransform( mat );

      return;
   }
   if (isMounted()) {
      MatrixF mat;
      //mMount.object->getMountTransform( mMount.node, mMount.xfm, &mat );
	  mMount.object->getRenderMountTransform( dt, mMount.node, mMount.xfm, &mat );

	  //apply offset
	  mat.mul(mDataBlock->mountOffset);

      Parent::setTransform(mat);
      Parent::setRenderTransform(mat);
   }
   if (isMounted()) {
      MatrixF mat;
      mMount.object->getRenderMountTransform( 0, mMount.node, mMount.xfm, &mat );
	  mat.mul(mDataBlock->mountOffset);

      Parent::setRenderTransform(mat);
   }*/
   //-JR
}


//----------------------------------------------------------------------------

void StateItem::setTransform(const MatrixF& mat)
{
   Point3F pos;
   mat.getColumn(3,&pos);
   MatrixF tmat;
   if (!mRotate) {
      // Forces all rotation to be around the z axis
      VectorF vec;
      mat.getColumn(1,&vec);
      tmat.set(EulerF(0,0,-mAtan2(-vec.x,vec.y)));
   }
   else
      tmat.identity();
   tmat.setColumn(3,pos);
   Parent::setTransform(tmat);
   if (!mStatic)
   {
      mAtRest = false;
      mAtRestCounter = 0;
   }

   if ( mPhysicsRep )
      mPhysicsRep->setTransform( getTransform() );

   setMaskBits(/*-JR todo: reimplement //RotationMask -JR |*/ PositionMask | NoWarpMask);
}


//----------------------------------------------------------------------------
void StateItem::updateWorkingCollisionSet(const U32 mask, const F32 dt)
{
   // It is assumed that we will never accelerate more than 10 m/s for gravity...
   //
   Point3F scaledVelocity = mVelocity * dt;
   F32 len    = scaledVelocity.len();
   F32 newLen = len + (10 * dt);

   // Check to see if it is actually necessary to construct the new working list,
   //  or if we can use the cached version from the last query.  We use the x
   //  component of the min member of the mWorkingQueryBox, which is lame, but
   //  it works ok.
   bool updateSet = false;

   Box3F convexBox = mConvex.getBoundingBox(getTransform(), getScale());
   F32 l = (newLen * 1.1) + 0.1;  // from Convex::updateWorkingList
   convexBox.minExtents -= Point3F(l, l, l);
   convexBox.maxExtents += Point3F(l, l, l);

   // Check containment
   {
      if (mWorkingQueryBox.minExtents.x != -1e9)
      {
         if (mWorkingQueryBox.isContained(convexBox) == false)
         {
            // Needed region is outside the cached region.  Update it.
            updateSet = true;
         }
         else
         {
            // We can leave it alone, we're still inside the cached region
         }
      }
      else
      {
         // Must update
         updateSet = true;
      }
   }

   // Actually perform the query, if necessary
   if (updateSet == true)
   {
      mWorkingQueryBox = convexBox;
      mWorkingQueryBox.minExtents -= Point3F(2 * l, 2 * l, 2 * l);
      mWorkingQueryBox.maxExtents += Point3F(2 * l, 2 * l, 2 * l);

      disableCollision();
      if (mCollisionObject)
         mCollisionObject->disableCollision();

      mConvex.updateWorkingList(mWorkingQueryBox, mask);

	  //-JR
      /*if (mCollisionObject)
         mCollisionObject->enableCollision();
      enableCollision();*/
	  SceneObject* mountedObject = getMountedObject(0);  
      SimObjectId mountedId = mountedObject?mountedObject->getId():0;  
  
      SceneObject* mountingObject = mMount.object;  
      SimObjectId mountingId = mountingObject?mountingObject->getId():0;  
  
      mConvex.updateWorkingList(mWorkingQueryBox,  
         isGhost() ? sClientCollisionMask : sServerCollisionMask/*,mountedId,mountingId*/);  
      enableCollision();
	  //-JR
   }
}

void StateItem::updateVelocity(const F32 dt)
{
   // Acceleration due to gravity
   mVelocity.z += (mGravity * mDataBlock->gravityMod) * dt;
   F32 len;
   if (mDataBlock->maxVelocity > 0 && (len = mVelocity.len()) > (mDataBlock->maxVelocity * 1.05)) {
      Point3F excess = mVelocity * (1.0 - (mDataBlock->maxVelocity / len ));
      excess *= 0.1f;
      mVelocity -= excess;
   }

   // Container buoyancy & drag
   mVelocity.z -= mBuoyancy * (mGravity * mDataBlock->gravityMod * mGravityMod) * dt;
   mVelocity   -= mVelocity * mDrag * dt;
}


void StateItem::updatePos(const U32 /*mask*/, const F32 dt)
{
   // Try and move
   Point3F pos;
   mObjToWorld.getColumn(3,&pos);
   delta.posVec = pos;

   bool contact = false;
   bool nonStatic = false;
   bool stickyNotify = false;
   CollisionList collisionList;
   F32 time = dt;

   static Polyhedron sBoxPolyhedron;
   static ExtrudedPolyList sExtrudedPolyList;
   static EarlyOutPolyList sEarlyOutPolyList;
   MatrixF collisionMatrix(true);
   Point3F end = pos + mVelocity * time;
   U32 mask = isServerObject() ? sServerCollisionMask : sClientCollisionMask;

   // Part of our speed problem here is that we don't track contact surfaces, like we do
   //  with the player.  In order to handle the most common and performance impacting
   //  instance of this problem, we'll use a ray cast to detect any contact surfaces below
   //  us.  This won't be perfect, but it only needs to catch a few of these to make a
   //  big difference.  We'll cast from the top center of the bounding box at the tick's
   //  beginning to the bottom center of the box at the end.
   Point3F startCast((mObjBox.minExtents.x + mObjBox.maxExtents.x) * 0.5,
                     (mObjBox.minExtents.y + mObjBox.maxExtents.y) * 0.5,
                     mObjBox.maxExtents.z);
   Point3F endCast((mObjBox.minExtents.x + mObjBox.maxExtents.x) * 0.5,
                   (mObjBox.minExtents.y + mObjBox.maxExtents.y) * 0.5,
                   mObjBox.minExtents.z);
   collisionMatrix.setColumn(3, pos);
   collisionMatrix.mulP(startCast);
   collisionMatrix.setColumn(3, end);
   collisionMatrix.mulP(endCast);
   RayInfo rinfo;
   bool doToughCollision = true;
   disableCollision();
   if (mCollisionObject)
      mCollisionObject->disableCollision();
   if (getContainer()->castRay(startCast, endCast, mask, &rinfo))
   {
      F32 bd = -mDot(mVelocity, rinfo.normal);

      if (bd >= 0.0)
      {
         // Contact!
         if (mDataBlock->sticky && rinfo.object->getTypeMask() & (STATIC_COLLISION_TYPEMASK)) {
            mVelocity.set(0, 0, 0);
            mAtRest = true;
            mAtRestCounter = 0;
            stickyNotify = true;
            mStickyCollisionPos    = rinfo.point;
            mStickyCollisionNormal = rinfo.normal;
            doToughCollision = false;
         } else {
            // Subtract out velocity into surface and friction
            VectorF fv = mVelocity + rinfo.normal * bd;
            F32 fvl = fv.len();
            if (fvl) {
               F32 ff = bd * mDataBlock->friction;
               if (ff < fvl) {
                  fv *= ff / fvl;
                  fvl = ff;
               }
            }
            bd *= 1 + mDataBlock->elasticity;
            VectorF dv = rinfo.normal * (bd + 0.002);
            mVelocity += dv;
            mVelocity -= fv;

            // Keep track of what we hit
            contact = true;
            U32 typeMask = rinfo.object->getTypeMask();
            if (!(typeMask & StaticObjectType))
               nonStatic = true;
            if (isServerObject() && (typeMask & ShapeBaseObjectType)) {
               ShapeBase* col = static_cast<ShapeBase*>(rinfo.object);
               queueCollision(col,mVelocity - col->getVelocity());
            }
         }
      }
   }
   enableCollision();
   if (mCollisionObject)
      mCollisionObject->enableCollision();

   if (doToughCollision)
   {
      U32 count;
      for (count = 0; count < 3; count++)
      {
         // Build list from convex states here...
         end = pos + mVelocity * time;


         collisionMatrix.setColumn(3, end);
         Box3F wBox = getObjBox();
         collisionMatrix.mul(wBox);
         Box3F testBox = wBox;
         Point3F oldMin = testBox.minExtents;
         Point3F oldMax = testBox.maxExtents;
         testBox.minExtents.setMin(oldMin + (mVelocity * time));
         testBox.maxExtents.setMin(oldMax + (mVelocity * time));

         sEarlyOutPolyList.clear();
         sEarlyOutPolyList.mNormal.set(0,0,0);
         sEarlyOutPolyList.mPlaneList.setSize(6);
         sEarlyOutPolyList.mPlaneList[0].set(wBox.minExtents,VectorF(-1,0,0));
         sEarlyOutPolyList.mPlaneList[1].set(wBox.maxExtents,VectorF(0,1,0));
         sEarlyOutPolyList.mPlaneList[2].set(wBox.maxExtents,VectorF(1,0,0));
         sEarlyOutPolyList.mPlaneList[3].set(wBox.minExtents,VectorF(0,-1,0));
         sEarlyOutPolyList.mPlaneList[4].set(wBox.minExtents,VectorF(0,0,-1));
         sEarlyOutPolyList.mPlaneList[5].set(wBox.maxExtents,VectorF(0,0,1));

         CollisionWorkingList& eorList = mConvex.getWorkingList();
         CollisionWorkingList* eopList = eorList.wLink.mNext;
         while (eopList != &eorList) {
            if ((eopList->mConvex->getObject()->getTypeMask() & mask) != 0)
            {
               Box3F convexBox = eopList->mConvex->getBoundingBox();
               if (testBox.isOverlapped(convexBox))
               {
                  eopList->mConvex->getPolyList(&sEarlyOutPolyList);
                  if (sEarlyOutPolyList.isEmpty() == false)
                     break;
               }
            }
            eopList = eopList->wLink.mNext;
         }
         if (sEarlyOutPolyList.isEmpty())
         {
            pos = end;
            break;
         }

         collisionMatrix.setColumn(3, pos);
         sBoxPolyhedron.buildBox(collisionMatrix, mObjBox);

         // Build extruded polyList...
         VectorF vector = end - pos;
         sExtrudedPolyList.extrude(sBoxPolyhedron, vector);
         sExtrudedPolyList.setVelocity(mVelocity);
         sExtrudedPolyList.setCollisionList(&collisionList);

         CollisionWorkingList& rList = mConvex.getWorkingList();
         CollisionWorkingList* pList = rList.wLink.mNext;
         while (pList != &rList) {
            if ((pList->mConvex->getObject()->getTypeMask() & mask) != 0)
            {
               Box3F convexBox = pList->mConvex->getBoundingBox();
               if (testBox.isOverlapped(convexBox))
               {
                  pList->mConvex->getPolyList(&sExtrudedPolyList);
               }
            }
            pList = pList->wLink.mNext;
         }

         if (collisionList.getTime() < 1.0)
         {
            // Set to collision point
            F32 dt = time * collisionList.getTime();
            pos += mVelocity * dt;
            time -= dt;

            // Pick the most resistant surface
            F32 bd = 0;
            const Collision* collision = 0;
            for (int c = 0; c < collisionList.getCount(); c++) {
               const Collision &cp = collisionList[c];
               F32 dot = -mDot(mVelocity,cp.normal);
               if (dot > bd) {
                  bd = dot;
                  collision = &cp;
               }
            }

            if (collision && mDataBlock->sticky && collision->object->getTypeMask() & (STATIC_COLLISION_TYPEMASK)) {
               mVelocity.set(0, 0, 0);
               mAtRest = true;
               mAtRestCounter = 0;
               stickyNotify = true;
               mStickyCollisionPos    = collision->point;
               mStickyCollisionNormal = collision->normal;
               break;
            } else {
               // Subtract out velocity into surface and friction
               if (collision) {
                  VectorF fv = mVelocity + collision->normal * bd;
                  F32 fvl = fv.len();
                  if (fvl) {
                     F32 ff = bd * mDataBlock->friction;
                     if (ff < fvl) {
                        fv *= ff / fvl;
                        fvl = ff;
                     }
                  }
                  bd *= 1 + mDataBlock->elasticity;
                  VectorF dv = collision->normal * (bd + 0.002);
                  mVelocity += dv;
                  mVelocity -= fv;

                  // Keep track of what we hit
                  contact = true;
                  U32 typeMask = collision->object->getTypeMask();
                  if (!(typeMask & StaticObjectType))
                     nonStatic = true;
                  if (isServerObject() && (typeMask & ShapeBaseObjectType)) {
                     ShapeBase* col = static_cast<ShapeBase*>(collision->object);
                     queueCollision(col,mVelocity - col->getVelocity());
                  }
               }
            }
         }
         else
         {
            pos = end;
            break;
         }
      }
      if (count == 3)
      {
         // Couldn't move...
         mVelocity.set(0, 0, 0);
      }
   }

   // If on the client, calculate delta for backstepping
   if (isGhost()) {
      delta.pos     = pos;
      delta.posVec -= pos;
      delta.dt = 1;
   }

   // Update transform
   MatrixF mat = mObjToWorld;
   mat.setColumn(3,pos);
   Parent::setTransform(mat);
   enableCollision();
   if (mCollisionObject)
      mCollisionObject->enableCollision();
   updateContainer();

   if ( mPhysicsRep )
      mPhysicsRep->setTransform( mat );

   //
   if (contact) {
      // Check for rest condition
      if (!nonStatic && mVelocity.len() < sAtRestVelocity) {
         mVelocity.x = mVelocity.y = mVelocity.z = 0;
         mAtRest = true;
         mAtRestCounter = 0;
      }

      // Only update the client if we hit a non-static shape or
      // if this is our final rest pos.
      if (nonStatic || mAtRest)
         setMaskBits(PositionMask);
   }

   // Collision callbacks. These need to be processed whether we hit
   // anything or not.
   if (!isGhost())
   {
      SimObjectPtr<StateItem> safePtr(this);
      if (stickyNotify)
      {
         notifyCollision();
         if(bool(safePtr))
			 onStickyCollision_callback( getIdString() );
      }
      else
         notifyCollision();

      // water
      if(bool(safePtr))
      {
         if(!mInLiquid && mWaterCoverage != 0.0f)
         {
			onEnterLiquid_callback( getIdString(), Con::getFloatArg(mWaterCoverage), mLiquidType.c_str() );
            mInLiquid = true;
         }
         else if(mInLiquid && mWaterCoverage == 0.0f)
         {
			 onLeaveLiquid_callback(getIdString(), mLiquidType.c_str());
            mInLiquid = false;
         }
      }
   }
}


//----------------------------------------------------------------------------

static MatrixF IMat(1);

bool StateItem::buildPolyList(PolyListContext context, AbstractPolyList* polyList, const Box3F&, const SphereF&)
{
   if ( context == PLC_Decal )
      return false;

   // Collision with the StateItem is always against the StateItem's object
   // space bounding box axis aligned in world space.
   Point3F pos;
   mObjToWorld.getColumn(3,&pos);
   IMat.setColumn(3,pos);
   polyList->setTransform(&IMat, mObjScale);
   polyList->setObject(this);
   polyList->addBox(mObjBox);
   return true;
}


//----------------------------------------------------------------------------

U32 StateItem::packUpdate(NetConnection *connection, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(connection,mask,stream);

   if (stream->writeFlag(mask & InitialUpdateMask)) {
      stream->writeFlag(mRotate);
      stream->writeFlag(mStatic);
      if (stream->writeFlag(getScale() != Point3F(1, 1, 1)))
         mathWrite(*stream, getScale());
   }
   //-JR
   //todo: re-implement this
   /*if (mask & ThrowSrcMask && mCollisionObject) {
      S32 gIndex = connection->getGhostIndex(mCollisionObject);
      if (stream->writeFlag(gIndex != -1))
         stream->writeInt(gIndex,NetConnection::GhostIdBitSize);
   }
   else
      stream->writeFlag(false);
   if (stream->writeFlag(mask & RotationMask && !mRotate)) {
      // Assumes rotation is about the Z axis
      AngAxisF aa(mObjToWorld);
      stream->writeFlag(aa.axis.z < 0);
      stream->write(aa.angle);
   }*/
   //-JR
   if (stream->writeFlag(mask & PositionMask)) {
      Point3F pos;
      mObjToWorld.getColumn(3,&pos);
      mathWrite(*stream, pos);
      if (!stream->writeFlag(mAtRest)) {
         mathWrite(*stream, mVelocity);
      }
      stream->writeFlag(!(mask & NoWarpMask));
   }

   //-JR
   //Advanced StateItem Support 
   if(stream->writeFlag(mask & StateMask))
   {
	  stream->writeFlag(loaded);
	  stream->writeFlag(triggerDown);
	  stream->writeFlag(altTriggerDown);
	  stream->writeInt(fireCount,3);
	  stream->writeFlag(ammo);                    
	  stream->writeFlag(target);                  
	  stream->writeFlag(wet);
	  stream->writeInt(mountPoint, 3);

	  //here for now, instead of manualstatemask, because that mask is being a butt-face
	  S32 st = mDataBlock->lookupState(state->name);
	  stream->writeInt(st,StateItemData::NumStateBits);
   }
   if(stream->writeFlag(mask & ManualStateMask))
   {
	  S32 st = mDataBlock->lookupState(state->name);
	  stream->writeInt(st,StateItemData::NumStateBits);
   }
   //-JR
   return retMask;
}

void StateItem::unpackUpdate(NetConnection *connection, BitStream *stream)
{
   Parent::unpackUpdate(connection,stream);
   if (stream->readFlag()) {
      mRotate = stream->readFlag();
      mStatic = stream->readFlag();
      if (stream->readFlag())
         mathRead(*stream, &mObjScale);
      else
         mObjScale.set(1, 1, 1);
   }
   //-JR
   //todo: reimplement
   /*if (stream->readFlag()) {
      S32 gIndex = stream->readInt(NetConnection::GhostIdBitSize);
      setCollisionTimeout(static_cast<ShapeBase*>(connection->resolveGhost(gIndex)));
   }*/
   MatrixF mat = mObjToWorld;
   /*if (stream->readFlag()) {
      // Assumes rotation is about the Z axis
      AngAxisF aa;
      aa.axis.set(0.0f, 0.0f, stream->readFlag() ? -1.0f : 1.0f);
      stream->read(&aa.angle);
      aa.setMatrix(&mat);
      Point3F pos;
      mObjToWorld.getColumn(3,&pos);
      mat.setColumn(3,pos);
   }*/
   if (stream->readFlag()) {
      Point3F pos;
      mathRead(*stream, &pos);
      F32 speed = mVelocity.len();
      if ((mAtRest = stream->readFlag()) == true)
         mVelocity.set(0.0f, 0.0f, 0.0f);
      else
         mathRead(*stream, &mVelocity);

      if (stream->readFlag() && isProperlyAdded()) {
         // Determin number of ticks to warp based on the average
         // of the client and server velocities.
         delta.warpOffset = pos - delta.pos;
         F32 as = (speed + mVelocity.len()) * 0.5f * TickSec;
         F32 dt = (as > 0.00001f) ? delta.warpOffset.len() / as: sMaxWarpTicks;
         delta.warpTicks = (S32)((dt > sMinWarpTicks)? getMax(mFloor(dt + 0.5f), 1.0f): 0.0f);

         if (delta.warpTicks)
         {
            // Setup the warp to start on the next tick, only the
            // object's position is warped.
            if (delta.warpTicks > sMaxWarpTicks)
               delta.warpTicks = sMaxWarpTicks;
            delta.warpOffset /= (F32)delta.warpTicks;
         }
         else {
            // Going to skip the warp, server and client are real close.
            // Adjust the frame interpolation to move smoothly to the
            // new position within the current tick.
            Point3F cp = delta.pos + delta.posVec * delta.dt;
            VectorF vec = delta.pos - cp;
            F32 vl = vec.len();
            if (vl) {
               F32 s = delta.posVec.len() / vl;
               delta.posVec = (cp - pos) * s;
            }
            delta.pos = pos;
            mat.setColumn(3,pos);
         }
      }
      else {
         // Set the StateItem to the server position
         delta.warpTicks = 0;
         delta.posVec.set(0,0,0);
         delta.pos = pos;
         delta.dt = 0;
         mat.setColumn(3,pos);
      }
   }

   //-JR
   //Advanced StateItem Support 
   if(stream->readFlag())
   {
	  loaded = stream->readFlag();
	  triggerDown = stream->readFlag();
	  altTriggerDown = stream->readFlag();
	  S32 count = stream->readInt(3);
	  //updateState(0); //not needed as it ticks properly now.
	  ammo = stream->readFlag();                    
	  target = stream->readFlag();                  
	  wet = stream->readFlag();
	  mountPoint = stream->readInt(3);

	  //manualstatemask being a buttface, as stated above
	  S32 st = stream->readInt(StateItemData::NumStateBits);
	  setState(st,true);
   }
   if(stream->readFlag())
   {
	  S32 st = stream->readInt(StateItemData::NumStateBits);
	  setState(st,true);
   }
	//-JR
   Parent::setTransform(mat);
}

DefineEngineMethod( StateItem, isStatic, bool, (),, "@brief Is the object static (ie, non-movable)?\n\n"   
   "@return True if the object is static, false if it is not.\n"
   "@tsexample\n"
	   "// Query the StateItem on if it is or is not static.\n"
	   "%isStatic = %StateItemData.isStatic();\n\n"
   "@endtsexample\n\n"
   )
{
   return object->isStatic();
}

DefineEngineMethod( StateItem, isAtRest, bool, (),, 
   "@brief Is the object at rest (ie, no longer moving)?\n\n"   
   "@return True if the object is at rest, false if it is not.\n"
   "@tsexample\n"
	   "// Query the StateItem on if it is or is not at rest.\n"
	   "%isAtRest = %StateItem.isAtRest();\n\n"
   "@endtsexample\n\n"
   )
{
   return object->isAtRest();
}

DefineEngineMethod( StateItem, isRotating, bool, (),, "@brief Is the object still rotating?\n\n"   
   "@return True if the object is still rotating, false if it is not.\n"
   "@tsexample\n"
	   "// Query the StateItem on if it is or is not rotating.\n"
	   "%isRotating = %StateItemData.isRotating();\n\n"
   "@endtsexample\n\n"
   )
{
   return object->isRotating();
}

DefineEngineMethod( StateItem, setCollisionTimeout, bool, (int ignoreColObj),(NULL), "@brief Temporarily disable collisions against a specific ShapeBase object.\n\n"
   "@param objectID ShapeBase object ID to disable collisions against.\n"
   "@return Returns true if the ShapeBase object requested could be found, false if it could not.\n"
   "@tsexample\n"
	   "// Set the ShapeBase Object ID to disable collisions against\n"
	   "%ignoreColObj = %player.getID();\n\n"
	   "// Inform this StateItem object to ignore collisions temproarily against the %ignoreColObj.\n"
	   "%StateItem.setCollisionTimeout(%ignoreColObj);\n\n"
   "@endtsexample\n\n"
   )
{
   ShapeBase* source = NULL;
   if (Sim::findObject(ignoreColObj),source) {
      object->setCollisionTimeout(source);
      return true;
   }
   return false;
}


DefineEngineMethod( StateItem, getLastStickyPos, const char*, (),, "@brief Get the position on the surface on which this StateItem is stuck.\n\n"   
   "@return Returns The XYZ position of where this StateItem is stuck.\n"
   "@tsexample\n"
	   "// Acquire the position where this StateItem is currently stuck\n"
	   "%stuckPosition = %StateItem.getLastStickPos();\n\n"
   "@endtsexample\n\n"
   )
{
   char* ret = Con::getReturnBuffer(256);
   if (object->isServerObject())
      dSprintf(ret, 255, "%g %g %g",
               object->mStickyCollisionPos.x,
               object->mStickyCollisionPos.y,
               object->mStickyCollisionPos.z);
   else
      dStrcpy(ret, "0 0 0");

   return ret;
}

DefineEngineMethod( StateItem, getLastStickyNormal, const char *, (),, "@brief Get the normal of the surface on which the object is stuck.\n\n"   
   "@return Returns The XYZ position of where this StateItem is stuck.\n"
   "@tsexample\n"
	   "// Acquire the position where this StateItem is currently stuck\n"
	   "%stuckPosition = %StateItem.getLastStickPos();\n\n"
   "@endtsexample\n\n"
   )
{
   char* ret = Con::getReturnBuffer(256);
   if (object->isServerObject())
      dSprintf(ret, 255, "%g %g %g",
               object->mStickyCollisionNormal.x,
               object->mStickyCollisionNormal.y,
               object->mStickyCollisionNormal.z);
   else
      dStrcpy(ret, "0 0 0");

   return ret;
}

//----------------------------------------------------------------------------

void StateItem::initPersistFields()
{
   addGroup("Misc");	
   addField("static",      TypeBool, Offset(mStatic, StateItem), "If true, the object is not moving in the world (and will not be updated through the network).\n");
   addField("rotate",      TypeBool, Offset(mRotate, StateItem), "If true, the object will automatically rotate around its Z axis.\n");
   endGroup("Misc");

   Parent::initPersistFields();
}

void StateItem::consoleInit()
{
   Con::addVariable("StateItem::minWarpTicks",TypeF32,&sMinWarpTicks, "Fraction of tick at which instant warp occures.\n"
	   "@ingroup GameObjects");
   Con::addVariable("StateItem::maxWarpTicks",TypeS32,&sMaxWarpTicks, "Max warp duration in ticks.\n"
	   "@ingroup GameObjects");
}

//----------------------------------------------------------------------------

void StateItem::prepRenderImage( SceneRenderState* state )
{
   // StateItems do NOT render if destroyed
   if (getDamageState() == Destroyed)
      return;

   Parent::prepRenderImage( state );
}

void StateItem::buildConvex(const Box3F& box, Convex* convex)
{
   if (mShapeInstance == NULL)
      return;

   // These should really come out of a pool
   mConvexList->collectGarbage();

   if (box.isOverlapped(getWorldBox()) == false)
      return;

   // Just return a box convex for the entire shape...
   Convex* cc = 0;
   CollisionWorkingList& wl = convex->getWorkingList();
   for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext) {
      if (itr->mConvex->getType() == BoxConvexType &&
          itr->mConvex->getObject() == this) {
         cc = itr->mConvex;
         break;
      }
   }
   if (cc)
      return;

   // Create a new convex.
   BoxConvex* cp = new BoxConvex;
   mConvexList->registerObject(cp);
   convex->addToWorkingList(cp);
   cp->init(this);

   mObjBox.getCenter(&cp->mCenter);
   cp->mSize.x = mObjBox.len_x() / 2.0f;
   cp->mSize.y = mObjBox.len_y() / 2.0f;
   cp->mSize.z = mObjBox.len_z() / 2.0f;
}

void StateItem::advanceTime(F32 dt)
{
   Parent::advanceTime(dt);

   //-JR
   //not going to rotate if mounted
   if(!isMounted()){
	   if( mRotate )
	   {
		  F32 r = (dt / sRotationSpeed) * M_2PI;
		  Point3F pos = mRenderObjToWorld.getPosition();
		  MatrixF rotMatrix;
		  if( mRotate )
		  {
			 rotMatrix.set( EulerF( 0.0, 0.0, r ) );
		  }
		  else
		  {
			 rotMatrix.set( EulerF( r * 0.5, 0.0, r ) );
		  }
		  MatrixF mat = mRenderObjToWorld;
		  mat.setPosition( pos );
		  mat.mul( rotMatrix );
		  setRenderTransform(mat);
	   }
   }
   else
   {
	  MatrixF mat;
	  mMount.object->getRenderMountTransform( dt, mMount.node, mMount.xfm, &mat );

	  //apply offset
	  mat.mul(mDataBlock->mountOffset);

	  Parent::setTransform(mat);
	  Parent::setRenderTransform(mat);
   }//-JR

   //-JR
   updateAnimation(dt);
}


//-JR

StateItemData* StateItem::getPendingStateItem()
{
   return (mDataBlock == InvalidImagePtr)? 0: mDataBlock;
}

bool StateItem::isFiring()
{
   return mDataBlock && state->fire;
}

bool StateItem::isAltFiring()
{
   return mDataBlock && state->altFire;
}

bool StateItem::isReloading()
{
   return mDataBlock && state->reload;
}

bool StateItem::isReady(U32 ns,U32 depth)
{
   // Will pressing the trigger lead to a fire state?
   if (depth++ > 5 || !mDataBlock)
      return false;
   StateItemData::StateData& stateData = (ns == -1) ? *state : mDataBlock->state[ns];
   if (stateData.fire)
      return true;

   // Try the transitions...
   if (stateData.ignoreLoadedForReady == true) {
      if ((ns = stateData.transition.loaded[true]) != -1)
         if (isReady(ns,depth))
            return true;
   } else {
      if ((ns = stateData.transition.loaded[loaded]) != -1)
         if (isReady(ns,depth))
            return true;
   }
   for (U32 i=0; i<StateItemData::MaxGenericTriggers; ++i)
   {
      if ((ns = stateData.transition.genericTrigger[i][genericTrigger[i]]) != -1)
         if (isReady(ns,depth))
            return true;
   }
   if ((ns = stateData.transition.ammo[ammo]) != -1)
      if (isReady(ns,depth))
         return true;
   if ((ns = stateData.transition.target[target]) != -1)
      if (isReady(ns,depth))
         return true;
   if ((ns = stateData.transition.wet[wet]) != -1)
      if (isReady(ns,depth))
         return true;
   if ((ns = stateData.transition.motion[motion]) != -1)
      if (isReady(ns,depth))
         return true;
   if ((ns = stateData.transition.trigger[1]) != -1)
      if (isReady(ns,depth))
         return true;
   if ((ns = stateData.transition.altTrigger[1]) != -1)
      if (isReady(ns,depth))
         return true;
   if ((ns = stateData.transition.timeout) != -1)
      if (isReady(ns,depth))
         return true;
   return false;
}

/*bool StateItem::isMounted(StateItemData* imageData)
{
   for (U32 i = 0; i < MaxMountedImages; i++)
      if (imageData == mMountedImageList[i].dataBlock)
         return true;
   return false;
}

S32 ShapeBase::getMountSlot(StateItemData* imageData)
{
   for (U32 i = 0; i < MaxMountedImages; i++)
      if (imageData == mMountedImageList[i].dataBlock)
         return i;
   return -1;
}

NetStringHandle StateItem::getSkinTag()
{
   return mDataBlock? skinNameHandle : NetStringHandle();
}*/
void StateItem::setGenericTriggerState(U32 trigger, bool state)
{
   if (mDataBlock && genericTrigger[trigger] != state) {
      setMaskBits(StateMask);
      genericTrigger[trigger] = state;
   }
}

bool StateItem::getGenericTriggerState(U32 trigger)
{
   if (!mDataBlock)
      return false;
   return genericTrigger[trigger];
}

void StateItem::setAmmoState(bool isAmmo)
{
   if (mDataBlock && !mDataBlock->usesEnergy && ammo != isAmmo) {
      setMaskBits(StateMask);
      ammo = isAmmo;
   }
}

bool StateItem::getAmmoState()
{
   if (!mDataBlock)
      return false;
   return ammo;
}

void StateItem::setWetState(bool isWet)
{
    
   if (mDataBlock && wet != isWet) {
      setMaskBits(StateMask);
      wet = isWet;
   }
}

bool StateItem::getWetState()
{
    
   if (!mDataBlock)
      return false;
   return wet;
}

void StateItem::setMotionState(bool motion)
{
   if (mDataBlock && motion != motion) {
      setMaskBits(StateMask);
      motion = motion;
   }
}

bool StateItem::getMotionState()
{
   if (!mDataBlock)
      return false;
   return motion;
}

void StateItem::setTargetState(bool target)
{
   if (mDataBlock && target != target) {
      setMaskBits(StateMask);
      target = target;
   }
}

bool StateItem::getTargetState()
{
   if (!mDataBlock)
      return false;
   return target;
}

void StateItem::setLoadedState(bool isloaded)
{
    
   if (mDataBlock && loaded != isloaded) {
      setMaskBits(StateMask);
      loaded = isloaded;
   }
}

bool StateItem::getLoadedState()
{
   if (!mDataBlock)
      return false;
   return loaded;
}

void StateItem::getMuzzleVector(VectorF* vec)
{
   MatrixF mat;
   getMuzzleTransform(&mat);

   /*if (mDataBlock->correctMuzzleVector){
	   GameConnection * gc = mMount.object->getControllingClient();
      if (gc)
         if (gc->isFirstPerson() && !gc->isAIControlled())
			 if (ShapeBase::getCorrectedAim(mat, vec))
               return;
   }*/
   GameConnection * gc = getControllingClient();
   if (gc && !gc->isAIControlled())
   {
      bool fp = gc->isFirstPerson();
      if ((fp && mDataBlock->correctMuzzleVector) ||
         (!fp && mDataBlock->correctMuzzleVectorTP))
         if (getCorrectedAim(mat, vec))
            return;
   }

   mat.getColumn(1,vec);
}

void StateItem::getMuzzlePoint(Point3F* pos)
{
   MatrixF mat;
   getMuzzleTransform(&mat);
   mat.getColumn(3,pos);
}


void StateItem::getRenderMuzzleVector(VectorF* vec)
{
   MatrixF mat;
   getRenderMuzzleTransform(&mat);

    
   /*if (mDataBlock->correctMuzzleVector)
      if (GameConnection * gc = getControllingClient())
         if (gc->isFirstPerson() && !gc->isAIControlled())
            if (getCorrectedAim(mat, vec))
               return;*/
   GameConnection * gc = getControllingClient();
   if (gc && !gc->isAIControlled())
   {
      bool fp = gc->isFirstPerson();
      if ((fp && mDataBlock->correctMuzzleVector) ||
         (!fp && mDataBlock->correctMuzzleVectorTP))
         if (getCorrectedAim(mat, vec))
            return;
   }

   mat.getColumn(1,vec);
}

void StateItem::getRenderMuzzlePoint(Point3F* pos)
{
   MatrixF mat;
   getRenderMuzzleTransform(&mat);
   mat.getColumn(3,pos);
}

//----------------------------------------------------------------------------

void StateItem::scriptCallback(const char* function)
{
   char buff1[32];
   dSprintf( buff1, 32, "%f", mDataBlock->useRemainderDT ? remainingDt : 0.0f );

				  //datablock		    //us		  //owner
   Con::executef( mDataBlock, function, getIdString(), mMount.object->getIdString());
}


//----------------------------------------------------------------------------

void StateItem::getMountTransform( S32 index, const MatrixF &xfm, MatrixF *outMat )
{
   // Returns mount point to world space transform
   if ( index >= 0 && index < SceneObject::NumMountPoints) {
      S32 ni = mDataBlock->mountPointNode[index];
      if (ni != -1) {
         MatrixF mountTransform = mShapeInstance->mNodeTransforms[ni];
         mountTransform.mul( xfm );
         const Point3F& scale = getScale();

         // The position of the mount point needs to be scaled.
         Point3F position = mountTransform.getPosition();
         position.convolve( scale );
         mountTransform.setPosition( position );

		 //apply our offsets
		 mountTransform.mul(mDataBlock->mountOffset);

         // Also we would like the object to be scaled to the model.
         outMat->mul(mObjToWorld, mountTransform);
         return;
      }
   }

   // Then let SceneObject handle it.
   Parent::getMountTransform( index, xfm, outMat );      
}

void StateItem::getStateItemTransform(MatrixF* mat)
{
   // Image transform in world space
    
   if (mDataBlock) {
      StateItemData& data = *mDataBlock;

	  //get shapebase object of our owner
	  ShapeBase *owner = static_cast<ShapeBase*>(mMount.object);

      MatrixF nmat;
      /*if (data.useEyeOffset && owner->isFirstPerson()) {
         owner->getEyeTransform(&nmat);
         mat->mul(nmat,data.eyeOffset);
      }*/
	  if (data.useEyeNode && isFirstPerson() && data.eyeMountNode != -1) {
         // We need to animate, even on the server, to make sure the nodes are in the correct location.
         mShapeInstance->animate();

         getEyeBaseTransform(&nmat);

         MatrixF mountTransform = mShapeInstance->mNodeTransforms[data.eyeMountNode];

         mat->mul(nmat, mountTransform);
      }
      else if (data.useEyeOffset && isFirstPerson()) {
         getEyeTransform(&nmat);
         mat->mul(nmat,data.eyeOffset);
	  }
      else {
         owner->getRenderMountTransform( 0.0f, mDataBlock->mountPoint, MatrixF::Identity, &nmat );
         mat->mul(nmat,data.mountTransform);
      }
   }
   else
      *mat = mObjToWorld;
}

void StateItem::getStateItemTransform(S32 node,MatrixF* mat)
{
   // Muzzle transform in world space
    
   /*if (mDataBlock) {
      if (node != -1) {
         MatrixF imat;
         getStateItemTransform(&imat);
         mat->mul(imat,mShapeInstance->mNodeTransforms[node]);*/
	if (mDataBlock)
    {
	  StateItemData &data = *mDataBlock;
      if (node != -1)
      {
         MatrixF nmat = mShapeInstance->mNodeTransforms[node];
         MatrixF mmat;

         if (data.useEyeNode && isFirstPerson() && data.eyeMountNode != -1)
         {
            // We need to animate, even on the server, to make sure the nodes are in the correct location.
            mShapeInstance->animate();

            MatrixF emat;
            getEyeBaseTransform(&emat);

            MatrixF mountTransform = mShapeInstance->mNodeTransforms[data.eyeMountNode];
            mountTransform.affineInverse();

            mmat.mul(emat, mountTransform);
         }
         else if (data.useEyeOffset && isFirstPerson())
         {
            MatrixF emat;
            getEyeTransform(&emat);
            mmat.mul(emat,data.eyeOffset);
         }
         else
         {
            MatrixF emat;
            getMountTransform( mDataBlock->mountPoint, MatrixF::Identity, &emat );
            mmat.mul(emat,data.mountTransform);
         }

         mat->mul(mmat, nmat);
      }
      else
         getStateItemTransform(mat);
   }
   else
      *mat = mObjToWorld;
}

void StateItem::getStateItemTransform(StringTableEntry nodeName,MatrixF* mat)
{
   getStateItemTransform(  getNodeIndex(  nodeName ), mat );
}

void StateItem::getMuzzleTransform(MatrixF* mat)
{
   // Muzzle transform in world space
    
   if (mDataBlock)
      getStateItemTransform(mDataBlock->muzzleNode,mat);
   else
      *mat = mObjToWorld;
}


//----------------------------------------------------------------------------

void StateItem::getRenderMountTransform( F32 delta, S32 mountPoint, const MatrixF &xfm, MatrixF *outMat )
{
   // Returns mount point to world space transform
   if ( mountPoint >= 0 && mountPoint < SceneObject::NumMountPoints) {
      S32 ni = mDataBlock->mountPointNode[mountPoint];
      if (ni != -1) {
         MatrixF mountTransform = mShapeInstance->mNodeTransforms[ni];
         mountTransform.mul( xfm );
         const Point3F& scale = getScale();

         // The position of the mount point needs to be scaled.
         Point3F position = mountTransform.getPosition();
         position.convolve( scale );
         mountTransform.setPosition( position );

		 mountTransform.mul(mDataBlock->mountOffset);

         // Also we would like the object to be scaled to the model.
         mountTransform.scale( scale );
         outMat->mul(getRenderTransform(), mountTransform);
         return;
      }
   }

   // Then let SceneObject handle it.
   Parent::getRenderMountTransform( delta, mountPoint, xfm, outMat );   
}


void StateItem::getRenderStateItemTransform(MatrixF* mat, bool noEyeOffset )
{
   // Image transform in world space
    
   if (mDataBlock) 
   {
      StateItemData& data = *mDataBlock;

      MatrixF nmat;
      //if ( !noEyeOffset && data.useEyeOffset && isFirstPerson() ) 
	  if ( data.useEyeNode && isFirstPerson() && data.eyeMountNode != -1 ) {
         getRenderEyeBaseTransform(&nmat);

         MatrixF mountTransform = mShapeInstance->mNodeTransforms[data.eyeMountNode];

         mat->mul(nmat, mountTransform);
      }
      else if ( !noEyeOffset && data.useEyeOffset && isFirstPerson() ) 
      {
         getRenderEyeTransform(&nmat);
         mat->mul(nmat,data.eyeOffset);
      }
      else 
      {
         getRenderMountTransform( 0.0f, data.mountPoint, MatrixF::Identity, &nmat );
         mat->mul(nmat,data.mountTransform);
      }
   }
   else
      *mat = getRenderTransform();
}

void StateItem::getRenderStateItemTransform(S32 node,MatrixF* mat)
{
   // Muzzle transform in world space
    
   /*if (mDataBlock) {
      if (node != -1) {
         MatrixF imat;
         getRenderStateItemTransform(&imat);
         mat->mul(imat,mShapeInstance->mNodeTransforms[node]);*/
   if (mDataBlock)
   {
      if (node != -1)
      {
         StateItemData& data = *mDataBlock;

         MatrixF nmat = mShapeInstance->mNodeTransforms[node];
         MatrixF mmat;

         if ( data.useEyeNode && isFirstPerson() && data.eyeMountNode != -1 )
         {
            MatrixF emat;
            getRenderEyeBaseTransform(&emat);

            MatrixF mountTransform = mShapeInstance->mNodeTransforms[data.eyeMountNode];
            mountTransform.affineInverse();

            mmat.mul(emat, mountTransform);
         }
         else if ( data.useEyeOffset && isFirstPerson() ) 
         {
            MatrixF emat;
            getRenderEyeTransform(&emat);
            mmat.mul(emat,data.eyeOffset);
         }
         else 
         {
            MatrixF emat;
            getRenderMountTransform( 0.0f, data.mountPoint, MatrixF::Identity, &emat );
            mmat.mul(emat,data.mountTransform);
         }

         mat->mul(mmat, nmat);
      }
      else
         getRenderStateItemTransform(mat);
   }
   else
      *mat = getRenderTransform();
}

void StateItem::getRenderStateItemTransform(StringTableEntry nodeName,MatrixF* mat)
{
   getRenderStateItemTransform( getNodeIndex(  nodeName ), mat );
}

void StateItem::getRenderMuzzleTransform(MatrixF* mat)
{
   // Muzzle transform in world space
    
   if (mDataBlock)
      getRenderStateItemTransform(mDataBlock->muzzleNode,mat);
   else
      *mat = getRenderTransform();
}


void StateItem::getRetractionTransform(MatrixF* mat)
{
   // Muzzle transform in world space
    
   if (mDataBlock) {
      if (mDataBlock->retractNode != -1)
         getStateItemTransform(mDataBlock->retractNode,mat);
      else
         getStateItemTransform(mDataBlock->muzzleNode,mat);
   } else {
      *mat = getTransform();
   }
}


void StateItem::getRenderRetractionTransform(MatrixF* mat)
{
   // Muzzle transform in world space
    
   if (mDataBlock) {
      if (mDataBlock->retractNode != -1)
         getRenderStateItemTransform(mDataBlock->retractNode,mat);
      else
         getRenderStateItemTransform(mDataBlock->muzzleNode,mat);
   } else {
      *mat = getRenderTransform();
   }
}


//----------------------------------------------------------------------------

S32 StateItem::getNodeIndex(StringTableEntry nodeName)
{
    
   if (mDataBlock)
      return mDataBlock->mShape->findNode(nodeName);
   else
      return -1;
}

// Modify muzzle if needed to aim at whatever is straight in front of eye.  Let the
// caller know if we actually modified the result.
bool StateItem::getCorrectedAim(const MatrixF& muzzleMat, VectorF* result)
{
   const F32 pullInD = sFullCorrectionDistance;
   const F32 maxAdjD = 500;

   VectorF  aheadVec(0, maxAdjD, 0);

   MatrixF  camMat;
   Point3F  camPos;

   F32 pos = 0;
   GameConnection * gc = getControllingClient();
   if (gc && !gc->isFirstPerson())
      pos = 1.0f;

   getCameraTransform(&pos, &camMat);

   camMat.getColumn(3, &camPos);
   camMat.mulV(aheadVec);
   Point3F  aheadPoint = (camPos + aheadVec);

   // Should we check if muzzle point is really close to camera?  Does that happen?
   Point3F  muzzlePos;
   muzzleMat.getColumn(3, &muzzlePos);

   Point3F  collidePoint;
   VectorF  collideVector;
   disableCollision();
      RayInfo rinfo;
      if (getContainer()->castRay(camPos, aheadPoint, STATIC_COLLISION_TYPEMASK|DAMAGEABLE_TYPEMASK, &rinfo) &&
         (mDot(rinfo.point - mObjToWorld.getPosition(), mObjToWorld.getForwardVector()) > 0)) // Check if point is behind us (could happen in 3rd person view)
         collideVector = ((collidePoint = rinfo.point) - camPos);
      else
         collideVector = ((collidePoint = aheadPoint) - camPos);
   enableCollision();

   // For close collision we want to NOT aim at ground since we're bending
   // the ray here as it is.  But we don't want to pop, so adjust continuously.
   F32   lenSq = collideVector.lenSquared();
   if (lenSq < (pullInD * pullInD) && lenSq > 0.04)
   {
      F32   len = mSqrt(lenSq);
      F32   mid = pullInD;    // (pullInD + len) / 2.0;
      // This gives us point beyond to focus on-
      collideVector *= (mid / len);
      collidePoint = (camPos + collideVector);
   }

   VectorF  muzzleToCollide = (collidePoint - muzzlePos);
   lenSq = muzzleToCollide.lenSquared();
   if (lenSq > 0.04)
   {
      muzzleToCollide *= (1 / mSqrt(lenSq));
      * result = muzzleToCollide;
      return true;
   }
   return false;
}

//----------------------------------------------------------------------------
void StateItem::onRecoil(StateItemData::StateData::RecoilState)
{
}

void StateItem::onStateItem(bool unmount)
{
}

void StateItem::onStateItemStateAnimation(const char* seqName, bool direction, bool scaleToState, F32 stateTimeOutValue)
{
}

void StateItem::onStateItemAnimThreadChange(StateItemData::StateData* lastState, const char* anim, F32 pos, F32 timeScale, bool reset)
{
}

void StateItem::onStateItemAnimThreadUpdate(F32 dt)
{
}
//----------------------------------------------------------------------------

/*void StateItem::setStateItem(	   StateItemData* imageData, 
                           NetStringHandle& skinNameHandle, 
                           bool loaded, 
                           bool ammo, 
                           bool triggerDown,
                           bool altTriggerDown,
						   bool motion,
                           bool genericTrigger0,
                           bool genericTrigger1,
                           bool genericTrigger2,
                           bool genericTrigger3,
                           bool target)
{
   //AssertFatal(imageSlot<MaxMountedImages,"Out of range image slot");

    

   // If we already have this datablock...
   if (mDataBlock == imageData) {
      // Mark that there is not a datablock change pending.
      //nextImage = InvalidImagePtr;
      // Change the skin handle if necessary.
      if (skinNameHandle != skinNameHandle) {
         if (!isGhost()) {
            // Serverside, note the skin handle and tell the client.
            skinNameHandle = skinNameHandle;
            setMaskBits(StateMask);
         }
         else {
            // Clientside, do the reskin.
            skinNameHandle = skinNameHandle;
            if (mShapeInstance) {
               String newSkin = skinNameHandle.getString();
               mShapeInstance->reSkin(newSkin, appliedSkinName);
               appliedSkinName = newSkin;
            }
         }
      }
      return;
   }

   // Check to see if we need to delay image changes until state change.
   if (!isGhost()) {
      if (imageData && mDataBlock && !state->allowImageChange) {
         nextImage = imageData;
         nextSkinNameHandle = skinNameHandle;
         nextLoaded = loaded;
         return;
      }
   }

   // Mark that updates are happenin'.
   setMaskBits(StateMask);

   // Notify script unmount since we're swapping datablocks.
   if (mDataBlock && !isGhost()) {
      F32 dt = mDataBlock->useRemainderDT ? remainingDt : 0.0f;
      mDataBlock->onUnmount_callback( this,  dt );
   }

   // Stop anything currently going on with the 
   resetImageSlot(imageSlot);

   // If we're just unselecting the current shape without swapping
   // in a new one, then bail.
   if (!imageData) {
      onStateItem(true);
      return;
   }

   // Otherwise, init the new shape.
   mDataBlock = imageData;
   state = &mDataBlock->state[0];
   skinNameHandle = skinNameHandle;
   mShapeInstance = new TSShapeInstance(mDataBlock->shape, isClientObject());
   if (isClientObject()) {
      if (mShapeInstance) {
         mShapeInstance->cloneMaterialList();
         String newSkin = skinNameHandle.getString();
         mShapeInstance->reSkin(newSkin, appliedSkinName);
         appliedSkinName = newSkin;
      }
   }
   loaded = loaded;
   ammo = ammo;
   triggerDown = triggerDown;
   altTriggerDown = altTriggerDown;
   target = target;
   motion = motion;
   genericTrigger[0] = genericTrigger0;
   genericTrigger[1] = genericTrigger1;
   genericTrigger[2] = genericTrigger2;
   genericTrigger[3] = genericTrigger3;

   // The server needs the shape loaded for muzzle mount nodes
   // but it doesn't need to run any of the animations.
   ambientThread = 0;
   animThread = 0;
   flashThread = 0;
   spinThread = 0;
   if (isGhost()) {
      if (mDataBlock->isAnimated) {
         animThread = mShapeInstance->addThread();
         mShapeInstance->setTimeScale(animThread,0);
      }
      if (mDataBlock->hasFlash) {
         flashThread = mShapeInstance->addThread();
         mShapeInstance->setTimeScale(flashThread,0);
      }
      if (mDataBlock->ambientSequence != -1) {
         ambientThread = mShapeInstance->addThread();
         mShapeInstance->setTimeScale(ambientThread,1);
         mShapeInstance->setSequence(ambientThread,
                                          mDataBlock->ambientSequence,0);
      }
      if (mDataBlock->spinSequence != -1) {
         spinThread = mShapeInstance->addThread();
         mShapeInstance->setTimeScale(spinThread,1);
         mShapeInstance->setSequence(spinThread,
                                          mDataBlock->spinSequence,0);
      }
   }

   // Set the image to its starting state.
   setState( (U32)0, true);

   // Update the mass for the mount object.
   updateMass();

   // Notify script mount.
   if ( !isGhost() )
   {
      F32 dt = mDataBlock->useRemainderDT ? remainingDt : 0.0f;
      mDataBlock->onMount_callback( this,  dt );
   }
   else
   {
      if ( imageData->lightType == StateItemData::PulsingLight )
         lightStart = Sim::getCurrentTime();
   }

   onStateItem(false);

   // Done.
}

//----------------------------------------------------------------------------
//may not be needed
void StateItem::resetStateItem()
{
   //AssertFatal(imageSlot<MaxMountedImages,"Out of range image slot");

   // Clear out current image
    
   delete mShapeInstance;
   mShapeInstance = 0;
   
   // stop sound
   for(Vector<SFXSource*>::iterator i = mSoundSources.begin(); i != mSoundSources.end(); i++)  
   {  
      SFX_DELETE((*i));  
   }  
   mSoundSources.clear(); 

   for (S32 i = 0; i < MaxStateItemEmitters; i++) {
      StateItemEmitter& em = emitter[i];
      if (bool(em.emitter)) {
         em.emitter->deleteWhenEmpty();
         em.emitter = 0;
      }
   }

   mDataBlock = 0;
   //nextImage = InvalidImagePtr;
   skinNameHandle = NetStringHandle();
   nextSkinNameHandle  = NetStringHandle();
   state = 0;
   delayTime = 0;
   remainingDt = 0;
   ammo = false;
   triggerDown = false;
   altTriggerDown = false;
   loaded = false;
   motion = false;

   for (U32 i=0; i<StateItemData::MaxGenericTriggers; ++i)
   {
      genericTrigger[i] = false;
   }

   lightStart = 0;
   if ( mLight != NULL )
      SAFE_DELETE( mLight );

   updateMass();
}*/

//----------------------------------------------------------------------------

bool StateItem::getTriggerState()
{
   if (isGhost() || !mDataBlock)
      return false;
   return triggerDown;
}

void StateItem::setTriggerState(bool trigger)
{
   if (isGhost() || !mDataBlock)
      return;
    

   if (trigger) {
      if (!triggerDown && mDataBlock) {
         triggerDown = true;
         if (!isGhost()) {
            setMaskBits(StateMask);
            updateState(0);
         }
      }
   }
   else
      if (triggerDown) {
         triggerDown = false;
         if (!isGhost()) {
            setMaskBits(StateMask);
            updateState(0);
         }
      }
}

bool StateItem::getAltTriggerState()
{
   if (isGhost() || !mDataBlock)
      return false;
   return altTriggerDown;
}

void StateItem::setAltTriggerState(bool trigger)
{
   if (isGhost() || !mDataBlock)
      return;
    

   if (trigger) {
      if (!altTriggerDown && mDataBlock) {
         altTriggerDown = true;
         if (!isGhost()) {
            setMaskBits(StateMask);
            updateState(0);
         }
      }
   }
   else
      if (altTriggerDown) {
         altTriggerDown = false;
         if (!isGhost()) {
            setMaskBits(StateMask);
            updateState(0);
         }
      }
}

//----------------------------------------------------------------------------

U32 StateItem::getFireState()
{
    
   // If there is no fire state, then try state 0
   if (mDataBlock && mDataBlock->fireState != -1)
      return mDataBlock->fireState;
   return 0;
}

U32 StateItem::getAltFireState()
{
   // If there is no alternate fire state, then try state 0
   if (mDataBlock && mDataBlock->altFireState != -1)
      return mDataBlock->altFireState;
   return 0;
}

U32  StateItem::getReloadState()
{
   // If there is no reload state, then try state 0
   if (mDataBlock && mDataBlock->reloadState != -1)
      return mDataBlock->reloadState;
   return 0;
}
//----------------------------------------------------------------------------
bool StateItem::hasState(const char* state)
{
   if (!state || !state[0])
      return false;

   if (mDataBlock)
   {
      for (U32 i = 0; i < StateItemData::MaxStates; i++)
      {
         StateItemData::StateData& sd = mDataBlock->state[i];
         if (sd.name && !dStricmp(state, sd.name))
            return true;
      }
   }

   return false;
}

void StateItem::setState(U32 newState,bool force)
{
   if (!mDataBlock)
      return;
    
   // The client never enters the initial fire state on its own, but it
   //  will continue to set that state...
   if (isGhost() && !force && newState == mDataBlock->fireState) {
      if (state != &mDataBlock->state[newState])
         return;
   }

   // The client never enters the initial alternate fire state on its own, but it
   //  will continue to set that state...
   if (isGhost() && !force && newState == mDataBlock->altFireState) {
      if (state != &mDataBlock->state[newState])
         return;
   }

   // The client never enters the initial reload state on its own, but it
   //  will continue to set that state...
   if (isGhost() && !force && newState == mDataBlock->reloadState) {
      if (state != &mDataBlock->state[newState])
         return;
   }

   // Eject shell casing on every state change
   StateItemData::StateData& nextStateData = mDataBlock->state[newState];
   if (isGhost() && nextStateData.ejectShell) {
      ejectShellCasing();
   }


   // Server must animate the shape if it is a firestate...
   if (isServerObject() && (newState == mDataBlock->fireState  || mDataBlock->state[newState].altFire))
      mShapeInstance->animate();

   // If going back into the same state, just reset the timer
   // and invoke the script callback
   if (!force && state == &mDataBlock->state[newState]) {
      delayTime = state->timeoutValue;
      if (state->script && !isGhost())
         scriptCallback(state->script);

      // If this is a flash sequence, we need to select a new position for the
      //  animation if we're returning to that state...
      if (animThread && state->sequence != -1 && state->flashSequence) {
         F32 randomPos = Platform::getRandom();
         mShapeInstance->setPos(animThread, randomPos);
         mShapeInstance->setTimeScale(animThread, 0);
         if (flashThread)
            mShapeInstance->setPos(flashThread, 0);
      }

      return;
   }

   F32 lastDelay = delayTime;
   StateItemData::StateData* lastState = state;
   state = &mDataBlock->state[newState];

   //
   // Do state cleanup first...
   //
   StateItemData::StateData& stateData = *state;
   delayTime = stateData.timeoutValue;

   // Mount pending images
   //not needed here, as i can see
   /*if (nextImage != InvalidImagePtr && stateData.allowImageChange) {
      setImage(nextImage,nextSkinNameHandle,nextLoaded);
      return;
   }*/

   // Stop any looping sounds or animations use in the last state.
   /*if (stateData.sound) {
	   for (Vector<SFXSource*>::iterator i = mSoundSources.begin(); i != mSoundSources.end(); i++){
		   if((*i)->mTrack == stateData.sound){
			   (*i)->stop();
			   mSoundSources.erase(i);
		   }
	   }
   }*/

   // Reset cyclic sequences back to the first frame to turn it off
   // (the first key frame should be it's off state).
   //if (animThread && animThread->getSequence()->isCyclic()) {
   if (animThread && animThread->getSequence()->isCyclic() && (stateData.sequenceNeverTransition || !(stateData.sequenceTransitionIn || lastState->sequenceTransitionOut))) {
      mShapeInstance->setPos(animThread,0);
      mShapeInstance->setTimeScale(animThread,0);
   }
   if (flashThread) {
      mShapeInstance->setPos(flashThread,0);
      mShapeInstance->setTimeScale(flashThread,0);
   }

   // Broadcast the reset
   onStateItemAnimThreadChange(lastState, NULL, 0, 0, true);

   // Check for immediate transitions, but only if we don't need to wait for
   // a time out.  Only perform this wait if we're not forced to change.
   S32 ns;
   if (delayTime <= 0 || !stateData.waitForTimeout) 
   {
	   if ((ns = stateData.transition.loaded[loaded]) != -1) {
		  setState(ns);
		  return;
	   }
	   for (U32 i=0; i<StateItemData::MaxGenericTriggers; ++i)
       {
         if ((ns = stateData.transition.genericTrigger[i][genericTrigger[i]]) != -1) {
            setState( ns);
            return;
         }
       }
	   //if (!imageData.usesEnergy)
		  if ((ns = stateData.transition.ammo[ammo]) != -1) {
			 setState(ns);
			 return;
		  }
	   if ((ns = stateData.transition.target[target]) != -1) {
		  setState( ns);
		  return;
	   }
	   if ((ns = stateData.transition.wet[wet]) != -1) {
		  setState( ns);
		  return;
	   }
	   if ((ns = stateData.transition.motion[motion]) != -1) {
         setState( ns);
         return;
      }
	   if ((ns = stateData.transition.trigger[triggerDown]) != -1) {
		  setState(ns);
		  return;
	   }
	   if ((ns = stateData.transition.altTrigger[altTriggerDown]) != -1) {
		  setState(ns);
		  return;
	   }
   }

   //
   // Initialize the new state...
   //
   //delayTime = stateData.timeoutValue;
   if (stateData.loaded != StateItemData::StateData::IgnoreLoaded)
      loaded = stateData.loaded == StateItemData::StateData::Loaded;
   if (!isGhost() && newState == mDataBlock->fireState) {
      setMaskBits(StateMask);
      fireCount = (fireCount + 1) & 0x7;
   }
   if (!isGhost() && mDataBlock->state[newState].altFire) {
      setMaskBits(StateMask);
      altFireCount = (altFireCount + 1) & 0x7;
   }
   if (!isGhost() && mDataBlock->state[newState].reload) {
      setMaskBits(StateMask);
      reloadCount = (reloadCount + 1) & 0x7;
   }

   // Apply recoil
   if (stateData.recoil != StateItemData::StateData::NoRecoil)
      onRecoil(stateData.recoil);

   // Apply image state animation on mounting shape
   if (stateData.shapeSequence && stateData.shapeSequence[0])
   {
      onStateItemStateAnimation(stateData.shapeSequence, stateData.direction, stateData.shapeSequenceScale, stateData.timeoutValue);
   }

   // Delete any loooping sounds that were in the previous state.
   if (lastState->sound && lastState->sound->getDescription()->mIsLooping)  
   {  
      for(Vector<SFXSource*>::iterator i = mSoundSources.begin(); i != mSoundSources.end(); i++)      
         SFX_DELETE((*i));    

      mSoundSources.clear();  
   }

   // Play sound
   if( stateData.sound && isGhost() )
   {
      const Point3F& velocity = getVelocity();
	   addSoundSource(SFX->createSource( stateData.sound, &getRenderTransform(), &velocity )); 
   }

   // Play animation
   /*if (animThread && stateData.sequence != -1) 
   {
      mShapeInstance->setSequence(animThread,stateData.sequence, stateData.direction ? 0.0f : 1.0f);
      if (stateData.flashSequence == false) 
      {
         F32 timeScale = (stateData.scaleAnimation && stateData.timeoutValue) ?
            mShapeInstance->getDuration(animThread) / stateData.timeoutValue : 1.0f;
         mShapeInstance->setTimeScale(animThread, stateData.direction ? timeScale : -timeScale);
      }
      else
      {
         F32 randomPos = Platform::getRandom();
         mShapeInstance->setPos(animThread, randomPos);
         mShapeInstance->setTimeScale(animThread, 0);

         mShapeInstance->setSequence(flashThread, stateData.sequenceVis, 0);
         mShapeInstance->setPos(flashThread, 0);
         F32 timeScale = (stateData.scaleAnimation && stateData.timeoutValue) ?
            mShapeInstance->getDuration(animThread) / stateData.timeoutValue : 1.0f;
         mShapeInstance->setTimeScale(flashThread, timeScale);*/
   updateAnimThread(lastState);

   // Start particle emitter on the client
   if (isGhost() && stateData.emitter)
      startStateItemEmitter(stateData);

   // Start spin thread
   if (spinThread) {
      switch (stateData.spin) {
       case StateItemData::StateData::IgnoreSpin:
         mShapeInstance->setTimeScale(spinThread, mShapeInstance->getTimeScale(spinThread));
         break;
       case StateItemData::StateData::NoSpin:
         mShapeInstance->setTimeScale(spinThread,0);
         break;
       case StateItemData::StateData::SpinUp:
         if (lastState->spin == StateItemData::StateData::SpinDown)
            delayTime *= 1.0f - (lastDelay / stateData.timeoutValue);
         break;
       case StateItemData::StateData::SpinDown:
         if (lastState->spin == StateItemData::StateData::SpinUp)
            delayTime *= 1.0f - (lastDelay / stateData.timeoutValue);
         break;
       case StateItemData::StateData::FullSpin:
         mShapeInstance->setTimeScale(spinThread,1);
         break;
      }
   }

   // Script callback on server
   if (stateData.script && stateData.script[0] && !isGhost())
      scriptCallback(stateData.script);

   // If there is a zero timeout, and a timeout transition, then
   // go ahead and transition imediately.
   if (!delayTime)
   {
      if ((ns = stateData.transition.timeout) != -1)
      {
         setState(ns);
         return;
      }
   }
}

void StateItem::updateAnimThread(StateItemData::StateData* lastState)
{
   StateItemData::StateData& stateData = *state;

   F32 randomPos = Platform::getRandom();

  if (animThread && stateData.sequence != -1) 
  {
     S32 seqIndex = stateData.sequence;  // Standard index without any prefix
     bool scaleAnim = stateData.scaleAnimation;

     // We're going to apply various prefixes to determine the final sequence to use.
     // Here is the order:
     // shapeBasePrefix_scriptPrefix_baseAnimName
     // shapeBasePrefix_baseAnimName
     // scriptPrefix_baseAnimName
     // baseAnimName

     // Collect the prefixes
     const char* shapeBasePrefix = getAnimPrefix();
     bool hasShapeBasePrefix = shapeBasePrefix && shapeBasePrefix[0];
     const char* scriptPrefix = getScriptAnimPrefix().getString();
     bool hasScriptPrefix = scriptPrefix && scriptPrefix[0];

     // Find the final sequence based on the prefix combinations
     if (hasShapeBasePrefix || hasScriptPrefix)
     {
        bool found = false;
        String baseSeqName(mShapeInstance->getShape()->getSequenceName(stateData.sequence));

        if (!found && hasShapeBasePrefix && hasScriptPrefix)
        {
           String seqName = String(shapeBasePrefix) + String("_") + String(scriptPrefix) + String("_") + baseSeqName;
           S32 index = mShapeInstance->getShape()->findSequence(seqName);
           if (index != -1)
           {
              seqIndex = index;
              found = true;
           }
        }

        if (!found && hasShapeBasePrefix)
        {
           String seqName = String(shapeBasePrefix) + String("_") + baseSeqName;
           S32 index = mShapeInstance->getShape()->findSequence(seqName);
           if (index != -1)
           {
              seqIndex = index;
              found = true;
           }
        }

        if (!found && hasScriptPrefix)
        {
           String seqName = String(scriptPrefix) + String("_") + baseSeqName;
           S32 index = mShapeInstance->getShape()->findSequence(seqName);
           if (index != -1)
           {
              seqIndex = index;
              found = true;
           }
        }
     }

     if (seqIndex != -1)
     {
        if (!lastState)
        {
           // No lastState indicates that we are just switching animation sequences, not states.  Transition into this new sequence, but only
           // if it is different than what we're currently playing.
           S32 prevSeq = -1;
           if (animThread->hasSequence())
           {
              prevSeq = mShapeInstance->getSequence(animThread);
           }
           if (seqIndex != prevSeq)
           {
              mShapeInstance->transitionToSequence(animThread, seqIndex, stateData.direction ? 0.0f : 1.0f, mDataBlock->scriptAnimTransitionTime, true);
           }
        }
        else if (!stateData.sequenceNeverTransition && stateData.sequenceTransitionTime && (stateData.sequenceTransitionIn || lastState->sequenceTransitionOut))
        {
           mShapeInstance->transitionToSequence(animThread, seqIndex, stateData.direction ? 0.0f : 1.0f, stateData.sequenceTransitionTime, true);
        }
        else
        {
           mShapeInstance->setSequence(animThread, seqIndex, stateData.direction ? 0.0f : 1.0f);
        }

        if (stateData.flashSequence == false) 
        {
           F32 timeScale = (scaleAnim && stateData.timeoutValue) ?
              mShapeInstance->getDuration(animThread) / stateData.timeoutValue : 1.0f;
           mShapeInstance->setTimeScale(animThread, stateData.direction ? timeScale : -timeScale);

           // Broadcast the sequence change
           String seqName = mShapeInstance->getShape()->getSequenceName(stateData.sequence);
           onStateItemAnimThreadChange(lastState, seqName, stateData.direction ? 0.0f : 1.0f, stateData.direction ? timeScale : -timeScale);
        }
        else
        {
           mShapeInstance->setPos(animThread, randomPos);
           mShapeInstance->setTimeScale(animThread, 0);

           S32 seqVisIndex = stateData.sequenceVis;

           // Go through the same process as the animThread sequence to find the flashThread sequence
           if (hasShapeBasePrefix || hasScriptPrefix)
           {
              bool found = false;
              String baseVisSeqName(mShapeInstance->getShape()->getSequenceName(stateData.sequenceVis));

              if (!found && hasShapeBasePrefix && hasScriptPrefix)
              {
                 String seqName = String(shapeBasePrefix) + String("_") + String(scriptPrefix) + String("_") + baseVisSeqName;
                 S32 index = mShapeInstance->getShape()->findSequence(seqName);
                 if (index != -1)
                 {
                    seqVisIndex = index;
                    found = true;
                 }
              }

              if (!found && hasShapeBasePrefix)
              {
                 String seqName = String(shapeBasePrefix) + String("_") + baseVisSeqName;
                 S32 index = mShapeInstance->getShape()->findSequence(seqName);
                 if (index != -1)
                 {
                    seqVisIndex = index;
                    found = true;
                 }
              }

              if (!found && hasScriptPrefix)
              {
                 String seqName = String(scriptPrefix) + String("_") + baseVisSeqName;
                 S32 index = mShapeInstance->getShape()->findSequence(seqName);
                 if (index != -1)
                 {
                    seqVisIndex = index;
                    found = true;
                 }
              }
           }

           mShapeInstance->setSequence(flashThread, seqVisIndex, 0);
           mShapeInstance->setPos(flashThread, 0);
           F32 timeScale = (scaleAnim && stateData.timeoutValue) ?
              mShapeInstance->getDuration(flashThread) / stateData.timeoutValue : 1.0f;
           mShapeInstance->setTimeScale(flashThread, timeScale);

           // Broadcast the sequence change
           String seqName = mShapeInstance->getShape()->getSequenceName(stateData.sequenceVis);
           onStateItemAnimThreadChange(lastState, seqName, stateData.direction ? 0.0f : 1.0f, stateData.direction ? timeScale : -timeScale);
        }
     }
  }
}
//----------------------------------------------------------------------------

void StateItem::updateState(F32 dt)
{
   if (!mDataBlock)
      return;

   if(!state)
      return;

   remainingDt = dt;
   F32 elapsed;

TICKAGAIN:

   StateItemData::StateData& stateData = *state;

   if ( delayTime > dt )
      elapsed = dt;
   else
      elapsed = delayTime;

   dt = elapsed;
   remainingDt -= elapsed;

   delayTime -= dt;

   // Energy management
   if (mDataBlock->usesEnergy) 
   {
      F32 newEnergy = getEnergyLevel() - stateData.energyDrain * dt;
      if (newEnergy < 0)
         newEnergy = 0;
      setEnergyLevel(newEnergy);

      if (!isGhost()) 
      {
         bool ammo = newEnergy > mDataBlock->minEnergy;
         if (ammo != ammo) 
         {
            setMaskBits(StateMask);
            ammo = ammo;
         }
      }
   }

   // Check for transitions. On some states we must wait for the
   // full timeout value before moving on.
   if (delayTime <= 0 || !stateData.waitForTimeout) 
   {
      S32 ns;

	  if ((ns = stateData.transition.loaded[loaded]) != -1) 
         setState(ns);
	  else if ((ns = stateData.transition.genericTrigger[0][genericTrigger[0]]) != -1)
         setState(ns);
      else if ((ns = stateData.transition.genericTrigger[1][genericTrigger[1]]) != -1)
         setState(ns);
      else if ((ns = stateData.transition.genericTrigger[2][genericTrigger[2]]) != -1)
         setState(ns);
      else if ((ns = stateData.transition.genericTrigger[3][genericTrigger[3]]) != -1)
         setState(ns);
      else if ((ns = stateData.transition.ammo[ammo]) != -1)  
         setState(ns);
      else if ((ns = stateData.transition.target[target]) != -1) 
         setState(ns);
      else if ((ns = stateData.transition.wet[wet]) != -1) 
         setState(ns);
	  else if ((ns = stateData.transition.motion[motion]) != -1)
         setState(ns);
      else if ((ns = stateData.transition.trigger[triggerDown]) != -1) 
         setState(ns);
      else if ((ns = stateData.transition.altTrigger[altTriggerDown]) != -1) 
         setState(ns);
      else if (delayTime <= 0 && (ns = stateData.transition.timeout) != -1)  
         setState(ns);
   }

   // Update the spinning thread timeScale
   if (spinThread) 
   {
	  float timeScale;

	  switch (stateData.spin) 
	  {
		 case StateItemData::StateData::IgnoreSpin:
		 case StateItemData::StateData::NoSpin:
		 case StateItemData::StateData::FullSpin: 
		 {
			timeScale = 0;
			mShapeInstance->setTimeScale(spinThread, mShapeInstance->getTimeScale(spinThread));
			break;
		 }

		 case StateItemData::StateData::SpinUp: 
		 {
			timeScale = 1.0f - delayTime / stateData.timeoutValue;
			mShapeInstance->setTimeScale(spinThread,timeScale);
			break;
		 }

		 case StateItemData::StateData::SpinDown: 
		 {
			timeScale = delayTime / stateData.timeoutValue;
			mShapeInstance->setTimeScale(spinThread,timeScale);
			break;
		 }
	  }
   }

   if ( remainingDt > 0.0f && delayTime > 0.0f && mDataBlock->useRemainderDT && dt != 0.0f )
      goto TICKAGAIN;
}


//----------------------------------------------------------------------------

void StateItem::updateAnimation(F32 dt)
{
   //if (!mMountedImageList[imageSlot].dataBlock)
   //   return;
    
   // Advance animation threads
   if (ambientThread)
      mShapeInstance->advanceTime(dt,ambientThread);
   if (animThread)
      mShapeInstance->advanceTime(dt,animThread);
   if (spinThread)
      mShapeInstance->advanceTime(dt,spinThread);
   if (flashThread)
      mShapeInstance->advanceTime(dt,flashThread);

   // Broadcast the update
   onStateItemAnimThreadUpdate(dt);

   updateSoundSources(getRenderTransform());

   // Particle emission
   for (S32 i = 0; i < MaxImageEmitters; i++) {
      StateItemEmitter& em = emitter[i];
      if (bool(em.emitter)) {
         if (em.time > 0) {
            em.time -= dt;

            MatrixF mat;
            getRenderStateItemTransform(em.node,&mat);
            Point3F pos,axis;
            mat.getColumn(3,&pos);
            mat.getColumn(1,&axis);
            em.emitter->emitParticles(pos,true,axis,getVelocity(),(U32) (dt * 1000));
         }
         else {
            em.emitter->deleteWhenEmpty();
            em.emitter = 0;
         }
      }
   }
}

//----------------------------------------------------------------------------

void StateItem::setScriptAnimPrefix(NetStringHandle prefix)
{
   if (mDataBlock) {
      setMaskBits(StateMask);
      scriptAnimPrefix = prefix;
   }
}

NetStringHandle StateItem::getScriptAnimPrefix()
{
   return mDataBlock? scriptAnimPrefix : NetStringHandle();
}
//----------------------------------------------------------------------------

void StateItem::startStateItemEmitter(StateItemData::StateData& state)
{
   StateItemEmitter* bem = 0;
   StateItemEmitter* em = emitter;
   StateItemEmitter* ee = &emitter[MaxImageEmitters];

   // If we are already emitting the same particles from the same
   // node, then simply extend the time.  Otherwise, find an empty
   // emitter slot, or grab the one with the least amount of time left.
   for (; em != ee; em++) {
      if (bool(em->emitter)) {
         if (state.emitter == em->emitter->getDataBlock() && state.emitterNode == em->node) {
            if (state.emitterTime > em->time)
               em->time = state.emitterTime;
            return;
         }
         if (!bem || (bool(bem->emitter) && bem->time > em->time))
            bem = em;
      }
      else
         bem = em;
   }

   bem->time = state.emitterTime;
   bem->node = state.emitterNode;
   bem->emitter = new ParticleEmitter;
   bem->emitter->onNewDataBlock(state.emitter,false);
   if( !bem->emitter->registerObject() )
   {
      bem->emitter.getPointer()->destroySelf();
      bem->emitter = NULL;
   }
}

void StateItem::submitLights( LightManager *lm, bool staticLighting )
{
   if ( staticLighting )
      return;

  //StateItemData *imageData = getMountedImage( i );

  if ( mDataBlock->lightType != StateItemData::NoLight )
  {                  
     F32 intensity;

     switch ( mDataBlock->lightType )
     {
     case StateItemData::ConstantLight:
     case StateItemData::SpotLight:
        intensity = 1.0f;
        break;

     case StateItemData::PulsingLight:
        intensity = 0.5f + 0.5f * mSin( M_PI_F * (F32)Sim::getCurrentTime() / (F32)mDataBlock->lightDuration + lightStart );
        intensity = 0.15f + intensity * 0.85f;
        break;

     case StateItemData::WeaponFireLight:
        {
        S32 elapsed = Sim::getCurrentTime() - lightStart;
        if ( elapsed > mDataBlock->lightDuration )
           return;
        intensity = ( 1.0 - (F32)elapsed / (F32)mDataBlock->lightDuration ) * mDataBlock->lightBrightness;
        break;
        }
     default:
        intensity = 1.0f;
        return;
     }

     if ( !mLight )
        mLight = LightManager::createLightInfo();

     mLight->setColor( mDataBlock->lightColor );
     mLight->setBrightness( intensity );   
     mLight->setRange( mDataBlock->lightRadius );  

     if ( mDataBlock->lightType == StateItemData::SpotLight )
     {
        mLight->setType( LightInfo::Spot );
        // Do we want to expose these or not?
        mLight->setInnerConeAngle( 15 );
        mLight->setOuterConeAngle( 40 );      
     }
     else
        mLight->setType( LightInfo::Point );

     MatrixF imageMat;
     getRenderStateItemTransform( &imageMat );

     mLight->setTransform( imageMat );

     lm->registerGlobalLight( mLight, NULL );         
  }
}


//----------------------------------------------------------------------------

void StateItem::ejectShellCasing()
{
    
   StateItemData* imageData = mDataBlock;

   if (!imageData->casing)
      return;

   MatrixF ejectTrans;
   getImageTransform(  imageData->ejectNode, &ejectTrans );

   Point3F ejectDir = imageData->shellExitDir;
   ejectDir.normalize();

   F32 ejectSpread = mDegToRad( imageData->shellExitVariance );
   MatrixF ejectOrient = MathUtils::createOrientFromDir( ejectDir );

   Point3F randomDir;
   randomDir.x = mSin( gRandGen.randF( -ejectSpread, ejectSpread ) );
   randomDir.y = 1.0;
   randomDir.z = mSin( gRandGen.randF( -ejectSpread, ejectSpread ) );
   randomDir.normalizeSafe();

   ejectOrient.mulV( randomDir );

   MatrixF imageTrans = getRenderTransform();
   imageTrans.mulV( randomDir );

   Point3F shellVel = randomDir * imageData->shellVelocity;
   Point3F shellPos = ejectTrans.getPosition();


   Debris *casing = new Debris;
   casing->onNewDataBlock( imageData->casing, false );
   casing->setTransform( imageTrans );

   if (!casing->registerObject())
      delete casing;

   casing->init( shellPos, shellVel );
}

void StateItem::addSoundSource(SFXSource* source)
{
   if(source != NULL)
   {
      if(mDataBlock->maxConcurrentSounds > 0 && mSoundSources.size() > mDataBlock->maxConcurrentSounds)
      {
         SFX_DELETE(mSoundSources.first());
         mSoundSources.pop_front();
      }
      source->play();
      mSoundSources.push_back(source);
   }
}

void StateItem::updateSoundSources( const MatrixF& renderTransform )
{
   //iterate through sources. if any of them have stopped playing, delete them.
   //otherwise, update the transform
   for (Vector<SFXSource*>::iterator i = mSoundSources.begin(); i != mSoundSources.end(); i++)
   {
      if((*i)->isStopped())
      {
         SFX_DELETE((*i));
         mSoundSources.erase(i);
         i--;
      }
      else
      {
         (*i)->setTransform(renderTransform);
      }
   }
}

ConsoleMethod(StateItem,setTrigger,void,3,3,"%StateItem.setTrigger(bool down)")
{
   object->setTriggerState(dAtob(argv[2]));
}

ConsoleMethod(StateItem,setAltTrigger,void,3,3,"%StateItem.setAltTrigger(bool down)")
{
   object->setAltTriggerState(dAtob(argv[2]));
}

ConsoleMethod(StateItem,getTrigger,bool,2,2,"%primTriggerDown = %StateItem.getTrigger();")
{
   return object->getTriggerState();
}

ConsoleMethod(StateItem,getAltTrigger,bool,2,2,"%altTriggerDown = %StateItem.getAltTrigger();")
{
   return object->getAltTriggerState();
}

void StateItem::setTrigger(bool trigger,bool alt)
{
   if(isServerObject())
   {
      if(alt)
         altTriggerDown = trigger;
      else
         triggerDown = trigger;

      setMaskBits(StateMask);
      updateState(0);
   }
}

ConsoleMethod(StateItem,setState,void,3,3,"%StateItem.setState(string stateName);")
{
   if(!object->isServerObject())
      return;
   S32 state = ((StateItemData*)object->getDataBlock())->lookupState(argv[2]);
   if(state != -1)
   {
      object->setState(state,true);
	  object->setMaskBits(StateItem::StateMask);
   }
}

ConsoleMethod(StateItem,getState,const char*,2,2,"%stateName = %StateItem.getState();")
{
   return object->getState();
}

ConsoleMethod( StateItem, getAmmo, bool, 2, 2, "()")
{
    return object->getAmmoState();
}

ConsoleMethod( StateItem, setAmmo, void, 3, 3, "(bool hasAmmo)")
{
	object->setAmmoState(dAtob(argv[2]));
}

ConsoleMethod( StateItem, getTarget, bool, 2, 2, "()")
{
    return object->getTarget();
}

ConsoleMethod( StateItem, setTarget, void, 3, 3, "(bool hasTarget)")
{
	object->setTarget(dAtob(argv[2]));
}

void StateItem::setTarget(bool hasTarget)
{
	target = hasTarget;
}
ConsoleMethod( StateItem, getWet, bool, 2, 2, "()")
{
    return object->getWetState();
}

ConsoleMethod( StateItem, setWet, void, 3, 3, "(bool isWet)")
{
	object->setWetState(dAtob(argv[2]));
}

ConsoleMethod( StateItem, getLoaded, bool, 2, 2, "()")
{
    return object->getLoadedState();
}

ConsoleMethod( StateItem, setLoaded, void, 3, 3, "(bool isLoaded)")
{
	object->setLoadedState(dAtob(argv[2]));
}

DefineEngineMethod( StateItem, getMuzzleVector, VectorF, ( ),,
   "Get the muzzle vector of the Image mounted in the specified slot.\n"
   "If the Image shape contains a node called 'muzzlePoint', then the muzzle "
   "vector is the forward direction vector of that node's transform in world "
   "space. If no such node is specified, the slot's mount node is used "
   "instead.\n\n"
   "If the correctMuzzleVector and firstPerson flags are set in the Image, "
   "the muzzle vector is computed to point at whatever object is right in "
   "front of the object's 'eye' node.\n"
   "@param slot Image slot to query\n"
   "@return the muzzle vector, or \"0 1 0\" if the slot is invalid\n\n" )
{
   VectorF vec(0, 1, 0);
   object->getMuzzleVector(&vec);

   return vec;
}

DefineEngineMethod( StateItem, getMuzzlePoint, Point3F, ( ),,
   "Get the muzzle position of the Image mounted in the specified slot.\n"
   "If the Image shape contains a node called 'muzzlePoint', then the muzzle "
   "position is the position of that node in world space. If no such node "
   "is specified, the slot's mount node is used instead.\n"
   "@param slot Image slot to query\n"
   "@return the muzzle position, or \"0 0 0\" if the slosetAmot is invalid\n\n" )
{
   Point3F pos(0, 0, 0);
   object->getMuzzlePoint(&pos);

   return pos;
}

void ShapeBase::setItemScriptAnimPrefix(U32 mountNode, NetStringHandle prefix)
{
   for (SceneObject* itr = mMount.list; itr; itr = itr->mMount.link){
	     //parse only for StateItems
	   if(itr->mMount.node == mountNode){
		 StateItem *itm = dynamic_cast<StateItem*>(itr);
		 if(itm){
		    if (itm->mDataBlock) {
			   setMaskBits(ImageMaskN << mountNode);
			   itm->scriptAnimPrefix = prefix;
		    }
		 }
		 return;
	   }
   }
}

NetStringHandle ShapeBase::getItemScriptAnimPrefix(U32 mountNode)
{
   for (SceneObject* itr = mMount.list; itr; itr = itr->mMount.link){
	     //parse only for StateItems
	   if(itr->mMount.node == mountNode){
		 StateItem *itm = dynamic_cast<StateItem*>(itr);
		 if(itm){
		    if (itm->mDataBlock) 
				return itm->scriptAnimPrefix;
			else
				return NetStringHandle();
		 }
	   }
   }
}
//-JR