// FireMix-Audio-Processor
// Copyright (c) 2013 Jon Evans
// http://craftyjon.com/projects/openlights/firemix
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef _JACK_CLIENT_H
#define _JACK_CLIENT_H

#include <math.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <aubio/aubio.h>

#define BUF_SIZE 256
#define HOP_SIZE 128

typedef jack_default_audio_sample_t sample_t;


class JackClient : public QObject
{
  Q_OBJECT

public:
  JackClient();
  ~JackClient();

  static void _jack_client_shutdown(void* arg);
  void jack_client_shutdown(void);

  static int _process(jack_nframes_t nframes, void* arg);
  int process(jack_nframes_t nframes);

private:
  jack_port_t *_input_port;
  jack_port_t *_output_port;
  jack_client_t *_client;
  bool _active;
  sample_t _buffer[BUF_SIZE];

};

#ifdef OLD
typedef jack_default_audio_sample_t jack_sample_t;

/** jack object */
typedef struct _aubio_jack_t aubio_jack_t;
/** jack process function */
typedef int (*aubio_process_func_t) (smpl_t ** input,
    smpl_t ** output, int nframes);

/** jack device creation function */
aubio_jack_t *new_aubio_jack (uint_t inchannels, uint_t outchannels,
    uint_t imidichan, uint_t omidichan,
    aubio_process_func_t callback);
/** activate jack client (run jackprocess function) */
uint_t aubio_jack_activate (aubio_jack_t * jack_setup);
/** close and delete jack client */
void aubio_jack_close (aubio_jack_t * jack_setup);

/** write a jack_midi_event_t to the midi output ringbuffer */
void aubio_jack_midi_event_write (aubio_jack_t * jack_setup,
    jack_midi_event_t * event);
#endif


#endif