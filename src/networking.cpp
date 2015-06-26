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

#include "networking.h"


Networking::Networking(uint16_t port_num)
{
    _socket = new QUdpSocket(this);
    _port_num = port_num;
}


Networking::~Networking()
{
}


bool Networking::open(void)
{
    return true;    
}


bool Networking::close(void)
{
    return true;
}


void Networking::transmit_onset()
{
    QByteArray dgram;
    dgram.resize(1);
    dgram[0] = MSG_ONSET;
    //qDebug() << "beat";
    _socket->writeDatagram(dgram, QHostAddress::LocalHost, _port_num);
}

void Networking::transmit_fft_data(int len, float *data)
{
    QByteArray dgram;
    dgram.resize(3 + len * sizeof(float));
    dgram[0] = MSG_FFT;
    dgram[1] = (char)len;
    dgram[2] = (char)((len & 0xff00) >> 8);
    memcpy(dgram.data() + 3, data, len * sizeof(float));
    _socket->writeDatagram(dgram, QHostAddress::LocalHost, _port_num);
}


void Networking::transmit_pitch_data(float pitch, float confidence)
{
    QByteArray dgram;
    dgram.resize(1 + 2 * sizeof(float));
    float *ptr = (float *)(dgram.data() + 1);

    dgram[0] = MSG_PITCH;
    *ptr++ = pitch;
    *ptr = confidence;

    _socket->writeDatagram(dgram, QHostAddress::LocalHost, _port_num);
}
