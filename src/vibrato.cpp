/***********************************************************************************************
Vibrato
-------
VCV Rack module based on Vibrato by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- CV inputs for speed, depth, fmspeed, fmdepth and inv/wet 
- trigger outputs (EOC) for speed and fmspeed
- polyphonic

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Vibrato : Module {

    enum ParamIds {
        SPEED_PARAM,
        FMSPEED_PARAM,
        DEPTH_PARAM,
        FMDEPTH_PARAM,
        INVWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        SPEED_CV_INPUT,
        DEPTH_CV_INPUT,
        FMSPEED_CV_INPUT,
        FMDEPTH_CV_INPUT,
        INVWET_CV_INPUT,
        IN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        EOC_OUTPUT,
        OUT_OUTPUT,
        EOC_FM_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        SPEED_LIGHT,
        SPEED_FM_LIGHT,
        NUM_LIGHTS
    };

    const double gainCut = 0.03125;
    const double gainBoost = 32.0;

    double p[16][16386]; //this is processed, not raw incoming samples
    double sweep[16];
    double sweepB[16];
    int gcount[16];

    double airPrev[16];
    double airEven[16];
    double airOdd[16];
    double airFactor[16];

    bool flip[16];

    uint32_t fpd[16];

    float A;
    float B;
    float C;
    float D;
    float E; //parameters. Always 0-1, and we scale/alter them elsewhere.

    // EOC
    dsp::PulseGenerator eocPulse, eocFmPulse;
    bool triggerEoc = false;
    bool triggerEocFm = false;
    float triggerLength = 0.0001f;
    float triggerThreshold = 0.0005f;

    float speedLight = 0.0f;
    float speedFmLight = 0.0f;
    const float lightLambda = 0.075f;

    Vibrato()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPEED_PARAM, 0.f, 1.f, 0.f, "Speed");
        configParam(FMSPEED_PARAM, 0.f, 1.f, 0.f, "FM Speed");
        configParam(DEPTH_PARAM, 0.f, 1.f, 0.f, "Depth");
        configParam(FMDEPTH_PARAM, 0.f, 1.f, 0.f, "FM Depth");
        configParam(INVWET_PARAM, 0.f, 1.f, 0.5f, "Inv/Wet");

        for (int i = 0; i < 16; i++) {
            for (int count = 0; count < 16385; count++) {
                p[i][count] = 0.0;
            }
            sweep[i] = sweepB[i] = 3.141592653589793238 / 2.0;
            gcount[i] = 0;

            airPrev[i] = 0.0;
            airEven[i] = 0.0;
            airOdd[i] = 0.0;
            airFactor[i] = 0.0;

            flip[i] = false;

            fpd[i] = 17;
        }
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_OUTPUT].isConnected() || outputs[EOC_OUTPUT].isConnected() || outputs[EOC_FM_OUTPUT].isConnected()) {

            A = params[SPEED_PARAM].getValue();
            A += inputs[SPEED_CV_INPUT].getVoltage() / 5;
            A = clamp(A, 0.01f, 0.99f);

            B = params[DEPTH_PARAM].getValue();
            B += inputs[DEPTH_CV_INPUT].getVoltage() / 5;
            B = clamp(B, 0.01f, 0.99f);

            C = params[FMSPEED_PARAM].getValue();
            C += inputs[FMSPEED_CV_INPUT].getVoltage() / 5;
            C = clamp(C, 0.01f, 0.99f);

            D = params[FMDEPTH_PARAM].getValue();
            D += inputs[FMDEPTH_CV_INPUT].getVoltage() / 5;
            D = clamp(D, 0.01f, 0.99f);

            E = params[INVWET_PARAM].getValue();
            E += inputs[INVWET_CV_INPUT].getVoltage() / 5;
            E = clamp(E, 0.01f, 0.99f);

            double speed = pow(0.1 + A, 6);
            double depth = (pow(B, 3) / sqrt(speed)) * 4.0;
            double speedB = pow(0.1 + C, 6);
            double depthB = pow(D, 3) / sqrt(speedB);
            double tupi = 3.141592653589793238 * 2.0;
            double wet = (E * 2.0) - 1.0; //note: inv/dry/wet

            // number of polyphonic channels
            int numChannels = inputs[IN_INPUT].getChannels();

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                // input
                long double inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                if (fabs(inputSample) < 1.18e-37)
                    inputSample = fpd[i] * 1.18e-37;

                double drySample = inputSample;

                airFactor[i] = airPrev[i] - inputSample;

                if (flip[i]) {
                    airEven[i] += airFactor[i];
                    airOdd[i] -= airFactor[i];
                    airFactor[i] = airEven[i];
                } else {
                    airOdd[i] += airFactor[i];
                    airEven[i] -= airFactor[i];
                    airFactor[i] = airOdd[i];
                }

                //air, compensates for loss of highs in the interpolation
                airOdd[i] = (airOdd[i] - ((airOdd[i] - airEven[i]) / 256.0)) / 1.0001;
                airEven[i] = (airEven[i] - ((airEven[i] - airOdd[i]) / 256.0)) / 1.0001;
                airPrev[i] = inputSample;
                inputSample += airFactor[i];

                flip[i] = !flip[i];

                if (gcount[i] < 1 || gcount[i] > 8192) {
                    gcount[i] = 8192;
                }
                int count = gcount[i];
                p[i][count + 8192] = p[i][count] = inputSample;

                double offset = depth + (depth * sin(sweep[i]));
                count += (int)floor(offset);

                inputSample = p[i][count] * (1.0 - (offset - floor(offset))); //less as value moves away from .0
                inputSample += p[i][count + 1]; //we can assume always using this in one way or another?
                inputSample += p[i][count + 2] * (offset - floor(offset)); //greater as value moves away from .0
                inputSample -= ((p[i][count] - p[i][count + 1]) - (p[i][count + 1] - p[i][count + 2])) / 50.0; //interpolation hacks 'r us
                inputSample *= 0.5; // gain trim

                //still scrolling through the samples, remember
                sweep[i] += (speed + (speedB * sin(sweepB[i]) * depthB));
                sweepB[i] += speedB;
                if (sweep[i] > tupi) {
                    sweep[i] -= tupi;
                }
                if (sweep[i] < 0.0) {
                    sweep[i] += tupi;
                } //through zero FM
                if (sweepB[i] > tupi) {
                    sweepB[i] -= tupi;
                }
                gcount[i]--;

                //Inv/Dry/Wet control
                if (wet != 1.0) {
                    inputSample = (inputSample * wet) + (drySample * (1.0 - fabs(wet)));
                }

                //begin 32 bit stereo floating point dither
                int expon;
                frexpf((float)inputSample, &expon);
                fpd[i] ^= fpd[i] << 13;
                fpd[i] ^= fpd[i] >> 17;
                fpd[i] ^= fpd[i] << 5;
                inputSample += ((double(fpd[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                //end 32 bit stereo floating point dither

                // bring gain back up
                inputSample *= gainBoost;

                // audio output
                outputs[OUT_OUTPUT].setChannels(numChannels);
                outputs[OUT_OUTPUT].setVoltage(inputSample, i);
            }

            // triggers
            if (sweep[0] < 0.1) {
                eocPulse.trigger(1e-3);
            }
            if (sweepB[0] < 0.1) {
                eocFmPulse.trigger(1e-3);
            }

            // lights
            lights[SPEED_LIGHT].setSmoothBrightness(fmaxf(0.0, (-sweep[0] / 5) + 1), args.sampleTime);
            lights[SPEED_FM_LIGHT].setSmoothBrightness(fmaxf(0.0, (-sweepB[0] / 5) + 1), args.sampleTime);

            // trigger outputs
            outputs[EOC_OUTPUT].setVoltage((eocPulse.process(args.sampleTime) ? 10.0 : 0.0));
            outputs[EOC_FM_OUTPUT].setVoltage((eocFmPulse.process(args.sampleTime) ? 10.0 : 0.0));
        }
    }
};

struct VibratoWidget : ModuleWidget {
    VibratoWidget(Vibrato* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vibrato_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Vibrato::SPEED_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(90.0, 65.0), module, Vibrato::FMSPEED_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Vibrato::DEPTH_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(90.0, 125.0), module, Vibrato::FMDEPTH_PARAM));
        addParam(createParamCentered<RwKnobLargeDark>(Vec(60.0, 190.0), module, Vibrato::INVWET_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(13, 37), module, Vibrato::SPEED_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(107, 37), module, Vibrato::SPEED_FM_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 245.0), module, Vibrato::SPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 285.0), module, Vibrato::DEPTH_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 245.0), module, Vibrato::FMSPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 285.0), module, Vibrato::FMDEPTH_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(60.0, 245.0), module, Vibrato::INVWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(60.0, 285.0), module, Vibrato::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(22.5, 325.0), module, Vibrato::EOC_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(60.0, 325.0), module, Vibrato::OUT_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(97.5, 325.0), module, Vibrato::EOC_FM_OUTPUT));
    }
};

Model* modelVibrato = createModel<Vibrato, VibratoWidget>("vibrato");