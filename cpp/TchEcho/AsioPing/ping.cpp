//
// ping.cpp
// ~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "pinger.h"
#include <vector>
#include <string>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

void GetIpList(std::vector<std::string> & ips)
{
	boost::property_tree::ptree pt;
	boost::property_tree::json_parser::read_json("./targets.json", pt);
	boost::property_tree::xml_parser::write_xml("targets.xml", pt);

	for (auto v : pt)
	{
		ips.push_back(v.second.data());
	}
}

int main(int argc, char* argv[])
{
	std::vector<std::string> ips;
	GetIpList(ips);
	try
	{
		boost::asio::io_service io_service;
		for (auto ip : ips)
		{
			auto p = pinger::Create(io_service);
			p->ping(ip.c_str());
		}
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
	}
}