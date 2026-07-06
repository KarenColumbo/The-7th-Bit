4040 binary counter VCO, RPi Pico-controlled. Very early alpha, needs MIDI playability. 

I really want a ramp wave of consistent amplitude (10 Vpp) and quality throughout the full freq range (8 Hz - 16 kHz). Couldn't get this out of the usual analogue suspects without fine-tuning, pot-trimming and fine-tuning again - and, of course, a really huge part count. So the trusty old 4040 binary counter seems to be the way to go. Using 7 outputs it generates a somewhat smooth and etxremely stable ramp wave (128 steps, I think) that can be treated like coming from any analog VCO.

Using 7 MCP4151 8-bit digipots instead of the fixed resistor ladder I'm able to destroy this clean ramp wave to my liking. There may not be a LOT of really useable wave forms in it, but since I full-wave rectify the ramp into a triangle and, further down the path, round it down to a sine-ish wave, there's many unusual sounds to explore, at least I think so.

Plus: I could be able to store the best ones as patches, so I'll stick with this design.
