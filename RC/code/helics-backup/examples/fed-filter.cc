/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#include "helics/application_api/MessageFederate.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <thread>
#include <stdexcept>
#include "helics/core/helicsCLI11.hpp"
#include "helics/core/helicsVersion.hpp"

int main (int argc, char *argv[])
{
    helics::helicsCLI11App app ("Filter Fed NS3 Example", "FilterFedNS3Example");
    std::string myname = "fed";
    std::string srcEndpoint = "endpoint1";
    std::string dstEndpoint = "endpoint2";

    app.add_option ("--name,-n", myname, "name of this federate");
    app.add_option ("--endpoint1,--source,-s", srcEndpoint, "name of the source endpoint");
    app.add_option ("--endpoint2,--dest,-d", dstEndpoint, "name of the destination endpoint");

    auto ret = app.helics_parse (argc, argv);

    helics::FederateInfo fi {};
    if (ret == helics::helicsCLI11App::parse_output::help_call)
    {
        fi.loadInfoFromArgs ("--help");
        return 0;
    }
    else if (ret != helics::helicsCLI11App::parse_output::ok)
    {
        return -1;
    }
    fi.defName = "filterFedNS3";
    fi.loadInfoFromArgs(argc, argv);
    fi.setProperty(helics_property_time_delta, helics::loadTimeFromString("1s"));

    auto mFed = std::make_unique<helics::MessageFederate> (myname, fi);
    auto name = mFed->getName();
    std::cout << " registering endpoint '" << srcEndpoint << "' for " << name<<'\n';
    auto &idsource = mFed->registerEndpoint(srcEndpoint, "");
    std::cout << " registering endpoint '" << dstEndpoint << "' for " << name<<'\n';
    (void)mFed->registerEndpoint(dstEndpoint, "");

    std::cout << "entering init State\n";
    mFed->enterInitializingMode ();
    std::cout << "entered init State\n";
    mFed->enterExecutingMode ();
    std::cout << "entered exec State\n";
    for (int i=1; i<10; ++i) {
        std::string message = "message sent from "+name+"/"+srcEndpoint+" to "+name+"/"+dstEndpoint+" at time " + std::to_string(i);
        mFed->sendMessage(idsource, name+"/"+dstEndpoint, message.data(), message.size());
        std::cout << message << std::endl;
        std::cout << "requesting time " << static_cast<double> (i) << "\n";
        auto newTime = mFed->requestTime (i);
        std::cout << "processed time " << static_cast<double> (newTime) << "\n";
        while (mFed->hasMessage())
        {
            auto nmessage = mFed->getMessage();
            std::cout << "received message from " << nmessage->source << " at " << static_cast<double>(nmessage->time) << " ::" << nmessage->data.to_string() << '\n';
        }

    }
    mFed->finalize ();
    return 0;
}
