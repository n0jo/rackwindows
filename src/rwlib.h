#ifndef RWLIB_H
#define RWLIB_H

#include "math.h"

namespace rwlib {

/* #functions
======================================================================================== */

//this denormalization routine produces a white noise at -300 dB which the noise
//shaping will interact with to produce a bipolar output, but the noise is actually
//all positive. That should stop any variables from going denormal, and the routine
//only kicks in if digital black is input. As a final touch, if you save to 24-bit
//the silence will return to being digital black again.
inline long double denormalize(long double inputSample)
{
    if (inputSample < 1.2e-38 && -inputSample < 1.2e-38) {
        static int noisesource = 0;
        //this declares a variable before anything else is compiled. It won't keep assigning
        //it to 0 for every sample, it's as if the declaration doesn't exist in this context,
        //but it lets me add this denormalization fix in a single place rather than updating
        //it in three different locations. The variable isn't thread-safe but this is only
        //a random seed and we can share it with whatever.
        noisesource = noisesource % 1700021;
        noisesource++;
        int residue = noisesource * noisesource;
        residue = residue % 170003;
        residue *= residue;
        residue = residue % 17011;
        residue *= residue;
        residue = residue % 1709;
        residue *= residue;
        residue = residue % 173;
        residue *= residue;
        residue = residue % 17;
        double applyresidue = residue;
        applyresidue *= 0.00000001;
        applyresidue *= 0.00000001;
        inputSample = applyresidue;
    }
    return inputSample;
}

/* #acceleration
======================================================================================== */
struct Acceleration {

    double ataLastOut;
    double s1;
    double s2;
    double s3;
    double o1;
    double o2;
    double o3;
    double m1;
    double m2;
    double des;

    Acceleration()
    {
        ataLastOut = 0.0;
        s1 = s2 = s3 = 0.0;
        o1 = o2 = o3 = 0.0;
        m1 = m2 = des = 0.0;
    }

    long double process(long double inputSample, float limitParam = 0.f, float drywetParam = 1.f, double overallscale = 1.0)
    {
        double intensity = pow(limitParam, 3) * (32 / overallscale);
        double wet = drywetParam;
        double dry = 1.0 - wet;

        double sense;
        double smooth;
        double accumulatorSample;

        double drySample = inputSample;

        s3 = s2;
        s2 = s1;
        s1 = inputSample;
        smooth = (s3 + s2 + s1) / 3.0;
        m1 = (s1 - s2) * ((s1 - s2) / 1.3);
        m2 = (s2 - s3) * ((s1 - s2) / 1.3);
        sense = fabs(m1 - m2);
        sense = (intensity * intensity * sense);
        o3 = o2;
        o2 = o1;
        o1 = sense;
        if (o2 > sense)
            sense = o2;
        if (o3 > sense)
            sense = o3;
        //sense on the most intense

        if (sense > 1.0)
            sense = 1.0;

        inputSample *= (1.0 - sense);

        inputSample += (smooth * sense);

        sense /= 2.0;

        accumulatorSample = (ataLastOut * sense) + (inputSample * (1.0 - sense));
        ataLastOut = inputSample;
        inputSample = accumulatorSample;

        if (wet != 1.0) {
            inputSample = (inputSample * wet) + (drySample * dry);
        }

        return inputSample;
    }
}; /* end Acceleration */

/* #atmochannel
======================================================================================== */
struct AtmosphereChannel {

    long double lastSampleA;
    long double lastSampleB;
    long double lastSampleC;
    long double lastSampleD;
    long double lastSampleE;
    long double lastSampleF;
    long double lastSampleG;
    long double lastSampleH;
    long double lastSampleI;
    long double lastSampleJ;
    long double lastSampleK;
    long double lastSampleL;
    long double lastSampleM;

    long double thresholdA;
    long double thresholdB;
    long double thresholdC;
    long double thresholdD;
    long double thresholdE;
    long double thresholdF;
    long double thresholdG;
    long double thresholdH;
    long double thresholdI;
    long double thresholdJ;
    long double thresholdK;
    long double thresholdL;
    long double thresholdM;

    AtmosphereChannel()
    {
        lastSampleA = 0.0;
        lastSampleB = 0.0;
        lastSampleC = 0.0;
        lastSampleD = 0.0;
        lastSampleE = 0.0;
        lastSampleF = 0.0;
        lastSampleG = 0.0;
        lastSampleH = 0.0;
        lastSampleI = 0.0;
        lastSampleJ = 0.0;
        lastSampleK = 0.0;
        lastSampleL = 0.0;
        lastSampleM = 0.0;

        thresholdA = 0.618033988749894;
        thresholdB = 0.679837387624884;
        thresholdC = 0.747821126387373;
        thresholdD = 0.82260323902611;
        thresholdE = 0.904863562928721;
        thresholdF = 0.995349919221593;
        thresholdG = 1.094884911143752;
        thresholdH = 1.204373402258128;
        thresholdI = 1.32481074248394;
        thresholdJ = 1.457291816732335;
        thresholdK = 1.603020998405568;
        thresholdL = 1.763323098246125;
        thresholdM = 1.939655408070737;
    }

    void update(double overallscale)
    {
        thresholdA = 0.618033988749894 / overallscale;
        thresholdB = 0.679837387624884 / overallscale;
        thresholdC = 0.747821126387373 / overallscale;
        thresholdD = 0.82260323902611 / overallscale;
        thresholdE = 0.904863562928721 / overallscale;
        thresholdF = 0.995349919221593 / overallscale;
        thresholdG = 1.094884911143752 / overallscale;
        thresholdH = 1.204373402258128 / overallscale;
        thresholdI = 1.32481074248394 / overallscale;
        thresholdJ = 1.457291816732335 / overallscale;
        thresholdK = 1.603020998405568 / overallscale;
        thresholdL = 1.763323098246125 / overallscale;
        thresholdM = 1.939655408070737 / overallscale;
    }

    long double process(long double inputSample, double overallscale = 1.0)
    {
        // TODO: block process
        update(overallscale);

        long double clamp;
        double drySample = inputSample;

        // amplitude aspect
        inputSample = sin(inputSample);

        clamp = inputSample - lastSampleA;
        if (clamp > thresholdA)
            inputSample = lastSampleA + thresholdA;
        if (-clamp > thresholdA)
            inputSample = lastSampleA - thresholdA;

        clamp = inputSample - lastSampleB;
        if (clamp > thresholdB)
            inputSample = lastSampleB + thresholdB;
        if (-clamp > thresholdB)
            inputSample = lastSampleB - thresholdB;

        clamp = inputSample - lastSampleC;
        if (clamp > thresholdC)
            inputSample = lastSampleC + thresholdC;
        if (-clamp > thresholdC)
            inputSample = lastSampleC - thresholdC;

        clamp = inputSample - lastSampleD;
        if (clamp > thresholdD)
            inputSample = lastSampleD + thresholdD;
        if (-clamp > thresholdD)
            inputSample = lastSampleD - thresholdD;

        clamp = inputSample - lastSampleE;
        if (clamp > thresholdE)
            inputSample = lastSampleE + thresholdE;
        if (-clamp > thresholdE)
            inputSample = lastSampleE - thresholdE;

        clamp = inputSample - lastSampleF;
        if (clamp > thresholdF)
            inputSample = lastSampleF + thresholdF;
        if (-clamp > thresholdF)
            inputSample = lastSampleF - thresholdF;

        clamp = inputSample - lastSampleG;
        if (clamp > thresholdG)
            inputSample = lastSampleG + thresholdG;
        if (-clamp > thresholdG)
            inputSample = lastSampleG - thresholdG;

        clamp = inputSample - lastSampleH;
        if (clamp > thresholdH)
            inputSample = lastSampleH + thresholdH;
        if (-clamp > thresholdH)
            inputSample = lastSampleH - thresholdH;

        clamp = inputSample - lastSampleI;
        if (clamp > thresholdI)
            inputSample = lastSampleI + thresholdI;
        if (-clamp > thresholdI)
            inputSample = lastSampleI - thresholdI;

        clamp = inputSample - lastSampleJ;
        if (clamp > thresholdJ)
            inputSample = lastSampleJ + thresholdJ;
        if (-clamp > thresholdJ)
            inputSample = lastSampleJ - thresholdJ;

        clamp = inputSample - lastSampleK;
        if (clamp > thresholdK)
            inputSample = lastSampleK + thresholdK;
        if (-clamp > thresholdK)
            inputSample = lastSampleK - thresholdK;

        clamp = inputSample - lastSampleL;
        if (clamp > thresholdL)
            inputSample = lastSampleL + thresholdL;
        if (-clamp > thresholdL)
            inputSample = lastSampleL - thresholdL;

        clamp = inputSample - lastSampleM;
        if (clamp > thresholdM)
            inputSample = lastSampleM + thresholdM;
        if (-clamp > thresholdM)
            inputSample = lastSampleM - thresholdM;

        // store the raw input sample again for use next time
        lastSampleM = lastSampleL;
        lastSampleL = lastSampleK;
        lastSampleK = lastSampleJ;
        lastSampleJ = lastSampleI;
        lastSampleI = lastSampleH;
        lastSampleH = lastSampleG;
        lastSampleG = lastSampleF;
        lastSampleF = lastSampleE;
        lastSampleE = lastSampleD;
        lastSampleD = lastSampleC;
        lastSampleC = lastSampleB;
        lastSampleB = lastSampleA;
        lastSampleA = drySample;

        return inputSample;
    }
};

/* #atmobuss
======================================================================================== */
struct AtmosphereBuss {

    long double lastSampleA;
    long double lastSampleB;
    long double lastSampleC;
    long double lastSampleD;
    long double lastSampleE;
    long double lastSampleF;
    long double lastSampleG;
    long double lastSampleH;
    long double lastSampleI;
    long double lastSampleJ;
    long double lastSampleK;
    long double lastSampleL;
    long double lastSampleM;

    long double thresholdA;
    long double thresholdB;
    long double thresholdC;
    long double thresholdD;
    long double thresholdE;
    long double thresholdF;
    long double thresholdG;
    long double thresholdH;
    long double thresholdI;
    long double thresholdJ;
    long double thresholdK;
    long double thresholdL;
    long double thresholdM;

    AtmosphereBuss()
    {
        lastSampleA = 0.0;
        lastSampleB = 0.0;
        lastSampleC = 0.0;
        lastSampleD = 0.0;
        lastSampleE = 0.0;
        lastSampleF = 0.0;
        lastSampleG = 0.0;
        lastSampleH = 0.0;
        lastSampleI = 0.0;
        lastSampleJ = 0.0;
        lastSampleK = 0.0;
        lastSampleL = 0.0;
        lastSampleM = 0.0;

        thresholdA = 0.618033988749894;
        thresholdB = 0.679837387624884;
        thresholdC = 0.747821126387373;
        thresholdD = 0.82260323902611;
        thresholdE = 0.904863562928721;
        thresholdF = 0.995349919221593;
        thresholdG = 1.094884911143752;
        thresholdH = 1.204373402258128;
        thresholdI = 1.32481074248394;
        thresholdJ = 1.457291816732335;
        thresholdK = 1.603020998405568;
        thresholdL = 1.763323098246125;
        thresholdM = 1.939655408070737;
    }

    void update(double overallscale)
    {
        thresholdA = 0.618033988749894 / overallscale;
        thresholdB = 0.679837387624884 / overallscale;
        thresholdC = 0.747821126387373 / overallscale;
        thresholdD = 0.82260323902611 / overallscale;
        thresholdE = 0.904863562928721 / overallscale;
        thresholdF = 0.995349919221593 / overallscale;
        thresholdG = 1.094884911143752 / overallscale;
        thresholdH = 1.204373402258128 / overallscale;
        thresholdI = 1.32481074248394 / overallscale;
        thresholdJ = 1.457291816732335 / overallscale;
        thresholdK = 1.603020998405568 / overallscale;
        thresholdL = 1.763323098246125 / overallscale;
        thresholdM = 1.939655408070737 / overallscale;
    }

    long double process(long double inputSample, double overallscale = 1.0)
    {
        // TODO: block process
        update(overallscale);

        long double clamp;
        double drySample = inputSample;

        clamp = inputSample - lastSampleA;
        if (clamp > thresholdA)
            inputSample = lastSampleA + thresholdA;
        if (-clamp > thresholdA)
            inputSample = lastSampleA - thresholdA;

        clamp = inputSample - lastSampleB;
        if (clamp > thresholdB)
            inputSample = lastSampleB + thresholdB;
        if (-clamp > thresholdB)
            inputSample = lastSampleB - thresholdB;

        clamp = inputSample - lastSampleC;
        if (clamp > thresholdC)
            inputSample = lastSampleC + thresholdC;
        if (-clamp > thresholdC)
            inputSample = lastSampleC - thresholdC;

        clamp = inputSample - lastSampleD;
        if (clamp > thresholdD)
            inputSample = lastSampleD + thresholdD;
        if (-clamp > thresholdD)
            inputSample = lastSampleD - thresholdD;

        clamp = inputSample - lastSampleE;
        if (clamp > thresholdE)
            inputSample = lastSampleE + thresholdE;
        if (-clamp > thresholdE)
            inputSample = lastSampleE - thresholdE;

        clamp = inputSample - lastSampleF;
        if (clamp > thresholdF)
            inputSample = lastSampleF + thresholdF;
        if (-clamp > thresholdF)
            inputSample = lastSampleF - thresholdF;

        clamp = inputSample - lastSampleG;
        if (clamp > thresholdG)
            inputSample = lastSampleG + thresholdG;
        if (-clamp > thresholdG)
            inputSample = lastSampleG - thresholdG;

        clamp = inputSample - lastSampleH;
        if (clamp > thresholdH)
            inputSample = lastSampleH + thresholdH;
        if (-clamp > thresholdH)
            inputSample = lastSampleH - thresholdH;

        clamp = inputSample - lastSampleI;
        if (clamp > thresholdI)
            inputSample = lastSampleI + thresholdI;
        if (-clamp > thresholdI)
            inputSample = lastSampleI - thresholdI;

        clamp = inputSample - lastSampleJ;
        if (clamp > thresholdJ)
            inputSample = lastSampleJ + thresholdJ;
        if (-clamp > thresholdJ)
            inputSample = lastSampleJ - thresholdJ;

        clamp = inputSample - lastSampleK;
        if (clamp > thresholdK)
            inputSample = lastSampleK + thresholdK;
        if (-clamp > thresholdK)
            inputSample = lastSampleK - thresholdK;

        clamp = inputSample - lastSampleL;
        if (clamp > thresholdL)
            inputSample = lastSampleL + thresholdL;
        if (-clamp > thresholdL)
            inputSample = lastSampleL - thresholdL;

        clamp = inputSample - lastSampleM;
        if (clamp > thresholdM)
            inputSample = lastSampleM + thresholdM;
        if (-clamp > thresholdM)
            inputSample = lastSampleM - thresholdM;

        // without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        // amplitude aspect
        inputSample = asin(inputSample);

        // store the raw input sample again for use next time
        lastSampleM = lastSampleL;
        lastSampleL = lastSampleK;
        lastSampleK = lastSampleJ;
        lastSampleJ = lastSampleI;
        lastSampleI = lastSampleH;
        lastSampleH = lastSampleG;
        lastSampleG = lastSampleF;
        lastSampleF = lastSampleE;
        lastSampleE = lastSampleD;
        lastSampleD = lastSampleC;
        lastSampleC = lastSampleB;
        lastSampleB = lastSampleA;
        lastSampleA = drySample;

        return inputSample;
    }
};

/* #biquadbandpass
======================================================================================== */
struct BiquadBandpass {

    long double biquad[11];
    double K;
    double norm;

    BiquadBandpass()
    {
        for (int i = 0; i < 11; i++) {
            biquad[i] = 0.0;
        }
    }

    void set(long double frequency, long double resonance)
    {
        biquad[0] = frequency;
        biquad[1] = resonance;
        update();
    }

    void setFrequency(long double frequency)
    {
        biquad[0] = frequency;
        update();
    }

    void setResonance(long double resonance)
    {
        biquad[1] = resonance;
        update();
    }

    void update()
    {
        K = tan(M_PI * biquad[0]);
        norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = K / biquad[1] * norm;
        biquad[4] = -biquad[2]; //for bandpass, ignore [3] = 0.0
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    long double process(long double inputSample)
    {
        // encode Console5: good cleanness
        inputSample = sin(inputSample);

        long double tempSample;
        tempSample = (inputSample * biquad[2]) + biquad[7];
        biquad[7] = (-tempSample * biquad[5]) + biquad[8];
        biquad[8] = (inputSample * biquad[4]) - (tempSample * biquad[6]);
        inputSample = tempSample;

        // without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        // decode Console5
        inputSample = asin(inputSample);

        return inputSample;
    }
}; /* end BiquadBandpass */

/* #cans
======================================================================================== */
struct Cans {

    double iirSampleAL;
    double iirSampleAR;
    double aL[1503];
    double aR[1503];
    double dL[1503];
    double dR[1503];
    int ax;
    int dx;

    int mode;

    Cans()
    {
        iirSampleAL = 0.0;
        iirSampleAR = 0.0;
        for (int count = 0; count < 1502; count++) {
            aL[count] = 0.0;
            aR[count] = 0.0;
            dL[count] = 0.0;
            dR[count] = 0.0;
        }
        ax = 1;
        dx = 1;

        mode = 1;
    }

    void setMode(int mode)
    {
        this->mode = mode < 1 || mode > 4 ? 1 : mode;
    }

    int getMode()
    {
        return this->mode;
    }

    void process(long double& inputSampleL, long double& inputSampleR, double overallscale = 1.0)
    {
        int am = (int)149.0 * overallscale;
        int dm = (int)223.0 * overallscale;
        int allpasstemp;

        //we do a volume compensation immediately to gain stage stuff cleanly
        if (mode == 1) {
            inputSampleL *= 0.855;
            inputSampleR *= 0.855;
        }
        if (mode == 2) {
            inputSampleL *= 0.748;
            inputSampleR *= 0.748;
        }
        if (mode == 3) {
            inputSampleL *= 0.713;
            inputSampleR *= 0.713;
        }
        if (mode == 4) {
            inputSampleL *= 0.680;
            inputSampleR *= 0.680;
        }

        // everything runs 'inside' Console
        // console channel
        inputSampleL = sin(inputSampleL);
        inputSampleR = sin(inputSampleR);

        // bass narrowing filter (we are using the iir filters from out of SubsOnly)
        long double drySample = inputSampleL;
        long double drySampleR = inputSampleR;
        long double bass = (mode * mode * 0.00001) / overallscale;
        long double mid = inputSampleL + inputSampleR;
        long double side = inputSampleL - inputSampleR;
        iirSampleAL = (iirSampleAL * (1.0 - (bass * 0.618))) + (side * bass * 0.618);
        side = side - iirSampleAL;
        inputSampleL = (mid + side) / 2.0;
        inputSampleR = (mid - side) / 2.0;

        // a darkened Midiverb-style allpass
        allpasstemp = ax - 1;
        if (allpasstemp < 0 || allpasstemp > am)
            allpasstemp = am;
        inputSampleL -= aL[allpasstemp] * 0.5;
        aL[ax] = inputSampleL;
        inputSampleL *= 0.5;
        inputSampleR -= aR[allpasstemp] * 0.5;
        aR[ax] = inputSampleR;
        inputSampleR *= 0.5;
        ax--;
        if (ax < 0 || ax > am) {
            ax = am;
        }
        inputSampleL += (aL[ax]) * 0.5;
        inputSampleR += (aR[ax]) * 0.5;
        if (ax == am) {
            inputSampleL += (aL[0]) * 0.5;
            inputSampleR += (aR[0]) * 0.5;
        } else {
            inputSampleL += (aL[ax + 1]) * 0.5;
            inputSampleR += (aR[ax + 1]) * 0.5;
        }

        // Cans A suppresses the crossfeed more, Cans B makes it louder
        if (mode == 1) {
            inputSampleL *= 0.125;
            inputSampleR *= 0.125;
        }
        if (mode == 2) {
            inputSampleL *= 0.25;
            inputSampleR *= 0.25;
        }
        if (mode == 3) {
            inputSampleL *= 0.30;
            inputSampleR *= 0.30;
        }
        if (mode == 4) {
            inputSampleL *= 0.35;
            inputSampleR *= 0.35;
        }

        //the crossfeed
        drySample += inputSampleR;
        drySampleR += inputSampleL;

        //a darkened Midiverb-style allpass, which is stretching the previous one even more
        allpasstemp = dx - 1;
        if (allpasstemp < 0 || allpasstemp > dm)
            allpasstemp = dm;
        inputSampleL -= dL[allpasstemp] * 0.5;
        dL[dx] = inputSampleL;
        inputSampleL *= 0.5;
        inputSampleR -= dR[allpasstemp] * 0.5;
        dR[dx] = inputSampleR;
        inputSampleR *= 0.5;
        dx--;
        if (dx < 0 || dx > dm) {
            dx = dm;
        }
        inputSampleL += (dL[dx]) * 0.5;
        inputSampleR += (dR[dx]) * 0.5;
        if (dx == dm) {
            inputSampleL += (dL[0]) * 0.5;
            inputSampleR += (dR[0]) * 0.5;
        } else {
            inputSampleL += (dL[dx + 1]) * 0.5;
            inputSampleR += (dR[dx + 1]) * 0.5;
        }

        //for all versions of Cans the second level of bloom is this far down
        //and, remains on the opposite speaker rather than crossing again to the original side
        inputSampleL *= 0.25;
        inputSampleR *= 0.25;

        //add the crossfeed and very faint extra verbyness
        drySample += inputSampleR;
        drySampleR += inputSampleL;

        //and output our can-opened headphone feed
        inputSampleL = drySample;
        inputSampleR = drySampleR;

        //bass narrowing filter
        mid = inputSampleL + inputSampleR;
        side = inputSampleL - inputSampleR;
        iirSampleAR = (iirSampleAR * (1.0 - bass)) + (side * bass);
        side = side - iirSampleAR;
        inputSampleL = (mid + side) / 2.0;
        inputSampleR = (mid - side) / 2.0;

        // ConsoleBuss
        if (inputSampleL > 1.0)
            inputSampleL = 1.0;
        if (inputSampleL < -1.0)
            inputSampleL = -1.0;
        inputSampleL = asin(inputSampleL);
        if (inputSampleR > 1.0)
            inputSampleR = 1.0;
        if (inputSampleR < -1.0)
            inputSampleR = -1.0;
        inputSampleR = asin(inputSampleR);
    }
}; /* end Cans */

/* #dark
======================================================================================== */
struct Dark {

    float lastSample[100];

    Dark()
    {
        for (int count = 0; count < 99; count++) {
            lastSample[count] = 0;
        }
    }

    long double process(long double inputSample, double overallscale = 1.0, bool highres = true)
    {
        int depth = (int)(17.0 * overallscale);
        if (depth < 3)
            depth = 3;
        if (depth > 98)
            depth = 98;

        float scaleFactor;
        if (highres)
            scaleFactor = 8388608.0;
        else
            scaleFactor = 32768.0;
        float outScale = scaleFactor;
        if (outScale < 8.0)
            outScale = 8.0;

        // //0-1 is now one bit, now we dither
        inputSample *= scaleFactor;

        //to do this style of dither, we quantize in either direction and then
        //do a reconstruction of what the result will be for each choice.
        //We then evaluate which one we like, and keep a history of what we previously had
        int quantA = floor(inputSample);
        int quantB = floor(inputSample + 1.0);

        //we have an average of all recent slews
        //we are doing that to voice the thing down into the upper mids a bit
        //it mustn't just soften the brightest treble, it must smooth high mids too
        float expectedSlew = 0;
        for (int x = 0; x < depth; x++) {
            expectedSlew += (lastSample[x + 1] - lastSample[x]);
        }
        expectedSlew /= depth;

        float testA = fabs((lastSample[0] - quantA) - expectedSlew);
        float testB = fabs((lastSample[0] - quantB) - expectedSlew);

        //select whichever one departs LEAST from the vector of averaged
        //reconstructed previous final samples. This will force a kind of dithering
        //as it'll make the output end up as smooth as possible
        if (testA < testB)
            inputSample = quantA;
        else
            inputSample = quantB;

        for (int x = depth; x >= 0; x--) {
            lastSample[x + 1] = lastSample[x];
        }
        lastSample[0] = inputSample;

        inputSample /= outScale;

        return inputSample;
    }
}; /* end Dark */

/* #peaksonly
======================================================================================== */
struct PeaksOnly {

    double a[1503];
    double b[1503];
    double c[1503];
    double d[1503];

    int ax, bx, cx, dx;

    PeaksOnly()
    {
        for (int count = 0; count < 1502; count++) {
            a[count] = 0.0;
            b[count] = 0.0;
            c[count] = 0.0;
            d[count] = 0.0;
        }
        ax = 1;
        bx = 1;
        cx = 1;
        dx = 1;
    }

    long double process(long double inputSample, double overallscale = 1.0)
    {
        int am = (int)149.0 * overallscale;
        int bm = (int)179.0 * overallscale;
        int cm = (int)191.0 * overallscale;
        int dm = (int)223.0 * overallscale; //these are 'good' primes, spacing out the allpasses
        int allpasstemp = 0;

        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        //amplitude aspect
        inputSample = asin(inputSample);

        //a single Midiverb-style allpass
        allpasstemp = ax - 1;
        if (allpasstemp < 0 || allpasstemp > am)
            allpasstemp = am;
        inputSample -= a[allpasstemp] * 0.5;
        a[ax] = inputSample;
        inputSample *= 0.5;
        ax--;
        if (ax < 0 || ax > am) {
            ax = am;
        }
        inputSample += (a[ax]);

        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        //amplitude aspect
        inputSample = asin(inputSample);

        //a single Midiverb-style allpass
        allpasstemp = bx - 1;
        if (allpasstemp < 0 || allpasstemp > bm)
            allpasstemp = bm;
        inputSample -= b[allpasstemp] * 0.5;
        b[bx] = inputSample;
        inputSample *= 0.5;
        bx--;
        if (bx < 0 || bx > bm) {
            bx = bm;
        }
        inputSample += (b[bx]);

        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        //amplitude aspect
        inputSample = asin(inputSample);

        //a single Midiverb-style allpass
        allpasstemp = cx - 1;
        if (allpasstemp < 0 || allpasstemp > cm)
            allpasstemp = cm;
        inputSample -= c[allpasstemp] * 0.5;
        c[cx] = inputSample;
        inputSample *= 0.5;
        cx--;
        if (cx < 0 || cx > cm) {
            cx = cm;
        }
        inputSample += (c[cx]);

        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        //amplitude aspect
        inputSample = asin(inputSample);

        //a single Midiverb-style allpass
        allpasstemp = dx - 1;
        if (allpasstemp < 0 || allpasstemp > dm)
            allpasstemp = dm;
        inputSample -= d[allpasstemp] * 0.5;
        d[dx] = inputSample;
        inputSample *= 0.5;
        dx--;
        if (dx < 0 || dx > dm) {
            dx = dm;
        }
        inputSample += (d[dx]);

        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        //amplitude aspect
        inputSample = asin(inputSample);

        inputSample *= 0.63679; //scale it to 0dB output at full blast

        return inputSample;
    }
}; /* end PeaksOnly */

/* #Slew
======================================================================================== */
struct Slew {

    double lastSample;

    Slew()
    {
        lastSample = 0.0;
    }

    long double process(long double inputSample, float clampParam = 0.f, double overallscale = 1.0)
    {
        double clamp;
        double threshold = pow((1 - clampParam), 4) / overallscale;
        double outputSample;

        clamp = inputSample - lastSample;
        outputSample = inputSample;
        if (clamp > threshold)
            outputSample = lastSample + threshold;
        if (-clamp > threshold)
            outputSample = lastSample - threshold;
        lastSample = outputSample;

        return outputSample;
    }
}; /* end Slew */

/* #slew2
======================================================================================== */
struct Slew2 {

    double LataLast3Sample;
    double LataLast2Sample;
    double LataLast1Sample;
    double LataHalfwaySample;
    double LataHalfDrySample;
    double LataHalfDiffSample;
    double LataA;
    double LataB;
    double LataC;
    double LataDecay;
    double LataUpsampleHighTweak;
    double LataDrySample;
    double LataDiffSample;
    double LataPrevDiffSample;

    bool LataFlip; //end defining of antialiasing variables

    double lastSample;

    Slew2()
    {
        LataLast3Sample = LataLast2Sample = LataLast1Sample = 0.0;
        LataHalfwaySample = LataHalfDrySample = LataHalfDiffSample = 0.0;
        LataA = LataB = LataC = LataDrySample = LataDiffSample = LataPrevDiffSample = 0.0;
        LataUpsampleHighTweak = 0.0414213562373095048801688; //more adds treble to upsampling
        LataDecay = 0.915965594177219015; //Catalan's constant, more adds focus and clarity
        lastSample = 0.0;

        LataFlip = false; //end reset of antialias parameters
    }

    long double process(long double inputSample, float clampParam = 0.f, double overallscale = 1.0)
    {
        double clamp;
        double threshold = pow((1 - clampParam), 4) / overallscale;

        LataDrySample = inputSample;

        LataHalfDrySample = LataHalfwaySample = (inputSample + LataLast1Sample + ((-LataLast2Sample + LataLast3Sample) * LataUpsampleHighTweak)) / 2.0;
        LataLast3Sample = LataLast2Sample;
        LataLast2Sample = LataLast1Sample;
        LataLast1Sample = inputSample;
        //setting up oversampled special antialiasing
        //begin first half- change inputSample -> LataHalfwaySample, LataDrySample -> LataHalfDrySample
        clamp = LataHalfwaySample - LataHalfDrySample;
        if (clamp > threshold)
            LataHalfwaySample = lastSample + threshold;
        if (-clamp > threshold)
            LataHalfwaySample = lastSample - threshold;
        lastSample = LataHalfwaySample;
        //end first half
        //begin antialiasing section for halfway sample
        LataC = LataHalfwaySample - LataHalfDrySample;
        if (LataFlip) {
            LataA *= LataDecay;
            LataB *= LataDecay;
            LataA += LataC;
            LataB -= LataC;
            LataC = LataA;
        } else {
            LataB *= LataDecay;
            LataA *= LataDecay;
            LataB += LataC;
            LataA -= LataC;
            LataC = LataB;
        }
        LataHalfDiffSample = (LataC * LataDecay);
        LataFlip = !LataFlip;
        //end antialiasing section for halfway sample
        //begin second half- inputSample and LataDrySample handled separately here
        clamp = inputSample - lastSample;
        if (clamp > threshold)
            inputSample = lastSample + threshold;
        if (-clamp > threshold)
            inputSample = lastSample - threshold;
        lastSample = inputSample;
        //end second half
        //begin antialiasing section for input sample
        LataC = inputSample - LataDrySample;
        if (LataFlip) {
            LataA *= LataDecay;
            LataB *= LataDecay;
            LataA += LataC;
            LataB -= LataC;
            LataC = LataA;
        } else {
            LataB *= LataDecay;
            LataA *= LataDecay;
            LataB += LataC;
            LataA -= LataC;
            LataC = LataB;
        }
        LataDiffSample = (LataC * LataDecay);
        LataFlip = !LataFlip;
        //end antialiasing section for input sample
        inputSample = LataDrySample;
        inputSample += ((LataDiffSample + LataHalfDiffSample + LataPrevDiffSample) / 0.734);
        LataPrevDiffSample = LataDiffSample / 2.0;
        //apply processing as difference to non-oversampled raw input

        return inputSample;
    }
}; /* end Slew2 */

/* #slewonly
======================================================================================== */
struct SlewOnly {

    double lastSample;

    SlewOnly()
    {
        lastSample = 0.0;
    }

    long double process(long double inputSample)
    {
        long double outputSample;
        double trim = 2.302585092994045684017991; //natural logarithm of 10

        outputSample = (inputSample - lastSample) * trim;
        lastSample = inputSample;
        if (outputSample > 1.0)
            outputSample = 1.0;
        if (outputSample < -1.0)
            outputSample = -1.0;

        return outputSample;
    }
}; /* end SlewOnly */

/* #subsonly
======================================================================================== */
struct SubsOnly {

    double iirSampleA;
    double iirSampleB;
    double iirSampleC;
    double iirSampleD;
    double iirSampleE;
    double iirSampleF;
    double iirSampleG;
    double iirSampleH;
    double iirSampleI;
    double iirSampleJ;
    double iirSampleK;
    double iirSampleL;
    double iirSampleM;
    double iirSampleN;
    double iirSampleO;
    double iirSampleP;
    double iirSampleQ;
    double iirSampleR;
    double iirSampleS;
    double iirSampleT;
    double iirSampleU;
    double iirSampleV;
    double iirSampleW;
    double iirSampleX;
    double iirSampleY;
    double iirSampleZ;

    SubsOnly()
    {
        iirSampleA = 0.0;
        iirSampleB = 0.0;
        iirSampleC = 0.0;
        iirSampleD = 0.0;
        iirSampleE = 0.0;
        iirSampleF = 0.0;
        iirSampleG = 0.0;
        iirSampleH = 0.0;
        iirSampleI = 0.0;
        iirSampleJ = 0.0;
        iirSampleK = 0.0;
        iirSampleL = 0.0;
        iirSampleM = 0.0;
        iirSampleN = 0.0;
        iirSampleO = 0.0;
        iirSampleP = 0.0;
        iirSampleQ = 0.0;
        iirSampleR = 0.0;
        iirSampleS = 0.0;
        iirSampleT = 0.0;
        iirSampleU = 0.0;
        iirSampleV = 0.0;
        iirSampleW = 0.0;
        iirSampleX = 0.0;
        iirSampleY = 0.0;
        iirSampleZ = 0.0;
    }

    long double process(long double inputSample, double overallscale = 1.0)
    {
        double iirAmount = 2250 / 44100.0;
        double gaintarget = 1.42;
        double gain;
        iirAmount /= overallscale;
        double altAmount = 1.0 - iirAmount;

        gain = gaintarget;

        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        iirSampleA = (iirSampleA * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleA;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleB = (iirSampleB * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleB;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleC = (iirSampleC * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleC;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleD = (iirSampleD * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleD;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleE = (iirSampleE * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleE;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleF = (iirSampleF * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleF;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleG = (iirSampleG * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleG;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleH = (iirSampleH * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleH;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleI = (iirSampleI * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleI;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleJ = (iirSampleJ * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleJ;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleK = (iirSampleK * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleK;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleL = (iirSampleL * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleL;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleM = (iirSampleM * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleM;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleN = (iirSampleN * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleN;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleO = (iirSampleO * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleO;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleP = (iirSampleP * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleP;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleQ = (iirSampleQ * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleQ;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleR = (iirSampleR * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleR;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleS = (iirSampleS * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleS;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleT = (iirSampleT * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleT;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleU = (iirSampleU * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleU;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleV = (iirSampleV * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleV;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleW = (iirSampleW * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleW;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleX = (iirSampleX * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleX;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleY = (iirSampleY * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleY;
        inputSample *= gain;
        gain = ((gain - 1) * 0.75) + 1;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;
        iirSampleZ = (iirSampleZ * altAmount) + (inputSample * iirAmount);
        inputSample = iirSampleZ;
        if (inputSample > 1.0)
            inputSample = 1.0;
        if (inputSample < -1.0)
            inputSample = -1.0;

        gain = gaintarget;

        return inputSample;
    }
}; /* end SubsOnly */

} // namespace rwlib

#endif // RWLIB_H