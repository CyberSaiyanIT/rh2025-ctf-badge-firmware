FROM debian
RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y git && rm -rf /var/lib/apt/lists/*
RUN adduser --disabled-password --gecos "" trunks
COPY --chown=trunks:trunks notes /home/trunks
COPY --chown=trunks:trunks drgero/ /home/trunks/drgero
RUN rm /home/trunks/drgero/sshd.c
USER trunks
WORKDIR /home/trunks