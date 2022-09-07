[![Silicon Labs](./docs/silabs/images/silabs-logo.jpg)](https://www.silabs.com)


# Silicon Labs Matter

Welcome to the Silicon Labs Matter Github repo. Matter is an application layer that provides a standardized interface between protocols and devices. With Matter, it is transparent to the underlying device whether the interaction is from a Thread application or a Wi-Fi application. For more information on Matter in general see the main [Matter Overview](https://www.silabs.com/wireless/matter) page. This repo is the starting point for all Silicon Labs-related Matter development. Silicon Labs supports Matter on both 802.15.4 (Thread) and 802.11 (Wi-Fi) transport protocols. 

_To see release notes containing list of features and knowns issues go to [SiliconLabs/matter/releases](https://github.com/SiliconLabs/matter/releases) and find the corresponding notes for the release you are using._

![Silicon Labs](./docs/silabs/images/silicon_labs_matter.png)

This repo contains documentation, demos, examples and all the code needed for Matter Accessory Device development on both Thread and Wi-Fi. The Thread development use cases differs from Wi-Fi because the Thread protocol requires the use of an Open Thread Border Router (OTBR).  

- To get started with the Thread demo and development see [Matter Thread](./docs/silabs/thread/DEMO_OVERVIEW.md)
- To get started with the Wi-Fi demo and development see [Matter Wi-Fi](./docs/silabs/wifi/DEMO_OVERVIEW.md)

The full documentation set starts here: [Silicon Labs Matter Table of Contents](./docs/silabs/README.md)

---


[![Builds](https://github.com/project-chip/connectedhomeip/workflows/Builds/badge.svg)](https://github.com/SiliconLabs/matter/actions/workflows/build.yaml)

[![Examples - EFR32](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20EFR32/badge.svg)](https://github.com/SiliconLabs/matter/actions/workflows/examples-efr32.yaml)

[![Examples - Linux Standalone](https://github.com/project-chip/connectedhomeip/workflows/Build%20example%20-%20Linux%20Standalone/badge.svg)](https://github.com/SiliconLabs/matter/actions/workflows/examples-linux-standalone.yaml)

[![Android](https://github.com/project-chip/connectedhomeip/workflows/Android/badge.svg)](https://github.com/SiliconLabs/matter/actions/workflows/android.yaml)

[![Unit / Interation Tests](https://github.com/project-chip/connectedhomeip/workflows/Unit%20/%20Interation%20Tests/badge.svg)](https://github.com/SiliconLabs/matter/actions/workflows/unit_integration_test.yaml)
[![Cirque](https://github.com/project-chip/connectedhomeip/workflows/Cirque/badge.svg)](https://github.com/SiliconLabs/matter/actions/workflows/cirque.yaml)

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
