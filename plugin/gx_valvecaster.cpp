/*
 * Copyright (C) 2014 Guitarix project MOD project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * --------------------------------------------------------------------------
 */


#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>

///////////////////////// MACRO SUPPORT ////////////////////////////////

#define __rt_func __attribute__((section(".rt.text")))
#define __rt_data __attribute__((section(".rt.data")))

///////////////////////// FAUST SUPPORT ////////////////////////////////

#define FAUSTFLOAT float
#ifndef N_
#define N_(String) (String)
#endif
#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

#define always_inline inline __attribute__((always_inline))

template <int32_t N> inline float faustpower(float x)
{
  return powf(x, N);
}
template <int32_t N> inline double faustpower(double x)
{
  return pow(x, N);
}
template <int32_t N> inline int32_t faustpower(int32_t x)
{
  return faustpower<N/2>(x) * faustpower<N-N/2>(x);
}
template <>      inline int32_t faustpower<0>(int32_t x)
{
  return 1;
}
template <>      inline int32_t faustpower<1>(int32_t x)
{
  return x;
}

////////////////////////////// LOCAL INCLUDES //////////////////////////

#include "gx_valvecaster.h"        // define struct PortIndex
#include "gx_pluginlv2.h"   // define struct PluginLV2
#include "resampler.cc"   // define struct PluginLV2
#include "resampler-table.cc"   // define struct PluginLV2
#include "zita-resampler/resampler.h"
#include "valvecaster.cc"    // dsp class generated by faust -> dsp2cc
#include "valvecasterbuster.cc"    // dsp class generated by faust -> dsp2cc

////////////////////////////// PLUG-IN CLASS ///////////////////////////

namespace valvecaster {

class SimpleResampler {
 private:
    Resampler r_up, r_down;
    int m_fact;
 public:
    SimpleResampler(): r_up(), r_down(), m_fact() {}
    void setup(int sampleRate, unsigned int fact);
    void up(int count, float *input, float *output);
    void down(int count, float *input, float *output);
};

void SimpleResampler::setup(int sampleRate, unsigned int fact)
{
	m_fact = fact;
	const int qual = 16; // resulting in a total delay of 2*qual (0.7ms @44100)
	// upsampler
	r_up.setup(sampleRate, sampleRate*fact, 1, qual);
	// k == inpsize() == 2 * qual
	// pre-fill with k-1 zeros
	r_up.inp_count = r_up.inpsize() - 1;
	r_up.out_count = 1;
	r_up.inp_data = r_up.out_data = 0;
	r_up.process();
	// downsampler
	r_down.setup(sampleRate*fact, sampleRate, 1, qual);
	// k == inpsize() == 2 * qual * fact
	// pre-fill with k-1 zeros
	r_down.inp_count = r_down.inpsize() - 1;
	r_down.out_count = 1;
	r_down.inp_data = r_down.out_data = 0;
	r_down.process();
}

void SimpleResampler::up(int count, float *input, float *output)
{
	r_up.inp_count = count;
	r_up.inp_data = input;
	r_up.out_count = count * m_fact;
	r_up.out_data = output;
	r_up.process();
	assert(r_up.inp_count == 0);
	assert(r_up.out_count == 0);
}

void SimpleResampler::down(int count, float *input, float *output)
{
	r_down.inp_count = count * m_fact;
	r_down.inp_data = input;
	r_down.out_count = count+1; // +1 == trick to drain input
	r_down.out_data = output;
	r_down.process();
	assert(r_down.inp_count == 0);
	assert(r_down.out_count == 1);
}

class Gx_valvecaster_
{
private:
  // pointer to buffer
  float*          output;
  float*          input;
  // pointer to dsp class
  PluginLV2*      valvecaster;
  PluginLV2*      valvecasterbuster;

  uint32_t fSamplingFreq;
  SimpleResampler smp;
  unsigned int fact;
  // bypass ramping
  float*          bypass;
  uint32_t        bypass_;
  // booster
  float*          boost;
  uint32_t        boost_;
 
  bool            needs_ramp_down;
  bool            needs_ramp_up;
  float           ramp_down;
  float           ramp_up;
  float           ramp_up_step;
  float           ramp_down_step;
  bool            bypassed;

  // boost ramping
  bool            needs_boost_down;
  bool            needs_boost_up;
  float           boost_down;
  float           boost_up;
  bool            boost_is;

  // private functions
  inline void run_dsp_(uint32_t n_samples);
  inline void connect_(uint32_t port,void* data);
  inline void init_dsp_(uint32_t rate);
  inline void connect_all__ports(uint32_t port, void* data);
  inline void activate_f();
  inline void clean_up();
  inline void deactivate_f();

public:
  // LV2 Descriptor
  static const LV2_Descriptor descriptor;
  // static wrapper to private functions
  static void deactivate(LV2_Handle instance);
  static void cleanup(LV2_Handle instance);
  static void run(LV2_Handle instance, uint32_t n_samples);
  static void activate(LV2_Handle instance);
  static void connect_port(LV2_Handle instance, uint32_t port, void* data);
  static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                                double rate, const char* bundle_path,
                                const LV2_Feature* const* features);
  Gx_valvecaster_();
  ~Gx_valvecaster_();
};

// constructor
Gx_valvecaster_::Gx_valvecaster_() :
  output(NULL),
  input(NULL),
  valvecaster(valvecaster::plugin()),
  valvecasterbuster(valvecasterbuster::plugin()),
  bypass(0),
  bypass_(2),
  boost(0),
  boost_(2),
  needs_ramp_down(false),
  needs_ramp_up(false),
  bypassed(false),
  needs_boost_down(false),
  needs_boost_up(false),
  boost_is(false) {};

// destructor
Gx_valvecaster_::~Gx_valvecaster_()
{
  // just to be sure the plug have given free the allocated mem
  // it didn't hurd if the mem is already given free by clean_up()
  if (valvecaster->activate_plugin !=0)
    valvecaster->activate_plugin(false, valvecaster);
  if (valvecasterbuster->activate_plugin !=0)
    valvecasterbuster->activate_plugin(false, valvecasterbuster);
  // delete DSP class
  valvecaster->delete_instance(valvecaster);
  valvecasterbuster->delete_instance(valvecasterbuster);
};

///////////////////////// PRIVATE CLASS  FUNCTIONS /////////////////////

void Gx_valvecaster_::init_dsp_(uint32_t rate)
{
  fSamplingFreq = rate;
  // samplerate check
  fact = fSamplingFreq/48000;
  if (fact>1) {
    smp.setup(fSamplingFreq, fact);
    fSamplingFreq = 48000;
  }
  // set values for internal ramping
  ramp_down_step = 32 * (256 * rate) / 48000; 
  ramp_up_step = ramp_down_step;
  ramp_down = ramp_down_step;
  ramp_up = 0.0;
  boost_down = ramp_down_step;
  boost_up = 0.0;

  valvecaster->set_samplerate(rate, valvecaster); // init the DSP class
  valvecasterbuster->set_samplerate(rate, valvecasterbuster); // init the DSP class
}

// connect the Ports used by the plug-in class
void Gx_valvecaster_::connect_(uint32_t port,void* data)
{
  switch ((PortIndex)port)
    {
    case EFFECTS_OUTPUT:
      output = static_cast<float*>(data);
      break;
    case EFFECTS_INPUT:
      input = static_cast<float*>(data);
      break;
    case BYPASS: 
      bypass = static_cast<float*>(data); // , 0.0, 0.0, 1.0, 1.0 
      break;
    case BOOST: 
      boost = static_cast<float*>(data); // , 0.0, 0.0, 1.0, 1.0 
      break;
    default:
      break;
    }
}

void Gx_valvecaster_::activate_f()
{
  // allocate the internal DSP mem
  if (valvecaster->activate_plugin !=0)
    valvecaster->activate_plugin(true, valvecaster);
  if (valvecasterbuster->activate_plugin !=0)
    valvecasterbuster->activate_plugin(true, valvecasterbuster);
}

void Gx_valvecaster_::clean_up()
{
  // delete the internal DSP mem
  if (valvecaster->activate_plugin !=0)
    valvecaster->activate_plugin(false, valvecaster);
  if (valvecasterbuster->activate_plugin !=0)
    valvecasterbuster->activate_plugin(false, valvecasterbuster);
}

void Gx_valvecaster_::deactivate_f()
{
  // delete the internal DSP mem
  if (valvecaster->activate_plugin !=0)
    valvecaster->activate_plugin(false, valvecaster);
  if (valvecasterbuster->activate_plugin !=0)
    valvecasterbuster->activate_plugin(false, valvecasterbuster);
}

void Gx_valvecaster_::run_dsp_(uint32_t n_samples)
{
  uint32_t ReCount = n_samples;
  if (fact>1) {
    ReCount = n_samples/fact ;
  }
  FAUSTFLOAT buf[ReCount];
  if (fact>1) {
     smp.down(ReCount, input, buf);
  } else {
    memcpy(buf, input, n_samples*sizeof(float));
  }
  FAUSTFLOAT buf1[ReCount];
  if (fact>1) {
     smp.down(ReCount, input, buf1);
  } else {
    memcpy(buf1, input, n_samples*sizeof(float));
  }
  // do inplace processing at default
  // check if booster is enabled
  if (boost_ != static_cast<uint32_t>(*(boost))) {
    boost_ = static_cast<uint32_t>(*(boost));
    if (boost_) {
      needs_boost_up = true;
      needs_boost_down = false;
    } else {
      needs_boost_down = true;
      needs_boost_up = false;
    }
  }
  // check if bypass is pressed
  if (bypass_ != static_cast<uint32_t>(*(bypass))) {
    bypass_ = static_cast<uint32_t>(*(bypass));
    // ramp_down = ramp_down_step;
    // ramp_up = 0.0;
    if (!bypass_) {
      needs_ramp_down = true;
      needs_ramp_up = false;
    } else {
      needs_ramp_up = true;
      needs_ramp_down = false;
    }  
  }
  if (!bypassed) {
    if (boost_is) valvecasterbuster->mono_audio(static_cast<int>(ReCount), buf, buf, valvecasterbuster);

    if (needs_boost_down) {
      float fade = 0;
      for (uint32_t i=0; i<ReCount; i++) {
        if (boost_down >= 0.0) {
          --boost_down; 
        }
        fade = max(0.0,boost_down) /ramp_down_step ;
        buf[i] = buf[i] * fade + buf1[i] * (1.0 - fade);
      }

      if (boost_down <= 0.0) {
        // when ramped down, clear buffer from valvecasterbuster class
        valvecasterbuster->clear_state(valvecasterbuster);
        needs_boost_down = false;
        boost_is = false;
        boost_down = ramp_down_step;
        boost_up = 0.0;
      } else {
        boost_up = boost_down;
      }

    } else if (needs_boost_up) {
      boost_is = true;
      float fade = 0;
      for (uint32_t i=0; i<ReCount; i++) {
        if (boost_up < ramp_up_step) {
          ++boost_up ;
        }
        fade = min(8192.0,boost_up) /ramp_up_step ;
        buf[i] = buf[i] * fade + buf1[i] * (1.0 - fade);
      }

      if (boost_up >= ramp_up_step) {
        needs_boost_up = false;
        boost_up = 0.0;
        boost_down = ramp_down_step;
      } else {
        boost_down = boost_up;
      }
    }

    valvecaster->mono_audio(static_cast<int>(ReCount), buf, buf, valvecaster);

  }
  // check if ramping is needed
  if (needs_ramp_down) {

    FAUSTFLOAT buf2[ReCount];
    if (fact>1) {
       smp.down(ReCount, input, buf2);
    } else {
      memcpy(buf2, input, n_samples*sizeof(float));
    }

    float fade = 0;
    for (uint32_t i=0; i<ReCount; i++) {
      if (ramp_down >= 0.0) {
        --ramp_down; 
      }
      fade = max(0.0,ramp_down) /ramp_down_step ;
      buf[i] = buf[i] * fade + buf2[i] * (1.0 - fade);
    }

    if (ramp_down <= 0.0) {
      // when ramped down, clear buffer from valvecaster class
      valvecaster->clear_state(valvecaster);
      valvecasterbuster->clear_state(valvecasterbuster);
      needs_ramp_down = false;
      bypassed = true;
      ramp_down = ramp_down_step;
      ramp_up = 0.0;
    } else {
      ramp_up = ramp_down;
    }

  } else if (needs_ramp_up) {

    bypassed = false;
    FAUSTFLOAT buf2[ReCount];
    if (fact>1) {
       smp.down(ReCount, input, buf2);
    } else {
      memcpy(buf2, input, n_samples*sizeof(float));
    }

    float fade = 0;
    for (uint32_t i=0; i<ReCount; i++) {
      if (ramp_up < ramp_up_step) {
        ++ramp_up ;
      }
      fade = min(8192.0,ramp_up) /ramp_up_step ;
      buf[i] = buf[i] * fade + buf2[i] * (1.0 - fade);
    }

    if (ramp_up >= ramp_up_step) {
      needs_ramp_up = false;
      ramp_up = 0.0;
      ramp_down = ramp_down_step;
    } else {
      ramp_down = ramp_up;
    }
  }

  if (fact>1) {
    smp.up(ReCount, buf, output);
  } else {
    memcpy(output, buf, n_samples*sizeof(float));
  }
}

void Gx_valvecaster_::connect_all__ports(uint32_t port, void* data)
{
  // connect the Ports used by the plug-in class
  connect_(port,data); 
  // connect the Ports used by the DSP class
  valvecaster->connect_ports(port,  data, valvecaster);
  valvecasterbuster->connect_ports(port,  data, valvecasterbuster);
}

////////////////////// STATIC CLASS  FUNCTIONS  ////////////////////////

LV2_Handle 
Gx_valvecaster_::instantiate(const LV2_Descriptor* descriptor,
                            double rate, const char* bundle_path,
                            const LV2_Feature* const* features)
{
  // init the plug-in class
  Gx_valvecaster_ *self = new Gx_valvecaster_();
  if (!self) {
    return NULL;
  }

  self->init_dsp_((uint32_t)rate);

  return (LV2_Handle)self;
}

void Gx_valvecaster_::connect_port(LV2_Handle instance, 
                                   uint32_t port, void* data)
{
  // connect all ports
  static_cast<Gx_valvecaster_*>(instance)->connect_all__ports(port, data);
}

void Gx_valvecaster_::activate(LV2_Handle instance)
{
  // allocate needed mem
  static_cast<Gx_valvecaster_*>(instance)->activate_f();
}

void Gx_valvecaster_::run(LV2_Handle instance, uint32_t n_samples)
{
  // run dsp
  static_cast<Gx_valvecaster_*>(instance)->run_dsp_(n_samples);
}

void Gx_valvecaster_::deactivate(LV2_Handle instance)
{
  // free allocated mem
  static_cast<Gx_valvecaster_*>(instance)->deactivate_f();
}

void Gx_valvecaster_::cleanup(LV2_Handle instance)
{
  // well, clean up after us
  Gx_valvecaster_* self = static_cast<Gx_valvecaster_*>(instance);
  self->clean_up();
  delete self;
}

const LV2_Descriptor Gx_valvecaster_::descriptor =
{
  GXPLUGIN_URI "#_valvecaster_",
  Gx_valvecaster_::instantiate,
  Gx_valvecaster_::connect_port,
  Gx_valvecaster_::activate,
  Gx_valvecaster_::run,
  Gx_valvecaster_::deactivate,
  Gx_valvecaster_::cleanup,
  NULL
};


} // end namespace valvecaster

////////////////////////// LV2 SYMBOL EXPORT ///////////////////////////

extern "C"
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
  switch (index)
    {
    case 0:
      return &valvecaster::Gx_valvecaster_::descriptor;
    default:
      return NULL;
    }
}

///////////////////////////// FIN //////////////////////////////////////
