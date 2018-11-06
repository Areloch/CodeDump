#ifndef _ENVIOMANAGER_H_
#define _ENVIOMANAGER_H_

#ifndef _MMATH_H_
#include "math/mMath.h"
#endif
#ifndef _SCENEOBJECT_H_
#include "sceneGraph/sceneObject.h"
#endif
#ifndef _SCENESTATE_H_
#include "sceneGraph/sceneState.h"
#endif
#ifndef _SCENEGRAPH_H_
#include "sceneGraph/sceneGraph.h"
#endif
#ifndef _GAMEBASE_H_
#include "T3D/gameBase.h"
#endif
#ifndef _ITICKABLE_H_
#include "core/iTickable.h"
#endif
#ifndef _LIGHTMANAGER_H_
#include "sceneGraph/lightManager.h"
#endif
#ifndef _LIGHTINFO_H_
#include "sceneGraph/lightInfo.h"
#endif

//sky, weather, etc

#ifndef _SKY_H_
#include "environment/sky/sky.h"
#endif

//#include "environment/sky/fog.h"

/*#ifndef _SUN_H_
#include "lightingSystem/synapseGaming/sgSun.cpp"
#endif*/

#ifndef _SUN_H_
#include "terrain/sun.h"
#endif

#ifndef _FXSUNLIGHT_H_
#include "T3D/fx/fxSunLight.h"
#endif
#ifndef _PRECIPITATION_H_
#include "T3D/fx/precipitation.h"
#endif
#ifndef _LIGHTNING_H_
#include "T3D/fx/lightning.h"
#endif

class Precipitation;
class sky;
//class sgSun;
class Lightning;
class fxSunLight;

typedef struct _color_target{
	F32		elevation; // maximum target elevation
	ColorF	color; //normalized 0 = 1.0 ... 
	F32		bandMod;  //6 is max
	ColorF	bandColor;
} COLOR_TARGET, *LPCOLOR_TARGET;

typedef Vector<COLOR_TARGET> COLOR_TARGETS;

#ifndef PI
#define PI   (3.141592653589793238462643f)
#endif

#define RADIAN (180/PI)
#define TORADIANS(x) (((x)/360) * (2*PI))
#define TODEGREES(x) (((x)/(2*PI)) * 360)

class environment : public SceneObject, public ITickable
{
	typedef SceneObject Parent;

  private:
    MRandom mRand;

	// PersistFields preparation
	bool mConvertedToRads; // initPersist takes arguments in degtrees. Have we converted?

	//==========================================
	//Manager
	//==========================================
	bool            mUseWind;
	bool            mUseDayNight;
	bool            mUpdateTime; //this lets us know that the game's time as porgressed enough to update world elements
	bool			mUseWeather;
	
	F32				mNorth;				//which direction is north(on a yaw rotation)
	//StringTableEntry     envioList; //text file containing all our bones and config data :o
	FileName	 envioList;

	FileName     skyDayList; //text file containing all our bones and config data :o
	FileName     skyMidList; //text file containing all our bones and config data :o
	FileName     skyNightList; //text file containing all our bones and config data :o

	//==========================================
	//Wind
	//==========================================
	//general properties
	VectorF			 wind;   //wind velocity

	//wind direction
	VectorF			 windDir;
	VectorF			 targetWindDir;
	VectorF			 normalWindDir;
	F32				 nextDirChangeTime;
	StringTableEntry cWindDir;
	StringTableEntry lastWindDir;

	//wind speed
	F32				speed;
	F32				lastWindSpeed;
	F32				normalWindSpeed;
	F32				topWindSpeed;
	F32				targetWindSpeed;
	F32				gustFrequency;
	F32				nextGustTime;

	S32				increment;

	S32				mWindLevel;
	S32				mWindType;
	S32				mWindSpeed;
	S32				mWindDir;

	//==========================================
	//Calendar
	//This is our limits on the date mechanics
	//==========================================
	F32				mSecsPerHour;	// Real-Time Seconds per Game-Hour

	U32				mHoursInDay;	// Number of Game-Hours per Game-day
	U32				mDaysInWeek;	// Number of Game-Days per Game-Week
	U32				mDaysInMonth;	// Number of Game-Days per Game-Month
	U32				mMonthsInYear;	// Number of Game-Months per Game-Year

	// Date tracking stuff
	SimTime			mStartTime;	// The time this object was created.
	SimTime			mDayLen;	// length of day in real world milliseconds
	SimTime			mTimeOfDay; // in game milliseconds standard time (one_game_minute = (mDayLen / 24) / 60)	
	U32				mYearLen;		// Length of year in virtual days
	S32             mMonth;		//month
	S32				mDay;		//day of the month

	//=========================================
	S32				mDayofWeek;
	S32				mSeason;

	S32				mMaxHours;
	S32				mMaxMinutes;

	S32				mMaxDaysInWeek;
	S32             mMaxDaysInMonth;
	S32				mMaxWeeksInMonth;
	S32				mMaxMonthsInYear;
	S32				mMaxDaysInYear;
	//=========================================

	//==========================================
	//Date
	//This represent's in-game today's date
	//==========================================
	U32				mInitialHour;	// Hour of Game-Day at initilization
	U32				mInitialDay;	// Day of Game-Month at initilization
	U32				mInitialMonth;	// Month of Game-Year at initilization
	U32				mInitialYear;	// arbatrairy Game-Year at initilization

	U32				mTimeOfYear;	// Number of days since the last winter equinox

	//=========================================
	/*S32				mDay;
	S32				mWeek;
	S32				_Month;
	S32				mYear;
	S32				mHour;
	S32				mMinute;

	S32				mMonthDay;
	S32				mDayOfMonth;*/
	//=========================================
	//==========================================
	//Day/night
	//==========================================
	F32				mAzimuth;		// Angle from true north of celestial object in radians
	F32				mElevation;		// Angle from horizon of celestial object in radians
	VectorF			mSunVector; // the sun vector

	//=========================================
	S32				mDayScale;			//in-game day length scaled in seconds
	F32				mSunAltitude;
	F32				mSunAzumith;
	F32				mMoonAltitude;
	F32				mMoonAzumith;

	S32				timeStep;
	S32				mMinBright;
	S32				mMaxBright;
	//=========================================
	
	//==========================================
	//Weather
	//==========================================
	F32				mStormTime;			//average time it takes for a storm to roll in - controlled by season
	F32				mRainFogChance;		//likelyhood of fog during rain and other weather

	F32				mPressure;			//current air pressure
	F32				mTempature;			//current air tempature
	F32				mSpeedOfSound;		//seed of sound - primarily used in ballistics
	F32				mDensity;			//air density
	ColorF			mFogColor;
	//F32				mHumidity;			//relative humidity
	//F32				mDewPoint;			//dew point tempature
	bool			mRain;					//are we doing rain at the moment?
	S32				mRainLevel;
	F32				mRainTime;

	F32				mCloudHeight[3];
	F32				mCloudSpeed[3];

	const char*     mPrecipDB;
	const char*		mLightnDB;


	//==========================================
	//Geography
	//Add later
	//Will control geographical elements such as climate identifiers, simulated altitude, etc
	//This directly controls almost all other elements for the great weather climate simulation 
	//used to simulate a longer-term weather simulation
	//==========================================
	S32				mClimate;
	//S32				mAltitude;

	S32				mMoonElevationTime;
	S32				mMoonDate;				//tracks phases of the moon
	F32				mSunElevation;
	F32				mLastSunEl;
	S32				elevationStep;

	F32				mFogDist;				//how thick is the fog right now?

	//-----------------------------------------
	//sky color stuff
	ColorF			mCurrentColor;
	F32				mBandMod;
	ColorF			mCurrentBandColor;

	// color management
	COLOR_TARGETS mColorTargets;

	// Global positioning stuff
	F32				mLongitude;		// longitudinal position of this mission
	F32				mLatitude;		// latitudinal position of this missiion
	F32				mAxisTilt;		// angle between global equator and tropic
	//-----------------------------------------

	// return number between 0 and 1 representing color variance
	F32 getColorVariance();

	struct season
	{
		const char*		name;		//name of season
		F32				timeEffect;	//the time effect it has on the day/night cycle
		F32				light;		//how much light at night there is
		F32				fog;		//how much fog there is when raining;
		Point3F         baseSkyEffect;
		Point3F			maxSkyEffect;
		F32				temp;		//average tempature for the season;

	};

	struct month
	{
		const char*     name;
		S32				daysInMonth;
		S32				season;
	};

	struct week
	{
		const char*     name;
		S32				dayNumber;
	};

	Vector<season*> mSeasons;
	Vector<month*>  mMonths;
	Vector<week*>   mWeeks;	

	static const S32 MAX_ENVIO  = 50;

	Sky*			mSky;		//our current sky
	//sgSun*		mSunObj;
	Sun*			mSunObj;
	fxSunLight*		mSun;
	fxSunLight*		mMoon;
	Precipitation*  mPrecip;
	Lightning*		mLightning;

	F32				mVisibleDistance;

	S32             atmosphereID;


	/*EnumTable::Enums seasons[MAX_ENVIO];
	EnumTable::Enums months[MAX_ENVIO];
	EnumTable::Enums days[MAX_ENVIO];

	EnumTable monthsTable();
	EnumTable daysTable();
	EnumTable seasonsTable();*/

public:
	//==========================================
	// Fog
	//==========================================
	//fogManager     *mFogManager;

	//==========================================
	//Wind
	//==========================================
	S32  _windLevel;
	enum windLevel
    {
        WL_Decreased         = 0,
		WL_Increased,
		WL_Decreasing,
        WL_Increasing,

		WL_MAX
    };
    static StringTableEntry eWindLevel[WL_MAX];
	
	enum windType
    {
        WT_Breezy         = 0,
		WT_Gusty,
		WT_Steady,

		WT_MAX
    };

	enum windSpeed
    {
        WS_VeryLight         = 0,
		WS_Light,
		WS_Moderate,
		WS_Heavy,
		WS_VeryHeavy,

		WS_MAX
    };

	enum windDirection
    {
        WD_North         = 0,
		WD_NorthEast,
		WD_East,
		WD_SouthEast,
		WD_South,
		WD_SouthWest,
		WD_West,
		WD_NorthWest,

		WD_MAX
    };
	//==========================================

	//==========================================
	//Calendar
	//==========================================
	enum Month
    {
        MT_January         = 0,
		MT_Feburary,
		MT_March,
		MT_April,
		MT_May,
		MT_June,
		MT_July,
		MT_August,
		MT_September,
		MT_October,
		MT_November,
		MT_December,

		MT_MAX
    };

	enum Day
    {
        DY_Monday         = 0,
		DY_Tuesday,
		DY_Wednesday,
		DY_Thursday,
		DY_Friday,
		DY_Saturday,
		DY_Sunday,

		DY_MAX
    };

	enum Season
    {
        SN_Spring         = 0,
		SN_Summer,
		SN_Fall,
		SN_Winter,

		SN_MAX
    };
	//==========================================
	//Weather
	//==========================================
	S32	 mSkyState;
	enum sky
	{
		SKY_Cloudless = 0,
		SKY_Cloudy,
		SKY_Raining,
		SKY_Lightning,
		SKY_MAX
	};
	static StringTableEntry eSkyState[SKY_MAX];


	//==========================================
	//Geography
	//==========================================
	//climate
	enum climate
    {
        CL_Temperate         = 0,
		CL_Desert,
		CL_Arctic,

		CL_MAX
    };
	

  protected:
	enum NetMaskBits {
        InitMask = BIT(0),
		windMask = BIT(1),
		climateMask = BIT(2),
		ambientMask = BIT(3)
	};

    bool onAdd();
    void onRemove();
	void inspectPostApply();

	//for the updating
	virtual void interpolateTick( F32 delta );
    virtual void processTick();
    virtual void advanceTime( F32 timeDelta );

   //=========================================================
   //wind
   //=========================================================
   inline const S32			getWindLevel() const {return mWindLevel;}
   inline const S32         getWindType() const  {return mWindType;}
   inline const S32         getWindSpeed() const {return mWindSpeed;}
   inline const S32         getWindDir() const   {return mWindDir;}
   inline const S32         getClimate() const   {return mClimate;}

   inline void  setWindLevel(const windLevel& wL = WL_Decreased) {mWindLevel = wL;}
   inline void  setWindType(const windType& wT = WT_Steady)      {mWindType = wT;}
   inline void  setWindSpeed(const windSpeed& wS = WS_VeryLight) {mWindSpeed = wS;}
   inline void  setWindDir(const windDirection& wD = WD_North)   {mWindDir = wD;}
   inline void  setClimate(const climate& cL = CL_Temperate)     {mClimate = cL;}

public:
	environment();
	~environment();

   static void initPersistFields();
   static void consoleInit();

   void updateValues();

   /*inline const windLevel			getWindLevel() const {return mWindLevel;}
   inline const windTypeTable       getWindType() const  {return mWindType;}
   inline const windSpeedTable      getWindSpeed() const {return mWindSpeed;}
   inline const windDirectionTable  getWindDir() const   {return mWindDir;}
   inline const climateTable        getClimate() const   {return mClimate;}*/

   //=========================================================
   //wind
   //=========================================================
   //Point3F getWindDir() { return windDir; }
   Point3F getWindVel() { return wind; }

   //=========================================================
   //sky, sun
   //=========================================================
   //update sky, sun, etc
   //void updateSky();
   void updateWind();
   //void sunCycle();

   //setup functions
   /*void setupSky();
   void setupSun();
   void setupMoon();*/
   void setupWind();
   //void setupCalendar();

   /*void getColor(ColorF &);
   void initColors();
   void AddColorTarget(F32 ele, ColorF& color, F32 bandMod, ColorF& bandColor);
   F32 Elevation(F32 lat, F32 dec, F32 mer);
   F32 Azimuth(F32 lat, F32 dec, F32 mer);
   VectorF getSunDirection();*/

   //=========================================================
   // Climate
   //=========================================================
   F32 getAirPressure() { return mPressure; }
   F32 getTempature() { return mTempature; }
   
   //=========================================================
   //weather
   //=========================================================
   /*void startRain();
   void stopRain();
   void startLightning();
   void stopLightning();
   void setupWeather();
   void updateWeather();*/

   F32 convertAPtoMB(F32 inch);
   F32 convertAPtoInch(F32 MB);

   U32  packUpdate(NetConnection *conn, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *conn,           BitStream *stream);

   void parseEnvioScript(const char*);

   const char * getUnits(const char *string, S32 startIndex, S32 endIndex, const char *set);
   const char * getUnit(const char *string, U32 index, const char *set);

   void applyEnvChanges()
   {
	   inspectPostApply();
   }

   U32 getUnitCount(const char *string, const char *set){
	   U32 count = 0;
	   U8 last = 0;
	   while(*string){
		  last = *string++;
		  for(U32 i =0; set[i]; i++) {
			 if(last == set[i])	 {
				count++;
				last = 0;
				break;
			 }
		  }
	   }
	   if(last)
		  count++;
	   return count;
	}

   
   // Declare Console Object.
   DECLARE_CONOBJECT(environment);
};

#endif