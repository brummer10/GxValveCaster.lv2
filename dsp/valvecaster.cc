// generated from file './/valvecaster.dsp' by dsp2cc:
// Code generated with Faust 0.9.90 (http://faust.grame.fr)

#include "valvecaster_table.h"

namespace valvecaster {

class Dsp: public PluginLV2 {
private:
	uint32_t fSamplingFreq;
	FAUSTFLOAT 	fslider0;
	FAUSTFLOAT	*fslider0_;
	double 	fRec0[2];
	double 	fConst0;
	double 	fConst1;
	double 	fConst2;
	double 	fConst3;
	double 	fConst4;
	double 	fConst5;
	double 	fConst6;
	double 	fConst7;
	double 	fConst8;
	FAUSTFLOAT 	fslider1;
	FAUSTFLOAT	*fslider1_;
	double 	fRec1[2];
	double 	fConst9;
	double 	fConst10;
	double 	fConst11;
	double 	fConst12;
	double 	fConst13;
	double 	fConst14;
	double 	fConst15;
	double 	fConst16;
	double 	fConst17;
	double 	fConst18;
	double 	fConst19;
	double 	fConst20;
	double 	fConst21;
	double 	fConst22;
	double 	fConst23;
	double 	fConst24;
	double 	fConst25;
	double 	fConst26;
	double 	fConst27;
	double 	fConst28;
	double 	fConst29;
	double 	fConst30;
	double 	fConst31;
	double 	fRec2[5];
	double 	fConst32;
	double 	fConst33;

	FAUSTFLOAT 	fsliderV0;
	FAUSTFLOAT 	*fsliderV0_;
	double 	fRecV0[2];
	void connect(uint32_t port,void* data);
	void clear_state_f();
	void init(uint32_t samplingFreq);
	void compute(int count, FAUSTFLOAT *input0, FAUSTFLOAT *output0);

	static void clear_state_f_static(PluginLV2*);
	static void init_static(uint32_t samplingFreq, PluginLV2*);
	static void compute_static(int count, FAUSTFLOAT *input0, FAUSTFLOAT *output0, PluginLV2*);
	static void del_instance(PluginLV2 *p);
	static void connect_static(uint32_t port,void* data, PluginLV2 *p);
public:
	Dsp();
	~Dsp();
};



Dsp::Dsp()
	: PluginLV2() {
	version = PLUGINLV2_VERSION;
	id = "valvecaster";
	name = N_("ValveCaster");
	mono_audio = compute_static;
	stereo_audio = 0;
	set_samplerate = init_static;
	activate_plugin = 0;
	connect_ports = connect_static;
	clear_state = clear_state_f_static;
	delete_instance = del_instance;
}

Dsp::~Dsp() {
}

inline void Dsp::clear_state_f()
{
	for (int i=0; i<2; i++) fRec0[i] = 0;
	for (int i=0; i<2; i++) fRec1[i] = 0;
	for (int i=0; i<5; i++) fRec2[i] = 0;
	for (int i=0; i<2; i++) fRecV0[i] = 0;
}

void Dsp::clear_state_f_static(PluginLV2 *p)
{
	static_cast<Dsp*>(p)->clear_state_f();
}

inline void Dsp::init(uint32_t samplingFreq)
{
	fSamplingFreq = samplingFreq;
	fConst0 = double(min(1.92e+05, max(1.0, (double)fSamplingFreq)));
	fConst1 = (4.44357420714026e-19 * fConst0);
	fConst2 = (3.8636652125391e-17 + (fConst0 * (8.44554340772754e-17 + (fConst0 * (1.23246239484246e-17 + fConst1)))));
	fConst3 = (6.92615634280557e-20 * fConst0);
	fConst4 = (4.26140835598916e-14 + (fConst0 * (6.23413327247725e-15 + (fConst0 * (2.26252802954969e-16 + fConst3)))));
	fConst5 = (3.23671585942339e-19 * fConst0);
	fConst6 = ((fConst0 * ((fConst0 * (0 - (8.59293924139801e-18 + fConst5))) - 5.72395031946121e-17)) - 2.61304666096613e-17);
	fConst7 = (5.0450378534428e-20 * fConst0);
	fConst8 = ((fConst0 * ((fConst0 * (0 - (1.64743466872933e-16 + fConst7))) - 4.3462553204519e-15)) - 2.88810562634027e-14);
	fConst9 = (2.46744442363404e-32 * fConst0);
	fConst10 = (2.10223322186702e-18 * fConst0);
	fConst11 = ((fConst0 * (8.44554340772754e-17 + (fConst0 * (fConst1 - 1.23246239484246e-17)))) - 3.8636652125391e-17);
	fConst12 = ((fConst0 * (6.23413327247725e-15 + (fConst0 * (fConst3 - 2.26252802954969e-16)))) - 4.26140835598916e-14);
	fConst13 = (2.61304666096613e-17 + (fConst0 * ((fConst0 * (8.59293924139801e-18 - fConst5)) - 5.72395031946121e-17)));
	fConst14 = (2.88810562634027e-14 + (fConst0 * ((fConst0 * (1.64743466872933e-16 - fConst7)) - 4.3462553204519e-15)));
	fConst15 = (1.7774296828561e-18 * fConst0);
	fConst16 = faustpower<2>(fConst0);
	fConst17 = ((fConst16 * (2.46492478968493e-17 - fConst15)) - 7.7273304250782e-17);
	fConst18 = (2.77046253712223e-19 * fConst0);
	fConst19 = ((fConst16 * (4.52505605909937e-16 - fConst18)) - 8.52281671197832e-14);
	fConst20 = (1.29468634376935e-18 * fConst0);
	fConst21 = (5.22609332193227e-17 + (fConst16 * (fConst20 - 1.7185878482796e-17)));
	fConst22 = (2.01801514137712e-19 * fConst0);
	fConst23 = (5.77621125268054e-14 + (fConst16 * (fConst22 - 3.29486933745867e-16)));
	fConst24 = ((2.66614452428415e-18 * fConst16) - 1.68910868154551e-16);
	fConst25 = ((4.15569380568334e-19 * fConst16) - 1.24682665449545e-14);
	fConst26 = (1.14479006389224e-16 - (1.94202951565403e-18 * fConst16));
	fConst27 = (8.6925106409038e-15 - (3.02702271206568e-19 * fConst16));
	fConst28 = (7.7273304250782e-17 + (fConst16 * (0 - (2.46492478968493e-17 + fConst15))));
	fConst29 = (8.52281671197832e-14 + (fConst16 * (0 - (4.52505605909937e-16 + fConst18))));
	fConst30 = ((fConst16 * (1.7185878482796e-17 + fConst20)) - 5.22609332193227e-17);
	fConst31 = ((fConst16 * (3.29486933745867e-16 + fConst22)) - 5.77621125268054e-14);
	fConst32 = (8.40893288746809e-18 * fConst0);
	fConst33 = faustpower<3>(fConst0);
	clear_state_f();
}

void Dsp::init_static(uint32_t samplingFreq, PluginLV2 *p)
{
	static_cast<Dsp*>(p)->init(samplingFreq);
}

void always_inline Dsp::compute(int count, FAUSTFLOAT *input0, FAUSTFLOAT *output0)
{
#define fslider0 (*fslider0_)
#define fslider1 (*fslider1_)
#define fsliderV0 (*fsliderV0_)
	double 	fSlowV0 = (0.0010000000000000009 * double(fsliderV0));

	double 	fSlow0 = (0.007000000000000006 * double(fslider0));
	double 	fSlow1 = (0.007000000000000006 * double(fslider1));
	for (int i=0; i<count; i++) {
		fRec0[0] = (fSlow0 + (0.993 * fRec0[1]));
		fRec1[0] = (fSlow1 + (0.993 * fRec1[1]));
		double fTemp0 = (1.93183260626955e-14 + ((fRec1[0] * ((fConst0 * (fConst8 + (fConst6 * fRec0[0]))) - 1.30652333048307e-14)) + (fConst0 * (fConst4 + (fConst2 * fRec0[0])))));
		double fTemp1 = (fConst9 * fRec0[0]);
		double fTemp2 = (fConst10 * fRec0[0]);
		fRec2[0] = ((double)input0[i] - (((((fRec2[1] * (7.7273304250782e-14 + ((fRec1[0] * ((fConst0 * (fConst31 + (fConst30 * fRec0[0]))) - 5.22609332193227e-14)) + (fConst0 * (fConst29 + (fConst28 * fRec0[0])))))) + (fRec2[2] * (1.15909956376173e-13 + ((fRec1[0] * ((fConst16 * (fConst27 + (fConst26 * fRec0[0]))) - 7.8391399828984e-14)) + (fConst16 * (fConst25 + (fConst24 * fRec0[0]))))))) + (fRec2[3] * (7.7273304250782e-14 + ((fRec1[0] * ((fConst0 * (fConst23 + (fConst21 * fRec0[0]))) - 5.22609332193227e-14)) + (fConst0 * (fConst19 + (fConst17 * fRec0[0]))))))) + (fRec2[4] * (1.93183260626955e-14 + ((fRec1[0] * ((fConst0 * (fConst14 + (fConst13 * fRec0[0]))) - 1.30652333048307e-14)) + (fConst0 * (fConst12 + (fConst11 * fRec0[0]))))))) / fTemp0));
		double fTemp3 = (fConst32 * fRec0[0]);
		double fTemp4 = (fConst0 * (0 - (9.86977769453617e-32 * fRec0[0])));
		output0[i] = (FAUSTFLOAT)tubeclip((fConst33 * (((fRec2[3] * ((2.10223322186702e-15 + (fRec1[0] * (2.46744442363404e-29 + fTemp4))) - fTemp3)) + ((fConst0 * ((fRec2[2] * fRec0[0]) * (1.26133993312021e-17 + (1.48046665418043e-31 * fRec1[0])))) + ((fRec2[1] * ((fRec1[0] * (fTemp4 - 2.46744442363404e-29)) - (2.10223322186702e-15 + fTemp3))) + ((fRec2[0] * (1.05111661093351e-15 + ((fRec1[0] * (1.23372221181702e-29 + fTemp1)) + fTemp2))) + (fRec2[4] * ((fTemp2 + (fRec1[0] * (fTemp1 - 1.23372221181702e-29))) - 1.05111661093351e-15)))))) / fTemp0)));
		// post processing
		for (int i=4; i>0; i--) fRec2[i] = fRec2[i-1];
		fRec1[1] = fRec1[0];
		fRec0[1] = fRec0[0];
	}
	for (int i=0; i<count; i++) {
		fRecV0[0] = ((0.999 * fRecV0[1]) + fSlowV0);
		output0[i] = (FAUSTFLOAT)((double)output0[i] * fRecV0[0]);
		// post processing
		fRecV0[1] = fRecV0[0];
	}

#undef fsliderV0 
#undef fslider0
#undef fslider1
}

void __rt_func Dsp::compute_static(int count, FAUSTFLOAT *input0, FAUSTFLOAT *output0, PluginLV2 *p)
{
	static_cast<Dsp*>(p)->compute(count, input0, output0);
}


void Dsp::connect(uint32_t port,void* data)
{
	switch ((PortIndex)port)
	{
	case GAIN: 
		fslider1_ = (float*)data; // , 0.5, 0.0, 1.0, 0.01 
		break;
	case TONE: 
		fslider0_ = (float*)data; // , 0.5, 0.0, 1.0, 0.01 
		break;
	case VOLUME: 
		fsliderV0_ = (float*)data; // , 0.5, 0.0, 1, 0.01 
		break;
	default:
		break;
	}
}

void Dsp::connect_static(uint32_t port,void* data, PluginLV2 *p)
{
	static_cast<Dsp*>(p)->connect(port, data);
}


PluginLV2 *plugin() {
	return new Dsp();
}

void Dsp::del_instance(PluginLV2 *p)
{
	delete static_cast<Dsp*>(p);
}

/*
typedef enum
{
   GAIN, 
   TONE, 
   VOLUME,
} PortIndex;
*/

} // end namespace valvecaster
