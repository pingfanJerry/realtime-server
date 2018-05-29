#include "UdpAcceptor.h"
#include "UdpSocketsOps.h"
#include "UdpConnector.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;


UdpAcceptor::UdpAcceptor( EventLoop* loop, const InetAddress& listenAddr, bool reuseport )
	: loop_( loop ),
	acceptSocket_( sockets::createUdpNonblockingOrDie( listenAddr.family() ) ),
	acceptChannel_( loop, acceptSocket_.fd() ),
	listenPort_( listenAddr.toPort() ),
	listenning_( false )
{
	acceptSocket_.setReuseAddr( true );
	acceptSocket_.setReusePort( reuseport );
	acceptSocket_.bindAddress( listenAddr );
	acceptChannel_.setReadCallback(
		std::bind( &UdpAcceptor::handleRead, this ) );
}

UdpAcceptor::~UdpAcceptor()
{
	acceptChannel_.disableAll();
	acceptChannel_.remove();
}

void UdpAcceptor::listen()
{
	loop_->assertInLoopThread();
	listenning_ = true;

	acceptChannel_.enableReading();
}

void UdpAcceptor::handleRead()
{
	loop_->assertInLoopThread();
	InetAddress peerAddr;

	struct sockaddr_in6 addr;
	bzero( &addr, sizeof addr );
	int readByteCount = sockets::recvfrom( acceptSocket_.fd(), &addr );

	if ( readByteCount >= 0 )
	{
		peerAddr.setSockAddrInet6( addr );

		UdpConnectorPtr tempConnector( new UdpConnector( loop_, peerAddr, listenPort_ ) );
		tempConnector->setNewConnectionCallback(
			std::bind( &UdpAcceptor::newConnection, this, _1, peerAddr ) );
		tempConnector->start();

		assert( 
			udpConnectors_.find( tempConnector->GetConnectSocket().fd() ) 
			== udpConnectors_.end() 
		);
		udpConnectors_[tempConnector->GetConnectSocket().fd()] = tempConnector;
	}
	else
	{
		LOG_SYSERR << "in UdpAcceptor::handleRead";
	}
}

void UdpAcceptor::newConnection( int connfd, const InetAddress& peerAddr )
{
	if ( newConnectionCallback_ )
	{
		newConnectionCallback_( connfd, peerAddr );
	}
	else
	{
		sockets::close( connfd );
	}
}