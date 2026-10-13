#include <dos.h>
#include "lib/cmixer/cmixer.h"
#include "soundblaster.h"
#include "audio.h"


static short audio_buffer[SOUNDBLASTER_SAMPLES_PER_BUFFER];
static int audio_interruptsWereEnabled;
static int audio_initialized = 0;


static const short* audio_callback(void) {
  /* Called from the soundblaster's interrupt handler; cmixer outputs
  ** interleaved stereo which is exactly what the soundblaster expects */
  cm_process(audio_buffer, SOUNDBLASTER_SAMPLES_PER_BUFFER);
  return audio_buffer;
}


static void audio_lock(cm_Event *e) {
  /* The mixer runs inside the soundblaster's interrupt handler, so the main
  ** program "locks" the mixer by disabling interrupts. The previous interrupt
  ** state is restored on unlock, which keeps this a no-op when called from
  ** inside the interrupt handler itself */
  if (e->type == CM_EVENT_LOCK) {
    audio_interruptsWereEnabled = disable();
  } else if (audio_interruptsWereEnabled) {
    enable();
  }
}


void audio_init(void) {
  cm_init(soundblaster_getSampleRate());
  cm_set_lock(audio_lock);
  audio_initialized = (soundblaster_init(audio_callback) == 0);
}


void audio_deinit(void) {
  soundblaster_deinit();
  audio_initialized = 0;
}


int audio_isInitialized(void) {
  return audio_initialized;
}


int audio_lockMixer(void) {
  /* Keeps the mixer (interrupt) from running; returns the value to pass to
   * audio_unlockMixer() */
  return disable();
}


void audio_unlockMixer(int wasEnabled) {
  if (wasEnabled) enable();
}
