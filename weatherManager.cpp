//-----------------------------------------------------------------------------
// Torque Game Engine 
// Copyright (C) GarageGames.com, Inc.
// Weather GameManager mod by Bil Simser (bsimser@shaw.ca)
//-----------------------------------------------------------------------------
#include "enviornment/enviornment.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void enviornment::setupWeather()
{
    // Initialize the weather system

	F32 press = convertAPtoMB(mPressure);

	//mPressure = 960;
	//expand better for seasons
	if((mSeason == 2 || mSeason == 3))
		press += mRand.randI(1,50);
	else
		press += mRand.randI(1,80);

	/*if(($time_info.month >= 7) && ($time_info.month <= 12))
		$weather_info.pressure += dice(1, 50);
	else
		$weather_info.pressure += dice(1, 80);*/
	    
	//$weather_info.change = 0;
	if(mSkyState)
	{
		switch(mSkyState)
		{
			case SKY_Cloudless:
			{
				press = 1030;
				break;
			}
			case SKY_Cloudy:
			{
				mSky->stormCloudsOn(1, mSeasons[mSeason]->timeEffect);
				//mSky->stormFogOn(1, mSeasons[mSeason]->timeEffect);
				press = 1010;
				break;
			}
			case SKY_Raining:
			{
				mSky->stormCloudsOn(1, mSeasons[mSeason]->timeEffect);
				//mSky->stormFogOn(1, mSeasons[mSeason]->timeEffect);
				startRain();
				press = 990;
				break;
			}
			case SKY_Lightning:
			{
				mSky->stormCloudsOn(1, mSeasons[mSeason]->timeEffect);
				//mSky->stormFogOn(1, mSeasons[mSeason]->timeEffect);
				startRain();
				startLightning();
				press = 970;
				break;
			}
		}
	}
	else
	{
		if(press <= 980)
			mSkyState = SKY_Lightning;
		else if(press <= 1000)
			mSkyState = SKY_Raining;
		else if(press <= 1020)
			mSkyState = SKY_Cloudy;
		else
			mSkyState = SKY_Cloudless;
	}

	//convert back
	mPressure = convertAPtoInch(press);
}

//-----------------------------------------------------------------------------
// This updates the weather changes based on time, pressure 
// and a random value. It also tells the engine to create the
// effects for the weather changes (rain, lightning, fog, etc.)
//-----------------------------------------------------------------------------
void enviornment::updateWeather()
{
    S32 diff = 0, change = 0;

	F32 press = convertAPtoMB(mPressure);
    
    // Later months in year don't need as much pressure to create a change
    // (or you can change this to suit your environment)
    //if(($time_info.month >= 9) && ($time_info.month <= 12))
	if((mMonth >= 9) && (mMonth <= 12))
		diff = (press > 985 ? -2 : 2);
    else
		diff = (press > 1015 ? -2 : 2);

    change += (mRand.randI(1, 4) * diff + mRand.randI(2, 6) - mRand.randI(2, 6));

    // Cap the changes so weather isn't too drastic
    change = mClamp(change, -12, 12);
    //$weather_info.change = max($weather_info.change, -12);	
    
    press += change;
    
    // Cap the pressure values so we don't go crazy with weather changes
	//should actually go into the 800's potentially, but those are extreme situations like hurricanes
    press = mClamp(press, 960, 1040);
	
    change = 0;

    // Based on the current sky conditions and the change
    // value, determine what the new weather should be    
    switch(mSkyState)
    {
		case SKY_Cloudless:
			if(press < 990)
				change = 1;
			else if(press < 1010)
				if(mRand.randI(1, 4) == 1)
					change = 1;
			    
		case SKY_Cloudy:
			if(press < 970)
				change = 2;
			else if(press < 990)
			{
				if(mRand.randI(1, 4) == 1)
					change = 2;
				else
					change = 0;
			} 
			else if(press > 1030)
			{
				if(mRand.randI(1, 4) == 1)
					change = 3;
			}
			    
		case SKY_Raining:
			if(press < 970)
			{
				if(mRand.randI(1, 4) == 1)
					change = 4;
				else
					change = 0;
			}
			else if(press > 1030)
				change = 5;
			else if(press > 1010)
				if(mRand.randI(1, 4) == 1)
					change = 5;
			    
		case SKY_Lightning:
			if(press > 1010)
				change = 6;
			else if(press > 990)
				if(mRand.randI(1, 4) == 1)
					change = 6;
			    
		default:
			mSkyState = SKY_Cloudless;
			change = 0;
    }
    
    // Implement the weather change now    
    switch(change)
    {
		case 1:
			//cloudy
			//used time_to_storm originally
			mSky->stormCloudsOn(1, mSeasons[mSeason]->timeEffect);
			//mSky->stormFogOn(1, mSeasons[mSeason]->timeEffect);
			mSkyState = SKY_Cloudy;
		    
		case 2:
			//rain
			startRain();
			mSkyState = SKY_Raining;
		    
		case 3:
			//clouds clear
			mSky->stormCloudsOn(0, mSeasons[mSeason]->timeEffect);
			//mSky->stormFogOn(0, mSeasons[mSeason]->timeEffect);
			mSkyState = SKY_Cloudless;
		    
		case 4:
			//lighting :o
			startLightning();
			mSkyState = SKY_Lightning;
	    
		case 5:
			//rain stops
			stopRain();
			mSkyState = SKY_Cloudy;
		    
		case 6:
			//lightning stops
			stopLightning();
			mSkyState = SKY_Raining;
		}

	//roll the pressure changes back
	mPressure = convertAPtoInch(press);
}

//-----------------------------------------------------------------------------
// Support methods below to start and stop the rain and lightning effects
//-----------------------------------------------------------------------------

void enviornment::startRain()
{
    if(!mPrecip)
    {
		mPrecip = new Precipitation();

		//set the DB
		GameBaseData* data;
	    if (Sim::findObject(mPrecipDB,data)) {
		   mPrecip->setDataBlock(data);
	    }

		mPrecip->mMinSpeed = 2.5;
		mPrecip->mMaxSpeed = 3.0;
		mPrecip->mNumDrops = 3000;
		mPrecip->mBoxWidth = 200;
		mPrecip->mBoxHeight = 100;
		mPrecip->mMinMass = 1.0;
		mPrecip->mMaxMass = 2.0;
		mPrecip->mRotateWithCamVel = true;
		mPrecip->mDoCollision = true;
		mPrecip->mUseTurbulence = false;

		if (isServerObject()){
			mPrecip->assignName("Precipitation");
		}

		// Register the Object.
		if (!mPrecip->registerObject()){
		   delete mPrecip;
		   return;
		}

		SimGroup* pMissionGroup = dynamic_cast<SimGroup*>(Sim::findObject("MissionGroup"));
		pMissionGroup->addObject(mPrecip);
    }
}

void enviornment::stopRain()
{
    if(mPrecip)
		delete mPrecip;
}

void enviornment::startLightning()
{
    if (!mLightning)
    {
		mLightning = new Lightning();

		//set the DB
		GameBaseData* data;
	    if (Sim::findObject(mLightnDB,data)) {
		   mLightning->setDataBlock(data);
	    }

		mLightning->setPosition(Point3F(350, 300, 180));
		mLightning->setScale(VectorF(250, 400, 500));
		mLightning->strikesPerMinute = 2;
		mLightning->strikeWidth = 2.5;
		mLightning->chanceToHitTarget = 100;
		mLightning->strikeRadius = 50;
		mLightning->boltStartRadius = 20;
		mLightning->color = ColorF(1.0f, 1.0f, 1.0f, 1.0f);
		mLightning->fadeColor = ColorF(0.1f, 0.1f, 1.0f, 1.0f);
		mLightning->useFog = 0;

		if (isServerObject()){
			mLightning->assignName("Lightning");
		}

		// Register the Object.
		if (!mLightning->registerObject()){
		   delete mLightning;
		   return;
		}

		SimGroup* pMissionGroup = dynamic_cast<SimGroup*>(Sim::findObject("MissionGroup"));
		pMissionGroup->addObject(mLightning);
    }
}

void enviornment::stopLightning()
{
    if(mLightning)
		delete mLightning;
}