#ifndef BELA_MIDI_H_
#define BELA_MIDI_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void* BelaMidi;

int bela_midi_available_messages(BelaMidi belaMidi);
unsigned int bela_midi_get_message(BelaMidi belaMidi, unsigned char* buf);
BelaMidi bela_midi_new(const char* port);
void bela_midi_free(BelaMidi belaMidi);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif /* BELA_MIDI_H_ */
