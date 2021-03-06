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

#ifndef _NETWORKING_H
#define _NETWORKING_H

#include <stdint.h>

#include <QtCore/QObject>
#include <QtCore/QDebug>
#include <QtNetwork/QUdpSocket>

#define TRANSMIT_PORT 3010

#define MSG_FFT 0x66
#define MSG_ONSET 0x77
#define MSG_PITCH 0x88


class Networking : public QObject
{
    Q_OBJECT

public:
    Networking(const QHostAddress& dest_addr, uint16_t port_num);
    ~Networking();

    bool open(void);
    bool close(void);

public slots:
    void transmit_onset(void);
    void transmit_fft_data(int len, float *data);
    void transmit_pitch_data(float pitch, float confidence);

private:
    QUdpSocket *_socket;
    QHostAddress _dest_addr;
    uint16_t _port_num;
};

#endif
