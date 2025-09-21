FROM debian
RUN adduser --disabled-password --gecos "" trunks
COPY ./entrypoint.sh /entrypoint.sh
COPY ./trunksssh /main
COPY ./server.key /etc/server.key
COPY exfiltrated/ /home/trunks
EXPOSE 22
CMD [ "/entrypoint.sh" ]
