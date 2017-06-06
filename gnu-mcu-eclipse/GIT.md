## Repository URLs

- the GNU MCU Eclipse git remote URL to clone from is `https://github.com/gnu-mcu-eclipse/openocd`
- the OpenOCD git remote URL is `https://git.code.sf.net/p/openocd/code`.

Add a remote named *openocd*, and pull its master → master.

## Update procedures

### The gnu-mcu-eclipse-dev branch

To keep the development repository in sync with the original OpenOCD repository:

- checkout master
- pull from openocd master
- checkout gnu-mcu-eclipse-dev
- merge master
- add a tag like gme-0.9.0-201505111122-dev after each public release

### The gnu-mcu-eclipse branch

To keep the stable development in sync with the develomnet branch:

- checkout gnu-mcu-eclipse
- merge gnu-mcu-eclipse-dev
- add a tag like gme-0.8.0-201505111122 after each public release




