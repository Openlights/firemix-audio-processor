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

#include <QtCore/QCoreApplication>
#include <QHostInfo>

#include "jack_client.h"
#include "networking.h"


int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QHostAddress dest_addr;
    if (argc > 1) {
        QHostInfo host_info = QHostInfo::fromName(argv[1]);
        if (host_info.error() != QHostInfo::NoError
            || host_info.addresses().empty())
        {
            qFatal("Could not resolve host: %s: %s", argv[1],
                   host_info.errorString().toUtf8().constData());
            exit(1);
        }
        dest_addr = host_info.addresses().first();
    } else {
        dest_addr = QHostAddress::LocalHost;
    }

    qDebug() << "Sending FFT data to" << dest_addr << "port" << TRANSMIT_PORT;

    JackClient jc;
    Networking net(dest_addr, TRANSMIT_PORT);

    QObject::connect(&jc, SIGNAL(onset_detected()), &net, SLOT(transmit_onset()));
    QObject::connect(&jc, SIGNAL(fft_data(int, float*)), &net, SLOT(transmit_fft_data(int, float*)));
    QObject::connect(&jc, SIGNAL(pitch_data(float,float)), &net, SLOT(transmit_pitch_data(float,float)));

    return app.exec();
}
