## Repository URLs

- the GNU ARM Eclipse git remote URL to clone from is **ssh://ilg-ul@git.code.sf.net/p/gnuarmeclipse/openocd**
- the OpenOCD git remote URL is **http://git.code.sf.net/p/openocd/code**.

Add a remote named *openocd*, and pull its master → master.

## Update procedures

### The gnuarmeclipse-dev branch

To keep the development repository in sync with the original OpenOCD repository:

- checkout master
- pull from openocd master
- checkout gnuarmeclipse-dev
- merge master
- add a tag like gae-0.9.0-2-dev after each public release

### The gnuarmeclipse branch

To keep the stable development in sync with the develomnet branch:

- checkout gnuarmeclipse
- merge gnuarmeclipse-dev
- add a tag like gae-0.8.0-2 after each public release




