# Base image with strongswan included
FROM strongx509/strongswan:latest

# Install development tools
RUN apt-get update -y
RUN apt-get install -y gcc make wget

WORKDIR /app

# Build the UDP forwarder program
COPY Makefile Makefile
COPY udp_forwarder.c udp_forwarder.c

#RUN gcc -Wall -pthread -o udp_forwarder udp_forwarder.c
RUN make

# Download startup file to the working directory
COPY startup startup
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
