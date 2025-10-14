FROM debian
RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y git && rm -rf /var/lib/apt/lists/*
RUN chmod -x /usr/bin/openssl && chmod -x /usr/bin/ssh
RUN adduser --disabled-password --gecos "" trunks
COPY ./entrypoint.sh /entrypoint.sh
COPY ./trunksssh /main
COPY ./server.key /etc/server.key
EXPOSE 22
CMD [ "/entrypoint.sh" ]
