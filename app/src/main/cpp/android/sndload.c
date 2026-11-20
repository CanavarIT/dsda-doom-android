/*
 * Android build: replacement for SDL/i_sndfile.c. Instead of libsndfile the
 * sound lumps (WAV/AIFF/VOC/OGG/FLAC/MP3 inside WADs) are decoded by SDL_mixer.
 */
#include "config.h"
#include "SDL.h"
#include "SDL_mixer.h"

#include "lprintf.h"
#include "z_zone.h"
#include <string.h>
#include "SDL/i_sndfile.h"

void *Load_SNDFile(const void *data, SDL_AudioSpec *sample, void **sampledata,
                   Uint32 *samplelen)
{
  SDL_RWops *rw = SDL_RWFromConstMem(data, (int) *samplelen);
  Mix_Chunk *chunk;
  int freq = 0, channels = 0;
  Uint16 format = 0;
  void *copy;

  if (!rw)
    return NULL;

  chunk = Mix_LoadWAV_RW(rw, 1);
  if (!chunk)
  {
    lprintf(LO_WARN, "Mix_LoadWAV_RW: %s\n", Mix_GetError());
    return NULL;
  }

  if (!Mix_QuerySpec(&freq, &format, &channels) || chunk->alen == 0)
  {
    Mix_FreeChunk(chunk);
    return NULL;
  }

  copy = Z_Malloc(chunk->alen);
  memcpy(copy, chunk->abuf, chunk->alen);

  sample->freq = freq;
  sample->format = format;
  sample->channels = (Uint8) channels;

  *sampledata = copy;
  *samplelen = chunk->alen;

  Mix_FreeChunk(chunk);
  return sample;
}
