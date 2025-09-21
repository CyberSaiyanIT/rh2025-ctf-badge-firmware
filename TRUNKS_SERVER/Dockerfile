FROM debian
RUN adduser --disabled-password --gecos "" appuser
USER appuser
COPY ./SSH/main /ssh
COPY ./NTP/main /ntp
ENTRYPOINT [ "./entrypoint.sh" ]