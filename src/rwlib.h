#ifndef RWLIB_H
#define RWLIB_H

namespace rwlib {

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

    void process(float sampleRate, long double& inputSampleL, long double& inputSampleR)
    {
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

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
        long double drySampleL = inputSampleL;
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
        drySampleL += inputSampleR;
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
        drySampleL += inputSampleR;
        drySampleR += inputSampleL;

        //and output our can-opened headphone feed
        inputSampleL = drySampleL;
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

    long double process(long double inputSample, float sampleRate, bool highres = true)
    {
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

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
        // float derez = derezParam;
        // if (derez > 0.0)
        //     scaleFactor *= pow(1.0 - derez, 6);
        // if (scaleFactor < 0.0001)
        //     scaleFactor = 0.0001;
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

    long double process(long double inputSample, float sampleRate)
    {
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

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

    long double process(long double inputSample, float sampleRate)
    {
        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

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