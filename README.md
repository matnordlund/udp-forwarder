# UDP Forwarder with HTTP Stats

This program listens for UDP packets on a specified port, forwards them to a remote IP and port, and provides real-time
statistics via an HTTP server. The statistics include the number of forwarded logs per second, per hour, and the number of
connected clients.

## Features
- Forwards UDP packets to a specified remote IP and port.
- Provides real-time stats via a built-in HTTP server, either as JSON or HTML.
- Can run as a daemon (background process).
- Ports are optional, with default values for the listen port, forward port, and HTTP port.
- Configuration can be read from configuration file, config.ini

## Default Ports
- **Listen Port:** `514`
- **Remote Forward Port:** `514`
- **HTTP Stats Port:** `8514`

## Running with Custom Ports
You can specify custom ports using the flags:
- -l <listen_port> for the UDP listen port.
- -r <remote_port> for the remote forward port.
- -w <http_port> for the HTTP stats server.

   ```bash
   ./udp_forwarder -l 5156 -r 514 -w 8080 10.120.223.123

## Running with config.ini

Example `config.ini` file that listens on UDP port 8514 and forward logs to UDP port 512 in 192.168.0.100.

```bash
listen_port=8514
remote_port=514
http_port=8080
forward_ip=192.168.0.100
```

## Compilation

To compile the program, use the provided `Makefile`. Make sure you have `gcc` installed.

1. Clone or download the project.
2. Navigate to the project directory.
3. Run the following command to compile the program:

   ```bash
   make
