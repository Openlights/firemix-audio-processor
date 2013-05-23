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

#include "jack_client.h"


JackClient::JackClient()
{
  const char **ports;
  const char *client_name = "firemix-audio-processor";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;

  _active = false;

  _client = jack_client_open(client_name, options, &status, server_name);

  if (_client == NULL)
  {
      qDebug() << "Unable to create JACK client.";
      if (status & JackServerFailed)
      {
          qDebug() << "Unable to connect to JACK server.";
      }
      return;
  }

  if (status & JackServerStarted)
  {
      qDebug() << "JACK server started.";
  }

  qDebug() << "Connected to JACK server";

  if (status & JackNameNotUnique)
  {
      qDebug() << "Unique name" << client_name << "assigned.";
  }

  jack_set_process_callback(_client, _process, this);
  jack_on_shutdown(_client, _jack_client_shutdown, this);

  _input_port = jack_port_register(_client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  if (_input_port == NULL)
  {
      qDebug() << "No JACK ports available";
      return;
  }

  if (jack_activate(_client))
  {
      qDebug() << "Cannot activate JACK client.";
      return;
  }

  ports = jack_get_ports(_client, NULL, NULL, JackPortIsPhysical | JackPortIsOutput);

  if (ports == NULL)
  {
      qDebug() << "No physical capture ports available.";
      return;
  }

  if (jack_connect(_client, ports[0], jack_port_name(_input_port)))
  {
      qDebug() << "Cannot connect input port.";
      return;
  }

  qDebug("Registered JACK input port.  Listening at %d kHz", jack_get_sample_rate(_client));

  free (ports);
  _active = true;
}


JackClient::~JackClient()
{
  qDebug() << "Shutting down JACK client";
  jack_client_close(_client);
}


void JackClient::_jack_client_shutdown(void* arg)
{
  JackClient* instance = (JackClient*)arg; 
  return instance->jack_client_shutdown(); 
}


void JackClient::jack_client_shutdown()
{
  qDebug() << "JACK shutdown";
}


int JackClient::_process(jack_nframes_t nframes, void* arg) { 
  JackClient* instance = (JackClient*)arg; 
  return instance->process(nframes); 
}


int JackClient::process(jack_nframes_t nframes) { 
  jack_default_audio_sample_t *in;

  in = (jack_default_audio_sample_t*)jack_port_get_buffer(_input_port, nframes);

  if(nframes < BUF_SIZE)
  {
      qDebug("Warning: Capturing less than %d frames into the buffer in process()!", BUF_SIZE);
      memset(_buffer, 0, sizeof(_buffer));
      memcpy(_buffer, in, sizeof(sample_t) * nframes);
  }
  else
  {
      memcpy(_buffer, in, sizeof(sample_t) * BUF_SIZE);
  }

  float avg = 0.0;
  for (int i = 0; i < BUF_SIZE; i++)
  {
    avg += fabs(_buffer[i]);
  }

  avg /= BUF_SIZE;

  if (avg > 0.1)
  {
    qDebug("%f", avg);
  }

  return 0;
}



#ifdef OLD


#if HAVE_AUBIO_DOUBLE
#define AUBIO_JACK_MAX_FRAMES 4096
#define AUBIO_JACK_NEEDS_CONVERSION
#endif

#define RINGBUFFER_SIZE 1024*sizeof(jack_midi_event_t)





/**
 * jack device structure 
 */
struct _aubio_jack_t
{
  /** jack client */
  jack_client_t *client;
  /** jack output ports */
  jack_port_t **oports;
  /** jack input ports */
  jack_port_t **iports;
  /** jack input buffer */
  jack_sample_t **ibufs;
  /** jack output buffer */
  jack_sample_t **obufs;
#ifdef AUBIO_JACK_NEEDS_CONVERSION
  /** converted jack input buffer */
  smpl_t **sibufs;
  /** converted jack output buffer */
  smpl_t **sobufs;
#endif
  /** jack input audio channels */
  uint_t ichan;
  /** jack output audio channels */
  uint_t ochan;
  /** jack input midi channels */
  uint_t imidichan;
  /** jack output midi channels */
  uint_t omidichan;
  /** midi output ringbuffer */
  jack_ringbuffer_t *midi_out_ring;
  /** jack samplerate (Hz) */
  uint_t samplerate;
  /** jack processing function */
  aubio_process_func_t callback;
};

/* static memory management */
static aubio_jack_t *aubio_jack_alloc (uint_t ichan, uint_t ochan,
    uint_t imidichan, uint_t omidichan);
static uint_t aubio_jack_free (aubio_jack_t * jack_setup);
/* jack callback functions */
static int aubio_jack_process (jack_nframes_t nframes, void *arg);
static void aubio_jack_shutdown (void *arg);

aubio_jack_t *
new_aubio_jack (uint_t ichan, uint_t ochan,
    uint_t imidichan, uint_t omidichan, aubio_process_func_t callback)
{
  aubio_jack_t *jack_setup = aubio_jack_alloc (ichan, ochan,
      imidichan, omidichan);
  uint_t i;
  char *client_name = "aubio";
  char *jack_port_type;
  char name[64];
  /* initial jack client setup */
  if ((jack_setup->client = jack_client_new (client_name)) == 0) {
    AUBIO_ERR ("jack server not running?\n");
    AUBIO_QUIT (AUBIO_FAIL);
  }

  if (jack_setup->omidichan) {
    jack_setup->midi_out_ring = jack_ringbuffer_create (RINGBUFFER_SIZE);

    if (jack_setup->midi_out_ring == NULL) {
      AUBIO_ERR ("Failed creating jack midi output ringbuffer.");
      AUBIO_QUIT (AUBIO_FAIL);
    }

    jack_ringbuffer_mlock (jack_setup->midi_out_ring);
  }

  /* set callbacks */
  jack_set_process_callback (jack_setup->client, aubio_jack_process,
      (void *) jack_setup);
  jack_on_shutdown (jack_setup->client, aubio_jack_shutdown,
      (void *) jack_setup);

  /* register jack output audio and midi ports */
  for (i = 0; i < ochan + omidichan; i++) {
    if (i < ochan) {
      jack_port_type = JACK_DEFAULT_AUDIO_TYPE;
      AUBIO_SPRINTF (name, "out_%d", i + 1);
    } else {
      jack_port_type = JACK_DEFAULT_MIDI_TYPE;
      AUBIO_SPRINTF (name, "midi_out_%d", i - ochan + 1);
    }
    if ((jack_setup->oports[i] =
            jack_port_register (jack_setup->client, name,
                jack_port_type, JackPortIsOutput, 0)) == 0) {
      goto beach;
    }
    AUBIO_DBG ("%s:%s\n", client_name, name);
  }

  /* register jack input audio ports */
  for (i = 0; i < ichan + imidichan; i++) {
    if (i < ichan) {
      jack_port_type = JACK_DEFAULT_AUDIO_TYPE;
      AUBIO_SPRINTF (name, "in_%d", i + 1);
    } else {
      jack_port_type = JACK_DEFAULT_MIDI_TYPE;
      AUBIO_SPRINTF (name, "midi_in_%d", i - ichan + 1);
    }
    if ((jack_setup->iports[i] =
            jack_port_register (jack_setup->client, name,
                jack_port_type, JackPortIsInput, 0)) == 0) {
      goto beach;
    }
    AUBIO_DBG ("%s:%s\n", client_name, name);
  }

  /* set processing callback */
  jack_setup->callback = callback;
  return jack_setup;

beach:
  AUBIO_ERR ("failed registering port \"%s:%s\"!\n", client_name, name);
  jack_client_close (jack_setup->client);
  AUBIO_QUIT (AUBIO_FAIL);
}

uint_t
aubio_jack_activate (aubio_jack_t * jack_setup)
{
  /* get sample rate */
  jack_setup->samplerate = jack_get_sample_rate (jack_setup->client);
  /* actual jack process activation */
  if (jack_activate (jack_setup->client)) {
    AUBIO_ERR ("jack client activation failed");
    return 1;
  }
  return 0;
}

void
aubio_jack_close (aubio_jack_t * jack_setup)
{
  /* bug : should disconnect all ports first */
  jack_client_close (jack_setup->client);
  aubio_jack_free (jack_setup);
}

/* memory management */
static aubio_jack_t *
aubio_jack_alloc (uint_t ichan, uint_t ochan,
    uint_t imidichan, uint_t omidichan)
{
  aubio_jack_t *jack_setup = AUBIO_NEW (aubio_jack_t);
  jack_setup->ichan = ichan;
  jack_setup->ochan = ochan;
  jack_setup->imidichan = imidichan;
  jack_setup->omidichan = omidichan;
  jack_setup->oports = AUBIO_ARRAY (jack_port_t *, ichan + imidichan);
  jack_setup->iports = AUBIO_ARRAY (jack_port_t *, ochan + omidichan);
  jack_setup->ibufs = AUBIO_ARRAY (jack_sample_t *, ichan);
  jack_setup->obufs = AUBIO_ARRAY (jack_sample_t *, ochan);
#ifdef AUBIO_JACK_NEEDS_CONVERSION
  /* allocate arrays for data conversion */
  jack_setup->sibufs = AUBIO_ARRAY (smpl_t *, ichan);
  uint_t i;
  for (i = 0; i < ichan; i++) {
    jack_setup->sibufs[i] = AUBIO_ARRAY (smpl_t, AUBIO_JACK_MAX_FRAMES);
  }
  jack_setup->sobufs = AUBIO_ARRAY (smpl_t *, ochan);
  for (i = 0; i < ochan; i++) {
    jack_setup->sobufs[i] = AUBIO_ARRAY (smpl_t, AUBIO_JACK_MAX_FRAMES);
  }
#endif
  return jack_setup;
}

static uint_t
aubio_jack_free (aubio_jack_t * jack_setup)
{
  if (jack_setup->omidichan && jack_setup->midi_out_ring) {
    jack_ringbuffer_free (jack_setup->midi_out_ring);
  }
  AUBIO_FREE (jack_setup->oports);
  AUBIO_FREE (jack_setup->iports);
  AUBIO_FREE (jack_setup->ibufs);
  AUBIO_FREE (jack_setup->obufs);
  AUBIO_FREE (jack_setup);
  return AUBIO_OK;
}

/* jack callback functions */
static void
aubio_jack_shutdown (void *arg UNUSED)
{
  AUBIO_ERR ("jack shutdown\n");
  AUBIO_QUIT (AUBIO_OK);
}

static void process_midi_output (aubio_jack_t * dev, jack_nframes_t nframes);

static int
aubio_jack_process (jack_nframes_t nframes, void *arg)
{
  aubio_jack_t *dev = (aubio_jack_t *) arg;
  uint_t i;
  for (i = 0; i < dev->ichan; i++) {
    /* get readable input */
    dev->ibufs[i] =
        (jack_sample_t *) jack_port_get_buffer (dev->iports[i], nframes);
  }
  for (i = 0; i < dev->ochan; i++) {
    /* get writable output */
    dev->obufs[i] =
        (jack_sample_t *) jack_port_get_buffer (dev->oports[i], nframes);
  }
#ifndef AUBIO_JACK_NEEDS_CONVERSION
  dev->callback (dev->ibufs, dev->obufs, nframes);
#else
  uint_t j;
  for (j = 0; j < MIN (nframes, AUBIO_JACK_MAX_FRAMES); j++) {
    for (i = 0; i < dev->ichan; i++) {
      dev->sibufs[i][j] = (smpl_t) dev->ibufs[i][j];
    }
  }
  dev->callback (dev->sibufs, dev->sobufs, nframes);
  for (j = 0; j < MIN (nframes, AUBIO_JACK_MAX_FRAMES); j++) {
    for (i = 0; i < dev->ochan; i++) {
      dev->obufs[i][j] = (jack_sample_t) dev->sobufs[i][j];
    }
  }
#endif

  /* now process midi stuff */
  if (dev->omidichan) {
    process_midi_output (dev, nframes);
  }

  return 0;
}

void
aubio_jack_midi_event_write (aubio_jack_t * dev, jack_midi_event_t * event)
{
  int written;

  if (jack_ringbuffer_write_space (dev->midi_out_ring) < sizeof (*event)) {
    AUBIO_ERR ("Not enough space to write midi output, midi event lost!\n");
    return;
  }

  written = jack_ringbuffer_write (dev->midi_out_ring,
      (char *) event, sizeof (*event));

  if (written != sizeof (*event)) {
    AUBIO_WRN ("Call to jack_ringbuffer_write failed, midi event lost! \n");
  }
}

static void
process_midi_output (aubio_jack_t * dev, jack_nframes_t nframes)
{
  int read, sendtime;
  jack_midi_event_t ev;
  unsigned char *buffer;
  jack_nframes_t last_frame_time = jack_last_frame_time (dev->client);
  // TODO for each omidichan
  void *port_buffer = jack_port_get_buffer (dev->oports[dev->ochan], nframes);

  if (port_buffer == NULL) {
    AUBIO_WRN ("Failed to get jack midi output port, will not send anything\n");
    return;
  }

  jack_midi_clear_buffer (port_buffer);

  // TODO add rate_limit

  while (jack_ringbuffer_read_space (dev->midi_out_ring)) {
    read = jack_ringbuffer_peek (dev->midi_out_ring, (char *) &ev, sizeof (ev));

    if (read != sizeof (ev)) {
      AUBIO_WRN ("Short read from the ringbuffer, possible note loss.\n");
      jack_ringbuffer_read_advance (dev->midi_out_ring, read);
      continue;
    }

    sendtime = ev.time + nframes - last_frame_time;

    /* send time is after current period, will do this one later */
    if (sendtime >= (int) nframes) {
      break;
    }

    if (sendtime < 0) {
      sendtime = 0;
    }

    jack_ringbuffer_read_advance (dev->midi_out_ring, sizeof (ev));

    buffer = jack_midi_event_reserve (port_buffer, sendtime, ev.size);

    if (buffer == NULL) {
      AUBIO_WRN ("Call to jack_midi_event_reserve failed, note lost.\n");
      break;
    }

    AUBIO_MEMCPY (buffer, ev.buffer, ev.size);
  }
}

#endif