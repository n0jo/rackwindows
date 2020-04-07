/***********************************************************************************************
Distance
--------
VCV Rack module based on Distance by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- mono
- CV inputs for distance and dry/wet

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Distance : Module {
    enum ParamIds {
        DISTANCE_PARAM,
        DRYWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        DISTANCE_CV_INPUT,
        DRYWET_CV_INPUT,
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

    double lastclampL;
    double clampL;
    double changeL;
    double thirdresultL;
    double prevresultL;
    double lastL;

    long double fpNShapeL;
    //default stuff

    float A;
    float B;

    Distance()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(DISTANCE_PARAM, 0.f, 1.f, 0.f, "Distance");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");

        A = 0.0;
        B = 1.0;
        thirdresultL = prevresultL = lastclampL = clampL = changeL = lastL = 0.0;
        fpNShapeL = 0.0;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_OUTPUT].isConnected()) {

            // params
            A = params[DISTANCE_PARAM].getValue();
            A += inputs[DISTANCE_CV_INPUT].getVoltage() / 5;
            A = clamp(A, 0.01f, 0.99f);

            B = params[DRYWET_PARAM].getValue();
            B += inputs[DRYWET_CV_INPUT].getVoltage() / 5;
            B = clamp(B, 0.01f, 0.99f);

            double gainCut = 0.0000152587890625;
            double gainBoost = 65536.0;

            // input
            float in1 = inputs[IN_INPUT].getVoltage();

            // pad gain massively to prevent distortion
            in1 *= gainCut;

            double overallscale = 1.0;
            overallscale /= 44100.0;
            overallscale *= args.sampleRate;

            double softslew = (pow(A * 2.0, 3.0) * 12.0) + 0.6;
            softslew *= overallscale;
            double filtercorrect = softslew / 2.0;
            double thirdfilter = softslew / 3.0;
            double levelcorrect = 1.0 + (softslew / 6.0);
            double postfilter;
            double wet = B;
            double dry = 1.0 - wet;
            double bridgerectifier;

            long double inputSampleL;
            long double drySampleL;

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
            drySampleL = inputSampleL;

            inputSampleL *= softslew;
            lastclampL = clampL;
            clampL = inputSampleL - lastL;
            postfilter = changeL = fabs(clampL - lastclampL);
            postfilter += filtercorrect;
            if (changeL > 1.5707963267949)
                changeL = 1.5707963267949;
            bridgerectifier = (1.0 - sin(changeL));
            if (bridgerectifier < 0.0)
                bridgerectifier = 0.0;
            inputSampleL = lastL + (clampL * bridgerectifier);
            lastL = inputSampleL;
            inputSampleL /= softslew;
            inputSampleL += (thirdresultL * thirdfilter);
            inputSampleL /= (thirdfilter + 1.0);
            inputSampleL += (prevresultL * postfilter);
            inputSampleL /= (postfilter + 1.0);
            //do an IIR like thing to further squish superdistant stuff
            thirdresultL = prevresultL;
            prevresultL = inputSampleL;
            inputSampleL *= levelcorrect;

            if (wet < 1.0) {
                inputSampleL = (drySampleL * dry) + (inputSampleL * wet);
            }

            //stereo 32 bit dither, made small and tidy.
            int expon;
            frexpf((float)inputSampleL, &expon);
            long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
            inputSampleL += (dither - fpNShapeL);
            fpNShapeL = dither;
            //end 32 bit dither

            // bring gain back up
            inputSampleL *= gainBoost;

            // output
            outputs[OUT_OUTPUT].setVoltage(inputSampleL);
        }
    }
};

struct DistanceWidget : ModuleWidget {
    DistanceWidget(Distance* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/distance_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Distance::DISTANCE_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Distance::DRYWET_PARAM));

        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Distance::DISTANCE_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Distance::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Distance::IN_INPUT));

        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Distance::OUT_OUTPUT));
    }
};

Model* modelDistance = createModel<Distance, DistanceWidget>("distance");