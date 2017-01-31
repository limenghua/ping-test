//
// pinger.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef PINGER_HPP
#define PINGER_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <istream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <functional>
#include <map>

#include "icmp_header.h"
#include "ipv4_header.h"

using boost::asio::ip::icmp;
using boost::asio::deadline_timer;
namespace posix_time = boost::posix_time;

using PingHandler = std::function<void(const std::string &, const std::string &)>;

class pinger : public boost::enable_shared_from_this<pinger>
{
public:
	typedef boost::shared_ptr<pinger> Pointer;
	static Pointer Create(boost::asio::io_service& io_service)
	{
		return Pointer(new pinger(io_service));
	}
	static unsigned short GetSequnceNumber()
	{
		static unsigned short currSequnce = 0;
		return ++currSequnce;
	}
	pinger(boost::asio::io_service& io_service)
		: resolver_(io_service), socket_(io_service, icmp::v4()),
		timer_(io_service), sequence_number_(0), num_replies_(0)
	{
	}

	void ping(const std::string & destination,PingHandler handler)
	{
		handlers[destination] = handler;

/*
		icmp::resolver::query query(icmp::v4(), destination, "");
		destination_ = *resolver_.resolve(query);//目标地址

		start_send();
		start_receive();
		*/
	}

	void start()
	{
		for (auto v : handlers)
		{
			std::string ip = v.first;
			icmp::resolver::query query(icmp::v4(), ip, "");
			destination_ = *resolver_.resolve(query);//目标地址

			std::string body("\"Hello!\" from Asio ping.");
			sequence_number_ = GetSequnceNumber();


			// Create an ICMP header for an echo request.
			icmp_header echo_request;
			echo_request.type(icmp_header::echo_request);
			echo_request.code(0);
			echo_request.identifier(get_identifier());
			echo_request.sequence_number(sequence_number_);
			compute_checksum(echo_request, body.begin(), body.end());

			// Encode the request packet.
			boost::asio::streambuf request_buffer;
			std::ostream os(&request_buffer);
			os << echo_request << body;

			// Send the request.
			socket_.send_to(request_buffer.data(), destination_);
			posix_time::ptime now = posix_time::microsec_clock::universal_time();
			startTime[ip] = now;
		}
		posix_time::ptime now = posix_time::microsec_clock::universal_time();
		timer_.expires_at(now + posix_time::seconds(1));
		timer_.async_wait(boost::bind(&pinger::handle_timeout, shared_from_this()));
		start_receive();
	}

private:
	void start_send()
	{
		std::string body("\"Hello!\" from Asio ping.");
		sequence_number_ = GetSequnceNumber();


		// Create an ICMP header for an echo request.
		icmp_header echo_request;
		echo_request.type(icmp_header::echo_request);
		echo_request.code(0);
		echo_request.identifier(get_identifier());
		echo_request.sequence_number(sequence_number_);
		compute_checksum(echo_request, body.begin(), body.end());

		// Encode the request packet.
		boost::asio::streambuf request_buffer;
		std::ostream os(&request_buffer);
		os << echo_request << body;

		// Send the request.
		time_sent_ = posix_time::microsec_clock::universal_time();//boost 得到时间
		socket_.send_to(request_buffer.data(), destination_);

		// Wait up to five seconds for a reply.
		num_replies_ = 0;
		timer_.expires_at(time_sent_ + posix_time::seconds(5));
		timer_.async_wait(boost::bind(&pinger::handle_timeout, shared_from_this()));
	}

	void handle_timeout()
	{
		posix_time::ptime outTime = posix_time::microsec_clock::universal_time()  - posix_time::seconds(5);

		std::map<std::string, posix_time::ptime>::iterator it;
		
		for (it = startTime.begin(); it != startTime.end(); )
		{
			if (it->second < outTime)
			{
				handlers[it->first](it->first, " Request timeout");
				handlers.erase(it->first);
				it = startTime.erase(it);
			}
			else
			{
				it++;
			}
		}

		if (!handlers.empty())
		{
			// Requests must be sent no less than one second apart.
			posix_time::ptime now = posix_time::microsec_clock::universal_time();
			timer_.expires_at(now + posix_time::seconds(1));
			timer_.async_wait(boost::bind(&pinger::start_send, shared_from_this()));
		}
	}

	void start_receive()
	{
		// Discard any data already in the buffer.
		reply_buffer_.consume(reply_buffer_.size());//streambuf.comsume()清空

													// Wait for a reply. We prepare the buffer to receive up to 64KB.
		socket_.async_receive(reply_buffer_.prepare(65536),//准备缓冲区
			boost::bind(&pinger::handle_receive, shared_from_this(), _2));
	}

	void handle_receive(std::size_t length)
	{
		// The actual number of bytes received is committed to the buffer so that we
		// can extract it using a std::istream object.
		reply_buffer_.commit(length);//转移到streambuf中，


									 // Decode the reply packet.
		std::istream is(&reply_buffer_);
		ipv4_header ipv4_hdr;
		icmp_header icmp_hdr;
		is >> ipv4_hdr >> icmp_hdr;

		// We can receive all ICMP packets received by the host, so we need to
		// filter out only the echo replies that match the our identifier and
		// expected sequence number.

		std::string ip = ipv4_hdr.source_address().to_string();
		std::stringstream ss;
		ss << length - ipv4_hdr.header_length()
			<< " bytes from " << ipv4_hdr.source_address()
			<< ": icmp_seq=" << icmp_hdr.sequence_number()
			<< ", ttl=" << ipv4_hdr.time_to_live();

		std::map<std::string, PingHandler>::iterator it = handlers.find(ip);
		if (it != handlers.end())
		{
			it->second(ip, ss.str());
		}
		handlers.erase(it);
		startTime.erase(ip);

		if (!handlers.empty())
		{
			start_receive();
		}
	}

	static unsigned short get_identifier()
	{
#if defined(BOOST_ASIO_WINDOWS)
		return static_cast<unsigned short>(::GetCurrentProcessId());//全局函数，widows api，得到进程id
#else
		return static_cast<unsigned short>(::getpid());//linux
#endif
	}

	icmp::resolver resolver_;
	icmp::endpoint destination_;
	icmp::socket socket_;
	deadline_timer timer_;
	unsigned short sequence_number_;
	posix_time::ptime time_sent_;
	boost::asio::streambuf reply_buffer_;
	std::size_t num_replies_;

	std::map<std::string, PingHandler> handlers;
	std::map<std::string, posix_time::ptime> startTime;
};

#endif //PINGER_HPP
