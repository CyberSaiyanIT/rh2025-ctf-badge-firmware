FROM debian
COPY ./entrypoint.sh /entrypoint.sh
COPY ./drgerossh /main
COPY ./server.key /etc/server.key
COPY ./drgero.pub /drgero.pub
EXPOSE 22
CMD [ "/entrypoint.sh" ]
