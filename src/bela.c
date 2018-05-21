/*
   Copyright 2007-2016 David Robillard <http://drobilla.net>

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <Bela.h>
#include <Midi_c.h>
#include <stdio.h>
#include <math.h>
#define M_PI 3.14159265358979323846

#include "jalv_internal.h"
#include "worker.h"

// playback_file stuff
#include <SampleLoader.h>
#include <SampleData.h>

#define NUM_CHANNELS 1

struct SampleData gSampleData[NUM_CHANNELS];

int gReadPtr;	// Position of last read sample from file
int gDonePlaying;

// playback_file stuff end
struct JalvBackend {
};

Midi* midi;
bool midiEnabled = true;
const char* gMidiPort0 = "hw:0,0,0";

// audio callback
void render(BelaContext* context, void* arg){
	Jalv* jalv = (Jalv*)arg;
	unsigned int inChannels = context->audioInChannels;
	unsigned int outChannels = context->audioOutChannels;
	unsigned int frames = context->audioFrames;
	float inData[frames * inChannels];
	float outData[frames * outChannels];

	//deinterleave
	float* inputs[inChannels];
	for(unsigned int j = 0; j < inChannels; ++j){
		inputs[j] = &inData[frames * j];
		for(unsigned int n = 0; n < frames; ++n){
			float playbackValue = 0;
			if(!gDonePlaying && j < NUM_CHANNELS)
			{
				if(gSampleData[j].samples)
				{
					playbackValue = gSampleData[j].samples[gReadPtr];
				}
				if(gSampleData[j].sampleLen == gReadPtr)
				{
					gReadPtr = 0;
					if(!jalv->opts.playback_loop)
					{
						gDonePlaying = 1;
					}
				}
				//printf("%d %d %d\n", gReadPtr, gSampleData[j].sampleLen, j);
				if(j == NUM_CHANNELS - 1)
				{
					++gReadPtr;
				}
			}
			inData[n + j * frames] = audioRead(context, n, j) + playbackValue;
		}
	}

	float* outputs[outChannels];
	
	for(int j = 0; j < outChannels; ++j){
		outputs[j] = &outData[frames * j];
		for(unsigned int n = 0; n < frames; ++n){
			outData[n + j * frames] = 0;
		}
	}

	/* Prepare port buffers */
	uint32_t in_index  = 0;
	uint32_t out_index = 0;
	for (uint32_t i = 0; i < jalv->num_ports; ++i) {
		struct Port* port = &jalv->ports[i];
		if (port->type == TYPE_AUDIO) {
			if (port->flow == FLOW_INPUT) {
				lilv_instance_connect_port(jalv->instance, i, ((float**)inputs)[in_index++]);
			} else if (port->flow == FLOW_OUTPUT) {
				lilv_instance_connect_port(jalv->instance, i, ((float**)outputs)[out_index++]);
			}
		} else if (port->type == TYPE_EVENT && port->flow == FLOW_INPUT) {
			lv2_evbuf_reset(port->evbuf, true);
	
	int availableMidiMessages = 0;
	if(midiEnabled)
	if(midi != NULL){
		if((availableMidiMessages = Midi_availableMessages(midi)) > 0)
			jalv->request_update = true;
	}

#undef AUTO_NOTES
#ifdef AUTO_NOTES
	static int count = 0;
	static bool status = 0;
	int notes = 1;
	int note = 60;
	int targetCount = 44100 / frames * 0.5;
	if(count % targetCount == 0){
		status = !status;
		jalv->request_update = true;
	}
	count++;
#endif
			if (jalv->request_update) {
				/* Plugin state has changed, request an update */
				const LV2_Atom_Object get = {
					{ sizeof(LV2_Atom_Object_Body), jalv->urids.atom_Object },
					{ 0, jalv->urids.patch_Get } };
				LV2_Evbuf_Iterator iter = lv2_evbuf_begin(port->evbuf);
				if(midiEnabled)
				while((availableMidiMessages = Midi_availableMessages(midi)) > 0){
					unsigned char buf[3];
					unsigned char size = Midi_getMessage(midi, buf);
					lv2_evbuf_write(&iter,
						0, 0,
						jalv->midi_event_id,
						size, buf
					);
				}
#ifdef AUTO_NOTES
				rt_printf("event %d %#x\n", status, note);
				for(int n = 0; n < notes; ++n){
					char buf[] = {
						0x80 + status * 16, note +  1 * n , 36,
					};
					lv2_evbuf_write(&iter,
									0, 0,
									jalv->midi_event_id,
									3, buf);
				}
#endif
				lv2_evbuf_write(&iter, 0, 0,
				                get.atom.type, get.atom.size,
				                (const uint8_t*)LV2_ATOM_BODY(&get));
			}
		} else if (port->type == TYPE_EVENT) {
			/* Clear event output for plugin to write to */
			lv2_evbuf_reset(port->evbuf, false);
		}
	}
	jalv->request_update = false;

	/* Run plugin for this cycle */
	const bool send_ui_updates = jalv_run(jalv, frames);

	// deinterleave
	int clip = 0;
	for(unsigned int j = 0; j < outChannels; ++j){
		inputs[j] = &inData[frames * j];
		for(unsigned int n = 0; n < frames; ++n){
			float value = outData[n + j * frames];
			if(value >= 1 || value < -1)
				clip = 1;
			audioWrite(context, n, j, value);
		}
	}
	// clipping indicator
	static int count = 0;
	if(clip == 1)
	{
		if(count == 0){
			rt_printf("clipping\n");
		}
		++count;
		if(count > 10)
			count = 0;
	}
	/* Deliver UI events */
	for (uint32_t p = 0; p < jalv->num_ports; ++p) {
		struct Port* const port = &jalv->ports[p];
		if (port->flow == FLOW_OUTPUT && port->type == TYPE_EVENT) {
			for (LV2_Evbuf_Iterator i = lv2_evbuf_begin(port->evbuf);
			     lv2_evbuf_is_valid(i);
			     i = lv2_evbuf_next(i)) {
				// Get event from LV2 buffer
				uint32_t frames, subframes, type, size;
				uint8_t* body;
				lv2_evbuf_get(i, &frames, &subframes, &type, &size, &body);

				if (jalv->has_ui && !port->old_api) {
					// Forward event to UI
					jalv_send_to_ui(jalv, p, type, size, body);
				}
			}
		} else if (send_ui_updates &&
		           port->flow == FLOW_OUTPUT && port->type == TYPE_CONTROL) {
			char buf[sizeof(ControlChange) + sizeof(float)];
			ControlChange* ev = (ControlChange*)buf;
			ev->index    = p;
			ev->protocol = 0;
			ev->size     = sizeof(float);
			*(float*)ev->body = port->control;
			if (zix_ring_write(jalv->plugin_events, buf, sizeof(buf))
			    < sizeof(buf)) {
				fprintf(stderr, "Plugin => UI buffer overflow!\n");
			}
		}
	}
}

bool setup(BelaContext* context, void* arg){
	static bool midiInit = false;
	if(midiEnabled && !midiInit){
		midi = Midi_new(gMidiPort0);
		midiInit = true;
	}
	Jalv* jalv = (Jalv*)arg;
	// Set audio parameters
	jalv->sample_rate = context->audioSampleRate;
	jalv->block_length  = context->audioFrames;

	char* filename = jalv->opts.playback_filename;

	if(filename)
	{
		for(int ch=0;ch<NUM_CHANNELS;ch++) {
			int sampleLen = getNumFrames(filename);
			gSampleData[ch].sampleLen = sampleLen;
			gSampleData[ch].samples = (float*)malloc(sizeof(float) * sampleLen);
			getSamples(filename, gSampleData[ch].samples, ch, 0, sampleLen);
			printf("loading %d samples from %s into channel %d\n", gSampleData[ch].sampleLen, filename, ch);
		}
		gReadPtr = 0;
		gDonePlaying = 0;
	} else {
		for(int ch=0;ch<NUM_CHANNELS;ch++) {
			gSampleData[ch].sampleLen = 0;
			gSampleData[ch].samples = NULL;
		}
	}

    return true;
}

void cleanup(BelaContext* context, void* arg){
	if(midiEnabled)
		Midi_delete(midi);

    for(int ch=0;ch<NUM_CHANNELS;ch++)
	{
		if(gSampleData[ch].samples)
			free(gSampleData[ch].samples);
	}
}

JalvBackend*
jalv_backend_init(Jalv* jalv)
{
	BelaInitSettings settings;	// Standard audio settings
	// Set default settings
	Bela_defaultSettings(&settings);
	settings.pruNumber = 0;
	settings.useDigital = 0;
	settings.periodSize = 64;
	settings.headphoneLevel = 0;
	settings.numDigitalChannels = 0;
	settings.useDigital = 0;
	settings.useAnalog = 0;
	settings.render = render;
	settings.setup = setup;
	settings.cleanup = cleanup;
	settings.highPerformanceMode = 1;
	if(Bela_initAudio(&settings, jalv) != 0) {
		fprintf(stderr, "Error: unable to initialise audio\n");
		return NULL;
	}


	// Count number of input and output audio ports/channels
	int inChannels = 0;
	int outChannels = 0;
	for (uint32_t i = 0; i < jalv->num_ports; ++i) {
		if (jalv->ports[i].type == TYPE_AUDIO) {
			if (jalv->ports[i].flow == FLOW_INPUT) {
				++inChannels;
			} else if (jalv->ports[i].flow == FLOW_OUTPUT) {
				++outChannels;
			}
		}
	}
	printf("Plugin requires %d inputs and %d outputs\n", inChannels, outChannels);

	jalv->midi_buf_size = 4096;

	// Allocate and return opaque backend
	JalvBackend* backend = (JalvBackend*)calloc(1, sizeof(JalvBackend));
	//backend->stream = stream;

	return backend;
}

void
jalv_backend_close(Jalv* jalv)
{
	// Clean up any resources allocated for audio
	Bela_cleanupAudio();

	free(jalv->backend);
	jalv->backend = NULL;
}

void
jalv_backend_activate(Jalv* jalv)
{
	if(Bela_startAudio()) {
		fprintf(stderr, "Error: unable to start real-time audio\n");
		// Stop the audio device
		Bela_stopAudio();
		// Clean up any resources allocated for audio
		Bela_cleanupAudio();
		return;
	}
}


void
jalv_backend_deactivate(Jalv* jalv)
{
	// Stop the audio device
	Bela_stopAudio();
}

void
jalv_backend_activate_port(Jalv* jalv, uint32_t port_index)
{
	struct Port* const port = &jalv->ports[port_index];
	switch (port->type) {
	case TYPE_CONTROL:
		lilv_instance_connect_port(jalv->instance, port_index, &port->control);
		break;
	default:
		break;
	}
}
