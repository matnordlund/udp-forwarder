# Base image with strongswan included
FROM strongx509/strongswan:latest

# Install development tools
RUN apt-get update -y
RUN apt-get install -y gcc make

WORKDIR /app

# Build the UDP forwarder program
COPY Makefile Makefile
COPY udp_forwarder.c udp_forwarder.c
RUN make

# Download startup file to the working directory
COPY startup startup
RUN chmod 755 startup

COPY healthcheck-script.sh healthcheck-script.sh
RUN chmod 755 healthcheck-script.sh
HEALTHCHECK CMD /app/healthcheck-script.sh 

# Clean up development tools
RUN apt-get -y remove gcc make
RUN apt-get -y autoremove
RUN apt-get clean

RUN mkdir -p /data

# Expose the UDP listening port (default 514) and HTTP port (default 8514)
EXPOSE 514/udp
EXPOSE 8514/tcp

CMD ["sh", "-c", "/app/startup"]
