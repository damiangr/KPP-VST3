/*
 * Copyright (C) 2018-2020 Oleg Kapitonov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

//-----------------------------------------------------------------------------
// LICENSE
// (c) 2019, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "../include/plugprocessor.h"
#include "../include/plugids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace Steinberg {
namespace Vst {

  PlugProcessor::PlugProcessor ()
  {
    setControllerClass (MyControllerUID);
    dsp = nullptr;
    ui = nullptr;
  }

  tresult PLUGIN_API PlugProcessor::initialize (FUnknown* context)
  {
    tresult result = AudioEffect::initialize (context);
    if (result != kResultTrue)
      return kResultFalse;

    addAudioInput (STR16 ("AudioInput"), Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("AudioOutput"), Vst::SpeakerArr::kStereo);

    mBass = 0.5;
    mMiddle = 0.5;
    mTreble = 0.5;
    mGain = 0.5;
    mVolume = 0.5;
    mVoice = 0.5;
    mBypass = false;

    return kResultTrue;
  }

  tresult PLUGIN_API PlugProcessor::setBusArrangements (Vst::SpeakerArrangement* inputs,
                                                        int32 numIns,
                                                        Vst::SpeakerArrangement* outputs,
                                                        int32 numOuts)
  {
    if (numIns == 1 && numOuts == 1 && inputs[0] == outputs[0])
    {
      return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
    }
    return kResultFalse;
  }

  tresult PLUGIN_API PlugProcessor::setupProcessing (Vst::ProcessSetup& setup)
  {
    sampleRate = setup.sampleRate;
    return AudioEffect::setupProcessing (setup);
  }

  tresult PLUGIN_API PlugProcessor::setActive (TBool state)
  {
    if (state)
    {
      dsp = new DistructionDsp();
      ui = new UI();
      dsp->init(sampleRate);
      dsp->buildUserInterface(ui);
    }
    else
    {
      if (dsp != nullptr)
      {
        delete dsp;
        dsp = nullptr;
      }
      if (ui != nullptr)
      {
        delete ui;
        ui = nullptr;
      }
    }
    return AudioEffect::setActive (state);
  }

  tresult PLUGIN_API PlugProcessor::process (ProcessData& data)
  {
    if (data.inputParameterChanges)
    {
      int32 numParamsChanged = data.inputParameterChanges->getParameterCount ();
      for (int32 index = 0; index < numParamsChanged; index++)
      {
        IParamValueQueue* paramQueue =
        data.inputParameterChanges->getParameterData (index);
        if (paramQueue)
        {
          ParamValue value;
          int32 sampleOffset;
          int32 numPoints = paramQueue->getPointCount ();
          switch (paramQueue->getParameterId ())
          {
            case kBassId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
              {
                mBass = value;
                ui->setBassValue((value * 2.0 - 1.0) * 15.0);
              }
              break;
            case kMiddleId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
              {
                mMiddle = value;
                ui->setMiddleValue((value * 2.0 - 1.0) * 15.0);
              }
              break;
            case kTrebleId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
              {
                mTreble = value;
                ui->setTrebleValue((value * 2.0 - 1.0) * 15.0);
              }
              break;
            case kGainId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
              {
                mGain = value;
                ui->setGainValue(value * 100.0);
              }
              break;
            case kVolumeId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
              {
                mVolume = value;
                ui->setVolumeValue(value);
              }
              break;
            case kVoiceId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
              {
                mVoice = value;
                ui->setVoiceValue(value);
              }
              break;
            case kBypassId:
              if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) ==
                kResultTrue)
                mBypass = (value > 0.5f);
              break;
          }
        }
      }
    }

    if (data.numInputs == 0 || data.numOutputs == 0)
    {
      return kResultOk;
    }

    if (data.numSamples > 0)
    {
      SpeakerArrangement arr;
      getBusArrangement(kOutput, 0, arr);
      int32 numChannels = SpeakerArr::getChannelCount(arr);

      float* inputs[2];
      float* outputs[2];

      if (numChannels >= 2)
      {
        inputs[0] = data.inputs[0].channelBuffers32[0];
        inputs[1] = data.inputs[0].channelBuffers32[1];

        outputs[0] = data.outputs[0].channelBuffers32[0];
        outputs[1] = data.outputs[0].channelBuffers32[1];
      }
      else
      {
        inputs[0] = data.inputs[0].channelBuffers32[0];
        inputs[1] = data.inputs[0].channelBuffers32[0];

        outputs[0] = data.outputs[0].channelBuffers32[0];
        outputs[1] = data.outputs[0].channelBuffers32[0];
      }

      if (!mBypass)
      {
        dsp->compute(data.numSamples, inputs, outputs);
      }
      else
      {
        for (int i = 0; i < data.numSamples; i++)
        {
          outputs[0][i] = inputs[0][i];
          outputs[1][i] = inputs[1][i];
        }
      }
    }
    return kResultOk;
  }

  tresult PLUGIN_API PlugProcessor::setState (IBStream* state)
  {
    if (!state)
      return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);

    float savedBass = 0.f;
    if (streamer.readFloat (savedBass) == false)
      return kResultFalse;

    float savedMiddle = 0.f;
    if (streamer.readFloat (savedMiddle) == false)
      return kResultFalse;

    float savedTreble = 0.f;
    if (streamer.readFloat (savedTreble) == false)
      return kResultFalse;

    float savedGain = 0.f;
    if (streamer.readFloat (savedGain) == false)
      return kResultFalse;

    float savedVolume = 0.f;
    if (streamer.readFloat (savedVolume) == false)
      return kResultFalse;

    float savedVoice = 0.f;
    if (streamer.readFloat (savedVoice) == false)
      return kResultFalse;

    int32 savedBypass = 0;
    if (streamer.readInt32 (savedBypass) == false)
      return kResultFalse;

    mBass = savedBass;
    mMiddle = savedMiddle;
    mTreble = savedTreble;
    mGain = savedGain;
    mVolume = savedVolume;
    mVoice = savedVoice;
    mBypass = savedBypass > 0;

    ui->setBassValue(mBass);
    ui->setMiddleValue(mMiddle);
    ui->setTrebleValue(mTreble);
    ui->setGainValue(mGain);
    ui->setVolumeValue(mVolume);
    ui->setVoiceValue(mVoice);

    return kResultOk;
  }

  tresult PLUGIN_API PlugProcessor::getState (IBStream* state)
  {
    float toSaveBass = mBass;
    float toSaveMiddle = mMiddle;
    float toSaveTreble = mTreble;
    float toSaveGain = mGain;
    float toSaveVolume = mVolume;
    float toSaveVoice = mVoice;
    int32 toSaveBypass = mBypass ? 1 : 0;

    IBStreamer streamer (state, kLittleEndian);
    streamer.writeFloat (toSaveBass);
    streamer.writeFloat (toSaveMiddle);
    streamer.writeFloat (toSaveTreble);
    streamer.writeFloat (toSaveGain);
    streamer.writeFloat (toSaveVolume);
    streamer.writeFloat (toSaveVoice);
    streamer.writeInt32 (toSaveBypass);

    return kResultOk;
  }

} // Vst
} // Steinberg
