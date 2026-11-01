#ifndef MC_FIGHTER_AUDIO_H
#define MC_FIGHTER_AUDIO_H
void audio_init(void);
void audio_quit(void);
void audio_music(const char *name);
void audio_stop_music(void);
void audio_play(const char *name);
void audio_voice(const char *name);
#endif
