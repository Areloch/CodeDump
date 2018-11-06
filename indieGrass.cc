//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#include "platform/platform.h"

#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "sceneGraph/sceneState.h"
#include "terrain/terrData.h"
#include "renderInstance/renderPassManager.h"
#include "gfx/gfxDrawUtil.h"
#include "gfx/primBuilder.h"
#include "T3D/gameConnection.h"
#include "gfx/gfxVertexBuffer.h"
#include "gfx/gfxStructs.h"
#include "sceneGraph/lightManager.h"
#include "sceneGraph/lightInfo.h"
#include "materials/shaderData.h"
#include "gfx/gfxTransformSaver.h"
#include "indieGrass.h"
#include "terrain/sky.h"
//#include "envioManager/enviornment.h"

extern bool gEditingMission;

IMPLEMENT_CO_NETOBJECT_V1(indieGrass);

//#define IG_DEBUG

#define IG_COLLISION_MASK   ( TerrainObjectType )

static F32						mCosTable[720];

indieGrass::indieGrass()
{
	// Setup NetObject.
	mTypeMask |= StaticObjectType | StaticTSObjectType | StaticRenderedObjectType;
	mNetFlags.set(Ghostable);

	// Reset Last Render Time.
	mLastRenderTime = 0;

	//render params
	mRender = false;
	mRenderCells = true;
	mRenderVectors = false;
	mRenderTangets = false;
	mLockFrustum = false;
	mRenderFrustum = false;

	mFoliageTexture = NULL;
	mFoliageFile = NULL;

	//grid stuffs
	mCellSize = 4;  //4 meter squared
	mGridArea = 256;
	mGrassPerCell = 4;

	//blade stuffs
	mBaseLength = 1.0f; //half a meter? :o
	mBaseWidth = 0.1f;
	mSteps = 4;
	mVerticalOffset = 0.2f; //vertical offset of the cell level

	//curve stuff
	hOriginCoeff = 1.f;	//controlls the origin point of the tip's curve
	hEndCoeff = 2.f;	//controlls the max endpoint of the tip's curve
	//hDirTan = VectorF(0,0,0); //represents the directional vector for the tip's curve
	//hEndTan = VectorF(-1, -1, -1); //represents the tip's vector for the tip's curve
	hDirTan = 0.f; //represents the directional vector for the tip's curve
	hEndTan = -1.f; //represents the tip's vector for the tip's curve


	//lod
	LODDistance[0] = 0;  //past 0 meters out
	LODDistance[1] = 100;  //past 50 meters out
	LODDistance[2] = 200; //past 80 meters out
	mCullRadius = 300;
	//F32 mFadeInGrad;		
    //F32 mFadeOutGrad;		

	//wiiiind
	mWindStrength = 1.f;	//0 is unaffected, 1 is completely bent from wind

	swayMag = 0.1f;
	swayPhase = 0.f;
	swayTimeRatio = 0.f;
	minSwayTime = 3.0f;
	maxSwayTime = 10.f;

	mCells = NULL;

	mRenderDelegate.bind(this, &indieGrass::renderObject);
}
//------------------------------------------------------------------------------

indieGrass::~indieGrass()
{
	mCells.clear();
}

//------------------------------------------------------------------------------

void indieGrass::initPersistFields()
{
	// Initialise parents' persistent fields.
	Parent::initPersistFields();

	addGroup( "Render" );
	addField( "renderGrass", TypeBool,	Offset( mRender,	indieGrass ) );
	addField( "renderCells", TypeBool,	Offset( mRenderCells,	indieGrass ) );
	addField( "lockFrustum", TypeBool,	Offset( mLockFrustum,	indieGrass ) );
	addField( "bladeTexture", TypeFilename,	Offset( mFoliageFile,	indieGrass ) );
	endGroup( "Render" );

	addGroup( "Grid" );
	addField( "cellSize", TypeF32,	Offset( mCellSize,	indieGrass ) );
	addField( "grassPerCell", TypeS32,	Offset( mGrassPerCell,	indieGrass ) );
	endGroup( "Grid" );

	addGroup( "Curve" );
	addField( "curveOriginCoefficient", TypeF32,	Offset( hOriginCoeff,		indieGrass ) );
	addField( "curveEndCoefficient", TypeF32,	Offset( hEndCoeff,		indieGrass ) );
	//addField( "curveDirectionTangent", TypeF32Vector,	Offset( hDirTan,	indieGrass ) );//controls bending resistance i think
	//addField( "curveEndTangent", TypeF32Vector,	Offset( hEndTan,	indieGrass ) );
	addField( "curveDirectionTangent", TypeF32,	Offset( hDirTan,	indieGrass ) );//controls bending resistance i think
	addField( "curveEndTangent", TypeF32,	Offset( hEndTan,	indieGrass ) );
	endGroup( "Curve" );

	addGroup( "Blade" );
	addField( "baseLength", TypeF32,	Offset( mBaseLength,	indieGrass ) );
	addField( "baseWidth",  TypeF32,	Offset( mBaseWidth,		indieGrass ) );
	addField( "stepCount",  TypeS32,	Offset( mSteps,		    indieGrass ) );
	endGroup( "Blade" );

	addGroup( "LOD" );
	addField( "LOD1MaxDistance", TypeF32,	Offset( LODDistance[0],	indieGrass ) );
	addField( "LOD2MaxDistance", TypeF32,	Offset( LODDistance[1],	indieGrass ) );
	addField( "LOD3MaxDistance", TypeF32,	Offset( LODDistance[2],	indieGrass ) );
	endGroup( "LOD" );
}

//================================================================================
// generation code
//================================================================================
void indieGrass::_findTerrainCallback( SceneObject *obj, void *param )
{
   Vector<TerrainBlock*> *terrains = reinterpret_cast<Vector<TerrainBlock*>*>( param );

   TerrainBlock *terrain = dynamic_cast<TerrainBlock*>( obj );
   if ( terrain )
      terrains->push_back( terrain );
}
void indieGrass::updateCellRenderList()
{
	const S32 cullerMasks = mCuller.ClipPlaneMask/* | FrustrumCuller::FarSphereMask*/;

	mRenderedCells = 0;
	for(U32 j=0; j < mCells.size(); j++)
	{
		 // What will be the world placement bounds for this cell.
		 Box3F bounds = mCells[j]->mBounds;

		 S32 clipMask = mCuller.testBoxVisibility( bounds, cullerMasks, 0 );
		if ( clipMask == -1 )
			mCells[j]->mRender = false;
		else 
		{
			mRenderedCells++;
			//if we're not already being rendered
			if(!mCells[j]->mRender){
				mCells[j]->mDirty = true;
				mCells[j]->mRender = true;
			}
		}
	}
	//if(mRenderedCells)
		//Con::printf("Rendered cells %i", mRenderedCells);
}

void indieGrass::generateCells()
{
	mFoliageTexture.free();

	//get us our terrain....
	//TerrainBlock * terrain = dynamic_cast<TerrainBlock*>(Sim::findObject("Terrain"));

	Vector<TerrainBlock*> terrainBlocks;
	
	bool			CollisionResult;
	RayInfo			RayEvent;
	Point3F nodeStart, nodeEnd;
	TerrainBlock *terrainBlock = NULL;

	U32 steps = (U32)(mGridArea / mCellSize);
	F32 height;
	
	Point3F minPoint, maxPoint, normal, newPos, cellPos;

	Box3F bounds;

	F32   terrainMinHeight = -5000.0f, 
         terrainMaxHeight = 5000.0f;
	
	Point2F start(-mGridArea / 2,-mGridArea / 2);
	Point2F gStart = Point2F(mCeil(start.x / mCellSize) * mCellSize, //start of the whole grid
	mCeil(start.y / mCellSize) * mCellSize);

	//kill the old ones, teehee
	mCells.clear();

	// Reload the texture.
	//since you be a 'tard >: (
	//commented
    //if ( mFoliageFile )
    //   mFoliageTexture.set( mFoliageFile, &GFXDefaultStaticDiffuseProfile, avar("%s() - mFoliageFile (line %d)", __FUNCTION__, __LINE__) );//&GFXMaterialStaticDXT5Profile );

	for(U32 x = 0; x < steps; x++)
	{
		for(U32 y = 0; y < steps; y++)
		{
			newPos = Point3F(gStart.x, gStart.y,0);
			newPos.x += mCellSize * x;
			newPos.y += mCellSize * y;

			bounds.minExtents = Point3F(newPos.x - (mCellSize/2), newPos.y - (mCellSize/2), terrainMinHeight); 
			bounds.maxExtents = Point3F(newPos.x + (mCellSize/2), newPos.y + (mCellSize/2), terrainMaxHeight);

			//newPos = cellPos;

			//terrain fetching
			//------------------------------------------------------------
			//getContainer()->findObjects( bounds, TerrainObjectType, _findTerrainCallback, &terrainBlocks );
			gClientContainer.findObjects( bounds, TerrainObjectType, _findTerrainCallback, &terrainBlocks );
			if ( terrainBlocks.empty() )
				return;

			// Which terrain do I place on?
			 if ( terrainBlocks.size() == 1 )
				terrainBlock = terrainBlocks.first();
			 else
			 {
				for ( U32 i = 0; i < terrainBlocks.size(); i++ )
				{
				   TerrainBlock *terrain = terrainBlocks[i];
				   const Box3F &terrBounds = terrain->getWorldBox();

				   if (  newPos.x < terrBounds.minExtents.x || newPos.x > terrBounds.maxExtents.x ||
						 newPos.y < terrBounds.minExtents.y || newPos.y > terrBounds.maxExtents.y )
					  continue;

				   terrainBlock = terrain;
				   break;
				}
			 }

			 //bail if we have nothing
			if ( !terrainBlock )
				continue;
			//------------------------------------------------------------
			//should have a terrain now, woo

			//newPos.x += mCellSize * x;
			//newPos.y += mCellSize * y;

			Point3F p = getRenderTransform().getPosition();
			
			if (terrainBlock->getNormalAndHeight(Point2F(newPos.x,
				newPos.y), &normal, &height))
			{
				newPos.z = height;
				//newPos.convolve(terrain->getScale());
				//keep it in world
				//terrain->getTransform().mulP(newPos);
			}

			/*Point3F shapePosWorld;
			MatrixF objToWorld = getRenderTransform();
			objToWorld.mulP(cellPos, &shapePosWorld);

			nodeStart = nodeEnd = shapePosWorld;
			nodeStart.z = 2000.f;
			nodeEnd.z = -2000.f;

			//do {
				// Perform Ray Cast Collision on Client.
				CollisionResult = gClientContainer.castRay(nodeStart, nodeEnd, IG_COLLISION_MASK, &RayEvent);

				if (CollisionResult)
				{
					// For now, let's pretend we didn't get a collision.
					CollisionResult = false;

					// Yes, so get it's type.
					U32 CollisionType = RayEvent.object->getTypeMask();
					//Con::printf("%i returned collision mask. terrain is %i", CollisionType, TerrainObjectType);
					// Check Illegal Placements, fail if we hit a disallowed type.
					if ((CollisionType & TerrainObjectType))
					{
						CollisionResult = true;
					}
					else
					{
						continue;
					}
				}*/
			//} while(!CollisionResult);

			// create the node
			cell *newCell = new cell;

			//newPos = RayEvent.point;

			newCell->mTransform.identity();//reset
			newCell->mTransform.setColumn(3, newPos);
			
			//this->getWorldTransform().mulP(newPos);
			newCell->mPosition = newPos;
			//newCell->mNormal = RayEvent.normal;
			newCell->mNormal = normal;
			newCell->mBounds.maxExtents = Point3F(newPos.x + (mCellSize/2),newPos.y + (mCellSize/2),newPos.z + (mCellSize));
			newCell->mBounds.minExtents = Point3F(newPos.x - (mCellSize/2),newPos.y - (mCellSize/2),newPos.z/* - (mCellSize/2)*/);

			minPoint.setMin( newCell->mBounds.minExtents - getRenderPosition() );
		    maxPoint.setMax( newCell->mBounds.maxExtents - getRenderPosition() );

			//newCell->mSeed = RandomGen.randI();

			mCells.push_back(newCell);

			/*do{
				//nodePosition = getPosition();
				//nodePosition = Point3F(gStart.x + x*gridSize.x, gStart.y + y*gridSize.y, 2000);	//give it a starting position
				//Point3F gridPos = Point3F(start.x + (x * gridSize.x), start.y + (y * gridSize.y), 2000);	//give it a starting position

				// Transform into world space coordinates
				Point3F shapePosWorld;
				MatrixF objToWorld = getRenderTransform();
				objToWorld.mulP(nodePos, &shapePosWorld);
				nodePosition = shapePosWorld;

				nodeStart = nodeEnd = nodePosition;
				nodeStart.z = 2000.f;
				nodeEnd.z = -2000.f;

				// Perform Ray Cast Collision on Client.
				CollisionResult = gClientContainer.castRay(nodeStart, nodeEnd, FG_COLL_MASK, &RayEvent);

				if (CollisionResult)
				{
					// For now, let's pretend we didn't get a collision.
					CollisionResult = false;

					// Yes, so get it's type.
					U32 CollisionType = RayEvent.object->getTypeMask();
					Con::printf("%i returned collision mask. terrain is %i", CollisionType, TerrainObjectType);
					// Check Illegal Placements, fail if we hit a disallowed type.
					if ((CollisionType & TerrainObjectType))
					{
						CollisionResult = true;
					}
					else
					{
						continue;
					}
				}
			} while(!CollisionResult);

			// Check for Relocation Problem.
		//	if ( mRetries > 0 )
		//	{
			nodePosition = RayEvent.point;
			Con::printf("Node position %f, %f, %f", nodePosition.x, nodePosition.y, nodePosition.z);
				//mNodes[y].Ci = x;
				//mNodes[y].Cj = y;
				//mNodes[y].Ni = x * y;
				//mNodes[y].normal = RayEvent.normal;
			nodeNormal = RayEvent.normal;

			mNumNodes++;
			Con::printf("Number of grid nodes %i", mNumNodes);
		//	}
		//	else
		//	{
				// Warning.
				//Clint too many warnings here.
				//Con::warnf(ConsoleLogEntry::General, "fxFoliageReplicator - Could not find satisfactory position for Foliage!");

				// Skip to next.
		//		Con::warnf("Ran out of retries! bad!");
		//		continue;
		//	}
			//RandomGen.setSeed(mNodes[y].mSeed);	//set the node's seed for the blades
			//Box3F boundBox;
			//boundBox.min = Point3F(-gridSize.x/2, -gridSize.y/2, gridSize.z);
			//boundBox.max = Point3F(gridSize.x/2, gridSize.y/2, gridSize.z);

			node *Node = new node;	//clear it and ready it for use

			Node->pos0 = nodePosition;
			Node->normal = nodeNormal;
			//Node->bBox = boundBox;

			mNodes.push_back(Node);
			mCurrentNumNodes++;*/
		}
	}

	startTime = Platform::getRealMilliseconds();

	F32 tIdx = 0.0f;

	// No, so setup Tables.
	for (U32 idx = 0; idx < 720; idx++, tIdx+=(F32)(M_2PI/720.0f))
	{
		mCosTable[idx] = mCos(tIdx);
	}

	// No, so choose a random Sway phase.
	swayPhase = RandomGen.randF(0, 719.0f);
	// Set to random Sway Time.
	swayTimeRatio = 719.0f / RandomGen.randF(minSwayTime, maxSwayTime);

	F32 memAllocated =	mCells.size() * sizeof(cell);
	memAllocated +=	mCells.size() * sizeof(cell*);
	Con::printf("indieGrass - Approx. %0.2fMb allocated for %i cells.", memAllocated / 1048576.0f, mCells.size());

	/*mObjBox.min.set(minPoint);
	mObjBox.max.set(maxPoint);
	setTransform(mObjToWorld);*/
}
void indieGrass::generateGrass()
{
	S32 dirtyCells =0, clearedCells = 0, LOD1Cells = 0;
	for(U32 j=0; j < mCells.size(); j++)
	{
		//is it even to be rendered?
		if(mCells[j]->mRender)
		{
			//VectorF camVector = mCells[j]->mPosition - mCuller.mCamPos;
			//F32 dist = getMax( camVector.len(), 0.01f );

			F32 dist = mCuller.getBoxDistance(mCells[j]->mBounds);
			
			if(dist > LODDistance[2]){
				if(mCells[j]->LOD != 3){
					mCells[j]->LOD = 3;
					mCells[j]->mDirty = true;
				}
			}
			else if(dist > LODDistance[1]){
				if(mCells[j]->LOD != 2){
					mCells[j]->LOD = 2;
					mCells[j]->mDirty = true;
				}
			}
			else if(dist > LODDistance[0]){
				if(mCells[j]->LOD != 1){
					mCells[j]->LOD = 1;
					mCells[j]->mDirty = true;
				}
			}

			//do we actually have to generate, or did we do that already?
			if(mCells[j]->mDirty)
			{
				dirtyCells++;
				switch(mCells[j]->LOD)
				{
					case(1):
						LOD1Cells++;
						generateGrassLOD1(mCells[j]);
						break;
					case(2):
						generateGrassLOD2(mCells[j]);
						break;
					case(3):
					default:
						generateGrassLOD3(mCells[j]);
						break;
				}
			}
		}
		else
		{
			//we aren't rendered, so make sure we generate the grass next time we are
			if(mCells[j]->mBlades.size()){ //do we have some grass in here leftover?
				emptyCell(mCells[j]);
				clearedCells++;
			}
		}
	}
	//let us know if we changed anything
	/*if(dirtyCells || clearedCells){
		Con::printf("Dirty cells this update: %i", dirtyCells);
		Con::printf("Cleared cells this update: %i", clearedCells);

		F32 memBlades =	mGrassPerCell * sizeof(cell::blade) * LOD1Cells;
		memBlades +=	mGrassPerCell * sizeof(cell::blade*) * LOD1Cells;
		Con::printf("indieGrass - Approx. %0.4fMb allocated for %i blades of grass per %i LOD 1 cells.", memBlades / 1048576.0f, mGrassPerCell, LOD1Cells);


		F32 memFreed =	clearedCells * sizeof(cell) * mGrassPerCell;
		memFreed +=	clearedCells * sizeof(cell*) * mGrassPerCell;
		Con::printf("indieGrass - Approx. %0.4fMb memory freed from %i cleared cells.", memFreed / 1048576.0f, clearedCells);
	}*/
}
void indieGrass::generateGrassLOD1(cell *mCell)
{
	VectorF wind = getWindDirection();

	//for(U32 j=0; j < mCells.size(); j++)
	//{
	for(U32 x=0; x< mGrassPerCell; x++)
	{
		cell::blade* _blade = new cell::blade();
		//setup the grass base curve

		//we offset for spread later, but this is testing atm
		//RandomGen.setSeed(mCell->mSeed);
		_blade->pos0 = mCell->mPosition;
		_blade->pos0.x += RandomGen.randRangeF(mCellSize/2);
		_blade->pos0.y += RandomGen.randRangeF(mCellSize/2);
		_blade->pos0.z += mVerticalOffset;

		_blade->tan0.x = 0;
		_blade->tan0.y = 0;
		_blade->tan0.z = mBaseLength;//mGrid.mNodes[j].length;

		_blade->tan1.x = wind.x;
		_blade->tan1.y = wind.y;
		_blade->tan1.z = 0;//it stands up initially

		//---------------------------------------------------
		//then set up the hermite curve, which has it's own pos0, tan0, pos1, and tan1

		//---------------------------------------------------
		//this was calculated empirically, so we just guess untill we get numbers that look good
		_blade->H.coef1 = hOriginCoeff;	
		_blade->H.coef2 = hEndCoeff;	
		_blade->H.tan0 = VectorF(hDirTan,hDirTan,hDirTan);	
		_blade->H.tan1 = VectorF(hEndTan,hEndTan,hEndTan);	
		//---------------------------------------------------

		_blade->H.pos0.x = _blade->pos0.x;
		//_blade->H.pos0.y = _blade->pos0.y; not needed
		_blade->H.pos0.z = mBaseLength;

		_blade->H.pos1.x = _blade->H.coef1 * mBaseLength;
		_blade->H.pos1.z = _blade->H.coef2 * mBaseLength;

		//---------------------------------------------------
		//and finally get the actual curve's pos1, using all the stuff we just setup.
		_blade->pos1.x = _blade->pos0.x + VarX(_blade->H, mWindStrength);
		_blade->pos1.y = _blade->pos0.y + VarY(_blade->H, mWindStrength);
		_blade->pos1.z = VarZ(_blade->H, mWindStrength);

		//probably not needed until we get to interaction
		_blade->worldBox.minExtents = Point3F(_blade->pos0.x - mBaseWidth,_blade->pos0.y - mBaseWidth,_blade->pos0.z);
		_blade->worldBox.maxExtents = Point3F(_blade->pos0.x + mBaseWidth,_blade->pos0.y + mBaseWidth,_blade->pos0.z + mBaseLength);

		mCell->mBlades.push_back(_blade);
	}
	mCell->mDirty = false; //flag so we don't 
	//}
}
void indieGrass::generateGrassLOD2(cell *mCell)
{
	VectorF wind = getWindDirection();

	//for(U32 j=0; j < mCells.size(); j++)
	//{
	for(U32 x=0; x< 4; x++)
	{
		cell::blade* _blade = new cell::blade();
		//setup the grass base curve

		//we offset for spread later, but this is testing atm
		_blade->pos0 = Point3F(mCell->mPosition.x-(mCellSize/2), mCell->mPosition.y-(mCellSize/2), mCell->mPosition.z);

		//ugly, but it'll work for now
		switch(x)
		{
		case(0):
			break; //already in place
		case(1):
			_blade->pos0.x += mCellSize;
			break;
		case(2):
			_blade->pos0.x += mCellSize;
			_blade->pos0.y += mCellSize;
			break;
		case(3):
			_blade->pos0.y += mCellSize;
			break;
		}

		_blade->tan0.x = 0;
		_blade->tan0.y = 0;
		_blade->tan0.z = mBaseLength;//mGrid.mNodes[j].length;

		_blade->tan1.x = wind.x;
		_blade->tan1.y = wind.y;
		_blade->tan1.z = 1 - wind.len();

		//---------------------------------------------------
		//then set up the hermite curve, which has it's own pos0, tan0, pos1, and tan1

		//---------------------------------------------------
		//this was calculated empirically, so we just guess untill we get numbers that look good
		_blade->H.coef1 = hOriginCoeff;	
		_blade->H.coef2 = hEndCoeff;	
		_blade->H.tan0 = VectorF(hDirTan,hDirTan,hDirTan);	
		_blade->H.tan1 = VectorF(hEndTan,hEndTan,hEndTan);
		//---------------------------------------------------

		_blade->H.pos0.x = _blade->pos0.x;
		//_blade->H.pos0.y = _blade->pos0.y; not needed
		_blade->H.pos0.z = mBaseLength;

		_blade->H.pos1.x = _blade->H.coef1 * mBaseLength;
		_blade->H.pos1.z = _blade->H.coef2 * mBaseLength;

		//---------------------------------------------------
		//and finally get the actual curve's pos1, using all the stuff we just setup.
		_blade->pos1.x = _blade->pos0.x + VarX(_blade->H, mWindStrength);
		_blade->pos1.y = _blade->pos0.y + VarY(_blade->H, mWindStrength);
		_blade->pos1.z = VarZ(_blade->H, mWindStrength);

		_blade->worldBox.minExtents = _blade->worldBox.maxExtents = _blade->pos0;

		mCell->mBlades.push_back(_blade);
	}
	mCell->mDirty = false; //flag so we don't 
	//}
}
void indieGrass::generateGrassLOD3(cell *mCell)
{
	VectorF wind = getWindDirection();

	cell::blade* _blade = new cell::blade();

	_blade->pos0 = mCell->mPosition;

	_blade->tan0.x = 0;
	_blade->tan0.y = 0;
	_blade->tan0.z = mBaseLength;

	_blade->tan1.x = wind.x;
	_blade->tan1.y = wind.y;
	_blade->tan1.z = 1 - wind.len();

	_blade->H.coef1 = hOriginCoeff;	
	_blade->H.coef2 = hEndCoeff;	
	_blade->H.tan0 = VectorF(hDirTan,hDirTan,hDirTan);	
	_blade->H.tan1 = VectorF(hEndTan,hEndTan,hEndTan);

	_blade->H.pos0.x = _blade->pos0.x;
	_blade->H.pos0.z = mBaseLength;

	_blade->H.pos1.x = _blade->H.coef1 * mBaseLength;
	_blade->H.pos1.z = _blade->H.coef2 * mBaseLength;
	_blade->pos1.x = _blade->pos0.x + VarX(_blade->H, mWindStrength);
	_blade->pos1.y = _blade->pos0.y + VarY(_blade->H, mWindStrength);
	_blade->pos1.z = VarZ(_blade->H, mWindStrength);

	_blade->worldBox.minExtents = _blade->worldBox.maxExtents = _blade->pos0;

	mCell->mBlades.push_back(_blade);

	mCell->mDirty = false; //flag so we don't 
}
//================================================================================
// Hermite Curve code
//================================================================================
inline F32 indieGrass::H0( const F32 &t)
{
	return (1-3*(t*t) + 2*(t*t*t));
}
inline F32 indieGrass::H1( const F32 &t)
{
	return (3*(t*t) - 2*(t*t*t));
}
inline F32 indieGrass::H_0( const F32 &t)
{
	return (t - 2*(t*t) + (t*t*t));
}
inline F32 indieGrass::H_1( const F32 &t)
{
	return (-(t*t) + (t*t*t));
}

//all this crap is to calculate the 'interpolation curve' that the grass moves along to simulate the swaying
//or crushing animation.
F32 indieGrass::VarZ2D(Hermite H, F32 wind)
{
	return H0(wind) * H.pos0.z + H1(wind) * H.pos1.z + H_0(wind) * H.tan0.z + H_1(wind) * H.tan1.z;
}

F32 indieGrass::VarX2D(Hermite H, F32 wind)
{
	return H0(wind) * H.pos0.x + H1(wind) * H.pos1.x + H_0(wind) * H.tan0.x + H_1(wind) * H.tan1.x;
}

F32 indieGrass::VarX(Hermite H, F32 wind)
{
	VectorF windDir = getWindDirection();
	return VarX2D(H, wind) * (windDir.x / wind);
}

F32 indieGrass::VarY(Hermite H, F32 wind)
{
	VectorF windDir = getWindDirection();
	return VarX2D(H, wind) * (windDir.z / wind);
}

F32 indieGrass::VarZ(Hermite H, F32 wind)
{
	return VarZ2D(H, wind);
}

//replace with direct feed from global wind
void indieGrass::processCell(cell *mCell)
{
	VectorF wind = getWindDirection();
	updateWindStrength();

	for(U32 x=0; x< mCell->mBlades.size(); x++)
	{
		mCell->mBlades[x]->tan1.z = 1 - mWindStrength;

		mCell->mBlades[x]->pos1.x = mCell->mBlades[x]->pos0.x + VarX(mCell->mBlades[x]->H, mWindStrength);
		mCell->mBlades[x]->pos1.y = mCell->mBlades[x]->pos0.y + VarY(mCell->mBlades[x]->H, mWindStrength);
		//mCell->mBlades[x]->pos1.z = mCell->mBlades[x]->pos0.z + VarZ(mCell->mBlades[x]->H, wind);
		mCell->mBlades[x]->pos1.z = VarZ(mCell->mBlades[x]->H, mWindStrength);
	}
}
//================================================================================
// Misc core code
//================================================================================

bool indieGrass::onAdd()
{
	if(!Parent::onAdd()) return(false);

	//dynamic_cast<SimSet*>(Sim::findObject("indieGrassSet"))->addObject(this);

	// Set initial bounding box.
	//
	// NOTE:-	Set this box to completely encapsulate your object.
	//			You must reset the world box and set the render transform
	//			after changing this.
	//mObjBox.min.set(-0.5f,-0.5f,-0.5f);
	//mObjBox.max.set(+0.5f,+0.5f,+0.5f);
	// We don't use any bounds.
	mObjBox.minExtents.set(-1e5, -1e5, -1e5);
	mObjBox.maxExtents.set( 1e5,  1e5,  1e5);
	// Reset the World Box.
	resetWorldBox();
	// Set the Render Transform.
	setRenderTransform(mObjToWorld);

	if ( isClientObject() )
    {
		generateCells();
	}

	// Add to Scene.
	addToScene();

	// Return OK.
	return(true);
}

void indieGrass::onRemove()
{
	// Remove from Scene.
	removeFromScene();

	//dynamic_cast<SimSet*>(Sim::findObject("indieGrassSet"))->removeObject(this);

//	mGrid.onRemove();

	// Do Parent.
	Parent::onRemove();
}

void indieGrass::inspectPostApply()
{
	// Set Parent.
	Parent::inspectPostApply();

	// Set fxPortal Mask.
	setMaskBits(indieGrassMask);
}

void indieGrass::onEditorEnable()
{
}

void indieGrass::onEditorDisable()
{
}

//================================================================================
// Rendering code
//================================================================================
bool indieGrass::prepRenderImage(	SceneState* state, const U32 stateKey, const U32 startZone,
										const bool modifyBaseZoneState)
{
	// Return if last state.
	if (isLastState(state, stateKey)) return false;
	// Set Last State.
	setLastState(state, stateKey);

		// Is Object Rendered?
   if (!state->isObjectRendered(this))
		return false;

   //curse you world matrix! currrrseee youuuuu
   GFXTransformSaver saver;

   // Setup the frustum culler.
   if (!mCuller.mSceneState || !mLockFrustum )
   {
		//mCuller.mFarDistance = mCullRadius;
		mCuller.init( state );
   }
   
   ObjectRenderInst *ri = gRenderInstManager->allocInst<ObjectRenderInst>();
   ri->mRenderDelegate = mRenderDelegate;
   ri->state = state;
   ri->type = RenderPassManager::RIT_Foliage;
   gRenderInstManager->addInst( ri );

   // Update the cells.
   updateCellRenderList();
   if(mRenderedCells)
	   generateGrass();

   return true;
}

void indieGrass::renderObject(ObjectRenderInst *ri, BaseMatInstance* overrideMat)
{
	//------------------------------------------------------------------
	// Setup
	//------------------------------------------------------------------
	// Calculate Elapsed Time and take new Timestamp.
	S32 Time = Platform::getVirtualMilliseconds();
	F32 ElapsedTime = (Time - mLastRenderTime) * 0.001f;
	mLastRenderTime = Time;

	
    SceneState* state = ri->state;

	// Return if we don't have a material
	//if (!mFoliageTexture) return;

    // Set up rendering state
    GFX->disableShaders();

	SceneGraphData sgData;

	// Set up projection and world transform info.
    MatrixF proj = GFX->getProjectionMatrix();
    GFX->pushWorldMatrix();
    GFX->multWorld(getRenderTransform());
    MatrixF world = GFX->getWorldMatrix();
    proj.mul(world);
    proj.transpose();
    //GFX->setVertexShaderConstF( 0, (float*)&proj, 4 );
	//mConstBuffer->set( mModelViewProjectConst, proj );

    // Store object and camera transform data
    sgData.objTrans = getRenderTransform();
    sgData.camPos = state->getCameraPosition();

	//------------------------------------------------------------------

	//if we're not acutally making grass, bail
	if(mSteps <= 0)
		return;

	//const F32	MinViewDist		= mViewClosest - mFieldData.mFadeOutRegion;
	//const F32	MaxViewDist		= ClippedViewDistance + mFieldData.mFadeInRegion;

	if(mRender)
	{
		for(U32 j=0; j < mCells.size(); j++)
		{
			if(mCells[j]->mRender)
			{
				// Calculate Fog Alpha.
				//FogAlpha = 1.0f - state->getHazeAndFog(Distance, pFoliageItem->Transform.getPosition().z - state->getCameraPosition().z);

				// Trivially reject the billboard if it's totally transparent.
				//if (FogAlpha < FXFOLIAGE_ALPHA_EPSILON) continue;

				// Yes, so are we fading out?
				/*if (Distance < mFieldData.mViewClosest)
				{
					// Yes, so set fade-out.
					ItemAlpha = 1.0f - ((mFieldData.mViewClosest - Distance) * mFadeOutGradient);
				}
				// No, so are we fading in?
				else if (Distance > ClippedViewDistance)
				{
					// Yes, so set fade-in
					ItemAlpha = 1.0f - ((Distance - ClippedViewDistance) * mFadeInGradient);
				}
				// No, so set full.
				else
				{
					ItemAlpha = 1.0f;
				}
				if (ItemAlpha > FogAlpha) ItemAlpha = FogAlpha;*/
				//-------------------------------------------------
				//update the grass sim
				//processCell(mCells[j]);//do a check later against inreaction/wind updates and fresh rendering
										//if we haven't changed from anything since last frame, 
										//don't bother processing

				switch(mCells[j]->LOD)
				{
					//per-blade rendering
					case(1):
						renderLOD1(sgData, mCells[j]);
						break;
					//2.5d rendering
					case(2):
						//renderLOD2(mCells[j]);
						break;
					//billboard rendering
					case(3):
					default:
						//renderLOD3(mCells[j]);
						break;
				}
			}
		}
	}
	// Used for debug drawing.
 	if(mRenderCells)
	{
		GFXDrawUtil* drawer = GFX->getDrawUtil();
        drawer->clearBitmapModulation();

		for(U32 j=0; j < mCells.size(); j++)
		{
			if(mCells[j]->mRender)
			{
				ColorI clr;
				if(mCells[j]->LOD==1)
					clr = ColorI(0,255,0);
				else if(mCells[j]->LOD==2)
					clr = ColorI(255,255,0);
				else if(mCells[j]->LOD==3)
					clr = ColorI(255,0,0);

				Point3F size = Point3F(mCells[j]->mBounds.len_x()/2,
										mCells[j]->mBounds.len_y()/2,mCells[j]->mBounds.len_z()/2);

				drawer->drawWireCube( size, mCells[j]->mPosition, clr );
			}
		}

		Point3F size = Point3F(mCells[1]->mBounds.len_x()/2,
										mCells[1]->mBounds.len_y()/2,mCells[1]->mBounds.len_z()/2);

		ColorI clr = ColorI(0,0,255);
		drawer->drawWireCube( size, mCuller.mCamPos, clr );
	}
	/*if(mRenderVectors)
	{
		for(U32 j=0; j < mCells.size(); j++)
		{
			if(mCells[j]->mRender)
			{
				if(mCells[j]->LOD == 1)
				{
					F32 pas = 1.0f / mSteps;
					Point3F p;

					//-------------------------------------------------
					//now that we're set, we generate the blade's curve and render
					for(U32 g=0; g < mCells[j]->mBlades.size(); g++)
					{
						Point3F _pos0 = mCells[j]->mBlades[g]->pos0;
						Point3F _pos1 = mCells[j]->mBlades[g]->pos1;
						VectorF _tan0 = mCells[j]->mBlades[g]->tan0;
						VectorF _tan1 = mCells[j]->mBlades[g]->tan1;

						p.x = _pos0.x;	//place the first
						p.y = _pos0.y;
						p.z = _pos0.z;

						//render
						glColor3f(0,0,255);
						glBegin(GL_LINES); //polygons are likely much more effecient
							glVertex3f(p.x, p.y, p.z);
							VectorF v = _tan0;
							v.normalize();
							glVertex3f(p.x*v.x, p.y*v.y, p.z*v.z);
						glEnd();

						glBegin(GL_LINES); //polygons are likely much more effecient
							glVertex3f(_pos1.x, _pos1.y, _pos1.z);
							v = _tan1;
							v.normalize();
							glVertex3f(_pos1.x*v.x, _pos1.y*v.y, _pos1.z*v.z);
						glEnd();
					}
				}
			}
		}
	}

	if(mRenderFrustum)
	{
		glColor3f(0,0,255);
		for(S32 i=0; i<mCuller.mNumClipPlanes-1; i++)
		{
			Point3F planePos;
			F32 len = mCuller.mClipPlane[i].len();
			MatrixF toWorld = getWorldTransform();
			Point3F point = Point3F(mCuller.mClipPlane[i].x,mCuller.mClipPlane[i].y,mCuller.mClipPlane[i].z); 

			toWorld.mulP(point, &planePos);

			Point3F camPos = state->getCameraPosition();

			glBegin(GL_LINE); //polygons are likely much more effecient
				glVertex3f(camPos.x, camPos.y, camPos.z);
				glVertex3f(planePos.x, planePos.y, planePos.z);
			glEnd();
		}
	}*/

	//-----------------------------------------------------------------
	//clear up
	GFX->popWorldMatrix();
}

void indieGrass::renderLOD1(SceneGraphData& sgd, /*MatInstance* mat,*/ cell *mCell)
{
	//------------------------------------------------------------------
	F32 h, h0, h1, h_0, h_1;

	Point3F _pos0, _pos1;
	VectorF _tan0, _tan1;

	F32 xTexScale = 10, yTexScale = 10, mWidth;

	F32 pas = 1.0f / mSteps;
	Point3F p, p1, p2, p3, p4;

    PrimBuild::beginToBuffer(GFXTriangleList, 36);
	//-------------------------------------------------
	//now that we're set, we generate the blade's curve and render

	//-------------------------------------------------
	for(U32 g=0; g < mCell->mBlades.size(); g++)
	{
		_pos0 = mCell->mBlades[g]->pos0;
		_pos1 = mCell->mBlades[g]->pos1;
		_tan0 = mCell->mBlades[g]->tan0;
		_tan1 = mCell->mBlades[g]->tan1;

		p = _pos0;	//place the first

		//render
		for(U32 i=0; i < mSteps; i++)
		{
			mWidth = (mBaseWidth / 2)  - i * ((mBaseWidth / 2) / (mSteps - 1));

			p1 = Point3F(p.x, p.y+mWidth, p.z);
			p2 = Point3F(p.x, p.y-mWidth, p.z);

			h = i*pas;
			h0 = H0(h);
			h_0 = H_0(h);
			h1 = H1(h);
			h_1 = H_1(h);

			p.x = h0 * _pos0.x + h1 * _pos1.x + h_0 * _tan0.x + h_1 * _tan1.x;
			p.y = h0 * _pos0.y + h1 * _pos1.y + h_0 * _tan0.y + h_1 * _tan1.y;
			p.z = h0 * _pos0.z + h1 * _pos1.z + h_0 * _tan0.z + h_1 * _tan1.z;

			p3 = Point3F(p.x, p.y+mWidth, p.z);
			p4 = Point3F(p.x, p.y-mWidth, p.z);

			//and assemble this segment's quad
			PrimBuild::texCoord2f(0, 0);
			PrimBuild::vertex3fv(p1);
			PrimBuild::texCoord2f(0, yTexScale);
			PrimBuild::vertex3fv(p2);
			PrimBuild::texCoord2f(xTexScale, yTexScale);
			PrimBuild::vertex3fv(p3);

			PrimBuild::texCoord2f(xTexScale, yTexScale);
			PrimBuild::vertex3fv(p1);
			PrimBuild::texCoord2f(xTexScale, 0);
			PrimBuild::vertex3fv(p3);
			PrimBuild::texCoord2f(0, 0);
			PrimBuild::vertex3fv(p4);
			//quad made
		}
	}
	//------------------------------------------------------------------
	U32 numPrims;
	GFXVertexBuffer* vertBuff = PrimBuild::endToBuffer(numPrims);
	GFX->setVertexBuffer(vertBuff);

	GFX->setTexture( 0, mFoliageTexture );

	//mMaterial->init(sgd, GFXVertexFlagUV0);

   //while(mMaterial->setupPass(sgd))
   //{
		/*if (gClientSceneGraph->isReflectPass())
		  GFX->setCullMode(GFXCullCW);
		else
		  GFX->setCullMode(GFXCullCCW);*/

		GFX->drawPrimitive(GFXTriangleList, 0, numPrims);
	//}
}

void indieGrass::renderLOD2(cell *mCell)
{
	//------------------------------------------------------------------
	/*F32 h, h0, h1, h_0, h_1;

	F32 pas = 1.0f / mSteps;
	Point3F p,pOne, pTwo;

	//-------------------------------------------------
	//now that that's set, we generate the blade's curve and render
	//4 blades, one in each corner
	for(U32 g=0; g < 2; g++)
	{
		Point3F _pos0[2] = {mCell->mBlades[g]->pos0,mCell->mBlades[g+2]->pos0};
		Point3F _pos1[2] = {mCell->mBlades[g]->pos1,mCell->mBlades[g+2]->pos1};
		VectorF _tan0[2] = {mCell->mBlades[g]->tan0,mCell->mBlades[g+2]->tan0};
		VectorF _tan1[2] = {mCell->mBlades[g]->tan1,mCell->mBlades[g+2]->tan1};

		pOne = _pos0[0];	//place the first
		pTwo = _pos0[1];	//place the second

		//render
		glColor3f(0,255,0);
		glBegin(GL_QUAD_STRIP); //polygons are likely much more effecient
		for(U32 i=0; i < mSteps; i++)
		{
			glVertex3f(pOne.x, pOne.y, pOne.z);
			glVertex3f(pTwo.x, pTwo.y, pTwo.z);

			h = i*pas;
			h0 = H0(h);
			h_0 = H_0(h);
			h1 = H1(h);
			h_1 = H_1(h);

			pOne.x = h0 * _pos0[0].x + h1 * _pos1[0].x + h_0 * _tan0[0].x + h_1 * _tan1[0].x;
			pOne.y = h0 * _pos0[0].y + h1 * _pos1[0].y + h_0 * _tan0[0].y + h_1 * _tan1[0].y;
			pOne.z = h0 * _pos0[0].z + h1 * _pos1[0].z + h_0 * _tan0[0].z + h_1 * _tan1[0].z;

			pTwo.x = h0 * _pos0[1].x + h1 * _pos1[1].x + h_0 * _tan0[1].x + h_1 * _tan1[1].x;
			pTwo.y = h0 * _pos0[1].y + h1 * _pos1[1].y + h_0 * _tan0[1].y + h_1 * _tan1[1].y;
			pTwo.z = h0 * _pos0[1].z + h1 * _pos1[1].z + h_0 * _tan0[1].z + h_1 * _tan1[1].z;
		}
		glEnd();
	}*/
}

void indieGrass::renderLOD3(cell *mCell)
{
	const Point4F	XRotation(1,0,0,0);
	const Point4F	YRotation(0,1,0,0);

	Point3F p;

	/*//billlllllllboaaaaards
	MatrixF	ModelView;

	// Store our Modelview.
	glPushMatrix();

	dglMultMatrix(&mCell->mTransform);
	dglGetModelview(&ModelView);
	ModelView.setColumn(0, XRotation);
	ModelView.setColumn(1, YRotation);
	dglLoadMatrix(&ModelView);

	//still undecided if we'll do the full sim on the billboard or not
	//likely not in the end though

	Point3F _pos0 = mCell->mBlades[0]->pos0;
	/*Point3F _pos1 = mCells[j]->mBlades[0]->pos1;
	VectorF _tan0 = mCells[j]->mBlades[0]->tan0;
	VectorF _tan1 = mCells[j]->mBlades[0]->tan1;*/

	/*p.x = _pos0.x;	//place the first
	p.y = _pos0.y;
	p.z = _pos0.z;

	F32 width = mCellSize/2;
	//S32 billsteps = 2;

	// Set Blend Function.
	/*glColor4f(Luminance,Luminance,Luminance, ItemAlpha);

	// Draw Top part of billboard.
	glTexCoord2f	(LeftTexPos,0);
	glVertex3f		(-Width+SwayOffsetX,SwayOffsetY,Height);
	glTexCoord2f	(RightTexPos,0);
	glVertex3f		(+Width+SwayOffsetX,SwayOffsetY,Height);

	// Set Ground Blend.
	if (mFieldData.mGroundAlpha < 1.0f) glColor4f(Luminance, Luminance, Luminance, mFieldData.mGroundAlpha < ItemAlpha ? mFieldData.mGroundAlpha : ItemAlpha);

	// Draw bottom part of billboard.
	glTexCoord2f	(RightTexPos,1);
	glVertex3f		(+Width,0,0);
	glTexCoord2f	(LeftTexPos,1);
	glVertex3f		(-Width,0,0);*/

	//render
	/*glColor3f(0,255,0);
	glBegin(GL_QUADS); //polygons are likely much more effecient
	//for(U32 i=0; i < billsteps; i++)
	//{
		glVertex3f(p.x-width, p.y, p.z);
		glVertex3f(p.x+width, p.y, p.z);

		/*h = 1*pas;
		h0 = H0(h);
		h_0 = H_0(h);
		h1 = H1(h);
		h_1 = H_1(h);

		p.x = h0 * _pos0.x + h1 * _pos1.x + h_0 * _tan0.x + h_1 * _tan1.x;
		p.y = h0 * _pos0.y + h1 * _pos1.y + h_0 * _tan0.y + h_1 * _tan1.y;
		p.z = h0 * _pos0.z + h1 * _pos1.z + h_0 * _tan0.z + h_1 * _tan1.z;*/

		/*glVertex3f(p.x-width, p.y, p.z+mBaseLength);
		glVertex3f(p.x+width, p.y, p.z+mBaseLength);
	//}
	glEnd();

	// Restore our Modelview.
	glPopMatrix();*/
}

//------------------------------------------------------------------------------

U32 indieGrass::packUpdate(NetConnection * con, U32 mask, BitStream * stream)
{
	// Pack Parent.
	U32 retMask = Parent::packUpdate(con, mask, stream);

	// Write fxPortal Mask Flag.
	if (stream->writeFlag(mask & indieGrassMask))
	{
		// Write Object Transform.
		stream->writeAffineTransform(mObjToWorld);

		//render params
		stream->writeFlag(mRender);
		stream->writeFlag(mRenderCells);
		stream->writeFlag(mRenderVectors);
		stream->writeFlag(mRenderTangets);
		stream->writeFlag(mLockFrustum);
		stream->writeFlag(mRenderFrustum);

		stream->writeString(mFoliageFile);

		//grid stuffs
		stream->write(mCellSize);
		stream->write(mGridArea);
		stream->write(mGrassPerCell);

		//blade stuffs
		stream->write(mBaseLength);
		stream->write(mBaseWidth);
		stream->write(mSteps);
		stream->write(mVerticalOffset);

		//curve stuff
		stream->write(hOriginCoeff);
		stream->write(hEndCoeff);
		stream->write(hDirTan);
		//stream->write(hDirTan.y);
		//stream->write(hDirTan.z);
		stream->write(hEndTan);
		//stream->write(hEndTan.y);
		//stream->write(hEndTan.z);

		//lod
		stream->write(LODDistance[0]);
		stream->write(LODDistance[1]);
		stream->write(LODDistance[2]);
		stream->write(mCullRadius);
		//F32 mFadeInGrad;		
		//F32 mFadeOutGrad;		

		//wiiiind
		stream->write(mWindStrength);

		stream->write(swayMag);
		stream->write(swayPhase);
		stream->write(swayTimeRatio);
		stream->write(minSwayTime);
		stream->write(maxSwayTime);
	}

	// Were done ...
	return(retMask);
}

//------------------------------------------------------------------------------

void indieGrass::unpackUpdate(NetConnection * con, BitStream * stream)
{
	// Unpack Parent.
	Parent::unpackUpdate(con, stream);

	// Read fxPortal Mask Flag.
	if(stream->readFlag())
	{
		MatrixF		ObjectMatrix;

		// Read Object Transform.
		stream->readAffineTransform(&ObjectMatrix);

		// Set Transform.
		setTransform(ObjectMatrix);
  
		// Set bounding box.
		//
		// NOTE:-	Set this box to completely encapsulate your object.
		//			You must reset the world box and set the render transform
		//			after changing this.
		mObjBox.minExtents.set(-1e5, -1e5, -1e5);
		mObjBox.maxExtents.set( 1e5,  1e5,  1e5);
		// Reset the World Box.
		resetWorldBox();
		// Set the Render Transform.
		setRenderTransform(mObjToWorld);

		//--------------------------------------------------------
		//render params
		mRender    = stream->readFlag();
		mRenderCells    = stream->readFlag();
		mRenderVectors    = stream->readFlag();
		mRenderTangets    = stream->readFlag();
		mLockFrustum    = stream->readFlag();
		mRenderFrustum    = stream->readFlag();

		mFoliageFile = stream->readSTString();

		//grid stuffs
		stream->read(&mCellSize);
		stream->read(&mGridArea);
		stream->read(&mGrassPerCell);

		//blade stuffs
		stream->read(&mBaseLength);
		stream->read(&mBaseWidth);
		stream->read(&mSteps);
		stream->read(&mVerticalOffset);

		//curve stuff
		stream->read(&hOriginCoeff);
		stream->read(&hEndCoeff);
		stream->read(&hDirTan);
		//stream->read(&hDirTan.y);
		//stream->read(&hDirTan.z);
		stream->read(&hEndTan);
		//stream->read(&hEndTan.y);
		//stream->read(&hEndTan.z);

		//lod
		stream->read(&LODDistance[0]);
		stream->read(&LODDistance[1]);
		stream->read(&LODDistance[2]);
		stream->read(&mCullRadius);
		//F32 mFadeInGrad;		
		//F32 mFadeOutGrad;		

		//wiiiind
		stream->read(&mWindStrength);

		stream->read(&swayMag);
		stream->read(&swayPhase);
		stream->read(&swayTimeRatio);
		stream->read(&minSwayTime);
		stream->read(&maxSwayTime);

		//clear and then refresh
		//mCells.clear();
		//generateCells();
	}
}

ConsoleFunction(StartiGReplication, void, 1, 1, "startiGReplication()")
{
	// Find the Replicator Set.
	/*SimSet *indieGrassSet = dynamic_cast<SimSet*>(Sim::findObject("indieGrassSet"));

	// Return if Error.
	if (!indieGrassSet)
	{
		// Console Warning.
		Con::warnf("indieGrass - Cannot locate the 'indieGrassSet', this is bad!");
		// Return here.
		return;
	}

	S32 startTime = Platform::getRealMilliseconds();

	// Parse Replication Object(s).
	for (SimSetIterator itr(indieGrassSet); *itr; ++itr)
	{
		
		// Fetch the Replicator Object.
		indieGrass* Replicator = static_cast<indieGrass*>(*itr);
		// Start Client Objects Only.
		if (Replicator->isClientObject()) Replicator->generateGrass();
	}

	float totalTime = Platform::getRealMilliseconds() - startTime;

	// Info ...
	Con::printf("indieGrass - total replication time: %0.3f seconds", totalTime/1000.0f );
	Con::printf("indieGrass - Client Foliage Replication Startup is complete.");*/
}

/*VectorF indieGrass::getWindVelocity()
{
   enviornment* envio = gClientSceneGraph->getCurrentEnv();
   return envio->getWindVelocity();
}*/

VectorF indieGrass::getWindDirection()
{
   Sky* _sky = gClientSceneGraph->getCurrentSky();
   VectorF windVec = _sky->getWindVelocity();

   windVec.normalize();

   return windVec;
}

void indieGrass::updateWindStrength()
{
   elapsedTime = (Platform::getRealMilliseconds() - startTime) / 1000;

   /*if(elapsedTime > windTime){
	   if(windInc){
		   windDec=true;
		   windInc=false;
		   windTime = Sim::getCurrentTime()/1000 + 6 + RandomGen.randRangeF(2);
	   }
	   else if(windDec){
		   windDec=false;
		   windInc=true;
		   windTime = Sim::getCurrentTime()/1000 + 6 + RandomGen.randRangeF(2);
	  }
   }

   if(windDec){
	   mWindStrength = windTime * elapsedTime;
	   if(mWindStrength < 0)
		   mWindStrength = 0;
   }
   else if(windInc){
	   mWindStrength = windTime / elapsedTime;
	   if(mWindStrength > 1)
		   mWindStrength = 1;
   }*/

		// Yes, so calculate Sway Offset.
	/*F32 swayOffset = swayMag * mCosTable[(U32)swayPhase];

	// Animate Sway Phase (Modulus).
	swayPhase = swayPhase + (swayTimeRatio * elapsedTime);
	if (swayPhase >= 720.0f) 
		swayPhase -= 720.0f;*/

	//mWindStrength = swayOffset;

}

inline void indieGrass::DrawCellBox(cell mCell)
{
	Box3F QuadBox = mCell.mBounds;
	// Define our debug box.
	static Point3F BoxPnts[] = {
								  Point3F(0,0,0),
								  Point3F(0,0,1),
								  Point3F(0,1,0),
								  Point3F(0,1,1),
								  Point3F(1,0,0),
								  Point3F(1,0,1),
								  Point3F(1,1,0),
								  Point3F(1,1,1)
								};

	static U32 BoxVerts[][4] = {
								  {0,2,3,1},     // -x
								  {7,6,4,5},     // +x
								  {0,1,5,4},     // -y
								  {3,2,6,7},     // +y
								  {0,4,6,2},     // -z
								  {3,7,5,1}      // +z
								};

	/*static Point3F BoxNormals[] = {
								  Point3F(-1, 0, 0),
								  Point3F( 1, 0, 0),
								  Point3F( 0,-1, 0),
								  Point3F( 0, 1, 0),
								  Point3F( 0, 0,-1),
								  Point3F( 0, 0, 1)
								};*/

	// Select the Box Colour.
	/*if(mCell.LOD == 1)
		glColor3f(0,255,0);
	else if(mCell.LOD == 2)
		glColor3f(255,255,0);
	else if(mCell.LOD == 3)
		glColor3f(255,0,0);

	// Project our Box Points.
	Point3F ProjectionPoints[8];
	for(U32 i = 0; i < 8; i++)
	{
		F32 height = 0.3;
		ProjectionPoints[i].set(BoxPnts[i].x ? QuadBox.max.x : QuadBox.min.x,
								BoxPnts[i].y ? QuadBox.max.y : QuadBox.min.y,
								BoxPnts[i].z ? (height * QuadBox.max.z) + (1-height) * QuadBox.min.z : QuadBox.min.z);

	}

	// Draw the Box.
	for(U32 x = 0; x < 6; x++)
	{
		// Draw a line-loop.
		glBegin(GL_LINE_LOOP);

		for(U32 y = 0; y < 4; y++)
		{
			// Draw Vertex.
			glVertex3f(	ProjectionPoints[BoxVerts[x][y]].x,
						ProjectionPoints[BoxVerts[x][y]].y,
						ProjectionPoints[BoxVerts[x][y]].z);
		}

		glEnd();
	}*/
}

void indieGrass::emptyCell(cell *mCell)
{
	//clear us out
	mCell->mDirty = true; //next time we're to be rendered, generate the grass
	mCell->mBlades.clear(); //since we don't have any anymore
}
