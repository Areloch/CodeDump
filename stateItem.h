//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _StateItem_H_
#define _StateItem_H_

#ifndef _ITEM_H_
   #include "T3D/item.h"
#endif
#ifndef _BOXCONVEX_H_
   #include "collision/boxConvex.h"
#endif
#ifndef _DYNAMIC_CONSOLETYPES_H_
   #include "console/dynamicTypes.h"
#endif

class PhysicsBody;


//----------------------------------------------------------------------------

//-JR
struct StateItemData: public ItemData {
   typedef ItemData Parent;
/*struct StateItemData: public RigidShapeData {
   typedef RigidShapeData Parent;*/
//-JR

   F32 friction;
   F32 elasticity;

   bool sticky;
   F32  gravityMod;
   F32  maxVelocity;

   S32 dynamicTypeField;

   bool        lightOnlyStatic;
   S32         lightType;
   ColorF      lightColor;
   S32         lightTime;
   F32         lightRadius;

   bool        simpleServerCollision;

   //=============================================================
   //States and other image-effective codestuffs here
   //Advanced StateItem Support
   //-JR
   bool		   useStates; //do we use the state machine?

public:
   enum Constants {
      MaxStates    = 31,            ///< We get one less than state bits because of
                                    /// the way data is packed.

      MaxGenericTriggers = 4,       ///< The number of generic triggers for the image.

      NumStateBits = 5,
   };
   enum LightType {
      NoLight = 0,
      ConstantLight,
      SpotLight,
      PulsingLight,
      WeaponFireLight,
      NumLightTypes
   };
   struct StateData {
      StateData();
      const char* name;             ///< State name

      /// @name Transition states
      ///
      /// @{

      ///
      struct Transition {
         S32 loaded[2];             ///< NotLoaded/Loaded
         S32 ammo[2];               ///< Noammo/ammo
         S32 target[2];             ///< target/noTarget
         S32 trigger[2];            ///< Trigger up/down
         S32 altTrigger[2];         ///< Second trigger up/down
         S32 wet[2];                ///< wet/notWet
         S32 motion[2];             ///< NoMotion/Motion
         S32 timeout;               ///< Transition after delay
         S32 genericTrigger[StateItemData::MaxGenericTriggers][2];    ///< Generic trigger Out/In
      } transition;
      bool ignoreLoadedForReady;

      /// @}

      /// @name State attributes
      /// @{

      bool fire;                    ///< Can only have one fire state
      bool altFire;                 ///< Can only have one alternate fire state
      bool reload;                  ///< Can only have one reload state
      bool ejectShell;              ///< Should we eject a shell casing in this state?
      bool allowImageChange;        ///< Can we switch to another image while in this state?
                                    ///
                                    ///  For instance, if we have a rocket launcher, the player
                                    ///  shouldn't be able to switch out <i>while</i> firing. So,
                                    ///  you'd set allowImageChange to false in firing states, and
                                    ///  true the rest of the time.
      bool scaleAnimation;          ///< Scale animation to fit the state timeout
      bool direction;               ///< Animation direction
      bool sequenceTransitionIn;    ///< Do we transition to the state's sequence when we enter the state?
      bool sequenceTransitionOut;   ///< Do we transition to the new state's sequence when we leave the state?
      bool sequenceNeverTransition; ///< Never allow a transition to this sequence.  Often used for a fire sequence.
      F32 sequenceTransitionTime;   ///< The time to transition in or out of a sequence.

      bool waitForTimeout;          ///< Require the timeout to pass before advancing to the next
                                    ///  state.
      F32 timeoutValue;             ///< A timeout value; the effect of this value is determined
                                    ///  by the flags scaleAnimation and waitForTimeout
      F32 energyDrain;              ///< Sets the energy drain rate per second of this state.
                                    ///
                                    ///  Energy is drained at energyDrain units/sec as long as
                                    ///  we are in this state.
      enum LoadedState {
         IgnoreLoaded,              ///< Don't change loaded state.
         Loaded,                    ///< Set us as loaded.
         NotLoaded,                 ///< Set us as not loaded.
         NumLoadedBits = 3          ///< How many bits to allocate to representing this state. (3 states needs 2 bits)
      } loaded;                     ///< Is the image considered loaded?
      enum SpinState {
         IgnoreSpin,                ///< Don't change spin state.
         NoSpin,                    ///< Mark us as having no spin (ie, stop spinning).
         SpinUp,                    ///< Mark us as spinning up.
         SpinDown,                  ///< Mark us as spinning down.
         FullSpin,                  ///< Mark us as being at full spin.
         NumSpinBits = 3            ///< How many bits to allocate to representing this state. (5 states needs 3 bits)
      } spin;                       ///< Spin thread state. (Used to control spinning weapons, e.g. chainguns)
      enum RecoilState {
         NoRecoil,
         LightRecoil,
         MediumRecoil,
         HeavyRecoil,
         NumRecoilBits = 3
      } recoil;                     ///< Recoil thread state.
                                    ///
                                    /// @note At this point, the only check being done is to see if we're in a
                                    ///       state which isn't NoRecoil; ie, no differentiation is made between
                                    ///       Light/Medium/Heavy recoils. Player::onImageRecoil() is the place
                                    ///       where this is handled.
      bool flashSequence;           ///< Is this a muzzle flash sequence?
                                    ///
                                    ///  A muzzle flash sequence is used as a source to randomly display frames from,
                                    ///  so if this is a flashSequence, we'll display a random piece each frame.
      S32 sequence;                 ///< Main thread sequence ID.
                                    ///
                                    ///
      S32 sequenceVis;              ///< Visibility thread sequence.

	  StringTableEntry shapeSequence;  ///< Sequence that is played on mounting shape
      bool shapeSequenceScale;         ///< Should the mounting shape's sequence playback be scaled
                                       ///  to the length of the state.

      const char* script;           ///< Function on datablock to call when we enter this state; passed the id of
                                    ///  the imageSlot.
      ParticleEmitterData* emitter; ///< A particle emitter; this emitter will emit as long as the gun is in this
                                    ///  this state.
      SFXTrack* sound;
      F32 emitterTime;              ///<
      S32 emitterNode;
   };

   /// @name State Data
   /// Individual state data used to initialize struct array
   /// @{
   const char*             fireStateName;

   const char*             stateName                  [MaxStates];

   const char*             stateTransitionLoaded      [MaxStates];
   const char*             stateTransitionNotLoaded   [MaxStates];
   const char*             stateTransitionAmmo        [MaxStates];
   const char*             stateTransitionNoAmmo      [MaxStates];
   const char*             stateTransitionTarget      [MaxStates];
   const char*             stateTransitionNoTarget    [MaxStates];
   const char*             stateTransitionWet         [MaxStates];
   const char*             stateTransitionNotWet      [MaxStates];
   const char*             stateTransitionMotion      [MaxStates];
   const char*             stateTransitionNoMotion    [MaxStates];
   const char*             stateTransitionTriggerUp   [MaxStates];
   const char*             stateTransitionTriggerDown [MaxStates];
   const char*             stateTransitionAltTriggerUp[MaxStates];
   const char*             stateTransitionAltTriggerDown[MaxStates];
   const char*             stateTransitionGeneric0In  [MaxStates];
   const char*             stateTransitionGeneric0Out [MaxStates];
   const char*             stateTransitionGeneric1In  [MaxStates];
   const char*             stateTransitionGeneric1Out [MaxStates];
   const char*             stateTransitionGeneric2In  [MaxStates];
   const char*             stateTransitionGeneric2Out [MaxStates];
   const char*             stateTransitionGeneric3In  [MaxStates];
   const char*             stateTransitionGeneric3Out [MaxStates];
   const char*             stateTransitionTimeout     [MaxStates];
   F32                     stateTimeoutValue          [MaxStates];
   bool                    stateWaitForTimeout        [MaxStates];

   bool                    stateFire                  [MaxStates];
   bool                    stateAlternateFire         [MaxStates];
   bool                    stateReload                [MaxStates];
   bool                    stateEjectShell            [MaxStates];
   F32                     stateEnergyDrain           [MaxStates];
   bool                    stateAllowImageChange      [MaxStates];
   bool                    stateScaleAnimation        [MaxStates];
   bool                    stateSequenceTransitionIn  [MaxStates];
   bool                    stateSequenceTransitionOut [MaxStates];
   bool                    stateSequenceNeverTransition [MaxStates];
   F32                     stateSequenceTransitionTime [MaxStates];
   bool                    stateDirection             [MaxStates];
   StateData::LoadedState  stateLoaded                [MaxStates];
   StateData::SpinState    stateSpin                  [MaxStates];
   StateData::RecoilState  stateRecoil                [MaxStates];
   const char*             stateSequence              [MaxStates];
   bool                    stateSequenceRandomFlash   [MaxStates];

   const char*             stateShapeSequence         [MaxStates];
   bool                    stateScaleShapeSequence    [MaxStates];

   bool                    stateIgnoreLoadedForReady  [MaxStates];

   SFXTrack*               stateSound                 [MaxStates];
   const char*             stateScript                [MaxStates];

   ParticleEmitterData*    stateEmitter               [MaxStates];
   F32                     stateEmitterTime           [MaxStates];
   const char*             stateEmitterNode           [MaxStates];

   /// @}
   
   /// @name Camera Shake ( while firing )
   /// @{
   bool              shakeCamera;
   VectorF           camShakeFreq;
   VectorF           camShakeAmp;         
   /// @}

   /// Maximum number of sounds this image can play at a time.
   /// Any value <= 0 indicates that it can play an infinite number of sounds.
   S32 maxConcurrentSounds; 
   
   /// If true it we will allow multiple timeout transitions to occur within
   /// a single tick ( eg. they have a very small timeout ).
   bool useRemainderDT;

   //
   bool emap;                       ///< Environment mapping on?
   bool correctMuzzleVector;        ///< Adjust 1st person firing vector to eye's LOS point?
   bool correctMuzzleVectorTP;      ///< Adjust 3rd person firing vector to camera's LOS point?
   bool firstPerson;                ///< Render the image when in first person?
   bool useEyeOffset;               ///< In first person, should we use the eyeTransform?
   bool useEyeNode;                 ///< In first person, should we attach the camera to the image's eye node?  Player still ultimately decides on what to do,

   bool animateOnServer;            ///< Indicates that the image should be animated on the server.  In most cases
                                    ///  you'll want this set if you're using useEyeNode.  You may also want to
                                    ///  set this if the muzzlePoint is animated while it shoots.  You can set this
                                    ///  to false even if these previous cases are true if the image's shape is set
                                    ///  up in the correct position and orientation in the 'root' pose and none of
                                    ///  the nodes are animated at key times, such as the muzzlePoint essentially
                                    ///  remaining at the same position at the start of the fire state (it could
                                    ///  animate just fine after the projectile is away as the muzzle vector is only
                                    ///  calculated at the start of the state).  You'll also want to set this to true
                                    ///  if you're animating the camera using an image's 'eye' node -- unless the movement
                                    ///  is very subtle and doesn't need to be reflected on the server.

   F32 scriptAnimTransitionTime;    ///< The amount of time to transition between the previous sequence and new sequence
                                    ///< when the script prefix has changed.

   StringTableEntry  itemAnimPrefix;     ///< Passed along to the mounting shape to modify
                                          ///  animation sequences played in 3rd person. [optional]


   StringTableEntry  shapeName;      ///< Name of shape to render.
   U32               mountPoint;     ///< Mount point for the image.
   MatrixF           mountOffset;    ///< Mount point offset, so we know where the image is.
   MatrixF           eyeOffset;      ///< Offset from eye for first person.

   ProjectileData* projectile;      ///< Information about what projectile this
                                    ///  image fires.

   F32   mass;                      ///< Mass!
   bool  usesEnergy;                ///< Does this use energy instead of ammo?
   F32   minEnergy;                 ///< Minimum energy for the weapon to be operable.
   bool  accuFire;                  ///< Should we automatically make image's aim converge with the crosshair?
   bool  cloakable;                 ///< Is this image cloakable when mounted?
                                    ///
                                    ///  One of: ConstantLight, PulsingLight, WeaponFireLight.
   S32         lightDuration;       ///< The duration in SimTime of Pulsing or WeaponFire type lights.
   F32         lightBrightness;     ///< Brightness of the light ( if it is WeaponFireLight ).
   /// @}

   /// @name Shape Data
   /// @{
   bool shapeIsValid;       ///< Indicates that the shape has been loaded and is valid


   U32 mCRC;                        ///< Checksum of shape.
                                    ///
                                    ///  Calculated by the ResourceManager, see
                                    ///  ResourceManager::load().
   bool computeCRC;                 ///< Should the shape's CRC be checked?
   MatrixF mountTransform;          ///< Transformation to get to the mountNode.
   /// @}

   /// @name Nodes
   /// @{
   S32 retractNode;     ///< Retraction node ID.
                        ///
                        ///  When the player bumps against an object and the image is retracted to
                        ///  avoid having it interpenetrating the object, it is retracted towards
                        ///  this node.
   S32 muzzleNode;      ///< Muzzle node ID.
                        ///
                        ///
   S32 ejectNode;       ///< Ejection node ID.
                        ///
                        ///  The eject node is the node on the image from which shells are ejected.
   S32 emitterNode;     ///< Emitter node ID.
                        ///
                        ///  The emitter node is the node from which particles are emitted.
   S32 eyeMountNode;  ///< eyeMount node ID.  Optionally used to mount an image to the player's
                                 /// eye node for first person.
   S32 eyeNode;       ///< Eye node ID.  Optionally used to attach the camera to for camera motion
                                 ///  control from the image.

   /// @}

   /// @name Animation
   /// @{
   S32 spinSequence;                ///< ID of the spin animation sequence.
   S32 ambientSequence;             ///< ID of the ambient animation sequence.

   bool isAnimated;                 ///< This image contains at least one animated states
   bool hasFlash;                   ///< This image contains at least one muzzle flash animation state
   S32 fireState;                   ///< The ID of the fire state.
   S32 altFireState;                ///< The ID of the alternate fire state.
   S32 reloadState;                 ///< The ID of the reload state
   /// @}

   /// @name Shell casing data
   /// @{
   DebrisData *   casing;              ///< Information about shell casings.

   S32            casingID;            ///< ID of casing datablock.
                                       ///
                                       ///  When the network tells the client about the casing, it
                                       ///  it transmits the ID of the datablock. The datablocks
                                       ///  having previously been transmitted, all the client
                                       ///  needs to do is call Sim::findObject() and look up the
                                       ///  the datablock.

   Point3F        shellExitDir;        ///< Vector along which to eject shells from the image.
   F32            shellExitVariance;   ///< Variance from this vector in degrees.
   F32            shellVelocity;       ///< Velocity with which to eject shell casings.
   /// @}

   /// @name State Array
   ///
   /// State array is initialized onAdd from the individual state
   /// struct array elements.
   ///
   /// @{
   StateData state[MaxStates];   ///< Array of states.
   bool      statesLoaded;       ///< Are the states loaded yet?
   /// @}

   /// @name Callbacks
   /// @{
   DECLARE_CALLBACK( void, onMount, ( ShapeBase* obj, S32 slot, F32 dt ) );
   DECLARE_CALLBACK( void, onUnmount, ( ShapeBase* obj, S32 slot, F32 dt ) );
   /// @}
   //-JR

   StateItemData();
   DECLARE_CONOBJECT(StateItemData);
   //-JR
   bool onAdd();
   bool preload(bool server, String &errorStr);
   S32 lookupState(const char* name);  ///< Get a state by name.
   void inspectPostApply();
   //-JR
   static void initPersistFields();
   virtual void packData(BitStream* stream);
   virtual void unpackData(BitStream* stream);
};


//----------------------------------------------------------------------------

//-JR
class StateItem: public Item
{
protected:
   typedef Item Parent;
//-JR

public:
   enum MaskBits {
      //Advanced StateItem Support -JR 
      /*HiddenMask   = Parent::NextFreeMask,
      ThrowSrcMask = Parent::NextFreeMask << 1,
      PositionMask = Parent::NextFreeMask << 2,
      RotationMask = Parent::NextFreeMask << 3,*/
	   
	  StateMask			= Parent::NextFreeMask << 0,
      ManualStateMask	= Parent::NextFreeMask << 1,
      NextFreeMask		= Parent::NextFreeMask << 2
	  //-JR
   };

protected:
   // Client interpolation data
   struct StateDelta {
      Point3F pos;
      VectorF posVec;
      S32 warpTicks;
      Point3F warpOffset;
      F32     dt;
   };
   StateDelta delta;

   // Static attributes
public:
   StateItemData* mDataBlock;

protected:
   static F32 mGravity;
   bool mStatic;
   bool mRotate;

   //
   VectorF mVelocity;
   bool mAtRest;

   S32 mAtRestCounter;
   static const S32 csmAtRestTimer;

   bool mInLiquid;

   ShapeBase* mCollisionObject;
   U32 mCollisionTimeout;

   PhysicsBody *mPhysicsRep;

   //-JR
   StateItemData::StateData *state;
   StateItemData::StateData *nextState;

   bool loaded;                  ///< Is the image loaded?
   bool nextLoaded;              ///< Is the next state going to result in the image being loaded?
   F32 delayTime;                ///< Time till next state.
   F32 remainingDt;				 ///< Remaining delta time for state transitions
   U32 fireCount;                ///< Fire skip count.
                                 ///
                                 /// This is incremented every time the triggerDown bit is changed,
                                 /// so that the engine won't be too confused if the player toggles the
                                 /// trigger a bunch of times in a short period.
                                 ///
                                 /// @note The network deals with this variable at 3-bit precision, so it
                                 /// can only range 0-7.
                                 ///
                                 /// @see StateItem::setState()

   U32 altFireCount;             ///< Alternate fire skip count.
                                    ///< @see fireCount

   U32 reloadCount;              ///< Reload skip count.
                                    ///< @see fireCount


   bool triggerDown;             ///< Is the trigger down?
   bool altTriggerDown;          ///< Is the other trigger down?

   bool ammo;                    ///< Do we have ammo?
   bool target;                  ///< Do we have a target?
   bool wet;					 //< are we wet?

   bool motion;                  ///< Is the player in motion?

   bool genericTrigger[StateItemData::MaxGenericTriggers];   ///< Generic triggers not assigned to anything in particular.  These


   S32 mountPoint;				 //where are we mounting?
   /// @}

   /// @name 3space
   ///
   /// Handles to threads in the 3space system.
   /// @{
   TSThread *ambientThread;
   TSThread *visThread;
   TSThread *animThread;
   TSThread *flashThread;
   TSThread *spinThread;
   /// @}

   /// @name Effects
   ///
   /// Variables relating to lights, sounds, and particles.
   /// @{
   SimTime lightStart;     ///< Starting time for light flashes.

   bool animLoopingSound;  ///< Are we playing a looping sound?
   Vector<SFXSource*> mSoundSources; ///< Vector of currently playing sounds
   void updateSoundSources(const MatrixF& renderTransform);  
   void addSoundSource(SFXSource* source);

   /// Represent the state of a specific particle emitter on the image.
   struct StateItemEmitter {
      S32 node;
      F32 time;
      SimObjectPtr<ParticleEmitter> emitter;
   };
   StateItemEmitter emitter[MaxImageEmitters];
   //-JR

  protected:
	DECLARE_CALLBACK( void, onStickyCollision, ( const char* objID ));
	DECLARE_CALLBACK( void, onEnterLiquid, ( const char* objID, const char* waterCoverage, const char* liquidType ));
	DECLARE_CALLBACK( void, onLeaveLiquid, ( const char* objID, const char* liquidType ));

  public:

   void registerLights(LightManager * lightManager, bool lightingScene);
   //-JR
   //advanced StateItem support
   /*enum LightType
   {
      NoLight = 0,
      ConstantLight,
      PulsingLight,

      NumLightTypes,
   };*/
   enum LightType {
      NoLight = 0,
      ConstantLight,
      SpotLight,
      PulsingLight,
      WeaponFireLight,
      NumLightTypes
   };

   NetStringHandle scriptAnimPrefix;      ///< The script based anim prefix
   //-JR
   

  private:
   S32         mDropTime;
   LightInfo*  mLight;

  public:

   Point3F mStickyCollisionPos;
   Point3F mStickyCollisionNormal;

   //
  private:
   OrthoBoxConvex mConvex;
   Box3F          mWorkingQueryBox;

   void updateVelocity(const F32 dt);
   void updatePos(const U32 mask, const F32 dt);
   void updateWorkingCollisionSet(const U32 mask, const F32 dt);
   bool buildPolyList(PolyListContext context, AbstractPolyList* polyList, const Box3F &box, const SphereF &sphere);
   void buildConvex(const Box3F& box, Convex* convex);
   //virtual void onDeleteNotify(SimObject*); //may not need? -JR

  protected:
   void _updatePhysics();
   void prepRenderImage(SceneRenderState* state);
   void advanceTime(F32 dt);

  public:
   DECLARE_CONOBJECT(StateItem);


   StateItem();
   ~StateItem();
   static void initPersistFields();
   static void consoleInit();

   bool onAdd();
   void onRemove();
   bool onNewDataBlock( GameBaseData *dptr, bool reload );

   bool isStatic()   { return mStatic; }
   bool isAtRest()   { return mAtRest; }
   bool isRotating() { return mRotate; }
   Point3F getVelocity() const;
   void setVelocity(const VectorF& vel);
   void applyImpulse(const Point3F& pos,const VectorF& vec);
   void setCollisionTimeout(ShapeBase* obj);
   ShapeBase* getCollisionObject()   { return mCollisionObject; };

   void processTick(const Move *move);
   void interpolateTick(F32 delta); //-JR
   virtual void setTransform(const MatrixF &mat);

   U32  packUpdate  (NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn,           BitStream *stream);

   //-JR
   StateItemData* getPendingStateItem();
   bool isFiring();

   /// Returns true if the mounted image is alternate firing
   /// @param   imageSlot   Mountpoint
   bool isAltFiring();

   /// Returns true if the mounted image is reloading
   /// @param   imageSlot   Mountpoint
   bool isReloading();

	bool isReady(U32 ns,U32 depth);
	//bool isMounted(StateItemData* imageData);
	//S32 ShapeBase::getMountSlot(StateItemData* imageData)
	NetStringHandle getSkinTag();
	void setAmmoState(bool ammo);
	bool getAmmoState();
	void setWetState(bool wet);
	bool getWetState();
	/// Sets the image as in motion or not, IE if you wanted a gun not to sway while the player is in motion
    /// @param   imageSlot   Image slot
    /// @param   motion     True if image is in motion
    void setMotionState(bool motion);

    /// Returns true if image is in motion
    /// @param   imageSlot   image slot
    bool getMotionState();

    /// Sets the flag if the image has a target
    /// @param   imageSlot   Image slot
    /// @param   ammo        True if the weapon has a target
    void setTargetState(bool ammo);

    /// Returns true if the image has a target
    /// @param   imageSlot   Image slot
    bool getTargetState();

	void setLoadedState(bool loaded);
	bool getLoadedState();
	/// Set the script animation prefix for the image
    /// @param   prefix           The prefix applied to the image
    void setScriptAnimPrefix(NetStringHandle prefix);

    /// Get the script animation prefix for the image
    NetStringHandle getScriptAnimPrefix();

	void getMuzzleVector(VectorF* vec);
	void getMuzzlePoint(Point3F* pos);
	void getRenderMuzzleVector(VectorF* vec);
	void getRenderMuzzlePoint(Point3F* pos);
	void scriptCallback(const char* function);
	void getMountTransform( S32 index, const MatrixF &xfm, MatrixF *outMat );
	void getStateItemTransform(MatrixF* mat);
	void getStateItemTransform(S32 node,MatrixF* mat);
	void getStateItemTransform(StringTableEntry nodeName,MatrixF* mat);
	void getMuzzleTransform(MatrixF* mat);
	void getRenderMountTransform( F32 delta, S32 mountPoint, const MatrixF &xfm, MatrixF *outMat );
	void getRenderStateItemTransform(MatrixF* mat, bool noEyeOffset = false );
	void getRenderStateItemTransform(S32 node,MatrixF* mat);
	void getRenderStateItemTransform(StringTableEntry nodeName,MatrixF* mat);
	void getRenderMuzzleTransform(MatrixF* mat);
	void getRetractionTransform(MatrixF* mat);
	void getRenderRetractionTransform(MatrixF* mat);
	S32  getNodeIndex(StringTableEntry nodeName);
	bool getCorrectedAim(const MatrixF& muzzleMat, VectorF* result);
	void updateMass();
	virtual void onStateItem(bool unmount);
    virtual void onRecoil(StateItemData::StateData::RecoilState);
    virtual void onStateItemStateAnimation(const char* seqName, bool direction, bool scaleToState, F32 stateTimeOutValue);
    virtual void onStateItemAnimThreadChange(StateItemData::StateData* lastState, const char* anim, F32 pos, F32 timeScale, bool reset=false);
    virtual void onStateItemAnimThreadUpdate(F32 dt);

	/*void setImage(  StateItemData* imageData, 
							   NetStringHandle& skinNameHandle, 
							   bool loaded, 
							   bool ammo, 
							   bool triggerDown,
							   bool altTriggerDown,
							   bool target);*/
	bool getTriggerState();
	void setTriggerState(bool trigger);
	bool getAltTriggerState();
	void setAltTriggerState(bool trigger);
	void setState(U32 newState,bool force = false);
	void updateAnimThread(StateItemData::StateData* lastState=NULL);
	/// Get the animation prefix for the image
    /// @param   imageSlot        Image slot id
    /// @param   imageShapeIndex  Shape index (1st person, etc.) used to look up the prefix text
    virtual const char* getAnimPrefix() { return ""; }

	void updateState(F32 dt);
	void startStateItemEmitter(StateItemData::StateData& state);
	void submitLights( LightManager *lm, bool staticLighting );
	void ejectShellCasing();

	/// @name State manipulation
   /// @{
   /// Check if the given state exists on the mounted Image
   /// @param   imageSlot   Image slot id
   /// @param   state       Image state to check for
   bool hasState(const char* state);

   /// Sets the state of the StateItem
   /// @param   state       State id
   /// @param   force       Force image to state or let it finish then change
   const char* getState(){if(state) return state->name; return "";}

   /// Sets the generic trigger state of the image
   /// @param   imageSlot   Image slot
   /// @param   trigger     Generic trigger number 0-3
   /// @param   state       True if generic trigger is down
   void setGenericTriggerState(U32 trigger, bool state);

   /// Returns the generic trigger state of the image
   /// @param   imageSlot   Image slot
   /// @param   trigger     Generic trigger number 0-3
   bool getGenericTriggerState(U32 trigger);


   /// Advance animation
   /// @param   dt          Change in time since last animation update
   void updateAnimation(F32 dt);

   /// Start up the particle emitter
   /// @param   state   State
   void startEmitter(StateItemData::StateData &state);


   /// Set the state of a trigger
   /// @param   trigger   True if the trigger is depressed
   /// @param   alt       True to set the value of the alt trigger
   void setTrigger(bool trigger, bool alt = false);
   bool getTrigger(bool alt = false){if(alt) return altTriggerDown; return triggerDown;}

   /// Sets the flag that signals the image is loaded with ammo
   /// @param   loaded   True if loaded with ammo
   bool getTarget(){ return target;}
   void setTarget(bool hasTarget);

   void throwCallback(const char* callback);

   U32 getFireState();

      /// Get the alternate firing action state of the image
   /// @param   imageSlot   Image slot id
   U32  getAltFireState();

   /// Get the reload action state of the image
   /// @param   imageSlot   Image slot id
   U32  getReloadState();
   //-JR

//-JR
};

typedef StateItem::LightType StateItemLightType;
DefineEnumType( StateItemLightType );

//-JR
typedef StateItemData::StateData::LoadedState StateItemLoadedState;
typedef StateItemData::StateData::SpinState StateItemSpinState;
typedef StateItemData::StateData::RecoilState StateItemRecoilState;

DefineEnumType( StateItemLoadedState );
DefineEnumType( StateItemSpinState );
DefineEnumType( StateItemRecoilState );
//-JR

#endif