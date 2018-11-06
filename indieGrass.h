//-----------------------------------------------------------------------------
// Torque Game Engine
// 
// Written by Melvyn May, 9th September 2002.
//-----------------------------------------------------------------------------

#ifndef _FXRENDEROBJECT_H_
#define _FXRENDEROBJECT_H_

#ifndef _SCENEOBJECT_H_
	#include "sim/sceneObject.h"
#endif
#ifndef _FRUSTRUMCULLER_H_
   #include "util/frustrumCuller.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
   #include "gfx/gfxTextureHandle.h"
#endif
#ifndef _GFX_GFXPRIMITIVEBUFFER_H_
   #include "gfx/gfxPrimitiveBuffer.h"
#endif
#ifndef _RENDERPASSMANAGER_H_
   #include "renderInstance/renderPassManager.h"
#endif
#ifndef _SCENEDATA_H_
	#include "materials/sceneData.h"
#endif

//#include "sceneGrid/gridManager.h"

struct node;

struct Hermite
{
	Point3F pos0;
	Point3F pos1;
	VectorF tan0;
	VectorF tan1;
	F32 coef1;
	F32 coef2;
};

class cell
{
protected:
   friend class indieGrass;

   struct blade
   {
	  S32		mSeed;		//the seed for each node for the node's blades
	  Box3F     worldBox;

	  Point3F	pos0;
	  Point3F	pos1;
	  VectorF	tan0;
	  VectorF	tan1;

	  Hermite	H;

	  //VectorF   Ei;			//the vector of crushing forces

	  //F32       height;
	  //F32		  width;
   };

   /// This is the x,y index for this cell.
   //not used at the moment
   //Point2I mIndex;

   //normal of this cell
   //used in aligning the grass and the like
   VectorF mNormal;

   /// The worldspace bounding box this cell.
   Box3F mBounds;

   /// List of blades in this cell
   Vector<blade*>  mBlades;

   // the LOD of this cell. Controlls rendering and generation parameters
   S32	LOD;

   MatrixF mTransform;

   Point3F mPosition;

   //is this grid to render it's grass?
   bool mRender;

   /// Used to mark the cell dirty and in need
   /// of a rebuild.
   bool mDirty;

   //used for the placement of grass
   S32 mSeed;

public:

	cell(){}
	~cell(){}

   //const Point2I& shiftIndex( const Point2I& shift ) { return mIndex += shift; }
   
   /// The worldspace bounding box this cell.
   const Box3F& getBounds() const { return mBounds; }

   /// The worldspace bounding box of the renderable
   /// content within this cell.
//   const Box3F& getRenderBounds() const { return mRenderBounds; }

   Point3F getCenter() const { return ( mBounds.minExtents + mBounds.maxExtents ) / 2.0f; }

   VectorF getSize() const { return VectorF( mBounds.len_x() / 2.0f,
                                             mBounds.len_y() / 2.0f,
                                             mBounds.len_z() / 2.0f ); }
};


class indieGrass : public SceneObject
{
private:
	typedef SceneObject		Parent;

protected:

	// Create and use these to specify custom events.
	//
	// NOTE:-	Using these allows you to group the changes into related
	//			events.  No need to change everything if something minor
	//			changes.  Only really important if you have *lots* of these
	//			objects at start-up or you send alot of changes whilst the
	//			game is in progress.
	//
	//			Use "setMaskBits(fxRenderObjectMask)" to signal.

	enum {	indieGrassMask		= (1 << 0),
			indieGrassAnother	= (1 << 1) };

	/*struct iGnode : public node
	{
		S32		mSeed;		//the seed for each node for the node's blades
		Box3F   bBox;

		Point3F	pos0;
		Point3F	pos1;
		VectorF	tan0;
		VectorF	tan1;

		Hermite	H;

		VectorF Ei;			//the vector of crushing forces
		VectorF Vi;			//wind vector.
	};

	Vector<iGnode*>	nodes;*/

	F32				mBaseLength;	//this is the generic length of all grass
	F32				mBaseWidth;
	Point2F			size;
	//Point3F			sizeGrid;	//how big the 'patches' are
	S32				mGrassPerCell;
	F32            mVerticalOffset;

	bool			mRender;
	bool			mRenderGrid;
	bool			mRenderCells;
	bool        	mRenderVectors;
	bool        	mRenderTangets;
	bool            mLockFrustum;
	bool            mRenderFrustum;


	S32				mLastRenderTime;
	U32				mRetries;
	MRandomLCG      RandomGen;

	F32             mCellSize;  //1 meter squared
	F32             mGridArea;
	F32             hOriginCoeff;	//controlls the origin point of the tip's curve
	F32             hEndCoeff;	//controlls the max endpoint of the tip's curve
	//VectorF         hDirTan; //represents the directional vector for the tip's curve
	//VectorF         hEndTan; //represents the tip's vector for the tip's curve
	F32             hDirTan; //represents the directional vector for the tip's curve
	F32             hEndTan; //represents the tip's vector for the tip's curve
	S32             mSteps;  //number of steps in the curve, also controls number of polies per blade

	F32             mWindStrength;

	//used for wind sway
	F32				startTime;
	F32				elapsedTime;
	//F32			windTime;

	F32				swayMag;
	F32				swayPhase;
	F32				swayTimeRatio;
	F32				minSwayTime;
	F32				maxSwayTime;
	
	//bool			windInc;
	//bool			windDec;

	//list of all cells
	Vector<cell*> mCells;

	//the count of currently rendered cells
	S32	mRenderedCells;

	//LOD stuff
	F32 LODDistance[2];     //list of distances for each LOD
	F32 mFadeInGrad;		//the fade in level for each LOD
    F32 mFadeOutGrad;		//the fade out level for each LOD
	F32 mCullRadius;		//the distance that cells are no longer rendered

	//nix later
	VectorF	Vi;	//our universal wind vector
	F32	wStr;	//strength of the wind

	//texture for the lower LOD'd grass
	StringTableEntry  mFoliageFile;
    GFXTexHandle     mFoliageTexture;
	//MatInstance*       mMaterial;

	FrustrumCuller mCuller;

	/// 
   GFXStateBlockRef mStateBlock;

   ObjectRenderInst::RenderDelegate mRenderDelegate;

   /// 
   //not technically needed yet, but eh
   GFXShaderConstBufferRef mConstBuffer;
   GFXShaderConstHandle *mModelViewProjectConst;

public:
	indieGrass();
	~indieGrass();

	inline F32 H0( const F32 &t);
	inline F32 H_0( const F32 &t);
	inline F32 H1( const F32 &t);
	inline F32 H_1( const F32 &t);

	// SceneObject
	void renderObject(ObjectRenderInst*, BaseMatInstance*);
	virtual bool prepRenderImage(SceneState*, const U32 stateKey, const U32 startZone,
								const bool modifyBaseZoneState = false);
	inline void DrawCellBox(cell mCell);
	void renderLOD1(SceneGraphData& sgd, /*MatInstance* mat,*/ cell *mCell);
	void renderLOD2(cell *mCell);
	void renderLOD3(cell *mCell);
	
	void generateGrass();//controlls assigning of LOD's and the like and then calls the sub-generation methods
	void generateGrassLOD1(cell *mCell);
	void generateGrassLOD2(cell *mCell);
	void generateGrassLOD3(cell *mCell);
	void generateCells();
	void updateCellRenderList();
	static void _findTerrainCallback( SceneObject*, void*);

	void processCell(cell *mCell);
	void emptyCell(cell *mCell);

	//curve functions
	F32 VarZ2D(Hermite H, F32 Vi);
	F32 VarX2D(Hermite H, F32 Vi);
	F32 VarX(Hermite H, F32 Vi);
	F32 VarY(Hermite H, F32 Vi);
	F32 VarZ(Hermite H, F32 Vi);
	
	VectorF getWindDirection();
	void    updateWindStrength();

	// SimObject      
	bool onAdd();
	void onRemove();
	void onEditorEnable();
	void onEditorDisable();
	void inspectPostApply();

	// NetObject
	U32 packUpdate(NetConnection *, U32, BitStream *);
	void unpackUpdate(NetConnection *, BitStream *);

	// ConObject.
	static void initPersistFields();

	// Declare Console Object.
	DECLARE_CONOBJECT(indieGrass);
};

#endif // _INDIE_GRASS_H_
