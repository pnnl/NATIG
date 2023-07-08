# helics-ns3

[![Build Status](https://github.com/GMLC-TDC/helics-ns3/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/GMLC-TDC/helics-ns3/actions/workflows/ci.yml)

[helics-ns3](https://github.com/GMLC-TDC/helics-ns3) is an [ns-3](https://www.nsnam.org/) module for coupling network simulations with other simulators using [HELICS](https://www.helics.org/).

## Prerequisites

Install version 2.1.1+ of [HELICS](https://github.com/GMLC-TDC/HELICS); if building from source, be sure to set the CMake variable `JSONCPP_OBJLIB=ON` and `-DCMAKE_INSTALL_PREFIX=<path to install folder you have access to>`.
For versions 2.3+,  be sure to set the cmake variables `-DHELICS_BUILD_CXX_SHARED_LIB=ON`.

Get a recent copy of ns-3, ideally from their GitLab repository.

Git:
```bash
git clone https://gitlab.com/nsnam/ns-3-dev.git
```

## Installation

Clone a copy of this repository into a folder named `helics` in the ns-3 `contrib` directory. The module directory name *must* be `helics`, otherwise the ns-3 build system will be confused.

```bash
cd ns-3-dev/
git clone https://github.com/GMLC-TDC/helics-ns3 contrib/helics
```

Run `./waf configure` with the `--disable-werror` option, and set the `--with-helics` option to the path of your HELICS installation. To enable examples or tests use `--enable-examples` or `--enable-tests`, respectively. If ZMQ is not found, `--with-zmq` can be used to specify where it is installed. Paths should be absolute.

After configuration is done, run `./waf build` to compile ns-3 with the HELICS module.

```bash
./waf configure --with-helics=/usr/local --disable-werror --enable-examples --enable-tests
./waf build
```

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.  Please see [CONTRIBUTING](./CONTRIBUTING.md) for some additional notes on licensing of new components and modifications.  

## Release
The helics-ns3 repository is distributed under the terms of the [GPL-v2](LICENSE) license as required by NS3 since this library derives some code from that code base.

Individual source files that do not require derivation from NS3 are licensed as BSD style.  Any contributions to non-NS3 derived files ideally should be made with BSD markings in the source file.
At some point in the future if legally allowed or the NS-3 derived components can be removed the repository will be relicensed.  

SPDX-License-Identifier: GPL-v2
