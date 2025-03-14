//#include <stdio.h>
#include <math.h>
//#include <stdlib.h>

class CTempRingBuffer
{
    enum { mLength = 256 }; // 128 -> 10 min recording
    float mBuffer[mLength];
    int mIndex{0};
    
public:
    CTempRingBuffer()
    {
        for (int i=0; i<mLength; i++)
            mBuffer[i] = 0;
    }
    void operator <<(float temperature)
    {
        mIndex--;
        if (mIndex <= 0)
            mIndex = mLength-1;
        mBuffer[mIndex] = temperature;
    }
    float operator[](int index) const
    {
        return mBuffer[(mIndex+index)%mLength];
    }
    int GetLength() const
    {
        return mLength;
    }
};

CTempRingBuffer mBuffer;

class CFindMax
{
    //const CTempRingBuffer& mBuffer;
    // Buffer samples the temperature in 5 second intervals
    // and holds the lowest temperature observed during this period
    static constexpr int mSamplePeriod = 5.f;

public:
//    CFindMax(CTempRingBuffer& buffer) : mBuffer{buffer}
//    {
//    }
    
    void FindMax(float& maxPu, float& maxTemp, int& maxDuration)
    {
        maxPu = 0;
        maxTemp = 0;
        maxDuration = 0;
        
        // Take latest temperature
        float temp = mBuffer[0];
        while (temp > 0)
        {
            int duration = 0;
            // Find how long the temperature was higher or equal
            float nextTemperature = FindRun(temp, duration);
            
            // calculate PU for temp, duration
            float pu = CalculatePu(temp, duration);
            if (pu > maxPu)
            {
                maxPu = pu;
                if (pu > 1)
                {
                    maxTemp = temp;
                    maxDuration = duration;
                }
            }
            if (pu < 1)
                break;
            temp = nextTemperature;
        }
    }
    
    float CalculatePu(float temperatureCelsius, float durationSeconds)
    {
        return (durationSeconds / 60.f) * pow(1.393f, temperatureCelsius-60.f);
    }
    
    float FindRun(float temp, int& duration)
    {
        for (int i=0; i<mBuffer.GetLength(); i++)
            if (mBuffer[i] >= temp)
                duration += mSamplePeriod;
            else
                return mBuffer[i];
                
        // no lower temperature than temp in buffer, do not continue
        return 0.f;
    }
};

CFindMax FindMax;
/*
class CGenerator
{
public:
    float operator[](int seconds)
    {
        constexpr int period = 200;
        constexpr float minTemp = 50;
        constexpr float maxTemp = 90;
        float phase;
        
        seconds %= period;
        if (seconds < period/2)
            phase = seconds/float(period/2);
        else
            phase = 2-seconds/float(period/2);

        return minTemp + phase*(maxTemp-minTemp) - rand()%5;
    }
};

int main()
{
    CTempRingBuffer Buffer;
    CFindMax FindMax{Buffer};
    CGenerator Generator;
    const int test[]= {70, 71, 72, 73, 72, 72, 73, 72, 72, 71, 0};
    
//    for (int i=0; test[i]; i++)
    for (int i=0; i<1000; i+=5)
    {
        float maxPu, maxTemp, maxDuration;
        float temp;
        //temp = test[i];
        temp = Generator[i];
        Buffer << temp;
        FindMax.FindMax(maxPu, maxTemp, maxDuration);
        //for (int j=0; j<10; j++)
        //    printf("%d ", (int)Buffer[j]);
        printf("%02d:%02d temp=%.1f PU=%.0f (%.1f C, %.0f s)\n",
            i/60, i%60, temp, maxPu, maxTemp, maxDuration);
    }

    return 0;
}
*/

class CAccumulator
{
    float pu{0.0f};
    const float margin{1.0f};
    float lastT{-1};
    float avgT{0};
public:
    void operator <<(float T)
    {
        if (lastT != -1)
        {
            avgT = (lastT + T) / 2.0 - margin;
            if (avgT < 55.0f)
            {
                // pasterizacia by mala prebiehat od 55C
                // ak klesneme pod 55C teoreticky bakterie sa uz mozu mnozit
                // co PU nemodeluje - takze radsej zacneme od znova
                pu = 0;
            } else {
                // akumulacia PU pre priemernu teplotu za predosle obdobie
                const float dt = 5.0f; // 5s
                pu += (dt/60.0f) * pow(1.393f, avgT-60);
            }
        }
        lastT = T;
    }
    float Get()
    {
        return pu;
    }
    int RemainToPu(float targetPu)
    {
        if (pu >= targetPu)
            return 0.f;
            
        float aux = (targetPu-pu)/pow(1.393f, avgT-60)*60;
        return (aux > 60*20) ? 60*20 : (int)aux;
    }
};
CAccumulator Accumulator;

class CCollect
{
  int mSamples{0};
  float mCollectMin{0};
  float mCollectMax{0};

  float mMin{0};
  float mMax{0};
  float mMaxPu{0};
  float mMaxMaxPu{0};
  float mMaxTemp{0};
  int mMaxDuration{0};

  // accumulator
  int mAccumulatorPu{0};
  int mAccumulatorTo50{0};

public:
  void Add(int value)
  {
    float temp = value/16.0f;
    if (mSamples == 0)
      mCollectMin = mCollectMax = temp;
    mSamples++;
    mCollectMin = min(mCollectMin, temp);
    mCollectMax = max(mCollectMax, temp);
  }
  void Finish()
  {
    if (mSamples > 0)
    {
      mMax = mCollectMax;
      mMin = mCollectMin;
      mBuffer << mMin;
        FindMax.FindMax(mMaxPu, mMaxTemp, mMaxDuration);
        mMaxMaxPu = max(mMaxMaxPu, mMaxPu);
      Accumulator << mMin;
      mAccumulatorPu = (int)Accumulator.Get();
      mAccumulatorTo50 = (int)Accumulator.RemainToPu(50.f);
    }
    mSamples = 0;
  }

  char* Info()
  {
      // TOP: PU 15 (75.3 deg, 120 s)
      // BOTTOM: min 45 deg, max 49deg, maxPu: 17
      
      BIOS::LCD::Printf(8, BIOS::LCD::Height-16, RGB565(b0b0b0), RGB565(000000),
            "min: %.1f\xf8" "C, max: %.1f\xf8" "C    ",
            mMin, mMax);

    static char info[64];
    sprintf(info, "PU_flat: %d (%.1f" "\xf8" " %ds) max: %d ",
      (int)mMaxPu, mMaxTemp, mMaxDuration, (int)mMaxMaxPu);

            BIOS::LCD::Printf(10+16, 16+20+2-8-16, RGB565(b0b0b0), RGB565(404040), info);

    sprintf(info, "PU_acc: %d (%02d:%02ds to 50 pu)  ",
      (int)mAccumulatorPu, mAccumulatorTo50/60, mAccumulatorTo50%60);

            BIOS::LCD::Printf(10+16, 16+20+2-8, RGB565(000000), MyGui::TitleColor, info);
    return info;
  }
};

CCollect Collect;
