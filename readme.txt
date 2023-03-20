Oric dflat file server
  This is a mini file server using the Pro Micro / Leonardo unit
  Connects to the Oric-1/Atmos printer port as follows;
  Printer Pin #     Arduino Pin #     Function
  3                 2 IN              Data bit 0: Clock
  5                 3 IN/OUT          Data bit 1: Nibble data in/out
  7                 4 IN/OUT          Data bit 2: Nibble data in/out
  9                 5 IN/OUT          Data bit 3: Nibble data in/out
  11                6 IN/OUT          Data bit 4: Nibble data in/out
  13                7 OUT             Data bit 5: Arduino ready signal
  15                8 IN              Data bit 6: Select (low=active)
  17                9 IN              Data bit 7: Select (low=active)
  1                 21                Strobe (low=active)
  19                20                ACK (not used)
  20                GND               Ground
  
  An SD card needs to be present on the SPI bus as follows:
  Arduino Pin #     Function
  10                CS
  11                MOSI
  12                MISO
  13                CLK
  
  Theory of operation:
  The file server is only active when STB and Select lines are low.
  If at any time these lines indicate not active, the file server
  is in mode 'waiting for select'.  Any open files are close.
  
  Once selected, the Arduino expects a command byte as follows;
  Command byte      Function
  0<filename>       Open file <filename> for reading
  1<filename>       Open file <filename> for writing (file is created/overwritten)
  2                 Close currently open file
  3<filename>       Delete file <filename>
  4                 Directory listing of root folder
  
  <filename> indicates a stream of nul terminated bytes providing a filename.
  
  No folder paths are expected - keep files short, ideally 8.3 standard.
  The Oric client software only tries to send and receive bytes if the Ready signal
  (Arduino pin 7) is high.  If the Arduino is not ready for too long then the Oric
  times out and de-selects the file server resulting in it going back to waiting for
  select.
  
  This mechanism is used by the Oric to test for errors; When reading or deleting a file,
  if file does not exist, file server goes busy for 250ms so the Oric times out.
  When reading or writing bytes, the Oric will de-deselect the file server when done.
  Data is sent to/from the Oric nibble (4 bits) at a time, synchronised by the Clock pin
  which is controlled by the Oric.  Data is only valid when the Clock goes from low to
  high.  Clearly two clock transitions from low to high are required for a single byte.
  
  The Oric controls the speed of the clock and has been set during trial and error
  experiments - it appears the Arduino can keep up with sensing signals and responding
  to actions with a clock rate of around 7.5KHz - this results in a real-world speed
  of around 2KB/s transfer speed (high-res screen loads in less than 4 seconds).
  
  No claim this code is either the most efficient or robust, but seems to work ok *BUT*
  I have noticed the SD card can take arbitrarily take longer to respond so have made
  tweaks to the Arduino side (500ms busy delay when error detected) and the Oric side
  (waits longer before timing out) to hopefully sort this.
  

