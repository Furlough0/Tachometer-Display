Tachometer 

I bought a Sieg X2P Milling machine which I intend(ed) to convert to CNC.
I am easily diverted into following secondary projects at a tangent to my current activity.
In this case the X2P has an interface for a tachometer and I immediately wanted to build my own version.

Jennifer Edwards had published (14/11/2018) such a project and I simply copied it.

It worked well enough, but I had a number of issues and observations.

1.	The display update interval seemed to be slower than the expected 750ms.
2.	I was intrigued by the header data what it was and what it represented.
3.	I discovered that the signals from my mill were very noisy.
4.	Analysing and debugging modified code was difficult.

Item 1. was partly caused by the electrical noise item 3.

Because my mill is not near my computer system I decided I needed a Tacho Sender (TS) simulation.
This allowed development of the TD software in comfort.
The TS consists of a second Arduino Uno. it sends signals on pins 4, 5 & 6 and attempts to simulate the data, clock and frame signals from a real mill.

Test and Development setup

Connections between the Tachometer Display (TD) and the Test Sender (TS).

Sender Pins 4, 5 and 6 are connected to Tachometer Pins 2, 13, 11 respectively.
In the TD the pins A5, A4, gnd, +5ve are connected to the I2C display.

On my mill the tacho sender emitted data packets at about the rate of 750ms.  But the display did not always update.

The display seemed to change every other packet or sometimes every third packet.  I wanted to eliminate the electrical noise first.  I made a new cable with screened cores.  I also added ferrite rings to all the wires.  Viewed on my scope this improved the signal quality but some spikes were still present.  I left that problem there as I wanted to move on to my other points.  

There is no point in having a simulator if when using it there is no sign of the communication difficulties you are trying to overcome.
The first (functioning) version had perfect transmission to my TD.  While trying various modifications to the TS I had added diagnostic signals in the TD code toggling pins at places of interest. 

I discovered that there is very little time to spare in the interrupt routine to perform much processing.  Even just setting a pin true or false would causes disruption to the display updates.

Monitoring the TS signals and comparing them to the actions taken in the TD software it could be seen that the receiving process could only just absorb the transmitted packets at the intended rate.

Hardware is the way to go I thought.  There is no reason why the hardware based serial shift function in the Arduino could not be used to capture the data packets from the sender.  So basically that is what now happens in the new version of the Tachometer.  

Associated files image1.jpg to image7.jpg are taken from Ultrascope for my two Channel Rigol DS1052E scope.
It was while taking these images that I decided I 'needed' a better and more importantly four channel scope.  
I bought the four channel version of my Rigol scope and in general was pleased with the purchase. Until I noticed two things.
One that the scope had a parasitic signals on all four channels which would only fade after about fifteen minutes.  A long warm up time.  
Secondly the Software support application in the form of Ultrascope instead of being upgraded to cover four channels over two has been scrapped and replaced with a piece of bloatware. Bloatware that does NOT do a simple job of allowing the user to analyze a waveform on a computer remote from the scope.  What were they thinking.  
The scope was returned for repair, but the seller could not reproduce the fault. So rather than keep the faulty item I decided to upgrade again which the seller did allow me to do.  So at least that was good.  So if you buy a 'refurbished' DS1054E and discover a signal showing with no input you will know who the original owner was! Rant over.
I now have a MSO5074, way overkill for my purposes and sadly still with bloatware.

Associated pictures showing the Tachometer display operating in various modes.
11340AA.jpg, 11610AA.jpg, 11704AA.jpg and 11710AA.jpg

