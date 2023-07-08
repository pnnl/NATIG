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
    helics::helicsCLI11App app ("SndRcv NS3 Example", "SndRcvNS3Example");
    std::string myname = "fed";
    std::string targetFederate = "ns3";
    std::string targetEndpoint = "endpoint1";
    std::string mysource = "endpoint1";
    std::string mydestination = "endpoint2";

    app.add_option ("--name,-n", myname, "name of this federate");
    app.add_option ("--target,-t", targetFederate, "name of the target federate");
    app.add_option ("--endpoint,-e", targetEndpoint, "name of the target endpoint");
    app.add_option ("--source,-s", mysource, "name of the source endpoint");
    app.add_option ("--destination,-d", mydestination, "name of the destination endpoint");

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

    std::string target = targetFederate + "/" + targetEndpoint;
    fi.loadInfoFromArgs(argc, argv);
    fi.setProperty(helics_property_int_log_level, 5);

    auto mFed = std::make_unique<helics::MessageFederate> (myname, fi);
    auto name = mFed->getName();
    std::cout << " registering endpoint '" << mysource << "' for " << name<<'\n';
    auto &idsource = mFed->registerEndpoint(mysource, "");
    std::cout << " registering endpoint '" << mydestination << "' for " << name<<'\n';
    (void)mFed->registerEndpoint(mydestination, "");

    std::cout << "entering init State\n";
    mFed->enterInitializingMode ();
    std::cout << "entered init State\n";
    mFed->enterExecutingMode ();
    std::cout << "entered exec State\n";
    for (int i=1; i<10; ++i) {
        std::string message = "message sent from "+name+"/"+mysource+" to "+target+" at time " + std::to_string(i);
        mFed->sendMessage(idsource, target, message.data(), message.size());
        std::cout << message << std::endl;
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
