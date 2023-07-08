/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#include "ns3/helics-helper.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include "helics/core/helicsCLI11.hpp"

/* This example can be run along with ns3-sndrcv.
 *
 * It shows how to register HELICS publications and subscriptions
 * using the helics_federate object created by the helics-ns3 module.
 *
 * Tying subscriptions and publication values in with the simulated
 * ns-3 network is left as an excersice to the reader. One idea is
 * creating a custom `ns3::Application` that registers a publication
 * and then extracts values from received packets and publishes them.
 */
int main (int argc, char *argv[])
{
    using ns3::helics_federate;
    helics::helicsCLI11App app ("PubSub NS3 Example", "PubSubNS3Example");
    std::string publishKey = "ns3_test_value";

    ns3::CommandLine cmd;
    ns3::HelicsHelper helicsHelper;
    cmd.AddValue ("key", "name of the key to publish on", publishKey);
    helicsHelper.SetupCommandLine (cmd);

    // Manually change the name of the broker so as to not clash with the
    // default name; but not if the user specified a name
    bool add_name = true;
    std::vector<std::string> args;
    const char* const name_setting = "--name=";
    const auto name_len = std::strlen(name_setting);
    for (int i = 0; i < argc; ++i)
    {
        if (std::strncmp (argv[i], name_setting, name_len) == 0)
        {
            add_name = false;
        }
        args.push_back (argv[i]);
    }
    if (add_name)
    {
        args.push_back ("--name=fed-pubsub");
    }

    cmd.Parse (args);
    helicsHelper.SetupFederate (args);
    helics_federate->setProperty (helics_property_int_log_level, 5);

    const auto name = helics_federate->getName();
    std::cout << " registering publication '" << publishKey << "' for " << name<<'\n';
    auto &pub = helics_federate->registerGlobalPublication<std::string> (publishKey);
    std::cout << " registering subscription '" << publishKey << "' for " << name<<'\n';
    auto &sub = helics_federate->registerSubscription (publishKey);

    std::cout << "entering init State\n";
    helics_federate->enterInitializingMode ();
    std::cout << "entered init State\n";
    helics_federate->enterExecutingMode ();
    std::cout << "entered exec State\n";
    for (int i=1; i<10; ++i)
    {
        const std::string message = "publishing message on "+publishKey+" at time " + std::to_string (i);
        helics_federate->publish (pub, message);
        std::cout << message << std::endl;
        const auto newTime = helics_federate->requestTime (i);
        std::cout << "processed time " << static_cast<double> (newTime) << "\n";
        while (sub.checkUpdate ())
        {
            const auto nmessage = sub.getValue<std::string> ();
            std::cout << "received publication on key " << sub.getKey () << " at " << static_cast<double> (sub.getLastUpdate ()) << " ::" << nmessage << '\n';
        }

    }
    helics_federate->finalize ();
    return 0;
}
