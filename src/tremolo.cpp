
/***********************************************************************************************
Tremolo
-------
VCV Rack module based on Tremolo by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- CV inputs for speed and depth
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Tremolo : Module {
    enum ParamIds {
        SPEED_PARAM,
        DEPTH_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        CLOCK_CV_INPUT,
        SPEED_CV_INPUT,
        DEPTH_CV_INPUT,
        IN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        SPEED_LIGHT,
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;

    // control parameters
    float speedParam;
    float depthParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    double sweep[16];
    double speedChase[16];
    double depthChase[16];
    double speedAmount[16];
    double depthAmount[16];
    double lastSpeed[16];
    double lastDepth[16];
    long double fpNShape[16];

    // other variables, which do not need to be updated every cycle
    double overallscale;
    double speedSpeed;
    double depthSpeed;
    float lastSpeedParam;
    float lastDepthParam;

    // constants
    const double tupi = 3.141592653589793238;

    Tremolo()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPEED_PARAM, 0.f, 1.f, 0.f, "Speed");
        configParam(DEPTH_PARAM, 0.f, 1.f, 0.f, "Depth");

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        speedSpeed = 0.0;
        depthSpeed = 0.0;
        lastSpeedParam = 0.0;
        lastDepthParam = 0.0;

        for (int i = 0; i < 16; i++) {
            sweep[i] = 3.141592653589793238 / 2.0;
            speedChase[i] = 0.0;
            depthChase[i] = 0.0;
            speedAmount[i] = 1.0;
            depthAmount[i] = 0.0;
            lastSpeed[i] = 1000.0;
            lastDepth[i] = 1000.0;
            fpNShape[i] = 0.0;
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();

        // quality
        json_object_set_new(rootJ, "quality", json_integer(quality));

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override
    {
        // quality
        json_t* qualityJ = json_object_get(rootJ, "quality");
        if (qualityJ)
            quality = json_integer_value(qualityJ);
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_OUTPUT].isConnected()) {

            speedParam = params[SPEED_PARAM].getValue();
            speedParam += inputs[SPEED_CV_INPUT].getVoltage() / 5;
            speedParam = clamp(speedParam, 0.01f, 0.99f);

            depthParam = params[DEPTH_PARAM].getValue();
            depthParam += inputs[DEPTH_CV_INPUT].getVoltage() / 5;
            depthParam = clamp(depthParam, 0.01f, 0.99f);

            double speed;
            double depth;
            double skew;
            double density;
            double control;
            double tempcontrol;
            double thickness;
            double out;
            double bridgerectifier;
            double offset;
            long double inputSample;
            long double drySample;

            // number of polyphonic channels
            int numChannels = inputs[IN_INPUT].getChannels();

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                if (speedParam != lastSpeedParam) {
                    speedChase[i] = pow(speedParam, 4);
                    speedSpeed = 300 / (fabs(lastSpeed[i] - speedChase[i]) + 1.0);
                    lastSpeed[i] = speedChase[i];
                }

                if (depthParam != lastDepthParam) {
                    depthChase[i] = depthParam;
                    depthSpeed = 300 / (fabs(lastDepth[i] - depthChase[i]) + 1.0);
                    lastDepth[i] = depthChase[i];
                }

                // input
                inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                if (quality == HIGH) {
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
                }

                drySample = inputSample;

                speedAmount[i] = (((speedAmount[i] * speedSpeed) + speedChase[i]) / (speedSpeed + 1.0));
                depthAmount[i] = (((depthAmount[i] * depthSpeed) + depthChase[i]) / (depthSpeed + 1.0));
                speed = 0.0001 + (speedAmount[i] / 1000.0);
                speed /= overallscale;
                depth = 1.0 - pow(1.0 - depthAmount[i], 5);
                skew = 1.0 + pow(depthAmount[i], 9);
                density = ((1.0 - depthAmount[i]) * 2.0) - 1.0;

                offset = sin(sweep[i]);
                sweep[i] += speed;
                if (sweep[i] > tupi) {
                    sweep[i] -= tupi;
                }
                control = fabs(offset);
                if (density > 0) {
                    tempcontrol = sin(control);
                    control = (control * (1.0 - density)) + (tempcontrol * density);
                } else {
                    tempcontrol = 1 - cos(control);
                    control = (control * (1.0 + density)) + (tempcontrol * -density);
                }
                //produce either boosted or starved version of control signal
                //will go from 0 to 1

                thickness = ((control * 2.0) - 1.0) * skew;
                out = fabs(thickness);

                //max value for sine function
                bridgerectifier = fabs(inputSample);
                if (bridgerectifier > 1.57079633)
                    bridgerectifier = 1.57079633;

                //produce either boosted or starved version
                if (thickness > 0)
                    bridgerectifier = sin(bridgerectifier);
                else
                    bridgerectifier = 1 - cos(bridgerectifier);

                if (inputSample > 0)
                    inputSample = (inputSample * (1 - out)) + (bridgerectifier * out);
                else
                    inputSample = (inputSample * (1 - out)) - (bridgerectifier * out);

                //blend according to density control
                inputSample *= (1.0 - control);
                inputSample *= 2.0;
                //apply tremolo, apply gain boost to compensate for volume loss
                inputSample = (drySample * (1 - depth)) + (inputSample * depth);

                if (quality == HIGH) {
                    //stereo 32 bit dither, made small and tidy.
                    int expon;
                    frexpf((float)inputSample, &expon);
                    long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                    inputSample += (dither - fpNShape[i]);
                    fpNShape[i] = dither;
                }

                // bring gain back up
                inputSample *= gainBoost;

                // output
                outputs[OUT_OUTPUT].setChannels(numChannels);
                outputs[OUT_OUTPUT].setVoltage(inputSample, i);
            }

            // lights
            lights[SPEED_LIGHT].setSmoothBrightness(fmaxf(0.0, (-sweep[0]) + 1), args.sampleTime);
        }
    }
};

struct TremoloWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Tremolo* module;
        int quality;

        void onAction(const event::Action& e) override
        {
            module->quality = quality;
        }

        void step() override
        {
            rightText = (module->quality == quality) ? "✔" : "";
        }
    };

    void appendContextMenu(Menu* menu) override
    {
        Tremolo* module = dynamic_cast<Tremolo*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator()); // separator

        MenuLabel* qualityLabel = new MenuLabel(); // menu label
        qualityLabel->text = "Quality";
        menu->addChild(qualityLabel);

        QualityItem* low = new QualityItem(); // low quality
        low->text = "Eco";
        low->module = module;
        low->quality = 0;
        menu->addChild(low);

        QualityItem* high = new QualityItem(); // high quality
        high->text = "High";
        high->module = module;
        high->quality = 1;
        menu->addChild(high);
    }

    TremoloWidget(Tremolo* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tremolo_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Tremolo::SPEED_PARAM));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(30.0, 120.0), module, Tremolo::DEPTH_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(11, 103), module, Tremolo::SPEED_LIGHT));

        // inputs
        // addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 165.0), module, Tremolo::CLOCK_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Tremolo::SPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Tremolo::DEPTH_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Tremolo::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Tremolo::OUT_OUTPUT));
    }
};

Model* modelTremolo = createModel<Tremolo, TremoloWidget>("tremolo");