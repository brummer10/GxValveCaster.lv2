# GxValveCaster.lv2
The ValveCaster is a little tube boost pedal simulation. It adds some overdrive and tube compression along with boosting the signal


![GxValveCaster](https://raw.githubusercontent.com/brummer10/GxValveCaster.lv2/master/GxValveCaster.png)


###### BUILD DEPENDENCY’S 

the following packages are needed to build GxValveCaster:

- libc6-dev
- libcairo2-dev
- libx11-dev
- x11proto-dev
- lv2-dev

note that those packages could have different, but similar names 
on different distributions. There is no configure script, 
make will simply fail when one of those packages isn't found.

## BUILD 

$ make install

will install into ~/.lv2

$ sudo make install

will install into /usr/lib/lv2

