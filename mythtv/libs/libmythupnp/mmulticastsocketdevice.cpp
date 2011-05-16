//////////////////////////////////////////////////////////////////////////////
// Program Name: mmulticastsocketdevice.cpp
// Created     : Oct. 1, 2005
//
// Purpose     : Multicast QSocketDevice Implmenetation
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <errno.h>

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# define GET_SOCKET_ERROR    WSAGetLastError()
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# define GET_SOCKET_ERROR    errno
#endif

// Qt headers
#include <QStringList>

// MythTV headers
#include "mmulticastsocketdevice.h"
#include "mythverbose.h"

#define LOC      (QString("MMulticastSocketDevice(%1:%2): ") \
                  .arg(m_address.toString()).arg(socket()))
#define LOC_WARN (QString("MMulticastSocketDevice(%1:%2) Warning: ") \
                  .arg(m_address.toString()).arg(socket()))
#define LOC_ERR  (QString("MMulticastSocketDevice(%1:%2) Error: ") \
                  .arg(m_address.toString()).arg(socket()))

MMulticastSocketDevice::MMulticastSocketDevice(
    QString sAddress, quint16 nPort, u_char ttl) :
    MSocketDevice(MSocketDevice::Datagram),
    m_address(sAddress), m_port(nPort)
{
    // ttl = UPnp::GetConfiguration()->GetValue( "UPnP/TTL", 4 );

    if (ttl == 0)
        ttl = 4;

    setProtocol(IPv4);
    setSocket(createNewSocket(), MSocketDevice::Datagram);

    m_imr.imr_multiaddr.s_addr = inet_addr(sAddress.toAscii().constData());
    m_imr.imr_interface.s_addr = htonl(INADDR_ANY);

    VERBOSE(VB_IMPORTANT, LOC + "ctor");

    if (setsockopt(socket(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &m_imr, sizeof( m_imr )) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "setsockopt - IP_ADD_MEMBERSHIP " + ENO);
    }

    if (setsockopt(socket(), IPPROTO_IP, IP_MULTICAST_TTL,
                   &ttl, sizeof(ttl)) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "setsockopt - IP_MULTICAST_TTL " + ENO);
    }

    setAddressReusable(true);

    if (bind(m_address, m_port) < 0)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "bind failed " + ENO);
}

MMulticastSocketDevice::~MMulticastSocketDevice()
{
    if (!m_address.isNull() &&
        (setsockopt(socket(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
                    (char*)(&m_imr), sizeof(m_imr) < 0)))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "setsockopt - IP_DROP_MEMBERSHIP " + ENO);
    }
}

qint64 MMulticastSocketDevice::writeBlock(
    const char *data, quint64 len,
    const QHostAddress & host, quint16 port)
{
#ifdef IP_MULTICAST_IF
    if (host.toString() == "239.255.255.250")
    {
        QList<QHostAddress>::const_iterator it = m_local_addresses.begin();
        int retx = 0;
        for (; it != m_local_addresses.end(); ++it)
        {
            if ((*it).protocol() != QAbstractSocket::IPv4Protocol)
                continue; // skip IPv6 addresses

            QString addr = (*it).toString();
            if (addr == "127.0.0.1")
                continue; // skip localhost address

            struct in_addr interface_addr;
            int ret = inet_pton(AF_INET, addr.toAscii().constData(),
                                (char*)&interface_addr);
            int ret2 = setsockopt(socket(), IPPROTO_IP, IP_MULTICAST_IF,
                                  &interface_addr, sizeof(interface_addr));
            retx = MSocketDevice::writeBlock(data, len, host, port);
            VERBOSE(VB_IMPORTANT, QString("writeBlock on %1 %2")
                    .arg((*it).toString()).arg((retx==(int)len)?"ok":"err"));
            usleep(5000 + (rand() % 5000));
        }
        return retx;
    }
#endif

    return MSocketDevice::writeBlock(data, len, host, port);
}
