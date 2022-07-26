[![Silicon Labs](./docs/silabs/images/silabs-logo.jpg)](https://www.silabs.com)


# Silicon Labs Matter

Welcome to the Silicon Labs Matter Github repo. This is your one stop shop for all things related to Silicon Labs and Matter development.

**To develop a Matter application with Silicon Labs please start here:** 

**[Silicon Labs Matter Table of Contents](./docs/silabs/README.md)**

---


[![Builds](https://github.com/project-chip/connectedhomeip/workflows/Builds/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/build.yaml)

[![Examples - EFR32](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20EFR32/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-efr32.yaml)
[![Examples - ESP32](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20ESP32/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-esp32.yaml)
[![Examples - i.MX Linux](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20i.MX%20Linux/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-linux-imx.yaml)
[![Examples - K32W with SE051](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20K32W%20with%20SE051/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-k32w.yaml)
[![Examples - Linux Standalone](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20Linux%20Standalone/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-linux-standalone.yaml)
[![Examples - nRF Connect SDK](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20nRF%20Connect%20SDK/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-nrfconnect.yaml)
[![Examples - QPG](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20QPG/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-qpg.yaml)
[![Examples - TI CC26X2X7](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20TI%20CC26X2X7/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/examples-cc13x2x7_26x2x7.yaml)

[![Android](https://github.com/project-chip/connectedhomeip/workflows/Android/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/android.yaml)

[![Unit / Interation Tests](https://github.com/project-chip/connectedhomeip/workflows/Unit%20/%20Interation%20Tests/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/unit_integration_test.yaml)
[![Cirque](https://github.com/project-chip/connectedhomeip/workflows/Cirque/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/cirque.yaml)
[![QEMU](https://github.com/project-chip/connectedhomeip/workflows/QEMU/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/qemu.yaml)

[![ZAP Templates](https://github.com/project-chip/connectedhomeip/workflows/ZAP/badge.svg)](https://github.com/project-chip/connectedhomeip/actions/workflows/zap_templates.yaml)



# Directory Structure

The Matter repository is structured as follows:

| File / Folder                          | Contents                                                                           |
| -------------------------------------- | ---------------------------------------------------------------------------------- |
| `build/`                               | Build system support content and build output directories                          |
| [BUILDING.md](docs/guides/BUILDING.md) | More detailed information on configuring and building Matter for different targets |
| `CODE_OF_CONDUCT.md`                   | Code of Conduct for Matter, and contributions to it                                |
| [CONTRIBUTING.md](./CONTRIBUTING.md)   | Guidelines for contributing to Matter                                              |
| `docs/`                                | Documentation, including [guides](./docs/guides)                                   |
| `examples/`                            | Example firmware applications that demonstrate use of the Matter technology        |
| `integrations/`                        | Third party integrations related to this project                                   |
| `integrations/docker/`                 | Docker scripts and Dockerfiles                                                     |
| `LICENSE`                              | Matter [License file](./LICENSE) (Apache 2.0)                                      |
| `BUILD.gn`                             | Top level GN build file                                                            |
| `README.md`                            | This file                                                                          |
| `src/`                                 | Implementation of Matter                                                           |
| `third_party/`                         | Third-party code used by Matter                                                    |
| `scripts/`                             | Scripts needed to work with the Matter repository                                  |

# License

Matter is released under the [Apache 2.0 license](./LICENSE).
