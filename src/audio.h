#ifndef AUDIO_H
#define AUDIO_H

void audio_init(void);
void audio_deinit(void);
int audio_isInitialized(void);
int audio_lockMixer(void);
void audio_unlockMixer(int wasEnabled);

#endif
