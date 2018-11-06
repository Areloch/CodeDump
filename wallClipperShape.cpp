//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "T3D/WallClipperShape.h"

#include "math/mathIO.h"
#include "math/mathUtils.h"
#include "sim/netConnection.h"
//#include "sceneGraph/sceneState.h"
#include "scene/sceneRenderState.h"
#include "console/consoleTypes.h"
#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "gfx/gfxTransformSaver.h"
#include "renderInstance/renderPassManager.h"
#include "environment/meshRoom.h"
#include "console/console.h"
#include "console/sim.h"
#include "environment/scatterSky.h"
#include "T3D/pointLight.h"
#include "T3D/tsStatic.h"

IMPLEMENT_CO_NETOBJECT_V1(WallClipperShape);

//-----------------------------------------------------------------------------
// Object setup and teardown
//-----------------------------------------------------------------------------
WallClipperShape::WallClipperShape()
{
   // Flag this object so that it will always
   // be sent across the network to clients
   mNetFlags.set( Ghostable | ScopeAlways );

   // Set it as a "static" object that casts shadows
   mTypeMask |= StaticObjectType;

   // Make sure to initialize our TSShapeInstance to NULL
   mShapeInstance = NULL;

	// No segment yet
	mSegmentIdx = -1;

	mRoomName = "";

	// Type not defined yet
//	mClipType = Undefined;

	// No Light
	mLight = NULL;
	mLightId = 0;

	// Wall
	mPosPercent = 0.0f;
}

WallClipperShape::~WallClipperShape()
{
	if(Sim::findObject(mLightId))
		mLight->deleteObject();
}

/*
//-----------------------------------------------------------------------------
// Object Editing
//-----------------------------------------------------------------------------
void WallClipperShape::initPersistFields()
{
   addGroup( "Rendering" );
   addField( "shapeFile",      TypeStringFilename, Offset( mShapeFile, WallClipperShape ) );
   endGroup( "Rendering" );

   // SceneObject already handles exposing the transform
   Parent::initPersistFields();
}

void WallClipperShape::inspectPostApply()
{
   Parent::inspectPostApply();

   // Flag the network mask to send the updates
   // to the client object
   setMaskBits( UpdateMask );
}
*/

bool WallClipperShape::onAdd()
{
   if ( !Parent::onAdd() )
      return false;
/*
   // Set up a 1x1x1 bounding box
   mObjBox.set( Point3F( -0.5f, -0.5f, -0.5f ),
                Point3F(  0.5f,  0.5f,  0.5f ) );

   resetWorldBox();

	_createShape();

   // Add this object to the scene
   addToScene();
*/
    if(isServerObject())
	{
		//fetch the clipper shape, if we can
		TSShape *shape = mShapeInstance->getShape();
		for (U32 i = 0; i < shape->details.size(); i++)  
		{  
		   const TSDetail * detail = &shape->details[i];  
		   S32 ss = detail->subShapeNum;  
		   S32 od = detail->objectDetailNum;  
		  
		   S32 start = shape->subShapeFirstObject[ss];  
		   S32 end   = shape->subShapeNumObjects[ss] + start;  
		   for (U32 j = start; j < end; j++)  
		   {  
			  // Sometimes it is handy to know the name of the mesh it is accessing  
			  const char *name = shape->names[mShapeInstance->mMeshObjects[j].object->nameIndex];  
		  
			  S32 num;
			  String shortName = String::GetTrailingNumber( name, num );
			  if(!dStrcmp(shortName, "Clipper") || !dStrcmp(shortName, "clipper"))
			  {
				  TSMesh * mesh = mShapeInstance->mMeshObjects[j].getMesh(od);  
			  
				  for (U32 k = 0; k < mesh->mNumVerts; k++)  
				  {  
					 Point2F pos = Point2F(mesh->smVertsList[k]->x, mesh->smVertsList[k]->z);
					 mClipVerts.push_back(pos);
				  }  
				  //we got what we need, now bail.
				  return true;
			  }
		   }  
		}
		//if that fails, grab the bounds
		{
			Point2F pos;

			F32 halfWidth = mObjBox.len_x() / 2.0f;
			F32 height = mObjBox.len_z();

			// Vertex 1
			pos.set(-halfWidth, 0.0f);
			mClipVerts.push_back(pos);

			// Vertex 2
			pos.set(halfWidth, 0.0f);
			mClipVerts.push_back(pos);

			// Vertex 3
			pos.set(halfWidth, height);
			mClipVerts.push_back(pos);

			// Vertex 4
			pos.set(-halfWidth, height);
			mClipVerts.push_back(pos);
		}
	}
	// Create clip verts
	// Right now it is hardcoded to support 4 verts to form a clipping
	// rectangle, but that could change to support any shape later
	/*if(isServerObject())
	{
		Point2F pos;

		F32 halfWidth = mObjBox.len_x() / 2.0f;
		F32 height = mObjBox.len_z();

		// Vertex 1
		pos.set(-halfWidth, 0.0f);
		mClipVerts.push_back(pos);

		// Vertex 2
		pos.set(halfWidth, 0.0f);
		mClipVerts.push_back(pos);

		// Vertex 3
		pos.set(halfWidth, height);
		mClipVerts.push_back(pos);

		// Vertex 4
		pos.set(-halfWidth, height);
		mClipVerts.push_back(pos);
	}*/

   return true;
}
/*
void WallClipperShape::onRemove()
{
   // Remove this object from the scene
   removeFromScene();

   // Remove our TSShapeInstance
   if ( mShapeInstance )
      SAFE_DELETE( mShapeInstance );

   Parent::onRemove();
}

void WallClipperShape::setTransform(const MatrixF & mat)
{
   // Let SceneObject handle all of the matrix manipulation
   Parent::setTransform( mat );

   // Dirty our network mask so that the new transform gets
   // transmitted to the client object
   setMaskBits( TransformMask );
}

U32 WallClipperShape::packUpdate( NetConnection *conn, U32 mask, BitStream *stream )
{
   // Allow the Parent to get a crack at writing its info
   U32 retMask = Parent::packUpdate( conn, mask, stream );

   // Write our transform information
   if ( stream->writeFlag( mask & TransformMask ) )
   {
      mathWrite(*stream, getTransform());
      mathWrite(*stream, getScale());
   }

   // Write out any of the updated editable properties
   if ( stream->writeFlag( mask & UpdateMask ) )
   {
      stream->write( mShapeFile );

      // Allow the server object a chance to handle a new shape
      createShape();
   }

   return retMask;
}

void WallClipperShape::unpackUpdate(NetConnection *conn, BitStream *stream)
{
   // Let the Parent read any info it sent
   Parent::unpackUpdate(conn, stream);

   if ( stream->readFlag() )  // TransformMask
   {
      mathRead(*stream, &mObjToWorld);
      mathRead(*stream, &mObjScale);

      setTransform( mObjToWorld );
   }

   if ( stream->readFlag() )  // UpdateMask
   {
      stream->read( &mShapeFile );

      if ( isProperlyAdded() )
         createShape();
   }
}

//-----------------------------------------------------------------------------
// Object Rendering
//-----------------------------------------------------------------------------
void WallClipperShape::createShape()
{
   if ( mShapeFile.isEmpty() )
      return;

   // If this is the same shape then no reason to update it
   if ( mShapeInstance && mShapeFile.equal( mShape.getPath().getFullPath(), String::NoCase ) )
      return;

   // Clean up our previous shape
   if ( mShapeInstance )
      SAFE_DELETE( mShapeInstance );
   mShape = NULL;

   // Attempt to get the resource from the ResourceManager
   mShape = ResourceManager::get().load( mShapeFile );

   if ( !mShape )
   {
      Con::errorf( "WallClipperShape::createShape() - Unable to load shape: %s", mShapeFile.c_str() );
      return;
   }

   // Attempt to preload the Materials for this shape
   if ( isClientObject() && 
        !mShape->preloadMaterialList( mShape.getPath() ) && 
        NetConnection::filesWereDownloaded() )
   {
      mShape = NULL;
      return;
   }

   // Update the bounding box
   mObjBox = mShape->bounds;
   resetWorldBox();

   // Create the TSShapeInstance
   mShapeInstance = new TSShapeInstance( mShape, isClientObject() );
}

bool WallClipperShape::prepRenderImage( SceneState *state, const U32 stateKey, 
                                         const U32 startZone, const bool modifyBaseZoneState)
{
   // Make sure we have a TSShapeInstance
   if ( !mShapeInstance )
      return false;

   // Make sure we haven't already been processed by this state
   if ( isLastState( state, stateKey ) )
      return false;

   // Update our state
   setLastState(state, stateKey);

   // If we are actually rendered then create and submit our RenderInst
   if ( state->isObjectRendered( this ) ) 
   {
      // Calculate the distance of this object from the camera
      Point3F cameraOffset;
      getRenderTransform().getColumn( 3, &cameraOffset );
      cameraOffset -= state->getDiffuseCameraPosition();
      F32 dist = cameraOffset.len();
      if ( dist < 0.01f )
         dist = 0.01f;

      // Set up the LOD for the shape
      F32 invScale = ( 1.0f / getMax( getMax( mObjScale.x, mObjScale.y ), mObjScale.z ) );

      mShapeInstance->setDetailFromDistance( state, dist * invScale );

      // Make sure we have a valid level of detail
      if ( mShapeInstance->getCurrentDetail() < 0 )
         return false;

      // GFXTransformSaver is a handy helper class that restores
      // the current GFX matrices to their original values when
      // it goes out of scope at the end of the function
      GFXTransformSaver saver;

      // Set up our TS render state      
      TSRenderState rdata;
      rdata.setSceneState( state );
      rdata.setFadeOverride( 1.0f );

      // Allow the light manager to set up any lights it needs
      LightManager* lm = NULL;
      if ( state->getSceneManager() )
      {
         lm = state->getSceneManager()->getLightManager();
         if ( lm && !state->isShadowPass() )
            lm->setupLights( this, getWorldSphere() );
      }

      // Set the world matrix to the objects render transform
      MatrixF mat = getRenderTransform();
      mat.scale( mObjScale );
      GFX->setWorldMatrix( mat );

      // Animate the the shape
      mShapeInstance->animate();

      // Allow the shape to submit the RenderInst(s) for itself
      mShapeInstance->render( rdata );

      // Give the light manager a chance to reset the lights
      if ( lm )
         lm->resetLights();
   }

   return false;
}
*/
//--------------------------------------------------------------------------
// Object Get/Set
//--------------------------------------------------------------------------

Point2F WallClipperShape::getClipVert(U32 idx)
{
	if(idx < mClipVerts.size())
	{
		Point2F clipVert = mClipVerts[idx];
		Point2F scale(getScale().x, getScale().z);
		clipVert.convolve(scale);
		return clipVert;
	}

	return Point2F(0.0f, 0.0f);
}

void WallClipperShape::updateClipper(MeshRoom *room)
{
	const MeshRoomSegment &seg = room->getSegment(mSegmentIdx);

	VectorF up(0.0f, 0.0f, 1.0f);
	VectorF left = seg.getP01() - seg.getP00();

	Point3F pt = seg.getP00() + mPosPercent * left;
	
	pt = MathUtils::mClosestPointOnSegment(seg.slice0->p1, seg.slice1->p1, pt);
	pt.z = getPosition().z;

	left.normalizeSafe();
	VectorF fwd = mCross(up, -left);

	MatrixF mat(true);
	mat.setColumn(0, -left);
	mat.setColumn(1, fwd);

	setTransform(mat);
	setPosition(pt);
	//setScale(getScale());
	inspectPostApply();

	if(mLightId)
	{
		// Variables
		Point3F start, end;
		VectorF toSun;
		MatrixF mat;
		F32 az, elev;
		RayInfo ri;

		// Get the sky object
		ScatterSky *sky = dynamic_cast<ScatterSky*>(Sim::findObject("Sky"));

		// Get clipper's light node
		S32 lightNode = getShape()->findNode("light");

		// Calculate a vector pointing toward the sun based on its azimuth/elevation
		az = mDegToRad(sky->getAzimuth());
		elev = mDegToRad(sky->getElevation());
		toSun.set(mSin(az)*mCos(elev), mCos(az)*mCos(elev), mSin(elev));
		toSun.normalizeSafe();

		// Calculate light node's world position; this will be the start of our ray cast
		getTransform().getColumn(3,&start);							// Clipper world pos
		getShape()->getNodeWorldTransform(lightNode, &mat);	// Light node object pos (method name is confusing)
		start += mat.getPosition();												// Light node world pos

		// If sunlight is coming in through the clipper, we cast a ray from the light node
		// in the direction of the sunlight.  The light is positioned 0.5 meter above the
		// hit-point in the direction of the hit-normal.  The light is set to a bright white.
		// If sunlight is not coming in through the clipper, the light is positioned 0.5
		// meters from the light node in the direction of the wall normal and given a less
		// bright blueish color.
		if(mDot(-toSun, getTransform().getForwardVector()) > 0)
		{
			// Sunlight is coming in... cast a ray to find bounced light location.
			// The ray cast will go from the light node to 20 units in the sunlight direction
			end = start - 20.0f * toSun;

			disableCollision();
			gPolysoupOverride = true;
			bool hit = gServerContainer.castRayRendered(start, end, 0xFFFFFFFF, &ri);
			gPolysoupOverride = false;
			enableCollision();

			if(hit)
			{
				mLight->setPosition(ri.point + 0.5f * ri.normal);
				mLight->setColor(1.0f, 1.0f, 1.0f);
				mLight->setBrightness(0.2f);
			}
		}
		else
		{
			// Sunlight is not coming in... simply position light to simulate ambient skylight
			mLight->setPosition(start + 0.5f * getTransform().getForwardVector());
			mLight->setColor(0.5f, 0.865f, 1.0f);
			mLight->setBrightness(0.1f);
		}
	}
}
void WallClipperShape::initPersistFields()
{
   addGroup( "Clipper" );

      addField( "segmentId", TypeS32, Offset( mSegmentIdx, WallClipperShape ), 
         "The Id of the segment we're attached to." );  

      addField( "roomName", TypeString, Offset( mRoomName, WallClipperShape ), 
         "The name of the room we're attached to" ); 

   endGroup( "MeshRoom" );

   Parent::initPersistFields();
}
U32 WallClipperShape::packUpdate(NetConnection * con, U32 mask, BitStream * stream)
{  
   U32 retMask = Parent::packUpdate(con, mask, stream);

   stream->writeInt(mSegmentIdx, 2);

   stream->writeString(mRoomName);

   // Were done ...
   return retMask;
}

void WallClipperShape::unpackUpdate(NetConnection * con, BitStream * stream)
{
   // Unpack Parent.
   Parent::unpackUpdate(con, stream);

   mSegmentIdx = stream->readInt(2);

   char temp[12];
   stream->readString(temp);

   mRoomName = temp;
}
