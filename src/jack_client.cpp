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

  _fft = new_aubio_fft(FFT_SIZE);
  _grain = new_cvec(FFT_SIZE);

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

  char _onset_mode[4] = "mkl";
  //char _onset_mode[4] = "hfc";
  //char _onset_mode[9] = "specflux";
  smpl_t threshold = 0.3;
  smpl_t silence = -70.0;
  _onset = new_aubio_onset(_onset_mode, BUF_SIZE, HOP_SIZE, _samplerate);
  aubio_onset_set_threshold(_onset, threshold);
  aubio_onset_set_silence(_onset, silence);
  aubio_onset_set_minioi_ms(_onset, 250);
  
  char _pitch_mode[7] = "yinfft";
  _pitch = new_aubio_pitch(_pitch_mode, BUF_SIZE, HOP_SIZE, _samplerate);
  _pitch_value = new_fvec(1);
  char _pitch_unit[3] = "Hz";
  aubio_pitch_set_unit(_pitch, _pitch_unit);

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
  del_aubio_fft(_fft);
  del_aubio_pitch(_pitch);
  del_cvec(_grain);
  del_fvec(_pitch_value);
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
  static unsigned int delay = 0;
  jack_default_audio_sample_t *in;

  in = (jack_default_audio_sample_t*)jack_port_get_buffer(_input_port, nframes);

  //qDebug("Wanted %d frames and got %d", BUF_SIZE, nframes);

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
      aubio_fft_do(_fft, _ibuf, _grain);

      if (delay > FFT_SEND_INTERVAL)
      {
        const int logged_size = 256;
        float logged_buffer[logged_size];

        const int bucket_indexes[] = {
            1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
            3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
            7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
            13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
            15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16,
            17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18,
            19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 21, 21, 21,
            21, 21, 22, 22, 22, 22, 23, 23, 23, 24, 24, 25, 26, 27, 28, 29,
            30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
            50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 200, 220, 256
        };
        const int bucket_lerp[] = {
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9,
            1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9,
            1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9,
            1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9,
            1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9,
            1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9,
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
            1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
        };

        const float adjust_volume[] = {
            4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 1, 1, 1, 1, 1, 1,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
            11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 5

//            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
//            3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
//            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
//            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
//            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
//            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
//            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
//            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
//            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
//            11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
//            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
//            13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
//            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
//            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
//            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
        };

        for (int i = 0; i < logged_size; i++)
        {
            if (bucket_indexes[i] == bucket_indexes[i+1])
            {
                float lerp = bucket_lerp[i] / 10.0;
                logged_buffer[i] = _grain->norm[bucket_indexes[i]] * (1.0 - lerp) + _grain->norm[bucket_indexes[i]+1] * (lerp);
                //qDebug("lerp %d : lerp %f from %f to %f is %f", i, lerp, _grain->norm[bucket_indexes[i]], _grain->norm[bucket_indexes[i]+1], logged_buffer[i]);
            }
            else
            {
                logged_buffer[i] = 0;
                for (int j = bucket_indexes[i]; (j <= bucket_indexes[i+1]); j++)
                {
                    logged_buffer[i] += _grain->norm[j];
                }
            }
            logged_buffer[i] *= adjust_volume[i] / 16.0;
            //qDebug("fft %d : currently %d, bucket size %f", i, current, bucket_size);
        }
        //qDebug("Used %d frames, bucket size is %f", current, bucket_size);

        emit fft_data(logged_size, logged_buffer);
        delay = 0;
      }
      else
      {
        delay += nframes;
      }
      
      aubio_pitch_do(_pitch, _ibuf, _pitch_value);
      smpl_t confidence = aubio_pitch_get_confidence(_pitch);
      smpl_t pitch_found = fvec_get_sample(_pitch_value, 0);
      
      //qDebug("Pitch detected %f confidence %f", pitch_found, confidence);

      //if (_grain->norm[0] > 0.1) {
      //  cvec_print(_grain);
      //}

      aubio_onset_do(_onset, _ibuf, _onset_list);

      if (_onset_list->data[0] != 0)
      {
        emit onset_detected();
        break;
      }
      pos = -1;
    }
    pos++;
  }

  return 0;
}
