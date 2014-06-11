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

  char _onset_mode[4] = "mkl";
  smpl_t silence = -40.0;

  _active = false;

  _onset = new_aubio_onset(_onset_mode, BUF_SIZE, HOP_SIZE, _samplerate);
  aubio_onset_set_silence(_onset, silence);
  aubio_onset_set_minioi_ms(_onset, 350);
  //aubio_onset_set_threshold (_onset, threshold);


  _ibuf = NULL;
  _ibuf = new_fvec(HOP_SIZE);
  _onset_list = new_fvec(1);

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

  _samplerate = jack_get_sample_rate(_client);

  qDebug("Registered JACK input port.  Listening at %d kHz", _samplerate);

  free (ports);
  _active = true;

  if (!_ibuf)
  {
    qDebug() << "Error allocating buffer";
  }
}


JackClient::~JackClient()
{
  qDebug() << "Shutting down JACK client";
  jack_client_close(_client);
  del_aubio_onset(_onset);
  del_fvec(_ibuf);
  del_fvec(_onset_list);
  aubio_cleanup();
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
  static unsigned int pos = 0;
  jack_default_audio_sample_t *in;

  in = (jack_default_audio_sample_t*)jack_port_get_buffer(_input_port, nframes);

  if(nframes < BUF_SIZE)
  {
      qDebug("Warning: wanted %d frames but got %d", BUF_SIZE, nframes);
      memset(_buffer, 0, sizeof(_buffer));
      memcpy(_buffer, in, sizeof(sample_t) * nframes);
  }
  else
  {
      memcpy(_buffer, in, sizeof(sample_t) * BUF_SIZE);
  }

  for (unsigned int i = 0; i < nframes; i++)
  {
    _ibuf->data[pos] = _buffer[i];
    if (pos == HOP_SIZE-1)
    {
      aubio_onset_do(_onset, _ibuf, _onset_list);
      if (_onset_list->data[0] != 0)
      {
        //qDebug() << "Onset";
        emit onset_detected();
        break;
      }
      pos = -1;
    }
    pos++;
  }

  return 0;
}
