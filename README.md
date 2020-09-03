# A serial <-> WiFi transparent bridge

Just a simple bridge to connect a serial port over WiFi using [Mongoose OS](https://mongoose-os.com/).

Build, flash and configure hostname (default is mos-serial-MAC).

Open a TCP connection to the configured port, any byte written will be echoed on the serial port, any byte received from the serial port will be echoed to the open connection.
