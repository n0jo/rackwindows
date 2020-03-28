/***********************************************************************************************
Chorus
-------
VCV Rack module based on Chorus by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- stereo wtf? needs redesign, as Chorus is apparently mono
- ensemble switch: changes behaviour to ChorusEnsemble
- CV inputs for speed, range and dry/wet

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Chorus : Module {
    enum ParamIds {
        SPEED_PARAM,
        RANGE_PARAM,
        DRYWET_PARAM,
        ENSEMBLE_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        SPEED_CV_INPUT,
        RANGE_CV_INPUT,
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
        ENSEMBLE_LIGHT,
        NUM_LIGHTS
    };

    long double fpNShapeL;
    long double fpNShapeR;
    //default stuff
    const static int totalsamples = 16386;
    float dL[totalsamples];
    float dR[totalsamples];
    double sweep;
    int gcount;
    double airPrevL;
    double airEvenL;
    double airOddL;
    double airFactorL;
    double airPrevR;
    double airEvenR;
    double airOddR;
    double airFactorR;
    bool fpFlip;

    float A;
    float B;
    float C;

    bool isEnsemble;

    Chorus()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPEED_PARAM, 0.f, 1.f, 0.5f, "Speed");
        configParam(RANGE_PARAM, 0.f, 1.f, 0.f, "Range");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");
        configParam(ENSEMBLE_PARAM, 0.f, 1.f, 0.f, "Ensemble");

        for (int count = 0; count < totalsamples - 1; count++) {
            dL[count] = 0;
            dR[count] = 0;
        }
        sweep = 3.141592653589793238 / 2.0;
        gcount = 0;
        airPrevL = 0.0;
        airEvenL = 0.0;
        airOddL = 0.0;
        airFactorL = 0.0;
        airPrevR = 0.0;
        airEvenR = 0.0;
        airOddR = 0.0;
        airFactorR = 0.0;
        fpFlip = true;
        fpNShapeL = 0.0;
        fpNShapeR = 0.0;
        //this is reset: values being initialized only once. Startup values, whatever they are.

        isEnsemble = false;
    }

    void process(const ProcessArgs& args) override
    {
        // params
        A = params[SPEED_PARAM].getValue();
        A += inputs[SPEED_CV_INPUT].getVoltage() / 5;
        A = clamp(A, 0.01f, 0.99f);

        B = params[RANGE_PARAM].getValue();
        B += inputs[RANGE_CV_INPUT].getVoltage() / 5;
        B = clamp(B, 0.01f, 0.99f);

        C = params[DRYWET_PARAM].getValue();

        // ensemble light
        isEnsemble = params[ENSEMBLE_PARAM].getValue() ? true : false;
        lights[ENSEMBLE_LIGHT].setBrightness(isEnsemble);

        // input
        float in1 = inputs[IN_L_INPUT].getVoltage();
        float in2 = inputs[IN_R_INPUT].getVoltage();

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= args.sampleRate;

        double speed = 0.0;
        double range = 0.0;
        double start[4];
        int loopLimit = (int)(totalsamples * 0.499);

        if (isEnsemble) {
            speed = pow(A, 3) * 0.001;
            range = pow(B, 3) * loopLimit * 0.12;
            // start[4];

            //now we'll precalculate some stuff that needn't be in every sample
            start[0] = range;
            start[1] = range * 2;
            start[2] = range * 3;
            start[3] = range * 4;
        } else {
            speed = pow(A, 4) * 0.001;
            range = pow(B, 4) * loopLimit * 0.499;
        }

        int count;
        double wet = C;
        double modulation = range * wet;
        double dry = 1.0 - wet;
        double tupi = 3.141592653589793238 * 2.0;
        double offset;
        //this is a double buffer so we will be splitting it in two

        long double inputSampleL;
        long double inputSampleR;
        double drySampleL;
        double drySampleR;

        speed *= overallscale;

        inputSampleL = in1;
        inputSampleR = in2;

        if (inputSampleL < 1.2e-38 && -inputSampleL < 1.2e-38) {
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
            inputSampleL = applyresidue;
        }
        if (inputSampleR < 1.2e-38 && -inputSampleR < 1.2e-38) {
            static int noisesource = 0;
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
            inputSampleR = applyresidue;
            //this denormalization routine produces a white noise at -300 dB which the noise
            //shaping will interact with to produce a bipolar output, but the noise is actually
            //all positive. That should stop any variables from going denormal, and the routine
            //only kicks in if digital black is input. As a final touch, if you save to 24-bit
            //the silence will return to being digital black again.
        }
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        airFactorL = airPrevL - inputSampleL;
        if (fpFlip) {
            airEvenL += airFactorL;
            airOddL -= airFactorL;
            airFactorL = airEvenL;
        } else {
            airOddL += airFactorL;
            airEvenL -= airFactorL;
            airFactorL = airOddL;
        }
        airOddL = (airOddL - ((airOddL - airEvenL) / 256.0)) / 1.0001;
        airEvenL = (airEvenL - ((airEvenL - airOddL) / 256.0)) / 1.0001;
        airPrevL = inputSampleL;
        inputSampleL += (airFactorL * wet);
        //air, compensates for loss of highs in flanger's interpolation

        airFactorR = airPrevR - inputSampleR;
        if (fpFlip) {
            airEvenR += airFactorR;
            airOddR -= airFactorR;
            airFactorR = airEvenR;
        } else {
            airOddR += airFactorR;
            airEvenR -= airFactorR;
            airFactorR = airOddR;
        }
        airOddR = (airOddR - ((airOddR - airEvenR) / 256.0)) / 1.0001;
        airEvenR = (airEvenR - ((airEvenR - airOddR) / 256.0)) / 1.0001;
        airPrevR = inputSampleR;
        inputSampleR += (airFactorR * wet);
        //air, compensates for loss of highs in flanger's interpolation

        if (gcount < 1 || gcount > loopLimit) {
            gcount = loopLimit;
        }
        count = gcount;
        dL[count + loopLimit] = dL[count] = inputSampleL;
        dR[count + loopLimit] = dR[count] = inputSampleR;
        gcount--;
        //double buffer

        if (isEnsemble) {
            offset = start[0] + (modulation * sin(sweep));
            count = gcount + (int)floor(offset);

            inputSampleL = dL[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleL += dL[count + 1]; //we can assume always using this in one way or another?
            inputSampleL += (dL[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleL -= (((dL[count] - dL[count + 1]) - (dL[count + 1] - dL[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleR = dR[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleR += dR[count + 1]; //we can assume always using this in one way or another?
            inputSampleR += (dR[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleR -= (((dR[count] - dR[count + 1]) - (dR[count + 1] - dR[count + 2])) / 50); //interpolation hacks 'r us

            offset = start[1] + (modulation * sin(sweep + 1.0));
            count = gcount + (int)floor(offset);
            inputSampleL += dL[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleL += dL[count + 1]; //we can assume always using this in one way or another?
            inputSampleL += (dL[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleL -= (((dL[count] - dL[count + 1]) - (dL[count + 1] - dL[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleR += dR[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleR += dR[count + 1]; //we can assume always using this in one way or another?
            inputSampleR += (dR[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleR -= (((dR[count] - dR[count + 1]) - (dR[count + 1] - dR[count + 2])) / 50); //interpolation hacks 'r us

            offset = start[2] + (modulation * sin(sweep + 2.0));
            count = gcount + (int)floor(offset);
            inputSampleL += dL[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleL += dL[count + 1]; //we can assume always using this in one way or another?
            inputSampleL += (dL[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleL -= (((dL[count] - dL[count + 1]) - (dL[count + 1] - dL[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleR += dR[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleR += dR[count + 1]; //we can assume always using this in one way or another?
            inputSampleR += (dR[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleR -= (((dR[count] - dR[count + 1]) - (dR[count + 1] - dR[count + 2])) / 50); //interpolation hacks 'r us

            offset = start[3] + (modulation * sin(sweep + 3.0));
            count = gcount + (int)floor(offset);
            inputSampleL += dL[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleL += dL[count + 1]; //we can assume always using this in one way or another?
            inputSampleL += (dL[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleL -= (((dL[count] - dL[count + 1]) - (dL[count + 1] - dL[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleR += dR[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleR += dR[count + 1]; //we can assume always using this in one way or another?
            inputSampleR += (dR[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleR -= (((dR[count] - dR[count + 1]) - (dR[count + 1] - dR[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleL *= 0.125; //to get a comparable level
            inputSampleR *= 0.125; //to get a comparable level

        } else {

            offset = range + (modulation * sin(sweep));
            count += (int)floor(offset);

            inputSampleL = dL[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleL += dL[count + 1]; //we can assume always using this in one way or another?
            inputSampleL += (dL[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleL -= (((dL[count] - dL[count + 1]) - (dL[count + 1] - dL[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleR = dR[count] * (1 - (offset - floor(offset))); //less as value moves away from .0
            inputSampleR += dR[count + 1]; //we can assume always using this in one way or another?
            inputSampleR += (dR[count + 2] * (offset - floor(offset))); //greater as value moves away from .0
            inputSampleR -= (((dR[count] - dR[count + 1]) - (dR[count + 1] - dR[count + 2])) / 50); //interpolation hacks 'r us

            inputSampleL *= 0.5; //to get a comparable level
            inputSampleR *= 0.5; //to get a comparable level
            //sliding
        }

        sweep += speed;
        if (sweep > tupi) {
            sweep -= tupi;
        }
        //still scrolling through the samples, remember

        if (wet != 1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
            inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
        }
        fpFlip = !fpFlip;

        //stereo 32 bit dither, made small and tidy.
        int expon;
        frexpf((float)inputSampleL, &expon);
        long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
        inputSampleL += (dither - fpNShapeL);
        fpNShapeL = dither;
        frexpf((float)inputSampleR, &expon);
        dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
        inputSampleR += (dither - fpNShapeR);
        fpNShapeR = dither;
        //end 32 bit dither

        // output
        outputs[OUT_L_OUTPUT].setVoltage(inputSampleL);
        outputs[OUT_R_OUTPUT].setVoltage(inputSampleR);
    }
};

struct ChorusWidget : ModuleWidget {
    ChorusWidget(Chorus* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/chorus_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 65.0), module, Chorus::SPEED_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 125.0), module, Chorus::RANGE_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 185.0), module, Chorus::DRYWET_PARAM));

        // switches
        addParam(createParamCentered<CKSS>(Vec(75.0, 155.0), module, Chorus::ENSEMBLE_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(75, 136.8), module, Chorus::ENSEMBLE_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 245), module, Chorus::SPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 245), module, Chorus::RANGE_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285), module, Chorus::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285), module, Chorus::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325), module, Chorus::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325), module, Chorus::OUT_R_OUTPUT));
    }
};

Model* modelChorus = createModel<Chorus, ChorusWidget>("chorus");