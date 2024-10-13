# Base image with strongswan included
FROM strongx509/strongswan:latest

# Install development tools
RUN apt-get update -y
RUN apt-get install -y gcc make wget

WORKDIR /app

# Download source files to the working directory
RUN wget https://raw.githubusercontent.com/matnordlund/udp-forwarder/refs/heads/main/udp_forwarder.c

# Build the UDP forwarder program
RUN gcc -Wall -pthread -o udp_forwarder udp_forwarder.c

# Download startup file to the working directory
RUN wget https://raw.githubusercontent.com/matnordlund/udp-forwarder/refs/heads/main/startup
RUN chmod 755 startup

# Clean up development tools
RUN apt-get -y remove gcc make wget
RUN apt-get -y autoremove
RUN apt-get clean

RUN mkdir -p /data

# Expose the UDP listening port (default 514) and HTTP port (default 8514)
EXPOSE 514/udp
EXPOSE 8514/tcp

CMD ["sh", "-c", "/app/startup"]
