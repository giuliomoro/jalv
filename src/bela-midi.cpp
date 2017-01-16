#include <Midi.h>
#include "bela-midi.h"
extern "C" {

int bela_midi_available_messages(BelaMidi belaMidi){
	Midi* midi = (Midi*)belaMidi;
	return midi->getParser()->numAvailableMessages();
}

unsigned int bela_midi_get_message(BelaMidi belaMidi, unsigned char* buf){
	Midi* midi = (Midi*)belaMidi;
	MidiChannelMessage message;
	message = midi->getParser()->getNextChannelMessage();
	//message.prettyPrint();
	buf[0] = message.getStatusByte() | message.getChannel();
	unsigned char size = message.getNumDataBytes();
	for(int n = 0; n < size; ++n){
		buf[n + 1] = message.getDataByte(n);	
	}
	return size + 1;
}

BelaMidi bela_midi_new(const char* port){
	Midi* midi = new Midi();
	if(midi != NULL){
		midi->readFrom(port);
		midi->enableParser(true);
	}
	return midi;
}

void bela_midi_free(BelaMidi belaMidi){
	Midi* midi = (Midi*)belaMidi;
	//delete midi;
}

} /* extern "C" */
