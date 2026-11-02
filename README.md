<div align="center">

  [![Gambo logo][gambo-logo]][website-link]

  <h3>Gambo</h3>

  A UCI chess engine with an ultra-aggressive style.
  <br>
  <strong>[Explore Gambo docs »][readme-link]</strong>
  <br>
  <br>
  [Report bug][issue-link]
  ·
  [Open a discussion][discussions-link]
  ·
  [Discord][discord-link]
  ·
  [Blog][website-blog-link]

  [![Build][build-badge]][build-link]
  [![License][license-badge]][license-link]
  <br>
  [![Release][release-badge]][release-link]
  [![Commits][commits-badge]][commits-link]
  <br>
  [![Website][website-badge]][website-link]
  [![Fishtest][fishtest-badge]][fishtest-link]
  [![Discord][discord-badge]][discord-link]

</div>

## Overview

[Gambo][website-link] is a **UCI chess engine** that analyzes chess positions
and computes the optimal moves with an aggressive style profile.

Gambo **does not include a graphical user interface** (GUI) that is required
to display a chessboard and to make it easy to input moves. These GUIs are
developed independently and are available online. **Read the documentation for
your GUI** of choice for information about how to use Gambo with it.

See also the Gambo [documentation][readme-link] for further usage help.

## Files

This distribution of Gambo consists of the following files:

  * [README.md][readme-link], the file you are currently reading.

  * [Copying.txt][license-link], a text file containing the GNU General Public
    License version 3.

  * [AUTHORS][authors-link], a text file with the list of authors for the project.

  * [src][src-link], a subdirectory containing the full source code, including a
    Makefile that can be used to compile Gambo on Unix-like systems.

  * a file with the .nnue extension, storing the neural network for the NNUE
    evaluation. Binary distributions will have this file embedded.

## Contributing

__See [Contributing Guide](CONTRIBUTING.md).__

### Donating hardware

Improving Gambo requires a massive amount of testing. You can donate your
hardware resources by installing the [Fishtest Worker][worker-link] and viewing
the current tests on [Fishtest][fishtest-link].

### Improving the code

In the [chessprogramming wiki][programming-link], many techniques used in
Gambo are explained with a lot of background information.
The main chess programming references describe many features and techniques
used by modern engines. However, they are generic rather than focused on
Gambo's precise implementation.

The engine testing is done on [Fishtest][fishtest-link].
If you want to help improve Gambo, please read this [guideline][guideline-link]
first, where the basics of chess engine development are explained.

Discussions about Gambo take place mainly in the Gambo
[Discord server][discord-link]. This is also the best place to ask questions
about the codebase and how to improve it.

## Compiling Gambo

Gambo has support for 32 or 64-bit CPUs, certain hardware instructions,
big-endian machines such as Power PC, and other platforms.

On Unix-like systems, it should be easy to compile Gambo directly from the
source code with the included Makefile in the folder `src`. In general, it is
recommended to run `make help` to see a list of make targets with corresponding
descriptions. An example suitable for most Intel and AMD chips:

```
cd src
make -j profile-build
```

Detailed compilation instructions for all platforms can be found in our
[documentation][wiki-compile-link]. Our wiki also has information about
the [UCI commands][wiki-uci-link] supported by Gambo.

## Terms of use

Gambo is free and distributed under the
[**GNU General Public License version 3**][license-link] (GPL v3). Essentially,
this means you are free to do almost exactly what you want with the program,
including distributing it among your friends, making it available for download
from your website, selling it (either by itself or as part of some bigger
software package), or using it as the starting point for a software project of
your own.

The only real limitation is that whenever you distribute Gambo in some way,
you MUST always include the license and the full source code (or a pointer to
where the source code can be found) to generate the exact binary you are
distributing. If you make any changes to the source code, these changes must
also be made available under GPL v3.

## Acknowledgements

Gambo uses neural networks trained on [data provided by the Leela Chess Zero
project][lc0-data-link], which is made available under the [Open Database License][odbl-link] (ODbL).


[authors-link]:       https://github.com/Falthera/Gambo/blob/main/AUTHORS
[build-link]:         https://github.com/Falthera/Gambo/actions/workflows/gambo-release.yml
[commits-link]:       https://github.com/Falthera/Gambo/commits/main
[discord-link]:       https://discord.gg/GWDRS3kU6R
[issue-link]:         https://github.com/Falthera/Gambo/issues/new/choose
[discussions-link]:   https://github.com/Falthera/Gambo/discussions/new
[fishtest-link]:      https://github.com/Falthera/Gambo/actions
[guideline-link]:     https://github.com/Falthera/Gambo/blob/main/CONTRIBUTING.md
[license-link]:       https://github.com/Falthera/Gambo/blob/main/Copying.txt
[programming-link]:   https://www.chessprogramming.org/Main_Page
[readme-link]:        https://github.com/Falthera/Gambo/blob/main/README.md
[release-link]:       https://github.com/Falthera/Gambo/releases/latest
[src-link]:           https://github.com/Falthera/Gambo/tree/main/src
[gambo-logo]:         prallogo.png
[uci-link]:           https://backscattering.de/chess/uci/
[website-link]:       https://github.com/Falthera/Gambo
[website-blog-link]:  https://github.com/Falthera/Gambo/releases
[wiki-link]:          https://github.com/Falthera/Gambo/blob/main/README.md
[wiki-compile-link]:  https://github.com/Falthera/Gambo/blob/main/README.md#compiling-gambo
[wiki-uci-link]:      https://github.com/Falthera/Gambo/blob/main/README.md#overview
[wiki-usage-link]:    https://github.com/Falthera/Gambo/blob/main/README.md#overview
[worker-link]:        https://github.com/Falthera/Gambo/actions
[lc0-data-link]:      https://storage.lczero.org/files/training_data
[odbl-link]:          https://opendatacommons.org/licenses/odbl/odbl-10.txt

[build-badge]:        https://img.shields.io/github/actions/workflow/status/Falthera/Gambo/gambo-release.yml?branch=main&style=for-the-badge&label=gambo&logo=github
[commits-badge]:      https://img.shields.io/github/commits-since/Falthera/Gambo/latest?style=for-the-badge
[discord-badge]:      https://img.shields.io/discord/435943710472011776?style=for-the-badge&label=discord&logo=Discord
[fishtest-badge]:     https://img.shields.io/website?style=for-the-badge&down_color=red&down_message=Offline&label=Gambo&up_color=success&up_message=Online&url=https%3A%2F%2Fgithub.com%2FFalthera%2FGambo
[license-badge]:      https://img.shields.io/github/license/Falthera/Gambo?style=for-the-badge&label=license&color=success
[release-badge]:      https://img.shields.io/github/v/release/Falthera/Gambo?style=for-the-badge&label=gambo%20release
[website-badge]:      https://img.shields.io/website?style=for-the-badge&down_color=red&down_message=Offline&label=gambo&up_color=success&up_message=Online&url=https%3A%2F%2Fgithub.com%2FFalthera%2FGambo
