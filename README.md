<!-- Improved compatibility of back to top link: See: https://github.com/othneildrew/Best-README-Template/pull/73 -->
<a id="readme-top"></a>
<!--
*** Thanks for checking out the Best-README-Template. If you have a suggestion
*** that would make this better, please fork the repo and create a pull request
*** or simply open an issue with the tag "enhancement".
*** Don't forget to give the project a star!
*** Thanks again! Now go create something AMAZING! :D
-->



<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![project_license][license-shield]][license-url]



<!-- PROJECT LOGO -->
<br />
<div align="center">
  <!--
  <a href="https://github.com/GXDE-OS/gxde-launcher">
    <img src="images/logo.png" alt="Logo" width="80" height="80">
  </a>
  -->

<h3 align="center">GXDE Launcher</h3>

  <p align="center">
    Launcher for <i>Gorgeous eXtended Deepin Environment</i>.
    <br />
    <a href="./docs/technology-overview.md"><strong>Technology Overview »</strong></a>
    <br />
    <br />
    <a href="https://github.com/GXDE-OS/gxde-launcher/actions">View Pipeline Status</a>
    &middot;
    <a href="https://gitee.com/GXDE-OS/gxde-launcher/issues">Report Bug</a>
    &middot;
    <a href="https://gitee.com/GXDE-OS/gxde-launcher/issues">Request Feature</a>
  </p>
</div>


<!-- ABOUT THE PROJECT -->
## About The Project
GXDE launcher is a launcher that launches your application. It is a fork of DDE 15's launcher with two modes:
- DDE 15 mode (the default apperance) when GXDE dock is in either fasion or efficient mode.
- DTK6 styled UI when GXDE dock is in Deepin V20/V23 mode.



### Built With

* CMake
* Qt6
* DFrameworkDBus-Qt6
* XCB-EWMH
* LayerShellQt
* GSettings-Qt6
* DTK2Widget-Qt6



<!-- GETTING STARTED -->
## Getting Started
Note that DTK2Widget-Qt6 is only avaliable in GXDE's own APT/pacman source. You might need to compile it yourself if you're not using GXDE.

### Debian Packaging
To build a Debian binary package, simply run:

```shell
$ ./build-deb -d
```

Dependencies will automatically being installed to your computer.

Next time you may run:

```shell
$ ./build-deb
```

To do a cleanup, run:
```shell
$ ./build-deb -c
```

Note that this will also clear the artifacts generated.

### Archlinux Packaging
To build a Arch `zstd` package, simply run:

```shell
$ ./build-pkg -d
```

Dependencies will automatically being installed to your computer.

Next time you may run:

```shell
$ ./build-pkg
```

To do a cleanup, run:
```shell
$ ./build-pkg -c
```

Note that this will also clear the artifacts generated.



<!-- USAGE EXAMPLES -->
## Usage
You may simply press the `SUPER` key to activate launcher. Or you may run the following command to start it manually from a terminal:
```bash
$ gxde-launcher -s
```



<!-- CONTRIBUTING -->
## Contributing
### Top contributors:

<a href="https://github.com/GXDE-OS/gxde-launcher/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=GXDE-OS/gxde-launcher" alt="contrib.rocks image" />
</a>



<!-- LICENSE -->
## License
GXDE launcher is licensed under GNU GENERAL PUBLIC LICENSE Version 3. You may see the [license file](./LICENSE) for details.


<!-- ACKNOWLEDGMENTS -->
## Acknowledgments
- DDE Launcher: https://github.com/linuxdeepin/dde-launcher
- Best README Templatee: https://github.com/othneildrew/Best-README-Template/blob/main/BLANK_README.md



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/GXDE-OS/gxde-launcher.svg?style=for-the-badge
[contributors-url]: https://github.com/GXDE-OS/gxde-launcher/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/GXDE-OS/gxde-launcher.svg?style=for-the-badge
[forks-url]: https://github.com/GXDE-OS/gxde-launcher/network/members
[stars-shield]: https://img.shields.io/github/stars/GXDE-OS/gxde-launcher.svg?style=for-the-badge
[stars-url]: https://github.com/GXDE-OS/gxde-launcher/stargazers
[issues-shield]: https://img.shields.io/github/issues/GXDE-OS/gxde-launcher.svg?style=for-the-badge
[issues-url]: https://github.com/GXDE-OS/gxde-launcher/issues
[license-shield]: https://img.shields.io/github/license/GXDE-OS/gxde-launcher.svg?style=for-the-badge
[license-url]: https://github.com/GXDE-OS/gxde-launcher/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/linkedin_username
[product-screenshot]: images/screenshot.png
<!-- Shields.io badges. You can a comprehensive list with many more badges at: https://github.com/inttter/md-badges -->
[Next.js]: https://img.shields.io/badge/next.js-000000?style=for-the-badge&logo=nextdotjs&logoColor=white
[Next-url]: https://nextjs.org/
[React.js]: https://img.shields.io/badge/React-20232A?style=for-the-badge&logo=react&logoColor=61DAFB
[React-url]: https://reactjs.org/
[Vue.js]: https://img.shields.io/badge/Vue.js-35495E?style=for-the-badge&logo=vuedotjs&logoColor=4FC08D
[Vue-url]: https://vuejs.org/
[Angular.io]: https://img.shields.io/badge/Angular-DD0031?style=for-the-badge&logo=angular&logoColor=white
[Angular-url]: https://angular.io/
[Svelte.dev]: https://img.shields.io/badge/Svelte-4A4A55?style=for-the-badge&logo=svelte&logoColor=FF3E00
[Svelte-url]: https://svelte.dev/
[Laravel.com]: https://img.shields.io/badge/Laravel-FF2D20?style=for-the-badge&logo=laravel&logoColor=white
[Laravel-url]: https://laravel.com
[Bootstrap.com]: https://img.shields.io/badge/Bootstrap-563D7C?style=for-the-badge&logo=bootstrap&logoColor=white
[Bootstrap-url]: https://getbootstrap.com
[JQuery.com]: https://img.shields.io/badge/jQuery-0769AD?style=for-the-badge&logo=jquery&logoColor=white
[JQuery-url]: https://jquery.com 