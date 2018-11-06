#include "environment/environment.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/stream/bitStream.h"
#include "math/mathIO.h"
#include "core/fileObject.h"
#include "math/mathUtils.h"
#include "lightingSystem/synapseGaming/sgLightManager.h"

IMPLEMENT_CO_NETOBJECT_V1(environment);

static EnumTable::Enums weatherType[] =
{
	{ environment::SKY_Cloudless,     "Cloudless"	},
	{ environment::SKY_Cloudy,     "Cloudy"			},
	{ environment::SKY_Raining,      "Raining"		},
	{ environment::SKY_Lightning,     "Storming"	},
};
static EnumTable weatherTypeTable(4, &weatherType[0]);

static EnumTable::Enums windType[] =
{
	{ environment::WT_Breezy,     "Breezy"			},
	{ environment::WT_Gusty,      "Gusty"			},
	{ environment::WT_Steady,     "Steady"			},
};
static EnumTable windTypeTable(3, &windType[0]);

static EnumTable::Enums windSpeed[] =
{
	{ environment::WS_VeryLight,     "VeryLight"		},
	{ environment::WS_Light,         "Light"			},
	{ environment::WS_Moderate,      "Moderate"			},
	{ environment::WS_Heavy,         "Heavy"			},
	{ environment::WS_VeryHeavy,     "VeryHeavy"		},
};
static EnumTable windSpeedTable(5, &windSpeed[0]);

static EnumTable::Enums windDirection[] =
{
	{ environment::WD_North,		"North"			},
	{ environment::WD_NorthEast,    "NorthEast"		},
	{ environment::WD_East,			"East"			},
	{ environment::WD_SouthEast,    "SouthEast"		},
	{ environment::WD_South,		"South"			},
	{ environment::WD_SouthWest,    "SouthWest"		},
	{ environment::WD_West,			"West"			},
	{ environment::WD_NorthWest,    "NorthWest"		},
};
static EnumTable windDirectionTable(8, &windDirection[0]);

static EnumTable::Enums climate[] =
{
	{ environment::CL_Temperate,    "Temperate"		},
	{ environment::CL_Desert,		"Desert"		},
	{ environment::CL_Arctic,		"Arctic"		},
};
static EnumTable climateTable(3, &climate[0]);

static EnumTable::Enums _month[] =
{
	{ environment::MT_January,		"January"		},
	{ environment::MT_Feburary,		"Feburary"		},
	{ environment::MT_March,		"March"			},
	{ environment::MT_April,		"April"			},
	{ environment::MT_May,			"May"			},
	{ environment::MT_June,			"June"			},
	{ environment::MT_July,			"July"			},
	{ environment::MT_August,		"August"		},
	{ environment::MT_September,    "September"		},
	{ environment::MT_October,		"October"		},
	{ environment::MT_November,		"November"		},
	{ environment::MT_December,		"December"		},
};
static EnumTable monthTable(12, &_month[0]);

static EnumTable::Enums day[] =
{
	{ environment::DY_Monday,		"Monday"		},
	{ environment::DY_Tuesday,		"Tuesday"		},
	{ environment::DY_Wednesday,	"Wednesday"		},
	{ environment::DY_Thursday,		"Thursday"		},
	{ environment::DY_Friday,		"Friday"		},
	{ environment::DY_Saturday,		"Saturday"		},
	{ environment::DY_Sunday,		"Sunday"		},
};
static EnumTable dayTable(7, &day[0]);

static EnumTable::Enums _season[] =
{
	{ environment::SN_Spring,		"Spring"		},
	{ environment::SN_Summer,		"Summer"		},
	{ environment::SN_Fall,			"Fall"			},
	{ environment::SN_Winter,		"Winter"		},
};
static EnumTable seasonTable(4, &_season[0]);


environment::environment()
{
    mTypeMask |= EnvironmentObjectType;

	mPressure = 29.0f;
	mTempature = 59;
	mNorth = 90;	//which direction is north(on a yaw rotation)

	//======================================
	mWindLevel = 1;
	mWindType = 1;
	mWindSpeed = 1;
	mWindDir = 1;
	mClimate = 1;

	mMonths = 4;
	mDayofWeek = 3;
	mSeason = 1;

	//======================================
	mSky = NULL;
	mSun = NULL;
	mMoon = NULL;

	//======================================
	mAxisTilt = 1;
	mLongitude = 1;
	mLatitude = 1;

	mSecsPerHour = 10;
	mHoursInDay = 24;
	mMonthsInYear = 12;
	mDaysInMonth = 30; //change back to calendar based

	mInitialHour = 12;
	mInitialMonth = 5;
	mInitialDay = 12;
	mInitialYear = 2008;
	//======================================

	mCurrentColor = ColorF(0.4f,0.6f,0.9f);

	wind = Point3F(0,0,0);

	//day/night crap
	/*mDayScale = 2;	//10 minute days
	timeStep = 1;
	elevationStep = 0;
	mSunElevation = 1;
	mMoonElevationTime = 1;*/

	mUseDayNight = false;
	mUseWeather = false;
}

//------------------------------------------------------------------------------
environment::~environment()
{
	mSun = NULL;
	mSunObj = NULL;
	mSky = NULL;
    mMoon = NULL;
    mPrecip = NULL;
    mLightning = NULL;
}

//---------------------------------------------------------------------------
bool environment::onAdd()
{
   if(!Parent::onAdd())
      return false;

   mObjBox.minExtents.set(-0.5f, -0.5f, -0.5f);
   mObjBox.maxExtents.set( 0.5f,  0.5f,  0.5f);
   resetWorldBox();

  //set up our ambient parameters
  envioList = "scriptsAndAssets/server/scripts/environmentData.cs";

  //parse our DB
  parseEnvioScript(envioList);

  mSky = gClientSceneGraph->getCurrentSky();
  //mSunObj = dynamic_cast<sgSun*>(Sim::findObject("Sun"));
  mSunObj = dynamic_cast<Sun*>(Sim::findObject("SunObj"));

  mSun = dynamic_cast<fxSunLight*>(Sim::findObject("Sun"));
  if(!mSun)
	mSun = NULL;

  mMoon = NULL;

  mPrecip = NULL;
  mLightning = NULL;

  mFogDist = gClientSceneGraph->getFogDistance();
  mFogColor = gClientSceneGraph->getFogColor();
  
  //setupSky();
  //setupSun();
  //setupWeather();
  setupWind();
  //setupCalendar();
  //initColors();

  F32 ele, azi;
  ele = azi = TORADIANS(90);
  MathUtils::getVectorFromAngles(mSunVector, azi, ele);
  
  mTempature = mSeasons[mSeason]->temp;

   //set Vals here
  mSpeedOfSound = mSqrt(1.4005 * 1715.7 *(mTempature + 459.6));
  mDensity = ((70.748663101605 * mPressure) / ((mTempature + 459.6) * 1715.7)) * 32.174;

  if(!isClientObject())
	  assignName("environment");

   initPersistFields();	//update our parameters

   mStartTime = Sim::getCurrentTime();

   addToScene();
   return true;
}

void environment::onRemove()
{
   removeFromScene();
   Parent::onRemove();
}

void environment::inspectPostApply()
{
    // Set Parent.
	Parent::inspectPostApply();

	/*setupSky();
    setupSun();
    setupWeather();*/
    setupWind();
    //setupCalendar();
    //initColors();

    F32 ele, azi;
    ele = azi = TORADIANS(90);
    MathUtils::getVectorFromAngles(mSunVector, azi, ele);
  
    mTempature = mSeasons[mSeason]->temp;

    //set Vals here
    mSpeedOfSound = mSqrt(1.4005 * 1715.7 *(mTempature + 459.6));
    mDensity = ((70.748663101605 * mPressure) / ((mTempature + 459.6) * 1715.7)) * 32.174;

    initPersistFields();	//update our parameters

	// Set fxPortal Mask.
	setMaskBits(InitMask);
}

U32 environment::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
	U32 retMask = Parent::packUpdate(con, mask, stream);

	//initilize us
   if(stream->writeFlag(mask & InitMask))
   {
	    stream->write(mTempature);
		stream->write(mPressure);
		stream->write(mSpeedOfSound);
		stream->write(mDensity);

		stream->writeRangedU32(getWindSpeed(), 0, WS_MAX);
		stream->writeRangedU32(getWindType(), 0, WT_MAX);
		stream->writeRangedU32(getWindDir(), 0, WD_MAX);
		stream->writeRangedU32(getWindLevel(), 0, WL_MAX);
		stream->writeRangedU32(getClimate(), 0, CL_MAX);

		stream->write(targetWindSpeed);
		stream->write(speed);
		stream->write(nextGustTime);
		stream->write(targetWindDir);
		stream->write(windDir);
		stream->write(nextDirChangeTime);
		stream->write(wind);

		stream->write(mMonth);
		stream->write(mDayofWeek);
		stream->write(mSeason);

		stream->write(mMaxHours);
		stream->write(mMaxMinutes);
		stream->write(mMaxDaysInWeek);
		stream->write(mMaxWeeksInMonth);
		stream->write(mMaxDaysInYear);

		stream->write(mInitialHour);
		stream->write(mInitialDay);
		stream->write(mInitialMonth);
		stream->write(mInitialYear);
		stream->write(mTimeOfYear);	

		stream->write(mAzimuth);
		stream->write(mElevation);
		stream->write(mSunVector.x);
		stream->write(mSunVector.y);
		stream->write(mSunVector.z);

		stream->write(mCurrentColor.red);
		stream->write(mCurrentColor.green);
		stream->write(mCurrentColor.blue);
   }

   if (stream->writeFlag(mask & windMask)) 
   {		  
		stream->writeRangedU32(getWindSpeed(), 0, WS_MAX);
		stream->writeRangedU32(getWindType(), 0, WT_MAX);
		stream->writeRangedU32(getWindDir(), 0, WD_MAX);
		stream->writeRangedU32(getWindLevel(), 0, WL_MAX);

		stream->write(targetWindSpeed);
		stream->write(speed);
		stream->write(nextGustTime);
		stream->write(targetWindDir);
		stream->write(windDir);
		stream->write(nextDirChangeTime);
		stream->write(wind);
   }

   if (stream->writeFlag(mask & climateMask)) 
   {
		stream->writeRangedU32(getClimate(), 0, CL_MAX);
   }

   if (stream->writeFlag(mask & ambientMask))
   {
		stream->write(mTempature);
		stream->write(mPressure);
		stream->write(mSpeedOfSound);
		stream->write(mDensity);
   }

   return retMask;
}

void environment::unpackUpdate(NetConnection *con, BitStream *stream)
{
   if (stream->readFlag()) 
   {
	   mTempature = stream->readFlag();
	   mPressure = stream->readFlag();
	   mSpeedOfSound = stream->readFlag();
	   mDensity = stream->readFlag();

	   setWindLevel((windLevel)stream->readRangedU32(0, WL_MAX));
	   setWindType((windType)stream->readRangedU32(0, WT_MAX));
	   setWindSpeed((windSpeed)stream->readRangedU32(0, WS_MAX));
	   setWindDir((windDirection)stream->readRangedU32(0, WD_MAX));
	   setClimate((climate)stream->readRangedU32(0, CL_MAX));

	   targetWindSpeed = stream->readFlag();
	   speed = stream->readFlag();
	   nextGustTime = stream->readFlag();

	   targetWindDir.x = stream->readFlag();
	   targetWindDir.y = stream->readFlag();
	   targetWindDir.z = stream->readFlag();

	   windDir.x = stream->readFlag();
	   windDir.y = stream->readFlag();
	   windDir.z = stream->readFlag();

	   nextDirChangeTime = stream->readFlag();

	   wind.x = stream->readFlag();
	   wind.y = stream->readFlag();
	   wind.z = stream->readFlag();

		mMonth = stream->readFlag();
		mDayofWeek = stream->readFlag();
		mSeason = stream->readFlag();

		mMaxHours = stream->readFlag();
		mMaxMinutes = stream->readFlag();
		mMaxDaysInWeek = stream->readFlag();
		mMaxWeeksInMonth = stream->readFlag();
		mMaxDaysInYear = stream->readFlag();

		mInitialHour = stream->readFlag();
		mInitialDay = stream->readFlag();
		mInitialMonth = stream->readFlag();
		mInitialYear = stream->readFlag();
		mTimeOfYear = stream->readFlag();	

		mAzimuth = stream->readFlag();
		mElevation = stream->readFlag();
		mSunVector.x = stream->readFlag();
		mSunVector.y = stream->readFlag();
		mSunVector.z = stream->readFlag();

		mCurrentColor.red = stream->readFlag();
		mCurrentColor.green = stream->readFlag();
		mCurrentColor.blue = stream->readFlag();
   }

   if (stream->readFlag()) 
   {

	   setWindLevel((windLevel)stream->readRangedU32(0, WL_MAX));
	   setWindType((windType)stream->readRangedU32(0, WT_MAX));
	   setWindSpeed((windSpeed)stream->readRangedU32(0, WS_MAX));
	   setWindDir((windDirection)stream->readRangedU32(0, WD_MAX));
	   setClimate((climate)stream->readRangedU32(0, CL_MAX));

	   targetWindSpeed = stream->readFlag();
	   speed = stream->readFlag();
	   nextGustTime = stream->readFlag();

	   targetWindDir.x = stream->readFlag();
	   targetWindDir.y = stream->readFlag();
	   targetWindDir.z = stream->readFlag();

	   windDir.x = stream->readFlag();
	   windDir.y = stream->readFlag();
	   windDir.z = stream->readFlag();

	   nextDirChangeTime = stream->readFlag();

	   wind.x = stream->readFlag();
	   wind.y = stream->readFlag();
	   wind.z = stream->readFlag();
   }

   if (stream->readFlag()) 
   {
	   setClimate((climate)stream->readRangedU32(0, CL_MAX));
   }

   if (stream->readFlag()) 
   {
	   mTempature = stream->readFlag();
	   mPressure = stream->readFlag();
	   mSpeedOfSound = stream->readFlag();
	   mDensity = stream->readFlag();
   }
	
}
//---------------------------------------------------------------------------
void environment::processTick()
{
	updateWind();

	//if(mUseDayNight)
	//	sunCycle();

	//============================================
	//day/night stuff
	/*if(mUseDayNight)
	{
		if((timeStep % mDayScale) == 0)
		{
			mMoonElevationTime = mDayScale * 10;
			mUpdateTime = true;
		}
		timeStep++;
		if(timeStep >= 3600)
			timeStep = 0;
		
		if(mUpdateTime)
		{
			mMinute++;
			elevationStep++;
			if(elevationStep >= 4)
			{
				elevationStep = 0;
				mSunElevation++;
				sunCycle();
			}
			if(mMinute >=60)
			{
				mMinute = 0;
				mHour++;
				if(mHour >= 24)
				{
					mHour = 0;
					mDay++;
					mMonthDay++;
					mDayofWeek = mMonthDay%7;		//day[]
					if(mMonthDay >= mMaxDaysInMonth)
					{
						mMonthDay = 1;
						mMonth++;
						//SetDefaultSeasonSky();

						if(mDay >= mMaxDaysInWeek)
						{
							mDay = 0;
							mWeek++;
							if(mWeek >= 53)
								mWeek =0;
						}
						if(mMonth >= mMaxMonthsInYear)
						{
							mSeason = 0;
							mMonth = 0;
							//sets year number and BC/AD
							//%yearSet = getword(mYear, 1);
							//%yearNow = getword(mYear, 0);
							/*if(%yearSet == "0")
							{
								%yearNow++;
								mYear= (%yearNow SPC %yearSet);
							}
							else
							{
								%yearNow--;
								mYear= (%yearNow SPC %yearSet);
							}*/
							/*mYear++;
						}
					} 
				}
			}

			/*$DayNow = getword($weekDay[$MyTime.Day], 1);
			$MonthNow= getword($Month[$MyTime.Month], 1);
			$YearNow = getword($MyTime.Year, 0);
			//%client = $playernow;
			//%MyTimeWord = ("Time is: " @ $MyTime.hours @ ":" @ $MyTime.minutes SPC $MyTime.MonthDay SPC $DayNow SPC $MonthNow @ " of year " @ $YearNow);
			//messageClient(%client, 'MyTime', %MyTimeWord);*/

			//minute-based filter update
			//not technically needed like this, is it?
			/*if(mMinute == $MyTime.Prev)
			{

			}
			else
			{
				$MyTime.Prev = $MyTime.minutes;
				%SetClock = ($MyTime.hours @ ":" @ $MyTime.minutes);
				%SetDater =($DayNow SPC $MyTime.MonthDay SPC $MonthNow @ " of " @ $YearNow SPC "BC");
				isMyclock.text = %SetClock;
				isMyDater.text = %SetDater;
			}
			mUpdateTime = false;
		}
	}*/
	//if(mUseWeather)
	//	updateWeather();
}

void environment::advanceTime(F32 deltaTime)
{
}

void environment::interpolateTick(F32 delta)
{
}

void environment::consoleInit()
{}

void environment::initPersistFields()
{
	Parent::initPersistFields();

	addGroup("Manager");
	addField("useWind",			TypeBool,		Offset(mUseWind,	environment ) );
	addField("useDayNight",		TypeBool,		Offset(mUseDayNight,	environment ) );
	addField("useWeather",		TypeBool,		Offset(mUseWeather,	environment ) );
	addField("skyMatDay",       TypeStringFilename, Offset(skyDayList,	environment ) );
	addField("skyMatMid",       TypeStringFilename, Offset(skyMidList,	environment ) );
	addField("skyMatNight",     TypeStringFilename, Offset(skyNightList,	environment ) );
	addField("envioData",       TypeStringFilename, Offset(envioList,	environment ) );
	endGroup("Manager");

	addGroup("Wind Data");
    addField("windType",        TypeEnum,		Offset(mWindType,  environment), 1, &windTypeTable);
    addField("windSpeed",       TypeEnum,		Offset(mWindSpeed, environment), 1, &windSpeedTable);
    addField("windDirection",   TypeEnum,		Offset(mWindDir,   environment), 1, &windDirectionTable);
    endGroup("Wind Data");

	addGroup("Weather Data");
	addField("weatherType",    TypeEnum,		Offset(mSkyState,  environment), 1, &weatherTypeTable);
	endGroup("Weather Data");

    addGroup("Climate");
    addField("climateType",     TypeEnum,		Offset(mClimate,   environment), 1, &climateTable);
    addField("tempature",		TypeF32,		Offset(mTempature,	environment ) );
    addField("north",			TypeF32,		Offset(mNorth,		environment ) );
    addField("pressure",	    TypeF32,		Offset(mPressure,	environment ) );
    addField("speedOfSound",	TypeF32,		Offset(mSpeedOfSound,	environment ) );
    addField("airDensity",		TypeF32,		Offset(mDensity,	environment ) );
    //addField("humidity",	    TypeF32,		Offset(mHumidity,	environment ) );
    endGroup("Climate");

	addGroup("Calendar");
	//named stuffs
	//addField("monthName",		TypeEnum,		Offset(_Month,	environment), 1, &monthTable);
	//addField("day",		TypeEnum,		Offset(mDayofWeek,		environment), 1, &dayTable);
	//addField("season",			TypeEnum,		Offset(mSeason,	environment), 1, &seasonTable);

	//controls day/night parameters
	//addField("date",			TypeF32,		Offset(mDay,	environment ) );
	//addField("hour",			TypeF32,		Offset(mHour,	environment ) );
	//addField("minute",			TypeF32,		Offset(mMinute,	environment ) );
	addField("hour",			TypeF32,		Offset(mInitialHour,	environment ) );
	addField("day",				TypeF32,		Offset(mInitialDay,	environment ) );
	addField("month",			TypeF32,		Offset(mInitialMonth,	environment ) );
	addField("year",			TypeF32,		Offset(mInitialYear,	environment ) );
	addField("secondsPerHour",	TypeF32,		Offset(mSecsPerHour,	environment ) );
	addField("hoursInDay",		TypeF32,		Offset(mHoursInDay,	environment ) );
	endGroup("Calendar");

	addGroup("Day/Night");
	addField("TimeScale",		TypeF32,		Offset(mDayScale, environment));
	endGroup("Day/Night");

	addGroup("Day/Geography");
	addField("axisTilt",			TypeF32,		Offset(mAxisTilt,	environment ) );
	addField("longitude",			TypeF32,		Offset(mLongitude,	environment ) );
	addField("latitude",			TypeF32,		Offset(mLatitude,	environment ) );
	endGroup("Geography");

	//addGroup("Geography");
	//endGroup("Geography");
}

void environment::parseEnvioScript(const char* szFilename)
{	
	//prep us up
	const char* read;
	StringTableEntry title, thing;
	FileObject file;

	if(file.openForRead(szFilename)) 
	{	
		//S32 index = 0;	
		//S32 LineCount = 0;  
		while(!file.isEOF())
		{		
			//lines[index] = (char*)file.readLine();
			read = (char*)file.readLine();
			thing = StringTable->insert(getUnit(read, 0, " "));
			if( _stricmp (thing, "new") == 0)
			{
				title = StringTable->insert(getUnit(read, 1, " "));
				if( _stricmp (title, "season" ) == 0){ //we're defining a new bone region/bone list
					 
					 //seasonCount++;
					 const char* name = StringTable->insert(getUnit(read, 2, " "));
					 F32 timeEffect = dAtof(StringTable->insert(getUnit(read, 3, " ")));
					 F32 light = dAtof(StringTable->insert(getUnit(read, 4, " ")));
					 F32 fog = dAtof(StringTable->insert(getUnit(read, 5, " ")));
					 F32 temp = dAtof(StringTable->insert(getUnit(read, 6, " ")));

					 season* tS = new season();
					 for(S32 i=0; i<4; i++)
					 {
						 if(_stricmp(name, _season[i].label) == 0)
						 {
							 tS->name = _season[i].label;
							 tS->light = light;
							 tS->timeEffect = timeEffect;
							 tS->fog = fog;
							 tS->temp = temp;
							 mSeasons.push_back(tS);
							 break;
						 }
					 }
				}
				if( _stricmp (title, "month" ) == 0){

					 //monthCount++;
					 const char* name = StringTable->insert(getUnit(read, 2, " "));
					 S32 daysInMonth = dAtoi(StringTable->insert(getUnit(read, 3, " ")));
					 S32 season = dAtoi(StringTable->insert(getUnit(read, 4, " ")));

					 month* tM = new month();
					 for(S32 i=0; i<12; i++)
					 {
						 if(_stricmp(name, _month[i].label) == 0)
						 {
							 tM->name = _month[i].label;
							 tM->daysInMonth = daysInMonth;
							 tM->season = season;
							 mMonths.push_back(tM);
							 break;
						 }
					 }
				}
				if( _stricmp (title, "day" ) == 0){

					 //dayCount++;
					 const char* name = StringTable->insert(getUnit(read, 2, " "));
					 S32 dayNumber = dAtoi(StringTable->insert(getUnit(read, 3, " ")));

					 week* tW = new week();
					 for(S32 i=0; i<7; i++)
					 {
						 if(_stricmp(name, day[i].label) == 0)
						 {
							 tW->name = day[i].label;
							 tW->dayNumber = dayNumber;
							 mWeeks.push_back(tW);
							 break;
						 }
					 }
				}
				//expand for strength values later
				if( _stricmp (title, "data" ) == 0){

					 const char* type = StringTable->insert(getUnit(read, 2, " "));
					 if( _stricmp (type, "Precipitation" ) == 0)
						mPrecipDB = StringTable->insert(getUnit(read, 3, " "));
					 else if(_stricmp (type, "Lightning" ) == 0)
						mLightnDB = StringTable->insert(getUnit(read, 3, " "));
				}
			}
		} 
		//seasonsTable(seasonCount, &seasons[0]);
		//monthsTable = new EnumTable(monthCount, &months[0]);
		//daysTable = new EnumTable(dayCount, &days[0]);
	}   
	else 
	{	
		Con::printf("environment::parseEnvioScript - File not found!");   
	}   
	file.close(); 
}


// -----------------------------
//Wind functions
void environment::updateWind()
{
	F32 time = Sim::getCurrentTime();
	time /= 1000;

	//wind stuff
	if(mUseWind)
	{
		switch(mWindLevel)
		{
			case WL_Decreased:
			{
				if(time > nextGustTime){
					mWindLevel = WL_Increasing;
					targetWindSpeed = topWindSpeed + mRand.randRangeF(4);
				}
				else if(mRand.randF() > 0.8)
					targetWindSpeed = normalWindSpeed + mRand.randRangeF(3);
				break;
			}
			case WL_Increasing:
			{
				if(speed == targetWindSpeed){
					mWindLevel = WL_Increased;
					nextGustTime = time + 6 + mRand.randRangeF(4);
				}
				break;
			}
			case WL_Increased:
			{
				if(time > nextGustTime){
					mWindLevel = WL_Decreasing;
					targetWindSpeed = normalWindSpeed + mRand.randRangeF(3);
				}
				else if(mRand.randF() > 0.8)
					targetWindSpeed = topWindSpeed + mRand.randRangeF(3);
				break;
			}
			case WL_Decreasing:
			{
				if(speed == targetWindSpeed){
					mWindLevel = WL_Decreased;
					nextGustTime = time + gustFrequency + mRand.randRangeF(6);
				}
				break;
			}
		}

		if(targetWindSpeed < 0)
			targetWindSpeed = 0;

		if(time > nextDirChangeTime){
			targetWindDir.y = normalWindDir.y + mRand.randRangeF(7);
			nextDirChangeTime = time + 180 + mRand.randRangeF(90);
		}
		else if(mRand.randF() > 0.8)
			targetWindDir.y = normalWindDir.y + mRand.randRangeF(6);

		if(speed != targetWindSpeed){
			if(targetWindSpeed < speed){
				speed -= mRand.randRangeI(increment);

				if(targetWindSpeed > speed)
					speed = targetWindSpeed;
			}
			else if(targetWindSpeed > speed){
				speed += mRand.randRangeI(increment);

				if(targetWindSpeed < speed)
					speed = targetWindSpeed;
			}
		}

		if(windDir != targetWindDir)
		{
			if(targetWindDir.y < windDir.y){
				windDir.y -= mRand.randRangeF(3);

				if(targetWindDir.y > windDir.y)
					windDir.y = targetWindDir.y;
			}
			else if(targetWindDir.y > windDir.y){
				windDir.y += mRand.randRangeF(3);

				if(targetWindDir.y < windDir.y)
					windDir.y = targetWindDir.y;
			}
		}

		if(speed < 0)
			speed = 0;

		wind = speed * windDir;

		//=====================================================
		S32 dirYaw = (S32)(windDir.y - mNorth) & 360;

		if(dirYaw < 22)
			mWindDir = WD_North;
		else if(dirYaw < 67)
			mWindDir = WD_NorthEast;
		else if(dirYaw < 112)
			mWindDir = WD_East;
		else if(dirYaw < 157)
			mWindDir = WD_SouthEast;
		else if(dirYaw < 202)
			mWindDir = WD_South;
		else if(dirYaw < 247)
			mWindDir = WD_SouthWest;
		else if(dirYaw < 292)
			mWindDir = WD_West;
		else if(dirYaw < 337)
			mWindDir = WD_NorthWest;
		else
			mWindDir = WD_North;
		//update directional if needed
	}
}

void environment::setupWind()
{
  //========================================
  //Wind stuff
  switch(mWindSpeed)
  {
	case WS_VeryLight:{
			topWindSpeed = 4;
			increment = 1;
			break;
		}
	case WS_Light:{
			topWindSpeed = 8;
			increment = 2;
			break;
		}
	case WS_Moderate:{
			topWindSpeed = 16;
			increment = 4;
			break;
		}
	case WS_Heavy:{
			topWindSpeed = 24;
			increment = 6;
			break;
		}
	case WS_VeryHeavy:{
			topWindSpeed = 32;
			increment = 10;
			break;
		}
  }

  switch(mWindType)
  {
	case WT_Breezy:{
		normalWindSpeed = topWindSpeed/6;
		gustFrequency = 18;
		break;
	}
	case WT_Gusty:{
		normalWindSpeed = topWindSpeed/3;
		gustFrequency = 12;
		break;
	}
	case WT_Steady:{
		normalWindSpeed = topWindSpeed/1.5;
		gustFrequency = 8;
		break;
	}
  }

  switch(mWindDir)
  {
	  //stored as degrees, as I hates me some radians
	case WD_North:
		normalWindDir = VectorF(0,0,0);
		break;
	case WD_NorthEast:
		normalWindDir = VectorF(0,45,0);
		break;
	case WD_East:
		normalWindDir = VectorF(0,90,0);
		break;
	case WD_SouthEast:
		normalWindDir = VectorF(0,135,0);
		break;
	case WD_South:
		normalWindDir = VectorF(0,180,0);
		break;
	case WD_SouthWest:
		normalWindDir = VectorF(0,225,0);
		break;
	case WD_West:
		normalWindDir = VectorF(0,270,0);
		break;
	case WD_NorthWest:
		normalWindDir = VectorF(0,315,0);
		break;
  }

  normalWindDir.y += mNorth;  //orient on our level's 'north'

  mWindLevel = WL_Decreased; //default
  targetWindSpeed = normalWindSpeed + mRand.randRangeF(2);

  speed = targetWindSpeed;
  nextGustTime = Sim::getCurrentTime()/1000 - gustFrequency + mRand.randRangeF(6);

  targetWindDir = VectorF(0,0,0);
  targetWindDir.y = normalWindDir.y + mRand.randRangeF(7); //randomize the angle a little

  windDir = targetWindDir;
  nextDirChangeTime = Sim::getCurrentTime()/1000 + 180 + mRand.randRangeF(90);

  wind = speed * windDir;

  //cWindDir = windDir;

   S32 dirYaw = (S32)(windDir.y - mNorth) & 360;

	if(dirYaw < 22)
		mWindDir = WD_North;
	else if(dirYaw < 67)
		mWindDir = WD_NorthEast;
	else if(dirYaw < 112)
		mWindDir = WD_East;
	else if(dirYaw < 157)
		mWindDir = WD_SouthEast;
	else if(dirYaw < 202)
		mWindDir = WD_South;
	else if(dirYaw < 247)
		mWindDir = WD_SouthWest;
	else if(dirYaw < 292)
		mWindDir = WD_West;
	else if(dirYaw < 337)
		mWindDir = WD_NorthWest;
	else
		mWindDir = WD_North;

  
	//Wind stuff
    //========================================
}

//converts the inchs of pressure and returns in millibars
F32 environment::convertAPtoMB(F32 inch)
{
	return inch * 33.8637526;
}
F32 environment::convertAPtoInch(F32 mb)
{
	return mb * 0.0295301;
}
//=====================================================

const char * environment::getUnits(const char *string, S32 startIndex, S32 endIndex, const char *set)
{
   S32 sz;
   S32 index = startIndex;
   while(index--)
   {
      if(!*string)
         return "";
      sz = dStrcspn(string, set);
      if (string[sz] == 0)
         return "";
      string += (sz + 1);
   }
   const char *startString = string;
   while(startIndex <= endIndex--)
   {
      sz = dStrcspn(string, set);
      string += sz;
      if (*string == 0)
         break;
      string++;
   }
   if(!*string)
      string++;
   U32 totalSize = (U32(string - startString));
   char *ret = Con::getReturnBuffer(totalSize);
   dStrncpy(ret, startString, totalSize - 1);
   ret[totalSize-1] = '\0';
   return ret;
}

const char * environment::getUnit(const char *string, U32 index, const char *set)
{
   U32 sz;
   while(index--)
   {
      if(!*string)
         return "";
      sz = dStrcspn(string, set);
      if (string[sz] == 0)
         return "";
      string += (sz + 1);
   }
   sz = dStrcspn(string, set);
   if (sz == 0)
      return "";
   char *ret = Con::getReturnBuffer(sz+1);
   dStrncpy(ret, string, sz);
   ret[sz] = '\0';
   return ret;
}
