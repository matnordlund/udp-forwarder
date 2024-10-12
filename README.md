# UDP Forwarder with HTTP Stats

This program listens for UDP packets on a specified port, forwards them to a remote IP and port, and provides real-time
statistics via an HTTP server. The statistics include the number of forwarded logs per second, per hour, and the number of
connected clients.

## Features
- Forwards UDP packets to a specified remote IP and port.
- Provides real-time stats via a built-in HTTP server, either as JSON or HTML.
- Can run as a daemon (background process).
- Ports are optional, with default values for the listen port, forward port, and HTTP port.

## Default Ports
- **Listen Port:** `514`
- **Remote Forward Port:** `514`
- **HTTP Stats Port:** `8514`

## Compilation

To compile the program, use the provided `Makefile`. Make sure you have `gcc` installed.

1. Clone or download the project.
2. Navigate to the project directory.
3. Run the following command to compile the program:

   ```bash
   make
