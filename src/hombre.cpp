/***********************************************************************************************
Hombre
-------
VCV Rack module based on Hombre by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- mono (hey Chris, is the original actually stereo?)
- CV inputs for voicing and intensity

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Hombre : Module {
    enum ParamIds {
        VOICING_PARAM,
        INTENSITY_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        VOICING_CV_INPUT,
        INTENSITY_CV_INPUT,
        IN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    double pL[4001];
    double pR[4001];
    double slide;
    int gcount;

    long double fpNShapeL;
    long double fpNShapeR;
    //default stuff

    float A;
    float B;

    Hombre()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(VOICING_PARAM, 0.f, 1.f, 0.5f, "");
        configParam(INTENSITY_PARAM, 0.f, 1.f, 0.5f, "");

        // A = 0.421;
        // B = 0.5;
        for (int count = 0; count < 4000; count++) {
            pL[count] = 0.0;
            pR[count] = 0.0;
        }
        gcount = 0;
        slide = 0.421;
        fpNShapeL = 0.0;
        fpNShapeR = 0.0;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void process(const ProcessArgs& args) override
    {
        // params
        A = params[VOICING_PARAM].getValue();
        A += inputs[VOICING_CV_INPUT].getVoltage() / 5;
        A = clamp(A, 0.01f, 0.99f);

        B = params[INTENSITY_PARAM].getValue();
        B += inputs[INTENSITY_CV_INPUT].getVoltage() / 5;
        B = clamp(B, 0.01f, 0.99f);

        // input
        float in1 = inputs[IN_INPUT].getVoltage();

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= args.sampleRate;

        double target = A;
        double offsetA;
        double offsetB;
        int widthA = (int)(1.0 * overallscale);
        int widthB = (int)(7.0 * overallscale); //max 364 at 44.1, 792 at 96K
        double wet = B;
        double dry = 1.0 - wet;
        double totalL;
        double totalR;
        int count;

        long double inputSampleL;
        long double inputSampleR;
        double drySampleL;
        // double drySampleR;

        inputSampleL = in1;
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
        // if (inputSampleR < 1.2e-38 && -inputSampleR < 1.2e-38) {
        //     static int noisesource = 0;
        //     noisesource = noisesource % 1700021;
        //     noisesource++;
        //     int residue = noisesource * noisesource;
        //     residue = residue % 170003;
        //     residue *= residue;
        //     residue = residue % 17011;
        //     residue *= residue;
        //     residue = residue % 1709;
        //     residue *= residue;
        //     residue = residue % 173;
        //     residue *= residue;
        //     residue = residue % 17;
        //     double applyresidue = residue;
        //     applyresidue *= 0.00000001;
        //     applyresidue *= 0.00000001;
        //     inputSampleR = applyresidue;
        //     //this denormalization routine produces a white noise at -300 dB which the noise
        //     //shaping will interact with to produce a bipolar output, but the noise is actually
        //     //all positive. That should stop any variables from going denormal, and the routine
        //     //only kicks in if digital black is input. As a final touch, if you save to 24-bit
        //     //the silence will return to being digital black again.
        // }
        drySampleL = inputSampleL;
        // drySampleR = inputSampleR;

        slide = (slide * 0.9997) + (target * 0.0003);

        offsetA = ((pow(slide, 2)) * 77) + 3.2;
        offsetB = (3.85 * offsetA) + 41;
        offsetA *= overallscale;
        offsetB *= overallscale;
        //adjust for sample rate

        if (gcount < 1 || gcount > 2000) {
            gcount = 2000;
        }
        count = gcount;

        pL[count + 2000] = pL[count] = inputSampleL;
        // pR[count + 2000] = pR[count] = inputSampleR;
        //double buffer

        count = (int)(gcount + floor(offsetA));

        totalL = pL[count] * 0.391; //less as value moves away from .0
        totalL += pL[count + widthA]; //we can assume always using this in one way or another?
        totalL += pL[count + widthA + widthA] * 0.391; //greater as value moves away from .0

        // totalR = pR[count] * 0.391; //less as value moves away from .0
        // totalR += pR[count + widthA]; //we can assume always using this in one way or another?
        // totalR += pR[count + widthA + widthA] * 0.391; //greater as value moves away from .0

        inputSampleL += ((totalL * 0.274));
        // inputSampleR += ((totalR * 0.274));

        count = (int)(gcount + floor(offsetB));

        totalL = pL[count] * 0.918; //less as value moves away from .0
        totalL += pL[count + widthB]; //we can assume always using this in one way or another?
        totalL += pL[count + widthB + widthB] * 0.918; //greater as value moves away from .0

        // totalR = pR[count] * 0.918; //less as value moves away from .0
        // totalR += pR[count + widthB]; //we can assume always using this in one way or another?
        // totalR += pR[count + widthB + widthB] * 0.918; //greater as value moves away from .0

        inputSampleL -= ((totalL * 0.629));
        // inputSampleR -= ((totalR * 0.629));

        inputSampleL /= 4;
        // inputSampleR /= 4;

        gcount--;
        //still scrolling through the samples, remember

        if (wet != 1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
            // inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
        }

        //stereo 32 bit dither, made small and tidy.
        int expon;
        frexpf((float)inputSampleL, &expon);
        long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
        inputSampleL += (dither - fpNShapeL);
        fpNShapeL = dither;
        // frexpf((float)inputSampleR, &expon);
        // dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
        // inputSampleR += (dither - fpNShapeR);
        // fpNShapeR = dither;
        //end 32 bit dither

        // output
        outputs[OUT_OUTPUT].setVoltage(inputSampleL);
    }
};

struct HombreWidget : ModuleWidget {
    HombreWidget(Hombre* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/hombre_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Hombre::VOICING_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Hombre::INTENSITY_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Hombre::VOICING_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Hombre::INTENSITY_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Hombre::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Hombre::OUT_OUTPUT));
    }
};

Model* modelHombre = createModel<Hombre, HombreWidget>("hombre");