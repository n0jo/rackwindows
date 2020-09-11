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

/* #electrohat
======================================================================================== */
struct ElectroHat {

    double storedSample;
    double lastSample;
    int tik;
    int lok;
    bool flip;

    ElectroHat()
    {
        storedSample = 0.0;
        lastSample = 0.0;
        tik = 3746926;
        lok = 0;
        flip = true;
    }

    long double process(long double inputSample, float typeParam, float trimParam, float brightnessParam, float drywetParam, double overallscale = 1.0, float sampleRate = 44100.f)
    {
        //we will go to another dither for 88 and 96K
        bool highSample = false;
        if (sampleRate > 64000)
            highSample = true;

        double drySample;
        double tempSample;

        // int deSyn = (int)(typeParam * 5.999) + 1;
        int deSyn = typeParam;
        double increment = trimParam;
        double brighten = brightnessParam;
        double outputlevel = 1.0;
        double wet = drywetParam;
        double dry = 1.0 - wet;

        if (deSyn == 4) {
            deSyn = 1;
            increment = 0.411;
            brighten = 0.87;
        }
        // 606 preset
        if (deSyn == 5) {
            deSyn = 2;
            increment = 0.111;
            brighten = 1.0;
        }
        // 808 preset
        if (deSyn == 6) {
            deSyn = 2;
            increment = 0.299;
            brighten = 0.359;
        }
        // 909 preset
        int tok = deSyn + 1;
        increment *= 0.98;
        increment += 0.01;
        increment += (double)tok;
        double fosA = increment;
        double fosB = fosA * increment;
        double fosC = fosB * increment;
        double fosD = fosC * increment;
        double fosE = fosD * increment;
        double fosF = fosE * increment;
        int posA = fosA;
        int posB = fosB;
        int posC = fosC;
        int posD = fosD;
        int posE = fosE;
        int posF = fosF;
        int posG = posF * posE * posD * posC * posB; // factorial

        drySample = inputSample;

        inputSample = fabs(inputSample) * outputlevel;

        if (flip) { // will always be true unless we have high sample rate
            tik++;
            tik = tik % posG;
            tok = tik * tik;
            tok = tok % posF;
            tok *= tok;
            tok = tok % posE;
            tok *= tok;
            tok = tok % posD;
            tok *= tok;
            tok = tok % posC;
            tok *= tok;
            tok = tok % posB;
            tok *= tok;
            tok = tok % posA;

            inputSample = tok * inputSample;
            if ((abs(lok - tok) < abs(lok + tok)) && (deSyn == 1)) {
                inputSample = -tok * inputSample;
            }
            if ((abs(lok - tok) > abs(lok + tok)) && (deSyn == 2)) {
                inputSample = -tok * inputSample;
            }
            if ((abs(lok - tok) < abs(lok + tok)) && (deSyn == 3)) {
                inputSample = -tok * inputSample;
            }

            lok = tok;

            tempSample = inputSample;
            inputSample = inputSample - (lastSample * brighten);
            lastSample = tempSample;

        } else { // we have high sample rate and this is an interpolate sample
            inputSample = lastSample;
            // not really interpolating, just sample-and-hold
        }

        if (highSample) {
            flip = !flip;
        } else {
            flip = true;
        }

        tempSample = inputSample;
        inputSample += storedSample;
        storedSample = tempSample;

        if (wet != 1.0) {
            inputSample = (inputSample * wet) + (drySample * dry);
        }

        return inputSample;
    }
}; /* end ElectroHat */

/* #golem
======================================================================================== */
struct Golem {

    double p[4099];
    bool flip;
    int count;

    Golem()
    {
        for (int i = 0; i < 4098; i++) {
            p[i] = 0.0;
        }
        flip = true;
        count = 0;
    }

    long double process(long double inputSampleL, long double inputSampleR, float balanceParam = 0.5, float offsetParam = 0.5, float phaseParam = 0.0)
    {
        // int phase = (int)((phaseParam * 5.999) + 1);
        int phase = (int)phaseParam;
        double balance = ((balanceParam * 2.0) - 1.0) / 2.0;
        double gainL = 0.5 - balance;
        double gainR = 0.5 + balance;
        double range = 30.0;
        if (phase == 3)
            range = 700.0;
        if (phase == 4)
            range = 700.0;
        double offset = pow((offsetParam * 2.0) - 1.0, 5) * range;
        if (phase > 4)
            offset = 0.0;
        if (phase > 5) {
            gainL = 0.5;
            gainR = 0.5;
        }
        int near = (int)floor(fabs(offset));
        double farLevel = fabs(offset) - near;
        int far = near + 1;
        double nearLevel = 1.0 - farLevel;

        if (phase == 2)
            inputSampleL = -inputSampleL;
        if (phase == 4)
            inputSampleL = -inputSampleL;

        inputSampleL *= gainL;
        inputSampleR *= gainR;

        if (count < 1 || count > 2048) {
            count = 2048;
        }

        if (offset > 0) {
            p[count + 2048] = p[count] = inputSampleL;
            inputSampleL = p[count + near] * nearLevel;
            inputSampleL += p[count + far] * farLevel;

            //consider adding third sample just to bring out superhighs subtly, like old interpolation hacks
            //or third and fifth samples, ditto
        }

        if (offset < 0) {
            p[count + 2048] = p[count] = inputSampleR;
            inputSampleR = p[count + near] * nearLevel;
            inputSampleR += p[count + far] * farLevel;
        }

        count -= 1;

        //the output is totally mono
        return inputSampleL + inputSampleR;
    }
}; /* end Golem */

/* #golembcn
======================================================================================== */
struct GolemBCN {

    double p[4099];
    bool flip;
    int count;

    GolemBCN()
    {
        for (int i = 0; i < 4098; i++) {
            p[i] = 0.0;
        }
        flip = true;
        count = 0;
    }

    long double process(long double inputSampleL, long double inputSampleR, float balanceParam = 0.f, float offsetParam = 0.f, float phaseParam = 0.f, int offsetScaling = 0)
    {
        int phase = (int)phaseParam;
        double balance = balanceParam * 0.5;
        double gainL = 0.5 - balance;
        double gainR = 0.5 + balance;
        double range = 30.0;
        if (phase == 3 || phase == 4) {
            range = 700.0;
        }

        double offset = 0.0;
        if (offsetScaling == 0) {
            offset = offsetParam * range; // lin
        } else {
            offset = pow(offsetParam, 3) * range; // exp
        }

        int near = (int)floor(fabs(offset));
        double farLevel = fabs(offset) - near;
        int far = near + 1;
        double nearLevel = 1.0 - farLevel;

        if (phase == 1 || phase == 3) {
            inputSampleL = -inputSampleL;
        }
        if (phase == 2 || phase == 4) {
            inputSampleR = -inputSampleR;
        }

        inputSampleL *= gainL;
        inputSampleR *= gainR;

        if (count < 1 || count > 2048) {
            count = 2048;
        }

        if (offset > 0) {
            p[count + 2048] = p[count] = inputSampleL;
            inputSampleL = p[count + near] * nearLevel;
            inputSampleL += p[count + far] * farLevel;
        }

        if (offset < 0) {
            p[count + 2048] = p[count] = inputSampleR;
            inputSampleR = p[count + near] * nearLevel;
            inputSampleR += p[count + far] * farLevel;
        }

        count -= 1;

        // the output is totally mono
        return inputSampleL + inputSampleR;
    }
}; /* end GolemBCN */

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

/* #slew3
======================================================================================== */
struct Slew3 {

    double lastSampleA;
    double lastSampleB;
    double lastSampleC;

    Slew3()
    {
        lastSampleA = lastSampleB = lastSampleC = 0.0;
    }

    long double process(long double inputSample, float clampParam = 0.0, double overallscale = 1.0)
    {
        double threshold = pow((1 - clampParam), 4) / overallscale;

        // regular slew clamping added
        double clamp = (lastSampleB - lastSampleC) * 0.381966011250105;
        clamp -= (lastSampleA - lastSampleB) * 0.6180339887498948482045;
        clamp += inputSample - lastSampleA;

        // now our output relates off lastSampleB
        lastSampleC = lastSampleB;
        lastSampleB = lastSampleA;
        lastSampleA = inputSample;

        if (clamp > threshold)
            inputSample = lastSampleB + threshold;
        if (-clamp > threshold)
            inputSample = lastSampleB - threshold;

        // split the difference between raw and smoothed for buffer
        lastSampleA = (lastSampleA * 0.381966011250105) + (inputSample * 0.6180339887498948482045);

        return inputSample;
    }
}; /* end Slew3 */

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

/* #tape
======================================================================================== */
struct Tape {

    double iirMidRollerA;
    double iirMidRollerB;
    double iirHeadBumpA;
    double iirHeadBumpB;

    long double biquadA[9];
    long double biquadB[9];
    long double biquadC[9];
    long double biquadD[9];

    bool flip;

    long double lastSample;

    double inputgain;
    double bumpgain;
    double headBumpFreq;
    double rollAmount;
    double softness = 0.618033988749894848204586;

    float lastSlamParam;
    float lastBumpParam;

    Tape()
    {
        iirMidRollerA = 0.0;
        iirMidRollerB = 0.0;
        iirHeadBumpA = 0.0;
        iirHeadBumpB = 0.0;

        for (int i = 0; i < 9; i++) {
            biquadA[i] = 0.0;
            biquadB[i] = 0.0;
            biquadC[i] = 0.0;
            biquadD[i] = 0.0;
        }

        flip = false;

        lastSample = 0.0;

        inputgain = 0.0;
        bumpgain = 0.0;

        lastSlamParam = 0.f;
        lastBumpParam = 0.f;

        onSampleRateChange();
    }

    void onSampleRateChange(double overallscale = 1.0)
    {
        headBumpFreq = 0.12 / overallscale;
        rollAmount = (1.0 - softness) / overallscale;

        biquadA[0] = biquadB[0] = 0.0072 / overallscale;
        biquadA[1] = biquadB[1] = 0.0009;
        double K = tan(M_PI * biquadB[0]);
        double norm = 1.0 / (1.0 + K / biquadB[1] + K * K);
        biquadA[2] = biquadB[2] = K / biquadB[1] * norm;
        biquadA[4] = biquadB[4] = -biquadB[2];
        biquadA[5] = biquadB[5] = 2.0 * (K * K - 1.0) * norm;
        biquadA[6] = biquadB[6] = (1.0 - K / biquadB[1] + K * K) * norm;

        biquadC[0] = biquadD[0] = 0.032 / overallscale;
        biquadC[1] = biquadD[1] = 0.0007;
        K = tan(M_PI * biquadD[0]);
        norm = 1.0 / (1.0 + K / biquadD[1] + K * K);
        biquadC[2] = biquadD[2] = K / biquadD[1] * norm;
        biquadC[4] = biquadD[4] = -biquadD[2];
        biquadC[5] = biquadD[5] = 2.0 * (K * K - 1.0) * norm;
        biquadC[6] = biquadD[6] = (1.0 - K / biquadD[1] + K * K) * norm;
    }

    long double process(long double inputSample, float slamParam = 0.5f, float bumpParam = 0.5f, double overallscale = 1.0)
    {
        if (slamParam != lastSlamParam) {
            inputgain = pow(10.0, ((slamParam - 0.5) * 24.0) / 20.0);
            lastSlamParam = slamParam;
        }

        if (bumpParam != lastBumpParam) {
            bumpgain = bumpParam * 0.1;
            lastBumpParam = bumpParam;
        }

        long double drySample = inputSample;

        long double highsSample = 0.0;
        long double nonHighsSample = 0.0;
        long double tempSample;

        if (flip) {
            iirMidRollerA = (iirMidRollerA * (1.0 - rollAmount)) + (inputSample * rollAmount);
            highsSample = inputSample - iirMidRollerA;
            nonHighsSample = iirMidRollerA;

            iirHeadBumpA += (inputSample * 0.05);
            iirHeadBumpA -= (iirHeadBumpA * iirHeadBumpA * iirHeadBumpA * headBumpFreq);
            iirHeadBumpA = sin(iirHeadBumpA);
            // interleaved biquad
            tempSample = (iirHeadBumpA * biquadA[2]) + biquadA[7];
            biquadA[7] = (iirHeadBumpA * biquadA[3]) - (tempSample * biquadA[5]) + biquadA[8];
            biquadA[8] = (iirHeadBumpA * biquadA[4]) - (tempSample * biquadA[6]);
            iirHeadBumpA = tempSample;
            if (iirHeadBumpA > 1.0)
                iirHeadBumpA = 1.0;
            if (iirHeadBumpA < -1.0)
                iirHeadBumpA = -1.0;
            iirHeadBumpA = asin(iirHeadBumpA);

            inputSample = sin(inputSample);
            // interleaved biquad
            tempSample = (inputSample * biquadC[2]) + biquadC[7];
            biquadC[7] = (inputSample * biquadC[3]) - (tempSample * biquadC[5]) + biquadC[8];
            biquadC[8] = (inputSample * biquadC[4]) - (tempSample * biquadC[6]);
            inputSample = tempSample;
            if (inputSample > 1.0)
                inputSample = 1.0;
            if (inputSample < -1.0)
                inputSample = -1.0;
            inputSample = asin(inputSample);
        } else {
            iirMidRollerB = (iirMidRollerB * (1.0 - rollAmount)) + (inputSample * rollAmount);
            highsSample = inputSample - iirMidRollerB;
            nonHighsSample = iirMidRollerB;

            iirHeadBumpB += (inputSample * 0.05);
            iirHeadBumpB -= (iirHeadBumpB * iirHeadBumpB * iirHeadBumpB * headBumpFreq);
            iirHeadBumpB = sin(iirHeadBumpB);
            // interleaved biquad
            tempSample = (iirHeadBumpB * biquadB[2]) + biquadB[7];
            biquadB[7] = (iirHeadBumpB * biquadB[3]) - (tempSample * biquadB[5]) + biquadB[8];
            biquadB[8] = (iirHeadBumpB * biquadB[4]) - (tempSample * biquadB[6]);
            iirHeadBumpB = tempSample;
            if (iirHeadBumpB > 1.0)
                iirHeadBumpB = 1.0;
            if (iirHeadBumpB < -1.0)
                iirHeadBumpB = -1.0;
            iirHeadBumpB = asin(iirHeadBumpB);

            inputSample = sin(inputSample);
            // interleaved biquad
            tempSample = (inputSample * biquadD[2]) + biquadD[7];
            biquadD[7] = (inputSample * biquadD[3]) - (tempSample * biquadD[5]) + biquadD[8];
            biquadD[8] = (inputSample * biquadD[4]) - (tempSample * biquadD[6]);
            inputSample = tempSample;
            if (inputSample > 1.0)
                inputSample = 1.0;
            if (inputSample < -1.0)
                inputSample = -1.0;
            inputSample = asin(inputSample);
        }
        flip = !flip;

        // set up UnBox
        long double groundSampleL = drySample - inputSample;

        // gain boost inside UnBox: do not boost fringe audio
        if (inputgain != 1.0) {
            inputSample *= inputgain;
        }

        // apply Soften depending on polarity
        long double applySoften = fabs(highsSample) * 1.57079633;
        if (applySoften > 1.57079633)
            applySoften = 1.57079633;
        applySoften = 1 - cos(applySoften);
        if (highsSample > 0)
            inputSample -= applySoften;
        if (highsSample < 0)
            inputSample += applySoften;

        //clip to 1.2533141373155 to reach maximum output
        if (inputSample > 1.2533141373155)
            inputSample = 1.2533141373155;
        if (inputSample < -1.2533141373155)
            inputSample = -1.2533141373155;

        // Spiral, for cleanest most optimal tape effect
        inputSample = sin(inputSample * fabs(inputSample)) / ((inputSample == 0.0) ? 1 : fabs(inputSample));

        // restrain resonant quality of head bump algorithm
        double suppress = (1.0 - fabs(inputSample)) * 0.00013;
        if (iirHeadBumpA > suppress)
            iirHeadBumpA -= suppress;
        if (iirHeadBumpA < -suppress)
            iirHeadBumpA += suppress;
        if (iirHeadBumpB > suppress)
            iirHeadBumpB -= suppress;
        if (iirHeadBumpB < -suppress)
            iirHeadBumpB += suppress;

        // apply UnBox processing
        inputSample += groundSampleL;

        // apply head bump
        inputSample += ((iirHeadBumpA + iirHeadBumpB) * bumpgain);

        // ADClip
        if (lastSample >= 0.99) {
            if (inputSample < 0.99)
                lastSample = ((0.99 * softness) + (inputSample * (1.0 - softness)));
            else
                lastSample = 0.99;
        }
        if (lastSample <= -0.99) {
            if (inputSample > -0.99)
                lastSample = ((-0.99 * softness) + (inputSample * (1.0 - softness)));
            else
                lastSample = -0.99;
        }
        if (inputSample > 0.99) {
            if (lastSample < 0.99)
                inputSample = ((0.99 * softness) + (lastSample * (1.0 - softness)));
            else
                inputSample = 0.99;
        }
        if (inputSample < -0.99) {
            if (lastSample > -0.99)
                inputSample = ((-0.99 * softness) + (lastSample * (1.0 - softness)));
            else
                inputSample = -0.99;
        }
        lastSample = inputSample;

        // final iron bar
        if (inputSample > 0.99)
            inputSample = 0.99;
        if (inputSample < -0.99)
            inputSample = -0.99;

        return inputSample;
    }
}; /* end Tape */

} // namespace rwlib

#endif // RWLIB_H