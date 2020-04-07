#include "plugin.hpp"

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

    long double fpNShapeL;
    //default stuff

    double sweep;
    double speedChase;
    double depthChase;
    double speedAmount;
    double depthAmount;
    double lastSpeed;
    double lastDepth;

    float A;
    float B;

    Tremolo()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPEED_PARAM, 0.f, 1.f, 0.f, "Speed");
        configParam(DEPTH_PARAM, 0.f, 1.f, 0.f, "Depth");

        A = 0.5;
        B = 1.0;
        sweep = 3.141592653589793238 / 2.0;
        speedChase = 0.0;
        depthChase = 0.0;
        speedAmount = 1.0;
        depthAmount = 0.0;
        lastSpeed = 1000.0;
        lastDepth = 1000.0;
        fpNShapeL = 0.0;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_OUTPUT].isConnected()) {

            // params
            A = params[SPEED_PARAM].getValue();
            A += inputs[SPEED_CV_INPUT].getVoltage() / 5;
            A = clamp(A, 0.01f, 0.99f);

            B = params[DEPTH_PARAM].getValue();
            B += inputs[DEPTH_CV_INPUT].getVoltage() / 5;
            B = clamp(B, 0.01f, 0.99f);

            // input
            float in1 = inputs[IN_INPUT].getVoltage();

            double overallscale = 1.0;
            overallscale /= 44100.0;
            overallscale *= args.sampleRate;

            speedChase = pow(A, 4);
            depthChase = B;
            double speedSpeed = 300 / (fabs(lastSpeed - speedChase) + 1.0);
            double depthSpeed = 300 / (fabs(lastDepth - depthChase) + 1.0);
            lastSpeed = speedChase;
            lastDepth = depthChase;

            double speed;
            double depth;
            double skew;
            double density;

            double tupi = 3.141592653589793238;
            double control;
            double tempcontrol;
            double thickness;
            double out;
            double bridgerectifier;
            double offset;

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

            speedAmount = (((speedAmount * speedSpeed) + speedChase) / (speedSpeed + 1.0));
            depthAmount = (((depthAmount * depthSpeed) + depthChase) / (depthSpeed + 1.0));
            speed = 0.0001 + (speedAmount / 1000.0);
            speed /= overallscale;
            depth = 1.0 - pow(1.0 - depthAmount, 5);
            skew = 1.0 + pow(depthAmount, 9);
            density = ((1.0 - depthAmount) * 2.0) - 1.0;

            offset = sin(sweep);
            sweep += speed;
            if (sweep > tupi) {
                sweep -= tupi;
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

            //do L
            bridgerectifier = fabs(inputSampleL);
            if (bridgerectifier > 1.57079633)
                bridgerectifier = 1.57079633;
            //max value for sine function
            if (thickness > 0)
                bridgerectifier = sin(bridgerectifier);
            else
                bridgerectifier = 1 - cos(bridgerectifier);
            //produce either boosted or starved version
            if (inputSampleL > 0)
                inputSampleL = (inputSampleL * (1 - out)) + (bridgerectifier * out);
            else
                inputSampleL = (inputSampleL * (1 - out)) - (bridgerectifier * out);
            //blend according to density control
            inputSampleL *= (1.0 - control);
            inputSampleL *= 2.0;
            //apply tremolo, apply gain boost to compensate for volume loss
            inputSampleL = (drySampleL * (1 - depth)) + (inputSampleL * depth);
            //end L

            //stereo 32 bit dither, made small and tidy.
            int expon;
            frexpf((float)inputSampleL, &expon);
            long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
            inputSampleL += (dither - fpNShapeL);
            fpNShapeL = dither;

            // lights
            lights[SPEED_LIGHT].setSmoothBrightness(fmaxf(0.0, (-sweep) + 1), args.sampleTime);

            // outputs
            outputs[OUT_OUTPUT].setVoltage(inputSampleL);
        }
    }
};

struct TremoloWidget : ModuleWidget {
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