#include "plugin.hpp"

struct Monitoring : Module {
    enum ParamIds {
        MODE_PARAM,
        CANS_PARAM,
        DITHER_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        IN_L_INPUT,
        IN_R_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_L_OUTPUT,
        OUT_R_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        DITHER_16_LIGHT,
        DITHER_24_LIGHT,
        NUM_LIGHTS
    };

    const double gainCut = 0.03125;
    const double gainBoost = 32.0;

    bool isDither24;

    long double bynL[13], bynR[13];
    long double noiseShapingL, noiseShapingR;
    //NJAD
    double aL[1503], bL[1503], cL[1503], dL[1503];
    double aR[1503], bR[1503], cR[1503], dR[1503];
    int ax, bx, cx, dx;
    //PeaksOnly
    double lastSampleL, lastSampleR;
    //SlewOnly
    double iirSampleAL, iirSampleBL, iirSampleCL, iirSampleDL, iirSampleEL, iirSampleFL, iirSampleGL;
    double iirSampleHL, iirSampleIL, iirSampleJL, iirSampleKL, iirSampleLL, iirSampleML, iirSampleNL, iirSampleOL, iirSamplePL;
    double iirSampleQL, iirSampleRL, iirSampleSL;
    double iirSampleTL, iirSampleUL, iirSampleVL;
    double iirSampleWL, iirSampleXL, iirSampleYL, iirSampleZL;

    double iirSampleAR, iirSampleBR, iirSampleCR, iirSampleDR, iirSampleER, iirSampleFR, iirSampleGR;
    double iirSampleHR, iirSampleIR, iirSampleJR, iirSampleKR, iirSampleLR, iirSampleMR, iirSampleNR, iirSampleOR, iirSamplePR;
    double iirSampleQR, iirSampleRR, iirSampleSR;
    double iirSampleTR, iirSampleUR, iirSampleVR;
    double iirSampleWR, iirSampleXR, iirSampleYR, iirSampleZR; // o/`
    //SubsOnly
    long double biquadL[11];
    long double biquadR[11];
    //Bandpasses

    uint32_t fpd;
    //default stuff

    float A;

    Monitoring()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(MODE_PARAM, 0.f, 6.0, 0.f, "Mode");
        configParam(CANS_PARAM, 1.f, 4.0, 0.f, "Cans");
        configParam(DITHER_PARAM, 0.f, 1.f, 1.f, "Dither");

        isDither24 = true;

        bynL[0] = 1000.0;
        bynL[1] = 301.0;
        bynL[2] = 176.0;
        bynL[3] = 125.0;
        bynL[4] = 97.0;
        bynL[5] = 79.0;
        bynL[6] = 67.0;
        bynL[7] = 58.0;
        bynL[8] = 51.0;
        bynL[9] = 46.0;
        bynL[10] = 1000.0;
        noiseShapingL = 0.0;
        bynR[0] = 1000.0;
        bynR[1] = 301.0;
        bynR[2] = 176.0;
        bynR[3] = 125.0;
        bynR[4] = 97.0;
        bynR[5] = 79.0;
        bynR[6] = 67.0;
        bynR[7] = 58.0;
        bynR[8] = 51.0;
        bynR[9] = 46.0;
        bynR[10] = 1000.0;
        noiseShapingR = 0.0;
        //end NJAD
        for (int count = 0; count < 1502; count++) {
            aL[count] = 0.0;
            bL[count] = 0.0;
            cL[count] = 0.0;
            dL[count] = 0.0;
            aR[count] = 0.0;
            bR[count] = 0.0;
            cR[count] = 0.0;
            dR[count] = 0.0;
        }
        ax = 1;
        bx = 1;
        cx = 1;
        dx = 1;
        //PeaksOnly
        lastSampleL = 0.0;
        lastSampleR = 0.0;
        //SlewOnly
        iirSampleAL = 0.0;
        iirSampleBL = 0.0;
        iirSampleCL = 0.0;
        iirSampleDL = 0.0;
        iirSampleEL = 0.0;
        iirSampleFL = 0.0;
        iirSampleGL = 0.0;
        iirSampleHL = 0.0;
        iirSampleIL = 0.0;
        iirSampleJL = 0.0;
        iirSampleKL = 0.0;
        iirSampleLL = 0.0;
        iirSampleML = 0.0;
        iirSampleNL = 0.0;
        iirSampleOL = 0.0;
        iirSamplePL = 0.0;
        iirSampleQL = 0.0;
        iirSampleRL = 0.0;
        iirSampleSL = 0.0;
        iirSampleTL = 0.0;
        iirSampleUL = 0.0;
        iirSampleVL = 0.0;
        iirSampleWL = 0.0;
        iirSampleXL = 0.0;
        iirSampleYL = 0.0;
        iirSampleZL = 0.0;

        iirSampleAR = 0.0;
        iirSampleBR = 0.0;
        iirSampleCR = 0.0;
        iirSampleDR = 0.0;
        iirSampleER = 0.0;
        iirSampleFR = 0.0;
        iirSampleGR = 0.0;
        iirSampleHR = 0.0;
        iirSampleIR = 0.0;
        iirSampleJR = 0.0;
        iirSampleKR = 0.0;
        iirSampleLR = 0.0;
        iirSampleMR = 0.0;
        iirSampleNR = 0.0;
        iirSampleOR = 0.0;
        iirSamplePR = 0.0;
        iirSampleQR = 0.0;
        iirSampleRR = 0.0;
        iirSampleSR = 0.0;
        iirSampleTR = 0.0;
        iirSampleUR = 0.0;
        iirSampleVR = 0.0;
        iirSampleWR = 0.0;
        iirSampleXR = 0.0;
        iirSampleYR = 0.0;
        iirSampleZR = 0.0; // o/`
        //SubsOnly
        for (int x = 0; x < 11; x++) {
            biquadL[x] = 0.0;
            biquadR[x] = 0.0;
        }
        //Bandpasses
        A = 0.0;
        fpd = 17;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void process(const ProcessArgs& args) override
    {
        // dither light
        isDither24 = params[DITHER_PARAM].getValue() ? true : false;
        lights[DITHER_24_LIGHT].setBrightness(isDither24);
        lights[DITHER_16_LIGHT].setBrightness(!isDither24);

        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {

            // params
            A = params[MODE_PARAM].getValue();

            double overallscale = 1.0;
            overallscale /= 44100.0;
            overallscale *= args.sampleRate;

            // int processing = (int)A * 16.999;
            int processing = (int)A + 3;
            int am = (int)149.0 * overallscale;
            int bm = (int)179.0 * overallscale;
            int cm = (int)191.0 * overallscale;
            int dm = (int)223.0 * overallscale; //these are 'good' primes, spacing out the allpasses
            int allpasstemp;
            //for PeaksOnly
            biquadL[0] = 0.0375 / overallscale;
            biquadL[1] = 0.1575; //define as AURAT, MONORAT, MONOLAT unless overridden
            if (processing == 7) {
                biquadL[0] = 0.0385 / overallscale;
                biquadL[1] = 0.0825;
            }
            if (processing == 11) {
                biquadL[0] = 0.1245 / overallscale;
                biquadL[1] = 0.46;
            }
            double K = tan(M_PI * biquadL[0]);
            double norm = 1.0 / (1.0 + K / biquadL[1] + K * K);
            biquadL[2] = K / biquadL[1] * norm;
            biquadL[4] = -biquadL[2]; //for bandpass, ignore [3] = 0.0
            biquadL[5] = 2.0 * (K * K - 1.0) * norm;
            biquadL[6] = (1.0 - K / biquadL[1] + K * K) * norm;
            //for Bandpasses
            biquadR[0] = 0.0375 / overallscale;
            biquadR[1] = 0.1575; //define as AURAT, MONORAT, MONOLAT unless overridden
            if (processing == 7) {
                biquadR[0] = 0.0385 / overallscale;
                biquadR[1] = 0.0825;
            }
            if (processing == 11) {
                biquadR[0] = 0.1245 / overallscale;
                biquadR[1] = 0.46;
            }
            K = tan(M_PI * biquadR[0]);
            norm = 1.0 / (1.0 + K / biquadR[1] + K * K);
            biquadR[2] = K / biquadR[1] * norm;
            biquadR[4] = -biquadR[2]; //for bandpass, ignore [3] = 0.0
            biquadR[5] = 2.0 * (K * K - 1.0) * norm;
            biquadR[6] = (1.0 - K / biquadR[1] + K * K) * norm;
            //for Bandpasses

            // get input
            long double inputSampleL = inputs[IN_L_INPUT].getVoltage();
            long double inputSampleR = inputs[IN_R_INPUT].getVoltage();

            // pad gain
            inputSampleL *= gainCut;
            inputSampleR *= gainCut;

            if (fabs(inputSampleL) < 1.18e-37)
                inputSampleL = fpd * 1.18e-37;
            if (fabs(inputSampleR) < 1.18e-37)
                inputSampleR = fpd * 1.18e-37;

            switch (processing) {
            case 0:
            case 1:
                break;
            case 2:
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
                //amplitude aspect
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
                inputSampleL += (aL[ax]);
                inputSampleR += (aR[ax]);
                //a single Midiverb-style allpass

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
                //amplitude aspect

                allpasstemp = bx - 1;
                if (allpasstemp < 0 || allpasstemp > bm)
                    allpasstemp = bm;
                inputSampleL -= bL[allpasstemp] * 0.5;
                bL[bx] = inputSampleL;
                inputSampleL *= 0.5;
                inputSampleR -= bR[allpasstemp] * 0.5;
                bR[bx] = inputSampleR;
                inputSampleR *= 0.5;
                bx--;
                if (bx < 0 || bx > bm) {
                    bx = bm;
                }
                inputSampleL += (bL[bx]);
                inputSampleR += (bR[bx]);
                //a single Midiverb-style allpass

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
                //amplitude aspect

                allpasstemp = cx - 1;
                if (allpasstemp < 0 || allpasstemp > cm)
                    allpasstemp = cm;
                inputSampleL -= cL[allpasstemp] * 0.5;
                cL[cx] = inputSampleL;
                inputSampleL *= 0.5;
                inputSampleR -= cR[allpasstemp] * 0.5;
                cR[cx] = inputSampleR;
                inputSampleR *= 0.5;
                cx--;
                if (cx < 0 || cx > cm) {
                    cx = cm;
                }
                inputSampleL += (cL[cx]);
                inputSampleR += (cR[cx]);
                //a single Midiverb-style allpass

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
                //amplitude aspect

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
                inputSampleL += (dL[dx]);
                inputSampleR += (dR[dx]);
                //a single Midiverb-style allpass

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
                //amplitude aspect

                inputSampleL *= 0.63679;
                inputSampleR *= 0.63679; //scale it to 0dB output at full blast
                //PeaksOnly
                break;
            case 3:
                double trim;
                trim = 2.302585092994045684017991; //natural logarithm of 10
                long double slewSample;
                slewSample = (inputSampleL - lastSampleL) * trim;
                lastSampleL = inputSampleL;
                if (slewSample > 1.0)
                    slewSample = 1.0;
                if (slewSample < -1.0)
                    slewSample = -1.0;
                inputSampleL = slewSample;
                slewSample = (inputSampleR - lastSampleR) * trim;
                lastSampleR = inputSampleR;
                if (slewSample > 1.0)
                    slewSample = 1.0;
                if (slewSample < -1.0)
                    slewSample = -1.0;
                inputSampleR = slewSample;
                //SlewOnly
                break;
            case 4:
                double iirAmount;
                iirAmount = (2250 / 44100.0) / overallscale;
                double gain;
                gain = 1.42;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;

                iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleAL;
                iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleAR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleBL;
                iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleBR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleCL = (iirSampleCL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleCL;
                iirSampleCR = (iirSampleCR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleCR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleDL = (iirSampleDL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleDL;
                iirSampleDR = (iirSampleDR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleDR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleEL = (iirSampleEL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleEL;
                iirSampleER = (iirSampleER * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleER;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleFL = (iirSampleFL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleFL;
                iirSampleFR = (iirSampleFR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleFR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleGL = (iirSampleGL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleGL;
                iirSampleGR = (iirSampleGR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleGR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleHL = (iirSampleHL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleHL;
                iirSampleHR = (iirSampleHR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleHR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleIL = (iirSampleIL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleIL;
                iirSampleIR = (iirSampleIR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleIR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleJL = (iirSampleJL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleJL;
                iirSampleJR = (iirSampleJR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleJR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleKL = (iirSampleKL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleKL;
                iirSampleKR = (iirSampleKR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleKR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleLL = (iirSampleLL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleLL;
                iirSampleLR = (iirSampleLR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleLR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleML = (iirSampleML * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleML;
                iirSampleMR = (iirSampleMR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleMR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleNL = (iirSampleNL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleNL;
                iirSampleNR = (iirSampleNR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleNR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleOL = (iirSampleOL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleOL;
                iirSampleOR = (iirSampleOR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleOR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSamplePL = (iirSamplePL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSamplePL;
                iirSamplePR = (iirSamplePR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSamplePR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleQL = (iirSampleQL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleQL;
                iirSampleQR = (iirSampleQR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleQR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleRL = (iirSampleRL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleRL;
                iirSampleRR = (iirSampleRR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleRR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleSL = (iirSampleSL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleSL;
                iirSampleSR = (iirSampleSR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleSR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleTL = (iirSampleTL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleTL;
                iirSampleTR = (iirSampleTR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleTR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleUL = (iirSampleUL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleUL;
                iirSampleUR = (iirSampleUR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleUR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleVL = (iirSampleVL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleVL;
                iirSampleVR = (iirSampleVR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleVR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleWL = (iirSampleWL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleWL;
                iirSampleWR = (iirSampleWR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleWR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleXL = (iirSampleXL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleXL;
                iirSampleXR = (iirSampleXR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleXR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleYL = (iirSampleYL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleYL;
                iirSampleYR = (iirSampleYR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleYR;
                inputSampleL *= gain;
                inputSampleR *= gain;
                gain = ((gain - 1) * 0.75) + 1;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;

                iirSampleZL = (iirSampleZL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
                inputSampleL = iirSampleZL;
                iirSampleZR = (iirSampleZR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
                inputSampleR = iirSampleZR;
                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;
                //SubsOnly
                break;
            case 5:
            case 6:
                long double mid;
                mid = inputSampleL + inputSampleR;
                long double side;
                side = inputSampleL - inputSampleR;
                if (processing < 6)
                    side = 0.0;
                else
                    mid = 0.0; //mono monitoring, or side-only monitoring
                inputSampleL = (mid + side) / 2.0;
                inputSampleR = (mid - side) / 2.0;
                break;
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
                //Bandpass: changes in EQ are up in the variable defining, not here
                //7 Vinyl, 8 9 10 Aurat, 11 Phone
                if (processing == 9) {
                    inputSampleR = (inputSampleL + inputSampleR) * 0.5;
                    inputSampleL = 0.0;
                }
                if (processing == 10) {
                    inputSampleL = (inputSampleL + inputSampleR) * 0.5;
                    inputSampleR = 0.0;
                }
                if (processing == 11) {
                    long double M;
                    M = (inputSampleL + inputSampleR) * 0.5;
                    inputSampleL = M;
                    inputSampleR = M;
                }

                inputSampleL = sin(inputSampleL);
                inputSampleR = sin(inputSampleR);
                //encode Console5: good cleanness

                long double tempSampleL;
                tempSampleL = (inputSampleL * biquadL[2]) + biquadL[7];
                biquadL[7] = (-tempSampleL * biquadL[5]) + biquadL[8];
                biquadL[8] = (inputSampleL * biquadL[4]) - (tempSampleL * biquadL[6]);
                inputSampleL = tempSampleL; //like mono AU, 7 and 8 store L channel

                long double tempSampleR;
                tempSampleR = (inputSampleR * biquadR[2]) + biquadR[7];
                biquadR[7] = (-tempSampleR * biquadR[5]) + biquadR[8];
                biquadR[8] = (inputSampleR * biquadR[4]) - (tempSampleR * biquadR[6]);
                inputSampleR = tempSampleR; // we are using the mono configuration

                if (inputSampleL > 1.0)
                    inputSampleL = 1.0;
                if (inputSampleL < -1.0)
                    inputSampleL = -1.0;
                if (inputSampleR > 1.0)
                    inputSampleR = 1.0;
                if (inputSampleR < -1.0)
                    inputSampleR = -1.0;
                //without this, you can get a NaN condition where it spits out DC offset at full blast!
                inputSampleL = asin(inputSampleL);
                inputSampleR = asin(inputSampleR);
                //amplitude aspect
                break;
            case 12:
            case 13:
            case 14:
            case 15:
                if (processing == 12) {
                    inputSampleL *= 0.855;
                    inputSampleR *= 0.855;
                }
                if (processing == 13) {
                    inputSampleL *= 0.748;
                    inputSampleR *= 0.748;
                }
                if (processing == 14) {
                    inputSampleL *= 0.713;
                    inputSampleR *= 0.713;
                }
                if (processing == 15) {
                    inputSampleL *= 0.680;
                    inputSampleR *= 0.680;
                }
                //we do a volume compensation immediately to gain stage stuff cleanly
                inputSampleL = sin(inputSampleL);
                inputSampleR = sin(inputSampleR);
                long double drySampleL;
                drySampleL = inputSampleL;
                long double drySampleR;
                drySampleR = inputSampleR; //everything runs 'inside' Console
                long double bass;
                bass = (processing * processing * 0.00001) / overallscale;
                //we are using the iir filters from out of SubsOnly

                mid = inputSampleL + inputSampleR;
                side = inputSampleL - inputSampleR;
                iirSampleAL = (iirSampleAL * (1.0 - (bass * 0.618))) + (side * bass * 0.618);
                side = side - iirSampleAL;
                inputSampleL = (mid + side) / 2.0;
                inputSampleR = (mid - side) / 2.0;
                //bass narrowing filter

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
                //a darkened Midiverb-style allpass

                if (processing == 12) {
                    inputSampleL *= 0.125;
                    inputSampleR *= 0.125;
                }
                if (processing == 13) {
                    inputSampleL *= 0.25;
                    inputSampleR *= 0.25;
                }
                if (processing == 14) {
                    inputSampleL *= 0.30;
                    inputSampleR *= 0.30;
                }
                if (processing == 15) {
                    inputSampleL *= 0.35;
                    inputSampleR *= 0.35;
                }
                //Cans A suppresses the crossfeed more, Cans B makes it louder

                drySampleL += inputSampleR;
                drySampleR += inputSampleL; //the crossfeed

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
                //a darkened Midiverb-style allpass, which is stretching the previous one even more

                inputSampleL *= 0.25;
                inputSampleR *= 0.25;
                //for all versions of Cans the second level of bloom is this far down
                //and, remains on the opposite speaker rather than crossing again to the original side

                drySampleL += inputSampleR;
                drySampleR += inputSampleL; //add the crossfeed and very faint extra verbyness

                inputSampleL = drySampleL;
                inputSampleR = drySampleR; //and output our can-opened headphone feed

                mid = inputSampleL + inputSampleR;
                side = inputSampleL - inputSampleR;
                iirSampleAR = (iirSampleAR * (1.0 - bass)) + (side * bass);
                side = side - iirSampleAR;
                inputSampleL = (mid + side) / 2.0;
                inputSampleR = (mid - side) / 2.0;
                //bass narrowing filter

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
                //ConsoleBuss processing
                break;
            case 16:
                long double inputSample = (inputSampleL + inputSampleR) * 0.5;
                inputSampleL = -inputSample;
                inputSampleR = inputSample;
                break;
            }

            //begin Not Just Another Dither
            if (processing == 1) {
                inputSampleL = inputSampleL * 32768.0; //or 16 bit option
                inputSampleR = inputSampleR * 32768.0; //or 16 bit option
            } else {
                inputSampleL = inputSampleL * 8388608.0; //for literally everything else
                inputSampleR = inputSampleR * 8388608.0; //we will apply the 24 bit NJAD
            } //on the not unreasonable assumption that we are very likely playing back on 24 bit DAC
            //if we're not, then all we did was apply a Benford Realness function at 24 bits down.

            bool cutbinsL;
            cutbinsL = false;
            bool cutbinsR;
            cutbinsR = false;
            long double drySampleL;
            drySampleL = inputSampleL;
            long double drySampleR;
            drySampleR = inputSampleR;
            inputSampleL -= noiseShapingL;
            inputSampleR -= noiseShapingR;
            //NJAD L
            long double benfordize;
            benfordize = floor(inputSampleL);
            while (benfordize >= 1.0)
                benfordize /= 10;
            while (benfordize < 1.0 && benfordize > 0.0000001)
                benfordize *= 10;
            int hotbinA;
            hotbinA = floor(benfordize);
            //hotbin becomes the Benford bin value for this number floored
            long double totalA;
            totalA = 0;
            if ((hotbinA > 0) && (hotbinA < 10)) {
                bynL[hotbinA] += 1;
                if (bynL[hotbinA] > 982)
                    cutbinsL = true;
                totalA += (301 - bynL[1]);
                totalA += (176 - bynL[2]);
                totalA += (125 - bynL[3]);
                totalA += (97 - bynL[4]);
                totalA += (79 - bynL[5]);
                totalA += (67 - bynL[6]);
                totalA += (58 - bynL[7]);
                totalA += (51 - bynL[8]);
                totalA += (46 - bynL[9]);
                bynL[hotbinA] -= 1;
            } else
                hotbinA = 10;
            //produce total number- smaller is closer to Benford real
            benfordize = ceil(inputSampleL);
            while (benfordize >= 1.0)
                benfordize /= 10;
            while (benfordize < 1.0 && benfordize > 0.0000001)
                benfordize *= 10;
            int hotbinB;
            hotbinB = floor(benfordize);
            //hotbin becomes the Benford bin value for this number ceiled
            long double totalB;
            totalB = 0;
            if ((hotbinB > 0) && (hotbinB < 10)) {
                bynL[hotbinB] += 1;
                if (bynL[hotbinB] > 982)
                    cutbinsL = true;
                totalB += (301 - bynL[1]);
                totalB += (176 - bynL[2]);
                totalB += (125 - bynL[3]);
                totalB += (97 - bynL[4]);
                totalB += (79 - bynL[5]);
                totalB += (67 - bynL[6]);
                totalB += (58 - bynL[7]);
                totalB += (51 - bynL[8]);
                totalB += (46 - bynL[9]);
                bynL[hotbinB] -= 1;
            } else
                hotbinB = 10;
            //produce total number- smaller is closer to Benford real
            long double outputSample;
            if (totalA < totalB) {
                bynL[hotbinA] += 1;
                outputSample = floor(inputSampleL);
            } else {
                bynL[hotbinB] += 1;
                outputSample = floor(inputSampleL + 1);
            }
            //assign the relevant one to the delay line
            //and floor/ceil signal accordingly
            if (cutbinsL) {
                bynL[1] *= 0.99;
                bynL[2] *= 0.99;
                bynL[3] *= 0.99;
                bynL[4] *= 0.99;
                bynL[5] *= 0.99;
                bynL[6] *= 0.99;
                bynL[7] *= 0.99;
                bynL[8] *= 0.99;
                bynL[9] *= 0.99;
                bynL[10] *= 0.99;
            }
            noiseShapingL += outputSample - drySampleL;
            if (noiseShapingL > fabs(inputSampleL))
                noiseShapingL = fabs(inputSampleL);
            if (noiseShapingL < -fabs(inputSampleL))
                noiseShapingL = -fabs(inputSampleL);
            if (processing == 1)
                inputSampleL = outputSample / 32768.0;
            else
                inputSampleL = outputSample / 8388608.0;
            if (inputSampleL > 1.0)
                inputSampleL = 1.0;
            if (inputSampleL < -1.0)
                inputSampleL = -1.0;
            //finished NJAD L

            //NJAD R
            benfordize = floor(inputSampleR);
            while (benfordize >= 1.0)
                benfordize /= 10;
            while (benfordize < 1.0 && benfordize > 0.0000001)
                benfordize *= 10;
            hotbinA = floor(benfordize);
            //hotbin becomes the Benford bin value for this number floored
            totalA = 0;
            if ((hotbinA > 0) && (hotbinA < 10)) {
                bynR[hotbinA] += 1;
                if (bynR[hotbinA] > 982)
                    cutbinsR = true;
                totalA += (301 - bynR[1]);
                totalA += (176 - bynR[2]);
                totalA += (125 - bynR[3]);
                totalA += (97 - bynR[4]);
                totalA += (79 - bynR[5]);
                totalA += (67 - bynR[6]);
                totalA += (58 - bynR[7]);
                totalA += (51 - bynR[8]);
                totalA += (46 - bynR[9]);
                bynR[hotbinA] -= 1;
            } else
                hotbinA = 10;
            //produce total number- smaller is closer to Benford real
            benfordize = ceil(inputSampleR);
            while (benfordize >= 1.0)
                benfordize /= 10;
            while (benfordize < 1.0 && benfordize > 0.0000001)
                benfordize *= 10;
            hotbinB = floor(benfordize);
            //hotbin becomes the Benford bin value for this number ceiled
            totalB = 0;
            if ((hotbinB > 0) && (hotbinB < 10)) {
                bynR[hotbinB] += 1;
                if (bynR[hotbinB] > 982)
                    cutbinsR = true;
                totalB += (301 - bynR[1]);
                totalB += (176 - bynR[2]);
                totalB += (125 - bynR[3]);
                totalB += (97 - bynR[4]);
                totalB += (79 - bynR[5]);
                totalB += (67 - bynR[6]);
                totalB += (58 - bynR[7]);
                totalB += (51 - bynR[8]);
                totalB += (46 - bynR[9]);
                bynR[hotbinB] -= 1;
            } else
                hotbinB = 10;
            //produce total number- smaller is closer to Benford real
            if (totalA < totalB) {
                bynR[hotbinA] += 1;
                outputSample = floor(inputSampleR);
            } else {
                bynR[hotbinB] += 1;
                outputSample = floor(inputSampleR + 1);
            }
            //assign the relevant one to the delay line
            //and floor/ceil signal accordingly
            if (cutbinsR) {
                bynR[1] *= 0.99;
                bynR[2] *= 0.99;
                bynR[3] *= 0.99;
                bynR[4] *= 0.99;
                bynR[5] *= 0.99;
                bynR[6] *= 0.99;
                bynR[7] *= 0.99;
                bynR[8] *= 0.99;
                bynR[9] *= 0.99;
                bynR[10] *= 0.99;
            }
            noiseShapingR += outputSample - drySampleR;
            if (noiseShapingR > fabs(inputSampleR))
                noiseShapingR = fabs(inputSampleR);
            if (noiseShapingR < -fabs(inputSampleR))
                noiseShapingR = -fabs(inputSampleR);
            if (processing == 1)
                inputSampleR = outputSample / 32768.0;
            else
                inputSampleR = outputSample / 8388608.0;
            if (inputSampleR > 1.0)
                inputSampleR = 1.0;
            if (inputSampleR < -1.0)
                inputSampleR = -1.0;
            //finished NJAD R

            //does not use 32 bit stereo floating point dither

            // bring gain back up
            inputSampleL *= gainBoost;
            inputSampleR *= gainBoost;

            // output
            outputs[OUT_L_OUTPUT].setVoltage(inputSampleL);
            outputs[OUT_R_OUTPUT].setVoltage(inputSampleR);
        }
    }
};

struct MonitoringWidget : ModuleWidget {
    MonitoringWidget(Monitoring* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/monitoring_dark.svg")));

        //screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwSwitchKnobMediumDark>(Vec(45.0, 85.0), module, Monitoring::MODE_PARAM));
        addParam(createParamCentered<RwSwitchKnobMediumDark>(Vec(45.0, 160.0), module, Monitoring::CANS_PARAM));

        // switch
        addParam(createParamCentered<RwCKSSRot>(Vec(45.0, 225.0), module, Monitoring::DITHER_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(15.0, 225.0), module, Monitoring::DITHER_16_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(75.0, 225.0), module, Monitoring::DITHER_24_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285.0), module, Monitoring::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285.0), module, Monitoring::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325.0), module, Monitoring::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Monitoring::OUT_R_OUTPUT));
    }
};

Model* modelMonitoring = createModel<Monitoring, MonitoringWidget>("monitoring");